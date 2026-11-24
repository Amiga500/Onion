#define _POSIX_C_SOURCE 200809L
#include "netclock.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
time_t test_wall(void){return access("clock-valid",F_OK)==0?1795478400:123456;}
int test_network(void){return access("offline",F_OK)!=0;}
int test_set_clock(const struct timespec *t){
    assert(t->tv_sec==1795478400 && t->tv_nsec==500000000);
    FILE *f=fopen("clock-valid","w");assert(f);fclose(f);return 0;
}
extern int test_sntp(unsigned port);
static void *ensure(void *p){(void)p;assert(netclock_ensure(NULL,NULL,0));return NULL;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static int cancel(void *p){return now()-*(double *)p>0.25;}
static int calls(void){
    FILE *f=fopen("calls","r");if(!f)return 0;int n=0,c;
    while((c=fgetc(f))!=EOF)if(c=='\n')n++;
    fclose(f);return n;
}
static void *server(void *p){
    int fd=*(int *)p;unsigned char q[48],r[48]={0};struct sockaddr_in a;socklen_t n=sizeof a;
    assert(recvfrom(fd,q,sizeof q,0,(struct sockaddr *)&a,&n)==48);
    r[0]=0x24;r[1]=2;memcpy(r+24,q+40,8);
    uint32_t sec=htonl((uint32_t)(1795478400ULL+2208988800ULL)),frac=htonl(0x80000000U);
    memcpy(r+40,&sec,4);memcpy(r+44,&frac,4);
    uint32_t wrong=htonl((uint32_t)(1795478401ULL+2208988800ULL));
    memcpy(r+40,&wrong,4);
    r[24]^=1;assert(sendto(fd,r,48,0,(struct sockaddr *)&a,n)==48);r[24]^=1;
    r[1]=0;assert(sendto(fd,r,48,0,(struct sockaddr *)&a,n)==48);r[1]=2;
    memcpy(r+40,&sec,4);
    assert(sendto(fd,r,48,0,(struct sockaddr *)&a,n)==48);return NULL;
}
int main(int argc,char **argv){
    assert(argc==2);unlink("clock-valid");unlink("calls");
    if(!strcmp(argv[1],"success")){
        pthread_t a,b;assert(!netclock_valid());
        assert(!pthread_create(&a,NULL,ensure,NULL));assert(!pthread_create(&b,NULL,ensure,NULL));
        pthread_join(a,NULL);pthread_join(b,NULL);assert(netclock_valid());assert(!netclock_syncing());
        assert(netclock_ensure(NULL,NULL,0));assert(calls()==2);
    }else if(!strcmp(argv[1],"insecure")){
        netclock_set_insecure(1);netclock_start();
        assert(!netclock_valid()&&!netclock_syncing()&&calls()==0);
        netclock_set_insecure(0);assert(netclock_ensure(NULL,NULL,0));assert(calls()==2);
    }else if(!strcmp(argv[1],"offline")){
        FILE *f=fopen("offline","w");assert(f);fclose(f);
        double t=now();assert(!netclock_ensure(NULL,NULL,0));assert(now()-t<0.25);
        assert(!netclock_syncing()&&calls()==0);
        unlink("offline");assert(netclock_ensure(NULL,NULL,0));assert(calls()==2);
    }else if(!strcmp(argv[1],"busybox")){
        assert(netclock_ensure(NULL,NULL,0));assert(calls()==3);assert(netclock_valid());
    }else if(!strcmp(argv[1],"sntp")){
        int fd=socket(AF_INET,SOCK_DGRAM,0);assert(fd>=0);struct sockaddr_in a={0};
        a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        assert(!bind(fd,(struct sockaddr *)&a,sizeof a));socklen_t n=sizeof a;
        assert(!getsockname(fd,(struct sockaddr *)&a,&n));pthread_t th;
        assert(!pthread_create(&th,NULL,server,&fd));assert(test_sntp(ntohs(a.sin_port)));
        pthread_join(th,NULL);close(fd);assert(netclock_valid());
    }else if(!strcmp(argv[1],"switch")){
        netclock_start();
        double t=now();assert(!netclock_ensure(cancel,&t,0));assert(netclock_syncing());
        assert(netclock_ensure(NULL,NULL,0));assert(netclock_valid());assert(calls()==1);
    }else if(!strcmp(argv[1],"cancel")){
        double t=now();assert(!netclock_ensure(cancel,&t,0));assert(now()-t<1.5);assert(netclock_syncing());
        assert(calls()==1);
        netclock_shutdown();assert(!netclock_syncing());assert(calls()==1);
    }else{
        double t=now();assert(!netclock_ensure(NULL,NULL,0));assert(now()-t<20.5);assert(!netclock_syncing());
        t=now();assert(!netclock_ensure(NULL,NULL,0));assert(now()-t<0.25);
    }
    netclock_shutdown();
    printf("clock %s: PASS\n",argv[1]);return 0;
}
