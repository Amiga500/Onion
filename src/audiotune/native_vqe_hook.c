/*
 * AudioTune v0.2.3 beta - firmware-verified SigmaStar MI_AO/VQE server hook.
 * Copyright (c) 2026. SPDX-License-Identifier: MIT.
 *
 * Beta feature: MI_AO readback is not acoustic proof of game EQ.
 * When preloaded into the PROCESS THAT OWNS AUDIO OUTPUT,
 * intercepts MI_AO_EnableChn AFTER the real enable call and requests
 * hardware VQE EQ from that same process. Does not hook or patch the
 * already-running audioserver; does not implement live settings changes.
 *
 * This ABI layout was checked against DWARF from the user's 2023
 * /config/lib/libmi_ao.so, not inferred from a different SDK revision.
 * If the library/ABI or sample rate differs, do not enable this hook.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <unistd.h>
#include "audiotune_presets.h"

typedef struct {
    int32_t eSamplerate, eBitwidth, eWorkmode, eSoundmode;
    uint32_t u32FrmNum, u32PtNumPerFrm, u32CodecChnCnt, u32ChnCnt;
    uint8_t workModeSetting[12];
} AudioAttr;
typedef struct { int32_t eMode, eHpfFreq; } HpfConfig;
typedef struct { int32_t eMode; uint32_t intensity, smooth; int32_t speed; } AnrConfig;
typedef struct { int32_t max,min,init; } AgcGain;
typedef struct {
    int32_t eMode;
    AgcGain gains;
    uint32_t drop,attack,release;
    int16_t compressionIn[5],compressionOut[5];
    int32_t targetDb,noiseDb;
    uint32_t noiseAttenDb;
} AgcConfig;
typedef struct { int32_t eMode; int16_t gainDb[129]; } EqConfig;
typedef struct {
    uint8_t hpf,anr,agc,eq;
    int32_t workRate,frameSamples;
    HpfConfig hpfCfg;
    AnrConfig anrCfg;
    AgcConfig agcCfg;
    EqConfig eqCfg;
} VqeConfig;

/* No C11 features needed by stock toolchain. */
typedef char assert_audioattr_size[(sizeof(AudioAttr)==44)?1:-1];
typedef char assert_audiovqe_size[(sizeof(VqeConfig)==360)?1:-1];
typedef char assert_eq_offset[(offsetof(VqeConfig,eqCfg)==96)?1:-1];
typedef char assert_eq_gain_offset[(offsetof(VqeConfig,eqCfg.gainDb)==100)?1:-1];

/* The beta does not write files or generate channel-status markers.
   Optional diagnostic output goes to stderr only with AUDIOTUNE_DEBUG=1. */
static void hook_log(const char *fmt, ...) {
    const char *debug=getenv("AUDIOTUNE_DEBUG");
    va_list ap;
    if (!debug || strcmp(debug,"1")!=0) return;
    va_start(ap,fmt);
    fprintf(stderr,"AudioTune VQE: ");
    vfprintf(stderr,fmt,ap);
    fputc('\n',stderr);
    va_end(ap);
}
#define HOOK_LOG(...) hook_log(__VA_ARGS__)
static void status_update(const char *state,int dev,int chn,int rc) {
    HOOK_LOG("%s dev=%d chn=%d rc=0x%x",state,dev,chn,(unsigned)rc);
}

static unsigned char installed[4][2];
static int (*real_enable_chn)(int,int);

static int native_profile(int gains[5]) {
    const char *path=getenv("AUDIOTUNE_VQE_PROFILE");
    int preset=0, eqOn=0, custom[5]={0};
    FILE *f;
    char line[160];
    if (!path || path[0]!='/') {
        HOOK_LOG("missing absolute AUDIOTUNE_VQE_PROFILE; skipped");
        return 0;
    }
    f=fopen(path,"r");
    if (!f) { HOOK_LOG("cannot open profile; skipped"); return 0; }
    while(fgets(line,sizeof(line),f)) {
        int i;
        if(sscanf(line,"preset=%d",&i)==1) preset=i;
        else if(sscanf(line,"eq_enabled=%d",&i)==1) eqOn=(i==1);
        else if(sscanf(line,"custom=%d %d %d %d %d",
                 &custom[0],&custom[1],&custom[2],&custom[3],&custom[4])==5) { }
    }
    fclose(f);
    if (!eqOn || preset<=0 || preset>6) {
        HOOK_LOG("EQ off / Flat / invalid profile; skipped");
        return 0;
    }
    if (preset==6) memcpy(gains,custom,sizeof(custom));
    else memcpy(gains,audiotune_preset_gains[preset],sizeof(custom));
    for(int i=0;i<5;i++) if(gains[i]<-AUDIOTUNE_EQ_GAIN_LIMIT || gains[i]>AUDIOTUNE_EQ_GAIN_LIMIT) {
        HOOK_LOG("out-of-range gain; skipped");
        return 0;
    }
    if (audiotune_gains_all_zero(gains)) {
        HOOK_LOG("all EQ gains are zero; native VQE bypassed");
        return 0;
    }
    return 1;
}

