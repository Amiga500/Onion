/* Asynchronous, rate-limited MusicBrainz/Cover Art Archive fetcher. No UI surfaces
 * or library pointers are touched by the worker. HTTPS is mandatory in production;
 * certificate verification defaults on. Local mock URLs are host-build only. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include "covers.h"
#include "netclock.h"
#include "cover_paths.h"
#include "album_art.h"
#include "stb_image.h"
#include <curl/curl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_DOWNLOAD (3*1024*1024)
typedef struct { char *path,*artist,*album; int folder_safe; } job;
typedef struct { unsigned char *bytes;size_t n,cap,max; long retry_after; } response;
static pthread_mutex_t mx=PTHREAD_MUTEX_INITIALIZER;
static pthread_t th;
static job *queue;
static int n_jobs, started, finished, cancelled, paused, completed, updated;
static int save_folder, retry_missing;
static cover_result final_result;
static int result_pending;
static int request_error;
static char data_path[1024],cover_dir[1100];
static double last_request;

static double clock_s(void)
{
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return t.tv_sec+(double)t.tv_nsec/1e9;
}
static int is_cancelled(void)
{
    pthread_mutex_lock(&mx);int c=cancelled;pthread_mutex_unlock(&mx);return c;
}
static int clock_cancel(void *ctx){(void)ctx;return is_cancelled();}
static int wait_ready(void)
{
    for(;;) {
        pthread_mutex_lock(&mx);int c=cancelled,p=paused;pthread_mutex_unlock(&mx);
        if(c)return 0;
        if(!p)return 1;
        usleep(100000);
    }
}
static int wait_rate(void)
{
    if(!wait_ready())return 0;
    while(clock_s()-last_request<1.5) {
        if(!wait_ready())return 0;
        usleep(50000);
    }
    last_request=clock_s();
    return 1;
}
static size_t receive_bytes(char *ptr,size_t sz,size_t num,void *user)
{
    response *r=user;
    if(sz && num>SIZE_MAX/sz)return 0;
    size_t n=sz*num;
    if(n>r->max-r->n)return 0;
    if(r->n+n+1>r->cap) {
        size_t cap=r->cap?r->cap*2:4096;
        while(cap<r->n+n+1)cap*=2;
        if(cap>r->max+1)cap=r->max+1;
        unsigned char *b=realloc(r->bytes,cap);
        if(!b)return 0;
        r->bytes=b;r->cap=cap;
    }
    memcpy(r->bytes+r->n,ptr,n);r->n+=n;r->bytes[r->n]=0;
    return n;
}
static int progress(void *ctx,curl_off_t a,curl_off_t b,curl_off_t c,curl_off_t d)
{
    (void)ctx;(void)a;(void)b;(void)c;(void)d;
    return !wait_ready();
}
/* Retry-After can be seconds or an HTTP date. Ignore malformed values and
 * reset on redirects so only the final response controls the retry. */
static size_t receive_header(char *ptr,size_t sz,size_t num,void *user)
{
    if(sz && num>SIZE_MAX/sz)return 0;
    size_t n=sz*num;response *r=user;
    if(n>=5&&!strncasecmp(ptr,"HTTP/",5))r->retry_after=0;
    if(n>12&&n<256&&!strncasecmp(ptr,"Retry-After:",12)){
        char buf[256];memcpy(buf,ptr+12,n-12);buf[n-12]=0;
        char *v=buf;while(isspace((unsigned char)*v))v++;
        char *end=v+strlen(v);while(end>v&&isspace((unsigned char)end[-1]))*--end=0;
        if(isdigit((unsigned char)*v)){
            errno=0;long sec=strtol(v,&end,10);
            if(!*end)r->retry_after=errno==ERANGE||sec>120?121:sec;
        }else if(netclock_valid()){
            time_t when=curl_getdate(v,NULL);double sec=difftime(when,time(NULL));
            if(when!=(time_t)-1&&sec>0)r->retry_after=sec>120?121:(long)sec;
        }else if(curl_getdate(v,NULL)!=(time_t)-1)r->retry_after=121;
    }
    return n;
}
static int get_url(const char *url,response *r,long *http)
{
    request_error=COVER_OK;
    CURL *c=curl_easy_init();if(!c){request_error=COVER_NETWORK;return 0;}
    char detail[CURL_ERROR_SIZE]={0};
    curl_easy_setopt(c,CURLOPT_ERRORBUFFER,detail);
    r->n=0;r->retry_after=0;*http=0;
    char ua[256];
    snprintf(ua,sizeof ua,"RedAmp/%s (https://github.com/Amiga500/Onion)",
             getenv("MA_VERSION")?getenv("MA_VERSION"):"1.1.11");
    curl_easy_setopt(c,CURLOPT_URL,url);
    curl_easy_setopt(c,CURLOPT_USERAGENT,ua);
    curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);
    curl_easy_setopt(c,CURLOPT_MAXREDIRS,5L);
