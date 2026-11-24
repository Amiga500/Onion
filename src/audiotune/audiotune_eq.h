/* Shared AudioTune EQ response for SDL preview and SigmaStar VQE.
 * SPDX-License-Identifier: MIT.
 * Both paths use RBJ low/high shelving filters (S=1) on the outer bands,
 * and Q=1 peaking filters on the three middle bands.
 */
#ifndef AUDIOTUNE_EQ_H
#define AUDIOTUNE_EQ_H
#include <math.h>
#include "audiotune_presets.h"

typedef struct {
    float b0,b1,b2,a1,a2;
} AudioTuneEQCoeff;

/* 0 = SAFE (no positive native gains), 1 = PUNCHY (up to +3 dB). */
#define AUDIOTUNE_HEADROOM_SAFE 0
#define AUDIOTUNE_HEADROOM_PUNCHY 1
#define AUDIOTUNE_NATIVE_PEAK_DB 3
/* This is a user-selected output target, not automatic headphone detection. */
#define AUDIOTUNE_OUTPUT_SPEAKER 0
#define AUDIOTUNE_OUTPUT_HEADPHONES 1
/* Avoid amplifying the Mini's sub-bass/noise. Native table resolution is
   roughly 186 Hz at 48 kHz; this is a protective low-frequency cut, not
   a calibrated speaker response. Headphone output retains the full shelf. */
static inline float audiotune_speaker_guard_db(int bin, float db) {
    if (bin==0 && db>-16.0f) return -16.0f;
    if (bin==1 && db>0.0f) return 0.0f;
    return db;
}

static inline void audiotune_eq_make(AudioTuneEQCoeff *b,int band,int rate,int db) {
    const float pi=3.14159265358979323846f;
    float hz=(float)audiotune_eq_freq[band];
    float A,w,sn,cs,alpha,beta,a0;
    if (hz<30.0f) hz=30.0f;
    if (hz>(float)rate*0.44f) hz=(float)rate*0.44f;
    A=powf(10.0f,(float)db/40.0f);
    w=2.0f*pi*hz/(float)rate;
    sn=sinf(w); cs=cosf(w);
    if (band==0 || band==AUDIOTUNE_EQ_BANDS-1) {
        /* Shelves with slope S=1 (RBJ Audio EQ Cookbook). */
        alpha=0.5f*sn*1.4142135623730951f;
        beta=2.0f*sqrtf(A)*alpha;
        if (band==0) {
            a0=(A+1.0f)+(A-1.0f)*cs+beta;
            b->b0=A*((A+1.0f)-(A-1.0f)*cs+beta)/a0;
            b->b1=2.0f*A*((A-1.0f)-(A+1.0f)*cs)/a0;
            b->b2=A*((A+1.0f)-(A-1.0f)*cs-beta)/a0;
            b->a1=-2.0f*((A-1.0f)+(A+1.0f)*cs)/a0;
            b->a2=((A+1.0f)+(A-1.0f)*cs-beta)/a0;
        } else {
            a0=(A+1.0f)-(A-1.0f)*cs+beta;
            b->b0=A*((A+1.0f)+(A-1.0f)*cs+beta)/a0;
            b->b1=-2.0f*A*((A-1.0f)+(A+1.0f)*cs)/a0;
            b->b2=A*((A+1.0f)+(A-1.0f)*cs-beta)/a0;
            b->a1=2.0f*((A-1.0f)-(A+1.0f)*cs)/a0;
            b->a2=((A+1.0f)-(A-1.0f)*cs-beta)/a0;
        }
    } else {
        /* Peaking EQ with Q=1.0. */
        alpha=sn*0.5f;
        a0=1.0f+alpha/A;
        b->b0=(1.0f+alpha*A)/a0;
        b->b1=-2.0f*cs/a0;
        b->b2=(1.0f-alpha*A)/a0;
        b->a1=b->b1;
        b->a2=(1.0f-alpha/A)/a0;
    }
}

static inline float audiotune_eq_response_db(float hz,int rate,
                                              const AudioTuneEQCoeff coeff[AUDIOTUNE_EQ_BANDS]) {
    const float pi=3.14159265358979323846f;
    float w=2.0f*pi*hz/(float)rate;
    float c1=cosf(w),s1=sinf(w),c2=cosf(2.0f*w),s2=sinf(2.0f*w);
    float result=0.0f;
    int k;
    for(k=0;k<AUDIOTUNE_EQ_BANDS;++k) {
        const AudioTuneEQCoeff *b=&coeff[k];
        float nr=b->b0+b->b1*c1+b->b2*c2;
        float ni=-b->b1*s1-b->b2*s2;
        float dr=1.0f+b->a1*c1+b->a2*c2;
        float di=-b->a1*s1-b->a2*s2;
        float numerator=nr*nr+ni*ni,denominator=dr*dr+di*di;
        if(numerator>1e-12f && denominator>1e-12f)
            result+=10.0f*log10f(numerator/denominator);
    }
    return result;
}

static inline float audiotune_eq_peak_db(int rate,
                                          const AudioTuneEQCoeff coeff[AUDIOTUNE_EQ_BANDS]) {
    float maximum=0.0f;
    int i;
    for(i=0;i<=1024;++i) {
        float hz=((float)rate*0.5f)*(float)i/1024.0f;
        float db=audiotune_eq_response_db(hz,rate,coeff);
        if (db>maximum) maximum=db;
    }
    /* Include native 129-bin centre points, where the table is quantized. */
    for(i=0;i<129;++i) {
        float hz=((float)rate*0.5f)*((float)i+0.5f)/129.0f;
        float db=audiotune_eq_response_db(hz,rate,coeff);
        if (db>maximum) maximum=db;
    }
    return maximum;
}

/* Peak of the actual discretized native table, including the protective
 * speaker guard; avoids attenuating the entire speaker profile just because
 * it has a large inaudible boost below the coarse first hardware bin. */
static inline float audiotune_native_peak_db(int rate,
        const AudioTuneEQCoeff coeff[AUDIOTUNE_EQ_BANDS],int output) {
    float peak=(output==AUDIOTUNE_OUTPUT_SPEAKER)?0.0f:
                audiotune_eq_peak_db(rate,coeff);
    int i;
    for(i=0;i<129;++i) {
        float hz=0.5f*(float)rate*((float)i+0.5f)/129.0f;
        float db=audiotune_eq_response_db(hz,rate,coeff);
        if(output==AUDIOTUNE_OUTPUT_SPEAKER)
            db=audiotune_speaker_guard_db(i,db);
        if(db>peak) peak=db;
    }
    return peak;
}

static inline int audiotune_native_attenuation_db(float peak,int mode) {
    int attenuation=peak>0.0f ? (int)ceilf(peak)+1 : 0;
    if (mode==AUDIOTUNE_HEADROOM_PUNCHY) attenuation-=AUDIOTUNE_NATIVE_PEAK_DB;
    return attenuation>0?attenuation:0;
}

/* Fast, stateless soft knee; asymptotically limited to +/-1.0f.
 * Only used for the SDL software path (never promised to protect games).
 */
static inline float audiotune_soft_limit(float signal) {
    const float knee=0.84f, room=0.16f;
    float mag=fabsf(signal);
    if (mag<=knee) return signal;
    mag=knee+(mag-knee)/(1.0f+(mag-knee)/room);
    return signal<0.0f?-mag:mag;
}
#endif
