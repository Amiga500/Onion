#define _POSIX_C_SOURCE 200809L
#include "adts_duration.h"
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdlib.h>

#define ADTS_BUF 65536
static const int sample_rates[] = {
    96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,7350
};

/* ADTS is explicitly framed. Parsing each validated header gives sample-accurate
 * length even for VBR input where the container's bitrate estimate is wrong.
 * Never decode audio, allocate based on input, or trust a declared frame length
 * without checking it against the source bytes. */
static int64_t adts_duration_scan(const char *path)
{
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char buf[ADTS_BUF];
    size_t n=0, pos=0;
    uint64_t nanoseconds=0;
    unsigned frames=0, invalid=0;
    int sr=0, result=0;
    /* Optional ID3v2 prefix, frequently present on raw broadcast recordings. */
    unsigned char id3[10];
    size_t h=fread(id3,1,sizeof id3,f);
    if (h==10 && !memcmp(id3,"ID3",3)) {
        if ((id3[6]|id3[7]|id3[8]|id3[9]) & 0x80) goto done;
        uint32_t skip=((uint32_t)id3[6]<<21)|((uint32_t)id3[7]<<14)|
                      ((uint32_t)id3[8]<<7)|id3[9];
        if (skip>16*1024*1024u || fseek(f,(long)skip+10,SEEK_SET)) goto done;
    } else rewind(f);
    for (;;) {
        if (n-pos<8192 && !feof(f)) {
            if (pos) { memmove(buf,buf+pos,n-pos);n-=pos;pos=0; }
            n+=fread(buf+n,1,ADTS_BUF-n,f);
        }
        if (n-pos<7) break;
        unsigned char *p=buf+pos;
        unsigned fi=(p[2]>>2)&15;
        int valid=p[0]==0xff && (p[1]&0xf6)==0xf0 && fi<13;
        unsigned len=valid?((unsigned)(p[3]&3)<<11)|((unsigned)p[4]<<3)|(p[5]>>5):0;
        unsigned header=(p[1]&1)?7:9;
        if (!valid || len<header || len>8191) {
            /* Allow a short ID3v1 footer or corrupted trailing padding, not
             * a mostly bogus file with a couple of fake ADTS sync words. */
            pos++;invalid++;
            if(invalid>2048) goto done;
            continue;
        }
        if (n-pos<len) {
            if (feof(f)) goto done;
            /* More data is needed; shift this validated header to the front. */
            if (pos) {memmove(buf,buf+pos,n-pos);n-=pos;pos=0;}
            n+=fread(buf+n,1,ADTS_BUF-n,f);
            if (n<len) goto done;
            p=buf;
        }
        if (sr && sr!=sample_rates[fi]) goto done;
        sr=sample_rates[fi];
        unsigned samples=1024u*(1u+(p[6]&3u));
        nanoseconds+=(uint64_t)samples*1000000000ull/(unsigned)sr;
        frames++;
        if (nanoseconds>86400ull*1000000000ull || frames>10000000u) goto done;
        pos+=len;
    }
    if(frames>=2 && invalid<=2048 && nanoseconds>=1000000ull) result=1;
done:
    fclose(f);
    if (!result) return 0;
    int64_t ms=(int64_t)((nanoseconds+500000ull)/1000000ull);
    return ms>0 && ms<=INT_MAX ? ms : 0;
}

/* Both bridge metadata indexing and playback call into this same shared
 * library. A bounded, thread-safe stat-keyed LRU prevents repeated full-file
 * scans and lets the background library worker populate known durations. */
#define ADTS_CACHE_ITEMS 128
#define ADTS_PLAYBACK_SCAN_LIMIT (32LL * 1024 * 1024)
typedef struct {
    char path[1024];
    off_t size;
    time_t mtime;
    long mtime_nsec;
    int64_t milliseconds;
    int valid;
} adts_cache_entry;
static adts_cache_entry known[ADTS_CACHE_ITEMS];
static unsigned next_slot;
static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

static int64_t adts_cached(const char *path, int playback)
{
    if (!path || strlen(path) >= sizeof known[0].path) return 0;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return 0;
    pthread_mutex_lock(&cache_lock);
    for (int i = 0; i < ADTS_CACHE_ITEMS; i++) {
        const adts_cache_entry *e = &known[i];
        if (e->valid && e->size == st.st_size && e->mtime == st.st_mtime &&
            e->mtime_nsec == st.st_mtim.tv_nsec && !strcmp(e->path, path)) {
            int64_t duration = e->milliseconds;
            pthread_mutex_unlock(&cache_lock);
            return duration;
        }
    }
    pthread_mutex_unlock(&cache_lock);
    /* Large podcasts are fully measured during the library's background
     * scan, not synchronously while opening the playback stream. */
    if (playback && st.st_size > ADTS_PLAYBACK_SCAN_LIMIT) return 0;
    int64_t duration = adts_duration_scan(path);
    pthread_mutex_lock(&cache_lock);
    adts_cache_entry *e = &known[next_slot++ % ADTS_CACHE_ITEMS];
    snprintf(e->path, sizeof e->path, "%s", path);
    e->size = st.st_size;
    e->mtime = st.st_mtime;
    e->mtime_nsec = st.st_mtim.tv_nsec;
    e->milliseconds = duration;
    e->valid = 1;
    pthread_mutex_unlock(&cache_lock);
    return duration;
}

int64_t adts_duration_ms(const char *path) { return adts_cached(path, 0); }
int64_t adts_duration_playback_ms(const char *path) { return adts_cached(path, 1); }