/* Approximate the same five Q=1 peaking filters used by the UI, then
   quantize to the native 129-band gain table. Unlike the old interpolation,
   these filters naturally return to ~0 dB outside the control bands.
   Frequency quantization can still differ substantially at 48 kHz. */
typedef struct { float b0,b1,b2,a1,a2; } ResponseBiquad;
static void response_peaking(ResponseBiquad *b,int rate,int hz,int db) {
    float f=fminf((float)hz,rate*0.44f);
    float A=powf(10.0f,(float)db/40.0f);
    float w=6.2831853071795864769f*f/rate;
    float alpha=sinf(w)*0.5f;
    float cs=cosf(w);
    float a0=1.0f+alpha/A;
    b->b0=(1.0f+alpha*A)/a0;
    b->b1=(-2.0f*cs)/a0;
    b->b2=(1.0f-alpha*A)/a0;
    b->a1=b->b1;
    b->a2=(1.0f-alpha/A)/a0;
}
static float response_db(float hz,int rate,const ResponseBiquad b[5]) {
    float w=6.2831853071795864769f*hz/rate;
    float c1=cosf(w),s1=sinf(w),c2=cosf(2*w),s2=sinf(2*w);
    float sum=0.0f;
    int k;
    for(k=0;k<5;k++) {
        float nr=b[k].b0+b[k].b1*c1+b[k].b2*c2;
        float ni=-b[k].b1*s1-b[k].b2*s2;
        float dr=1.0f+b[k].a1*c1+b[k].a2*c2;
        float di=-b[k].a1*s1-b[k].a2*s2;
        float nn=nr*nr+ni*ni,dd=dr*dr+di*di;
        if(nn>1e-12f && dd>1e-12f) sum+=10.0f*log10f(nn/dd);
    }
    return sum;
}
static int build_vqe_table(VqeConfig *cfg, int rate, const int gains[5]) {
    ResponseBiquad b[5];
    float maxdb=0.0f;
    float raw[129];
    int i,attenuate,limited=0, minbin=0, maxbin=-50;
    for(i=0;i<5;i++) response_peaking(&b[i],rate,audiotune_eq_freq[i],gains[i]);
    for(i=0;i<1025;i++) {
        float freq=(rate*0.5f)*(float)i/1024.0f;
        float v=response_db(freq,rate,b);
        if(v>maxdb) maxdb=v;
    }
    /* Each VQE bin covers one interval, not a single isolated frequency.
       Using its centre avoids a low/high-frequency shelf. */
    for(i=0;i<129;i++) {
        float freq=(rate*0.5f)*((float)i+0.5f)/129.0f;
        raw[i]=response_db(freq,rate,b);
        if(raw[i]>maxdb) maxdb=raw[i];
    }
    /* Ensure no positive gain in native path; extra 1 dB guard.
       Supported range is [-50,+20] dB in SigmaStar audio docs. */
    attenuate=maxdb>0?((int)ceilf(maxdb)+1):0;
    for(i=0;i<129;i++) {
        float v=raw[i]-attenuate;
        int rounded=(int)(v<0?v-0.5f:v+0.5f);
        /* Saturate instead of rejecting extreme ±12 dB combinations.
           The native ABI has a -50 dB floor and never sends a boost >0. */
        if(rounded < -50) { rounded=-50; limited++; }
        if(rounded > 0) { rounded=0; limited++; }
        if(i==0 || rounded<minbin) minbin=rounded;
        if(rounded>maxbin) maxbin=rounded;
        cfg->eqCfg.gainDb[i]=(int16_t)rounded;
    }
    HOOK_LOG("native EQ attenuation: %d dB; 129-bin range %d..%d dB; saturated bins=%d; approximate curve, NOT exact preview",
             attenuate,minbin,maxbin,limited);
    return 1;
}

