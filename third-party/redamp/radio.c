/* Buffered streaming radio transport with HTTPS verification and ICY metadata.
 * All network activity runs on a dedicated thread; playback reads from a bounded
 * RAM ring. No audio bytes or radio metadata are written to the SD card.
 */
#include "radio.h"
#include "netclock.h"
#include <curl/curl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#define RADIO_BUF (256 * 1024)
struct radio_stream {
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t have, space;
    char *url;
    unsigned char *buf;
    size_t head, tail, count;
    int cancel, done, began;
    int ffmpeg_hint;
    int http_status;
    char content_type[96];
    int non_audio;
    size_t bytes_this_attempt;
    int metaint, until_meta, meta_remaining, meta_used;
    unsigned char metadata[4096];
    char title[160];
    unsigned char prefix[96];
    size_t prefix_len, prefix_off;
};
static pthread_mutex_t active_lock = PTHREAD_MUTEX_INITIALIZER;
static radio_stream *active;
static pthread_once_t curl_once=PTHREAD_ONCE_INIT;
static int curl_ready;
static atomic_int network_error;
int radio_take_network_error(void){return atomic_exchange(&network_error,0);}
static void curl_init_once(void) { curl_ready=(curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK); }

int radio_is_url(const char *s) {
    return s && (!strncasecmp(s,"https://",8) || !strncasecmp(s,"http://",7));
}
static int validate_url(const char *s) {
    if (!radio_is_url(s) || strlen(s)>1800) return 0;
    for (const unsigned char *p=(const unsigned char *)s; *p; p++)
        if (*p<=32 || *p==127) return 0;
    return 1;
}
static int should_stop(radio_stream *r) {
    pthread_mutex_lock(&r->lock); int stop=r->cancel; pthread_mutex_unlock(&r->lock);
    return stop;
}
static int clock_cancel(void *ctx){return should_stop(ctx);}
static int progress(void *ctx,curl_off_t a,curl_off_t b,curl_off_t c,curl_off_t d) {
    (void)a;(void)b;(void)c;(void)d;
    return should_stop(ctx) ? 1 : 0;
}
static int push_audio(radio_stream *r,const unsigned char *p,size_t n) {
    size_t off=0;
    while (off<n) {
        pthread_mutex_lock(&r->lock);
        while (!r->cancel && r->count==RADIO_BUF)
            pthread_cond_wait(&r->space,&r->lock);
        if (r->cancel) { pthread_mutex_unlock(&r->lock); return 0; }
        size_t k=RADIO_BUF-r->count;
        if (k>n-off) k=n-off;
        if (k>RADIO_BUF-r->head) k=RADIO_BUF-r->head;
        memcpy(r->buf+r->head,p+off,k);
        r->head=(r->head+k)%RADIO_BUF; r->count+=k; off+=k;
        pthread_cond_broadcast(&r->have);
        pthread_mutex_unlock(&r->lock);
    }
    return 1;
}
static void parse_meta(radio_stream *r) {
    r->metadata[r->meta_used]=0;
    const char *s=(const char *)r->metadata;
    const char *p=strstr(s,"StreamTitle='");
    if (!p) return;
    p+=13;
    const char *end=strstr(p,"';");
    if (!end) return;
    size_t len=(size_t)(end-p);
    if (len>=sizeof r->title) len=sizeof r->title-1;
    pthread_mutex_lock(&r->lock);
    memcpy(r->title,p,len);r->title[len]=0;
    pthread_mutex_unlock(&r->lock);
}
static size_t radio_write(char *ptr,size_t size,size_t nmemb,void *ctx) {
    radio_stream *r=ctx;
    if (size && nmemb>SIZE_MAX/size) return 0;
    size_t len=size*nmemb, i=0;
    if(r->http_status>=300)return len;
    if(!r->bytes_this_attempt){
        fprintf(stderr,"[radio] response HTTP %d, type=%s\n",r->http_status,
                r->content_type[0]?r->content_type:"(unspecified)");
        if(r->non_audio){fprintf(stderr,"[radio] server returned non-audio content\n");return 0;}
    }
    r->bytes_this_attempt += len;
    if (r->metaint<=0) return push_audio(r,(unsigned char*)ptr,len) ? len:0;
    while (i<len) {
        if (r->until_meta) {
            size_t n=(size_t)r->until_meta;
            if (n>len-i) n=len-i;
            if (!push_audio(r,(unsigned char*)ptr+i,n)) return 0;
            r->until_meta-=(int)n; i+=n;
            continue;
        }
        if (r->meta_remaining<0) {
            r->meta_remaining=(unsigned char)ptr[i++]*16;
            r->meta_used=0;
            if (r->meta_remaining==0) { r->until_meta=r->metaint;r->meta_remaining=-1; }
            continue;
        }
        size_t n=(size_t)r->meta_remaining;
        if (n>len-i) n=len-i;
        size_t copy=n;
        if (copy>sizeof(r->metadata)-1-(size_t)r->meta_used)
            copy=sizeof(r->metadata)-1-(size_t)r->meta_used;
        memcpy(r->metadata+r->meta_used,ptr+i,copy);
        r->meta_used+=(int)copy;
        r->meta_remaining-=(int)n; i+=n;
        if (r->meta_remaining==0) {
            parse_meta(r);r->meta_remaining=-1;r->until_meta=r->metaint;
        }
    }
    return len;
}
static size_t radio_header(char *p,size_t size,size_t count,void *ctx) {
    radio_stream *r=ctx;
    size_t n=size*count;
    if((n>=12&&!strncasecmp(p,"HTTP/",5))||(n>=7&&!strncasecmp(p,"ICY ",4))){
        const char *sp=memchr(p,' ',n);
        r->http_status=sp&&p+n-sp>=4&&isdigit((unsigned char)sp[1])&&isdigit((unsigned char)sp[2])&&isdigit((unsigned char)sp[3])?
            (sp[1]-'0')*100+(sp[2]-'0')*10+sp[3]-'0':0;
        r->metaint=0;r->until_meta=0;r->meta_remaining=-1;r->meta_used=0;
        r->content_type[0]=0;r->non_audio=0;
        pthread_mutex_lock(&r->lock);r->ffmpeg_hint=0;pthread_mutex_unlock(&r->lock);
    }
    if (n>13 && !strncasecmp(p,"content-type:",13)) {
        char ctype[96]={0};
        size_t take=n-13<sizeof ctype-1?n-13:sizeof ctype-1;
        memcpy(ctype,p+13,take);
        for(char *c=ctype;*c;c++) if(*c==';' || *c=='\r' || *c=='\n') {*c=0;break;}
        for(char *c=ctype;*c;c++) *c=(char)tolower((unsigned char)*c);
        snprintf(r->content_type,sizeof r->content_type,"%s",ctype);
        r->non_audio=strstr(ctype,"text/html")||strstr(ctype,"application/json")||strstr(ctype,"application/xhtml+xml");
        int ff = strstr(ctype,"mpegurl") != NULL ||
                 strstr(ctype,"audio/aac") != NULL;
        if(ff) {
            pthread_mutex_lock(&r->lock);r->ffmpeg_hint=1;pthread_mutex_unlock(&r->lock);
        }
    }
    if (n>12 && !strncasecmp(p,"icy-metaint:",12)) {
        char tmp[32]={0};
        size_t k=n-12<sizeof tmp-1?n-12:sizeof tmp-1;
        memcpy(tmp,p+12,k);
        long v=strtol(tmp,NULL,10);
        if (v>0 && v<10000000) {
            r->metaint=(int)v;r->until_meta=(int)v;r->meta_remaining=-1;
        }
    }
    return n;
}
static void *radio_worker(void *ctx) {
    radio_stream *r=ctx;
    char bundled_ca[1024]={0};
    const char *home=getenv("MA_HOME");
    if (home && snprintf(bundled_ca,sizeof bundled_ca,"%s/cacert.pem",home)<(int)sizeof bundled_ca &&
        access(bundled_ca,R_OK)==0) {
        /* Optional trusted CA bundle for verified mode. */
    } else bundled_ca[0]=0;
    if(!should_stop(r))fprintf(stderr,"[radio] opening %s\n",r->url);
    int healthy=0;
    for (int failures=0;failures<3 && !should_stop(r);) {
        r->bytes_this_attempt = 0;
        r->http_status=0;r->content_type[0]=0;r->non_audio=0;
        pthread_mutex_lock(&r->lock);r->ffmpeg_hint=0;pthread_mutex_unlock(&r->lock);
        CURL *c=curl_easy_init();
        if (!c) break;
        r->metaint=0;r->until_meta=0;r->meta_remaining=-1;r->meta_used=0;
        char detail[CURL_ERROR_SIZE]={0};
        curl_easy_setopt(c,CURLOPT_ERRORBUFFER,detail);
        curl_easy_setopt(c,CURLOPT_FAILONERROR,1L);
        curl_easy_setopt(c,CURLOPT_URL,r->url);
        curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);
        curl_easy_setopt(c,CURLOPT_MAXREDIRS,5L);
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(c,CURLOPT_PROTOCOLS_STR,"http,https");
        curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS_STR,"http,https");
