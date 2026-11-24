/* Test-only FFmpeg bridge: exercises loader and decoder contract without FFmpeg. */
#include "ffbridge.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

typedef struct {int pos;const atomic_int *cancel;} fake;
int ffbridge_ready(void) {return 1;}
void *ffbridge_open(const char *path,int live,const atomic_int *cancel,
                    ffaudio_info *info,char *err,size_t cap) {
    (void)live;(void)err;(void)cap;
    if(!path) return NULL;
    fake *f=calloc(1,sizeof *f);
    if(!f) return NULL;
    f->cancel=cancel;info->samplerate=48000;info->channels=2;
    info->frames=48000;info->kbps=96;
    return f;
}
int ffbridge_read(void *ctx,float *out,int frames) {
    fake *f=ctx;
    if(atomic_load(f->cancel)) return 0;
    if(f->pos+frames>48000) frames=48000-f->pos;
    for(int i=0;i<frames*2;i++) out[i]=0.25f;
    f->pos+=frames;
    return frames;
}
int64_t ffbridge_seek(void *ctx,int64_t frame) {
    fake *f=ctx;
    f->pos=(int)frame;
    return frame;
}
void ffbridge_title(void *ctx,char *out,size_t n) {
    (void)ctx;snprintf(out,n,"Mock Radio");
}
void ffbridge_close(void *ctx) {free(ctx);}
int ffbridge_metadata(const char *path,track_meta *m) {
    (void)path;
    snprintf(m->title,sizeof m->title,"Mock track");
    snprintf(m->album,sizeof m->album,"Mock album");
    return 1;
}

#ifndef MOCK_LEGACY
void *ffbridge_open_tls(const char *path,int live,const atomic_int *cancel,
                         ffaudio_info *info,char *err,size_t cap,int insecure) {
    const char *expect=getenv("REDAMP_TEST_EXPECT_INSECURE");
    if(insecure!=!!(live&&expect&&atoi(expect)==1)){
        snprintf(err,cap,"unexpected TLS policy");return NULL;
    }
    return ffbridge_open(path,live,cancel,info,err,cap);
}
#endif
