#ifndef REDAMP_LIBRARY_H
#define REDAMP_LIBRARY_H
#include <stddef.h>

typedef struct {
    char *path, *title, *artist, *album, *album_artist;
    int track_no;
    int duration_ms;
} library_track;

typedef struct {
    const char *artist, *album;
    int first, count;  // offsets into the artist/album/track sorted array
} library_album;

typedef struct {
    const char *name;
    int first_album, album_count;
} library_artist;

/* Main-thread APIs; no pointers obtained here are valid across a completed rescan. */
void library_init(const char *root, const char *cache_path);
void library_quit(void);
int library_start_scan(void);      /* 1 started, 0 running/unavailable */
int library_poll(void);            /* 1 when new index published */
int library_is_scanning(void);
int library_count(void);
int library_album_count(void);
int library_artist_count(void);
const library_track *library_song(int i);
const library_album *library_get_album(int i);
const library_artist *library_get_artist(int i);
#endif
