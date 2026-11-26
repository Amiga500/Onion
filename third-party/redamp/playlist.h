#ifndef PLAYLIST_H
#define PLAYLIST_H
#include "meta.h"

typedef struct {
    char *path;
    char name[200];   // display name
    int dur_ms;
    int has_meta;
} pl_item;

void pl_init(void);
void pl_quit(void);
void pl_lock(void);
void pl_unlock(void);

// these lock internally
unsigned pl_revision(void); /* list or asynchronous metadata changed */
int pl_count(void);
void pl_clear(void);
int pl_add(const char *path);
int pl_add_named(const char *path, const char *title);
int pl_add_dir(const char *dir, int recursive);
int pl_dir_has_tracks(const char *dir, int recursive);
int pl_m3u_has_tracks(const char *path);       // returns tracks added
// copies item i (name, duration); returns 0 if out of range
int pl_get(int i, pl_item *out);
const char *pl_path(int i);                           // valid until the list changes (call under lock or from UI thread)
void pl_set_hint(int first_visible);                  // metadata worker looks here first
long long pl_total_ms(int *complete);
void pl_remove(int i);
int pl_move(int from,int to);

int pl_save_m3u(const char *path);
int pl_load_m3u(const char *path);

// sorting helper for directory listings
int name_cmp(const char *a, const char *b);

#endif
