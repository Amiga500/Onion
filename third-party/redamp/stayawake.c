#define _POSIX_C_SOURCE 200809L
#include "stayawake.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>

/* Both markers are global keymon conventions. Keep separate ownership records
 * and hold the created inode open to prevent reuse if keymon replaces a flag. */
static int owned_fd=-1, owned_mode=-1, last_mode=-1;
static ino_t owned_ino;
static dev_t owned_dev;
static double retry_at;
static double clock_s(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static const char *owner_path(int standby) {
#ifdef M2D_HOST
    const char *test=getenv(standby?"MA_STAY_AWAKE_OWNER":"MA_VIDEO_PLAYING_OWNER");
    if(test&&*test)return test;
#endif
    return standby?"/tmp/redamp-stay_awake.owner":"/tmp/redamp-video_playing.owner";
}
static const char *flag_path(int standby) {
#ifdef M2D_HOST
    const char *test=getenv(standby?"MA_STAY_AWAKE_FLAG":"MA_VIDEO_PLAYING_FLAG");
    if(test&&*test)return test;
#endif
    return standby?"/tmp/stay_awake":"/tmp/video_playing";
}
static int is_same(int mode, ino_t ino, dev_t dev) {
    struct stat st;
    return lstat(flag_path(mode), &st)==0 && st.st_ino==ino && st.st_dev==dev;
}
static void cleanup_mode(int mode) {
    const char *path=owner_path(mode);
    FILE *f=fopen(path,"r");
    if(!f)return;
    long pid=0;unsigned long long ino=0,dev=0;
    int fields=fscanf(f,"%ld %llu %llu",&pid,&ino,&dev);
    fclose(f);
    if(fields<2||pid<=0){unlink(path);return;}
    if((long)(pid_t)pid!=pid||kill((pid_t)pid,0)==0||errno!=ESRCH)return;
    /* Accept the previous stay_awake owner format for upgrades only. */
    struct stat st;
    if(fields==2 && lstat(flag_path(mode),&st)==0)dev=(unsigned long long)st.st_dev;
    if(is_same(mode,(ino_t)ino,(dev_t)dev))unlink(flag_path(mode));
    unlink(path);
}
void stayawake_cleanup_stale(void) {
    cleanup_mode(0);cleanup_mode(1);
}
void stayawake_release(void) {
    if(owned_fd>=0){
        const char *path=owner_path(owned_mode);
        FILE *f=fopen(path,"r");
        long pid=0;unsigned long long ino=0,dev=0;
        int match=f&&fscanf(f,"%ld %llu %llu",&pid,&ino,&dev)==3&&
            pid==(long)getpid()&&(ino_t)ino==owned_ino&&(dev_t)dev==owned_dev;
        if(f)fclose(f);
        if(match){
            if(is_same(owned_mode,owned_ino,owned_dev))unlink(flag_path(owned_mode));
            unlink(path);
        }
        close(owned_fd);owned_fd=-1;owned_mode=-1;
    }
    retry_at=0;last_mode=-1;
}
void stayawake_update(int playing,int standby) {
    standby=!!standby;
    if(!playing){stayawake_release();return;}
    if(last_mode!=standby){stayawake_release();last_mode=standby;}
    double t=clock_s();
    if(t<retry_at)return;
    retry_at=t+2.0;
    if(owned_fd>=0){
        if(is_same(standby,owned_ino,owned_dev))return;
        stayawake_release();last_mode=standby;retry_at=t+2.0;
    }
    cleanup_mode(standby);
    int fd=open(flag_path(standby),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0644);
    if(fd<0)return;
    struct stat st;
    if(fstat(fd,&st)!=0){close(fd);return;}
    const char *path=owner_path(standby);
    int ofd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW|O_CLOEXEC,0600);
    FILE *f=ofd>=0?fdopen(ofd,"w"):NULL;
    if(ofd>=0&&!f)close(ofd);
    int opened=f!=NULL;
    int ok=0;
    if(f){
        ok=fprintf(f,"%ld %llu %llu\n",(long)getpid(),(unsigned long long)st.st_ino,
                   (unsigned long long)st.st_dev)>0;
        if(fclose(f)!=0)ok=0;
    }
    if(!ok){
        if(is_same(standby,st.st_ino,st.st_dev))unlink(flag_path(standby));
        if(opened)unlink(path);
        close(fd);return;
    }
    owned_fd=fd;owned_mode=standby;owned_ino=st.st_ino;owned_dev=st.st_dev;
}
