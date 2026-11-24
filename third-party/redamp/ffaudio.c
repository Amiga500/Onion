/* Optional FFmpeg 4.4.8 backend loader. The game/music engine has no build-
 * time dependency on FFmpeg or its headers; the bridge can be absent safely. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include "ffaudio.h"
#include "netclock.h"
#include "ffbridge.h"
#include <dlfcn.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

struct ffaudio {
    void *backend;
    atomic_int cancel;
    char *path;
    int live, retries;
    uint64_t good_since;
    ffaudio_info info;
};
static pthread_once_t init_once=PTHREAD_ONCE_INIT;
static pthread_mutex_t active_lock=PTHREAD_MUTEX_INITIALIZER;
static ffaudio *active;
static char load_error[192];
static void *module;
static int (*p_ready)(void);
static void *(*p_open)(const char *, int, const atomic_int *, ffaudio_info *,char *,size_t);
static void *(*p_open_tls)(const char *, int, const atomic_int *, ffaudio_info *,char *,size_t,int);
static int (*p_read)(void *, float *, int);
static int64_t (*p_seek)(void *, int64_t);
static void (*p_title)(void *, char *, size_t);
static void (*p_close)(void *);
static int (*p_metadata)(const char *, track_meta *);
static void fail(char *out,size_t n,const char *message) {
    if(out && n) snprintf(out,n,"%s",message ? message : "FFmpeg unavailable");
}
static uint64_t ticks(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000u+(unsigned)t.tv_nsec/1000000u;
}
static void load_bridge(void) {
    const char *home=getenv("MA_HOME");
    char file[1024];
    if (!home || !home[0] || strlen(home)>800) {
        snprintf(load_error,sizeof load_error,"FFmpeg missing: MA_HOME not set");return;
    }
    snprintf(file,sizeof file,"%s/lib/libredamp_ffaudio.so",home);
    module=dlopen(file,RTLD_NOW|RTLD_LOCAL);
    if (!module) {
        snprintf(load_error,sizeof load_error,"Needs RedAmp FFmpeg libraries");
        fprintf(stderr,"[ffaudio] %s: %s\n",file,dlerror());return;
    }
#define LOAD(field,symbol) do { *(void **)(&field)=dlsym(module,symbol); if(!field) goto bad; }while(0)
    LOAD(p_ready,"ffbridge_ready");
    LOAD(p_open,"ffbridge_open");
    *(void **)(&p_open_tls)=dlsym(module,"ffbridge_open_tls");
    LOAD(p_read,"ffbridge_read");
    LOAD(p_seek,"ffbridge_seek");
    LOAD(p_title,"ffbridge_title");
    LOAD(p_close,"ffbridge_close");
    LOAD(p_metadata,"ffbridge_metadata");
#undef LOAD
    if(!p_ready()) goto bad;
    return;
bad:
    snprintf(load_error,sizeof load_error,"Incompatible RedAmp FFmpeg libraries");
    dlclose(module);module=NULL;
}
const char *ffaudio_last_error(void) {
    pthread_once(&init_once,load_bridge);
    return load_error[0] ? load_error : "AAC/HLS stream could not be opened";
}
static void *open_backend(const char *path,int live,const atomic_int *cancel,
                          ffaudio_info *info,char *err,size_t n) {
    int insecure=live&&netclock_insecure();
    if(p_open_tls)return p_open_tls(path,live,cancel,info,err,n,insecure);
    if(insecure){fail(err,n,"Insecure HTTPS requires updated FFmpeg libraries");return NULL;}
    return p_open(path,live,cancel,info,err,n);
}
static int clock_cancel(void *ctx){return atomic_load(&((ffaudio *)ctx)->cancel);}
ffaudio *ffaudio_open(const char *path,int live,ffaudio_info *info,char *error,size_t error_size) {
    pthread_once(&init_once,load_bridge);
    if(!module) { fail(error,error_size,ffaudio_last_error());return NULL; }
    if (!path || !info) return NULL;
    /* The background music service deliberately does not load FFmpeg streams. */
    if (getenv("MA_GAME_SERVICE")) {
        fail(error,error_size,"AAC/HLS is unavailable in Music over games");return NULL;
    }
    ffaudio *f=calloc(1,sizeof *f);
    if(!f) return NULL;
    atomic_init(&f->cancel,0);
    f->path=strdup(path);f->live=live;
    if(!f->path) { free(f);return NULL; }
    pthread_mutex_lock(&active_lock);
    if(active) { pthread_mutex_unlock(&active_lock);free(f->path);free(f);return NULL; }
    active=f;
    pthread_mutex_unlock(&active_lock);
    if(live&&!netclock_insecure())netclock_ensure(clock_cancel,f,0);
    if(!atomic_load(&f->cancel))
        f->backend=open_backend(path,live,&f->cancel,&f->info,error,error_size);
    if(!f->backend&&live&&!atomic_load(&f->cancel)){
        if(!netclock_network_available())fail(error,error_size,"Network unavailable");
        else if(!netclock_insecure()&&!netclock_valid())fail(error,error_size,"Cannot reach stream / check system date");
    }
    if(!f->backend || atomic_load(&f->cancel)) {
        if(f->backend) p_close(f->backend);
        pthread_mutex_lock(&active_lock);if(active==f) active=NULL;pthread_mutex_unlock(&active_lock);
        free(f->path);free(f);return NULL;
    }
    *info=f->info;
    return f;
}
int ffaudio_read(ffaudio *f,float *out,int frames) {
    if(!f || !f->backend || frames<=0 || atomic_load(&f->cancel)) return 0;
    int n=p_read(f->backend,out,frames);
    if(n>0) {
        if(!f->good_since) f->good_since=ticks();
        if(ticks()-f->good_since>=60000) f->retries=0;
        return n;
    }
    if(!f->live || atomic_load(&f->cancel)) return 0;
    f->good_since=0;
    while(f->retries<4 && !atomic_load(&f->cancel)) {
        int secs=1<<f->retries++;
        if(f->backend)p_close(f->backend);
        f->backend=NULL;
        /* Small sleeps keep stop/switch responsive during backoff. */
        for(int i=0;i<secs*10 && !atomic_load(&f->cancel);i++) usleep(100000);
        if(atomic_load(&f->cancel)) return 0;
        char err[192]={0};ffaudio_info next={0};
        if(!netclock_insecure())netclock_ensure(clock_cancel,f,0);
        if(atomic_load(&f->cancel))return 0;
        f->backend=open_backend(f->path,1,&f->cancel,&next,err,sizeof err);
        if(!f->backend) continue;
        n=p_read(f->backend,out,frames);
        if(n>0) { f->info=next;f->good_since=ticks();return n; }
    }
    return 0;
}
int64_t ffaudio_seek(ffaudio *f,int64_t frame) {
    if(!f || !f->backend || f->live) return -1;
    return p_seek(f->backend,frame);
}
void ffaudio_title(ffaudio *f,char *out,size_t size) {
    if(!size) return;
    out[0]=0;
    if(f && f->backend) p_title(f->backend,out,size);
}
void ffaudio_cancel_active(void) {
    pthread_mutex_lock(&active_lock);
    if(active) atomic_store_explicit(&active->cancel,1,memory_order_release);
    pthread_mutex_unlock(&active_lock);
}
void ffaudio_close(ffaudio *f) {
    if(!f) return;
    pthread_mutex_lock(&active_lock);
    if(active==f) active=NULL;
    atomic_store(&f->cancel,1);
    pthread_mutex_unlock(&active_lock);
    if(f->backend) p_close(f->backend);
    free(f->path);free(f);
}
int ffaudio_metadata(const char *path,track_meta *meta) {
    pthread_once(&init_once,load_bridge);
    if(!module || !p_metadata || !path || !meta) return 0;
    return p_metadata(path,meta);
}