#else
        curl_easy_setopt(c,CURLOPT_PROTOCOLS,(long)(CURLPROTO_HTTP|CURLPROTO_HTTPS));
        curl_easy_setopt(c,CURLOPT_REDIR_PROTOCOLS,(long)(CURLPROTO_HTTP|CURLPROTO_HTTPS));
#endif
        int insecure=netclock_insecure();
        if(!insecure)netclock_ensure(clock_cancel,r,0);
        if(should_stop(r)){curl_easy_cleanup(c);break;}
        curl_easy_setopt(c,CURLOPT_SSL_VERIFYPEER,insecure?0L:1L);
        curl_easy_setopt(c,CURLOPT_SSL_VERIFYHOST,insecure?0L:2L);
        if (!insecure && bundled_ca[0]) curl_easy_setopt(c,CURLOPT_CAINFO,bundled_ca);
        if(insecure)fprintf(stderr,"[radio] insecure HTTPS explicitly enabled\n");
        curl_easy_setopt(c,CURLOPT_CONNECTTIMEOUT,8L);
        /* A connected server that never sends audio must not hang forever. */
        curl_easy_setopt(c,CURLOPT_LOW_SPEED_LIMIT,16L);
        curl_easy_setopt(c,CURLOPT_LOW_SPEED_TIME,12L);
        curl_easy_setopt(c,CURLOPT_NOSIGNAL,1L);
        char agent[96];
        snprintf(agent,sizeof agent,"RedAmp/%s (RedOnion)",
                 getenv("MA_VERSION") ? getenv("MA_VERSION") : "1.2.1");
        curl_easy_setopt(c,CURLOPT_USERAGENT,agent);
        curl_easy_setopt(c,CURLOPT_HTTPHEADER,NULL);
        curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,radio_write);
        curl_easy_setopt(c,CURLOPT_WRITEDATA,r);
        curl_easy_setopt(c,CURLOPT_HEADERFUNCTION,radio_header);
        curl_easy_setopt(c,CURLOPT_HEADERDATA,r);
        curl_easy_setopt(c,CURLOPT_XFERINFOFUNCTION,progress);
        curl_easy_setopt(c,CURLOPT_XFERINFODATA,r);
        curl_easy_setopt(c,CURLOPT_NOPROGRESS,0L);
        struct curl_slist *hdr=curl_slist_append(NULL,"Icy-MetaData: 1");
        curl_easy_setopt(c,CURLOPT_HTTPHEADER,hdr);
        struct curl_slist *aliases=curl_slist_append(NULL,"ICY 200 OK");
        if(aliases)curl_easy_setopt(c,CURLOPT_HTTP200ALIASES,aliases);
        CURLcode code=curl_easy_perform(c);
        long status=0;curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&status);
        if (r->bytes_this_attempt >= 32 * 1024) {failures = 0;healthy=1;} /* a healthy connection gets a fresh retry budget */
        failures++;
        int cancelled=should_stop(r);
        if(!cancelled)fprintf(stderr,"[radio] station exited (curl=%d http=%ld failures=%d bytes=%zu): %s\n",code,status,failures,r->bytes_this_attempt,detail[0]?detail:curl_easy_strerror(code));
        else fprintf(stderr,"[radio] stream closed (http=%ld bytes=%zu)\n",status,r->bytes_this_attempt);
        curl_slist_free_all(aliases);
        curl_slist_free_all(hdr);
        curl_easy_cleanup(c);
        if(cancelled)break;
        if(code==CURLE_PEER_FAILED_VERIFICATION || code==CURLE_SSL_CACERT_BADFILE){
            atomic_store(&network_error,!netclock_network_available()?2:!netclock_valid()?3:4);
            break;
        }
        /* DNS/connect failures have no value in an immediate retry: stop after
         * the first error rather than repeating attempts for every station. */
        if(code==CURLE_COULDNT_RESOLVE_HOST || code==CURLE_COULDNT_CONNECT ||
           code==CURLE_COULDNT_RESOLVE_PROXY) {
            atomic_store(&network_error,code==CURLE_COULDNT_RESOLVE_HOST?1:2);
            break;
        }
        if (should_stop(r)) break;
        if (status>=400 && status<500){atomic_store(&network_error,6);break;}
        if(!healthy&&r->bytes_this_attempt==0){
            atomic_store(&network_error,code==CURLE_OPERATION_TIMEDOUT?5:6);break;
        }
        if (failures<3) for (int i=0;i<failures*10 && !should_stop(r);i++) usleep(100000);
    }
    pthread_mutex_lock(&r->lock);
    r->done=1;
    pthread_cond_broadcast(&r->have);
    pthread_mutex_unlock(&r->lock);
    return NULL;
}
radio_stream *radio_open(const char *url) {
    atomic_store(&network_error,0);
    if (!validate_url(url)) return NULL;
    pthread_once(&curl_once,curl_init_once);
    if (!curl_ready) return NULL;
    radio_stream *r=calloc(1,sizeof *r);
    if (!r) return NULL;
    r->url=strdup(url);r->buf=malloc(RADIO_BUF);
    if (!r->url || !r->buf) { free(r->url);free(r->buf);free(r);return NULL; }
    pthread_mutex_init(&r->lock,NULL);pthread_cond_init(&r->have,NULL);pthread_cond_init(&r->space,NULL);
    pthread_mutex_lock(&active_lock);
    /* The audio engine is single-source; never register a second station. */
    if (active) { pthread_mutex_unlock(&active_lock);radio_close(r);return NULL; }
    active=r;
    pthread_mutex_unlock(&active_lock);
    if (pthread_create(&r->thread,NULL,radio_worker,r)!=0) { radio_close(r);return NULL; }
    r->began=1;
    return r;
}
size_t radio_read(radio_stream *r,void *dst,size_t bytes) {
    size_t made=0;
    unsigned char *p=dst;
    while (made<bytes && r->prefix_off<r->prefix_len)
        p[made++]=r->prefix[r->prefix_off++];
    while (made<bytes) {
        pthread_mutex_lock(&r->lock);
        while (!r->count && !r->done && !r->cancel)
            pthread_cond_wait(&r->have,&r->lock);
        if ((!r->count && r->done) || r->cancel) { pthread_mutex_unlock(&r->lock);break; }
        size_t n=r->count;
        if (n>bytes-made) n=bytes-made;
        if (n>RADIO_BUF-r->tail) n=RADIO_BUF-r->tail;
        memcpy(p+made,r->buf+r->tail,n);
        r->tail=(r->tail+n)%RADIO_BUF; r->count-=n;made+=n;
        pthread_cond_signal(&r->space);
        pthread_mutex_unlock(&r->lock);
    }
    return made;
}
void radio_abort_active(void) {
    pthread_mutex_lock(&active_lock);
    if (active) {
        radio_stream *r=active;
        pthread_mutex_lock(&r->lock);
        r->cancel=1;
        pthread_cond_broadcast(&r->have);
        pthread_cond_broadcast(&r->space);
        pthread_mutex_unlock(&r->lock);
    }
    pthread_mutex_unlock(&active_lock);
}
void radio_close(radio_stream *r) {
    if (!r) return;
    pthread_mutex_lock(&active_lock);
    if (active==r) active=NULL;
    pthread_mutex_lock(&r->lock);
    r->cancel=1;
    pthread_cond_broadcast(&r->have);pthread_cond_broadcast(&r->space);
    pthread_mutex_unlock(&r->lock);
    pthread_mutex_unlock(&active_lock);
    if (r->began) pthread_join(r->thread,NULL);
    pthread_mutex_destroy(&r->lock);
    pthread_cond_destroy(&r->have);pthread_cond_destroy(&r->space);
    free(r->url);free(r->buf);free(r);
}
void radio_title(radio_stream *r,char *out,size_t cap) {
    if (!cap) return;
    if (!r) { *out=0;return; }
    pthread_mutex_lock(&r->lock);
    snprintf(out,cap,"%s",r->title);
    pthread_mutex_unlock(&r->lock);
}

int radio_probe(radio_stream *r,unsigned char *out,size_t n) {
    if (!r || n>sizeof r->prefix || r->prefix_len) return 0;
    size_t got=radio_read(r,out,n);
    if (got!=n) {if(!should_stop(r))fprintf(stderr,"[radio] probe short: %zu/%zu bytes\n",got,n);return 0;}
    memcpy(r->prefix,out,n);r->prefix_len=n;r->prefix_off=0;
    return 1;
}

/* Decoders for chained Ogg streams can publish titles from codec headers.
 * ICY metadata remains supported independently. */
void radio_set_title(radio_stream *r,const char *title)
{
    if (!r || !title || !*title) return;
    pthread_mutex_lock(&r->lock);
    snprintf(r->title,sizeof r->title,"%s",title);
    pthread_mutex_unlock(&r->lock);
}

int radio_is_ffmpeg_stream(radio_stream *r) {
    if(!r) return 0;
    pthread_mutex_lock(&r->lock);
    int yes=r->ffmpeg_hint;
    pthread_mutex_unlock(&r->lock);
    return yes;
}
