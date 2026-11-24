#include "covers.h"
#include "netclock.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
int album_art_embedded_exists(const char *path){(void)path;return 0;}
#define STB_IMAGE_IMPLEMENTATION
/* Separate test TU supplies the image decoder used by the production skin renderer. */
#include "stb_image.h"
int main(int argc,char **argv)
{
    if(argc<5)return 2;
    if(getenv("REDAMP_TEST_INSECURE"))netclock_set_insecure(1);
    cover_job j[2]={{argv[1],argv[2],argv[3],!getenv("REDAMP_TEST_MIXED")}, {0}};
    int count=1;
    if(argc>=6 && strcmp(argv[5],"cancel")) {
        j[1]= (cover_job){argv[5],argv[2],"Another Album",!getenv("REDAMP_TEST_MIXED")};count=2;
    }
    if(!(getenv("REDAMP_TEST_MANUAL")?covers_start_manual(j,count,atoi(argv[4]),getenv("MA_DATA")):
        covers_start(j,count,atoi(argv[4]),getenv("MA_DATA"))))return 3;
    for(int i=0;i<9000;i++){
        if(argc>=6 && !strcmp(argv[5],"cancel") && i==3)covers_cancel();
        int done=0,total=0,running=0,updated=0;
        covers_poll(&done,&total,&running,&updated);
        if(!running){printf("completed=%d total=%d downloaded=%d\n",done,total,updated);break;}
        usleep(20000);
    }
    covers_shutdown();cover_result result;
    if(!covers_take_result(&result))return 4;
    printf("result saved=%d existing=%d missing=%d skipped=%d failed=%d status=%d\n",
           result.saved,result.existing,result.missing,result.skipped,result.failed,result.status);
    return 0;
}

/* Inject ENOSPC without filling the host file system. */
ssize_t __real_write(int,const void*,size_t);
ssize_t __wrap_write(int fd,const void *buf,size_t n)
{
    if(getenv("REDAMP_TEST_ENOSPC")){errno=ENOSPC;return -1;}
    return __real_write(fd,buf,n);
}
