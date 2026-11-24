/* Focused regression for M3U radio names, saved playlists and PLS station URLs. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "playlist.h"
#include "decode.h"
#include "meta.h"
int radio_is_url(const char *s) { return s && (!strncasecmp(s,"http://",7)||!strncasecmp(s,"https://",8)); }
int dec_supported(const char *p) { (void)p;return 1; }
int meta_read(const char *p,track_meta *m) { (void)p;memset(m,0,sizeof *m);return 0; }
void meta_display_name(const track_meta *m,const char *p,char *out,int n) {
    (void)m;snprintf(out,(size_t)n,"%s",p);
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL: %s\n",#x);return 1;} } while (0)
int main(int argc,char **argv) {
    if (argc!=4) return 2;
    pl_init();
    CHECK(!pl_m3u_has_tracks(argv[3]));
    CHECK(pl_m3u_has_tracks(argv[1]));
    CHECK(pl_load_m3u(argv[1])==2);
    CHECK(pl_count()==2);
    pl_item it;
    CHECK(pl_get(0,&it));
    CHECK(!strcmp(it.path,"https://example.invalid/jazz.mp3"));
    CHECK(!strcmp(it.name,"Jazz Station"));
    CHECK(pl_save_m3u(argv[3]));
    pl_clear();
    CHECK(pl_load_m3u(argv[3])==2);
    CHECK(pl_get(0,&it) && !strcmp(it.name,"Jazz Station"));
    pl_clear();
    CHECK(pl_load_m3u(argv[2])==1);
    CHECK(pl_get(0,&it) && !strcmp(it.path,"https://example.invalid/live"));
    pl_quit();
    puts("M3U / PLS radio playlists: PASS");
    return 0;
}