#if LIBCURL_VERSION_NUM >= 0x075500
#ifdef M2D_HOST
    curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");
    curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");
#else
    curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"https");
    curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"https");
#endif
#else
#ifdef M2D_HOST
    curl_easy_setopt(c,CURLOPT_PROTOCOLS,(long)(CURLPROTO_HTTP|CURLPROTO_HTTPS));
    curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS,(long)(CURLPROTO_HTTP|CURLPROTO_HTTPS));
#else
    curl_easy_setopt(c,CURLOPT_PROTOCOLS,(long)CURLPROTO_HTTPS);
    curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS,(long)CURLPROTO_HTTPS);
#endif
#endif

    int insecure=netclock_insecure();
    if(!insecure&&!netclock_ensure(clock_cancel,NULL,0)){
        curl_easy_cleanup(c);request_error=COVER_CLOCK;return 0;
    }
    if(insecure)fprintf(stderr,"[covers] insecure HTTPS explicitly enabled\n");
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,insecure?0L:1L);
    curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,insecure?0L:2L);
    char ca[1200];
    const char *home=getenv("MA_HOME");
    if(!insecure&&home&&snprintf(ca,sizeof ca,"%s/cacert.pem",home)<(int)sizeof ca&&access(ca,R_OK)==0)
        curl_easy_setopt(c,CURLOPT_CAINFO,ca);
    curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,7L);
    curl_easy_setopt(c,CURLOPT_TIMEOUT,18L);
    curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,receive_header);
    curl_easy_setopt(c,CURLOPT_HEADERDATA,r);
    curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,receive_bytes);
    curl_easy_setopt(c,CURLOPT_WRITEDATA,r);
    curl_easy_setopt(c,CURLOPT_XFERINFOFUNCTION,progress);
    curl_easy_setopt(c,CURLOPT_NOPROGRESS,0L);
    CURLcode code=curl_easy_perform(c);
    curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,http);
    if(code!=CURLE_OK&&!is_cancelled()){
        request_error=(code==CURLE_PEER_FAILED_VERIFICATION||code==CURLE_SSL_CACERT_BADFILE)?
            (!netclock_valid()?COVER_CLOCK:COVER_TLS):
            code==CURLE_WRITE_ERROR?COVER_RESPONSE:COVER_NETWORK;
        fprintf(stderr,"[covers] request failed (curl=%d http=%ld): %s\n",code,*http,
                detail[0]?detail:curl_easy_strerror(code));
    }
    curl_easy_cleanup(c);
    return code==CURLE_OK;
}
/* Only the first release-group object, never a lower-ranked candidate. */
/* Look up only a direct object member. MusicBrainz can put nested artist
 * or release IDs before the release-group ID; never use those by accident. */
