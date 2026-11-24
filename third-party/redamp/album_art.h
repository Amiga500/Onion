#ifndef REDAMP_ALBUM_ART_H
#define REDAMP_ALBUM_ART_H
#include "mini2d.h"
/* Returns a cached surface, owned by module (do not free). NULL if unavailable. */
m2d_surf *album_art_for_track(const char *path);
void album_art_clear(void);
void album_art_clear_slot(int index);
void album_art_set_cache_dir(const char *dir);
/* Resolver supplies album artist/title and whether generic folder artwork is safe. */
typedef int (*album_art_resolver)(const char *path,char artist[160],char album[160]);
void album_art_set_resolver(album_art_resolver resolver);
m2d_surf *album_art_for_track_slot(const char *path,int slot);
m2d_surf *album_art_player(const char *path,int size);
int album_art_embedded_exists(const char *path);
#endif
