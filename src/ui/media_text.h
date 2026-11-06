/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_MEDIA_TEXT_H
#define MAINUI_MEDIA_TEXT_H
#include <stdbool.h>
#include <stddef.h>

/* One UTF-8 line never exceeds this many bytes; longer source lines wrap. */
#define MAINUI_TEXT_LINE_BYTES 1024
#define MAINUI_TEXT_FILE_LIMIT (2u * 1024u * 1024u)
#define MAINUI_TEXT_MAX_LINES 32768

typedef struct {
    size_t start, end;
} MainUITextLine;

typedef struct {
    char *data;
    size_t bytes, cursor;
    MainUITextLine *lines;
    int count, capacity, top, width;
    bool complete, truncated;
} MainUITextReader;

/* Exactly <console ROM root>/media/manuals/<ROM basename>.txt.
 * No alternative folders, case variants or ROM-directory fallback. */
bool mainui_text_find(const char *rom_root, const char *rom, char path[4096]);
/* The same exact directory and ROM basename, only .txt or .pdf. */
bool mainui_manual_find(const char *rom_root, const char *rom, const char *suffix, char path[4096]);
/* Loads at most 2 MiB, on demand; invalid UTF-8 is replaced by '?' so SDL_ttf
 * can render it. Files beyond the limit remain readable up to the cap. */
bool mainui_text_open(MainUITextReader *reader, const char *path);
void mainui_text_close(MainUITextReader *reader);
/* Cached, incremental wrapping: only new lines are measured. The callback
 * measures a NUL-terminated UTF-8 string in pixels, and may use SDL_ttf. */
void mainui_text_layout(MainUITextReader *reader, int width, int (*measure)(const char *, void *),
                        void *context, int wanted);
void mainui_text_line(const MainUITextReader *reader, int index,
                      char output[MAINUI_TEXT_LINE_BYTES]);
/* Source-byte segments of case-insensitive ASCII matches crossing visual
 * line breaks. Returns at most max overlapping spans on this visual line. */
int mainui_text_match_segments(const MainUITextReader *reader, int index, const char *term,
                               MainUITextLine *spans, int max);
#endif