static const char *json_value(const char *obj,const char *key)
{
    int depth=0;
    size_t len=strlen(key);
    for(const char *p=obj;*p;p++){
        if(*p=='{'||*p=='[')depth++;
        else if(*p=='}'||*p==']'){if(--depth<=0)break;}
        else if(*p=='"'){
            const char *begin=++p;
            while(*p&&*p!='"'){if(*p=='\\'&&p[1])p++;p++;}
            if(!*p)return NULL;
            if(depth==1&&(size_t)(p-begin)==len&&!memcmp(begin,key,len)){
                const char *q=p+1;while(isspace((unsigned char)*q))q++;
                if(*q==':'){q++;while(isspace((unsigned char)*q))q++;return q;}
            }
        }
    }
    return NULL;
}
static int json_string(const char *obj,const char *key,char *out,size_t size)
{
    const char *p=json_value(obj,key);
    if(!p||*p++!='"')return 0;
    size_t i=0;
    while(*p&&*p!='"'&&i+1<size) {
        if(*p=='\\') {
            p++;if(!*p)return 0;
            if(*p=='u'){
                unsigned code=0;
                for(int k=0;k<4;k++){
                    unsigned char c=(unsigned char)*++p;
                    if(!isxdigit(c))return 0;
                    code=code*16+(isdigit(c)?c-'0':tolower(c)-'a'+10);
                }
                p++;
                if(!code||(code>=0xd800&&code<=0xdfff))return 0;
                size_t bytes=code<128?1:code<2048?2:3;
                if(i+bytes>=size)return 0;
                if(bytes==1)out[i++]=(char)code;
                else if(bytes==2){out[i++]=(char)(0xc0|(code>>6));out[i++]=(char)(0x80|(code&63));}
                else {out[i++]=(char)(0xe0|(code>>12));out[i++]=(char)(0x80|((code>>6)&63));out[i++]=(char)(0x80|(code&63));}
                continue;
            }
            if(*p!='"'&&*p!='\\'&&*p!='/')return 0;
        }
        out[i++]=*p++;
    }
    if(*p!='"')return 0;
    out[i]=0;return 1;
}
static int parse_best(const char *json,int *score,char *id,size_t idcap,char *artist,size_t acap,char *title,size_t tcap)
{
    const char *p=json_value(json,"release-groups");
    if(!p||*p++!='[')return 0;
    while(isspace((unsigned char)*p))p++;
    if(*p!='{')return 0;
    /* Isolate the first JSON object, honoring quotes and nested structures. */
    const char *q=p;int depth=0,in=0,esc=0;
    for(;*q;q++) {
        if(in) {if(esc)esc=0;else if(*q=='\\')esc=1;else if(*q=='"')in=0;}
        else if(*q=='"')in=1;
        else if(*q=='{')depth++;
        else if(*q=='}'&&!--depth){q++;break;}
    }
    if(depth||q-p>40000||q==p)return 0;
    char *obj=strndup(p,(size_t)(q-p));if(!obj)return 0;
    const char *sc=json_value(obj,"score");
    if(!sc||!isdigit((unsigned char)*sc)) {free(obj);return 0;}
    *score=atoi(sc);
    int ok=json_string(obj,"id",id,idcap)&&json_string(obj,"title",title,tcap);
    const char *ac=json_value(obj,"artist-credit");
    if(!ac)ok=0;
    else {
        const char *first=*ac=='['?ac+1:NULL;
        if(first)while(isspace((unsigned char)*first))first++;
        if(!first||*first!='{'||!json_string(first,"name",artist,acap))ok=0;
    }
    free(obj);
    if(!ok||strlen(id)!=36)return 0;
    for(size_t i=0;i<36;i++)if(!isxdigit((unsigned char)id[i])&&id[i]!='-')return 0;
    return 1;
}
static const char *trim_the(const char *s)
{
    while(isspace((unsigned char)*s))s++;
    return !strncasecmp(s,"The ",4)?s+4:s;
}
static int artist_matches(const char *a,const char *b)
{
    a=trim_the(a);b=trim_the(b);
    size_t n=strlen(a),m=strlen(b);
    while(n&&isspace((unsigned char)a[n-1]))n--;
    while(m&&isspace((unsigned char)b[m-1]))m--;
    return n==m&&!strncasecmp(a,b,n);
}
static int image_ok(const unsigned char *data,size_t n)
{
    if(n<32||n>MAX_DOWNLOAD)return 0;
    int x=0,y=0,c=0;
    if(!stbi_info_from_memory(data,(int)n,&x,&y,&c)||x<1||y<1||x>1024||y>1024||
       (size_t)x*y>1024u*1024u)return 0;
    unsigned char *pixels=stbi_load_from_memory(data,(int)n,&x,&y,&c,4);
    if(!pixels)return 0;
    stbi_image_free(pixels);return 1;
}
static int write_atomic(const char *path,const unsigned char *data,size_t n)
{
    char tmp[1400];if(snprintf(tmp,sizeof tmp,"%s.tmp.XXXXXX",path)>=(int)sizeof tmp)return 0;
    int fd=mkstemp(tmp);if(fd<0)return 0;
    int ok=1;size_t pos=0;
    while(pos<n) {
        ssize_t w=write(fd,data+pos,n-pos);
        if(w<=0){ok=0;break;}pos+=(size_t)w;
    }
    if(fsync(fd))ok=0;
    if(close(fd))ok=0;
    /* FAT32 SD cards do not support hard links; same-directory rename is atomic.
     * Recheck the destination so a newly added user image is not overwritten. */
    if(ok&&access(path,F_OK)!=0&&errno==ENOENT&&rename(tmp,path)==0)return 1;
    unlink(tmp);return 0;
}
/* Load the negative cache once per worker run. A sorted array keeps lookups
 * logarithmic; compact expired/duplicate records with a FAT-safe rename. */
