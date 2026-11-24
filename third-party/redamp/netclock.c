/* One bounded NTP operation shared by radio, FFmpeg and cover workers.
 * Never change the host clock in desktop builds. No shell, daemon or SD writes. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include "netclock.h"
#include <time.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef NETCLOCK_TEST
extern time_t test_wall(void);
#define wall_time() test_wall()
#else
#define wall_time() time(NULL)
#endif
/* A clock years behind the release cannot validate modern TLS certificates.
 * The upper bound also catches nonsensical dates on 32-bit time_t systems. */
int netclock_valid(void){time_t t=wall_time();return t>=1788220800LL&&t<2145916800LL;}
static atomic_int syncing, insecure;
void netclock_set_insecure(int enabled){atomic_store(&insecure,!!enabled);}
int netclock_insecure(void){return atomic_load(&insecure);}
#if !defined(M2D_HOST) || defined(NETCLOCK_TEST)
static pthread_mutex_t worker_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_t worker;
static int worker_created;
static atomic_int closing;
#endif
int netclock_syncing(void){return atomic_load(&syncing);}
/* A usable IPv4 default route and an up, addressed interface are necessary
 * for Internet access on Miyoo. This is a local check, not proof of WAN access.
 * No network traffic, Wi-Fi toggle, ping, or DNS lookup is needed. */
