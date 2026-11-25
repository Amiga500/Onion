#ifndef REDAMP_COLLECTIONS_H
#define REDAMP_COLLECTIONS_H
#define COL_MAX 10000
#define COL_NAMES 128
#define COL_NAME_CHARS 40
#define COL_NAME (4*COL_NAME_CHARS) /* UTF-8 byte capacity */
typedef struct { char *path, *title; } col_track;
typedef struct { col_track *items; int count, cap; } collection;
void col_clear(collection *c);
int col_find(const collection *c, const char *path);
int col_add(collection *c, const char *path, const char *title);
void col_remove(collection *c, int row);
int col_read(collection *c, const char *file);
int col_write(const collection *c, const char *file);
int col_valid_name(const char *name);
int col_list(const char *dir, char names[COL_NAMES][COL_NAME+1]);
#endif