typedef struct { char hash[17]; time_t when; } missing_entry;
static missing_entry *missing;
static size_t missing_n, missing_cap;
static int missing_cmp(const void *a,const void *b)
{
    return strcmp(((const missing_entry *)a)->hash,((const missing_entry *)b)->hash);
}
static int missing_hash(const job *j, char hash[17])
{return cover_album_key(j->artist,j->album,hash); }
static int missing_push(missing_entry e)
{
    if(missing_n==missing_cap){
        size_t cap=missing_cap?missing_cap*2:128;
        missing_entry *p=realloc(missing,cap*sizeof *p);
        if(!p)return 0;
        missing=p;missing_cap=cap;
    }
    missing[missing_n++]=e;return 1;
}
static void missing_load(void)
{
    if(!netclock_valid())return; /* do not expire records against a bogus clock */
    char path[1200],line[128];snprintf(path,sizeof path,"%s/missing-v2.txt",cover_dir);
    FILE *f=fopen(path,"r");if(!f)return;
    time_t now=time(NULL);int compact=0,ok=1;
    while(fgets(line,sizeof line,f)){
        missing_entry e={0};char hash[24];unsigned long long t;
        if(sscanf(line,"%23s %llu",hash,&t)!=2||strlen(hash)!=16||
           strspn(hash,"0123456789abcdefABCDEF")!=16||t>(unsigned long long)now||
           (unsigned long long)now-t>=30ULL*86400){compact=1;continue;}
        memcpy(e.hash,hash,16);e.when=(time_t)t;
        if(!missing_push(e)){ok=0;break;}
    }
    if(ferror(f))ok=0;
    fclose(f);
    if(missing_n>1)qsort(missing,missing_n,sizeof *missing,missing_cmp);
    size_t n=0;
    for(size_t i=0;i<missing_n;i++){
        if(n&&!strcmp(missing[n-1].hash,missing[i].hash)){
            if(missing[i].when>missing[n-1].when)missing[n-1]=missing[i];
            compact=1;
        }else missing[n++]=missing[i];
    }
    missing_n=n;
    if(!ok||!compact)return; /* never replace a partially read file */
    char tmp[1240];snprintf(tmp,sizeof tmp,"%s.XXXXXX",path);
    int fd=mkstemp(tmp);if(fd<0)return;
    f=fdopen(fd,"w");if(!f){close(fd);unlink(tmp);return;}
    for(size_t i=0;i<missing_n;i++)
        if(fprintf(f,"%s %llu\n",missing[i].hash,(unsigned long long)missing[i].when)<0)ok=0;
    if(fflush(f)||fsync(fd))ok=0;
    if(fclose(f))ok=0;
    if(!ok||rename(tmp,path))unlink(tmp);
}
static int is_missing(const job *j)
{
    missing_entry e={0};if(!missing_hash(j,e.hash)||!missing_n)return 0;
    missing_entry *found=bsearch(&e,missing,missing_n,sizeof *missing,missing_cmp);
    return found&&time(NULL)-found->when<30*86400;
}
static void mark_missing(const job *j)
{
    if(!netclock_valid())return;
    missing_entry e={0};if(!missing_hash(j,e.hash))return;
    e.when=time(NULL);
    if(missing_push(e))qsort(missing,missing_n,sizeof *missing,missing_cmp);
    char m[1200];snprintf(m,sizeof m,"%s/missing-v2.txt",cover_dir);
    FILE *f=fopen(m,"a");if(!f)return;
    fprintf(f,"%s %llu\n",e.hash,(unsigned long long)e.when);fclose(f);
}
static int exists(const job *j)
{
    static const char *names[]={"cover.jpg","cover.jpeg","cover.png","folder.jpg", "folder.png",
                                "front.jpg","front.png",NULL};
    char full[1400];struct stat st;
    for(int i=0;j->folder_safe&&names[i];i++)if(cover_folder_image(j->path,names[i],full,sizeof full)&&
           !stat(full,&st)&&S_ISREG(st.st_mode))return 1;
    if(cover_cache_path(j->artist,j->album,data_path,full,sizeof full)&&!stat(full,&st)&&S_ISREG(st.st_mode))return 1;
    if(cover_album_folder_image(j->path,j->artist,j->album,full,sizeof full)&&!stat(full,&st)&&S_ISREG(st.st_mode))return 1;
    return album_art_embedded_exists(j->path);
}
static void clean_queue(void)
{
    for(int i=0;i<n_jobs;i++){free(queue[i].path);free(queue[i].artist);free(queue[i].album);}
    free(queue);queue=NULL;n_jobs=0;
}
static int query_phrase(const char *src,char *out,size_t cap)
{
    size_t n=0;
    while(*src){
        if(*src=='"'||*src=='\\'){if(n+1>=cap)return 0;out[n++]='\\';}
        if(n+1>=cap)return 0;
        out[n++]=*src++;
    }
    out[n]=0;return 1;
}
/* Only definitive absence is cached. Network errors, malformed responses and
 * failed writes must never suppress a subsequent fetch for 30 days. */
