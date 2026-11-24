/* Host-only ownership tests, using private paths for both keymon markers. */
#define _POSIX_C_SOURCE 200809L
#include "stayawake.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);return 1;} } while(0)
static int exists(const char *p){return access(p,F_OK)==0;}
static void delay(void){struct timespec t={2,100000000};nanosleep(&t,NULL);}
int main(void)
{
    char dir[]="/tmp/redamp-stayawake-test-XXXXXX",flag[128],owner[128],video[128],vowner[128];
    CHECK(mkdtemp(dir));
    snprintf(flag,sizeof flag,"%s/flag",dir);snprintf(owner,sizeof owner,"%s/owner",dir);
    snprintf(video,sizeof video,"%s/video",dir);snprintf(vowner,sizeof vowner,"%s/vowner",dir);
    CHECK(setenv("MA_STAY_AWAKE_FLAG",flag,1)==0&&setenv("MA_STAY_AWAKE_OWNER",owner,1)==0);
    CHECK(setenv("MA_VIDEO_PLAYING_FLAG",video,1)==0&&setenv("MA_VIDEO_PLAYING_OWNER",vowner,1)==0);
    FILE *f=fopen(flag,"w");CHECK(f);fclose(f);
    stayawake_update(1,1);CHECK(!exists(owner));
    CHECK(unlink(flag)==0);
    for(int i=0;i<100;i++)stayawake_update(1,1);
    CHECK(!exists(flag)); /* no repeated claim */
    delay();
    stayawake_update(1,1);CHECK(exists(flag)&&exists(owner));
    stayawake_update(1,0);CHECK(!exists(flag)&&!exists(owner)&&exists(video)&&exists(vowner));
    stayawake_update(1,1);CHECK(exists(flag)&&exists(owner)&&!exists(video)&&!exists(vowner));
    stayawake_update(0,1);CHECK(!exists(flag)&&!exists(owner));
    /* Never remove or take ownership of a marker created by another app. */
    f=fopen(flag,"w");CHECK(f);fclose(f);
    f=fopen(video,"w");CHECK(f);fclose(f);
    stayawake_update(1,1);stayawake_update(1,0);stayawake_release();
    CHECK(exists(flag)&&exists(video)&&!exists(owner)&&!exists(vowner));
    unlink(flag);unlink(video);
    /* Replacement must survive release, even if created immediately. */
    stayawake_update(1,0);CHECK(exists(video));
    CHECK(unlink(video)==0);f=fopen(video,"w");CHECK(f);fclose(f);
    stayawake_release();CHECK(exists(video)&&!exists(vowner));unlink(video);
    /* Both stale markers can be cleaned after a process dies. */
    for(int mode=0;mode<2;mode++){
        pid_t pid=fork();CHECK(pid>=0);
        if(pid==0){stayawake_update(1,mode);_exit(0);}
        CHECK(waitpid(pid,NULL,0)==pid);
        CHECK(exists(mode?flag:video));stayawake_cleanup_stale();
        CHECK(!exists(mode?flag:video)&&!exists(mode?owner:vowner));
    }
    CHECK(rmdir(dir)==0);
    puts("PASS both keymon markers, mode switches, foreign/replaced flags, retry and stale cleanup");return 0;
}