int MI_AO_EnableChn(int dev,int chn) {
    int rc, gains[5];
    AudioAttr attr;
    VqeConfig config,existing;
    int (*getpub)(int,AudioAttr *);
    int (*getvqe)(int,int,VqeConfig *);
    int (*setvqe)(int,int,const VqeConfig *);
    int (*enablevqe)(int,int);
    const char *enabled=getenv("AUDIOTUNE_NATIVE_VQE");

    if (!real_enable_chn) {
        real_enable_chn=(int (*)(int,int))dlsym(RTLD_NEXT,"MI_AO_EnableChn");
        if (!real_enable_chn) { HOOK_LOG("original enable function unavailable"); status_update("CHANNEL_API_MISSING",dev,chn,-1); return -1; }
    }
    rc=real_enable_chn(dev,chn);
    if(rc) status_update("CHANNEL_ENABLE_FAILED",dev,chn,rc);
    if(rc || !enabled || strcmp(enabled,"1")!=0) return rc;
    /* Reject a different MI_AO library even if /config/lib matched the
       launcher checksum. LD_LIBRARY_PATH can be overridden by the runtime. */
    {
        Dl_info origin;
        memset(&origin,0,sizeof(origin));
        if (!dladdr((void *)real_enable_chn,&origin) || !origin.dli_fname ||
            strcmp(origin.dli_fname,"/config/lib/libmi_ao.so")!=0) {
            HOOK_LOG("MI_AO loaded from unexpected path: %s; VQE skipped",
                     origin.dli_fname?origin.dli_fname:"(unknown)");
            return rc;
        }
    }
    if(dev<0 || dev>=4 || chn<0 || chn>=2) {
        HOOK_LOG("unexpected dev=%d chn=%d; skipped",dev,chn);
        return rc;
    }

    enablevqe=(int (*)(int,int))dlsym(RTLD_NEXT,"MI_AO_EnableVqe");
    if(installed[dev][chn]) {
        /* Channel re-enables discard VQE enable state in SigmaStar's API. */
        if(enablevqe) {
            int r=enablevqe(dev,chn);
            HOOK_LOG("re-enable VQE dev=%d chn=%d rc=0x%x",dev,chn,r);
            status_update(r==0?"EQ_REENABLE_OK":"EQ_REENABLE_FAILED",dev,chn,r);
            if(r!=0) installed[dev][chn]=0;
        } else status_update("EQ_REENABLE_API_MISSING",dev,chn,-1);
        return rc;
    }
    if(!native_profile(gains)) return rc;
    getpub=(int (*)(int,AudioAttr *))dlsym(RTLD_NEXT,"MI_AO_GetPubAttr");
    getvqe=(int (*)(int,int,VqeConfig *))dlsym(RTLD_NEXT,"MI_AO_GetVqeAttr");
    setvqe=(int (*)(int,int,const VqeConfig *))dlsym(RTLD_NEXT,"MI_AO_SetVqeAttr");
    if(!getpub || !getvqe || !setvqe || !enablevqe) {
        HOOK_LOG("required VQE APIs missing; skipped"); status_update("VQE_API_MISSING",dev,chn,-1); return rc;
    }
    memset(&attr,0,sizeof(attr));
    if(getpub(dev,&attr)!=0) {
        HOOK_LOG("cannot read output attributes; skipped"); status_update("PUBATTR_FAILED",dev,chn,-1); return rc;
    }
    /* The tested SDK exposes EQ VQE for 8/16/48 kHz; do not guess or
       alter the audioserver's chosen sample rate. */
    if(attr.eSamplerate!=8000 && attr.eSamplerate!=16000 && attr.eSamplerate!=48000) {
        HOOK_LOG("unsupported VQE rate %d; skipped",attr.eSamplerate);
        status_update("UNSUPPORTED_RATE",dev,chn,attr.eSamplerate);
        return rc;
    }
    if(attr.eBitwidth!=0) {
        HOOK_LOG("unsupported bit width enum %d; skipped",attr.eBitwidth);
        status_update("UNSUPPORTED_BITWIDTH",dev,chn,attr.eBitwidth);
        return rc;
    }
    memset(&existing,0,sizeof(existing));
    {
        int grc=getvqe(dev,chn,&existing);
        if(grc==0) {
            HOOK_LOG("GetVqeAttr dev=%d chn=%d existing flags: hpf=%u anr=%u agc=%u eq=%u rate=%d frames=%d",
                     dev,chn,(unsigned)existing.hpf,(unsigned)existing.anr,
                     (unsigned)existing.agc,(unsigned)existing.eq,
                     existing.workRate,existing.frameSamples);
            /* A successful GetVqeAttr can return a previously initialized,
               *disabled* EQ slot. Preserve other processing settings, and
               never overwrite an existing enabled EQ configuration. */
            if(existing.eq) {
                HOOK_LOG("existing EQ already enabled; refusing to overwrite");
                status_update("EXISTING_EQ_REFUSED",dev,chn,0);
                return rc;
            }
            if(existing.workRate && existing.workRate!=attr.eSamplerate) {
                HOOK_LOG("existing VQE rate mismatch (%d != %d); skipped",
                         existing.workRate,attr.eSamplerate);
                return rc;
            }
            config=existing;
        } else {
            HOOK_LOG("GetVqeAttr returned 0x%x (no usable config); initializing EQ-only VQE",grc);
            memset(&config,0,sizeof(config));
        }
    }
    config.eq=1;
    config.workRate=attr.eSamplerate;
    if(config.frameSamples==0) config.frameSamples=128;
    config.eqCfg.eMode=1; /* USER mode */
    if(!build_vqe_table(&config,attr.eSamplerate,gains)) {
        status_update("TABLE_REJECTED",dev,chn,0); return rc;
    }
    rc=setvqe(dev,chn,&config);
    if(rc!=0) {
        HOOK_LOG("SetVqeAttr dev=%d chn=%d failed 0x%x; normal output retained",dev,chn,rc);
        status_update("SET_VQE_FAILED",dev,chn,rc);
        return 0; /* original channel enable already succeeded */
    }
    HOOK_LOG("SetVqeAttr dev=%d chn=%d accepted rc=0",dev,chn);
    rc=enablevqe(dev,chn);
    if(rc!=0) {
        HOOK_LOG("EnableVqe dev=%d chn=%d failed 0x%x; normal output retained",dev,chn,rc);
        status_update("ENABLE_VQE_FAILED",dev,chn,rc);
        return 0;
    }
    HOOK_LOG("EnableVqe dev=%d chn=%d accepted rc=0",dev,chn);
    installed[dev][chn]=1;
    { VqeConfig readback;
      memset(&readback,0,sizeof(readback));
      if(getvqe(dev,chn,&readback)==0) {
          HOOK_LOG("VQE readback dev=%d chn=%d eq=%u mode=%d firstBin=%d",
                   dev,chn,(unsigned)readback.eq,readback.eqCfg.eMode,
                   (int)readback.eqCfg.gainDb[0]);
          status_update(readback.eq==1?"EQ_READBACK_OK":"EQ_READBACK_MISMATCH",
                        dev,chn,0);
      } else {
          HOOK_LOG("VQE readback unavailable after enable");
          status_update("EQ_READBACK_UNAVAILABLE",dev,chn,0);
      }
    }
    HOOK_LOG("native VQE requested dev=%d chn=%d rate=%d; LISTEN/MEASURE to verify",
             dev,chn,attr.eSamplerate);
    return 0;
}

/* Track channel shutdown so the status script does not report an enabled
   channel after a game/runtime transition closes it. */
int MI_AO_DisableChn(int dev,int chn) {
    int (*real_disable)(int,int)=(int (*)(int,int))dlsym(RTLD_NEXT,"MI_AO_DisableChn");
    int rc;
    if(!real_disable) { HOOK_LOG("original MI_AO_DisableChn unavailable"); return -1; }
    rc=real_disable(dev,chn);
    if(rc==0 && dev>=0 && dev<4 && chn>=0 && chn<2 && installed[dev][chn])
        status_update("CHANNEL_DISABLED",dev,chn,0);
    return rc;
}
