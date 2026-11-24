// On-demand, bounded music-library index. Only the worker touches a pending list;
// the main thread publishes it on completion. No metadata I/O in the frame loop.
#include "library.h"
#include "meta.h"
#include "decode.h"
#include "playlist.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define LIB_MAX_TRACKS 8192
#define LIB_MAX_DEPTH 16
#define LIB_MAX_PATH 1024
#define LIB_CACHE_MAGIC "RA-LIB05"
#define LIB_MAX_CACHE (8u*1024u*1024u)

typedef struct { library_track *v; int n, cap; library_album *albums; int na;
                 library_artist *artists; int nr; } index_data;
static index_data active, pending;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t thread;
static int running, complete, cancel_scan;
static char scan_root[LIB_MAX_PATH], cache_file[LIB_MAX_PATH];

static char *copy_nonempty(const char *s, const char *fallback)
{
    return strdup(s && *s ? s : fallback);
}
static void free_index(index_data *d)
{
    for (int i=0;i<d->n;i++) {
        free(d->v[i].path); free(d->v[i].title); free(d->v[i].artist); free(d->v[i].album); free(d->v[i].album_artist);
    }
    free(d->v);free(d->albums);free(d->artists);memset(d,0,sizeof *d);
}
static int add(index_data *d, const char *path, const track_meta *m)
{
    if (d->n >= LIB_MAX_TRACKS) return 0;
    if (d->n == d->cap) {
        int nc=d->cap?d->cap*2:128;
        if (nc>LIB_MAX_TRACKS) nc=LIB_MAX_TRACKS;
        library_track *v=realloc(d->v,sizeof *v*(size_t)nc);
        if(!v)return 0;
        d->v=v;d->cap=nc;
    }
    library_track t={0};
    char base[200];meta_display_name(NULL,path,base,sizeof base);
    t.path=strdup(path);
    t.title=copy_nonempty(m->title,base);
    t.artist=copy_nonempty(m->artist,"Unknown Artist");
    t.album=copy_nonempty(m->album,"Unknown Album");
    t.album_artist=copy_nonempty(m->album_artist,m->artist[0]?m->artist:"Unknown Artist");
    t.track_no=m->track_no;
    t.duration_ms=m->dur_ms;
    if(!t.path||!t.title||!t.artist||!t.album||!t.album_artist) {
        free(t.path);free(t.title);free(t.artist);free(t.album);free(t.album_artist);return 0;
    }
    d->v[d->n++]=t;
    return 1;
}
static int cmp_track(const void *a, const void *b)
{
    const library_track *x=a,*y=b;
    int z=name_cmp(x->album_artist,y->album_artist);if(z)return z;
    z=name_cmp(x->album,y->album);if(z)return z;
    if(x->track_no>0&&y->track_no>0&&x->track_no!=y->track_no)
        return x->track_no<y->track_no?-1:1;
    z=name_cmp(x->title,y->title);return z?z:name_cmp(x->path,y->path);
}
static int build_groups(index_data *d)
{
    if(d->n)qsort(d->v,(size_t)d->n,sizeof *d->v,cmp_track);
    if(!d->n)return 1;
    d->albums=calloc((size_t)d->n,sizeof *d->albums);
    d->artists=calloc((size_t)d->n,sizeof *d->artists);
    if(!d->albums||!d->artists)return 0;
    for(int i=0;i<d->n;i++) {
        library_track *t=&d->v[i];
        if(i==0 || name_cmp(t->album_artist,d->v[i-1].album_artist)) {
            library_artist *a=&d->artists[d->nr++];
            a->name=t->album_artist;a->first_album=d->na;
        }
        if(i==0 || name_cmp(t->album_artist,d->v[i-1].album_artist) || name_cmp(t->album,d->v[i-1].album)) {
            library_album *a=&d->albums[d->na++];
            a->artist=t->album_artist;a->album=t->album;a->first=i;
            d->artists[d->nr-1].album_count++;
        }
        d->albums[d->na-1].count++;
    }
    return 1;
}
static int stopped(void)
{
    pthread_mutex_lock(&lock);int out=cancel_scan;pthread_mutex_unlock(&lock);return out;
}
static void scan_dir(index_data *d,const char *dir,int depth)
{
    if(depth>LIB_MAX_DEPTH || d->n>=LIB_MAX_TRACKS || stopped())return;
    DIR *dh=opendir(dir);if(!dh)return;
    struct dirent *e;
    while((e=readdir(dh)) && d->n<LIB_MAX_TRACKS && !stopped()) {
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")||e->d_name[0]=='.')continue;
        char path[LIB_MAX_PATH];
        int len=snprintf(path,sizeof path,"%s/%s",dir,e->d_name);
        if(len<0||(size_t)len>=sizeof path)continue;
        struct stat st;
        if(lstat(path,&st))continue; // don't follow symlink loops/off-card paths
        if(S_ISDIR(st.st_mode))scan_dir(d,path,depth+1);
        else if(S_ISREG(st.st_mode)&&dec_supported(e->d_name)) {
            track_meta m;
            if(!meta_read(path,&m))memset(&m,0,sizeof m);
            add(d,path,&m);
        }
    }
    closedir(dh);
}
static int write_u32(FILE *f,uint32_t n) { unsigned char b[4]={n,n>>8,n>>16,n>>24};return fwrite(b,1,4,f)==4; }
static int read_u32(FILE *f,uint32_t *v)
{unsigned char b[4];if(fread(b,1,4,f)!=4)return 0;*v=(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;return 1;}
static int write_str(FILE *f,const char *s)
{size_t n=strlen(s);return n<=LIB_MAX_PATH&&write_u32(f,(uint32_t)n)&&fwrite(s,1,n,f)==n;}
static char *read_str(FILE *f,int max)
{
    uint32_t n;if(!read_u32(f,&n)||n>(uint32_t)max)return NULL;
    char *s=malloc((size_t)n+1);if(!s)return NULL;
    if(fread(s,1,n,f)!=n||memchr(s,0,n)){free(s);return NULL;}
    s[n]=0;return s;
}
static int store_cache(const index_data *d)
{
    char tmp[LIB_MAX_PATH+16];
    if(snprintf(tmp,sizeof tmp,"%s.tmp",cache_file)>=(int)sizeof tmp)return 0;
    FILE *f=fopen(tmp,"wb");if(!f)return 0;
    int ok=fwrite(LIB_CACHE_MAGIC,1,8,f)==8 && write_str(f,scan_root)&&write_u32(f,(uint32_t)d->n);
    for(int i=0;ok && i<d->n;i++) {
        const library_track *t=&d->v[i];
        ok=write_str(f,t->path)&&write_str(f,t->title)&&write_str(f,t->artist)&&write_str(f,t->album)
            &&write_str(f,t->album_artist)&&write_u32(f,(uint32_t)t->track_no)&&write_u32(f,(uint32_t)t->duration_ms);
    }
    if(fflush(f)||fsync(fileno(f)))ok=0;
    if(fclose(f))ok=0;
    if(ok&&rename(tmp,cache_file)==0)return 1;
    unlink(tmp);return 0;
}
static int load_cache(index_data *d)
{
    struct stat st;
    if(stat(cache_file,&st)||st.st_size<16||st.st_size>LIB_MAX_CACHE)return 0;
    FILE *f=fopen(cache_file,"rb");if(!f)return 0;
    char magic[8];uint32_t n=0;
    int ok=fread(magic,1,8,f)==8&&!memcmp(magic,LIB_CACHE_MAGIC,8);
    char *root=ok?read_str(f,LIB_MAX_PATH-1):NULL;
    ok=root&&!strcmp(root,scan_root)&&read_u32(f,&n)&&n<=LIB_MAX_TRACKS;
    free(root);
    if(ok){
        d->v=calloc(n?n:1,sizeof *d->v);ok=d->v!=NULL;
        if(ok){d->cap=n;d->n=(int)n;}
    }
    for(uint32_t i=0;ok&&i<n;i++) {
        library_track *t=&d->v[i];uint32_t tr=0,ms=0;
        t->path=read_str(f,LIB_MAX_PATH-1);
        t->title=read_str(f,159);t->artist=read_str(f,159);t->album=read_str(f,159);
        t->album_artist=read_str(f,159);
        if(!t->path||!t->title||!t->artist||!t->album||!t->album_artist||
           strlen(t->path)<=strlen(scan_root)||
           strncmp(t->path,scan_root,strlen(scan_root))||
           t->path[strlen(scan_root)]!='/')ok=0;
        if(ok)ok=read_u32(f,&tr)&&read_u32(f,&ms)&&tr<=999&&ms<=86400000;
        t->track_no=(int)tr;t->duration_ms=(int)ms;
    }
    if(fclose(f))ok=0;
    if(ok)ok=build_groups(d);
    if(!ok)free_index(d);
    return ok;
}
static void *worker(void *unused)
{
    (void)unused;
    index_data d={0};scan_dir(&d,scan_root,0);
    if(!stopped() && build_groups(&d)){
        store_cache(&d);
        pthread_mutex_lock(&lock);pending=d;complete=1;pthread_mutex_unlock(&lock);
    } else {
        free_index(&d);pthread_mutex_lock(&lock);complete=-1;pthread_mutex_unlock(&lock);
    }
    pthread_mutex_lock(&lock);running=0;pthread_mutex_unlock(&lock);
    return NULL;
}
void library_init(const char *root,const char *cache)
{
    snprintf(scan_root,sizeof scan_root,"%s",root);
    snprintf(cache_file,sizeof cache_file,"%s",cache);
    index_data d={0};if(load_cache(&d))active=d;
}
int library_start_scan(void)
{
    pthread_mutex_lock(&lock);
    if(running||complete){pthread_mutex_unlock(&lock);return 0;}
    running=1;cancel_scan=0;
    pthread_mutex_unlock(&lock);
    if(pthread_create(&thread,NULL,worker,NULL)) {
        pthread_mutex_lock(&lock);running=0;pthread_mutex_unlock(&lock);return 0;
    }
    return 1;
}
int library_poll(void)
{
    pthread_mutex_lock(&lock);
    int done=complete;
    if(done){complete=0;if(done>0){free_index(&active);active=pending;memset(&pending,0,sizeof pending);}}
    pthread_mutex_unlock(&lock);
    if(done)pthread_join(thread,NULL);
    return done;
}
int library_is_scanning(void){pthread_mutex_lock(&lock);int x=running||complete;pthread_mutex_unlock(&lock);return x;}
void library_quit(void)
{
    pthread_mutex_lock(&lock);int join=running||complete;cancel_scan=1;pthread_mutex_unlock(&lock);
    if(join)pthread_join(thread,NULL);
    free_index(&pending);free_index(&active);
}
int library_count(void){return active.n;}
int library_album_count(void){return active.na;}
int library_artist_count(void){return active.nr;}
const library_track *library_song(int i){return i>=0&&i<active.n?&active.v[i]:NULL;}
const library_album *library_get_album(int i){return i>=0&&i<active.na?&active.albums[i]:NULL;}
const library_artist *library_get_artist(int i){return i>=0&&i<active.nr?&active.artists[i]:NULL;}
