#ifndef REDAMP_COVER_PATHS_H
#define REDAMP_COVER_PATHS_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
/* Album identity is independent of its directory. Separate the two fields in
 * the hash so artist/title boundaries cannot collide. Ignore untagged albums. */
static inline int cover_album_key(const char *artist,const char *album,char key[17])
{
    if(!artist||!*artist||!album||!*album||!strcasecmp(artist,"Unknown Artist")||
       !strcasecmp(album,"Unknown Album"))return 0;
    uint64_t h=UINT64_C(14695981039346656037);
    const char *fields[]={artist,album};
    for(int i=0;i<2;i++){
        for(const unsigned char *p=(const unsigned char *)fields[i];*p;p++){
            unsigned c=*p;if(c>='A'&&c<='Z')c+='a'-'A';
            h=(h^c)*UINT64_C(1099511628211);
        }
        h=(h^0)*UINT64_C(1099511628211);
    }
    snprintf(key,17,"%016llx",(unsigned long long)h);return 1;
}
static inline int cover_cache_path(const char *artist,const char *album,const char *data,
                                  char *dst,size_t cap)
{
    char key[17];if(!data||!*data||!cover_album_key(artist,album,key))return 0;
    int n=snprintf(dst,cap,"%s/covers/album-%s.jpg",data,key);
    return n>=0&&n<(int)cap;
}
static inline int cover_folder_image(const char *track, const char *name, char *dst, size_t cap)
{
    const char *slash = track ? strrchr(track, '/') : NULL;
    if (!slash || !name) return 0;
    return snprintf(dst, cap, "%.*s/%s", (int)(slash - track), track, name) < (int)cap;
}
static inline int cover_album_folder_image(const char *track,const char *artist,const char *album,
                                          char *dst,size_t cap)
{
    char key[17],name[40];if(!cover_album_key(artist,album,key))return 0;
    snprintf(name,sizeof name,"redamp-%s.jpg",key);
    return cover_folder_image(track,name,dst,cap);
}
#endif