static int request(const char *url,response *r,long *http)
{
    int ok=0;
    static const int delays[]={5,15,30};
    for(int attempt=0;attempt<4;attempt++){
        if(!wait_rate())break;
        ok=get_url(url,r,http);
        if(!ok||(*http!=503&&*http!=429))break;
        if(attempt==3||r->retry_after>120){
            fprintf(stderr,"[covers] HTTP %ld, stopping batch (Retry-After=%ld)\n",*http,r->retry_after);
            request_error=COVER_BUSY;break;
        }
        int delay=delays[attempt];
        if(r->retry_after>delay)delay=(int)r->retry_after;
        fprintf(stderr,"[covers] HTTP %ld, retry %d/3 in %d seconds\n",*http,attempt+1,delay);
        double due=clock_s()+delay;
        while(clock_s()<due){if(!wait_ready())return 0;usleep(100000);}
    }
    return ok;
}
static void *worker(void *unused)
{
    (void)unused;
    cover_result res={.total=n_jobs};
    int prepared=0;
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK){res.status=COVER_NETWORK;goto done;}
    for(int i=0;i<n_jobs;i++) {
        if(!wait_ready())break;
        job *j=&queue[i];
        if(exists(j)){res.existing++;goto next;}
        if(!*j->artist||!*j->album||!strcmp(j->artist,"Unknown Artist")||!strcmp(j->album,"Unknown Album")){
            fprintf(stderr,"[covers] skipped missing tags: %s\n",j->path);res.skipped++;goto next;
        }
        if(!prepared){
            if(!netclock_network_available()||(!netclock_insecure()&&!netclock_ensure(clock_cancel,NULL,0)&&!netclock_insecure())){
                if(!is_cancelled())res.status=netclock_network_available()?COVER_CLOCK:COVER_NETWORK;
                break;
            }
            missing_load();prepared=1;
        }
        if(!retry_missing&&is_missing(j)){res.missing++;goto next;}
        fprintf(stderr,"[covers] searching %s / %s\n",j->artist,j->album);
        CURL *esc=curl_easy_init();if(!esc){res.status=COVER_NETWORK;break;}
        char query[768],album[352],artist_query[352];
        int qn=query_phrase(j->album,album,sizeof album)&&query_phrase(j->artist,artist_query,sizeof artist_query)?
            snprintf(query,sizeof query,"releasegroup:\"%s\" AND artist:\"%s\"",album,artist_query):-1;
        char *encoded=qn>=0&&qn<(int)sizeof query?curl_easy_escape(esc,query,0):NULL;
        char url[3000];
#ifdef M2D_HOST
        const char *base=getenv("REDAMP_MB_URL");if(!base||!*base)base="https://musicbrainz.org/ws/2/release-group";
#else
        const char *base="https://musicbrainz.org/ws/2/release-group";
#endif
        int urlok=encoded&&snprintf(url,sizeof url,"%s/?query=%s&fmt=json&limit=1",base,encoded)<(int)sizeof url;
        if(encoded)curl_free(encoded);
        curl_easy_cleanup(esc);
        if(!urlok){res.skipped++;goto next;}
        response r={.max=256*1024};long http=0;
        int ok=request(url,&r,&http);
        if(!ok){free(r.bytes);if(!is_cancelled())res.status=request_error?request_error:COVER_NETWORK;break;}
        if(http!=200||!r.n){
            fprintf(stderr,"[covers] MusicBrainz HTTP %ld\n",http);
            free(r.bytes);res.status=request_error==COVER_BUSY?COVER_BUSY:COVER_SERVER;break;
        }
        int score=0;char id[64]={0},artist[256]={0};
        char title[256]={0};
        int parsed=parse_best((const char *)r.bytes,&score,id,sizeof id,artist,sizeof artist,title,sizeof title);
        const char *groups=json_value((const char *)r.bytes,"release-groups");
        const char *arr=groups&&*groups=='['?groups:NULL;
        if(arr){arr++;while(isspace((unsigned char)*arr))arr++;}
        int empty=arr&&*arr==']';
        free(r.bytes);
        if(!parsed&&!empty){
            fprintf(stderr,"[covers] unexpected MusicBrainz response\n");
            res.failed++;res.status=COVER_RESPONSE;goto next;
        }
        if(empty||score<90||!artist_matches(j->artist,artist)||strcasecmp(j->album,title)){
            fprintf(stderr,"[covers] no confident match (score=%d artist=%s album=%s)\n",score,artist,title);
            mark_missing(j);res.missing++;goto next;
        }
#ifdef M2D_HOST
        const char *archive=getenv("REDAMP_CAA_URL");if(!archive||!*archive)archive="https://coverartarchive.org/release-group";
#else
        const char *archive="https://coverartarchive.org/release-group";
#endif
        if(snprintf(url,sizeof url,"%s/%s/front-500",archive,id)>=(int)sizeof url){res.skipped++;goto next;}
        response img={.max=MAX_DOWNLOAD};http=0;
        ok=request(url,&img,&http);
        if(!ok){free(img.bytes);if(!is_cancelled())res.status=request_error?request_error:COVER_NETWORK;break;}
        if(http==404){fprintf(stderr,"[covers] no front image for %s\n",id);mark_missing(j);res.missing++;free(img.bytes);goto next;}
        if(http!=200){fprintf(stderr,"[covers] Cover Art Archive HTTP %ld\n",http);free(img.bytes);res.status=request_error==COVER_BUSY?COVER_BUSY:COVER_SERVER;break;}
        if(!image_ok(img.bytes,img.n)){
            fprintf(stderr,"[covers] invalid or oversized image for %s\n",id);
            res.failed++;res.status=COVER_RESPONSE;
        }else if(exists(j))res.existing++;
        else {
            char dest[1400];
            int valid=cover_cache_path(j->artist,j->album,data_path,dest,sizeof dest);
            if(valid&&write_atomic(dest,img.bytes,img.n)) {
                fprintf(stderr,"[covers] saved %s\n",dest);res.saved++;
                if(save_folder){
                    char local[1400];
                    if(cover_album_folder_image(j->path,j->artist,j->album,local,sizeof local)&&
                       access(local,F_OK)!=0&&!write_atomic(local,img.bytes,img.n)){
                        fprintf(stderr,"[covers] cannot save folder copy: %s\n",local);
                        res.failed++;res.status=COVER_STORAGE;
                    }
                }
                pthread_mutex_lock(&mx);updated++;pthread_mutex_unlock(&mx);
            }else{
                fprintf(stderr,"[covers] could not save %s\n",valid?dest:j->path);
                res.failed++;res.status=COVER_STORAGE;
            }
        }
        free(img.bytes);
next:
        res.checked++;
        pthread_mutex_lock(&mx);completed++;pthread_mutex_unlock(&mx);
    }
