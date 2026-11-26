/*
 * SD Card Doctor v0.6.3 - Onion / RedOnion SD-card diagnostics.
 * Copyright (c) 2026. MIT License.
 *
 * Filesystem scans are read-only. fsck.fat, if available, is run ONLY with -n.
 * This app NEVER unmounts the card or repairs a mounted FAT filesystem.
 * Test writes require confirmation and use a unique, verified temporary file.
 * Cleanup is a reviewed move to a reversible quarantine, never a deletion.
 */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <SDL/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <sys/klog.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#ifndef O_DIRECT
#define O_DIRECT 0
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef ST_RDONLY
#define ST_RDONLY 1
#endif
#define W 640
#define H 480
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#ifndef ROOT_PATH
#define ROOT_PATH "/mnt/SDCARD"
#endif
#define QDIR ROOT_PATH "/.sdcarddoctor-quarantine"
#define TEST_PREFIX ".sdcarddoctor-test-"
#define TEST_SIG "SDCARD_DOCTOR_V01_TEST_FILE_DO_NOT_EDIT"
#define TEST_BYTES (32u * 1024u * 1024u)
#define IO_CHUNK (256u * 1024u)
#define READ_TEST_BYTES (64u * 1024u * 1024u)
#define QUICK_MAX_ENTRIES 15600u
#define QUICK_MAX_FILES 1950u
#define QUICK_SAMPLE_BYTES 32768u
/* Full read of specifically named boot-critical files unless implausibly large.
 * For other script/config files, keep full reads within a modest quick budget. */
#define QUICK_KEY_FILE_MAX (64u * 1024u * 1024u)
#define QUICK_AUTO_FULL_MAX (1024u * 1024u)
#define QUICK_AUTO_FULL_BUDGET (48u * 1024u * 1024u)
#define MAX_DIR_DEPTH 24
#define MAX_GROUPS 64
#define MAX_CANDIDATES 48
#define MAX_FAILURES 96
#define LOG_LIMIT_BYTES (128u*1024u)

static volatile sig_atomic_t interrupted = 0;
static int input_fd = -1;
static bool raw_input = false;
static bool needs_redraw = true;
/* /tmp is often RAM-backed. Enforce a cap even when scans encounter endless
 * errors. Only the launcher's dedicated, regular-file stderr is truncated. */
static bool log_full=false;
static void bounded_log(const char *fmt, ...) {
    struct stat st;
    va_list ap;
    if (log_full) return;
    if (fstat(STDERR_FILENO,&st)==0 && S_ISREG(st.st_mode) && st.st_size>=LOG_LIMIT_BYTES) {
        fputs("sdcarddoctor: LOG LIMIT REACHED; FURTHER LINES OMITTED\n",stderr);
        fflush(stderr);log_full=true;return;
    }
    fputs("sdcarddoctor: ",stderr);
    va_start(ap,fmt);vfprintf(stderr,fmt,ap);va_end(ap);
    fputc('\n',stderr);fflush(stderr);
}
#define LOG(...) bounded_log(__VA_ARGS__)
static void signal_stop(int sig) { (void)sig; interrupted = 1; }
/* Compact, self-contained 5x7 title font (columns stored as 5-bit row masks).
 * No need to locate an Onion or system TTF file. */
typedef struct { char c; unsigned char bits[7]; } Glyph;
static const Glyph font[] = {
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{31,4,4,4,4,4,31}},       {'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}},  {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}},     {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}},    {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}},   {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
 {'+',{0,4,4,31,4,4,0}},     {'-',{0,0,0,31,0,0,0}},
 {'/',{1,2,2,4,8,8,16}},     {':',{0,4,4,0,4,4,0}},
 {'.',{0,0,0,0,0,12,12}},    {'!',{4,4,4,4,4,0,4}},
 {'?',{14,17,1,2,4,0,4}},    {'=',{0,31,0,31,0,0,0}},
 {'(',{2,4,8,8,8,4,2}},     {')',{8,4,2,2,2,4,8}},
 {'#',{10,31,10,10,31,10,0}}, {'%',{17,2,4,8,17,0,0}},
 {' ',{0,0,0,0,0,0,0}},
 /* Additional ASCII punctuation; keep indices aligned with small_font. */
 {'$',{4,15,20,14,5,30,4}},
 {'&',{12,18,20,8,21,18,13}},
 {'\'',{4,4,8,0,0,0,0}},
 {'"',{10,10,10,0,0,0,0}},
 {'*',{0,4,21,14,21,4,0}},
 {',',{0,0,0,0,0,6,4}},
 {';',{0,6,6,0,6,6,4}},
 {'<',{2,4,8,16,8,4,2}},
 {'>',{8,4,2,1,2,4,8}},
 {'@',{14,17,23,21,23,16,14}},
 {'[',{14,8,8,8,8,8,14}},
 {'\\',{16,8,8,4,2,2,1}},
 {']',{14,2,2,2,2,2,14}},
 {'^',{4,10,17,0,0,0,0}},
 {'_',{0,0,0,0,0,0,31}},
 {'`',{8,4,2,0,0,0,0}},
 {'{',{3,4,4,8,4,4,3}},
 {'|',{4,4,4,4,4,4,4}},
 {'}',{24,4,4,2,4,4,24}},
 {'~',{0,0,9,22,0,0,0}}
};

/* Native 9x13 fixed-width 1-bit glyphs, no image scaling.
 * Raster shapes from DejaVu Sans Mono Bold (permissive DejaVu license). */
static const uint16_t small_font[][13] = {
    {0x000,0x038,0x038,0x078,0x068,0x06c,0x0cc,0x0fc,0x0c6,0x0c6,0x186,0x000,0x000}, /* A */
    {0x000,0x0f8,0x0cc,0x0c6,0x0cc,0x0f8,0x0cc,0x0c6,0x0c6,0x0ce,0x0fc,0x000,0x000}, /* B */
    {0x000,0x03c,0x060,0x0e0,0x0c0,0x0c0,0x0c0,0x0c0,0x0e0,0x060,0x03c,0x000,0x000}, /* C */
    {0x000,0x0f8,0x0cc,0x0cc,0x0c6,0x0c6,0x0c6,0x0c6,0x0cc,0x0cc,0x0f8,0x000,0x000}, /* D */
    {0x000,0x0fe,0x0c0,0x0c0,0x0c0,0x0fc,0x0c0,0x0c0,0x0c0,0x0c0,0x0fe,0x000,0x000}, /* E */
    {0x000,0x0fe,0x0c0,0x0c0,0x0c0,0x0fc,0x0c0,0x0c0,0x0c0,0x0c0,0x0c0,0x000,0x000}, /* F */
    {0x000,0x03c,0x060,0x0c0,0x0c0,0x0c0,0x0ce,0x0c6,0x0c6,0x066,0x03c,0x000,0x000}, /* G */
    {0x000,0x0ce,0x0ce,0x0ce,0x0ce,0x0fe,0x0ce,0x0ce,0x0ce,0x0ce,0x0ce,0x000,0x000}, /* H */
    {0x000,0x0fc,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x0fc,0x000,0x000}, /* I */
    {0x000,0x07c,0x00c,0x00c,0x00c,0x00c,0x00c,0x00c,0x00c,0x08c,0x078,0x000,0x000}, /* J */
    {0x000,0x0c6,0x0cc,0x0d8,0x0f0,0x0f0,0x0f8,0x0d8,0x0cc,0x0ce,0x0c6,0x000,0x000}, /* K */
    {0x000,0x060,0x060,0x060,0x060,0x060,0x060,0x060,0x060,0x060,0x07e,0x000,0x000}, /* L */
    {0x000,0x0ce,0x0ce,0x0ee,0x0fe,0x0b6,0x0b6,0x086,0x086,0x086,0x086,0x000,0x000}, /* M */
    {0x000,0x0c6,0x0e6,0x0e6,0x0e6,0x0f6,0x0d6,0x0de,0x0de,0x0ce,0x0ce,0x000,0x000}, /* N */
    {0x000,0x078,0x0cc,0x0cc,0x0c6,0x0c6,0x0c6,0x0c6,0x0cc,0x0cc,0x078,0x000,0x000}, /* O */
    {0x000,0x0fc,0x0cc,0x0c6,0x0c6,0x0cc,0x0fc,0x0c0,0x0c0,0x0c0,0x0c0,0x000,0x000}, /* P */
    {0x000,0x078,0x0cc,0x0cc,0x0c6,0x0c6,0x0c6,0x0c6,0x0cc,0x0cc,0x078,0x00c,0x004}, /* Q */
    {0x000,0x0f8,0x0cc,0x0cc,0x0cc,0x0cc,0x0f8,0x0dc,0x0cc,0x0ce,0x0c6,0x000,0x000}, /* R */
    {0x000,0x078,0x0c4,0x0c0,0x0e0,0x0f8,0x03c,0x00e,0x00e,0x08c,0x078,0x000,0x000}, /* S */
    {0x000,0x0fe,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x000,0x000}, /* T */
    {0x000,0x0c6,0x0c6,0x0c6,0x0c6,0x0c6,0x0c6,0x0c6,0x0c6,0x0cc,0x078,0x000,0x000}, /* U */
    {0x000,0x0c6,0x0c6,0x0cc,0x0cc,0x0cc,0x06c,0x068,0x078,0x078,0x038,0x000,0x000}, /* V */
    {0x000,0x182,0x182,0x186,0x1b6,0x0b6,0x0be,0x0ee,0x0ee,0x0cc,0x0cc,0x000,0x000}, /* W */
    {0x000,0x0c6,0x0cc,0x06c,0x078,0x038,0x038,0x078,0x06c,0x0cc,0x1c6,0x000,0x000}, /* X */
    {0x000,0x186,0x0ce,0x0cc,0x07c,0x078,0x038,0x030,0x030,0x030,0x030,0x000,0x000}, /* Y */
    {0x000,0x0fe,0x00e,0x01c,0x01c,0x038,0x030,0x070,0x0e0,0x0c0,0x0fe,0x000,0x000}, /* Z */
    {0x000,0x078,0x06c,0x0cc,0x0cc,0x0d6,0x0d6,0x0cc,0x0cc,0x06c,0x078,0x000,0x000}, /* 0 */
    {0x000,0x078,0x038,0x018,0x018,0x018,0x018,0x018,0x018,0x018,0x0fe,0x000,0x000}, /* 1 */
    {0x000,0x078,0x08c,0x00c,0x00c,0x01c,0x018,0x030,0x060,0x0c0,0x0fc,0x000,0x000}, /* 2 */
    {0x000,0x078,0x08c,0x00c,0x00c,0x038,0x00c,0x00c,0x004,0x08c,0x078,0x000,0x000}, /* 3 */
    {0x000,0x01c,0x03c,0x03c,0x06c,0x0cc,0x0cc,0x0fe,0x00c,0x00c,0x00c,0x000,0x000}, /* 4 */
    {0x000,0x0fc,0x0c0,0x0c0,0x0f8,0x08c,0x00c,0x00c,0x00c,0x08c,0x078,0x000,0x000}, /* 5 */
    {0x000,0x038,0x064,0x0c0,0x0f8,0x0ec,0x0c6,0x0c6,0x0c6,0x06c,0x038,0x000,0x000}, /* 6 */
    {0x000,0x0fc,0x00c,0x00c,0x01c,0x018,0x018,0x030,0x030,0x070,0x060,0x000,0x000}, /* 7 */
    {0x000,0x078,0x0cc,0x0cc,0x0cc,0x078,0x0cc,0x0c4,0x0c4,0x0cc,0x078,0x000,0x000}, /* 8 */
    {0x000,0x078,0x0cc,0x0cc,0x0cc,0x0ce,0x0ce,0x07c,0x00c,0x00c,0x078,0x000,0x000}, /* 9 */
    {0x000,0x000,0x030,0x030,0x030,0x1fe,0x1fe,0x030,0x030,0x030,0x000,0x000,0x000}, /* + */
    {0x000,0x000,0x000,0x000,0x000,0x000,0x078,0x078,0x000,0x000,0x000,0x000,0x000}, /* - */
    {0x000,0x004,0x00c,0x00c,0x008,0x018,0x010,0x030,0x020,0x060,0x040,0x0c0,0x0c0}, /* / */
    {0x000,0x000,0x000,0x000,0x030,0x030,0x030,0x000,0x030,0x030,0x030,0x000,0x000}, /* : */
    {0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x030,0x030,0x030,0x000,0x000}, /* . */
    {0x000,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x000,0x030,0x030,0x000,0x000}, /* ! */
    {0x000,0x038,0x00c,0x00c,0x01c,0x038,0x030,0x030,0x000,0x030,0x030,0x000,0x000}, /* ? */
    {0x000,0x000,0x000,0x0fe,0x0fe,0x000,0x000,0x0fe,0x0fe,0x000,0x000,0x000,0x000}, /* = */
    {0x018,0x018,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x018,0x018,0x000}, /* ( */
    {0x060,0x030,0x030,0x010,0x018,0x018,0x018,0x018,0x010,0x030,0x030,0x060,0x000}, /* ) */
    {0x000,0x036,0x034,0x024,0x0fe,0x06c,0x048,0x1fe,0x0d8,0x0d8,0x090,0x000,0x000}, /* # */
    {0x000,0x0e0,0x1a0,0x1a0,0x0e2,0x008,0x020,0x08c,0x012,0x012,0x00e,0x000,0x000}, /* % */
    {0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000}, /* space */
    {0x010,0x010,0x07c,0x0d4,0x0d0,0x0f0,0x03c,0x01c,0x014,0x0dc,0x078,0x010,0x010}, /* $ */
    {0x000,0x078,0x060,0x060,0x060,0x0f0,0x0f2,0x19a,0x19e,0x0ce,0x07e,0x000,0x000}, /* & */
    {0x000,0x030,0x030,0x030,0x030,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000}, /* ' */
    {0x000,0x04c,0x04c,0x04c,0x04c,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000}, /* " */
    {0x000,0x010,0x094,0x078,0x078,0x094,0x010,0x000,0x000,0x000,0x000,0x000,0x000}, /* * */
    {0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x030,0x030,0x030,0x020,0x000}, /* , */
    {0x000,0x000,0x000,0x000,0x030,0x030,0x030,0x000,0x030,0x030,0x030,0x020,0x000}, /* ; */
    {0x000,0x000,0x000,0x002,0x01e,0x0f0,0x0c0,0x0f0,0x01e,0x006,0x000,0x000,0x000}, /* < */
    {0x000,0x000,0x000,0x080,0x0f0,0x03c,0x00e,0x03c,0x0f0,0x080,0x000,0x000,0x000}, /* > */
    {0x000,0x038,0x0c4,0x086,0x19e,0x1b6,0x122,0x122,0x1b6,0x19e,0x080,0x040,0x03c}, /* @ */
    {0x038,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x038,0x000}, /* [ */
    {0x000,0x0c0,0x0c0,0x040,0x060,0x020,0x030,0x010,0x018,0x008,0x00c,0x00c,0x004}, /* \\ */
    {0x078,0x018,0x018,0x018,0x018,0x018,0x018,0x018,0x018,0x018,0x018,0x078,0x000}, /* ] */
    {0x000,0x030,0x078,0x06c,0x0c6,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000}, /* ^ */
    {0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x1fe,0x1fe}, /* _ */
    {0x020,0x030,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000}, /* ` */
    {0x01c,0x030,0x030,0x030,0x030,0x030,0x0e0,0x030,0x030,0x030,0x030,0x030,0x01c}, /* { */
    {0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030,0x030}, /* | */
    {0x0e0,0x030,0x030,0x030,0x030,0x038,0x01c,0x038,0x030,0x030,0x030,0x030,0x0e0}, /* } */
    {0x000,0x000,0x000,0x000,0x000,0x0e2,0x0fe,0x01c,0x000,0x000,0x000,0x000,0x000}, /* ~ */
};

typedef char Font_index_alignment[(ARRAY_LEN(font) == ARRAY_LEN(small_font)) ? 1 : -1];

typedef struct { uint32_t *pixels; int pitch; } Canvas;
/* Test branch (RedOnion unified look): text in Exo 2 through SDL_ttf and the
 * shared header, panels, selection and hints of Tweaks. Without the font the
 * built-in bitmap font below is still used. */
#include "theme/render/canvas_ui.h"
#define CUI(c) (&(CuiCanvas){(c)->pixels,(c)->pitch})
#define UI_SMALL 15
#define UI_BIG 25
static uint32_t rgb(unsigned r, unsigned g, unsigned b) { return (r << 16) | (g << 8) | b; }
static void put(Canvas *c, int x, int y, uint32_t color) {
    if ((unsigned)x < W && (unsigned)y < H) c->pixels[y * c->pitch + x] = color;
}
static void rect(Canvas *c, int x, int y, int w, int h, uint32_t color) {
    int xx, yy;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (yy = y0; yy < y1; ++yy) {
        uint32_t *p = c->pixels + yy * c->pitch;
        for (xx = x0; xx < x1; ++xx) p[xx] = color;
    }
}
/* Small labels use 1:1 native 9x13 pixel glyphs: no interpolation or blur. */
#define SMALL_W 9
#define SMALL_H 13
#define SMALL_ADVANCE 9
static void glyph(Canvas *c, int x, int y, char ch, int scale, uint32_t color) {
    int i, row, col;
    const unsigned char *bits = NULL;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));
    for (i = 0; i < (int)ARRAY_LEN(font); ++i)
        if (font[i].c == ch) { bits = font[i].bits; break; }
    if (!bits) return;
    if (scale == 1) {
        for (row = 0; row < SMALL_H; ++row) {
            uint16_t mask = small_font[i][row];
            for (col = 0; col < SMALL_W; ++col)
                if (mask & (1u << (8 - col)))
                    put(c, x + col, y + row, color);
        }
    } else {
        for (row = 0; row < 7; ++row)
            for (col = 0; col < 5; ++col)
                if (bits[row] & (1u << (4-col)))
                    rect(c,x+col*scale,y+row*scale,scale,scale,color);
    }
}
static int text_width(const char *s, int scale) {
    int count = (int)strlen(s);
    if (cui_font(scale == 1 ? UI_SMALL : UI_BIG)) return cui_text_width(s, scale == 1 ? UI_SMALL : UI_BIG);
    if (!count) return 0;
    return scale == 1 ? count * SMALL_ADVANCE - 1 : count * 6*scale - scale;
}
static void label(Canvas *c, int x, int y, const char *s, int scale, uint32_t color) {
    int advance = scale == 1 ? SMALL_ADVANCE : 6*scale;
    /* Same top-left placement as the bitmap glyphs: centre on their middle. */
    if (cui_font(scale == 1 ? UI_SMALL : UI_BIG)) {
        cui_text(CUI(c), x, y + (scale == 1 ? 6.5f : 3.5f * scale), s, scale == 1 ? UI_SMALL : UI_BIG, color, CUI_LEFT);
        return;
    }
    while (*s) { glyph(c,x,y,*s++,scale,color); x += advance; }
}
static void centered(Canvas *c, int x, int y, const char *s, int scale, uint32_t color) {
    label(c,x-text_width(s,scale)/2,y,s,scale,color);
}
static uint32_t convert_pixel(uint32_t color, const SDL_PixelFormat *fmt) {
    return (((color >> 16 & 255u) >> fmt->Rloss) << fmt->Rshift) |
           (((color >>  8 & 255u) >> fmt->Gloss) << fmt->Gshift) |
           (((color       & 255u) >> fmt->Bloss) << fmt->Bshift);
}