int netclock_network_available(void)
{
#ifdef NETCLOCK_TEST
    extern int test_network(void);
    return test_network();
#elif defined(M2D_HOST)
    return getenv("REDAMP_TEST_OFFLINE")==NULL; /* Desktop tests use loopback mocks. */
#else
    FILE *f=fopen("/proc/net/route","r");
    if(!f)return 0;
    int fd=socket(AF_INET,SOCK_DGRAM,0),ok=0;
    char line[256];
    while(fd>=0&&fgets(line,sizeof line,f)){
        char name[IFNAMSIZ];unsigned long dest,mask;unsigned flags;
        if(sscanf(line,"%15s %lx %*s %x %*s %*s %*s %lx",name,&dest,&flags,&mask)!=4 ||
           dest||mask||!(flags&1)||(flags&0x200)||!strcmp(name,"lo"))continue;
        struct ifreq req={0};snprintf(req.ifr_name,sizeof req.ifr_name,"%s",name);
        if(ioctl(fd,SIOCGIFFLAGS,&req)<0||!(req.ifr_flags&IFF_UP)||!(req.ifr_flags&IFF_RUNNING))continue;
        if(ioctl(fd,SIOCGIFADDR,&req)<0||req.ifr_addr.sa_family!=AF_INET)continue;
        uint32_t addr=ntohl(((struct sockaddr_in *)&req.ifr_addr)->sin_addr.s_addr);
        if(!addr||(addr>>24)==127||(addr>>16)==0xa9fe)continue;
        ok=1;break;
    }
    if(fd>=0)close(fd);
    fclose(f);return ok;
#endif
}
#if !defined(M2D_HOST) || defined(NETCLOCK_TEST)
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static double next_attempt;
static int synced;
static double mono(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void nap(void){struct timespec t={0,100000000};nanosleep(&t,NULL);}
static int stopped(int (*cancel)(void *),void *ctx){return cancel&&cancel(ctx);}
/* Give each attempt a deadline; always reap the exact child we spawned. */
static int run(const char *path,char *const argv[],double until,int (*cancel)(void *),void *ctx)
{
#ifdef NETCLOCK_TEST
    char *const env[]={"PATH=/usr/bin:/bin",NULL};
#else
    char *const env[]={"PATH=/mnt/SDCARD/.tmp_update/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        "LD_LIBRARY_PATH=/lib:/config/lib:/customer/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/.tmp_update/lib/parasyte",NULL};
#endif
    int last=0;while(argv[last+1])last++;
    fprintf(stderr,"[clock] trying %s (%s)\n",path,argv[last]);
    pid_t pid;int status=0;
    int rc=posix_spawn(&pid,path,NULL,NULL,argv,env);
    if(rc){fprintf(stderr,"[clock] %s: %s\n",path,strerror(rc));return 0;}
    while(mono()<until&&!stopped(cancel,ctx)){
        pid_t w=waitpid(pid,&status,WNOHANG);
        if(w==pid){
            fprintf(stderr,"[clock] %s exit=%d\n",path,WIFEXITED(status)?WEXITSTATUS(status):-1);
            return WIFEXITED(status)&&WEXITSTATUS(status)==0;
        }
        if(w<0&&errno!=EINTR)return 0;
        nap();
    }
    fprintf(stderr,"[clock] %s %s\n",path,stopped(cancel,ctx)?"cancelled":"timed out");
    kill(pid,SIGKILL);
    while(waitpid(pid,&status,0)<0&&errno==EINTR){}
    return 0;
}
/* Last resort for firmware with missing/broken NTP binaries or DNS. NTP is
 * unauthenticated, like ntpdate; never relax TLS verification for time sync.
 * Connected UDP filters the source address/port; a random originate token
 * prevents accepting unrelated or stale responses. No DNS can block us. */
static int sntp(const char *ip,unsigned port,double until,int (*cancel)(void *),void *ctx)
{
    int fd=socket(AF_INET,SOCK_DGRAM,0);
    if(fd<0)return 0;
    unsigned char q[48]={0},r[512];q[0]=0x23; /* NTPv4 client */
    int random=open("/dev/urandom",O_RDONLY);
    if(random<0){close(fd);return 0;}
    ssize_t n=read(random,q+40,8);close(random);
    if(n!=8){close(fd);return 0;}
    struct sockaddr_in server={0};server.sin_family=AF_INET;server.sin_port=htons((uint16_t)port);
    if(inet_pton(AF_INET,ip,&server.sin_addr)!=1||connect(fd,(struct sockaddr *)&server,sizeof server)<0||
       send(fd,q,sizeof q,0)!=(ssize_t)sizeof q){close(fd);return 0;}
    fprintf(stderr,"[clock] SNTP fallback %s\n",ip);
    int ok=0;
    while(mono()<until&&!stopped(cancel,ctx)){
        struct pollfd p={.fd=fd,.events=POLLIN};
        int ready=poll(&p,1,100);
        if(ready<0&&errno==EINTR)continue;
        if(ready<0||(p.revents&(POLLERR|POLLHUP|POLLNVAL)))break;
        if(!(p.revents&POLLIN))continue;
        n=recv(fd,r,sizeof r,MSG_DONTWAIT);
        if(n<48||(r[0]>>6)==3||((r[0]>>3)&7)<3||((r[0]>>3)&7)>4||
           (r[0]&7)!=4||r[1]<1||r[1]>15||memcmp(r+24,q+40,8))continue;
        uint32_t secs,frac;memcpy(&secs,r+40,4);memcpy(&frac,r+44,4);
        /* Current releases support dates before 2038; NTP era 0 ends in 2036.
         * Unfold era 1 for the remaining representable dates. */
        uint64_t ntp=ntohl(secs);if(!ntp&&!frac)continue;
        if(ntp<2208988800ULL)ntp+=4294967296ULL;
        uint64_t unix_sec=ntp-2208988800ULL;
        if(unix_sec<1788220800ULL||unix_sec>=2145916800ULL)continue;
        if(stopped(cancel,ctx))break;
        if(netclock_valid()){ok=1;break;} /* OS may have synchronized meanwhile. */
        struct timespec stamp={.tv_sec=(time_t)unix_sec,.tv_nsec=(long)(((uint64_t)ntohl(frac)*1000000000ULL)>>32)};
#ifdef NETCLOCK_TEST
        extern int test_set_clock(const struct timespec *);
        if(test_set_clock(&stamp)==0)ok=netclock_valid();
#else
        if(clock_settime(CLOCK_REALTIME,&stamp)==0)ok=netclock_valid();
#endif
        else fprintf(stderr,"[clock] SNTP clock_settime: %s\n",strerror(errno));
        break;
    }
    close(fd);return ok;
}
#ifdef NETCLOCK_TEST
int test_sntp(unsigned port){return sntp("127.0.0.1",port,mono()+1.5,NULL,NULL);}
#endif
#endif
#if !defined(M2D_HOST) || defined(NETCLOCK_TEST)
static int sync_now(int (*cancel)(void *),void *ctx,int force)
{
    if(!force&&netclock_valid())return 1;
    if(!netclock_network_available())return 0;
    while(pthread_mutex_trylock(&lock)!=0){
        if(stopped(cancel,ctx))return 0;
        nap();
    }
    if((netclock_valid()&&(!force||synced))||mono()<next_attempt||stopped(cancel,ctx)||!netclock_network_available()){
        int valid=netclock_valid();pthread_mutex_unlock(&lock);return valid;
    }
    fprintf(stderr,"[clock] NTP sync requested (epoch=%lld)\n",(long long)wall_time());
#ifdef NETCLOCK_TEST
    const char *ntpdate[]={NETCLOCK_TEST_PATH,NULL};
#else
    const char *ntpdate[]={"/mnt/SDCARD/.tmp_update/bin/ntpdate","/usr/sbin/ntpdate",
        "/usr/bin/ntpdate","/sbin/ntpdate","/bin/ntpdate",
        "/mnt/SDCARD/miyoo/bin/ntpdate","/customer/app/ntpdate",NULL};
#endif
    const char *path=NULL;
    for(int i=0;ntpdate[i];i++)if(access(ntpdate[i],X_OK)==0){path=ntpdate[i];break;}
    const char *servers[]={"time.google.com","pool.ntp.org"};
    double deadline=mono()+20.0;
    int ok=0;
    /* Onion uses ntpdate -t 3 -u; allow the client time to complete its
     * samples. Do not assume an executable necessarily works on this firmware. */
    for(int i=0;path&&i<2&&!stopped(cancel,ctx)&&netclock_network_available()&&mono()<deadline;i++){
        double until=mono()+5.0;if(until>deadline)until=deadline;
        char *args[]={(char *)path,"-b","-p","2","-t","3","-u",(char *)servers[i],NULL};
        ok=run(path,args,until,cancel,ctx);
        if(netclock_valid()){synced=ok;break;}
    }
    if(!netclock_valid()&&!stopped(cancel,ctx)&&netclock_network_available()&&mono()<deadline){
#ifdef NETCLOCK_TEST
        const char *bb=NETCLOCK_TEST_BUSYBOX;
#else
        const char *bb=access("/bin/busybox",X_OK)==0?"/bin/busybox":"/usr/bin/busybox";
#endif
        if(access(bb,X_OK)==0){
            double until=mono()+4.0;if(until>deadline)until=deadline;
            char *args[]={(char *)bb,"ntpd","-n","-q","-p","time.google.com",NULL};
            ok=run(bb,args,until,cancel,ctx);
            if(ok&&netclock_valid())synced=1;
        }
    }
#ifndef NETCLOCK_TEST
    const char *ips[]={"216.239.35.0","216.239.35.4"}; /* Google Public NTP */
    for(int i=0;i<2&&!netclock_valid()&&!stopped(cancel,ctx)&&netclock_network_available()&&mono()<deadline;i++){
        double until=mono()+2.5;if(until>deadline)until=deadline;
        if(sntp(ips[i],123,until,cancel,ctx))synced=1;
    }
#endif
    int valid=netclock_valid();
    /* Do not restart a failed shared job on every station selection. */
    next_attempt=mono()+(stopped(cancel,ctx)?2.0:30.0);
    fprintf(stderr,"[clock] %s (epoch=%lld)\n",synced?"NTP synchronized":valid?"Clock plausible; NTP not confirmed":"NTP failed; check Wi-Fi/system date",(long long)wall_time());
    pthread_mutex_unlock(&lock);
    return valid;
}

static int worker_cancel(void *ctx){(void)ctx;return atomic_load(&closing)||netclock_insecure();}
static void *clock_worker(void *ctx)
{
    sync_now(worker_cancel,NULL,(int)(intptr_t)ctx);
    atomic_store(&syncing,0);
    return NULL;
}
static void start_worker(int force)
{
    pthread_mutex_lock(&worker_lock);
    if(!netclock_insecure()&&!atomic_load(&closing)&&!atomic_load(&syncing)&&
       (force||!netclock_valid())&&mono()>=next_attempt&&netclock_network_available()){
        if(worker_created){pthread_join(worker,NULL);worker_created=0;}
        atomic_store(&syncing,1);
        int rc=pthread_create(&worker,NULL,clock_worker,(void *)(intptr_t)force);
        if(!rc)worker_created=1;
        else {atomic_store(&syncing,0);fprintf(stderr,"[clock] worker: %s\n",strerror(rc));}
    }
    pthread_mutex_unlock(&worker_lock);
}
#endif
void netclock_start(void)
{
#if !defined(M2D_HOST) || defined(NETCLOCK_TEST)
    if(!netclock_insecure())start_worker(0);
#endif
}
/* Consumers may abandon their wait without cancelling the shared clock job. */
int netclock_ensure(int (*cancel)(void *),void *ctx,int force)
{
#if defined(M2D_HOST) && !defined(NETCLOCK_TEST)
    (void)cancel;(void)ctx;(void)force;return 1;
#else
    if(!force&&netclock_valid())return 1;
    if(stopped(cancel,ctx)||!netclock_network_available())return 0;
    start_worker(force);
    while(atomic_load(&syncing)&&!netclock_insecure()&&!stopped(cancel,ctx)&&!atomic_load(&closing))nap();
    return !stopped(cancel,ctx)&&netclock_valid();
#endif
}
void netclock_shutdown(void)
{
#if !defined(M2D_HOST) || defined(NETCLOCK_TEST)
    pthread_mutex_lock(&worker_lock);
    atomic_store(&closing,1);
    if(worker_created){pthread_join(worker,NULL);worker_created=0;}
    pthread_mutex_unlock(&worker_lock);
#endif
}