done:
    free(missing);missing=NULL;missing_n=missing_cap=0;
    if(is_cancelled())res.status=COVER_CANCELLED;
    fprintf(stderr,"[covers] finished: checked=%d/%d saved=%d existing=%d missing=%d skipped=%d failed=%d status=%d\n",
            res.checked,res.total,res.saved,res.existing,res.missing,res.skipped,res.failed,res.status);
    pthread_mutex_lock(&mx);final_result=res;result_pending=1;finished=1;pthread_mutex_unlock(&mx);
    return NULL;
}
int covers_take_result(cover_result *out)
{
    pthread_mutex_lock(&mx);int ready=result_pending;
    if(ready&&out){*out=final_result;result_pending=0;}
    pthread_mutex_unlock(&mx);return ready;
}
void covers_cancel(void)
{
    pthread_mutex_lock(&mx);cancelled=1;pthread_mutex_unlock(&mx);
}
void covers_set_paused(int p)
{
    pthread_mutex_lock(&mx);paused=!!p;pthread_mutex_unlock(&mx);
}
void covers_poll(int *done,int *total,int *running,int *new_images)
{
    pthread_mutex_lock(&mx);
    if(done)*done=completed;
    if(total)*total=n_jobs;
    if(running)*running=started&&!finished;
    if(new_images){*new_images=updated;updated=0;}
    int should_join=started&&finished;
    pthread_mutex_unlock(&mx);
    if(should_join) {
        pthread_join(th,NULL);
        pthread_mutex_lock(&mx);started=0;finished=0;clean_queue();pthread_mutex_unlock(&mx);
    }
}
static int start(const cover_job *jobs,int count,int folder,const char *data,int manual)
{
    if(!jobs||count<=0||count>8192||!data)return 0;
    covers_poll(NULL,NULL,NULL,NULL);
    pthread_mutex_lock(&mx);
    if(started){pthread_mutex_unlock(&mx);return 0;}
    if(snprintf(data_path,sizeof data_path,"%s",data)>=(int)sizeof data_path||
       snprintf(cover_dir,sizeof cover_dir,"%s/covers",data)>=(int)sizeof cover_dir) {
        pthread_mutex_unlock(&mx);return 0;
    }
    struct stat st;
    if((mkdir(cover_dir,0755)&&errno!=EEXIST)||stat(cover_dir,&st)||!S_ISDIR(st.st_mode)){
        fprintf(stderr,"[covers] cannot create cache directory %s\n",cover_dir);
        pthread_mutex_unlock(&mx);return 0;
    }
    queue=calloc((size_t)count,sizeof *queue);if(!queue){pthread_mutex_unlock(&mx);return 0;}
    n_jobs=count;save_folder=!!folder;retry_missing=manual;cancelled=paused=finished=completed=updated=0;
    for(int i=0;i<count;i++) {
        queue[i].folder_safe=jobs[i].folder_safe;
        queue[i].path=strdup(jobs[i].path?jobs[i].path:"");
        queue[i].artist=strdup(jobs[i].artist?jobs[i].artist:"");
        queue[i].album=strdup(jobs[i].album?jobs[i].album:"");
        if(!queue[i].path||!queue[i].artist||!queue[i].album) {
            clean_queue();pthread_mutex_unlock(&mx);return 0;
        }
    }
    started=1;pthread_mutex_unlock(&mx);
    if(pthread_create(&th,NULL,worker,NULL)) {
        pthread_mutex_lock(&mx);started=0;clean_queue();pthread_mutex_unlock(&mx);return 0;
    }
    return 1;
}
int covers_start(const cover_job *jobs,int count,int folder,const char *data)
{return start(jobs,count,folder,data,0);}
int covers_start_manual(const cover_job *jobs,int count,int folder,const char *data)
{return start(jobs,count,folder,data,1);}
void covers_shutdown(void)
{
    covers_cancel();
    pthread_mutex_lock(&mx);int join=started;pthread_mutex_unlock(&mx);
    if(join)pthread_join(th,NULL);
    pthread_mutex_lock(&mx);started=0;clean_queue();pthread_mutex_unlock(&mx);
}
