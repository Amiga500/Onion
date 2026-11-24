// Album art: cover.{jpg,jpeg,png}, folder.{jpg,png}, then embedded MP3 APIC / FLAC PICTURE.
// Art is loaded only when selected album changes. Strict caps keep malicious artwork
// from consuming excessive RAM or stalling the 64 MB Miyoo devices.
#include "album_art.h"
#include "stb_image.h"
#include "cover_paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <sys/stat.h>
#define ART_INPUT_MAX (3u*1024u*1024u)
#define ART_PIXELS_MAX (1024u*1024u)
static char cache_dir[1024];
static album_art_resolver resolve_album;
void album_art_set_resolver(album_art_resolver resolver){resolve_album=resolver;}
typedef struct { char path[1024]; m2d_surf *source, *large, *small; } art_slot;
static art_slot slots[2];
void album_art_set_cache_dir(const char *dir)
{
    snprintf(cache_dir,sizeof cache_dir,"%s",dir?dir:"");
}
static void clear_slot(int i)
{
    art_slot *s=&slots[i];
    if(s->source)m2d_surface_free(s->source);
    if(s->large)m2d_surface_free(s->large);
    if(s->small)m2d_surface_free(s->small);
    memset(s,0,sizeof *s);
}
void album_art_clear_slot(int index){if(index>=0&&index<2)clear_slot(index);}
void album_art_clear(void){clear_slot(0);clear_slot(1);}

