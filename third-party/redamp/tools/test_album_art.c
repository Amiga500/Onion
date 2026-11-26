#include "album_art.h"
#include "cover_paths.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

m2d_surf *m2d_surface_new(int w,int h){m2d_surf *s=calloc(1,sizeof *s);if(!s)return NULL;
 s->w=w;s->h=h;s->px=calloc((size_t)w*h,4);if(!s->px){free(s);return NULL;}return s;}
void m2d_surface_free(m2d_surf *s){if(s){free(s->px);free(s);}}
void m2d_surface_upload_rgba(m2d_surf *s,const unsigned char *rgba,int w,int h){
 for(int i=0;i<w*h;i++)s->px[i]=0xff000000|(uint32_t)rgba[i*4]<<16|(uint32_t)rgba[i*4+1]<<8|rgba[i*4+2];
}
void m2d_clear(m2d_surf *s,uint32_t argb,int c) {(void)s;(void)argb;(void)c;}
void m2d_blit(m2d_surf *src,int x,int y,int w,int h,m2d_surf *dst,int dx,int dy,
              int dw,int dh,uint32_t tint,int blend,int flip)
{(void)src;(void)x;(void)y;(void)w;(void)h;(void)dst;(void)dx;(void)dy;
 (void)dw;(void)dh;(void)tint;(void)blend;(void)flip;}
static int resolve(const char *path,char artist[160],char album[160]){
 snprintf(artist,160,"Sample Artist");
 snprintf(album,160,"%s",strstr(path,"second")?"Second Album":strstr(path,"missing")?"Missing Album":"Sample Album");
 return !strstr(path,"mixed/");
}
int main(int argc,char **argv){assert(argc==9);
 m2d_surf *s=album_art_for_track(argv[1]);assert(s&&s->w==16&&s->h==16);
 assert(album_art_for_track(argv[1])==s);
 m2d_surf *scaled=album_art_player(argv[1],236);assert(scaled&&scaled->w==236&&scaled->h==236);
 assert(album_art_player(argv[1],236)==scaled);
 scaled=album_art_player(argv[1],112);assert(scaled&&scaled->w==112&&scaled->h==112);
 scaled=album_art_player(argv[1],236);assert(scaled&&scaled->w==236&&scaled->h==236);
 album_art_clear();
 s=album_art_for_track(argv[2]);assert(s&&s->w==24&&s->h==24);album_art_clear();
 s=album_art_for_track(argv[3]);assert(s&&s->w==32&&s->h==32);album_art_clear();
 album_art_set_cache_dir(argv[4]);
 album_art_set_resolver(resolve);
 s=album_art_for_track(argv[5]);assert(s&&s->w==42&&s->h==42);album_art_clear();
 s=album_art_for_track(argv[6]);assert(s&&s->w==35);album_art_clear();
 s=album_art_for_track(argv[7]);assert(s&&s->w==36);album_art_clear();
 assert(!album_art_for_track(argv[8]));album_art_clear();
 puts("external and embedded art, folder-cover priority, mixed albums and missing artwork: PASS");return 0;
}