static bool present(SDL_Surface *screen, const uint32_t *pixels) {
    int y, x, bpp;
    SDL_PixelFormat *fmt = screen->format;
    bool locked = SDL_MUSTLOCK(screen);
    if (locked && SDL_LockSurface(screen) != 0) {
        LOG("SDL_LockSurface failed: %s", SDL_GetError());
        return false;
    }
    bpp = fmt->BytesPerPixel;
    if (!screen->pixels || (bpp != 2 && bpp != 4) ||
        screen->w != W || screen->h != H || screen->pitch < W * bpp ||
        screen->pitch % bpp != 0) {
        LOG("unsupported screen: %dx%d, pitch=%d, bpp=%d, pixels=%p",
            screen->w, screen->h, screen->pitch, bpp, screen->pixels);
        if (locked) SDL_UnlockSurface(screen);
        return false;
    }
    for (y = 0; y < H; ++y) {
        const uint32_t *src = pixels + (H - 1 - y) * W;
        void *row = (unsigned char *)screen->pixels + y * screen->pitch;
        if (bpp == 4) {
            uint32_t *dst = (uint32_t *)row;
            if (fmt->Rmask == 0x00ff0000u && fmt->Gmask == 0x0000ff00u &&
                fmt->Bmask == 0x000000ffu) {
                for (x = 0; x < W; ++x) dst[x] = src[W - 1 - x];
            } else {
                for (x = 0; x < W; ++x)
                    dst[x] = convert_pixel(src[W - 1 - x], fmt);
            }
        } else {
            uint16_t *dst = (uint16_t *)row;
            for (x = 0; x < W; ++x)
                dst[x] = (uint16_t)convert_pixel(src[W - 1 - x], fmt);
        }
    }
    if (locked) SDL_UnlockSurface(screen);
    if (SDL_Flip(screen) != 0) {
        LOG("SDL_Flip failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

typedef struct {
    bool mounted, readonly, vfat, fat32, device_verified;
    char device[256], fstype[64], volume_label[24];
    uint64_t total, free_bytes, available, used;
    dev_t devno;
    int battery; /* -1: unknown */
} CardInfo;
static CardInfo card;
/* Read-only FAT32 metadata inspection. Never clear bits on a mounted card. */
typedef struct {
    bool valid, readable, clean, no_hard_error, fsinfo_valid, boot_dirty;
    uint32_t fat1, free_clusters;
    char detail[100];
} FatStatus;
static FatStatus fatstatus;


typedef struct {
    char name[256];
    uint64_t bytes, files;
} TopGroup;
typedef struct {
    DIR *dir;
    char path[PATH_MAX];
    int group;
} DirFrame;
typedef struct {
    bool active, deep, storage, cancelled, limited;
    DirFrame stack[MAX_DIR_DEPTH];
    int depth, file_fd;
    bool quick_full_pending;
    uint64_t file_left, files, dirs, bytes, bad, skipped, entries;
    uint64_t file_reads;
    char last_error[PATH_MAX], reading[PATH_MAX];
    TopGroup groups[MAX_GROUPS];
    int group_count;
    uint8_t *buffer;
    Uint32 began;
    unsigned quick_index, quick_probe_index;
    uint64_t quick_root_files, quick_root_entries;
    uint64_t quick_full_files, quick_sample_files;
    uint64_t quick_auto_full_reserved;
    unsigned quick_oversized;
    unsigned system_dirs_found, system_dirs_missing, core_missing;
    unsigned system_files_found, system_files_missing;
    unsigned failures_count, failures_omitted;
    struct { char path[224]; int error; } failures[MAX_FAILURES];
    unsigned kernel_errors, kernel_unclean; bool kernel_checked;
    char kernel_sample[120], kernel_unclean_sample[120];
    unsigned repair_leftovers;
} FileScan;
static FileScan scan;

/* These locations follow RedOnion 4.5.x's published runtime.sh and its
 * actual SD-card layout. The order intentionally gives boot/runtime components
 * their own budgets before emulator and user-data sampling. Some directories
 * are optional (and differ by device, package selection and Onion version). */
typedef struct { const char *rel; unsigned max_files, max_entries; bool core; } QuickTarget;
static const QuickTarget quick_targets[] = {
    {".tmp_update/script",  400, 2400, true},
    {".tmp_update/bin",     210, 1200, true},
    {".tmp_update/config",  180, 1250, true},
    {".tmp_update/lib",     220, 1150, false},
    {".tmp_update/res",      60,  500, false},
    {".tmp_update/startup",  30,  180, false},
    {"miyoo/app",            80,  400, false},
    {"miyoo/lib",            80,  410, false},
    {"miyoo354/app",         45,  300, false},
    {"RetroArch/.retroarch",160, 1050, false},
    {"RetroArch",            75,  540, false},
    {"Emu",                 150, 1150, false},
    {"RApp",                 80,  650, false},
    {"App",                  90,  650, false},
    {"BIOS",                 50,  450, false},
    {"Saves",                50,  380, false},
    {"Roms",                 50,  400, false},
    {"Media",                25,  280, false},
    {"Themes",               35,  350, false},
    {"",                     30,  240, false} /* root files only */
};
/* Read critical scripts, boot resources and executables in FULL (up to 64 MiB
 * for a single file), regardless of their position in FAT directory order.
 * Every entry except the two mandatory boot helpers is optional. */
typedef struct { const char *rel; bool core; } QuickProbe;
static const QuickProbe quick_files[] = {
    {".tmp_update/runtime.sh", true},
    {".tmp_update/script/log.sh", true},
    {".tmp_update/script/network/update_networking.sh", false},
    {".tmp_update/script/network/wifi_power_save.sh", false},
    {".tmp_update/script/ota_update.sh", false},
    {".tmp_update/script/console_pictures.sh", false},
    {".tmp_update/script/patch_ra_cfg.sh", false},
    {".tmp_update/script/blue_light.sh", false},
    {".tmp_update/script/remove_last_recent_entry.sh", false},
    {".tmp_update/script/stop_audioserver.sh", false},
    {".tmp_update/config/passwd", false},
    {".tmp_update/config/group", false},
    {".tmp_update/config/startup/app", false},
    {".tmp_update/config/keymap.json", false},
    {".tmp_update/config/romwinidx.json", false},
    {".tmp_update/config/console-position.json", false},
    {".tmp_update/res/consoles/map.txt", false},
    {".tmp_update/res/consoles/roms.txt", false},
    {"miyoo/app/MainUI", false},
    {"miyoo354/app/MainUI", false},
    {".tmp_update/bin/MainUI", false},
    {".tmp_update/bin/keymon", false},
    {".tmp_update/bin/gameSwitcher", false},
    {".tmp_update/bin/bootScreen", false},
    {".tmp_update/bin/chargingState", false},
    {"miyoo/lib/libgamename.so", false},
    {"RetroArch/.retroarch/retroarch.cfg", false},
    {"RetroArch/retroarch", false},
    {"RetroArch/retroarch_miyoo354", false},
    {"App/SDCardDoctor/launch.sh", false},
    {"App/SDCardDoctor/config.json", false}
};

/* Read-only fsck is deliberately a separate optional diagnostic operation.
 * Never pass -a, -r or any writing/repair flag. */
typedef struct {
    pid_t pid;
    int pipe_fd, exit_code, wait_status;
    bool active, cancelling, complete, exited, killed;
    Uint32 cancel_at;
    char output[8192];
    size_t output_len;
    /* A mounted Linux FAT volume can be marked active without corruption.
     * Interpret the checker output cautiously: an unknown message must never
     * be silently treated as a clean bill of health. */
    char line[384];
    size_t line_len;
    bool saw_active_flag, other_messages, reported_damage, output_truncated;
    /* The sole byte-65 bit-0 difference between FAT32 boot and backup boot
     * sectors is a common effect of Linux mounting the card read/write.
     * All other differences must remain visible for offline investigation. */
    bool boot_backup_diff, boot_backup_mount_only, boot_backup_other;

} FatCheck;
static FatCheck fatcheck={.pid=-1,.pipe_fd=-1};
/* A checker stuck in uninterruptible I/O may survive cancellation. Keep its
 * PID even after releasing the UI; never launch a second checker over it. */
static pid_t detached_checker_pid=-1;

typedef enum { SPEED_NONE, SPEED_RAW_READ, SPEED_WRITE, SPEED_VERIFY } SpeedStage;
typedef struct {
    SpeedStage stage;
    int fd;
    void *data;
    uint64_t done, size;
    Uint32 started, finished;
    double read_mbps, write_mbps;
    bool direct, write_direct, verified, cancelled;
    char file[PATH_MAX], message[128];
} Speed;
static Speed speedtest;
static uint8_t test_pattern[IO_CHUNK], test_header[IO_CHUNK];

typedef struct { char name[256]; uint64_t bytes; bool directory; } Candidate;
static Candidate candidates[MAX_CANDIDATES];
static int candidates_count=0, candidates_selected=0;
static bool restore_list=false;

typedef enum {
    PAGE_MAIN, PAGE_RUNNING, PAGE_SCAN_RESULT, PAGE_STORAGE_RESULT,
    PAGE_SPEED_MENU, PAGE_SPEED_CONFIRM, PAGE_SPEED_RESULT,
    PAGE_REPAIR_MENU, PAGE_CLEAN_LIST, PAGE_CONFIRM_FILE, PAGE_FAT_STATUS, PAGE_FAT_INFO,
    PAGE_FAT_CHECK_WARN, PAGE_FAT_CHECK_RESULT, PAGE_CARD_ID, PAGE_DIAGNOSTICS, PAGE_REPORT_CONFIRM,
    PAGE_FAT_REPORT_CONFIRM, PAGE_NOTICE
} Page;
typedef enum { JOB_NONE, JOB_QUICK, JOB_DEEP, JOB_STORAGE, JOB_SPEED_READ, JOB_SPEED_WRITE, JOB_FAT_CHECK } Job;
static Page page=PAGE_MAIN, notice_back=PAGE_MAIN;
static Job job=JOB_NONE, last_job=JOB_NONE;
static int main_selected=0, speed_selected=0, repair_selected=0, storage_scroll=0, diagnostics_scroll=0;
/* Text-only information panels scroll by whole native-bitmap-font lines. */
static int info_scroll=0, info_row_count=0;
static Page info_page=PAGE_MAIN;
static char notice_title[48], notice_body[512];
static bool quit_requested=false, exit_armed=false, b_exit_armed=false;
static char card_name[64],card_manfid[32],card_serial[32],card_date[32];
static char recent_report[PATH_MAX];

enum { KEY_UP_ID, KEY_DOWN_ID, KEY_LEFT_ID, KEY_RIGHT_ID,
       KEY_A_ID, KEY_B_ID, KEY_SELECT_ID, KEY_START_ID, KEY_COUNT };
typedef struct { unsigned short evdev_code; SDLKey sdl; bool down; } Key;
static Key keys[KEY_COUNT] = {
    {KEY_UP, SDLK_UP, false}, {KEY_DOWN, SDLK_DOWN, false},
    {KEY_LEFT, SDLK_LEFT, false}, {KEY_RIGHT, SDLK_RIGHT, false},
    {KEY_SPACE, SDLK_SPACE, false}, {KEY_LEFTCTRL, SDLK_LCTRL, false},
    {KEY_RIGHTCTRL, SDLK_RCTRL, false}, {KEY_ENTER, SDLK_RETURN, false}
};

static uint16_t u16le(const unsigned char *p) { return (uint16_t)(p[0]|(p[1]<<8)); }
static uint32_t u32le(const unsigned char *p) { return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
/* Do not claim FAT32 merely because the kernel reports "vfat". */
static bool verify_fat32(const char *device) {
    unsigned char b[512];
    uint64_t reserved, fats, spf, total, root_sectors, data_sectors, clusters;
    uint32_t bps, spc, root_entries;
    int fd;
    if (strncmp(device,"/dev/",5)) return false;
    fd=open(device,O_RDONLY|O_CLOEXEC|O_NONBLOCK|O_NOFOLLOW);
    if (fd<0) return false;
    bool ok=read(fd,b,sizeof(b))==(ssize_t)sizeof(b);
    close(fd);
    if (!ok || b[510]!=0x55 || b[511]!=0xaa) return false;
    bps=u16le(b+11); spc=b[13]; reserved=u16le(b+14); fats=b[16];
    root_entries=u16le(b+17);
    total=u16le(b+19); if (!total) total=u32le(b+32);
    spf=u16le(b+22); if (!spf) spf=u32le(b+36);
    if ((bps!=512 && bps!=1024 && bps!=2048 && bps!=4096) ||
        !spc || (spc&(spc-1)) || !reserved || !fats || fats>4 || !spf || !total) return false;
    root_sectors=(root_entries*32u+(bps-1))/bps;
    if (total<=reserved+fats*spf+root_sectors) return false;
    data_sectors=total-(reserved+fats*spf+root_sectors);
    clusters=data_sectors/spc;
    return clusters>=65525 && root_entries==0 && u16le(b+22)==0;
}
/* FAT32 labels normally live in an ATTR_VOLUME_ID entry in the root
 * directory. The boot-sector BPB label (offset 71) is only a fallback and
 * can be stale. Read a bounded number of root clusters: no card writes.
 * Non-ASCII OEM bytes are shown as '?' rather than guessed characters. */
static bool fat_label_text(const unsigned char *src, char *out, size_t len) {
    size_t i, end=11;
    if (len<12 || src[0]==0x00 || src[0]==0xe5) return false;
    while (end && src[end-1]==' ') end--;
    if (!end) return false;
    for (i=0;i<end;i++) {
        unsigned char ch=src[i];
        out[i]=(ch>=32 && ch<=126)?(char)ch:'?';
    }
    out[end]=0;
    return strcmp(out,"NO NAME")!=0;
}
static void read_fat32_volume_label(const char *device, char *out, size_t len) {
    unsigned char boot[512], entry[32], fat_entry[4];
    uint32_t reserved, fat_sectors, total_sectors, root, clusters, max_clusters, c;
    uint64_t fat_start, data_start, offset, sectors, cluster_bytes;
    unsigned bps, spc, fats, fat_index=0, pass;
    int fd;
    unsigned char *buf=NULL;
    char fallback[24]="UNLABELED", name[24];
    snprintf(out,len,"UNAVAILABLE");
    if (len<12 || !device || !*device) return;
    fd=open(device,O_RDONLY|O_CLOEXEC|O_NONBLOCK|O_NOFOLLOW);
    if (fd<0) return;
    if (pread(fd,boot,sizeof(boot),0)!=(ssize_t)sizeof(boot) ||
        boot[510]!=0x55 || boot[511]!=0xaa) goto done;
    bps=u16le(boot+11);spc=boot[13];reserved=u16le(boot+14);
    fats=boot[16];fat_sectors=u32le(boot+36);root=u32le(boot+44);
    total_sectors=u16le(boot+19);if (!total_sectors) total_sectors=u32le(boot+32);
    if ((bps!=512 && bps!=1024 && bps!=2048 && bps!=4096) ||
        !spc || (spc&(spc-1)) || spc>128 || !reserved ||
        !fats || fats>4 || !fat_sectors || root<2 ||
        !total_sectors || total_sectors<=reserved+(uint64_t)fats*fat_sectors ||
        u16le(boot+22)!=0 || u16le(boot+17)!=0) goto done;
    if (boot[66]==0x29 && fat_label_text(boot+71,name,sizeof(name)))
        snprintf(fallback,sizeof(fallback),"%s",name);
    snprintf(out,len,"%s",fallback);
    sectors=total_sectors-reserved-(uint64_t)fats*fat_sectors;
    clusters=(uint32_t)(sectors/spc);
    if (clusters<65525 || root>=clusters+2u) goto done;
    cluster_bytes=(uint64_t)bps*spc;
    if (cluster_bytes>131072u) goto done;
    fat_start=(uint64_t)reserved*bps;
    data_start=((uint64_t)reserved+(uint64_t)fats*fat_sectors)*bps;
    /* FAT32 ExtFlags selects the active table if mirroring is disabled. */
    if (u16le(boot+40)&0x80u) {
        fat_index=u16le(boot+40)&0x0fu;
        if (fat_index>=fats) goto done;
    }
    fat_start+=(uint64_t)fat_index*fat_sectors*bps;
    buf=malloc((size_t)cluster_bytes);
    if (!buf) goto done;
    max_clusters=16; c=root;
    for (pass=0;pass<max_clusters && c>=2 && c<clusters+2u;pass++) {
        size_t i;
        offset=data_start+(uint64_t)(c-2u)*cluster_bytes;
        if (pread(fd,buf,(size_t)cluster_bytes,(off_t)offset)!=(ssize_t)cluster_bytes) break;
        for (i=0;i<(size_t)cluster_bytes;i+=sizeof(entry)) {
            const unsigned char *e=buf+i;
            if (e[0]==0x00) goto done; /* end of root directory */
            if (e[0]==0xe5 || e[0]==0x05 || e[11]==0x0f) continue;
            if ((e[11]&0x1fu)==0x08u && !(e[11]&0x10u) &&
                fat_label_text(e,name,sizeof(name))) {
                snprintf(out,len,"%s",name);
                goto done;
            }
        }
        if ((uint64_t)c*4u+4u>(uint64_t)fat_sectors*bps) break;
        if (pread(fd,fat_entry,4,(off_t)(fat_start+(uint64_t)c*4u))!=4) break;
        c=u32le(fat_entry)&0x0fffffffu;
        if (c>=0x0ffffff8u || c==0x0ffffff7u) break;
    }
done:
    free(buf);
    close(fd);
}

/* FAT32: FAT[1] bit 27=clean, bit 26=no prior I/O error. */
static void refresh_card(void);
static void inspect_fat32(void) {
    unsigned char boot[512], info[512], fat[4];
    unsigned bps, reserved, fsinfo, fats;
    uint32_t spf;
    int fd;
    memset(&fatstatus,0,sizeof(fatstatus));
    refresh_card();
    if (!card.mounted || !card.fat32 || !card.device_verified) {
        snprintf(fatstatus.detail,sizeof(fatstatus.detail),"FAT32 PARTITION NOT VERIFIED");return;
    }
    fd=open(card.device,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if (fd<0) {
        snprintf(fatstatus.detail,sizeof(fatstatus.detail),"CANNOT READ BLOCK DEVICE");return;
    }
    if (pread(fd,boot,sizeof(boot),0)!=(ssize_t)sizeof(boot)) goto fail;
    bps=u16le(boot+11);reserved=u16le(boot+14);
    fsinfo=u16le(boot+48);fats=boot[16];spf=u32le(boot+36);
    if ((bps!=512 && bps!=1024 && bps!=2048 && bps!=4096) || reserved<2 ||
        !fats || fats>4 || !spf || fsinfo>=reserved || !verify_fat32(card.device)) goto fail;
    if (pread(fd,fat,sizeof(fat),(off_t)reserved*bps+4)!=(ssize_t)sizeof(fat)) goto fail;
    fatstatus.fat1=u32le(fat);
    fatstatus.boot_dirty=(boot[65]&0x01u)!=0;
    /* This verifies the marker, not overall directory/cluster integrity. */
    fatstatus.clean=(fatstatus.fat1 & 0x08000000u)!=0;
    fatstatus.no_hard_error=(fatstatus.fat1 & 0x04000000u)!=0;
    fatstatus.readable=true;fatstatus.valid=true;
    if (fsinfo>0 && pread(fd,info,sizeof(info),(off_t)fsinfo*bps)==(ssize_t)sizeof(info)) {
        fatstatus.fsinfo_valid=u32le(info)==0x41615252u &&
            u32le(info+484)==0x61417272u && u32le(info+508)==0xaa550000u;
        fatstatus.free_clusters=u32le(info+488);
    }
    snprintf(fatstatus.detail,sizeof(fatstatus.detail),"FLAGS READ - MOUNTED VALUES MAY CHANGE");
    close(fd);return;
fail:
    snprintf(fatstatus.detail,sizeof(fatstatus.detail),"COULD NOT READ FAT32 METADATA");
    close(fd);
}
static int read_battery(void) {
    /* On Onion the voltage-derived estimate is often published here. */
    FILE *onion=fopen("/tmp/percBat","r");
    int estimate=-1;
    if (onion) {
        if (fscanf(onion,"%d",&estimate)!=1 || estimate<0 || estimate>100) estimate=-1;
        fclose(onion);
        if (estimate>=0) return estimate;
    }
    DIR *d=opendir("/sys/class/power_supply");
    struct dirent *ent;
    int result=-1;
    if (!d) return -1;
    while ((ent=readdir(d))) {
        char typepath[PATH_MAX], path[PATH_MAX], type[32];
        int charge;
        FILE *fp;
        if (ent->d_name[0]=='.') continue;
        if (snprintf(typepath,sizeof(typepath),"/sys/class/power_supply/%s/type",ent->d_name)>=(int)sizeof(typepath)) continue;
        fp=fopen(typepath,"r"); if (!fp) continue;
        type[0]=0; (void)fgets(type,sizeof(type),fp); fclose(fp);
        if (strncmp(type,"Battery",7)) continue;
        if (snprintf(path,sizeof(path),"/sys/class/power_supply/%s/capacity",ent->d_name)>=(int)sizeof(path)) continue;
        fp=fopen(path,"r"); if (!fp) continue;
        if (fscanf(fp,"%d",&charge)==1 && charge>=0 && charge<=100) result=charge;
        fclose(fp);
        break;
    }
    closedir(d);
    return result;
}
static void refresh_card(void) {
    struct statvfs vfs;
    struct stat sb;
    char dev[256], mount[PATH_MAX], fs[64], opts[256];
    int dump, pass;
    FILE *fp;
    memset(&card,0,sizeof(card));
    card.battery=read_battery();
    strcpy(card.device,"UNKNOWN"); strcpy(card.fstype,"UNKNOWN"); strcpy(card.volume_label,"UNAVAILABLE");
    fp=fopen("/proc/mounts","r");
    if (fp) {
        while (fscanf(fp,"%255s %4095s %63s %255s %d %d",dev,mount,fs,opts,&dump,&pass)==6) {
            if (strcmp(mount,ROOT_PATH)) continue;
            card.mounted=true;
            snprintf(card.device,sizeof(card.device),"%s",dev);
            snprintf(card.fstype,sizeof(card.fstype),"%s",fs);
            card.vfat=!strcmp(fs,"vfat") || !strcmp(fs,"msdos");
            card.readonly=strstr(opts,"ro")!=NULL && (!strncmp(opts,"ro,",3) || !strcmp(opts,"ro"));
            break;
        }
        fclose(fp);
    }
    if (card.mounted && stat(ROOT_PATH,&sb)==0) card.devno=sb.st_dev;
    if (card.mounted && card.devno && !strncmp(card.device,"/dev/",5) &&
        stat(card.device,&sb)==0 && S_ISBLK(sb.st_mode) && sb.st_rdev==card.devno)
        card.device_verified=true;
    /* Never inspect or benchmark an unverified block device. */
    if (card.mounted && statvfs(ROOT_PATH,&vfs)==0) {
        uint64_t unit=vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
        card.total=(uint64_t)vfs.f_blocks*unit;
        card.free_bytes=(uint64_t)vfs.f_bfree*unit;
        card.available=(uint64_t)vfs.f_bavail*unit;
        card.used=card.total-card.free_bytes;
        if (vfs.f_flag & ST_RDONLY) card.readonly=true;
    }
    if (card.vfat && card.device_verified) card.fat32=verify_fat32(card.device);
    if (card.fat32) read_fat32_volume_label(card.device,card.volume_label,sizeof(card.volume_label));
}
static DIR *open_dir_safe(const char *,dev_t);
static void sysfs_line(char *dest,size_t max,const char *path) {
    FILE *f=fopen(path,"r");
    if (!f) {snprintf(dest,max,"UNAVAILABLE");return;}
    if (!fgets(dest,(int)max,f)) snprintf(dest,max,"UNAVAILABLE");
    else dest[strcspn(dest,"\r\n")]=0;
    fclose(f);
}
/* CID MID identifies the card manufacturer/controller source, not the
 * retail brand. Many brands outsource production and IDs can be spoofed. */
static const char *manufacturer_name(const char *raw) {
    char *end;
    unsigned long id;
    if (!raw || !*raw || !strcmp(raw,"UNAVAILABLE")) return "UNKNOWN";
    errno=0;id=strtoul(raw,&end,0);
    if (errno || end==raw || (*end && *end!='\n') || id>255u) return "UNKNOWN";
    switch (id) {
    case 0x01: return "PANASONIC";
    case 0x02: return "TOSHIBA / KIOXIA";
    case 0x03: return "SANDISK";
    case 0x1b: return "SAMSUNG";
    case 0x1d: return "ADATA";
    case 0x27: return "PHISON";
    case 0x28: return "LEXAR";
    case 0x31: return "SILICON POWER";
    case 0x41: return "KINGSTON";
    case 0x74: return "TRANSCEND";
    case 0x82: return "SONY";
    case 0xad: return "LONGSYS / AMAZON+"; /* OEM for multiple retail brands */
    default: return "UNKNOWN";
    }
}
static void read_card_identity(void) {
    char base[80],path[PATH_MAX],*tail;
    const char *name=strrchr(card.device,'/');
    name=name?name+1:card.device;
    snprintf(card_name,sizeof(card_name),"UNAVAILABLE");
    snprintf(card_manfid,sizeof(card_manfid),"UNAVAILABLE");
    snprintf(card_serial,sizeof(card_serial),"UNAVAILABLE");
    snprintf(card_date,sizeof(card_date),"UNAVAILABLE");
    if (!card.device_verified || strncmp(name,"mmcblk",6)) return;
    if (strlen(name)>=sizeof(base)) return;
    memcpy(base,name,strlen(name)+1);
    tail=strrchr(base,'p');
    if (tail && tail>base+6 && isdigit((unsigned char)tail[1])) {
        char *q=tail+1;
        while (*q && isdigit((unsigned char)*q)) q++;
        if (!*q) *tail=0;
    }
#define READ_CARD_FIELD(field,out) do { \
    snprintf(path,sizeof(path),"/sys/block/%s/device/" field,base); \
    sysfs_line(out,sizeof(out),path); \
} while(0)
    READ_CARD_FIELD("name",card_name);
    READ_CARD_FIELD("manfid",card_manfid);
    READ_CARD_FIELD("serial",card_serial);
    READ_CARD_FIELD("date",card_date);
#undef READ_CARD_FIELD
}
/* Existing logs are historical, not proof the scan itself caused an error. */
static void inspect_kernel_log(void) {
    char *log=malloc(65536u);
    int count;
    scan.kernel_errors=0;scan.kernel_unclean=0;scan.kernel_checked=false;
    scan.kernel_sample[0]=0;scan.kernel_unclean_sample[0]=0;
    if (!log) return;
    count=klogctl(3,log,65535); /* SYSLOG_ACTION_READ_ALL; does not consume */
    if (count>=0) scan.kernel_checked=true;
    if (count>0) {
        char *pos=log,*end=log+count;
        *end=0;
        while (pos<end) {
            char *next=memchr(pos,'\n',(size_t)(end-pos));
            size_t n=next?(size_t)(next-pos):(size_t)(end-pos);
            char old=pos[n];pos[n]=0;
            /* Standard Linux FAT mount notice after an abrupt power loss.
             * It is NOT proof of file corruption or a device I/O failure. */
            bool unclean=(strcasestr(pos,"FAT-fs") || strcasestr(pos,"vfat")) &&
                strcasestr(pos,"Volume was not properly unmounted")!=NULL;
            bool device_issue=(strcasestr(pos,"mmc") || strcasestr(pos,"FAT-fs")) &&
                (strcasestr(pos,"error") || strcasestr(pos,"fail") ||
                 strcasestr(pos,"timeout") || strcasestr(pos,"reset") ||
                 strcasestr(pos,"corrupt"));
            if (unclean) {
                if (++scan.kernel_unclean==1) {
                    size_t take=n<sizeof(scan.kernel_unclean_sample)-1?n:sizeof(scan.kernel_unclean_sample)-1;
                    memcpy(scan.kernel_unclean_sample,pos,take);
                    scan.kernel_unclean_sample[take]=0;
                }
            } else if (device_issue || strcasestr(pos,"I/O error") ||
                       strcasestr(pos,"buffer i/o") || strcasestr(pos,"end_request")) {
                if (++scan.kernel_errors==1) {
                    size_t take=n<sizeof(scan.kernel_sample)-1?n:sizeof(scan.kernel_sample)-1;
                    memcpy(scan.kernel_sample,pos,take);scan.kernel_sample[take]=0;
                }
            }
            pos[n]=old;
            if (!next) break;
            pos=next+1;
        }
    }
    free(log);
}
static void inspect_repair_leftovers(void) {
    DIR *d=open_dir_safe(ROOT_PATH,card.devno);
    struct dirent *e;
    scan.repair_leftovers=0;
    if (!d) return;
    while ((e=readdir(d))) {
        const char *name=e->d_name;
        const size_t n=strlen(name);
        if (!strncmp(name,"FOUND.",6) && n==9 &&
            isdigit((unsigned char)name[6]) && isdigit((unsigned char)name[7]) &&
            isdigit((unsigned char)name[8])) scan.repair_leftovers++;
        if (n>=8 && !strncasecmp(name,"FSCK",4) && !strcasecmp(name+n-4,".REC"))
            scan.repair_leftovers++;
    }
    closedir(d);
}
static void human_size(uint64_t bytes, char *buf, size_t sz) {
    const char *units[]={"B","KB","MB","GB","TB"};
    double val=(double)bytes;
    int unit=0;
    while (val>=1024.0 && unit<4) { val/=1024.0; ++unit; }
    snprintf(buf,sz,val>=100 ? "%.0f %s" : "%.1f %s",val,units[unit]);
}
static void set_notice(const char *title, const char *detail) {
    snprintf(notice_title,sizeof(notice_title),"%s",title);
    snprintf(notice_body,sizeof(notice_body),"%s",detail);
    /* A notice is an overlay, not an implicit jump to the main menu. */
    switch (page) {
    case PAGE_RUNNING:
        notice_back=(job==JOB_SPEED_READ || job==JOB_SPEED_WRITE)?PAGE_SPEED_MENU:
                    job==JOB_FAT_CHECK?PAGE_REPAIR_MENU:PAGE_MAIN;
        break;
    case PAGE_CONFIRM_FILE: notice_back=PAGE_CLEAN_LIST; break;
    case PAGE_REPORT_CONFIRM: notice_back=PAGE_DIAGNOSTICS; break;
    case PAGE_FAT_REPORT_CONFIRM: notice_back=PAGE_FAT_CHECK_RESULT; break;
    case PAGE_NOTICE: break; /* Keep the existing parent of nested notices. */
    default: notice_back=page; break;
    }
    page=PAGE_NOTICE;
    info_page=PAGE_MAIN; info_scroll=0;
    needs_redraw=true;
}
static void safe_short(const char *in, char *out, size_t len) {
    size_t i=0;
    if (!len) return;
    for (; *in && i+1<len; ++in) {
        unsigned char c=(unsigned char)*in;
        if (c>='a'&&c<='z') c-=32;
        if (!(c>='A'&&c<='Z') && !(c>='0'&&c<='9') &&
            c!=' '&&c!='.'&&c!='-'&&c!='/'&&c!=':'&&c!='+'&&c!='%'&&c!='#') c=' ';
        out[i++]=(char)c;
    }
    out[i]=0;
}
/* Use last component of a filename for the compact bitmap interface. */
static void compact_name(const char *path, char *out, size_t len) {
    const char *base=strrchr(path,'/');
    safe_short(base?base+1:path,out,len);
}
static bool safe_filename(const char *s) {
    return *s && strcmp(s,".") && strcmp(s,"..") && !strchr(s,'/') &&
           !strchr(s,'\\') && strlen(s)<240;
}
static bool path_join(char *dst,size_t size,const char *a,const char *b) {
    int len=snprintf(dst,size,"%s/%s",a,b);
    return len>0 && (size_t)len<size;
}
static void scan_error(const char *path, int err) {
    ++scan.bad;
    if (!scan.last_error[0]) snprintf(scan.last_error,sizeof(scan.last_error),"%s",path);
    if (scan.failures_count<MAX_FAILURES) {
        unsigned i=scan.failures_count++;
        snprintf(scan.failures[i].path,sizeof(scan.failures[i].path),"%s",path);
        scan.failures[i].error=err;
    } else scan.failures_omitted++;
    if (scan.bad<=MAX_FAILURES) LOG("scan failure: %s: %s",path,strerror(err));
}
/* Save reports only after an explicit confirmation, under the verified app's
 * reports/ directory. Use directory-relative opens to refuse symlink swaps. */
static int open_reports_directory(char *reason,size_t reason_len) {
    struct stat st;
    int appfd=-1, reportsfd=-1;
    refresh_card();
    if (!card.mounted || card.readonly || !card.device_verified) {
        snprintf(reason,reason_len,"CARD NOT WRITABLE OR VERIFIED");
        return -1;
    }
    if (lstat(ROOT_PATH "/App",&st)<0 || !S_ISDIR(st.st_mode) || st.st_dev!=card.devno) {
        snprintf(reason,reason_len,"UNSAFE APP FOLDER");
        return -1;
    }
    appfd=open(ROOT_PATH "/App/SDCardDoctor",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (appfd<0 || fstat(appfd,&st)<0 || !S_ISDIR(st.st_mode) || st.st_dev!=card.devno) {
        if (appfd>=0) close(appfd);
        snprintf(reason,reason_len,"UNSAFE APP FOLDER");
        return -1;
    }
    if (mkdirat(appfd,"reports",0755)<0 && errno!=EEXIST) {
        close(appfd);
        snprintf(reason,reason_len,"CANNOT CREATE REPORTS FOLDER");
        return -1;
    }
    reportsfd=openat(appfd,"reports",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    close(appfd);
    if (reportsfd<0 || fstat(reportsfd,&st)<0 || !S_ISDIR(st.st_mode) || st.st_dev!=card.devno) {
        if (reportsfd>=0) close(reportsfd);
        snprintf(reason,reason_len,"UNSAFE REPORTS FOLDER");
        return -1;
    }
    return reportsfd;
}
static int create_report_file(int reportsfd,const char *prefix,char *basename,size_t basename_len) {
    for (int attempt=0;attempt<20;attempt++) {
        int fd;
        if (snprintf(basename,basename_len,"%s-%ld-%d.txt",prefix,(long)time(NULL),attempt)
            >=(int)basename_len) return -1;
        fd=openat(reportsfd,basename,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if (fd>=0) return fd;
        if (errno!=EEXIST) break;
    }
    return -1;
}
static bool save_scan_report(char *reason,size_t reason_len) {
    FILE *f;
    int fd, reportsfd, rc;
    char basename[96];
    reportsfd=open_reports_directory(reason,reason_len);
    if (reportsfd<0) return false;
    fd=create_report_file(reportsfd,"report",basename,sizeof(basename));
    if (fd<0) {
        close(reportsfd);
        snprintf(reason,reason_len,"CANNOT CREATE REPORT");return false;
    }
    f=fdopen(fd,"w");
    if (!f) {
        close(fd);unlinkat(reportsfd,basename,0);close(reportsfd);
        snprintf(reason,reason_len,"REPORT OPEN FAILED");return false;
    }
    fprintf(f,"SD Card Doctor v0.6.3 - scan report\n");
    fprintf(f,"Files: %" PRIu64 " Directories: %" PRIu64 " Errors: %" PRIu64 "\n",scan.files,scan.dirs,scan.bad);
    if (scan.limited) fprintf(f,"Quick Sample: full-file reads %" PRIu64
        ", sampled reads %" PRIu64 ", oversized priority files sampled %u\n"
        "Core installation components missing: %u\n",
        scan.quick_full_files,scan.quick_sample_files,scan.quick_oversized,scan.core_missing);
    fprintf(f,"Kernel log accessible: %s\nKernel I/O/error alerts (historical): %u\n"
            "Card was not shut down cleanly (historical notices): %u\n"
            "Repair leftovers: %u\n",
            scan.kernel_checked?"yes":"no",scan.kernel_errors,
            scan.kernel_unclean,scan.repair_leftovers);
    if (scan.kernel_unclean)
        fprintf(f,"Note: an unclean shutdown does not prove file corruption.\n"
                  "First unclean-shutdown notice: %s\n",scan.kernel_unclean_sample);
    if (scan.kernel_errors)
        fprintf(f,"First kernel I/O/error alert: %s\n",scan.kernel_sample);
    for (unsigned i=0;i<scan.failures_count;i++)
        fprintf(f,"Error %u: %s (errno=%d, %s)\n",i+1,scan.failures[i].path,
                scan.failures[i].error,strerror(scan.failures[i].error));
    if (scan.failures_omitted) fprintf(f,"More omitted: %u\n",scan.failures_omitted);
    rc=fflush(f);
    if (rc==0) rc=fsync(fd);
    if (fclose(f)!=0) rc=-1;
    if (rc!=0) {
        unlinkat(reportsfd,basename,0);close(reportsfd);
        snprintf(reason,reason_len,"REPORT WRITE FAILED");return false;
    }
    close(reportsfd);
    snprintf(recent_report,sizeof(recent_report),ROOT_PATH "/App/SDCardDoctor/reports/%s",basename);
    LOG("report saved: %s",recent_report);
    snprintf(reason,reason_len,"REPORT SAVED IN REPORTS FOLDER");return true;
}
/* Only explicit A -> confirmation -> A writes a FAT check report. */
static bool save_fat_report(char *reason,size_t reason_len) {
    FILE *f;
    int fd, reportsfd, rc;
    char basename[96];
    reportsfd=open_reports_directory(reason,reason_len);
    if (reportsfd<0) return false;
    fd=create_report_file(reportsfd,"fat-report",basename,sizeof(basename));
    if (fd<0) {
        close(reportsfd);
        snprintf(reason,reason_len,"CANNOT CREATE FAT REPORT");return false;
    }
    f=fdopen(fd,"w");
    if (!f) {
        close(fd);unlinkat(reportsfd,basename,0);close(reportsfd);
        snprintf(reason,reason_len,"REPORT OPEN FAILED");return false;
    }
    fprintf(f,"SD Card Doctor v0.6.3 - read-only FAT check report\n");
    fprintf(f,"Partition: %s\nMounted: %s\nFilesystem: %s\n",
            card.device,card.mounted?"yes":"no",card.fstype);
    fprintf(f,"Volume label: %s\n",card.volume_label);
    fprintf(f,"Checker: fsck.fat -n (no repairs performed)\nExit code: %d\n",
            fatcheck.exit_code);
    fprintf(f,"Cancelled: %s\nOutput truncated: %s\n",
            fatcheck.cancelling?"yes":"no",fatcheck.output_truncated?"yes":"no");
    fprintf(f,"Result flags: reported_damage=%d backup_diff=%d other_output=%d\n",
            fatcheck.reported_damage,fatcheck.boot_backup_other,fatcheck.other_messages);
    fprintf(f,"CAUTION: mounted FAT32 checks are provisional.\n\nChecker output:\n%s\n",
            fatcheck.output);
    rc=fflush(f);
    if (rc==0) rc=fsync(fd);
    if (fclose(f)!=0) rc=-1;
    if (rc!=0) {
        unlinkat(reportsfd,basename,0);close(reportsfd);
        snprintf(reason,reason_len,"REPORT WRITE FAILED");return false;
    }
    close(reportsfd);
    snprintf(recent_report,sizeof(recent_report),ROOT_PATH "/App/SDCardDoctor/reports/%s",basename);
    LOG("FAT report saved: %s",recent_report);
    snprintf(reason,reason_len,"FAT REPORT SAVED IN REPORTS FOLDER");return true;
}

static int group_for(const char *name) {
    int i;
    for (i=0;i<scan.group_count;i++) if (!strcmp(scan.groups[i].name,name)) return i;
    if (scan.group_count>=MAX_GROUPS-1) {
        if (scan.group_count==MAX_GROUPS-1) {
            strcpy(scan.groups[MAX_GROUPS-1].name,"OTHER FOLDERS");
            scan.group_count=MAX_GROUPS;
        }
        return MAX_GROUPS-1;
    }
    i=scan.group_count++;
    snprintf(scan.groups[i].name,sizeof(scan.groups[i].name),"%s",name);
    return i;
}
static int compare_groups(const void *aa,const void *bb);
static DIR *open_dir_safe(const char *path,dev_t expected_dev) {
    struct stat st;
    int fd=open(path,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    DIR *d;
    if (fd<0) return NULL;
    if (fstat(fd,&st)<0 || !S_ISDIR(st.st_mode) || st.st_dev!=expected_dev) {
        close(fd);errno=EXDEV;return NULL;
    }
    d=fdopendir(fd);
    if (!d) close(fd);
    return d;
}
/* Full read of explicitly named critical files is split into 256 KiB
 * pieces by advance_scan(), not performed in a blocking probe callback. */
static void quick_probe_file(const QuickProbe *probe) {
    char path[PATH_MAX];
    struct stat st;
    int fd;
    uint64_t wanted;
    if (!path_join(path,sizeof(path),ROOT_PATH,probe->rel)) return;
    if (lstat(path,&st)<0) {
        if (errno==ENOENT) {
            ++scan.system_files_missing;
            if (probe->core) ++scan.core_missing;
        } else scan_error(path,errno);
        return;
    }
    if (st.st_dev!=card.devno || !S_ISREG(st.st_mode)) {
        if (probe->core) ++scan.core_missing;
        ++scan.skipped;
        return;
    }
    fd=open(path,O_RDONLY|O_CLOEXEC|O_NONBLOCK|O_NOFOLLOW);
    if (fd<0) {scan_error(path,errno);return;}
    ++scan.system_files_found;
    ++scan.files;
    snprintf(scan.reading,sizeof(scan.reading),"%s",path);
    wanted=(uint64_t)st.st_size;
    if (wanted>QUICK_KEY_FILE_MAX) {
        wanted=QUICK_SAMPLE_BYTES;
        ++scan.quick_oversized;
        ++scan.quick_sample_files;
    } else {
        scan.quick_full_pending=true;
    }
    if (!wanted) {
        if (scan.quick_full_pending) ++scan.quick_full_files;
        scan.quick_full_pending=false;
        close(fd);return;
    }
    scan.file_fd=fd;
    scan.file_left=wanted;
}
/* A known probe is already checked before general folder sampling. Avoid
 * reading it twice, and preserve the budget for other system files. */
static bool quick_is_probe(const char *path) {
    size_t rootlen=strlen(ROOT_PATH);
    size_t i;
    const char *rel;
    if (strncmp(path,ROOT_PATH,rootlen) || path[rootlen]!='/') return false;
    rel=path+rootlen+1;
    for (i=0;i<ARRAY_LEN(quick_files);i++)
        if (!strcmp(rel,quick_files[i].rel)) return true;
    return false;
}
static bool quick_auto_full(const char *path,const char *name,uint64_t size) {
    const char *ext=strrchr(name,'.');
    bool config=false,script=false,launcher=false;
    if (!ext || size>QUICK_AUTO_FULL_MAX ||
        scan.quick_auto_full_reserved+size>QUICK_AUTO_FULL_BUDGET) return false;
    script=strstr(path,ROOT_PATH "/.tmp_update/script/") == path;
    config=strstr(path,ROOT_PATH "/.tmp_update/config/") == path ||
           strstr(path,ROOT_PATH "/RetroArch/.retroarch/") == path;
    launcher=strstr(path,ROOT_PATH "/Emu/")==path ||
             strstr(path,ROOT_PATH "/RApp/")==path ||
             strstr(path,ROOT_PATH "/App/")==path;
    if ((script && (!strcmp(ext,".sh") || !strcmp(ext,".conf"))) ||
        (config && (!strcmp(ext,".json") || !strcmp(ext,".cfg") ||
                    !strcmp(ext,".conf") || !strcmp(ext,".ini"))) ||
        (launcher && (!strcmp(name,"launch.sh") || !strcmp(name,"config.json")))) {
        scan.quick_auto_full_reserved+=size;
        return true;
    }
    return false;
}
/* Begin the next available important folder. A missing optional folder is OK. */
static bool quick_open_next(void) {
    while (scan.quick_index<ARRAY_LEN(quick_targets)) {
        const QuickTarget *t=&quick_targets[scan.quick_index++];
        char path[PATH_MAX];
        struct stat st;
        DIR *dir;
        if (t->rel[0]) {
            if (!path_join(path,sizeof(path),ROOT_PATH,t->rel)) continue;
        } else snprintf(path,sizeof(path),"%s",ROOT_PATH);
        if (lstat(path,&st)<0) {
            if (errno==ENOENT) {
                if (t->core) ++scan.core_missing;
                ++scan.system_dirs_missing;
            } else scan_error(path,errno);
            continue;
        }
        if (!S_ISDIR(st.st_mode) || st.st_dev!=card.devno) {
            if (t->core) ++scan.core_missing;
            ++scan.skipped;
            continue;
        }
        dir=open_dir_safe(path,card.devno);
        if (!dir) {scan_error(path,errno);continue;}
        scan.stack[0].dir=dir;
        snprintf(scan.stack[0].path,sizeof(scan.stack[0].path),"%s",path);
        scan.stack[0].group=0;
        scan.depth=1;
        scan.quick_root_files=0;
        scan.quick_root_entries=0;
        ++scan.system_dirs_found;
        snprintf(scan.reading,sizeof(scan.reading),"%s",path);
        return true;
    }
    return false;
}
static void close_scan_dirs(void) {
    int i;
    for (i=scan.depth-1;i>=0;i--) {
        if (scan.stack[i].dir) {closedir(scan.stack[i].dir);scan.stack[i].dir=NULL;}
    }
    scan.depth=0;
}
static void stop_scan(void) {
    if (scan.file_fd>=0) { close(scan.file_fd); scan.file_fd=-1; }
    scan.quick_full_pending=false;
    close_scan_dirs();
    free(scan.buffer); scan.buffer=NULL;
    scan.active=false;
}
static bool begin_scan(bool deep,bool storage) {
    DIR *dir;
    struct stat root_st;
    stop_scan();
    refresh_card();
    if (!card.mounted) {set_notice("NO CARD", "SD CARD IS NOT MOUNTED");return false;}
    memset(&scan,0,sizeof(scan)); scan.file_fd=-1;
    if (stat(ROOT_PATH,&root_st)<0 || !S_ISDIR(root_st.st_mode)) {
        set_notice("SCAN ERROR","CARD ROOT IS NOT AVAILABLE");return false;
    }
    card.devno=root_st.st_dev;
    dir=(deep || storage)?open_dir_safe(ROOT_PATH,card.devno):NULL;
    if ((deep || storage) && !dir) {set_notice("SCAN ERROR",strerror(errno));return false;}
    scan.buffer=malloc(IO_CHUNK);
    if (!scan.buffer) {if (dir) closedir(dir);set_notice("NO MEMORY","CANNOT ALLOCATE READ BUFFER");return false;}
    scan.stack[0].dir=dir; strcpy(scan.stack[0].path,ROOT_PATH);
    scan.stack[0].group=0; scan.depth=dir?1:0;
    scan.group_count=1;
    strcpy(scan.groups[0].name,"ROOT FILES");
    scan.deep=deep; scan.storage=storage; scan.active=true; scan.began=SDL_GetTicks();
    page=PAGE_RUNNING;
    job=storage ? JOB_STORAGE : deep ? JOB_DEEP : JOB_QUICK;
    last_job=job;
    LOG("scan begin: mode=%s",storage?"storage":deep?"deep":"quick");
    return true;
}
/* Each call does a bounded amount of work, so B/exit can be polled. */
static void advance_scan(void) {
    int n;
    Uint32 started=SDL_GetTicks();
    if (!scan.active) return;
    for (n=0;n<512;n++) {
        DirFrame *f;
        struct dirent *e;
        struct stat st;
        char path[PATH_MAX];
        int err, group;
        if (scan.file_fd>=0) {
            size_t wanted=scan.file_left<IO_CHUNK?(size_t)scan.file_left:IO_CHUNK;
            ssize_t rd=read(scan.file_fd,scan.buffer,wanted);
            if (rd>0) {
                scan.bytes+=(uint64_t)rd; scan.file_reads+=(uint64_t)rd;
                scan.file_left-=(uint64_t)rd;
                if (!scan.file_left && scan.quick_full_pending) ++scan.quick_full_files;
            } else {
                scan_error(scan.reading,rd<0?errno:EIO); /* premature EOF */
            }
            if (rd<=0 || !scan.file_left) {
                close(scan.file_fd);scan.file_fd=-1;scan.file_left=0;
                scan.quick_full_pending=false;
            }
            if (SDL_GetTicks()-started >= 28 && n>=8) break;
            continue;
        }
        if (!scan.deep && !scan.storage) {
            /* First inspect named system files regardless of FAT directory order. */
            if (scan.quick_probe_index<ARRAY_LEN(quick_files)) {
                quick_probe_file(&quick_files[scan.quick_probe_index++]);
                if (SDL_GetTicks()-started>=28 && n>=8) break;
                continue;
            }
            if (scan.depth>0) {
                const QuickTarget *t=&quick_targets[scan.quick_index-1];
                if (scan.quick_root_files>=t->max_files ||
                    scan.quick_root_entries>=t->max_entries ||
                    scan.entries>=QUICK_MAX_ENTRIES || scan.files>=QUICK_MAX_FILES) {
                    close_scan_dirs();
                    scan.limited=true;
                }
            }
            if (scan.depth==0 && scan.entries<QUICK_MAX_ENTRIES &&
                scan.files<QUICK_MAX_FILES && quick_open_next()) continue;
        }
        if (scan.depth==0 || (!scan.deep && !scan.storage &&
                              (scan.entries>=QUICK_MAX_ENTRIES || scan.files>=QUICK_MAX_FILES))) {
            if (scan.depth>0) close_scan_dirs();
            if (!scan.deep && !scan.storage) scan.limited=true;
            scan.active=false; free(scan.buffer);scan.buffer=NULL;
            if (scan.storage) qsort(scan.groups,(size_t)scan.group_count,sizeof(scan.groups[0]),compare_groups);
            if (!scan.storage) {inspect_kernel_log();inspect_repair_leftovers();}
            page=scan.storage ? PAGE_STORAGE_RESULT : PAGE_SCAN_RESULT;
            job=JOB_NONE;
            LOG("scan done: files=%" PRIu64 " dirs=%" PRIu64 " bad=%" PRIu64 " skipped=%" PRIu64
        " fully_read=%" PRIu64 " sampled=%" PRIu64 " oversized_probes=%u",
        scan.files,scan.dirs,scan.bad,scan.skipped,scan.quick_full_files,
        scan.quick_sample_files,scan.quick_oversized);
            needs_redraw=true;
            return;
        }
        f=&scan.stack[scan.depth-1];
        errno=0;
        e=readdir(f->dir);
        if (!e) {
            err=errno; if (err) scan_error(f->path,err);
            closedir(f->dir);f->dir=NULL;--scan.depth;
            continue;
        }
        if (!strcmp(e->d_name,".") || !strcmp(e->d_name,"..")) continue;
        ++scan.entries;
        if (!scan.deep && !scan.storage) ++scan.quick_root_entries;
        if (e->d_type==DT_LNK) {++scan.skipped;continue;}
        if (!path_join(path,sizeof(path),f->path,e->d_name)) {++scan.skipped;continue;}
        /* Avoid repeated absolute-path lookups inside large FAT32 directories. */
        if (fstatat(dirfd(f->dir),e->d_name,&st,AT_SYMLINK_NOFOLLOW)<0) {
            scan_error(path,errno);continue;
        }
        group=(scan.depth==1 && S_ISDIR(st.st_mode)) ? group_for(e->d_name) :
              (scan.depth==1 ? 0 : f->group);
        if (st.st_dev!=card.devno) {++scan.skipped;continue;}
        if (S_ISLNK(st.st_mode)) {++scan.skipped;continue;}
        if (S_ISDIR(st.st_mode)) {
            DIR *child;
            ++scan.dirs;
            if (scan.depth>=MAX_DIR_DEPTH || (!scan.deep && !scan.storage &&
                (scan.depth>=4 || !quick_targets[scan.quick_index-1].rel[0]))) {
                ++scan.skipped;continue;
            }
            child=open_dir_safe(path,card.devno);
            if (!child) {scan_error(path,errno);continue;}
            f=&scan.stack[scan.depth]; f->dir=child;
            snprintf(f->path,sizeof(f->path),"%s",path);f->group=group;
            ++scan.depth;
        } else if (S_ISREG(st.st_mode)) {
            if (!scan.deep && !scan.storage && quick_is_probe(path)) continue;
            ++scan.files;
            if (!scan.deep && !scan.storage) ++scan.quick_root_files;
            if (scan.storage) scan.bytes+=(uint64_t)st.st_size;
            if (group>=0 && group<scan.group_count) {
                scan.groups[group].files++;
                scan.groups[group].bytes+=(uint64_t)st.st_size;
            }
            if (!scan.storage && st.st_size>0) {
                scan.file_fd=open(path,O_RDONLY|O_CLOEXEC|O_NONBLOCK|O_NOFOLLOW);
                if (scan.file_fd<0) scan_error(path,errno);
                else {
                    if (scan.deep) scan.file_left=(uint64_t)st.st_size;
                    else if (quick_auto_full(path,e->d_name,(uint64_t)st.st_size)) {
                        scan.file_left=(uint64_t)st.st_size;
                        scan.quick_full_pending=true;
                    } else if ((uint64_t)st.st_size<=QUICK_SAMPLE_BYTES) {
                        /* A small regular file fits completely even in the
                         * generic sample path. Record the successful full read. */
                        scan.file_left=(uint64_t)st.st_size;
                        scan.quick_full_pending=true;
                    } else {
                        scan.file_left=QUICK_SAMPLE_BYTES;
                        scan.quick_full_pending=false;
                        ++scan.quick_sample_files;
                    }
                    snprintf(scan.reading,sizeof(scan.reading),"%s",path);
                }
            }
        } else ++scan.skipped;
        if (SDL_GetTicks()-started >= 28 && n>=8) break;
    }
    needs_redraw=true;
}
static int compare_groups(const void *aa,const void *bb) {
    const TopGroup *a=aa,*b=bb;
    return a->bytes>b->bytes?-1:a->bytes<b->bytes?1:0;
}

/* Speed read: a read-only O_DIRECT stream from the card's partition. */
static void speed_cleanup(void) {
    if (speedtest.fd>=0) {close(speedtest.fd);speedtest.fd=-1;}
    if (speedtest.file[0]) {
        if (unlink(speedtest.file)<0 && errno!=ENOENT)
            LOG("temporary test file removal FAILED: %s: %s",speedtest.file,strerror(errno));
        speedtest.file[0]=0;
    }
    free(speedtest.data);speedtest.data=NULL;
    speedtest.stage=SPEED_NONE;
}
static void finish_speed(const char *message) {
    snprintf(speedtest.message,sizeof(speedtest.message),"%s",message);
    speed_cleanup();
    job=JOB_NONE;page=PAGE_SPEED_RESULT;needs_redraw=true;
    LOG("speed test finished: %s read=%.2f write=%.2f verified=%d",
        message,speedtest.read_mbps,speedtest.write_mbps,speedtest.verified);
}
static bool speed_prepare(void) {
    memset(&speedtest,0,sizeof(speedtest));speedtest.fd=-1;
    if (posix_memalign(&speedtest.data,4096,IO_CHUNK)!=0) {
        set_notice("NO MEMORY","CANNOT ALLOCATE TEST BUFFER");return false;
    }
    return true;
}
static bool begin_speed_read(void) {
    struct stat st;
    uint64_t sectors=0;
    refresh_card();
    if (!card.mounted || !card.device_verified || stat(card.device,&st)<0 || !S_ISBLK(st.st_mode)) {
        set_notice("READ TEST", "BLOCK DEVICE NOT AVAILABLE");return false;
    }
    if (!speed_prepare()) return false;
    speedtest.fd=open(card.device,O_RDONLY|O_CLOEXEC|O_DIRECT|O_NOFOLLOW);
    speedtest.direct=speedtest.fd>=0;
    if (speedtest.fd<0) speedtest.fd=open(card.device,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if (speedtest.fd<0) {finish_speed("CANNOT OPEN BLOCK DEVICE");return false;}
    if (ioctl(speedtest.fd,BLKGETSIZE64,&sectors)<0 || sectors<READ_TEST_BYTES+4u*1024u*1024u) {
        finish_speed("DEVICE SIZE UNAVAILABLE");return false;
    }
    if (lseek(speedtest.fd,4u*1024u*1024u,SEEK_SET)<0) {finish_speed("CANNOT SEEK DEVICE");return false;}
    speedtest.size=READ_TEST_BYTES;
    speedtest.stage=SPEED_RAW_READ;speedtest.started=SDL_GetTicks();
    job=JOB_SPEED_READ;last_job=job;page=PAGE_RUNNING;
    LOG("read test begin: device=%s direct=%d",card.device,speedtest.direct);
    return true;
}
static void fill_test_pattern(uint8_t *buf, bool header) {
    uint32_t x=0x6d765457u;
    size_t i;
    for (i=0;i<IO_CHUNK;i++) {
        x^=x<<13; x^=x>>17; x^=x<<5;
        buf[i]=(uint8_t)(x&255u);
    }
    if (header) memcpy(buf,TEST_SIG,sizeof(TEST_SIG));
}
static bool begin_speed_write(void) {
    struct statvfs vfs;
    struct stat st;
    int attempts;
    refresh_card();
    if (!card.mounted || card.readonly || stat(ROOT_PATH,&st)<0 || !S_ISDIR(st.st_mode)) {
        set_notice("WRITE TEST", "CARD IS NOT WRITABLE");return false;
    }
    if (card.battery>=0 && card.battery<25) {
        set_notice("LOW BATTERY","CHARGE ABOVE 25 PERCENT FIRST");return false;
    }
    if (statvfs(ROOT_PATH,&vfs)<0 || (uint64_t)vfs.f_bavail*vfs.f_frsize < TEST_BYTES+128u*1024u*1024u) {
        set_notice("LOW SPACE","NEED 160 MB FREE FOR SAFE TEST");return false;
    }
    if (!speed_prepare()) return false;
    speedtest.file[0]=0;
    for(attempts=0;attempts<16;attempts++) {
        snprintf(speedtest.file,sizeof(speedtest.file), ROOT_PATH "/" TEST_PREFIX "%ld-%lu-%d.tmp",
                 (long)getpid(),(unsigned long)SDL_GetTicks(),attempts);
        speedtest.fd=open(speedtest.file,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
        if (speedtest.fd>=0) break;
        if (errno!=EEXIST) break;
    }
    if (speedtest.fd<0) {
        LOG("cannot create test file: %s",strerror(errno));
        /* Never unlink a file that this run did not successfully create. */
        speedtest.file[0]=0;
        finish_speed("CANNOT CREATE TEST FILE");return false;
    }
    fill_test_pattern(test_pattern,false);
    fill_test_pattern(test_header,true);
    speedtest.size=TEST_BYTES;speedtest.started=SDL_GetTicks();
    speedtest.stage=SPEED_WRITE;
    job=JOB_SPEED_WRITE;last_job=job;page=PAGE_RUNNING;
    LOG("write test begin: %s",speedtest.file);
    return true;
}
static bool direct_fallback(int fd_kind) {
    /* Only unsupported-direct-IO errors permit retry. A real EIO is never
     * hidden by a buffered second attempt. */
    int e=errno,fd;
    off_t where=(off_t)speedtest.done;
    if (e!=EINVAL && e!=EOPNOTSUPP && e!=ENOSYS && e!=ENOTSUP) return false;
    if (fd_kind==0 && !speedtest.direct) return false;
    if (fd_kind==1 && !speedtest.write_direct) return false;
    close(speedtest.fd);
    fd=open(fd_kind==0?card.device:speedtest.file,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if (fd<0) {speedtest.fd=-1;return false;}
    speedtest.fd=fd;
    if (lseek(fd,(fd_kind==0?(off_t)(4u*1024u*1024u):0)+where,SEEK_SET)<0) return false;
    if (fd_kind==0) speedtest.direct=false;
    else speedtest.write_direct=false;
    LOG("O_DIRECT unsupported on %s - resumed buffered reads",fd_kind==0?"device":"verify file");
    return true;
}
static void advance_speed(void) {
    ssize_t n;
    uint8_t *buf=speedtest.data;
    if (!speedtest.stage) return;
    if (speedtest.stage==SPEED_RAW_READ) {
        n=read(speedtest.fd,buf,IO_CHUNK);
        if (n<0 && direct_fallback(0)) n=read(speedtest.fd,buf,IO_CHUNK);
        if (n<=0) {finish_speed("READ FAILED");return;}
        speedtest.done+=(uint64_t)n;
        if (speedtest.done>=speedtest.size) {
            Uint32 elapsed=SDL_GetTicks()-speedtest.started;
            speedtest.read_mbps=(double)speedtest.done/1000000.0/((elapsed?elapsed:1)/1000.0);
            finish_speed(speedtest.direct?"DIRECT DEVICE READ OK":"BUFFERED READ - MAY BE CACHED");
        }
    } else if (speedtest.stage==SPEED_WRITE) {
        n=write(speedtest.fd,speedtest.done==0?test_header:test_pattern,IO_CHUNK);
        if (n!=IO_CHUNK) {finish_speed("WRITE FAILED - FILE CLEANED");return;}
        speedtest.done+=(uint64_t)n;
        if (speedtest.done>=speedtest.size) {
            if (fsync(speedtest.fd)<0) {finish_speed("FLUSH FAILED - FILE CLEANED");return;}
            speedtest.finished=SDL_GetTicks();
            speedtest.write_mbps=(double)speedtest.done/1000000.0/
                (((speedtest.finished-speedtest.started)?speedtest.finished-speedtest.started:1)/1000.0);
            close(speedtest.fd); speedtest.fd=-1;
            speedtest.fd=open(speedtest.file,O_RDONLY|O_CLOEXEC|O_DIRECT|O_NOFOLLOW);
            speedtest.write_direct=speedtest.fd>=0;
            if (speedtest.fd<0) speedtest.fd=open(speedtest.file,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
            if (speedtest.fd<0) {finish_speed("CANNOT VERIFY WRITTEN DATA");return;}
            speedtest.stage=SPEED_VERIFY;speedtest.done=0;
        }
    } else if (speedtest.stage==SPEED_VERIFY) {
        const uint8_t *expected=speedtest.done==0?test_header:test_pattern;
        n=read(speedtest.fd,buf,IO_CHUNK);
        if (n<0 && direct_fallback(1)) n=read(speedtest.fd,buf,IO_CHUNK);
        if (n!=IO_CHUNK) {finish_speed("VERIFY READ FAILED");return;}
        if (memcmp(buf,expected,IO_CHUNK)!=0) {finish_speed("DATA MISMATCH - CHECK CARD");return;}
        speedtest.done+=(uint64_t)n;
        if (speedtest.done>=speedtest.size) {
            speedtest.verified=true;
            finish_speed(speedtest.write_direct?"WRITE + DIRECT VERIFY OK":"WRITE VERIFIED - READ MAY BE CACHED");
        }
    }
    needs_redraw=true;
}

/* Root-only cleanup candidates: never descend into ROMs, saves or system folders. */
static bool is_mac_metadata_dir(const char *s) {
    return !strcmp(s,".Spotlight-V100") || !strcmp(s,".Trashes") ||
           !strcmp(s,".fseventsd");
}
static bool is_test_filename(const char *s) {
    return !strncmp(s,TEST_PREFIX,strlen(TEST_PREFIX)) &&
           strlen(s)>strlen(TEST_PREFIX) && strstr(s,".tmp")!=NULL;
}
static bool test_has_signature(const char *path) {
    char buf[sizeof(TEST_SIG)];
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    ssize_t n;
    if (fd<0) return false;
    n=read(fd,buf,sizeof(buf));close(fd);
    return n==(ssize_t)sizeof(buf) && memcmp(buf,TEST_SIG,sizeof(buf))==0;
}
static int compare_candidates(const void *a,const void *b) {
    const Candidate *aa=a,*bb=b;
    return strcmp(aa->name,bb->name);
}
static void load_candidates(bool restore) {
    DIR *d;struct dirent *e;
    const char *folder=restore?QDIR:ROOT_PATH;
    candidates_count=0;candidates_selected=0;restore_list=restore;
    refresh_card();
    if (!card.mounted) return;
    if (restore) {
        struct stat qst;
        if (lstat(QDIR,&qst)<0 || !S_ISDIR(qst.st_mode) || qst.st_dev!=card.devno) return;
    }
    d=open_dir_safe(folder,card.devno);
    if (!d) return;
    while ((e=readdir(d))) {
        struct stat st;
        char path[PATH_MAX];
        bool valid;
        if (!safe_filename(e->d_name) || !path_join(path,sizeof(path),folder,e->d_name)) continue;
        if (lstat(path,&st)<0 || (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))) continue;
        valid=restore || (S_ISDIR(st.st_mode) && is_mac_metadata_dir(e->d_name)) ||
              (S_ISREG(st.st_mode) && (!strcmp(e->d_name,".DS_Store") ||
               !strncmp(e->d_name,"._",2) ||
               (is_test_filename(e->d_name) && test_has_signature(path))));
        if (!valid) continue;
        if (candidates_count>=MAX_CANDIDATES) break;
        snprintf(candidates[candidates_count].name,sizeof(candidates[0].name),"%s",e->d_name);
        candidates[candidates_count].bytes=S_ISDIR(st.st_mode)?0:(uint64_t)st.st_size;
        candidates[candidates_count].directory=S_ISDIR(st.st_mode);
        ++candidates_count;
    }
    closedir(d);
    qsort(candidates,(size_t)candidates_count,sizeof(candidates[0]),compare_candidates);
}
static bool move_candidate(bool restore,char *reason,size_t reasonsz) {
    const char *from=restore?QDIR:ROOT_PATH;
    const char *to=restore?ROOT_PATH:QDIR;
    const char *name;
    char src[PATH_MAX],dst[PATH_MAX];
    struct stat st;
    if (candidates_selected<0 || candidates_selected>=candidates_count) return false;
    name=candidates[candidates_selected].name;
    if (!safe_filename(name) || !path_join(src,sizeof(src),from,name) ||
        !path_join(dst,sizeof(dst),to,name)) return false;
    if (lstat(src,&st)<0 ||
        (candidates[candidates_selected].directory?!S_ISDIR(st.st_mode):!S_ISREG(st.st_mode))) {
        snprintf(reason,reasonsz,"SOURCE ITEM CHANGED");return false;
    }
    if (restore) {
        if (lstat(QDIR,&st)<0 || !S_ISDIR(st.st_mode) || st.st_dev!=card.devno) {
            snprintf(reason,reasonsz,"UNSAFE QUARANTINE FOLDER");return false;
        }
    }
    if (!restore) {
        if (is_test_filename(name) && !test_has_signature(src)) {
            snprintf(reason,reasonsz,"TEST FILE SIGNATURE CHANGED");return false;
        }
        if (strcmp(name,".DS_Store") && strncmp(name,"._",2) &&
            !is_test_filename(name) && !is_mac_metadata_dir(name)) {
            snprintf(reason,reasonsz,"NOT AN APPROVED FILE");return false;
        }
        if (mkdir(QDIR,0700)<0 && errno!=EEXIST) {
            snprintf(reason,reasonsz,"CANNOT CREATE QUARANTINE");return false;
        }
        if (lstat(QDIR,&st)<0 || !S_ISDIR(st.st_mode) || st.st_dev!=card.devno) {
            snprintf(reason,reasonsz,"UNSAFE QUARANTINE FOLDER");return false;
        }
    }
    /* rename() could overwrite a destination, so reject an existing name. */
    if (lstat(dst,&st)==0 || errno!=ENOENT) {
        snprintf(reason,reasonsz,"DESTINATION EXISTS");return false;
    }
    if (rename(src,dst)<0) {
        snprintf(reason,reasonsz,"MOVE FAILED: %s",strerror(errno));return false;
    }
    LOG("%s: %s",restore?"restored":"quarantined",name);
    snprintf(reason,reasonsz,restore?"ITEM RESTORED":"ITEM QUARANTINED");
    return true;
}

/* Dosfstools FAT32 peak usage on 32-bit Linux is approximately the larger
 * of two FAT copies or one FAT plus a 4-byte-per-cluster owner map. Unlike
 * the old estimate, the number of FATs does NOT multiply memory by 3: the
 * temporary FAT copy is freed before allocating the owner map. This is an
 * estimate, not a guarantee for every installed fsck.fat implementation. */
static uint64_t available_ram(void) {
    FILE *f=fopen("/proc/meminfo","r");
    char line[160]; unsigned long long kb=0;
    uint64_t avail=0,free_kb=0,cached=0,buffers=0,shmem=0;
    if (!f) return 0;
    while (fgets(line,sizeof(line),f)) {
        if (sscanf(line,"MemAvailable: %llu kB",&kb)==1) avail=kb;
        else if (sscanf(line,"MemFree: %llu kB",&kb)==1) free_kb=kb;
        else if (sscanf(line,"Cached: %llu kB",&kb)==1) cached=kb;
        else if (sscanf(line,"Buffers: %llu kB",&kb)==1) buffers=kb;
        else if (sscanf(line,"Shmem: %llu kB",&kb)==1) shmem=kb;
    }
    fclose(f);
    if (!avail) {
        /* Old kernels may omit MemAvailable. File cache is reclaimable;
         * exclude shmem and reserve another 8 MiB from this fallback. */
        uint64_t cache=cached>shmem?cached-shmem:0;
        avail=free_kb+cache+buffers;
        avail=avail>8192u?avail-8192u:0;
    }
    return avail*1024u;
}
static bool fatcheck_memory_ok(char *reason,size_t len) {
    unsigned char boot[512];
    uint64_t available,fat_bytes,clusters,total,used_sectors,peak,required;
    unsigned bps,fats,spc,reserved;uint32_t spf;
    int fd;
    if (!card.device_verified) {
        snprintf(reason,len,"FAT32 DEVICE NOT VERIFIED");return false;
    }
    fd=open(card.device,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if (fd<0) {snprintf(reason,len,"CANNOT READ FAT BOOT SECTOR");return false;}
    bool valid=pread(fd,boot,sizeof(boot),0)==(ssize_t)sizeof(boot);
    close(fd);
    if (!valid) {snprintf(reason,len,"CANNOT READ FAT BOOT SECTOR");return false;}
    bps=u16le(boot+11);fats=boot[16];spf=u32le(boot+36);
    spc=boot[13];reserved=u16le(boot+14);
    total=u16le(boot+19);if (!total) total=u32le(boot+32);
    used_sectors=(uint64_t)reserved+(uint64_t)fats*spf;
    if ((bps!=512 && bps!=1024 && bps!=2048 && bps!=4096) ||
        !spc || (spc&(spc-1)) || !reserved || !fats || fats>4 || !spf ||
        total<=used_sectors) {
        snprintf(reason,len,"INVALID FAT32 MEMORY PARAMETERS");return false;
    }
    clusters=(total-used_sectors)/spc;
    fat_bytes=(uint64_t)bps*spf;
    if (clusters<65525u || clusters+2u>fat_bytes/4u) {
        snprintf(reason,len,"INVALID FAT32 CLUSTER COUNT");return false;
    }
    /* Add 16 MiB for file/directory bookkeeping and keep 12 MiB spare.
     * Large cards with unusually small clusters still fail this guard. */
    peak=(fat_bytes*2u>fat_bytes+4u*clusters)?fat_bytes*2u:fat_bytes+4u*clusters;
    required=peak+28u*1024u*1024u;
    available=available_ram();
    LOG("FAT check RAM: available=%" PRIu64 " MiB estimated required=%" PRIu64 " MiB clusters=%" PRIu64,
        available/1048576u,(required+1048575u)/1048576u,clusters);
    if (!available) {
        snprintf(reason,len,"RAM STATUS UNAVAILABLE - USE PC");return false;
    }
    if (available<required) {
        snprintf(reason,len,"FSCK NEEDS ~%" PRIu64 " MB RAM; HAVE %" PRIu64 " MB - USE PC",
                 (required+1048575u)/1048576u,available/1048576u);
        return false;
    }
    return true;
}
/* Read-only checker: never invoke a shell, never pass repair switches. */
static const char *fsck_binary(void) {
    static const char *paths[]={
        "/sbin/fsck.fat", "/usr/sbin/fsck.fat", "/bin/fsck.fat",
        "/usr/bin/fsck.fat", "/sbin/fsck.vfat", "/usr/sbin/fsck.vfat",
        "/bin/fsck.vfat", ROOT_PATH "/.tmp_update/bin/fsck.fat",
        ROOT_PATH "/.tmp_update/bin/fsck.vfat"
    };
    size_t i;
    struct stat st;
    for (i=0;i<ARRAY_LEN(paths);i++) {
        if (stat(paths[i],&st)==0 && S_ISREG(st.st_mode) && access(paths[i],X_OK)==0)
            return paths[i];
    }
    return NULL;
}
/* Reap a detached checker if it has finally exited. kill(pid,0) guards
 * against starting a second check while an earlier child is still alive. */
static bool detached_checker_alive(void) {
    int status;
    pid_t reaped;
    if (detached_checker_pid<=0) return false;
    reaped=waitpid(detached_checker_pid,&status,WNOHANG);
    if (reaped==detached_checker_pid) {
        LOG("previous detached fsck process reaped");
        detached_checker_pid=-1;
        return false;
    }
    if (kill(detached_checker_pid,0)==0 || errno==EPERM) return true;
    if (errno==ESRCH) {
        detached_checker_pid=-1;
        return false;
    }
    /* Unknown error: refuse to start rather than risk concurrent fsck. */
    return true;
}
static bool begin_fatcheck(void) {
    const char *binary;
    int pipes[2];
    pid_t pid;
    if (fatcheck.active || detached_checker_alive()) {
        set_notice("CHECK STILL RUNNING","PREVIOUS FAT CHECK IS STILL ACTIVE. ANOTHER CHECK IS BLOCKED.");
        return false;
    }
    refresh_card();
    if (!card.mounted || !card.vfat || !card.fat32 ||
        !card.device_verified) {
        set_notice("CHECK UNAVAILABLE","MOUNTED FAT32 DEVICE NOT VERIFIED");return false;
    }
    char reason[140];
    if (!fatcheck_memory_ok(reason,sizeof(reason))) {
        set_notice("FAT CHECK UNAVAILABLE",reason);
        return false;
    }
    binary=fsck_binary();
    if (!binary) {set_notice("NO FSCK TOOL","FSCK.FAT NOT INSTALLED - USE A COMPUTER");return false;}
    if (pipe(pipes)<0) {set_notice("CHECK ERROR","COULD NOT CREATE OUTPUT PIPE");return false;}
    pid=fork();
    if (pid<0) {close(pipes[0]);close(pipes[1]);set_notice("CHECK ERROR","COULD NOT START CHECKER");return false;}
    if (pid==0) {
        close(pipes[0]);
        if (dup2(pipes[1],STDOUT_FILENO)<0 || dup2(pipes[1],STDERR_FILENO)<0) _exit(127);
        close(pipes[1]);
        /* This command can never perform FAT repairs. */
        execl(binary,binary,"-n",card.device,(char *)NULL);
        _exit(127);
    }
    close(pipes[1]);
    (void)fcntl(pipes[0],F_SETFL,fcntl(pipes[0],F_GETFL,0)|O_NONBLOCK);
    memset(&fatcheck,0,sizeof(fatcheck));
    fatcheck.pid=pid;fatcheck.pipe_fd=pipes[0];fatcheck.active=true;
    job=last_job=JOB_FAT_CHECK;page=PAGE_RUNNING;needs_redraw=true;
    LOG("read-only fsck.fat -n started on %s; pid=%ld",card.device,(long)pid);
    return true;
}
/* Recognize fsck.fat's exact "offset:original/backup" diff-list syntax;
 * this does not interpret arbitrary text as harmless metadata. */
static bool fatcheck_offset_list(const char *start, const char *end,
                                 bool *mount_bit_only) {
    const char *p=start;
    bool saw_entry=false, only_mount_bit=true;
    while (p<end) {
        char *next;
        unsigned long at, original, backup;
        while (p<end && (isspace((unsigned char)*p) || *p==',')) ++p;
        if (p==end) break;
        if (!isdigit((unsigned char)*p)) return false;
        errno=0;
        at=strtoul(p,&next,10);
        if (errno || next==p || next>=end || *next!=':') return false;
        p=next+1;
        if (p>=end || !isxdigit((unsigned char)*p)) return false;
        errno=0;original=strtoul(p,&next,16);
        if (errno || next==p || next>=end || *next!='/') return false;
        p=next+1;
        if (p>=end || !isxdigit((unsigned char)*p)) return false;
        errno=0;backup=strtoul(p,&next,16);
        if (errno || next==p || (next<end &&
            !isspace((unsigned char)*next) && *next!=',')) return false;
        if (original>255 || backup>255 || at!=65 ||
            (original ^ backup)!=1) only_mount_bit=false;
        saw_entry=true;
        p=next;
    }
    if (mount_bit_only) *mount_bit_only=only_mount_bit;
    return saw_entry;
}
/* A matching backup discrepancy is only classified as routine when the
 * COMPLETE difference list is present and every listed change is precisely
 * the mounted dirty bit at byte 65. Anything else remains a finding. */
static void fatcheck_analyze_boot_backup(void) {
    const char *start, *offsets, *end;
    bool only_status=false;
    start=strstr(fatcheck.output,
                 "There are differences between boot sector and its backup.");
    if (!start) return;
    fatcheck.boot_backup_diff=true;
    offsets=strstr(start,"Differences: (offset:original/backup)");
    end=offsets?strstr(offsets,"Not automatically fixing this."):NULL;
    if (!offsets || !end || fatcheck.output_truncated) {
        fatcheck.boot_backup_other=true;
        return;
    }
    offsets+=strlen("Differences: (offset:original/backup)");
    if (!fatcheck_offset_list(offsets,end,&only_status)) {
        fatcheck.boot_backup_other=true;
        return;
    }
    fatcheck.boot_backup_mount_only=only_status;
    fatcheck.boot_backup_other=!only_status;
}
/* Read-only checks on mounted FAT32 often notice an in-use dirty bit.
 * Classify known messages without equating an exit code of 1 to corruption.
 * Unrecognized findings are INCONCLUSIVE, never silently called healthy. */
static void fatcheck_parse_line(void) {
    char *p=fatcheck.line;
    char *end;
    while (*p && isspace((unsigned char)*p)) ++p;
    end=p+strlen(p);
    while (end>p && isspace((unsigned char)end[-1])) *--end=0;
    if (!*p) goto clear;
    if (strcasestr(p,"Dirty bit is set") || strcasestr(p,"removing dirty bit")) {
        fatcheck.saw_active_flag=true;
        goto clear;
    }
    if (!strncmp(p,"fsck.fat ",9) || !strncmp(p,"fsck.vfat ",10) ||
        !strncmp(p,"dosfsck ",8) ||
        !strcmp(p,"Leaving filesystem unchanged.") ||
        !strcmp(p,"Checking for unused clusters.") ||
        !strcmp(p,"Checking free cluster summary.") ||
        !strcmp(p,"Checking we can access the last sector of the filesystem") ||
        !strcmp(p,"Not automatically fixing this.") ||
        !strcmp(p,"There are differences between boot sector and its backup.") ||
        !strncmp(p,"This is mostly harmless. Differences:",34) ||
        (strstr(p," files, ") && strstr(p," clusters"))) goto clear;
    /* Parse difference values exactly at completion; while the checker is
     * active, accept only whole lines matching its known offset-list grammar. */
    if (fatcheck_offset_list(p,p+strlen(p),NULL)) goto clear;
    /* Explicitly reported read and cluster-chain problems are worth flagging,
     * but mounted checks still require offline confirmation. */
    if (strcasestr(p,"cross-linked") || strcasestr(p,"bad cluster") ||
        strcasestr(p,"FATs differ") || strcasestr(p,"unconnected cluster") ||
        strcasestr(p,"read error") || strcasestr(p,"cannot read") ||
        strcasestr(p,"invalid cluster") || strcasestr(p,"file size is") ||
        strcasestr(p,"contains a free cluster")) fatcheck.reported_damage=true;
    else fatcheck.other_messages=true;
clear:
    fatcheck.line_len=0;
    fatcheck.line[0]=0;
}

/* Keep an on-screen scrollback of the actual checker output. Routine mount
 * dirty-bit messages are excluded from the display, as requested. */
#define FAT_UI_COLUMNS 61
#define FAT_UI_MAX_LINES 256
static char fat_ui_lines[FAT_UI_MAX_LINES][FAT_UI_COLUMNS+1];
static int fat_ui_count=0, fat_ui_scroll=0;
/* Hide only the routine byte-65 mount bit within a mixed difference list;
 * preserve all OTHER offsets so the user can examine them offline. */
static bool fatcheck_visible_offsets(const char *src,size_t len,
                                     char *out,size_t cap) {
    char copy[384], *cursor, *tok, *save=NULL;
    size_t used=0;
    if (len>=sizeof(copy) || !fatcheck_offset_list(src,src+len,NULL)) return false;
    memcpy(copy,src,len);copy[len]=0;
    cursor=copy;
    while ((tok=strtok_r(cursor," ,\t\r\n",&save))!=NULL) {
        unsigned long at=0, original=0, backup=0;
        char extra=0;
        size_t token_len;
        cursor=NULL;
        if (sscanf(tok,"%lu:%lx/%lx%c",&at,&original,&backup,&extra)==3 &&
            at==65 && original<=255 && backup<=255 &&
            (original ^ backup)==1) continue;
        token_len=strlen(tok);
        if (used+token_len+(used?2:0)+1>cap) return false;
        if (used) {out[used++]=',';out[used++]=' ';}
        memcpy(out+used,tok,token_len);used+=token_len;
    }
    out[used]=0;
    return true;
}
static void fatcheck_build_ui_lines(void) {
    const char *p=fatcheck.output;
    bool in_mount_backup_block=false,in_diff_offsets=false;
    fat_ui_count=0;
    fat_ui_scroll=0;
    while (*p && fat_ui_count<FAT_UI_MAX_LINES) {
        const char *start=p, *display=start, *trim;
        size_t len=0,trim_len,display_len,pos=0;
        bool drop=false;
        char filtered[384];
        while (start[len] && start[len]!='\n') ++len;
        trim=start;
        while (trim<start+len && isspace((unsigned char)*trim)) ++trim;
        trim_len=(size_t)(start+len-trim);
        while (trim_len && isspace((unsigned char)trim[trim_len-1])) --trim_len;
        if (trim_len==strlen("There are differences between boot sector and its backup.") &&
            !strncmp(trim,"There are differences between boot sector and its backup.",trim_len) &&
            fatcheck.boot_backup_mount_only) in_mount_backup_block=true;
        if (trim_len>=strlen("This is mostly harmless. Differences:") &&
            !strncmp(trim,"This is mostly harmless. Differences:",
                     strlen("This is mostly harmless. Differences:")))
            in_diff_offsets=true;
        {
            const char *d=strcasestr(start,"Dirty bit is set");
            const char *r=strcasestr(start,"Automatically removing dirty bit");
            drop=(d && (size_t)(d-start)<len) ||
                 (r && (size_t)(r-start)<len) || in_mount_backup_block;
        }
        if (fatcheck.boot_backup_other && in_diff_offsets && !drop &&
            fatcheck_visible_offsets(start,len,filtered,sizeof(filtered))) {
            display=filtered;len=strlen(filtered);
        }
        if (trim_len==strlen("Not automatically fixing this.") &&
            !strncmp(trim,"Not automatically fixing this.",trim_len)) {
            in_mount_backup_block=false;
            in_diff_offsets=false;
        }
        display_len=len;
        while (!drop && pos<display_len && fat_ui_count<FAT_UI_MAX_LINES) {
            size_t remain=display_len-pos, take=remain;
            size_t i;
            char row[FAT_UI_COLUMNS+1];
            if (take>FAT_UI_COLUMNS) {
                /* Prefer breaking between words; only split a long unbroken
                 * value (such as a file name) when unavoidable. */
                size_t limit=FAT_UI_COLUMNS;
                take=limit;
                for (i=limit;i>limit/2;i--)
                    if (display[pos+i]==' ' || display[pos+i]=='\t') {
                        take=i;break;
                    }
            }
            while (take && isspace((unsigned char)display[pos+take-1])) --take;
            if (!take) {++pos;continue;}
            for (i=0;i<take;i++) {
                unsigned char c=(unsigned char)display[pos+i];
                row[i]=(c>=32 && c<=126)?(char)c:' ';
            }
            row[take]=0;
            memcpy(fat_ui_lines[fat_ui_count++],row,take+1);
            pos+=take;
            while (pos<display_len && (display[pos]==' ' || display[pos]=='\t')) ++pos;
        }
        p=start;
        while (*p && *p!='\n') ++p;
        if (*p=='\n') ++p;
    }
    if (!fat_ui_count) strcpy(fat_ui_lines[fat_ui_count++],"NO ADDITIONAL CHECKER DETAILS.");
}

/* Poll and drain output without blocking SDL; drain all remaining output after
 * the child exits. Escalate cancellation but never block waiting for D-state IO. */
static void fatcheck_done(void) {
    if (fatcheck.line_len) fatcheck_parse_line();
    fatcheck_analyze_boot_backup();
    if (fatcheck.pipe_fd>=0) {close(fatcheck.pipe_fd);fatcheck.pipe_fd=-1;}
    fatcheck.active=false;fatcheck.complete=true;
    fatcheck.exit_code=!fatcheck.exited?-1:WIFEXITED(fatcheck.wait_status)?
        WEXITSTATUS(fatcheck.wait_status):-1;
    job=JOB_NONE;page=PAGE_FAT_CHECK_RESULT;needs_redraw=true;
    fatcheck_build_ui_lines();
    LOG("fsck.fat -n completed exit=%d cancelled=%d output=%s",
        fatcheck.exit_code,fatcheck.cancelling,fatcheck.output);
}
static void advance_fatcheck(void) {
    char buf[256];
    ssize_t got=-1;int batches=0;
    if (!fatcheck.active) return;
    if (fatcheck.cancelling && !fatcheck.killed && SDL_GetTicks()-fatcheck.cancel_at>=2000) {
        if (fatcheck.pid>0) (void)kill(fatcheck.pid,SIGKILL);
        fatcheck.killed=true;
        LOG("read-only checker SIGKILL requested after cancellation timeout");
    }
    if (!fatcheck.exited && fatcheck.pid>0) {
        pid_t done=waitpid(fatcheck.pid,&fatcheck.wait_status,WNOHANG);
        if (done==fatcheck.pid || (done<0 && errno==ECHILD)) {
            fatcheck.exited=true;fatcheck.pid=-1;
        }
    }
    while (batches++<8 && (got=read(fatcheck.pipe_fd,buf,sizeof(buf)))>0) {
        size_t i;
        for(i=0;i<(size_t)got;i++) {
            char c=buf[i];
            if (c=='\n' || c=='\r') fatcheck_parse_line();
            else if (fatcheck.line_len+1 < sizeof(fatcheck.line)) {
                fatcheck.line[fatcheck.line_len++]=c;
                fatcheck.line[fatcheck.line_len]=0;
            } else fatcheck.other_messages=true;
            if (c=='\r') continue;
            if (c=='\t') c=' ';
            if ((unsigned char)c<32 && c!='\n') c=' ';
            if ((unsigned char)c>126) c=' ';
            if (fatcheck.output_len+1>=sizeof(fatcheck.output)) {
                fatcheck.output_truncated=true;
            } else {
                fatcheck.output[fatcheck.output_len++]=c;
            }
        }
        fatcheck.output[fatcheck.output_len]=0;
    }
    if (got==0 && fatcheck.exited) {fatcheck_done();return;}
    if (got<0 && errno!=EAGAIN && errno!=EINTR && fatcheck.exited) {
        fatcheck_done();return;
    }
    /* A child in uninterruptible block IO can resist SIGKILL. Detach so the
     * UI can exit; the kernel/init will finish cleaning it up when IO returns. */
    if (fatcheck.cancelling && SDL_GetTicks()-fatcheck.cancel_at>=5000) {
        if (fatcheck.pid>0) {
            (void)kill(fatcheck.pid,SIGKILL);
            detached_checker_pid=fatcheck.pid;
            LOG("fsck stuck after cancellation: retain PID %ld and block restart",
                (long)detached_checker_pid);
        }
        fatcheck_done();return;
    }
}
static void cancel_fatcheck(void) {
    if (fatcheck.active && !fatcheck.cancelling) {
        fatcheck.cancelling=true;fatcheck.cancel_at=SDL_GetTicks();
        if (fatcheck.pid>0) (void)kill(fatcheck.pid,SIGTERM);
        LOG("read-only fsck cancellation requested");
    }
}
static void cancel_job(void) {
    if (scan.active) {scan.cancelled=true;stop_scan();}
    if (speedtest.stage) {speedtest.cancelled=true;speed_cleanup();}
    if (fatcheck.active) {cancel_fatcheck();return;}
    job=JOB_NONE;
    LOG("active job cancelled");
}

#define BG0 CUI_BG
#define BG1 CUI_BG
#define WHITE CUI_WHITE
#define MUTED CUI_GREY
#define TEAL CUI_GOOD
#define AMBER CUI_WARN
#define BAD CUI_BAD
static void panel(Canvas *c,int x,int y,int w,int h) {
    cui_rrect(CUI(c),x,y,w,h,12,CUI_PANEL);
}
/* Selectable box inside a panel: accent when selected, as in Tweaks. */
static void item_box(Canvas *c,int x,int y,int w,int h,bool selected) {
    cui_rrect(CUI(c),x,y,w,h,9,selected?CUI_ACCENT:CUI_TRACK);
}
static void draw_background(Canvas *c) {
    cui_rect(CUI(c),0,0,W,H,CUI_BG);
}
static void limited_label(Canvas *c,int x,int y,const char *s,int max_chars,uint32_t color) {
    char out[256];
    size_t n=strlen(s);
    if (max_chars<1) return;
    if (max_chars>(int)sizeof(out)-1) max_chars=(int)sizeof(out)-1;
    /* The larger bitmap font must still fit inside either side panel. */
    if (x<50 && y>415 && max_chars>65) max_chars=65;
    else if (x>425 && max_chars>18) max_chars=18;
    else if (x<215 && x>20 && y>90 && y<414 && max_chars>18) max_chars=18;
    safe_short(s,out,(size_t)max_chars+1);
    if ((int)n>max_chars && max_chars>3) {
        out[max_chars-3]='.';out[max_chars-2]='.';out[max_chars-1]='.';out[max_chars]=0;
    }
    if (cui_font(UI_SMALL)) {
        /* Exo 2 is not monospaced: keep the bitmap font's width limit. */
        char fit[256];
        cui_fit(out,UI_SMALL,max_chars*SMALL_ADVANCE+10,fit,sizeof(fit));
        label(c,x,y,fit,1,color);
        return;
    }
    label(c,x,y,out,1,color);
}
static void small_pair(Canvas *c,int x,int y,const char *a,const char *b) {
    const unsigned char *p=(const unsigned char *)(b?b:"");
    char line[19]={0};
    int len=0,row=0;
    label(c,x,y,a,1,MUTED);
    /* Normal values fit 18 columns; longer values wrap into the second row. */
    while (*p && row<2) {
        const unsigned char *word;
        size_t size;
        if (isspace(*p)) {p++;continue;}
        word=p;
        while (*p && !isspace(*p)) ++p;
        size=(size_t)(p-word);
        while (size && row<2) {
            size_t take=size;
            if (len && (size_t)len+1+size<=18) line[len++]=' ';
            else if (len) {
                line[len]=0;label(c,x,y+18+row*17,line,1,WHITE);
                row++;len=0;continue;
            }
            if (take>18-(size_t)len) take=18-(size_t)len;
            memcpy(line+len,word,take);len+=(int)take;line[len]=0;
            word+=take;size-=take;
            if (size) {label(c,x,y+18+row*17,line,1,WHITE);row++;len=0;line[0]=0;}
        }
    }
    if (len && row<2) label(c,x,y+18+row*17,line,1,WHITE);
}
static void progress(Canvas *c,int x,int y,int w,int pct) {
    cui_progress(CUI(c),x,y,w,9,pct/100.0f);
}
static void header(Canvas *c) {
    draw_background(c);
    cui_header(CUI(c),"sd","SD Card Doctor",
               card.mounted?(card.readonly?"Read only":"Mounted RW"):"No card",card.battery);
    cui_text(CUI(c),60+cui_text_width("SD Card Doctor",25)+10,31,"v0.6.3",14,CUI_DIM,CUI_LEFT);
}
static void footer(Canvas *c,const char *hint) {
    char fit[256];
    int x=20;
    cui_fit(hint?hint:"",15,600,fit,sizeof(fit));
    cui_text(CUI(c),20,419,fit,15,CUI_GREY,CUI_LEFT);
    if (page==PAGE_RUNNING) {
        cui_hint(CUI(c),x,456,"B",job==JOB_FAT_CHECK?"CANCEL CHECK":
                 (job==JOB_SPEED_READ||job==JOB_SPEED_WRITE)?"CANCEL TEST":"CANCEL SCAN",22);
    } else if (page==PAGE_FAT_CHECK_RESULT) {
        x+=cui_hint(CUI(c),x,456,"A","SAVE REPORT",22);
        cui_hint(CUI(c),x,456,"B","BACK",22);
    } else {
        x+=cui_hint(CUI(c),x,456,"A","SELECT",22);
        cui_hint(CUI(c),x,456,"B",page==PAGE_MAIN?"EXIT":"BACK",22);
    }
    /* SELECT+START leaves from any screen */
    x=620-(cui_text_width("SELECT+START",14)+18)-7-cui_text_width("EXIT",22);
    cui_hint(CUI(c),x,456,"SELECT+START","EXIT",22);
}
static void panel_heading(Canvas *c,int x,const char *s) {
    char fit[64];
    cui_fit(s,15,160,fit,sizeof(fit));
    cui_text(CUI(c),x+16,128,fit,15,CUI_GREY,CUI_LEFT);
}
/* Recognizable microSD card with a clean beveled upper-right corner.
 * No decorative moving lights when idle. */
static void sd_silhouette(Canvas *c,int x,int y,int w,int h,int cut,uint32_t color) {
    int row;
    for (row=0;row<h;row++) {
        int right=w;
        int left=0;
        if (row<cut) right=w-cut+row;
        if (row==h-1 || row==h-2) {left=2;right-=2;}
        rect(c,x+left,y+row,right-left,1,color);
    }
}
static void sd_card(Canvas *c,int x,int y) {
    int i;
    sd_silhouette(c,x,y,140,180,27,rgb(86,86,92));
    sd_silhouette(c,x+3,y+3,134,174,24,CUI_PANEL);
    rect(c,x+14,y+39,112,2,rgb(86,86,92));
    for (i=0;i<7;i++) cui_rrect(CUI(c),x+17+i*15,y+12,10,18,2,rgb(120,120,126));
    centered(c,x+70,y+63,"MICRO SD",2,CUI_WHITE);
    centered(c,x+70,y+102,card.fat32?"FAT32":card.vfat?"VFAT":"STORAGE",1,CUI_GREY);
    cui_rrect(CUI(c),x+20,y+126,100,30,15,CUI_TRACK);
    centered(c,x+70,y+134,card.mounted?"CONNECTED":"NO CARD",1,card.mounted?TEAL:BAD);
    if (scan.active || speedtest.stage || fatcheck.active) {
        /* Activity dots only during an actual operation, with live redraws. */
        for (i=0;i<4;i++) cui_disc(CUI(c),x+40+i*20,y+168,3,
             (unsigned)i==((SDL_GetTicks()/350u)%4u)?CUI_ACCENT:CUI_DIM);
    }
}

static void left_card_info(Canvas *c) {
    char buf[80],capacity[32],free_sz[32];
    panel(c,18,103,190,302);
    panel_heading(c,18,"CARD INFORMATION");
    small_pair(c,32,165,"VOLUME LABEL",card.volume_label);
    small_pair(c,32,209,"FILESYSTEM",card.fat32?"FAT32":card.vfat?"VFAT (UNVERIFIED)":card.fstype);
    small_pair(c,32,252,"MOUNT",card.mounted?(card.readonly?"READ ONLY":"READ AND WRITE"):"NOT MOUNTED");
    human_size(card.total,capacity,sizeof(capacity));
    human_size(card.available,free_sz,sizeof(free_sz));
    snprintf(buf,sizeof(buf),"%s / %s",free_sz,capacity);
    small_pair(c,32,295,"FREE / TOTAL",card.mounted?buf:"UNAVAILABLE");
    small_pair(c,32,337,"BLOCK DEVICE",card.device);
    if (card.battery>=0) {
        snprintf(buf,sizeof(buf),"BATTERY %d%%",card.battery);
        limited_label(c,32,387,buf,21,card.battery<25?AMBER:MUTED);
    } else limited_label(c,32,387,"BATTERY UNKNOWN",21,MUTED);
}
/* Line icons of the main list, 28 px, centred on (x, y). */
static void tool_icon(Canvas *c,int i,float x,float y,uint32_t col) {
    CuiCanvas *cc=CUI(c);
    const float w=2.5f;
    switch (i) {
    case 0: /* quick sample: bolt */
        cui_line(cc,x+3,y-12,x-6,y+1,w,col);cui_line(cc,x-6,y+1,x+1,y+1,w,col);
        cui_line(cc,x+1,y+1,x-3,y+12,w,col);cui_line(cc,x-3,y+12,x+7,y-2,w,col);
        cui_line(cc,x+7,y-2,x,y-2,w,col);cui_line(cc,x,y-2,x+3,y-12,w,col);break;
    case 1: /* deep scan: magnifier */
        cui_ring(cc,x-2,y-2,8,w,col);cui_line(cc,x+4,y+4,x+11,y+11,w+0.5f,col);break;
    case 2: { /* speed test: gauge */
        int k;
        for (k=0;k<=12;k++) {
            float a0=3.14159f*(1+k/12.0f),a1=3.14159f*(1+(k+1)/12.0f);
            if (k<12) cui_line(cc,x+12*cosf(a0),y+5+12*sinf(a0),x+12*cosf(a1),y+5+12*sinf(a1),w,col);
        }
        cui_line(cc,x,y+5,x+6,y-3,w,col);cui_disc(cc,x,y+5,2.2f,col);break;
    }
    case 3: /* storage: folder */
        cui_line(cc,x-12,y-8,x-5,y-8,w,col);cui_line(cc,x-5,y-8,x-2,y-5,w,col);
        cui_line(cc,x-2,y-5,x+12,y-5,w,col);cui_line(cc,x+12,y-5,x+12,y+10,w,col);
        cui_line(cc,x+12,y+10,x-12,y+10,w,col);cui_line(cc,x-12,y+10,x-12,y-8,w,col);break;
    default: /* safe repair: shield with a tick */
        cui_line(cc,x,y-12,x+10,y-8,w,col);cui_line(cc,x+10,y-8,x+10,y,w,col);
        cui_line(cc,x+10,y,x,y+12,w,col);cui_line(cc,x,y+12,x-10,y,w,col);
        cui_line(cc,x-10,y,x-10,y-8,w,col);cui_line(cc,x-10,y-8,x,y-12,w,col);
        cui_line(cc,x-4,y,x-1,y+3,w,col);cui_line(cc,x-1,y+3,x+5,y-4,w,col);break;
    }
}
/* Main page as in Tweaks: the card on one row, then the tools as list rows
 * with an icon, a short description on the right and a chevron. */
static void main_list(Canvas *c) {
    static const char *items[]={"Quick sample","Deep scan","Speed test","Storage","Safe repair"};
    static const char *descs[]={"samples key files","reads every file","read · write","folder sizes","asks before changing"};
    CuiCanvas *cc=CUI(c);
    char line[96],capacity[32],free_sz[32];
    int i;
    /* card row */
    cui_line(cc,26,77,39,77,2.5f,CUI_WHITE);cui_line(cc,39,77,45,83,2.5f,CUI_WHITE);
    cui_line(cc,45,83,45,103,2.5f,CUI_WHITE);cui_line(cc,45,103,26,103,2.5f,CUI_WHITE);
    cui_line(cc,26,103,26,77,2.5f,CUI_WHITE);
    for (i=0;i<3;i++) cui_line(cc,31+i*4,82,31+i*4,87,2,CUI_WHITE);
    if (card.mounted) {
        human_size(card.total,capacity,sizeof(capacity));
        human_size(card.available,free_sz,sizeof(free_sz));
        snprintf(line,sizeof(line),"%s · %s",card.volume_label[0]?card.volume_label:"SD card",capacity);
        cui_text(cc,66,80,line,22,CUI_WHITE,CUI_LEFT);
        snprintf(line,sizeof(line),"%s · %s free · %s",card.fat32?"FAT32":card.vfat?"VFAT (unverified)":card.fstype,free_sz,card.device);
        cui_text(cc,66,103,line,15,CUI_GREY,CUI_LEFT);
        if (card.total) {
            int pct=(int)(card.used*100/card.total);
            cui_progress(cc,430,83,190,10,pct/100.0f);
            snprintf(line,sizeof(line),"%d%% used",pct);
            cui_text(cc,620,107,line,15,CUI_GREY,CUI_RIGHT);
        }
    } else {
        cui_text(cc,66,90,"No card found",22,BAD,CUI_LEFT);
    }
    /* tools */
    for (i=0;i<5;i++) {
        int y=120+i*56;
        bool sel=i==main_selected;
        if (sel) cui_rrect(cc,10,y+3,W-20,50,10,CUI_ACCENT);
        tool_icon(c,i,35,y+28,sel?CUI_WHITE:rgb(220,220,224));
        cui_text(cc,66,y+28,items[i],25,CUI_WHITE,CUI_LEFT);
        cui_text(cc,594,y+28,descs[i],17,sel?CUI_WHITE:CUI_GREY,CUI_RIGHT);
        cui_chevron(cc,612,y+28,1,sel?CUI_WHITE:CUI_DIM);
    }
}
static void info_row(Canvas *c,int y,const char *title,const char *detail,bool selected) {
    item_box(c,445,y,164,45,selected);
    limited_label(c,455,y+8,title,19,WHITE);
    if (detail && *detail) limited_label(c,455,y+25,detail,19,selected?rgb(235,205,214):MUTED);
}
static void result_stats(Canvas *c) {
    char tmp[64],sz[32];
    panel(c,18,103,190,302);
    panel_heading(c,18,"SCAN RESULTS");
    snprintf(tmp,sizeof(tmp),"%" PRIu64 " FILES",scan.files);
    small_pair(c,32,166,scan.limited?"SAMPLED":"CHECKED",tmp);
    human_size(scan.bytes,sz,sizeof(sz));
    small_pair(c,32,220,scan.storage?"TOTAL SIZE":"DATA READ",sz);
    snprintf(tmp,sizeof(tmp),"%" PRIu64 " ERRORS",scan.bad);
    small_pair(c,32,274,"STATUS",tmp);
    if (scan.limited) {
        snprintf(tmp,sizeof(tmp),"%u CORE MISSING",scan.core_missing);
        small_pair(c,32,328,"SYSTEM",tmp);
    } else {
        snprintf(tmp,sizeof(tmp),"%" PRIu64 " SKIPPED",scan.skipped);
        small_pair(c,32,328,"OTHER",tmp);
    }
}
static void result_details(Canvas *c) {
    char tmp[96];
    panel(c,432,103,190,302);
    panel_heading(c,432,"FINDINGS");
    centered(c,527,171,scan.bad?"ISSUES FOUND":scan.limited?"SAMPLE DONE":"SCAN FINISHED",1,scan.bad?AMBER:TEAL);
    if (scan.bad) {
        compact_name(scan.last_error,tmp,sizeof(tmp));
        limited_label(c,447,220,"FIRST ERROR FILE",19,MUTED);
        limited_label(c,447,245,tmp,19,WHITE);
    } else {
        limited_label(c,447,231,"NO READ ERRORS",19,WHITE);
        limited_label(c,447,250,"OBSERVED",19,WHITE);
    }
    if (scan.limited) {
        snprintf(tmp,sizeof(tmp),"%u SYSTEM FOLDERS",scan.system_dirs_found);
        limited_label(c,447,303,tmp,19,TEAL);
        snprintf(tmp,sizeof(tmp),"%" PRIu64 " FULL FILES",scan.quick_full_files);
        limited_label(c,447,326,tmp,19,MUTED);
        limited_label(c,447,351,scan.core_missing?"CHECK INSTALL":"PARTIAL READ TEST",19,scan.core_missing?AMBER:MUTED);
    } else {
        limited_label(c,447,320,"NOT A HEALTH",18,MUTED);
        limited_label(c,447,340,"CERTIFICATION",18,MUTED);
    }
}
static void storage_details(Canvas *c) {
    int i;
    char sz[32],shortname[95];
    panel(c,432,103,190,302);
    panel_heading(c,432,"LARGEST FOLDERS");
    for(i=0;i<6;i++) {
        int ix=storage_scroll+i;
        if (ix>=scan.group_count) break;
        human_size(scan.groups[ix].bytes,sz,sizeof(sz));
        compact_name(scan.groups[ix].name,shortname,sizeof(shortname));
        limited_label(c,446,160+i*39,shortname,18,WHITE);
        limited_label(c,446,176+i*39,sz,18,MUTED);
    }
}
static void speed_side(Canvas *c) {
    int i;
    panel(c,432,103,190,302);
    panel_heading(c,432,"SPEED TOOLS");
    for(i=0;i<2;i++) {
        info_row(c,164+i*73,i==0?"READ TEST":"WRITE TEST",
                 i==0?"64 MB RAW DEVICE":"32 MB - VERIFIED",speed_selected==i);
    }
    limited_label(c,447,337,"WRITE NEEDS",19,AMBER);
    limited_label(c,447,355,"CONFIRMATION",19,AMBER);
}
static void speed_report(Canvas *c) {
    char tmp[96];
    panel(c,432,103,190,302);
    panel_heading(c,432,"MEASUREMENTS");
    if (last_job==JOB_SPEED_READ) {
        snprintf(tmp,sizeof(tmp),"%.1f MB/S",speedtest.read_mbps);
        small_pair(c,447,174,"SEQUENTIAL READ",speedtest.read_mbps>0?tmp:"UNAVAILABLE");
        small_pair(c,447,257,"DATA PATH",speedtest.direct?"DIRECT IO":"BUFFERED / CACHED");
    } else {
        snprintf(tmp,sizeof(tmp),"%.1f MB/S",speedtest.write_mbps);
        small_pair(c,447,174,"WRITE + FLUSH",speedtest.write_mbps>0?tmp:"UNAVAILABLE");
        small_pair(c,447,257,"VERIFICATION",speedtest.verified?"PASSED":"NOT VERIFIED");
    }
}
static void repair_side(Canvas *c) {
    int i;
    static const char *names[]={"CHECK FAT STATUS","LIVE FAT CHECK","CARD IDENTITY","REVIEW CLEANUP","RESTORE FILES","FAT32 REPAIR"};
    static const char *descs[]={"READ ONLY FLAGS","FSCK -N ONLY","SYSFS HARDWARE ID","REVERSIBLE MOVES","QUARANTINED FILES","OFFLINE REQUIRED"};
    panel(c,432,103,190,302);panel_heading(c,432,"SAFE REPAIR");
    for(i=0;i<6;i++) {
        int y=158+i*40;
        item_box(c,445,y,164,36,repair_selected==i);
        limited_label(c,455,y+4,names[i],17,WHITE);
        limited_label(c,455,y+20,descs[i],17,repair_selected==i?rgb(235,205,214):MUTED);
    }
}
static void info_panel_text(Canvas *c,const char *body,const char *controls,uint32_t color);
static void fat_status_side(Canvas *c) {
    char number[32];
    panel(c,432,103,190,302);panel_heading(c,432,"FAT32 STATUS");
    if (!fatstatus.readable) {
        info_panel_text(c,fatstatus.detail,"B: BACK",AMBER);
        return;
    }
    /* The live Linux boot-status flag is intentionally hidden. It can be set
     * simply because this filesystem is mounted, which confuses users. */
    small_pair(c,447,158,"FAT CLEAN FLAG",fatstatus.clean?"SET (STORED)":"CLEAR (STORED)");
    small_pair(c,447,209,"FAT ERROR FLAG",fatstatus.no_hard_error?"NO ERROR FLAG":"ERROR FLAGGED");
    small_pair(c,447,260,"FSINFO SIGNATURE",fatstatus.fsinfo_valid?"VALID":"UNAVAILABLE");
    if (fatstatus.fsinfo_valid && fatstatus.free_clusters!=UINT32_MAX)
        snprintf(number,sizeof(number),"%" PRIu32,fatstatus.free_clusters);
    else snprintf(number,sizeof(number),"UNKNOWN");
    small_pair(c,447,311,"FREE CLUSTERS",number);
    label(c,447,367,"FAT[1] IS STORED",1,MUTED);
    label(c,447,386,"NOT LIVE OS STATE",1,MUTED);
}
static void card_id_side(Canvas *c) {
    panel(c,432,103,190,302);panel_heading(c,432,"CARD IDENTITY");
    small_pair(c,447,158,"MMC NAME",card_name);
    {
        char heading[64],*end;
        unsigned long mid;
        errno=0;mid=strtoul(card_manfid,&end,0);
        if (!errno && end!=card_manfid && !*end && mid<=255u)
            snprintf(heading,sizeof(heading),"MANUF ID 0X%02lX",mid);
        else snprintf(heading,sizeof(heading),"MANUFACTURER MID");
        small_pair(c,447,213,heading,manufacturer_name(card_manfid));
    }
    small_pair(c,447,268,"SERIAL",card_serial);
    small_pair(c,447,323,"MFG DATE",card_date);
}
static void diagnostics_side(Canvas *c) {
    char row[96];int i;
    panel(c,432,103,190,302);panel_heading(c,432,"FAILED FILES");
    if (!scan.failures_count) limited_label(c,446,177,"NONE CAPTURED",18,TEAL);
    for (i=0;i<5;i++) {
        int ix=diagnostics_scroll+i;
        if (ix>=(int)scan.failures_count) break;
        compact_name(scan.failures[ix].path,row,sizeof(row));
        limited_label(c,447,150+40*i,row,18,WHITE);
        snprintf(row,sizeof(row),"ERROR %d",scan.failures[ix].error);
        limited_label(c,447,167+40*i,row,18,AMBER);
    }
    if (scan.kernel_checked) snprintf(row,sizeof(row),"KERNEL ALERTS %u",scan.kernel_errors);
    else snprintf(row,sizeof(row),"KERNEL LOG N/A");
    limited_label(c,447,350,row,19,scan.kernel_errors?AMBER:MUTED);
    if (scan.kernel_checked)
        snprintf(row,sizeof(row),"UNCLEAN SHUTDOWN %u",scan.kernel_unclean);
    else snprintf(row,sizeof(row),"UNCLEAN SHUTDOWN N/A");
    limited_label(c,447,370,row,19,scan.kernel_unclean?MUTED:TEAL);
    snprintf(row,sizeof(row),"FSCK LEFTOVERS %u",scan.repair_leftovers);
    limited_label(c,447,390,row,19,scan.repair_leftovers?AMBER:MUTED);
}
static void candidate_side(Canvas *c) {
    int i,start=candidates_selected-2;
    char name[80],sz[28];
    if (start<0) start=0;
    if (start>candidates_count-5) start=candidates_count-5;
    if (start<0) start=0;
    panel(c,432,103,190,302);
    panel_heading(c,432,restore_list?"RESTORE FILES":"CLEANUP FILES");
    if (!candidates_count) {
        limited_label(c,446,195,"NO ELIGIBLE FILES",20,TEAL);
        limited_label(c,446,222,"FOUND IN FOLDER",20,MUTED);
        return;
    }
    for(i=0;i<5;i++) {
        int ix=start+i;
        if (ix>=candidates_count) break;
        compact_name(candidates[ix].name,name,sizeof(name));
        if (candidates[ix].directory) snprintf(sz,sizeof(sz),"FOLDER - REVIEW");
        else human_size(candidates[ix].bytes,sz,sizeof(sz));
        info_row(c,157+i*47,name,sz,ix==candidates_selected);
    }
}
/* Information panels are 190 pixels wide, with 18 visible 9-pixel glyphs
 * per row. Store every wrapped line, not truncated text. Only an individual
 * word longer than the full panel width must be split across rows. */
#define INFO_COLS 18
#define INFO_MAX_LINES 64
#define INFO_VISIBLE_ROWS 11
#define INFO_LINE_Y 165
#define INFO_LINE_STEP 20
static int info_wrap(const char *message, char lines[INFO_MAX_LINES][INFO_COLS+1], int count) {
    const unsigned char *p=(const unsigned char *)(message?message:"");
    char line[INFO_COLS+1]={0};
    int len=0;
    while (*p && count<INFO_MAX_LINES) {
        const unsigned char *word;
        size_t size;
        if (*p=='\n') {
            memcpy(lines[count],line,(size_t)len+1);
            ++count;len=0;line[0]=0;++p;continue;
        }
        if (isspace(*p)) {++p;continue;}
        word=p;
        while (*p && !isspace(*p)) ++p;
        size=(size_t)(p-word);
        while (size && count<INFO_MAX_LINES) {
            size_t take=size;
            if (len && (size_t)(len+1)+size<=INFO_COLS) {
                line[len++]=' ';
            } else if (len) {
                memcpy(lines[count],line,(size_t)len+1);
                ++count;len=0;line[0]=0;
                continue;
            }
            if (take>INFO_COLS-(size_t)len) take=INFO_COLS-(size_t)len;
            memcpy(line+len,word,take);
            len+=(int)take;line[len]=0;
            word+=take;size-=take;
            if (size) {
                memcpy(lines[count],line,(size_t)len+1);
                ++count;len=0;line[0]=0;
            }
        }
    }
    if (len && count<INFO_MAX_LINES) {
        memcpy(lines[count],line,(size_t)len+1);
        ++count;
    }
    return count;
}
static void info_panel_text(Canvas *c,const char *body,const char *controls,uint32_t color) {
    char lines[INFO_MAX_LINES][INFO_COLS+1]={{0}};
    int n=info_wrap(body,lines,0),i,max_scroll;
    if (controls && *controls) {
        /* Exactly two blank text rows between the message and button hints. */
        if (n<INFO_MAX_LINES) lines[n++][0]=0;
        if (n<INFO_MAX_LINES) lines[n++][0]=0;
        n=info_wrap(controls,lines,n);
    }
    if (info_page!=page) {info_page=page;info_scroll=0;}
    info_row_count=n;
    max_scroll=n>INFO_VISIBLE_ROWS?n-INFO_VISIBLE_ROWS:0;
    if (info_scroll>max_scroll) info_scroll=max_scroll;
    if (info_scroll<0) info_scroll=0;
    for (i=0;i<INFO_VISIBLE_ROWS && i+info_scroll<n;i++)
        label(c,446,INFO_LINE_Y+i*INFO_LINE_STEP,lines[i+info_scroll],1,
              controls && lines[i+info_scroll][0] && i+info_scroll>=n-2 ? TEAL : color);
    if (max_scroll) {
        /* This hint is a separate, always-visible line, not cut-off text. */
        label(c,446,388,"UP/DOWN: SCROLL",1,MUTED);
    }
}
static void modal_side(Canvas *c,const char *title,const char *desc,const char *controls) {
    panel(c,432,103,190,302);
    panel_heading(c,432,title);
    info_panel_text(c,desc,controls,WHITE);
}
static void live_fat_check_warning(Canvas *c) {
    modal_side(c,"LIVE FAT CHECK",
        "READ ONLY FSCK.FAT -N. THIS DOES NOT REPAIR FILES. CHECKING A MOUNTED CARD MAY SHOW FALSE ERRORS AND MAY TAKE TIME.",
        "A: START B: CANCEL");
}
static void fatcheck_result_screen(Canvas *c) {
    int i;
    const char *line1,*line2;
    uint32_t color=TEAL;
    panel(c,18,103,604,302);
    panel_heading(c,18,"FAT CHECK REPORT");
    if (fatcheck.cancelling) {
        line1="CHECK CANCELLED";
        line2="NO REPAIR WRITES WERE MADE.";
    } else if (fatcheck.exit_code==2) {
        line1="CHECK COULD NOT COMPLETE";
        line2="CHECK FILESYSTEM OFFLINE ON A COMPUTER.";
        color=AMBER;
    } else if (fatcheck.exit_code!=0 && fatcheck.exit_code!=1) {
        line1="CHECK UNAVAILABLE OR INCOMPLETE";
        line2="SEE CHECKER DETAILS BELOW.";
        color=AMBER;
    } else if (fatcheck.reported_damage) {
        line1="CHECKER REPORTED ERRORS";
        line2="VERIFY WITH THE CARD UNMOUNTED ON A COMPUTER.";
        color=AMBER;
    } else if (fatcheck.boot_backup_other) {
        line1="BOOT SECTOR BACKUP DIFFERS";
        line2="MAY BE HARMLESS; CONFIRM WHILE UNMOUNTED.";
        color=AMBER;
    } else if (!fatcheck.other_messages && !fatcheck.output_truncated &&
               (fatcheck.saw_active_flag || fatcheck.boot_backup_mount_only)) {
        line1="ONLY MOUNT-STATUS DIFFERENCES FOUND";
        line2="NO OTHER ISSUES REPORTED; CHECK OFFLINE TO CONFIRM.";
    } else if (fatcheck.exit_code==0 && !fatcheck.other_messages &&
               !fatcheck.output_truncated) {
        line1="NO ERRORS REPORTED";
        line2="A MOUNTED-CARD CHECK IS NOT A FINAL HEALTH TEST.";
    } else {
        line1="MOUNTED-CARD RESULT IS INCONCLUSIVE";
        line2="SEE DETAILS; CHECK OFFLINE FOR CERTAINTY.";
        color=AMBER;
    }
    label(c,35,157,line1,1,color);
    label(c,35,181,line2,1,WHITE);
    rect(c,34,209,571,1,CUI_TRACK);
    label(c,35,218,"A: SAVE REPORT  UP/DOWN: SCROLL  B: BACK",1,TEAL);
    for(i=0;i<8;i++) {
        int line=i+fat_ui_scroll;
        if (line>=fat_ui_count) break;
        label(c,35,243+i*18,fat_ui_lines[line],1,WHITE);
    }
    if (fatcheck.output_truncated)
        label(c,35,386,"OUTPUT CAP REACHED - SOME LINES OMITTED",1,AMBER);
    else if (fat_ui_count>8)
        label(c,35,386,"MORE OUTPUT: UP/DOWN",1,MUTED);
}

/* A running check, as in the mockup: its name and progress big at the top,
 * the file being read, three numbers in boxes and a note on what it does. */
static void stat_box(Canvas *c,int x,const char *title,const char *value,uint32_t vcol,const char *sub) {
    CuiCanvas *cc=CUI(c);
    char fit[64];
    cui_rrect(cc,x,214,190,112,12,CUI_PANEL);
    cui_text(cc,x+16,236,title,13,CUI_GREY,CUI_LEFT);
    cui_fit(value,26,160,fit,sizeof(fit));
    cui_text(cc,x+16,270,fit,26,vcol,CUI_LEFT);
    cui_fit(sub,14,160,fit,sizeof(fit));
    cui_text(cc,x+16,303,fit,14,CUI_GREY,CUI_LEFT);
}
static void running_screen(Canvas *c) {
    CuiCanvas *cc=CUI(c);
    char name[64],pct_s[16],line[160],fit[160],v1[32],s1[48],v2[32],s2[48],v3[32],s3[48];
    float frac=-1;
    Uint32 began=scan.began;
    uint64_t used=card.total>card.available?card.total-card.available:0;
    if (job==JOB_FAT_CHECK) {
        snprintf(name,sizeof(name),"FAT check");
        snprintf(line,sizeof(line),"Read-only check of %s",card.device);
        snprintf(v1,sizeof(v1),"%s",card.fat32?"FAT32":"FAT");snprintf(s1,sizeof(s1),"%s",card.device);
        snprintf(v2,sizeof(v2),"Read only");snprintf(s2,sizeof(s2),"fsck.fat -n");
        snprintf(v3,sizeof(v3),"%s",fatcheck.cancelling?"Cancelling":"Checking");snprintf(s3,sizeof(s3),"nothing is changed");
        began=0;
    } else if (job==JOB_QUICK || job==JOB_DEEP || job==JOB_STORAGE) {
        char sz[32];
        const char *rel=scan.reading;
        snprintf(name,sizeof(name),"%s",job==JOB_QUICK?"Quick sample":job==JOB_DEEP?"Deep scan":"Storage");
        /* how much of the used space is read (or counted) so far */
        if (job!=JOB_QUICK && used) {
            frac=(float)((double)scan.bytes/(double)used);
            if (frac>0.99f) frac=0.99f;
        }
        /* the file as the user knows it: Roms/GBA/Golden Sun (USA).zip */
        if (!strncmp(rel,"/mnt/SDCARD/",12)) rel+=12;
        snprintf(line,sizeof(line),"%s %s",job==JOB_STORAGE?"Counting":"Reading",*rel?rel:"the folders...");
        snprintf(v1,sizeof(v1),"%" PRIu64,scan.files);
        snprintf(s1,sizeof(s1),"in %" PRIu64 " folders",scan.dirs);
        human_size(scan.bytes,sz,sizeof(sz));
        if (job==JOB_STORAGE) {
            snprintf(v2,sizeof(v2),"%s",sz);snprintf(s2,sizeof(s2),"counted so far");
        } else {
            snprintf(v2,sizeof(v2),"%" PRIu64,scan.bad);
            snprintf(s2,sizeof(s2),"%s",scan.bad?"see the report":"none so far");
        }
    } else {
        double p=speedtest.size?(double)speedtest.done/(double)speedtest.size:0;
        snprintf(name,sizeof(name),"Speed test");
        frac=(float)p;
        snprintf(line,sizeof(line),"%s",speedtest.stage==SPEED_RAW_READ?"Reading the card (no writes)":
                 speedtest.stage==SPEED_WRITE?"Writing a 32 MB temporary file":"Checking the written data");
        snprintf(v1,sizeof(v1),"%s",speedtest.stage==SPEED_RAW_READ?"Read":speedtest.stage==SPEED_WRITE?"Write":"Verify");
        snprintf(s1,sizeof(s1),"%s",speedtest.stage==SPEED_RAW_READ?"no writes":
                 speedtest.stage==SPEED_WRITE?"then verify":"last step");
        snprintf(v2,sizeof(v2),"%.0f MB",(double)speedtest.done/1048576.0);
        snprintf(s2,sizeof(s2),"of %.0f MB",(double)speedtest.size/1048576.0);
        began=speedtest.started;
    }
    if (began) {
        Uint32 sec=(SDL_GetTicks()-began)/1000u;
        snprintf(v3,sizeof(v3),"%02u:%02u",sec/60u,sec%60u);
        if (frac>0.02f) {
            unsigned left=(unsigned)((float)sec*(1.0f-frac)/frac);
            if (left<60) snprintf(s3,sizeof(s3),"under a minute left");
            else snprintf(s3,sizeof(s3),"about %u min left",(left+30)/60);
        } else snprintf(s3,sizeof(s3),"elapsed");
    }
    cui_text(cc,20,95,name,26,CUI_WHITE,CUI_LEFT);
    if (frac>=0) {
        snprintf(pct_s,sizeof(pct_s),"%d%%",(int)(frac*100));
        cui_text(cc,620,95,pct_s,26,CUI_WARN,CUI_RIGHT);
        cui_progress(cc,20,124,600,10,frac);
    } else {
        /* no total to measure against: a block that runs along the track */
        float t=(float)(SDL_GetTicks()%1600u)/1600.0f;
        cui_rrect(cc,20,124,600,10,5,CUI_TRACK);
        cui_rrect(cc,20+t*480,124,120,10,5,CUI_ACCENT);
    }
    cui_fit(line,16,600,fit,sizeof(fit));
    cui_text(cc,20,160,fit,16,CUI_GREY,CUI_LEFT);
    stat_box(c,20,job==JOB_FAT_CHECK?"FILESYSTEM":job==JOB_SPEED_READ||job==JOB_SPEED_WRITE?"STAGE":"FILES READ",v1,CUI_WHITE,s1);
    stat_box(c,225,job==JOB_FAT_CHECK?"MODE":job==JOB_STORAGE?"SIZE":job==JOB_SPEED_READ||job==JOB_SPEED_WRITE?"DATA":"READ ERRORS",
             v2,(job==JOB_QUICK||job==JOB_DEEP)?(scan.bad?CUI_BAD:CUI_GOOD):CUI_WHITE,s2);
    stat_box(c,430,job==JOB_FAT_CHECK?"STATE":"TIME",v3,CUI_WHITE,s3);
}
static void draw_screen(Canvas *c) {
    char tmp[512],sizebuf[32];
    header(c);
    if (page==PAGE_MAIN) {
        main_list(c);
        footer(c,card.readonly?"CARD IS READ ONLY - WRITE TOOLS DISABLED":"READ-ONLY DIAGNOSTICS BY DEFAULT");
        return;
    }
    if (page==PAGE_RUNNING) {
        /* FAT check examines the partition, not a file-scan counter. */
        running_screen(c);
        if (job==JOB_FAT_CHECK)
            footer(c,"FSCK.FAT -N READ ONLY - MOUNTED RESULTS MAY CHANGE");
        else if (job==JOB_QUICK || job==JOB_DEEP || job==JOB_STORAGE)
            footer(c,"SCANNING FILES - RESULT IS NOT A PHYSICAL HEALTH RATING");
        else footer(c,"TEST IN PROGRESS - TEMPORARY FILE CLEANED ON EXIT");
        return;
    }
    if (page==PAGE_SCAN_RESULT || page==PAGE_STORAGE_RESULT) {
        result_stats(c);sd_card(c,251,133);
        if (page==PAGE_SCAN_RESULT) {
            result_details(c);
            footer(c,"A: FILE ERRORS / KERNEL LOG / SAVE REPORT  B: BACK");
        } else {
            storage_details(c);
            footer(c,"FOLDER TOTALS INCLUDE HIDDEN FOLDERS - UP DOWN SCROLL");
        }
        return;
    }
    if (page==PAGE_SPEED_MENU) {
        left_card_info(c);sd_card(c,251,133);speed_side(c);
        footer(c,"READ: NO CARD WRITES   WRITE: 32 MB TEMPORARY FILE");return;
    }
    if (page==PAGE_SPEED_RESULT) {
        left_card_info(c);sd_card(c,251,133);speed_report(c);
        footer(c,speedtest.message);
        return;
    }
    if (page==PAGE_SPEED_CONFIRM) {
        left_card_info(c);sd_card(c,251,133);
        modal_side(c,"CONFIRM WRITE",
            "CREATE A 32 MB TEMPORARY FILE. TEST WRITE SPEED, FLUSH DATA, VERIFY CONTENTS, THEN REMOVE THE FILE.",
            "A: CONFIRM\nB: CANCEL");
        footer(c,"WRITE TEST REQUIRES 160 MB FREE AND SAFE BATTERY");return;
    }
    if (page==PAGE_REPAIR_MENU) {
        left_card_info(c);sd_card(c,251,133);repair_side(c);
        footer(c,"LIVE FSCK IS READ ONLY - FAT REPAIR REQUIRES UNMOUNT");return;
    }
    if (page==PAGE_CLEAN_LIST) {
        left_card_info(c);sd_card(c,251,133);candidate_side(c);
        snprintf(tmp,sizeof(tmp),"%d FILES - NOTHING DELETED AUTOMATICALLY",candidates_count);
        footer(c,tmp);return;
    }
    if (page==PAGE_CONFIRM_FILE) {
        const char *name=candidates_selected<candidates_count ? candidates[candidates_selected].name:"UNKNOWN";
        left_card_info(c);sd_card(c,251,133);
        if (candidates_selected<candidates_count && candidates[candidates_selected].directory)
            snprintf(sizebuf,sizeof(sizebuf),"FOLDER");
        else human_size(candidates_selected<candidates_count?candidates[candidates_selected].bytes:0,sizebuf,sizeof(sizebuf));
        snprintf(tmp,sizeof(tmp),"%s: %s. SIZE: %s. REVIEW THE SELECTED ITEM BEFORE PROCEEDING.",
                 restore_list?"RESTORE":"QUARANTINE",name,sizebuf);
        modal_side(c,restore_list?"CONFIRM RESTORE":"CONFIRM MOVE",tmp,
                   "A: CONFIRM\nB: CANCEL");
        footer(c,"QUARANTINE IS REVERSIBLE - DOES NOT FREE SPACE");return;
    }
    if (page==PAGE_DIAGNOSTICS) {
        left_card_info(c);sd_card(c,251,133);diagnostics_side(c);
        footer(c,"A: SAVE REPORT  UP/DOWN: LIST  B: BACK");return;
    }
    if (page==PAGE_REPORT_CONFIRM) {
        left_card_info(c);sd_card(c,251,133);
        modal_side(c,"SAVE REPORT",
            "SAVE A SMALL DIAGNOSTIC TEXT REPORT IN APP/SDCARDDOCTOR/REPORTS ON THIS SD CARD?",
            "A: CONFIRM\nB: CANCEL");
        footer(c,"OPT IN WRITE - NO AUTOMATIC REPORT SAVING");return;
    }
    if (page==PAGE_CARD_ID) {
        left_card_info(c);sd_card(c,251,133);card_id_side(c);
        footer(c,"RAW IDENTIFIERS ARE NOT PROOF OF A GENUINE CARD");return;
    }
    if (page==PAGE_FAT_STATUS) {
        left_card_info(c);sd_card(c,251,133);fat_status_side(c);
        footer(c,"FAT[1] FLAGS ARE STORED METADATA, NOT LIVE HEALTH STATUS");return;
    }
    if (page==PAGE_FAT_CHECK_WARN) {
        left_card_info(c);sd_card(c,251,133);
        live_fat_check_warning(c);
        footer(c,"DO NOT TREAT LIVE RESULTS AS AN OFFLINE REPAIR PLAN");return;
    }
    if (page==PAGE_FAT_CHECK_RESULT) {
        fatcheck_result_screen(c);
        footer(c,"A: SAVE REPORT  UP/DOWN: SCROLL  B: BACK");return;
    }
    if (page==PAGE_FAT_REPORT_CONFIRM) {
        left_card_info(c);sd_card(c,251,133);
        modal_side(c,"SAVE FAT REPORT",
            "SAVE THE READ-ONLY FAT CHECK RESULTS IN APP/SDCARDDOCTOR/REPORTS? THIS IS AN OPTIONAL WRITE.",
            "A: CONFIRM\nB: CANCEL");
        footer(c,"ONLY SAVES AFTER CONFIRMATION - NO REPAIR");return;
    }
    if (page==PAGE_FAT_INFO) {
        left_card_info(c);sd_card(c,251,133);
        modal_side(c,"FAT32 REPAIR",
            "REPAIR ONLY OFFLINE. BACK UP YOUR FILES, UNMOUNT THE CARD ON A COMPUTER, AND RUN FSCK.FAT -N BEFORE MAKING CHANGES.",
            "B: BACK");
        footer(c,"ON-DEVICE WRITING FSCK IS DISABLED FOR SAFETY");return;
    }
    if (page==PAGE_NOTICE) {
        left_card_info(c);sd_card(c,251,133);modal_side(c,notice_title,notice_body,"B: BACK");
        footer(c,"B: BACK ONE SCREEN");return;
    }
}

static void on_press(int id) {
    char msg[160]={0};
    if (exit_armed || b_exit_armed) return;
    if (id==KEY_B_ID) {
        switch(page) {
        case PAGE_MAIN: b_exit_armed=true; return;
        case PAGE_RUNNING:
            if (job==JOB_FAT_CHECK) {
                cancel_job();needs_redraw=true;return;
            }
            {
                Job old_job=job;
                cancel_job();
                page=(old_job==JOB_SPEED_READ || old_job==JOB_SPEED_WRITE)?
                     PAGE_SPEED_MENU:PAGE_MAIN;
            }
            break;
        case PAGE_CONFIRM_FILE:page=PAGE_CLEAN_LIST;break;
        case PAGE_CLEAN_LIST:case PAGE_FAT_STATUS:case PAGE_FAT_INFO:
        case PAGE_CARD_ID:case PAGE_FAT_CHECK_WARN:case PAGE_FAT_CHECK_RESULT:page=PAGE_REPAIR_MENU;break;
        case PAGE_DIAGNOSTICS:page=PAGE_SCAN_RESULT;break;
        case PAGE_REPORT_CONFIRM:page=PAGE_DIAGNOSTICS;break;
        case PAGE_FAT_REPORT_CONFIRM:page=PAGE_FAT_CHECK_RESULT;break;
        case PAGE_SPEED_CONFIRM:case PAGE_SPEED_RESULT:page=PAGE_SPEED_MENU;break;
        case PAGE_NOTICE:page=notice_back;break;
        default:page=PAGE_MAIN;break;
        }
        needs_redraw=true;
        return;
    }
    if (id==KEY_UP_ID || id==KEY_DOWN_ID) {
        int delta=id==KEY_DOWN_ID?1:-1;
        if (page==PAGE_MAIN) main_selected=(main_selected+5+delta)%5;
        else if (page==PAGE_SPEED_MENU) speed_selected=(speed_selected+2+delta)%2;
        else if (page==PAGE_REPAIR_MENU) repair_selected=(repair_selected+6+delta)%6;
        else if (page==PAGE_CLEAN_LIST && candidates_count)
            candidates_selected=(candidates_selected+candidates_count+delta)%candidates_count;
        else if (page==PAGE_DIAGNOSTICS && scan.failures_count>5) {
            diagnostics_scroll+=delta;
            if (diagnostics_scroll<0) diagnostics_scroll=0;
            if (diagnostics_scroll>(int)scan.failures_count-5)
                diagnostics_scroll=(int)scan.failures_count-5;
        }
        else if (page==PAGE_NOTICE || page==PAGE_SPEED_CONFIRM ||
                 page==PAGE_CONFIRM_FILE || page==PAGE_REPORT_CONFIRM ||
                 page==PAGE_FAT_REPORT_CONFIRM ||
                 page==PAGE_FAT_INFO || page==PAGE_FAT_CHECK_WARN ||
                 (page==PAGE_FAT_STATUS && !fatstatus.readable)) {
            int maxscroll=info_row_count>INFO_VISIBLE_ROWS?info_row_count-INFO_VISIBLE_ROWS:0;
            info_scroll+=delta;
            if (info_scroll<0) info_scroll=0;
            if (info_scroll>maxscroll) info_scroll=maxscroll;
        }
        else if (page==PAGE_FAT_CHECK_RESULT && fat_ui_count>8) {
            fat_ui_scroll+=delta;
            if (fat_ui_scroll<0) fat_ui_scroll=0;
            if (fat_ui_scroll>fat_ui_count-8) fat_ui_scroll=fat_ui_count-8;
        }
        else if (page==PAGE_STORAGE_RESULT && scan.group_count>6) {
            storage_scroll+=delta;
            if (storage_scroll<0) storage_scroll=0;
            if (storage_scroll>scan.group_count-6) storage_scroll=scan.group_count-6;
        }
        needs_redraw=true;return;
    }
    if (id!=KEY_A_ID) return;
    switch(page) {
    case PAGE_SCAN_RESULT:
        diagnostics_scroll=0;page=PAGE_DIAGNOSTICS;break;
    case PAGE_DIAGNOSTICS:
        page=PAGE_REPORT_CONFIRM;break;
    case PAGE_FAT_CHECK_RESULT:
        page=PAGE_FAT_REPORT_CONFIRM;break;
    case PAGE_FAT_REPORT_CONFIRM:
        if (save_fat_report(msg,sizeof(msg))) set_notice("REPORT SAVED",msg);
        else set_notice("REPORT FAILED",msg);
        break;
    case PAGE_REPORT_CONFIRM:
        if (save_scan_report(msg,sizeof(msg))) set_notice("REPORT SAVED",msg);
        else set_notice("REPORT FAILED",msg);
        break;
    case PAGE_MAIN:
        if (main_selected==0) begin_scan(false,false);
        else if (main_selected==1) begin_scan(true,false);
        else if (main_selected==2) page=PAGE_SPEED_MENU;
        else if (main_selected==3) {storage_scroll=0;begin_scan(false,true);}
        else page=PAGE_REPAIR_MENU;
        break;
    case PAGE_SPEED_MENU:
        if (speed_selected==0) begin_speed_read();
        else {refresh_card();page=PAGE_SPEED_CONFIRM;}
        break;
    case PAGE_SPEED_CONFIRM:
        begin_speed_write();break;
    case PAGE_REPAIR_MENU:
        if (repair_selected==0) {inspect_fat32();page=PAGE_FAT_STATUS;}
        else if (repair_selected==1) page=PAGE_FAT_CHECK_WARN;
        else if (repair_selected==2) {read_card_identity();page=PAGE_CARD_ID;}
        else if (repair_selected==5) page=PAGE_FAT_INFO;
        else {
            load_candidates(repair_selected==4);
            page=PAGE_CLEAN_LIST;
        }
        break;
    case PAGE_FAT_CHECK_WARN:
        begin_fatcheck();break;
    case PAGE_CLEAN_LIST:
        if (candidates_count) page=PAGE_CONFIRM_FILE;
        break;
    case PAGE_CONFIRM_FILE:
        refresh_card();
        if (!card.mounted || card.readonly) {
            set_notice("CARD READ ONLY","CANNOT MOVE FILES");
        } else if (move_candidate(restore_list,msg,sizeof(msg))) {
            set_notice("OPERATION COMPLETE",msg);
        } else {
            set_notice("MOVE BLOCKED",msg[0]?msg:"NO CHANGE MADE");
        }
        break;
    default:break;
    }
    needs_redraw=true;
}
static void set_key(int id,bool down) {
    if (id<0 || id>=KEY_COUNT || keys[id].down==down) return;
    keys[id].down=down;
    /* Catch the exact SELECT+START chord before forwarding any action. */
    if (keys[KEY_SELECT_ID].down && keys[KEY_START_ID].down && !exit_armed) {
        exit_armed=true;
        cancel_job();
        LOG("SELECT+START exit request, waiting for key releases");
        return;
    }
    if (b_exit_armed || exit_armed) {
        if (b_exit_armed && !keys[KEY_B_ID].down) quit_requested=true;
        if (exit_armed && !keys[KEY_SELECT_ID].down && !keys[KEY_START_ID].down && !fatcheck.active) quit_requested=true;
        return;
    }
    if (down) on_press(id);
}
static void poll_raw(void) {
    struct input_event e;
    ssize_t n;
    while ((n=read(input_fd,&e,sizeof(e)))==(ssize_t)sizeof(e)) {
        size_t i;
        if (e.type!=EV_KEY || e.value==2) continue;
        for(i=0;i<ARRAY_LEN(keys);i++) {
            if (keys[i].evdev_code==e.code) {set_key((int)i,e.value==1);break;}
        }
    }
    if (n==0 || (n<0 && errno!=EAGAIN && errno!=EINTR)) {
        LOG("evdev unavailable, using SDL fallback");
        close(input_fd);input_fd=-1;raw_input=false;
        memset(keys,0,sizeof(keys)); /* Restore code mapping below. */
        keys[KEY_UP_ID].evdev_code=KEY_UP; keys[KEY_UP_ID].sdl=SDLK_UP;
        keys[KEY_DOWN_ID].evdev_code=KEY_DOWN; keys[KEY_DOWN_ID].sdl=SDLK_DOWN;
        keys[KEY_LEFT_ID].evdev_code=KEY_LEFT; keys[KEY_LEFT_ID].sdl=SDLK_LEFT;
        keys[KEY_RIGHT_ID].evdev_code=KEY_RIGHT; keys[KEY_RIGHT_ID].sdl=SDLK_RIGHT;
        keys[KEY_A_ID].evdev_code=KEY_SPACE; keys[KEY_A_ID].sdl=SDLK_SPACE;
        keys[KEY_B_ID].evdev_code=KEY_LEFTCTRL; keys[KEY_B_ID].sdl=SDLK_LCTRL;
        keys[KEY_SELECT_ID].evdev_code=KEY_RIGHTCTRL; keys[KEY_SELECT_ID].sdl=SDLK_RCTRL;
        keys[KEY_START_ID].evdev_code=KEY_ENTER; keys[KEY_START_ID].sdl=SDLK_RETURN;
    }
}
static void poll_sdl(SDL_Event *event) {
    size_t i;
    if (raw_input || (event->type!=SDL_KEYDOWN && event->type!=SDL_KEYUP)) return;
    for(i=0;i<ARRAY_LEN(keys);i++) {
        if (keys[i].sdl==event->key.keysym.sym) {
            set_key((int)i,event->type==SDL_KEYDOWN);break;
        }
    }
}
int main(int argc,char **argv) {
    SDL_Surface *screen=NULL;
    uint32_t *pixels=NULL;
    Uint32 last_frame=0,last_refresh=0,last_checker_poll=0;
    int result=1;
    /* RedOnion's boot notice after an unclean shutdown opens it with
     * --fat-status: on Safe repair, the cursor on CHECK FAT STATUS (the
     * card's "not unmounted cleanly" flag, read only). B goes back to the
     * main page, on Safe repair. */
    if (argc>1 && strcmp(argv[1],"--fat-status")==0) {
        page=PAGE_REPAIR_MENU;main_selected=4;repair_selected=0;
    }
    signal(SIGINT,signal_stop);signal(SIGTERM,signal_stop);
    scan.file_fd=-1;speedtest.fd=-1;
    refresh_card();
    LOG("v0.6.3 startup: mounted=%d device=%s fs=%s fat32=%d",
         card.mounted,card.device,card.fstype,card.fat32);
    input_fd=open("/dev/input/event0",O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    raw_input=input_fd>=0;
    if (!raw_input) LOG("evdev inaccessible, SDL keyboard fallback");
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0) {
        LOG("SDL_Init failed: %s",SDL_GetError());goto cleanup;
    }
    SDL_ShowCursor(SDL_DISABLE);SDL_EnableKeyRepeat(0,0);
    screen=SDL_SetVideoMode(W,H,32,SDL_SWSURFACE);
    if (!screen) screen=SDL_SetVideoMode(W,H,16,SDL_SWSURFACE);
    if (!screen || !screen->format) {
        LOG("SDL video setup failed: %s",SDL_GetError());goto shutdown_sdl;
    }
    pixels=calloc((size_t)W*H,sizeof(*pixels));
    if (!pixels) {LOG("no canvas memory");goto shutdown_sdl;}
    while (!interrupted && !quit_requested) {
        SDL_Event event;
        Uint32 now=SDL_GetTicks();
        while (SDL_PollEvent(&event)) {
            if (event.type==SDL_QUIT) quit_requested=true;
            else poll_sdl(&event);
        }
        if (input_fd>=0) poll_raw();
        if (scan.active) advance_scan();
        if (fatcheck.active) advance_fatcheck();
        if (detached_checker_pid>0 && now-last_checker_poll>=1000u) {
            (void)detached_checker_alive();last_checker_poll=now;
        }
        if (exit_armed && !fatcheck.active && !keys[KEY_SELECT_ID].down &&
            !keys[KEY_START_ID].down) quit_requested=true;
        if (speedtest.stage) {
            /* Benchmark in batches: no mandatory 10ms pause per 256KB block. */
            Uint32 batch=SDL_GetTicks();
            int ops=0;
            while (speedtest.stage && ops++<16) {
                advance_speed();
                if (SDL_GetTicks()-batch>=30) break;
            }
        }
        if (page==PAGE_MAIN && now-last_refresh>=15000) {
            refresh_card();last_refresh=now;needs_redraw=true;
        }
        if (fatcheck.active && now-last_frame>=145u) needs_redraw=true;
        if (needs_redraw && now-last_frame>=(job==JOB_NONE?35u:125u)) {
            Canvas canvas={pixels,W};
            draw_screen(&canvas);
            if (!present(screen,pixels)) break;
            last_frame=now;needs_redraw=false;
        }
        SDL_Delay(job==JOB_NONE?10:2);
    }
    result=(quit_requested || interrupted)?0:1;
    free(pixels);
shutdown_sdl:
    SDL_Quit();
cleanup:
    /* This cleanup also executes after signal-triggered shutdown. */
    cancel_job();
    if (fatcheck.active) {
        if (fatcheck.pid>0) {kill(fatcheck.pid,SIGKILL);(void)waitpid(fatcheck.pid,NULL,WNOHANG);}
        if (fatcheck.pipe_fd>=0) close(fatcheck.pipe_fd);
        fatcheck.active=false;
    }
    if (detached_checker_pid>0) {
        (void)kill(detached_checker_pid,SIGKILL);
        (void)waitpid(detached_checker_pid,NULL,WNOHANG);
    }
    if (input_fd>=0) close(input_fd);
    LOG("shutdown status=%d",result);
    return result;
}