static uint32_t be32(const unsigned char *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint32_t sync32(const unsigned char *p){return ((uint32_t)p[0]&127)<<21|((uint32_t)p[1]&127)<<14|((uint32_t)p[2]&127)<<7|((uint32_t)p[3]&127);}
static m2d_surf *decode_img(const unsigned char *bytes,size_t n)
{
    if(n<16||n>ART_INPUT_MAX)return NULL;
    int w=0,h=0,ch=0;
    if(!stbi_info_from_memory(bytes,(int)n,&w,&h,&ch)||w<1||h<1||w>1024||h>1024||
       (size_t)w*h>ART_PIXELS_MAX)return NULL;
    unsigned char *rgba=stbi_load_from_memory(bytes,(int)n,&w,&h,&ch,4);
    if(!rgba)return NULL;
    m2d_surf *s=m2d_surface_new(w,h);
    if(s)m2d_surface_upload_rgba(s,rgba,w,h);
    stbi_image_free(rgba);return s;
}
static m2d_surf *load_file(const char *path)
{
    struct stat st;if(stat(path,&st)||!S_ISREG(st.st_mode)||st.st_size<16||st.st_size>ART_INPUT_MAX)return NULL;
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    size_t n=(size_t)st.st_size;
    unsigned char *data=malloc(n);m2d_surf *s=NULL;
    if(data&&fread(data,1,n,f)==n)s=decode_img(data,n);
    free(data);fclose(f);return s;
}
static m2d_surf *load_apic(const unsigned char *d,size_t n)
{
    // APIC payload: encoding byte, nul-terminated MIME, picture type,
    // description in encoding-dependent format, then image data.
    if(n<12)return NULL;
    int enc=d[0];size_t p=1;
    while(p<n&&d[p])p++;
    if(++p>=n)return NULL;
    p++; // picture type
    if(enc==0||enc==3){while(p<n&&d[p])p++;p++;}
    else if(enc==1||enc==2){while(p+1<n&&(d[p]||d[p+1]))p+=2;p+=2;}
    else return NULL;
    if(p>=n)return NULL;
    return decode_img(d+p,n-p);
}
static m2d_surf *from_mp3(const char *path)
{
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    unsigned char h[10];
    if(fread(h,1,10,f)!=10||memcmp(h,"ID3",3)||(h[3]!=3&&h[3]!=4)) {fclose(f);return NULL;}
    uint32_t len=sync32(h+6);
    if(len>ART_INPUT_MAX+1024*128){fclose(f);return NULL;}
    unsigned char *buf=malloc(len);
    if(!buf||fread(buf,1,len,f)!=len){free(buf);fclose(f);return NULL;}
    fclose(f);
    m2d_surf *s=NULL;
    size_t p=0;
    while(p+10<=len){
        const unsigned char *q=buf+p;
        uint32_t n=h[3]==4?sync32(q+4):be32(q+4);
        if(!q[0]||n>len-p-10)break;
        if(!memcmp(q,"APIC",4)&&n<=ART_INPUT_MAX){s=load_apic(q+10,n);if(s)break;}
        p+=10+n;
    }
    free(buf);return s;
}
static m2d_surf *flac_picture(const unsigned char *data,size_t len)
{
    // Picture block: type, mime length + data, description length + data,
    // width/height/depth/colors, data length + data.
    if(len<32)return NULL;
    size_t p=4;
    for(int i=0;i<2;i++){
        if(p+4>len)return NULL;
        uint32_t n=be32(data+p);p+=4;
        if(n>len-p)return NULL;
        p+=n;
    }
    if(p+20>len)return NULL;
    p+=16;
    uint32_t size=be32(data+p);p+=4;
    if(size>len-p||size>ART_INPUT_MAX)return NULL;
    return decode_img(data+p,size);
}
static m2d_surf *from_flac(const char *path)
{
    FILE *f=fopen(path,"rb");if(!f)return NULL;
    char magic[4];if(fread(magic,1,4,f)!=4||memcmp(magic,"fLaC",4)){fclose(f);return NULL;}
    m2d_surf *s=NULL;
    for(int b=0;b<64;b++){
        unsigned char h[4];if(fread(h,1,4,f)!=4)break;
        uint32_t n=(uint32_t)h[1]<<16|(uint32_t)h[2]<<8|h[3];
        if((h[0]&127)==6 && n<=ART_INPUT_MAX){
            unsigned char *data=malloc(n);
            if(data&&fread(data,1,n,f)==n)s=flac_picture(data,n);
            free(data);if(s)break;
        }else if(fseek(f,n,SEEK_CUR))break;
        if(h[0]&128)break;
    }
    fclose(f);return s;
}
static m2d_surf *load_track_image(const char *path)
{
    const char *names[]={"cover.jpg","cover.jpeg","cover.png","folder.jpg","folder.png",
                         "front.jpg","front.png",NULL};
    char full[1400],artist[160]={0},album[160]={0};
    int folder_safe=resolve_album?resolve_album(path,artist,album):1;
    for(int i=0;folder_safe&&names[i];i++) {
        if(cover_folder_image(path,names[i],full,sizeof full)) {
            m2d_surf *found=load_file(full);
            if(found)return found;
        }
    }
    /* A tagged track's own image takes priority over downloaded art. */
    const char *ext=strrchr(path,'.');
    m2d_surf *embedded=ext&&!strcasecmp(ext,".mp3")?from_mp3(path):
                       ext&&!strcasecmp(ext,".flac")?from_flac(path):NULL;
    if(embedded)return embedded;
    if(cover_album_folder_image(path,artist,album,full,sizeof full)) {
        m2d_surf *found=load_file(full);if(found)return found;
    }
    if(cover_cache_path(artist,album,cache_dir,full,sizeof full)) {
        m2d_surf *found=load_file(full);
        if(found)return found;
    }
    return NULL;
}
/* Slot 0 belongs to library selection; slot 1 to the actual playing track. */
m2d_surf *album_art_for_track_slot(const char *path,int slot)
{
    if(slot<0||slot>1||!path||!*path)return NULL;
    art_slot *s=&slots[slot];
    if(!strcmp(s->path,path))return s->source;
    clear_slot(slot);
    snprintf(s->path,sizeof s->path,"%s",path);
    s->source=load_track_image(path);
    return s->source;
}
m2d_surf *album_art_for_track(const char *path)
{
    return album_art_for_track_slot(path,0);
}
static m2d_surf *scaled_for_player(int sz)
{
    art_slot *s=&slots[1];
    m2d_surf **dst=sz==150?&s->large:&s->small;
    if(!s->source)return NULL;
    if(!*dst) {
        *dst=m2d_surface_new(sz,sz);
        if(*dst) {
            m2d_clear(*dst,0xff000000u,0);
            m2d_blit(s->source,0,0,s->source->w,s->source->h,
                     *dst,0,0,sz,sz,0xffffffffu,M2D_REPLACE,0);
        }
    }
    return *dst;
}
m2d_surf *album_art_player(const char *path,int size)
{
    if(size!=150&&size!=22)return NULL;
    if(!album_art_for_track_slot(path,1))return NULL;
    return scaled_for_player(size);
}
/* Cheap presence check for the downloader; metadata remains unmodified. */
int album_art_embedded_exists(const char *path)
{
    FILE *f=fopen(path,"rb");if(!f)return 0;
    unsigned char h[10];int yes=0;
    if(fread(h,1,10,f)==10 && !memcmp(h,"fLaC",4)) {
        if(fseek(f,4,SEEK_SET)==0)for(int i=0;i<64;i++) {
            unsigned char b[4];if(fread(b,1,4,f)!=4)break;
            unsigned n=(unsigned)b[1]<<16|(unsigned)b[2]<<8|b[3];
            if((b[0]&127)==6 && n>=32 && n<=3*1024*1024){yes=1;break;}
            if(fseek(f,(long)n,SEEK_CUR))break;
            if(b[0]&128)break;
        }
    } else if(!memcmp(h,"ID3",3) && (h[3]==3||h[3]==4)) {
        unsigned n=sync32(h+6);if(n>4*1024*1024)n=4*1024*1024;
        while(n>=10) {
            unsigned char b[10];if(fread(b,1,10,f)!=10)break;
            unsigned len=h[3]==4?sync32(b+4):be32(b+4);
            if(!b[0]||len>n-10)break;
            if(!memcmp(b,"APIC",4)&&len>12){yes=1;break;}
            if(fseek(f,(long)len,SEEK_CUR))break;
            n-=len+10;
        }
    }
    fclose(f);return yes;
}
