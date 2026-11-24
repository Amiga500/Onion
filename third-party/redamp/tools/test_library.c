#include "library.h"
#include "meta.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
int dec_supported(const char *p){const char *e=strrchr(p,'.');return e && !strcmp(e,".mp3");}
int name_cmp(const char *a,const char *b){return strcasecmp(a,b);}
int meta_read(const char *p,track_meta *m){
 memset(m,0,sizeof *m);
 const char *f=strrchr(p,'/');f=f?f+1:p;
 if(strstr(p,"artist-a"))strcpy(m->artist,"Artist A");else strcpy(m->artist,"Artist B");
 if(strstr(p,"album-one"))strcpy(m->album,"Album One");
 else strcpy(m->album,"Album Two");
 snprintf(m->title,sizeof m->title,"%s",f);
 m->track_no=atoi(f);m->dur_ms=60000;return 1;
}
void meta_display_name(const track_meta *m,const char *p,char *out,int n){(void)m;snprintf(out,n,"%s",strrchr(p,'/')+1);}
static void touch(const char *p){int fd=open(p,O_CREAT|O_WRONLY,0644);assert(fd>=0);close(fd);}
static void wait_scan(void){for(int i=0;i<1000;i++){int done=library_poll();if(done){assert(done==1);return;}usleep(5000);}assert(!"scan timed out");}
int main(void){
 char root[]="/tmp/redamp-lib-test-XXXXXX";assert(mkdtemp(root));
 char a[500],b[500],c[500],f[500],cache[500];
 snprintf(a,sizeof a,"%s/artist-a",root);assert(!mkdir(a,0755));
 snprintf(b,sizeof b,"%s/album-one",a);assert(!mkdir(b,0755));
 snprintf(c,sizeof c,"%s/album-two",a);assert(!mkdir(c,0755));
 snprintf(f,sizeof f,"%s/02.mp3",b);touch(f);
 snprintf(f,sizeof f,"%s/01.mp3",b);touch(f);
 snprintf(f,sizeof f,"%s/03.mp3",c);touch(f);
 snprintf(f,sizeof f,"%s/artist-b",root);assert(!mkdir(f,0755));
 char d[500];snprintf(d,sizeof d,"%s/album-two",f);assert(!mkdir(d,0755));
 snprintf(f,sizeof f,"%s/01.mp3",d);touch(f);
 snprintf(f,sizeof f,"%s/symlink",a);assert(!symlink(a,f));
 snprintf(cache,sizeof cache,"%s/cache.bin",root);
 library_init(root,cache);assert(library_count()==0);
 assert(library_start_scan());wait_scan();
 assert(library_count()==4);assert(library_album_count()==3);assert(library_artist_count()==2);
 const library_album *al=library_get_album(0);assert(al && !strcmp(al->album,"Album One")&&al->count==2);
 assert(!strcmp(library_song(0)->title,"01.mp3"));
 library_quit();
 library_init(root,cache);assert(library_count()==4);assert(library_album_count()==3);
 snprintf(f,sizeof f,"%s/04.mp3",c);touch(f);
 assert(library_start_scan());wait_scan();assert(library_count()==5);
 library_quit();puts("library index, sort, cache reload, rescan and symlink safety: PASS");
 return 0;
}
