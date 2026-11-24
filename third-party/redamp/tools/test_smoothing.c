#include "mini2d.h"
#include <stdlib.h>
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL: %s\n",#x);exit(1);}}while(0)
int main(void) {
    m2d_surf *s=m2d_surface_new(2,2),*d=m2d_surface_new(4,4);
    CHECK(s&&d);
    s->px[0]=0xffff0000u;s->px[1]=0xff0000ffu;
    s->px[2]=0xff00ff00u;s->px[3]=0xffffffffu;
    m2d_blit_smooth2x(s,d,0,0);
    CHECK(d->px[0]==s->px[0]); /* clamped at outermost corner */
    CHECK(d->px[1]!=s->px[0] && d->px[1]!=s->px[1]);
    CHECK(d->px[15]==s->px[3]);
    m2d_surface_free(s);m2d_surface_free(d);
    /* A single changed native pixel affects its neighbors under bilinear taps. */
    s=m2d_surface_new(5,5);d=m2d_surface_new(10,10);
    m2d_surf *full=m2d_surface_new(10,10);
    CHECK(s&&d&&full);
    for(int i=0;i<25;i++)s->px[i]=0xff000000u|(i*1234567u&0xffffffu);
    m2d_blit_smooth2x(s,d,0,0);
    s->px[12]=0xffff0000u;
    m2d_blit_smooth2x_region(s,d,0,0,1,1,4,4);
    m2d_blit_smooth2x(s,full,0,0);
    for(int i=0;i<100;i++)CHECK(d->px[i]==full->px[i]);
    m2d_surface_free(s);m2d_surface_free(d);m2d_surface_free(full);
    puts("Winamp bilinear scaling, edge bounds and regional cache: PASS");
    return 0;
}
