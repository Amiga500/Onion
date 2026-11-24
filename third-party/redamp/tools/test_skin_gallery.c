#include "skin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
m2d_surf *m2d_surface_new(int w,int h){m2d_surf *s=calloc(1,sizeof *s);if(!s)return NULL;
 s->w=w;s->h=h;s->px=calloc((size_t)w*h,4);if(!s->px){free(s);return NULL;}return s;}
void m2d_surface_free(m2d_surf *s){if(s){free(s->px);free(s->spans);free(s->rowidx);free(s);}}
void m2d_surface_upload_rgba(m2d_surf *s,const unsigned char *rgba,int w,int h){
 for(int i=0;i<w*h;i++){unsigned a=rgba[4*i+3];s->px[i]=a<<24|(rgba[4*i]*a/255)<<16|
 (rgba[4*i+1]*a/255)<<8|(rgba[4*i+2]*a/255);}}
void m2d_surface_build_spans(m2d_surf *s){(void)s;}
int main(int argc,char **argv){assert(argc>=2);skin *base=NULL;
 for(int i=1;i<argc;i++){
  skin *s=skin_load(argv[i],base);
  if(!s||!s->s[SK_MAIN]||!s->s[SK_CBUTTONS]){fprintf(stderr,"bad skin: %s\n",argv[i]);return 1;}
  if(i==1)base=s;else skin_free(s);
 }
 skin_free(base);printf("%d bundled skins parsed and loaded: PASS\n",argc-1);return 0;
}
