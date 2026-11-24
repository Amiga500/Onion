/* Deterministic reproduction of a cancelled stream-open completing after a
 * replacement request. No network, actual audio worker, mocked decoder. */
#define _DEFAULT_SOURCE
#include "audio.h"
#include "decode.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
struct decoder { int unused; };
static atomic_int entered,cancelled,ready,reading,release_pcm;
int radio_is_url(const char *p){return p&&strstr(p,"http://")==p;}
void dec_cancel_pending(void){atomic_store(&cancelled,1);}
void dec_error_message(char *out,int cap){snprintf(out,cap,"cancelled old request");}
void dec_radio_title(decoder *d,char *out,int cap){(void)d;if(cap)out[0]=0;}
void dec_close(decoder *d){free(d);}
int64_t dec_seek(decoder *d,int64_t f){(void)d;return f;}
decoder *dec_open(const char *path,dec_info *info){
    if(strstr(path,"old")){
        atomic_store(&cancelled,0);atomic_store(&entered,1);
        while(!atomic_load(&cancelled))usleep(1000);
        usleep(50000);return NULL;
    }
    *info=(dec_info){48000,2,128,48000};
    atomic_store(&ready,1);return calloc(1,sizeof(decoder));
}
int dec_read(decoder *d,float *out,int frames){
    (void)d;memset(out,0,(size_t)frames*2*sizeof(float));
    atomic_store(&reading,1);while(!atomic_load(&release_pcm))usleep(1000);return frames;
}
static void wait_for(atomic_int *p){for(int i=0;i<2000&&!atomic_load(p);i++)usleep(1000);assert(atomic_load(p));}
int main(void){
    audio_init();audio_play_file("http://old",0,0);wait_for(&entered);
    assert(audio_buffering());assert(audio_state()==A_PLAYING);
    audio_play_file("http://new",0,0);wait_for(&ready);wait_for(&reading);
    assert(audio_buffering());assert(audio_pos_ms()==0);
    atomic_store(&release_pcm,1);
    for(int i=0;i<2000&&audio_buffering();i++)usleep(1000);
    assert(!audio_buffering());
    assert(!audio_take_error());assert(!audio_take_finished());
    audio_set_paused(1);assert(audio_state()==A_PAUSED);
    audio_set_paused(0);assert(audio_state()==A_PLAYING);
    audio_stop();assert(!audio_buffering());audio_quit();puts("Superseded stream open and first-PCM buffering: PASS");return 0;
}
