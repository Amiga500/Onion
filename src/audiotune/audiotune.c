/*
 * AudioTune v0.2.9 beta - sound diagnostics and five-band preview EQ.
 * Miyoo Mini / Mini Plus, Onion / RedOnion. SPDX-License-Identifier: MIT.
 *
 * The UI provides a software EQ preview. On verified Miyoo firmware the
 * optional native hook can apply a saved EQ profile through audioserver.
 */
#define _POSIX_C_SOURCE 200809L
#include <SDL/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <math.h>
#include <signal.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "audiotune_presets.h"
#include "audiotune_eq.h"
#include "audiotune_mp3.h"

#define W 640
#define H 480
#define VERSION "V0.2.9"
#define ARRAY_LEN(x) ((int)(sizeof(x) / sizeof((x)[0])))
#define PI_F 3.14159265358979323846f
#define LUT_BITS 11
#define LUT_N (1 << LUT_BITS)
#define EQ_BANDS 5
#define GRAPH_STEPS 177
#define SETTINGS_FILE "/mnt/SDCARD/.tmp_update/config/audiotune.ini"
#define SETTINGS_DIR "/mnt/SDCARD/.tmp_update/config"
/* Test signals remain conservative; a user's own MP3 plays at unity gain. */
#define TEST_SIGNAL_GAIN 0.35f
#define MUSIC_DEMO_GAIN 0.35f
#define MP3_GAIN 1.0f

/* No persistent diagnostic files in the beta; opt in with AUDIOTUNE_DEBUG=1. */
#define LOG(...) do { if (getenv("AUDIOTUNE_DEBUG") && \
    strcmp(getenv("AUDIOTUNE_DEBUG"),"1")==0) { \
    fprintf(stderr,"audiotune: " __VA_ARGS__); \
    fputc('\n',stderr); fflush(stderr); } } while(0)

/* Millisecond diagnostics use elapsed time; the device's wall clock may be wrong. */
static double timing_now_ms(void) {
    struct timeval now;
    if (gettimeofday(&now,NULL)!=0) return 0.0;
    return (double)now.tv_sec*1000.0+(double)now.tv_usec/1000.0;
}
static volatile sig_atomic_t interrupted = 0;
static void on_signal(int sig) { (void)sig; interrupted = 1; }

/* Compact, self-contained 5x7 font (columns stored as 5-bit row masks).
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
 {',',{0,0,0,0,0,12,8}},    {'/',{1,2,2,4,8,8,16}},     {':',{0,4,4,0,4,4,0}},
 {'.',{0,0,0,0,0,12,12}},    {'!',{4,4,4,4,4,0,4}},
 {'?',{14,17,1,2,4,0,4}},    {'=',{0,31,0,31,0,0,0}},
 {'(',{2,4,8,8,8,4,2}},     {')',{8,4,2,2,2,4,8}},
 {'#',{10,31,10,10,31,10,0}}, {'%',{17,2,4,8,17,0,0}},
 {' ',{0,0,0,0,0,0,0}},
 /* Additional printable ASCII punctuation (large 5x7 labels). */
 {'"',{10,10,0,0,0,0,0}},     {'$',{4,15,20,14,5,30,4}},
 {'&',{12,18,20,8,21,18,13}}, {'\'',{4,4,8,0,0,0,0}},
 {'*',{0,21,14,31,14,21,0}}, {';',{0,12,12,0,12,8,16}},
 {'<',{2,4,8,16,8,4,2}},    {'>',{8,4,2,1,2,4,8}},
 {'@',{14,17,23,21,23,16,15}}, {'[',{14,8,8,8,8,8,14}},
 {'\\',{16,8,8,4,2,2,1}}, {']',{14,2,2,2,2,2,14}},
 {'^',{4,10,17,0,0,0,0}},   {'_',{0,0,0,0,0,0,31}},
 {'`',{8,4,2,0,0,0,0}},     {'{',{6,8,8,16,8,8,6}},
 {'|',{4,4,4,4,4,4,4}},     {'}',{12,2,2,1,2,2,12}},
 {'~',{0,0,9,22,0,0,0}}
};

/* Sharp 8x11 screen font: fixed-size monochrome glyphs rendered at native pixel
 * resolution. Unlike 5x7 nearest-neighbour stretching, bottom strokes stay
 * aligned and no characters are resampled at runtime.
 * Raster shapes adapted from the permissively licensed DejaVu Sans Mono. */
static const uint8_t small_font[][11] = {
    {0x38,0x38,0x38,0x2c,0x6c,0x7c,0x44,0xc6,0xc6,0x00,0x00}, /* A */
    {0x7c,0x4c,0x46,0x4c,0x7c,0x46,0x46,0x46,0x7c,0x00,0x00}, /* B */
    {0x3c,0x30,0x60,0x60,0x60,0x60,0x60,0x30,0x3c,0x00,0x00}, /* C */
    {0x78,0x6c,0x66,0x66,0x66,0x66,0x66,0x6c,0x78,0x00,0x00}, /* D */
    {0x7e,0x60,0x60,0x60,0x7c,0x60,0x60,0x60,0x7e,0x00,0x00}, /* E */
    {0x7e,0x60,0x60,0x60,0x7c,0x60,0x60,0x60,0x60,0x00,0x00}, /* F */
    {0x3c,0x60,0x60,0x60,0x60,0x6e,0x66,0x66,0x3c,0x00,0x00}, /* G */
    {0x66,0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0x66,0x00,0x00}, /* H */
    {0x7c,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7c,0x00,0x00}, /* I */
    {0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x4c,0x78,0x00,0x00}, /* J */
    {0x66,0x6c,0x78,0x78,0x78,0x78,0x6c,0x6c,0x66,0x00,0x00}, /* K */
    {0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x7e,0x00,0x00}, /* L */
    {0xee,0xee,0xee,0xfe,0xde,0xde,0xc6,0xc6,0xc6,0x00,0x00}, /* M */
    {0x66,0x66,0x76,0x76,0x56,0x5e,0x4e,0x4e,0x4e,0x00,0x00}, /* N */
    {0x38,0x6c,0x66,0x46,0xc6,0x46,0x66,0x6c,0x38,0x00,0x00}, /* O */
    {0x7c,0x6e,0x66,0x6e,0x7c,0x60,0x60,0x60,0x60,0x00,0x00}, /* P */
    {0x38,0x6c,0x66,0x46,0xc6,0x46,0x66,0x6c,0x3c,0x0c,0x00}, /* Q */
    {0x7c,0x6c,0x64,0x6c,0x78,0x6c,0x6c,0x66,0x66,0x00,0x00}, /* R */
    {0x38,0x64,0x60,0x70,0x3c,0x1c,0x06,0x4c,0x7c,0x00,0x00}, /* S */
    {0xfe,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00}, /* T */
    {0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x6c,0x3c,0x00,0x00}, /* U */
    {0xc6,0x46,0x64,0x6c,0x6c,0x2c,0x38,0x38,0x38,0x00,0x00}, /* V */
    {0xc2,0xc6,0xde,0xde,0xfe,0x7e,0x6e,0x6c,0x6c,0x00,0x00}, /* W */
    {0xc6,0x6c,0x3c,0x38,0x38,0x38,0x7c,0x6c,0xc6,0x00,0x00}, /* X */
    {0xc6,0x66,0x6c,0x3c,0x38,0x18,0x18,0x18,0x18,0x00,0x00}, /* Y */
    {0x7e,0x0e,0x0c,0x1c,0x38,0x30,0x70,0x60,0x7e,0x00,0x00}, /* Z */
    {0x38,0x6c,0x64,0x66,0x7e,0x66,0x64,0x6c,0x38,0x00,0x00}, /* 0 */
    {0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7e,0x00,0x00}, /* 1 */
    {0x38,0x4c,0x0c,0x0c,0x1c,0x18,0x30,0x60,0x7c,0x00,0x00}, /* 2 */
    {0x38,0x4c,0x0c,0x0c,0x38,0x0c,0x04,0x4c,0x78,0x00,0x00}, /* 3 */
    {0x1c,0x1c,0x3c,0x2c,0x6c,0x4c,0x7e,0x0c,0x0c,0x00,0x00}, /* 4 */
    {0x7c,0x60,0x60,0x78,0x0c,0x04,0x04,0x4c,0x78,0x00,0x00}, /* 5 */
    {0x3c,0x64,0x60,0x7c,0x6c,0x66,0x66,0x6c,0x3c,0x00,0x00}, /* 6 */
    {0x7c,0x0c,0x0c,0x1c,0x18,0x18,0x30,0x30,0x30,0x00,0x00}, /* 7 */
    {0x38,0x6c,0x64,0x6c,0x38,0x6c,0x46,0x6c,0x3c,0x00,0x00}, /* 8 */
    {0x38,0x6c,0x4c,0x4c,0x6e,0x3c,0x04,0x4c,0x38,0x00,0x00}, /* 9 */
    {0x00,0x00,0x10,0x10,0x10,0xfe,0x10,0x10,0x10,0x00,0x00}, /* + */
    {0x00,0x00,0x00,0x00,0x3c,0x3c,0x00,0x00,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x10,0x20}, /* , */
    {0x04,0x0c,0x08,0x08,0x18,0x10,0x30,0x20,0x60,0x40,0x00}, /* / */
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00,0x00}, /* : */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00}, /* . */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x10,0x00,0x00}, /* ! */
    {0x3c,0x4c,0x0c,0x18,0x10,0x30,0x00,0x30,0x30,0x00,0x00}, /* ? */
    {0x00,0x00,0x00,0x00,0xfe,0x00,0xfe,0x00,0x00,0x00,0x00}, /* = */
    {0x18,0x10,0x30,0x30,0x30,0x30,0x30,0x10,0x18,0x08,0x00}, /* ( */
    {0x30,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x30,0x30,0x00}, /* ) */
    {0x00,0x16,0x34,0x7e,0x2c,0x2c,0xfe,0x58,0x58,0x00,0x00}, /* # */
    {0x60,0xd0,0xd0,0x66,0x18,0x4e,0x1a,0x1a,0x0e,0x00,0x00}, /* % */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /*   */
    /* Matching 8x11 punctuation glyphs; indices stay aligned with font[]. */
    {0x66,0x66,0x44,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* " */
    {0x10,0x7c,0xd0,0xc0,0x78,0x1c,0x0c,0x7c,0x10,0x00,0x00}, /* $ */
    {0x38,0x6c,0x6c,0x38,0x70,0xde,0xcc,0xcc,0x76,0x00,0x00}, /* & */
    {0x18,0x18,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ' */
    {0x00,0x24,0x18,0x7e,0x18,0x24,0x00,0x00,0x00,0x00,0x00}, /* * */
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x10,0x20}, /* ; */
    {0x00,0x00,0x0c,0x18,0x30,0x60,0x30,0x18,0x0c,0x00,0x00}, /* < */
    {0x00,0x00,0x60,0x30,0x18,0x0c,0x18,0x30,0x60,0x00,0x00}, /* > */
    {0x38,0x6c,0xc6,0xde,0xd6,0xd6,0xde,0xc0,0x7c,0x00,0x00}, /* @ */
    {0x38,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x38,0x00,0x00}, /* [ */
    {0x40,0x60,0x20,0x30,0x10,0x18,0x08,0x0c,0x04,0x00,0x00}, /* backslash */
    {0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x38,0x00,0x00}, /* ] */
    {0x10,0x38,0x6c,0xc6,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ^ */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7e}, /* _ */
    {0x30,0x18,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ` */
    {0x0e,0x18,0x18,0x30,0x60,0x30,0x18,0x18,0x0e,0x00,0x00}, /* { */
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00}, /* | */
    {0x70,0x18,0x18,0x0c,0x06,0x0c,0x18,0x18,0x70,0x00,0x00}, /* } */
    {0x00,0x00,0x00,0x00,0x72,0xde,0x8c,0x00,0x00,0x00,0x00}, /* ~ */
};


/* Both raster sizes must have exactly the same character ordering. */
typedef char audiotune_font_table_sizes_must_match[
    ARRAY_LEN(font) == ARRAY_LEN(small_font) ? 1 : -1];

typedef struct { uint32_t *pixels; int pitch; } Canvas;
/* Test branch (RedOnion unified look): Exo 2 text through SDL_ttf and the
 * Tweaks header, tabs, panels, selection and hints. Without the font the
 * built-in bitmap font below is still used. */
#include "theme/render/canvas_ui.h"
#define CUI(c) (&(CuiCanvas){(c)->pixels,(c)->pitch})
static int ui_size(int scale) { return scale==1?15:scale==2?21:25; }

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
static void round_rect(Canvas *c, int x, int y, int w, int h, int r, uint32_t color) {
    int yy, xx;
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    for (yy = 0; yy < h; ++yy) {
        int inset = 0;
        if (r) {
            int dy = yy < r ? r - yy - 1 : (yy >= h-r ? yy - (h-r) : -1);
            if (dy >= 0) {
                /* Pixel-perfect curved corners without floating point. */
                while (inset < r && ((r-inset-1)*(r-inset-1) + dy*dy >= r*r)) ++inset;
            }
        }
        for (xx = inset; xx < w - inset; ++xx) put(c, x + xx, y + yy, color);
    }
}
static void framed_round(Canvas *c, int x, int y, int w, int h, int r,
                         uint32_t frame, uint32_t fill) {
    round_rect(c, x, y, w, h, r, frame);
    round_rect(c, x+1, y+1, w-2, h-2, r > 1 ? r-1 : 0, fill);
}
static void disc(Canvas *c, int cx, int cy, int radius, uint32_t color) {
    int dx, dy;
    for (dy = -radius; dy <= radius; ++dy)
        for (dx = -radius; dx <= radius; ++dx)
            if (dx*dx + dy*dy <= radius*radius) put(c,cx+dx,cy+dy,color);
}
static void ring(Canvas *c, int cx, int cy, int radius, int thickness, uint32_t color) {
    int dx, dy, inner = (radius-thickness)*(radius-thickness);
    for (dy = -radius; dy <= radius; ++dy)
        for (dx = -radius; dx <= radius; ++dx) {
            int d = dx*dx + dy*dy;
            if (d <= radius*radius && d >= inner) put(c,cx+dx,cy+dy,color);
        }
}
/* Small labels use 1:1 pixel glyphs: no scaling, interpolation or blur. */
#define SMALL_W 8
#define SMALL_H 11
#define SMALL_ADVANCE 8
static void glyph(Canvas *c, int x, int y, char ch, int scale, uint32_t color) {
    int i, row, col;
    const unsigned char *bits = NULL;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));
    for (i = 0; i < ARRAY_LEN(font); ++i)
        if (font[i].c == ch) { bits = font[i].bits; break; }
    /* An unsupported character must not silently disappear. */
    if (!bits) {
        for (i = 0; i < ARRAY_LEN(font); ++i)
            if (font[i].c == '?') { bits = font[i].bits; break; }
    }
    if (!bits) return;
    if (scale == 1) {
        for (row = 0; row < SMALL_H; ++row) {
            uint8_t mask = small_font[i][row];
            for (col = 0; col < SMALL_W; ++col)
                if (mask & (1u << (7 - col)))
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
    if (cui_font(ui_size(scale))) return cui_text_width(s,ui_size(scale));
    int count = (int)strlen(s);
    if (!count) return 0;
    return scale == 1 ? count * SMALL_ADVANCE - 1 : count * 6*scale - scale;
}
static void label(Canvas *c, int x, int y, const char *s, int scale, uint32_t color) {
    /* Same top-left placement as the bitmap glyphs: centre on their middle. */
    if (cui_font(ui_size(scale))) {
        cui_text(CUI(c),x,y+(scale==1?5.5f:3.5f*scale),s,ui_size(scale),color,CUI_LEFT);
        return;
    }
    int advance = scale == 1 ? SMALL_ADVANCE : 6*scale;
    while (*s) { glyph(c,x,y,*s++,scale,color); x += advance; }
}
static void centered(Canvas *c, int x, int y, const char *s, int scale, uint32_t color) {
    label(c,x-text_width(s,scale)/2,y,s,scale,color);
}
/* Draw a label so two independent status lines share the same right edge. */
static void right_aligned(Canvas *c, int right, int y, const char *s, int scale, uint32_t color) {
    label(c,right-text_width(s,scale),y,s,scale,color);
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


/* The evdev codes are from Onion's hardware keymap. SDL fallback uses the
   same key semantics (as in SD Card Doctor v0.6.1): A=Space, B=Left Ctrl,
   SELECT=Right Ctrl. This only applies when /dev/input/event0 is unavailable.
   SDL fallback still requires an on-device test on each supported model. */
typedef enum {
    KEY_UP_ID, KEY_DOWN_ID, KEY_LEFT_ID, KEY_RIGHT_ID,
    KEY_A_ID, KEY_B_ID, KEY_X_ID, KEY_Y_ID, KEY_L1_ID, KEY_L2_ID, KEY_R1_ID, KEY_R2_ID,
    KEY_SELECT_ID, KEY_START_ID, KEY_MENU_ID, AUDIO_KEY_COUNT
} KeyId;
typedef struct { unsigned short linux_code; SDLKey sdl_code; bool down; } InputKey;
#define KEY_DEF(code, fallback) { code, fallback, false }
static InputKey keys[AUDIO_KEY_COUNT] = {
    KEY_DEF(KEY_UP, SDLK_UP), KEY_DEF(KEY_DOWN, SDLK_DOWN),
    KEY_DEF(KEY_LEFT, SDLK_LEFT), KEY_DEF(KEY_RIGHT, SDLK_RIGHT),
    KEY_DEF(KEY_SPACE, SDLK_SPACE), KEY_DEF(KEY_LEFTCTRL, SDLK_LCTRL),
    KEY_DEF(KEY_LEFTSHIFT, SDLK_LSHIFT), KEY_DEF(KEY_LEFTALT, SDLK_LALT),
    KEY_DEF(KEY_E, SDLK_e), KEY_DEF(KEY_TAB, SDLK_TAB),
    KEY_DEF(KEY_T, SDLK_t), KEY_DEF(KEY_BACKSPACE, SDLK_BACKSPACE),
    KEY_DEF(KEY_RIGHTCTRL, SDLK_RCTRL), KEY_DEF(KEY_ENTER, SDLK_RETURN),
    KEY_DEF(KEY_ESC, SDLK_ESCAPE)
};
#undef KEY_DEF

typedef enum { PAGE_HOME, PAGE_TESTS, PAGE_EQ, PAGE_PROFILES, PAGE_COUNT } Page;
typedef enum {
    TEST_DEMO, TEST_BASS, TEST_MID, TEST_TREBLE, TEST_SWEEP,
    TEST_PINK, TEST_LEFT, TEST_RIGHT, TEST_CENTER, TEST_COUNT
} TestId;
static const char *const test_names[TEST_COUNT] = {
    "MUSIC DEMO", "BASS 400 HZ", "MID 1000 HZ", "TREBLE 4 KHZ",
    "FREQ SWEEP", "PINK NOISE", "LEFT CHANNEL", "RIGHT CHANNEL", "CENTER SIGNAL"
};
static const char *const test_detail[TEST_COUNT] = {
    "ORIGINAL SYNTH LOOP", "AUDIBLE BASS - 6 SEC", "MID TONE - 6 SEC",
    "HIGH TONE - 5 SEC", "180 TO 8000 HZ", "BROADBAND - 10 SEC",
    "HEADPHONES ONLY", "HEADPHONES ONLY", "DUAL CHANNEL - 5 SEC"
};
static const int test_seconds[TEST_COUNT] = {12,6,6,5,8,10,4,4,5};

typedef enum { PROFILE_FLAT, PROFILE_PRESENCE, PROFILE_FULL,
               PROFILE_CLEAR, PROFILE_WARM, PROFILE_BRIGHT,
               PROFILE_CUSTOM, PROFILE_COUNT } Profile;
static const char *const profile_names[PROFILE_COUNT] = {
    "FLAT", "PRESENCE", "FULL", "CLEAR", "WARM", "BRIGHT", "CUSTOM"
};
static const char *const profile_info[PROFILE_COUNT] = {
    "NO TONAL CORRECTION", "CRISP MIDRANGE AND DETAIL", "RICHER, MORE OPEN SOUND",
    "EMPHASIZED DETAILS", "FULLER, SOFTER SOUND", "EXTRA HIGH END", "YOUR ADJUSTED EQ"
};
/* Built-in profiles and band centers are shared with the native VQE hook. */
static const char *const eq_labels[EQ_BANDS] = {"250 HZ", "500 HZ", "1 KHZ", "4 KHZ", "8 KHZ"};

typedef struct { float b0,b1,b2,a1,a2,z1,z2; } Biquad;
typedef struct {
    bool open;
    SDL_AudioSpec format;
    bool playing;
    bool loop; /* repeat preview in EQ and Profiles without a timeout */
    bool eq_on;
    bool native_hook_present; /* Guarded by SDL_LockAudio on UI thread. */
    bool native_preview_valid; /* Applied native EQ snapshot was verified. */
    bool native_delta_active; /* Apply only nonzero profile differences. */
    int native_gains[EQ_BANDS];
    int native_cut;
    int native_output;
    int test;
    int gains[EQ_BANDS];
    float headroom;
    bool limiter_enabled; /* software preview only */
    Biquad biquad[2][EQ_BANDS];
    uint32_t sample_index;
    uint32_t total_samples;
    float phase;
    float phase2;
    float sweep_hz;
    float sweep_mul;
    uint32_t noise_rng;
    float noise_state[7];
    float meter_l, meter_r;
    float music_left, music_right;
} Audio;
static Audio audio;
/* Driver headroom estimate uses the exact shared EQ response at 48 kHz. */
static int native_headroom_db=0;
/* One shared music source for Home, Tests, EQ and Profiles. */
static AudioTuneMP3 music_mp3;
/* Probe file existence at boot; defer the 10+ MiB read and first decode
   until the user actually presses START for music. */
static bool mp3_candidate=false, mp3_attempted=false;
static bool audio_initializing=false;
static bool music_custom_available(void) {
    return music_mp3.loaded || (mp3_candidate && !mp3_attempted);
}
static void prepare_music_on_demand(void) {
    double began;
    if (mp3_attempted) return;
    mp3_attempted=true;
    if (!mp3_candidate) { LOG("music: no sample.mp3; built-in preview selected"); return; }
    began=timing_now_ms();
    if (audiotune_mp3_open(&music_mp3,"sample.mp3"))
        LOG("music: custom sample.mp3 loaded: %d Hz %d channel(s), %lu bytes; load %.0f ms",
            music_mp3.rate,music_mp3.channels,(unsigned long)music_mp3.length,
            timing_now_ms()-began);
    else LOG("music: sample.mp3 could not be opened/decoded; built-in preview selected; load %.0f ms",
             timing_now_ms()-began);
}
static float wave_table[LUT_N];
static Page page = PAGE_HOME;
static int selected_home = 0;
static int selected_test = TEST_DEMO;
static int selected_band = 2;
static int selected_profile = PROFILE_FLAT;
static int profile_cursor = PROFILE_FLAT;
static int custom_gains[EQ_BANDS];
static int graph_delta_y[GRAPH_STEPS+1];
static bool eq_enabled = false;
/* Old settings without these fields default to full SAFE compensation. */
static int headroom_mode=AUDIOTUNE_HEADROOM_SAFE;
static bool soft_limiter_enabled=false;
/* Manual target: no reliance on an unverified headphone-jack detector. */
static int output_mode=AUDIOTUNE_OUTPUT_SPEAKER;
static int applied_native_gains[EQ_BANDS]={0};
static int applied_native_cut=0;
static int applied_native_output=AUDIOTUNE_OUTPUT_SPEAKER;
static bool applied_native_valid=false;
static const char *applied_state_path=NULL;
/* Keep the user's saved Custom selection; bypass processing for a flat curve. */
static bool effective_eq_enabled(void) {
    const int *gains=(selected_profile==PROFILE_CUSTOM)?custom_gains:
                      audiotune_preset_gains[selected_profile];
    return eq_enabled && !audiotune_gains_all_zero(gains);
}
static bool dirty_settings = false;
static bool quit_requested = false;
static bool exit_chord_armed = false;
static bool raw_input = false;
static int input_fd = -1;
static char audio_error[96] = "AUDIO UNAVAILABLE";
static bool vqe_test_process=false;
static bool vqe_restore_requested=false;
static const char *vqe_status_path;
static void update_audio_eq(void);
static void update_vqe_test_state(void) {
    char line[32]; FILE *f;
    bool detected=false,valid=false,changed=false;
    int gains[EQ_BANDS]={0},mode=0,out=0,cut=0;
    if (vqe_status_path && strncmp(vqe_status_path,"/tmp/audiotune-",15)==0) {
        f=fopen(vqe_status_path,"r");
        if (f) {
            if (fgets(line,sizeof(line),f) && strncmp(line,"TEST_ACTIVE",11)==0)
                detected=true;
            fclose(f);
        }
    }
    if (detected && applied_state_path &&
        strncmp(applied_state_path,"/tmp/audiotune-vqe-",19)==0) {
        char tail;
        f=fopen(applied_state_path,"r");
        if (f) {
            char state[144];
            if(fgets(state,sizeof(state),f) &&
               sscanf(state,"V1 %d %d %d %d %d %d %d %d %c",
                      &gains[0],&gains[1],&gains[2],&gains[3],&gains[4],
                      &mode,&out,&cut,&tail)==8 &&
               (mode==0 || mode==1) && (out==0 || out==1) &&
               cut>=0 && cut<=40) {
                int i;valid=true;
                for(i=0;i<EQ_BANDS;++i)
                    if(gains[i]<-AUDIOTUNE_EQ_GAIN_LIMIT ||
                       gains[i]>AUDIOTUNE_EQ_GAIN_LIMIT) valid=false;
            }
            fclose(f);
        }
    }
    changed=(detected!=vqe_test_process || valid!=applied_native_valid ||
             (valid && (memcmp(gains,applied_native_gains,sizeof(gains))!=0 ||
                        cut!=applied_native_cut || out!=applied_native_output)));
    if(audio.open) SDL_LockAudio();
    audio.native_hook_present=detected;
    if(audio.open) SDL_UnlockAudio();
    vqe_test_process=detected;
    applied_native_valid=valid;
    if(valid) {
        memcpy(applied_native_gains,gains,sizeof(gains));
        applied_native_cut=cut;
        applied_native_output=out;
    }
    if(changed) update_audio_eq();
}

static float clampf(float a, float lo, float hi) {
    return a < lo ? lo : (a > hi ? hi : a);
}
static int clampi(int a, int lo, int hi) {
    return a < lo ? lo : (a > hi ? hi : a);
}
static void initialize_wave_table(void) {
    int i;
    for (i=0; i<LUT_N; ++i)
        wave_table[i] = sinf((float)i * (2.0f * PI_F / LUT_N));
}
static float lookup_sin(float phase) {
    int i = (int)phase;
    float fract = phase - i;
    int idx = i & (LUT_N-1);
    return wave_table[idx] + fract*(wave_table[(idx+1)&(LUT_N-1)]-wave_table[idx]);
}
/* Direct-form II transposed; state is separate for each channel and band. */
static float process_biquad(Biquad *b, float x) {
    float y=b->b0*x+b->z1;
    b->z1=b->b1*x-b->a1*y+b->z2;
    b->z2=b->b2*x-b->a2*y;
    if(!isfinite(y) || !isfinite(b->z1) || !isfinite(b->z2)) {
        b->z1=b->z2=0.0f;
        return 0.0f;
    }
    /* Prevent long-lived denormal feedback that can consume CPU and sound
       like a low-level residue after a shelf filter change. */
    if(fabsf(b->z1)<1e-20f) b->z1=0.0f;
    if(fabsf(b->z2)<1e-20f) b->z2=0.0f;
    return y;
}
static void update_audio_eq(void) {
    int ch,i,rate=audio.open?audio.format.freq:48000;
    int new_gains[EQ_BANDS];
    AudioTuneEQCoeff coeff[EQ_BANDS],native_coeff[EQ_BANDS];
    int correction[EQ_BANDS];
    bool delta_active=false;
    AudioTuneEQCoeff graph_coeff[EQ_BANDS];
    float maximum,headroom;
    /* On the Profiles page the highlighted profile is a temporary live
       audition, committed only by A. Nothing is saved while browsing. */
    bool preview_on=(page==PAGE_PROFILES && profile_cursor!=selected_profile)?
                    (profile_cursor!=PROFILE_FLAT):effective_eq_enabled();
    memcpy(new_gains,(page==PAGE_PROFILES)?
           (profile_cursor==PROFILE_CUSTOM?custom_gains:
            audiotune_preset_gains[profile_cursor]):
           (selected_profile==PROFILE_CUSTOM?custom_gains:
            audiotune_preset_gains[selected_profile]),sizeof(new_gains));
    if(audiotune_gains_all_zero(new_gains)) preview_on=false;
    /* When a known native profile is active, audition only the DIFFERENCE
       against the applied native settings. This avoids stacking the full
       software curve on top of native EQ. It is only an approximation because
       SigmaStar quantizes its curve to 129 bins. */
    for(i=0;i<EQ_BANDS;++i) {
        correction[i]=(applied_native_valid && vqe_test_process ?
                       (preview_on?new_gains[i]:0)-applied_native_gains[i]
                       :new_gains[i]);
        audiotune_eq_make(&coeff[i],i,rate,correction[i]);
    }
    maximum=audiotune_eq_peak_db(rate,coeff);
    for(i=0;i<EQ_BANDS;++i) {
        if(correction[i]) delta_active=true;
        audiotune_eq_make(&graph_coeff[i],i,rate,new_gains[i]);
    }
    for(i=0;i<=GRAPH_STEPS;++i) {
        float hz=90.0f*powf(9000.0f/90.0f,(float)i/GRAPH_STEPS);
        float db=audiotune_eq_response_db(clampf(hz,90.0f,rate*0.44f),rate,graph_coeff);
        graph_delta_y[i]=clampi((int)(db*3.8f),-46,46);
    }
    /* SAFE: full peak + 1 dB guard. PUNCHY: allow at most +3 dB peak
       in the SOFTWARE EQ; a local optional limiter catches excursions. */
    headroom=maximum>0.0f ? fmaxf(0.0f,maximum+1.0f-
              (headroom_mode==AUDIOTUNE_HEADROOM_PUNCHY &&
               output_mode==AUDIOTUNE_OUTPUT_HEADPHONES ?
               AUDIOTUNE_NATIVE_PEAK_DB:0)) : 0.0f;
    headroom=powf(10.0f,-headroom/20.0f);
    native_headroom_db=0;
    if (preview_on) {
        float native_max;
        for(i=0;i<EQ_BANDS;++i)
            audiotune_eq_make(&native_coeff[i],i,48000,new_gains[i]);
        native_max=audiotune_native_peak_db(48000,native_coeff,output_mode);
        native_headroom_db=audiotune_native_attenuation_db(native_max,
          output_mode==AUDIOTUNE_OUTPUT_SPEAKER?AUDIOTUNE_HEADROOM_SAFE:headroom_mode);
    }
    if(audio.open) SDL_LockAudio();
    memcpy(audio.gains,new_gains,sizeof(new_gains));
    audio.eq_on=preview_on;
    audio.native_preview_valid=(applied_native_valid && vqe_test_process);
    audio.native_delta_active=audio.native_preview_valid && delta_active;
    audio.headroom=headroom;
    /* The local differential preview is deliberately at limited headroom.
       It never applies the native attenuation twice. */
    if(audio.native_delta_active) {
        int safe_mode=(output_mode==AUDIOTUNE_OUTPUT_SPEAKER)?
                      AUDIOTUNE_HEADROOM_SAFE:headroom_mode;
        float cut=maximum>0.0f ?
           fmaxf(0.0f,maximum+1.0f-(safe_mode==AUDIOTUNE_HEADROOM_PUNCHY?
                                 AUDIOTUNE_NATIVE_PEAK_DB:0)):0.0f;
        audio.headroom=powf(10.0f,-cut/20.0f);
    }
    audio.limiter_enabled=soft_limiter_enabled;
    memcpy(audio.native_gains,applied_native_gains,sizeof(audio.native_gains));
    audio.native_cut=applied_native_cut;
    audio.native_output=applied_native_output;
    for(ch=0;ch<2;++ch)
        for(i=0;i<EQ_BANDS;++i) {
            float old1=audio.biquad[ch][i].z1,old2=audio.biquad[ch][i].z2;
            /* Discard stale integrator energy on major mode/preset changes;
               especially important for a 250 Hz shelf on a tiny speaker. */
            float change=fabsf(audio.biquad[ch][i].b0-coeff[i].b0);
            if(change>0.025f || !isfinite(old1) || !isfinite(old2))
                old1=old2=0.0f;
            audio.biquad[ch][i].b0=coeff[i].b0;
            audio.biquad[ch][i].b1=coeff[i].b1;
            audio.biquad[ch][i].b2=coeff[i].b2;
            audio.biquad[ch][i].a1=coeff[i].a1;
            audio.biquad[ch][i].a2=coeff[i].a2;
            audio.biquad[ch][i].z1=old1;
            audio.biquad[ch][i].z2=old2;
        }
    if(audio.open) SDL_UnlockAudio();
}
static float prng_white(Audio *a) {
    a->noise_rng = a->noise_rng*1664525u+1013904223u;
    return ((float)((a->noise_rng>>8)&0xffffffu)/8388608.0f) - 1.0f;
}
static float pink_sample(Audio *a) {
    float white=prng_white(a),out;
    /* Paul Kellet's seven-stage low-cost pink-noise approximation. */
    float *b=a->noise_state;
    b[0]=0.99886f*b[0]+white*0.0555179f;
    b[1]=0.99332f*b[1]+white*0.0750759f;
    b[2]=0.96900f*b[2]+white*0.1538520f;
    b[3]=0.86650f*b[3]+white*0.3104856f;
    b[4]=0.55000f*b[4]+white*0.5329522f;
    b[5]=-0.7616f*b[5]-white*0.0168980f;
    out=b[0]+b[1]+b[2]+b[3]+b[4]+b[5]+b[6]+white*0.5362f;
    b[6]=white*0.115926f;
    return clampf(out*0.13f,-1.0f,1.0f);
}
static float next_sample(Audio *a) {
    static const float melody[]={392.0f,440.0f,523.25f,587.33f,
                                 440.0f,349.23f,392.0f,293.66f};
    float freq=1000.0f, value;
    int sr=a->format.freq;
    if (a->test==TEST_PINK) return pink_sample(a);
    switch (a->test) {
        case TEST_BASS: freq=400.0f; break;
        case TEST_MID: freq=1000.0f; break;
        case TEST_TREBLE: freq=4000.0f; break;
        case TEST_SWEEP: freq=a->sweep_hz; a->sweep_hz*=a->sweep_mul; break;
        case TEST_LEFT: case TEST_RIGHT: case TEST_CENTER: freq=750.0f; break;
        case TEST_DEMO: {
            unsigned long note_len=(unsigned long)((float)sr*0.33f);
            unsigned long note=(a->sample_index/note_len) % ARRAY_LEN(melody);
            unsigned long in_note=a->sample_index%note_len;
            float env=clampf((float)in_note/(sr*0.015f),0,1) *
                      clampf((note_len-in_note)/(sr*0.065f),0,1);
            freq=melody[note];
            value=(lookup_sin(a->phase)*0.80f+lookup_sin(a->phase2)*0.20f)*env;
            a->phase2+=freq*2.0f*LUT_N/sr;
            if (a->phase2>=LUT_N) a->phase2-=LUT_N;
            a->phase+=freq*LUT_N/sr;
            if (a->phase>=LUT_N) a->phase-=LUT_N;
            return value;
        }
        default: break;
    }
    value=lookup_sin(a->phase);
    a->phase+=freq*LUT_N/sr;
    if (a->phase>=LUT_N) a->phase-=LUT_N;
    return value;
}
static void sdl_audio_callback(void *userdata, Uint8 *stream, int len) {
    Audio *a=(Audio *)userdata;
    int16_t *out=(int16_t *)stream;
    int frames=len/((int)sizeof(int16_t)*2);
    int i,ch,k;
    float peak[2]={0.0f,0.0f};
    memset(stream,0,(size_t)len);
    if (!a->playing) { a->meter_l=0; a->meter_r=0; return; }
    for (i=0;i<frames;++i) {
        float v, envelope;
        if (!(a->test==TEST_DEMO && music_mp3.loaded) &&
            a->sample_index>=a->total_samples) {
            if (a->loop && a->test==TEST_DEMO) {
                /* One complete eight-note phrase, with existing note envelope
                   fading to zero at the seam. Keep filter state continuous. */
                a->sample_index=0;
            } else {
                a->playing=false;
                break;
            }
        }
        {
            float mp3_left=0.0f,mp3_right=0.0f;
            bool custom=(a->test==TEST_DEMO && music_mp3.loaded &&
                         audiotune_mp3_sample(&music_mp3,a->format.freq,
                                              &mp3_left,&mp3_right));
            v=custom ? 0.0f : next_sample(a);
            /* Retain channel separation for stereo MP3 sources. */
            a->music_left=custom ? mp3_left : v;
            a->music_right=custom ? mp3_right : v;
        }
        /* MP3 duration is independent of the built-in synth phrase timer.
           Never fade custom music every 2.64 s; the MP3 decoder loops. */
        if (a->test==TEST_DEMO && music_mp3.loaded)
            envelope=clampf((float)a->sample_index/(a->format.freq*0.025f),0,1);
        else
            envelope=clampf((float)a->sample_index/(a->format.freq*0.025f),0,1) *
                     clampf((float)(a->total_samples-a->sample_index)/
                            (a->format.freq*0.055f),0,1);
        for (ch=0;ch<2;++ch) {
            /* MP3 is a listening reference: unlike the generated tones, it
               must not be reduced by a fixed -16 dB software gain. */
            const float source_gain=(a->test==TEST_DEMO)?
                (music_mp3.loaded ? MP3_GAIN : MUSIC_DEMO_GAIN):TEST_SIGNAL_GAIN;
            float signal=(ch==0?a->music_left:a->music_right)*envelope*source_gain;
            if ((a->test==TEST_LEFT && ch==1) ||
                (a->test==TEST_RIGHT && ch==0)) signal=0.0f;
            /* The preloaded audioserver processes our PCM too. Do not apply
               a second EQ or headroom cut while the native hook is loaded.
               Edits to the on-screen bands take effect after server sync on
               exit, not instantly in this mode. */
            if ((!a->native_hook_present && a->eq_on) || a->native_delta_active) {
                signal*=a->headroom;
                for (k=0;k<EQ_BANDS;++k)
                    signal=process_biquad(&a->biquad[ch][k],signal);
            }
            /* The optional local limiter does not process game audio, and is
               intentionally bypassed under the native hook for comparison. */
            if ((a->limiter_enabled && !a->native_hook_present) ||
                a->native_delta_active) signal=audiotune_soft_limit(signal);
            /* Retain the complete signed 16-bit output range. The previous
               +/-0.68 limiter silently clipped full-scale MP3 peaks. */
            signal=clampf(signal,-1.0f,32767.0f/32768.0f);
            if (fabsf(signal)>peak[ch]) peak[ch]=fabsf(signal);
            out[i*2+ch]=(int16_t)(signal*32768.0f);
        }
        ++a->sample_index;
        if (a->test==TEST_DEMO && music_mp3.loaded &&
            a->sample_index>(uint32_t)a->format.freq*3600u)
            a->sample_index=(uint32_t)a->format.freq;
    }
    a->meter_l=peak[0]; a->meter_r=peak[1];
}
static void audio_stop(void) {
    if (!audio.open) return;
    SDL_LockAudio();
    audio.playing=false;
    audio.meter_l=audio.meter_r=0;
    SDL_UnlockAudio();
}
static void audio_play(int test, bool loop) {
    int ch,i;
    if (!audio.open) return;
    /* No SDL callback race: load first, then take SDL's audio lock. */
    if (test==TEST_DEMO) prepare_music_on_demand();
    SDL_LockAudio();
    audio.test=test;
    audio.sample_index=0;
    audio.loop=(loop && test==TEST_DEMO);
    audio.total_samples=audio.loop?
        (uint32_t)((uint32_t)(audio.format.freq*0.33f)*8u):
        (uint32_t)(audio.format.freq*test_seconds[test]);
    if (test==TEST_DEMO && music_mp3.loaded) audiotune_mp3_rewind(&music_mp3);
    audio.phase=audio.phase2=0;
    audio.sweep_hz=180.0f;
    audio.sweep_mul=expf(logf(8000.0f/180.0f)/(float)audio.total_samples);
    audio.noise_rng=0x2c17e543u;
    memset(audio.noise_state,0,sizeof(audio.noise_state));
    for (ch=0;ch<2;++ch)
        for (i=0;i<EQ_BANDS;++i)
            audio.biquad[ch][i].z1=audio.biquad[ch][i].z2=0;
    audio.meter_l=audio.meter_r=0;
    audio.playing=true;
    SDL_UnlockAudio();
}
static void audio_toggle(int test, bool loop) {
    bool same;
    if (!audio.open) return;
    SDL_LockAudio(); same=audio.playing && audio.test==test; SDL_UnlockAudio();
    if (same) audio_stop(); else audio_play(test,loop);
}
static bool audio_is_playing(void) {
    bool playing;
    if (!audio.open) return false;
    SDL_LockAudio(); playing=audio.playing; SDL_UnlockAudio();
    return playing;
}
static void audio_initialize(void) {
    SDL_AudioSpec want, got;
    if (getenv("AUDIOTUNE_NO_AUDIO")) {
        snprintf(audio_error,sizeof(audio_error),"SAFE MODE: NO OSS BRIDGE");
        LOG("safe mode: audioserver bridge unavailable; playback disabled");
        return;
    }
    if (SDL_InitSubSystem(SDL_INIT_AUDIO)<0) {
        snprintf(audio_error,sizeof(audio_error),"SDL AUDIO INIT FAILED");
        LOG("SDL_InitSubSystem(AUDIO): %s",SDL_GetError());
        return;
    }
    memset(&want,0,sizeof(want)); memset(&got,0,sizeof(got));
    want.freq=48000;
    want.format=AUDIO_S16SYS;
    want.channels=2;
    want.samples=1024;
    want.callback=sdl_audio_callback;
    want.userdata=&audio;
    if (SDL_OpenAudio(&want,&got)<0) {
        /* Older SDL/OSS bridges may reject 48 kHz. Keep the diagnostics
           usable instead of failing audio initialization outright. */
        LOG("SDL_OpenAudio 48 kHz rejected: %s; retrying 22.05 kHz",SDL_GetError());
        want.freq=22050; want.samples=512;
        memset(&got,0,sizeof(got));
        if (SDL_OpenAudio(&want,&got)<0) {
            snprintf(audio_error,sizeof(audio_error),"SDL AUDIO OPEN FAILED");
            LOG("SDL_OpenAudio fallback: %s",SDL_GetError());
            return;
        }
    }
    if (got.format!=AUDIO_S16SYS || got.channels!=2 ||
        got.freq<16000 || got.freq>48000) {
        snprintf(audio_error,sizeof(audio_error),"UNSUPPORTED AUDIO FORMAT");
        LOG("unsupported obtained format: freq=%d channels=%u format=%u",
            got.freq,(unsigned)got.channels,(unsigned)got.format);
        SDL_CloseAudio();
        return;
    }
    audio.format=got;
    audio.open=true;
    update_audio_eq();
    LOG("SDL audio ready freq=%d stereo S16 samples=%u",got.freq,(unsigned)got.samples);
    SDL_PauseAudio(0);
    snprintf(audio_error,sizeof(audio_error),"AUDIO READY");
}
static void audio_shutdown(void) {
    if (audio.open) {
        audio_stop();
        SDL_PauseAudio(1);
        SDL_CloseAudio();
        audio.open=false;
    }
}
/* AudioTune settings live in Onion/RedOnion's persistent config directory.
   The App config.json is an Onion manifest and is never changed. */
static void save_settings(void);
static void apply_profile(int id) {
    selected_profile=clampi(id,0,PROFILE_COUNT-1);
    profile_cursor=selected_profile;
    /* The sound curve is independent of the speaker/headphone protection target. */
    eq_enabled=(selected_profile!=PROFILE_FLAT);
    dirty_settings=true;
    update_audio_eq();
    save_settings();
}
static void ensure_custom_profile(void);
static void change_eq_gain(int band, int delta) {
    int before;
    /* At a limit, do not create Custom or dirty the SD-card setting. */
    before=(selected_profile==PROFILE_CUSTOM)?custom_gains[band]:
           audiotune_preset_gains[selected_profile][band];
    if (clampi(before+delta,-AUDIOTUNE_EQ_GAIN_LIMIT,AUDIOTUNE_EQ_GAIN_LIMIT)==before) return;
    ensure_custom_profile();
    custom_gains[band]=clampi(before+delta,-AUDIOTUNE_EQ_GAIN_LIMIT,AUDIOTUNE_EQ_GAIN_LIMIT);
    eq_enabled=true;
    dirty_settings=true;
    update_audio_eq();
    /* The press/repeat handler saves on key release, not every 95 ms. */
}
/* Convert an existing preset to Custom before editing its EQ values. */
static void ensure_custom_profile(void) {
    if (selected_profile != PROFILE_CUSTOM) {
        memcpy(custom_gains,audiotune_preset_gains[selected_profile],sizeof(custom_gains));
        selected_profile=PROFILE_CUSTOM;
    }
    profile_cursor=PROFILE_CUSTOM;
}
static void reset_eq_band(void) {
    ensure_custom_profile();
    custom_gains[selected_band]=0;
    eq_enabled=true;
    dirty_settings=true;
    update_audio_eq();
    save_settings();
}
static void reset_eq_all(void) {
    ensure_custom_profile();
    memset(custom_gains,0,sizeof(custom_gains));
    eq_enabled=true;
    dirty_settings=true;
    update_audio_eq();
    save_settings();
}
static void save_settings(void) {
    FILE *f;
    char tmp[128];
    if (!dirty_settings) return;
    if (mkdir(SETTINGS_DIR,0755)!=0 && errno!=EEXIST) {
        LOG("cannot create %s: %s",SETTINGS_DIR,strerror(errno));
        return;
    }
    snprintf(tmp,sizeof(tmp),"%s.tmp",SETTINGS_FILE);
    f=fopen(tmp,"w");
    if (!f) { LOG("cannot save %s: %s",tmp,strerror(errno)); return; }
    fprintf(f,"# AudioTune beta EQ profile\n");
    fprintf(f,"preset=%d\neq_enabled=%d\n",selected_profile,eq_enabled?1:0);
    fprintf(f,"headroom_mode=%d\nsoft_limiter=%d\noutput_mode=%d\n",headroom_mode,
            soft_limiter_enabled?1:0,output_mode);
    fprintf(f,"custom=%d %d %d %d %d\n",custom_gains[0],custom_gains[1],
            custom_gains[2],custom_gains[3],custom_gains[4]);
    {
        int failed=0, saved_errno=0;
        if (fflush(f)!=0) { failed=1; saved_errno=errno; }
        if (!failed && fsync(fileno(f))!=0) { failed=1; saved_errno=errno; }
        if (fclose(f)!=0 && !failed) { failed=1; saved_errno=errno; }
        if (failed) {
            LOG("cannot finish saving %s: %s",tmp,strerror(saved_errno));
            unlink(tmp); return;
        }
    }
    if (rename(tmp,SETTINGS_FILE)!=0) {
        LOG("rename %s to %s failed: %s",tmp,SETTINGS_FILE,strerror(errno));
        unlink(tmp); return;
    }
    LOG("EQ profile settings saved");
    dirty_settings=false;
}
static void load_settings(void) {
    FILE *f=fopen(SETTINGS_FILE,"r");
    char line[160];
    int p=0,e=0;
    if (!f) return;
    while (fgets(line,sizeof(line),f)) {
        int x[EQ_BANDS],i;
        if (sscanf(line,"preset=%d",&i)==1) p=i;
        else if (sscanf(line,"eq_enabled=%d",&i)==1) e=(i!=0);
        else if (sscanf(line,"headroom_mode=%d",&i)==1)
            headroom_mode=(i==AUDIOTUNE_HEADROOM_PUNCHY)?AUDIOTUNE_HEADROOM_PUNCHY:AUDIOTUNE_HEADROOM_SAFE;
        else if (sscanf(line,"soft_limiter=%d",&i)==1) soft_limiter_enabled=(i==1);
        else if (sscanf(line,"output_mode=%d",&i)==1)
            output_mode=(i==AUDIOTUNE_OUTPUT_HEADPHONES)?AUDIOTUNE_OUTPUT_HEADPHONES:AUDIOTUNE_OUTPUT_SPEAKER;
        else if (sscanf(line,"custom=%d %d %d %d %d",
                        &x[0],&x[1],&x[2],&x[3],&x[4])==EQ_BANDS)
            for(i=0;i<EQ_BANDS;++i) custom_gains[i]=clampi(x[i],-AUDIOTUNE_EQ_GAIN_LIMIT,AUDIOTUNE_EQ_GAIN_LIMIT);
    }
    fclose(f);
    selected_profile=clampi(p,0,PROFILE_COUNT-1);
    profile_cursor=selected_profile;
    eq_enabled=(e!=0);
    dirty_settings=false;
    LOG("loaded settings: preset=%d eq=%d",selected_profile,eq_enabled);
}

/* ButtonLab-style pixels, inset panels and amber-gold accents. */
#define BG_DARK CUI_BG
#define BORDER CUI_TRACK
#define PANEL_BG CUI_PANEL
#define WHITE CUI_WHITE
#define MUTED CUI_GREY
#define TEAL rgb(214,86,112) /* accent, light enough for text on the panels */
#define AMBER CUI_WARN

static void box_panel(Canvas *c,int x,int y,int w,int h) {
    cui_rrect(CUI(c),x,y,w,h,12,CUI_PANEL);
}
static void draw_background(Canvas *c) {
    cui_rect(CUI(c),0,0,W,H,CUI_BG);
}
static void draw_header(Canvas *c) {
    static const char *const tabs[]={"Home","Tests","EQ","Profiles"};
    CuiCanvas *cc=CUI(c);
    int i,x,widths[4],total=0;
    cui_header(cc,"wave","AudioTune",audio.open?"Audio ready":
               audio_initializing?"Audio starting":"Audio offline",-1);
    cui_text(cc,60+cui_text_width("AudioTune",25)+10,31,VERSION,14,CUI_DIM,CUI_LEFT);
    /* pages as tabs, like the Package Manager: L1/R1 change them */
    cui_rect(cc,0,60,W,34,CUI_HEAD);
    cui_rrect(cc,14,66,30,22,6,CUI_TRACK);
    cui_text(cc,29,77,"L1",14,CUI_GREY,CUI_CENTER);
    cui_rrect(cc,596,66,30,22,6,CUI_TRACK);
    cui_text(cc,611,77,"R1",14,CUI_GREY,CUI_CENTER);
    for (i=0;i<4;i++) {widths[i]=cui_text_width(tabs[i],19)+34;total+=widths[i];}
    x=320-total/2;
    for (i=0;i<4;i++) {
        if (i==(int)page) cui_rrect(cc,x,64,widths[i],26,13,CUI_ACCENT);
        cui_text(cc,x+widths[i]/2,77,tabs[i],19,i==(int)page?CUI_WHITE:CUI_GREY,CUI_CENTER);
        x+=widths[i];
    }
}
static void draw_footer(Canvas *c) {
    const char *hint;
    bool music=false;
    if (audio.open) {
        SDL_LockAudio();
        music=audio.playing && audio.test==TEST_DEMO;
        SDL_UnlockAudio();
    }
    switch (page) {
        case PAGE_HOME:
            hint=vqe_test_process?
                "UP/DOWN: CHOOSE  A: OPEN  X: RESTORE AUDIO  START: MUSIC":
                music?"UP/DOWN: CHOOSE    A: OPEN    START: STOP MUSIC":
                      "UP/DOWN: CHOOSE    A: OPEN    START: MUSIC";
            break;
        case PAGE_TESTS:
            hint="UP/DOWN: TEST    A: PLAY / STOP    START: FLAT / EQ";
            break;
        case PAGE_EQ:
            hint=music?
                "L/R: BAND  UP/DN: DB  X: 0  Y: ALL 0  A: EQ  START: STOP":
                "L/R: BAND  UP/DN: DB  X: 0  Y: ALL 0  A: EQ  START: PLAY";
            break;
        default:
            hint=music?
                "UP/DOWN: PRESET   A: APPLY   X: SAVE   START: STOP MUSIC":
                "UP/DOWN: PRESET   A: APPLY   X: SAVE   START: MUSIC";
            break;
    }
    {
        /* Round hints: B, then L2/R2 where the page has them, and
           SELECT+START on the right. L1/R1 are on the tabs. */
        CuiCanvas *cc=CUI(c);
        char fit[128];
        int x=20;
        cui_fit(hint,14,600,fit,sizeof(fit));
        cui_text(cc,320,430,fit,14,CUI_GREY,CUI_CENTER);
        x+=cui_hint(cc,x,459,"B",page==PAGE_HOME?"EXIT":"HOME",21);
        if (page==PAGE_EQ || page==PAGE_PROFILES) {
            x+=cui_hint(cc,x,459,"L2","OUTPUT",21);
            cui_hint(cc,x,459,"R2",page==PAGE_EQ?"LIMITER":
                     output_mode==AUDIOTUNE_OUTPUT_HEADPHONES?"MODE":"HP ONLY",21);
        }
        x=620-(cui_text_width("SELECT+START",14)+18)-7-cui_text_width("EXIT",21);
        cui_hint(cc,x,459,"SELECT+START","EXIT",21);
    }
}
static void draw_meter(Canvas *c,int x,int y,int w,float level,const char *side) {
    int filled=clampi((int)(clampf(level,0.0f,1.0f)*(w-54)),0,w-54);
    label(c,x,y-1,side,1,rgb(220,220,224));
    round_rect(c,x+28,y-1,w-54,13,4,CUI_TRACK);
    if (filled>0) round_rect(c,x+28,y-1,filled,13,4,TEAL);
}
static void read_meters(float *l,float *r, int *active_test, float *fraction) {
    *l=*r=0; *fraction=0; *active_test=-1;
    if (!audio.open) return;
    SDL_LockAudio();
    *l=audio.meter_l;
    *r=audio.meter_r;
    if (audio.playing) {
        *active_test=audio.test;
        if (audio.total_samples && !(audio.test==TEST_DEMO && music_mp3.loaded))
            *fraction=(float)audio.sample_index/audio.total_samples;
    }
    SDL_UnlockAudio();
}
static void draw_home(Canvas *c,int test,float l,float r) {
    int i;
    static const char *const names[]={"TEST SIGNALS","FIVE BAND EQ","EQ PROFILES"};
    static const char *const sub[]={"TONES AND CHANNELS","ADJUST LIVE SOUND","SPEAKER / HEADPHONES"};
    box_panel(c,18,98,394,317);
    box_panel(c,424,98,198,317);
    label(c,47,120,"AUDIO OUTPUT",2,WHITE);
    label(c,451,120,"QUICK ACCESS",2,WHITE);
    disc(c,214,238,67,rgb(30,30,32));
    ring(c,214,238,63,2,rgb(86,86,92));
    disc(c,214,238,47,CUI_TRACK);
    disc(c,214,238,33,CUI_PANEL);
    disc(c,214,238,20,audio.open?CUI_ACCENT:rgb(90,90,95));
    ring(c,214,238,18,2,audio.open?TEAL:AMBER);
    rect(c,203,224,5,28,CUI_WHITE);
    rect(c,212,216,5,44,CUI_WHITE);
    rect(c,221,229,5,17,CUI_WHITE);
    centered(c,214,159,audio.open?"SOUND SYSTEM READY":
             audio_initializing?"INITIALIZING AUDIO":"PLAYBACK UNAVAILABLE",1,
             audio.open?TEAL:AMBER);
    centered(c,214,309,vqe_restore_requested?"RESTORE REQUESTED":
             vqe_test_process?"SYSTEM VQE: HOOKED":
             "SYSTEM VQE: OFF",1,
             vqe_restore_requested?AMBER:vqe_test_process?AMBER:MUTED);
    draw_meter(c,58,332,325,l,"L");
    draw_meter(c,58,354,325,r,"R");
    centered(c,214,383,test==TEST_DEMO?
             (music_custom_available()?"CUSTOM MUSIC - LOOPING":"BUILT-IN MUSIC - LOOPING"):
             test>=0?test_names[test]:
             (audio.open?(music_custom_available()?"START: CUSTOM MUSIC":"START: BUILT-IN MUSIC"):
              audio_error),1,test>=0?TEAL:MUTED);
    for (i=0;i<3;++i) {
        int y=161+i*74;
        uint32_t border=(i==selected_home)?CUI_ACCENT:CUI_TRACK;
        uint32_t fill=(i==selected_home)?CUI_ACCENT:CUI_TRACK;
        framed_round(c,437,y,172,61,9,border,fill);
        centered(c,523,y+11,names[i],1,WHITE);
        centered(c,523,y+34,sub[i],1,(i==selected_home)?WHITE:MUTED);
    }
}
static void draw_tests(Canvas *c,int current,float l,float r,float progress) {
    int i;
    box_panel(c,18,98,300,317);
    box_panel(c,331,98,291,317);
    label(c,45,120,"TEST SIGNALS",2,WHITE);
    label(c,359,120,"SIGNAL PREVIEW",2,WHITE);
    for (i=0;i<TEST_COUNT;++i) {
        int y=155+i*27;
        bool sel=(i==selected_test);
        if (sel) framed_round(c,32,y-5,273,26,7,CUI_ACCENT,CUI_ACCENT);
        label(c,48,y,test_names[i],1,sel?WHITE:rgb(220,220,224));
        if (i==current) label(c,264,y,"ON",1,sel?WHITE:TEAL);
    }
    centered(c,475,163,test_names[selected_test],2,WHITE);
    centered(c,475,201,selected_test==TEST_DEMO?
             (music_custom_available()?"CUSTOM MP3 LOOP":"BUILT-IN SYNTH LOOP"):
             test_detail[selected_test],1,MUTED);
    centered(c,475,230, current==selected_test?"PLAYING":"A: START / STOP",1,
             current==selected_test?TEAL:rgb(220,220,224));
    round_rect(c,365,261,225,12,5,CUI_TRACK);
    if (current==selected_test && progress>0)
        round_rect(c,365,261,clampi((int)(225*progress),0,225),12,5,TEAL);
    draw_meter(c,363,302,230,l,"L");
    draw_meter(c,363,330,230,r,"R");
    centered(c,475,379,audio.open?
             (selected_test==TEST_DEMO?"MUSIC LOOPS UNTIL STOPPED":
              "SOFT OUTPUT - AUTO STOP"):audio_error,1,
             audio.open?MUTED:AMBER);
}
/* EQ as in the mockup: the profile on a strip, then one column per band
 * (its gain on top, a vertical track and a round knob, its frequency under
 * it); the chosen band's column stands out. */
static void draw_eq(Canvas *c,int current) {
    CuiCanvas *cc=CUI(c);
    int i;
    int gains[EQ_BANDS];
    char fit[128];
    cui_rrect(cc,18,102,604,38,10,CUI_PANEL);
    cui_text(cc,34,121,"PROFILE",13,CUI_GREY,CUI_LEFT);
    cui_text(cc,104,121,profile_names[selected_profile],19,CUI_WHITE,CUI_LEFT);
    /* The hardware setting remains unchanged until the launcher syncs on
       exit. Do not claim a live Flat/EQ toggle while the hook is loaded. */
    {
        const char *state=vqe_test_process?
                  (applied_native_valid?"Live EQ approx":"Native hook"):
                  effective_eq_enabled()?"EQ active":"EQ bypassed";
        char right[80];
        snprintf(right,sizeof(right),"%s - %s",state,
                 music_custom_available()?"custom music":"built-in music");
        cui_text(cc,606,121,right,14,(vqe_test_process||!effective_eq_enabled())?CUI_WARN:CUI_GOOD,CUI_RIGHT);
    }
    if (selected_profile==PROFILE_CUSTOM)
        memcpy(gains,custom_gains,sizeof(gains));
    else memcpy(gains,audiotune_preset_gains[selected_profile],sizeof(gains));
    /* Knob centres map linearly to the full track: +12 at the very top,
       -12 at the very bottom. */
    for (i=0;i<EQ_BANDS;++i) {
        const int track_top=196, track_bottom=344;
        const int zero=(track_top+track_bottom)/2;
        const int travel=(track_bottom-track_top)/2;
        int cx=78+i*121, val=gains[i];
        int knob=zero-(val*travel)/AUDIOTUNE_EQ_GAIN_LIMIT;
        bool chosen=(i==selected_band);
        char value[16];
        if (chosen) cui_rrect(cc,cx-54,150,108,236,12,CUI_PANEL);
        snprintf(value,sizeof(value),val?"%+d":"0",val);
        cui_text(cc,cx,174,value,20,chosen?CUI_WHITE:CUI_GREY,CUI_CENTER);
        cui_rrect(cc,cx-3,track_top,6,track_bottom-track_top,3,CUI_TRACK);
        cui_rect(cc,cx-7,zero,14,1,CUI_DIM);
        if (val>0) cui_rect(cc,cx-3,knob,6,zero-knob,chosen?CUI_ACCENT:CUI_DIM);
        else if (val<0) cui_rect(cc,cx-3,zero,6,knob-zero,chosen?CUI_ACCENT:CUI_DIM);
        if (chosen) {
            cui_disc(cc,cx,knob,11,CUI_WHITE);
            cui_disc(cc,cx,knob,8,CUI_ACCENT);
        } else cui_disc(cc,cx,knob,9,CUI_WHITE);
        cui_text(cc,cx,366,eq_labels[i],14,chosen?CUI_WHITE:CUI_GREY,CUI_CENTER);
    }
    {
        char info[110];
        const char *target=output_mode==AUDIOTUNE_OUTPUT_SPEAKER?"Speaker":"Headphones";
        const char *mode=(output_mode==AUDIOTUNE_OUTPUT_HEADPHONES &&
                          headroom_mode==AUDIOTUNE_HEADROOM_PUNCHY)?"punchy":"safe";
        if (!effective_eq_enabled())
            snprintf(info,sizeof(info),"Output: %s, %s - EQ off - limiter %s",target,mode,
                     soft_limiter_enabled?"on":"off");
        else
            snprintf(info,sizeof(info),"Output: %s, %s - cut -%d dB - limiter %s%s",
                     target,mode,native_headroom_db,soft_limiter_enabled?"on":"off",
                     vqe_test_process?" - applied on exit":"");
        cui_fit(info,14,600,fit,sizeof(fit));
        cui_text(cc,320,404,fit,14,CUI_GREY,CUI_CENTER);
    }
    (void)current;
}
static void draw_profiles(Canvas *c,int current) {
    int i,j;
    int values[EQ_BANDS];
    box_panel(c,18,98,297,317);
    box_panel(c,329,98,293,317);
    label(c,45,120,"SOUND PROFILES",2,WHITE);
    label(c,358,120,profile_cursor==selected_profile?
          "PROFILE DETAILS":(audio_is_playing()?"AUDITIONING":"PREVIEW PROFILE"),2,WHITE);
    for (i=0;i<PROFILE_COUNT;++i) {
        int y=160+i*35;
        bool highlighted=(i==profile_cursor);
        if (highlighted)
            framed_round(c,31,y-7,269,31,8,CUI_ACCENT,CUI_ACCENT);
        label(c,48,y,profile_names[i],1,WHITE);
        if (i==selected_profile) label(c,250,y,"ON",1,highlighted?WHITE:TEAL);
    }
    centered(c,475,162,profile_names[profile_cursor],2,WHITE);
    centered(c,475,203,profile_info[profile_cursor],1,MUTED);
    {
        char info[64];
        const bool headphones=(output_mode==AUDIOTUNE_OUTPUT_HEADPHONES);
        const char *mode=(headphones && headroom_mode==AUDIOTUNE_HEADROOM_PUNCHY)?
                         "PUNCHY":"SAFE";
        /* R2 is a headphones-only control: the speaker is always Safe.
           Keep the actual mode visible even when the native hook is loaded. */
        if (!headphones)
            snprintf(info,sizeof(info),"SAFE - SPEAKER LOCKED  CUT: -%d DB",native_headroom_db);
        else if (vqe_test_process)
            snprintf(info,sizeof(info),"MODE: %s - APPLY ON EXIT",mode);
        else if (!effective_eq_enabled() && profile_cursor==selected_profile)
            snprintf(info,sizeof(info),"MODE: %s - EQ OFF",mode);
        else
            snprintf(info,sizeof(info),"MODE: %s  CUT: -%d DB",mode,native_headroom_db);
        centered(c,475,233,info,1,MUTED);
    }
    if (profile_cursor==PROFILE_CUSTOM) memcpy(values,custom_gains,sizeof(values));
    else memcpy(values,audiotune_preset_gains[profile_cursor],sizeof(values));
    centered(c,475,218,output_mode==AUDIOTUNE_OUTPUT_SPEAKER?
             "OUTPUT: SPEAKER (SAFE BASS)":"OUTPUT: HEADPHONES",1,TEAL);
    for (j=0;j<EQ_BANDS;++j) {
        int cx=373+j*50,mid=299,delta=values[j]*42/AUDIOTUNE_EQ_GAIN_LIMIT;
        char val[16];
        round_rect(c,cx-10,254,20,90,4,CUI_TRACK);
        rect(c,cx-8,mid,16,1,CUI_DIM);
        if (delta>0) round_rect(c,cx-7,mid-delta,14,delta,2,TEAL);
        else if (delta<0) round_rect(c,cx-7,mid,14,-delta,2,AMBER);
        snprintf(val,sizeof(val),"%+d",values[j]);
        centered(c,cx,352,val,1,WHITE);
    }
    centered(c,475,375,current==TEST_DEMO?
             (music_custom_available()?"CUSTOM MUSIC LOOPING":"BUILT-IN MUSIC LOOPING"):
             (music_custom_available()?"START: CUSTOM MUSIC":"START: BUILT-IN MUSIC"),
             1,current==TEST_DEMO?TEAL:MUTED);
    centered(c,475,393,output_mode==AUDIOTUNE_OUTPUT_HEADPHONES?
             "A: APPLY  L2: OUTPUT  R2: MODE":
             "A: APPLY  L2: OUTPUT  R2: HP ONLY",1,MUTED);
}
static void draw(Canvas *c) {
    float l,r,progress;
    int test;
    read_meters(&l,&r,&test,&progress);
    draw_background(c);
    draw_header(c);
    switch(page) {
        case PAGE_HOME: draw_home(c,test,l,r);break;
        case PAGE_TESTS: draw_tests(c,test,l,r,progress);break;
        case PAGE_EQ: draw_eq(c,test);break;
        case PAGE_PROFILES: draw_profiles(c,test);break;
        default:break;
    }
    draw_footer(c);
}
/* Input actions are handled on first keydown; START is committed on keyup
   to avoid toggling playback while entering a SELECT+START exit chord. */
static bool exit_b_armed=false;
/* EQ-only auto-repeat. Schedule ourselves instead of enabling SDL repeat,
   because evdev repeat and SDL fallback behave differently on Miyoo. */
static bool eq_repeat_armed[2]={false,false};
static Uint32 eq_repeat_due[2]={0,0};
#define EQ_REPEAT_DELAY_MS 330u
#define EQ_REPEAT_PERIOD_MS 95u
static void on_key_press(KeyId id) {
    switch(id) {
        case KEY_UP_ID:
            if (page==PAGE_HOME) selected_home=(selected_home+2)%3;
            else if (page==PAGE_TESTS) {
                selected_test=(selected_test+TEST_COUNT-1)%TEST_COUNT;
                if (audio_is_playing() && audio.test!=TEST_DEMO) audio_stop();
            } else if (page==PAGE_EQ) change_eq_gain(selected_band,+1);
            else if (page==PAGE_PROFILES) {
                profile_cursor=(profile_cursor+PROFILE_COUNT-1)%PROFILE_COUNT;
                update_audio_eq();
            }
            break;
        case KEY_DOWN_ID:
            if (page==PAGE_HOME) selected_home=(selected_home+1)%3;
            else if (page==PAGE_TESTS) {
                selected_test=(selected_test+1)%TEST_COUNT;
                if (audio_is_playing() && audio.test!=TEST_DEMO) audio_stop();
            } else if (page==PAGE_EQ) change_eq_gain(selected_band,-1);
            else if (page==PAGE_PROFILES) {
                profile_cursor=(profile_cursor+1)%PROFILE_COUNT;
                update_audio_eq();
            }
            break;
        case KEY_LEFT_ID:
            if (page==PAGE_EQ) selected_band=(selected_band+EQ_BANDS-1)%EQ_BANDS;
            break;
        case KEY_RIGHT_ID:
            if (page==PAGE_EQ) selected_band=(selected_band+1)%EQ_BANDS;
            break;
        case KEY_L2_ID:
            /* The output protection target is independent of the EQ preset. */
            if (page==PAGE_PROFILES || page==PAGE_EQ) {
                output_mode=(output_mode==AUDIOTUNE_OUTPUT_SPEAKER)?
                    AUDIOTUNE_OUTPUT_HEADPHONES:AUDIOTUNE_OUTPUT_SPEAKER;
                dirty_settings=true;
                update_audio_eq();
                save_settings();
            }
            break;
        case KEY_R2_ID:
            if (page==PAGE_PROFILES && output_mode==AUDIOTUNE_OUTPUT_HEADPHONES) {
                /* PUNCHY is headphone-only. Do not change a hidden setting
                   while SPEAKER is selected and its native mode stays Safe. */
                headroom_mode=(headroom_mode==AUDIOTUNE_HEADROOM_SAFE)?
                    AUDIOTUNE_HEADROOM_PUNCHY:AUDIOTUNE_HEADROOM_SAFE;
                dirty_settings=true;
                update_audio_eq();
                save_settings();
            } else if (page==PAGE_EQ) {
                soft_limiter_enabled=!soft_limiter_enabled;
                dirty_settings=true;
                update_audio_eq();
                save_settings();
            }
            break;
        case KEY_L1_ID:
            if (page==PAGE_EQ) save_settings();
            page=(Page)((page+PAGE_COUNT-1)%PAGE_COUNT);
            if (page==PAGE_PROFILES) profile_cursor=selected_profile;
            update_audio_eq();
            break;
        case KEY_R1_ID:
            if (page==PAGE_EQ) save_settings();
            page=(Page)((page+1)%PAGE_COUNT);
            if (page==PAGE_PROFILES) profile_cursor=selected_profile;
            update_audio_eq();
            break;
        case KEY_A_ID:
            if (page==PAGE_HOME) {
                page=(Page)(selected_home+1);
                if (page==PAGE_PROFILES) profile_cursor=selected_profile;
                update_audio_eq();
            }
            else if (page==PAGE_TESTS) audio_toggle(selected_test,selected_test==TEST_DEMO);
            else if (page==PAGE_EQ) {
                eq_enabled=!eq_enabled;
                dirty_settings=true;
                update_audio_eq();
                save_settings();
            }
            else if (page==PAGE_PROFILES) apply_profile(profile_cursor);
            break;
        case KEY_B_ID:
            if (page==PAGE_EQ) save_settings();
            if (page==PAGE_HOME) { exit_b_armed=true; audio_stop(); }
            else {page=PAGE_HOME; update_audio_eq();}
            break;
        case KEY_X_ID:
            if (page==PAGE_EQ) {
                reset_eq_band();
            } else if (page==PAGE_HOME && vqe_test_process) {
                /* Restore on exit, under the same black-screen handshake as EQ.
                   Do not interrupt playback by restarting the server here. */
                audio_stop();
                eq_enabled=false;
                dirty_settings=true;
                save_settings();
                vqe_restore_requested=true;
                quit_requested=true;
            } else if (page==PAGE_PROFILES) {
                if (profile_cursor!=PROFILE_CUSTOM)
                    memcpy(custom_gains,audiotune_preset_gains[profile_cursor],sizeof(custom_gains));
                selected_profile=PROFILE_CUSTOM;
                profile_cursor=PROFILE_CUSTOM;
                eq_enabled=true;
                dirty_settings=true;
                update_audio_eq();
                save_settings();
            }
            break;
        case KEY_Y_ID:
            if (page==PAGE_EQ) reset_eq_all();
            break;
        default: break;
    }
}
static void change_key(KeyId id,bool down) {
    bool was=keys[id].down;
    keys[id].down=down;
    if (was==down) return;
    if (id==KEY_UP_ID || id==KEY_DOWN_ID) {
        int dir=(id==KEY_UP_ID)?0:1;
        if (down && page==PAGE_EQ && !exit_chord_armed) {
            eq_repeat_armed[dir]=true;
            eq_repeat_due[dir]=SDL_GetTicks()+EQ_REPEAT_DELAY_MS;
        } else {
            if (!down && eq_repeat_armed[dir]) save_settings();
            eq_repeat_armed[dir]=false;
        }
    }
    if (keys[KEY_SELECT_ID].down && keys[KEY_START_ID].down &&
        !exit_chord_armed) {
        exit_chord_armed=true;
        audio_stop();
        LOG("SELECT+START chord; waiting for both releases");
    }
    if (exit_chord_armed) {
        if (!keys[KEY_SELECT_ID].down && !keys[KEY_START_ID].down)
            quit_requested=true;
        return;
    }
    if (exit_b_armed) {
        if (!keys[KEY_B_ID].down) quit_requested=true;
        return;
    }
    if (id==KEY_START_ID) {
        if (!down && !keys[KEY_SELECT_ID].down) {
            if (page==PAGE_EQ || page==PAGE_PROFILES)
                audio_toggle(TEST_DEMO,true);
            else if (page==PAGE_HOME)
                audio_toggle(TEST_DEMO,true);
            else {
                eq_enabled=!eq_enabled;
                dirty_settings=true;
                update_audio_eq();
                save_settings();
            }
        }
    } else if (down && id!=KEY_SELECT_ID) {
        on_key_press(id);
    }
}
static void repeat_eq_held_keys(Uint32 now) {
    int i;
    if (page!=PAGE_EQ || exit_chord_armed || exit_b_armed) {
        eq_repeat_armed[0]=eq_repeat_armed[1]=false;
        return;
    }
    for (i=0;i<2;++i) {
        KeyId key=i==0?KEY_UP_ID:KEY_DOWN_ID;
        if (!eq_repeat_armed[i] || !keys[key].down) continue;
        if ((int32_t)(now-eq_repeat_due[i])<0) continue;
        change_eq_gain(selected_band,i==0?+1:-1);
        eq_repeat_due[i]=now+EQ_REPEAT_PERIOD_MS;
    }
}
static void read_evdev(void) {
    struct input_event e;
    ssize_t size=0;
    while ((size=read(input_fd,&e,sizeof(e)))==(ssize_t)sizeof(e)) {
        int i;
        if (e.type!=EV_KEY || e.value==2) continue;
        for(i=0;i<AUDIO_KEY_COUNT;++i)
            if (keys[i].linux_code==e.code) {
                change_key((KeyId)i,e.value==1);
                break;
            }
    }
    if (size==0 || (size<0 && errno!=EAGAIN && errno!=EINTR)) {
        LOG("evdev input disconnected; falling back to SDL");
        close(input_fd); input_fd=-1; raw_input=false;
        {
            int i;
            for(i=0;i<AUDIO_KEY_COUNT;++i) keys[i].down=false;
            eq_repeat_armed[0]=eq_repeat_armed[1]=false;
            save_settings();
        }
    }
}
static void read_sdl(const SDL_Event *event) {
    int i;
    if (raw_input || (event->type!=SDL_KEYDOWN && event->type!=SDL_KEYUP)) return;
    for(i=0;i<AUDIO_KEY_COUNT;++i)
        if (keys[i].sdl_code == event->key.keysym.sym) {
            change_key((KeyId)i,event->type==SDL_KEYDOWN);
            break;
        }
}
/* The launcher runs the slow audioserver refresh while the app still owns
   its SDL framebuffer. The black screen remains visible until it completes.
   Handshakes use unique files in /tmp; a missing launcher cannot trap exit. */
static void signal_file(const char *path,const char *message) {
    FILE *f;
    if (!path || strncmp(path,"/tmp/audiotune-",15)!=0) return;
    f=fopen(path,"w");
    if (f) { fputs(message,f); fclose(f); }
}
static void await_audio_apply(SDL_Surface *screen,uint32_t *pixels) {
    const char *request=getenv("AUDIOTUNE_APPLY_REQUEST");
    const char *decision=getenv("AUDIOTUNE_APPLY_DECISION");
    const char *ready=getenv("AUDIOTUNE_APPLY_READY");
    const char *done=getenv("AUDIOTUNE_APPLY_DONE");
    Uint32 begin;
    char reply[16]={0};
    FILE *f;
    if (!screen || !pixels || !request || !decision || !ready || !done ||
        strncmp(request,"/tmp/audiotune-",15)!=0 ||
        strncmp(decision,"/tmp/audiotune-",15)!=0) return;
    signal_file(request,"EXIT\n");
    begin=SDL_GetTicks();
    while((Uint32)(SDL_GetTicks()-begin)<1800u && !interrupted) {
        f=fopen(decision,"r");
        if (f) { if(fgets(reply,sizeof(reply),f)==NULL) reply[0]=0; fclose(f); break; }
        SDL_Delay(20);
    }
    if (strncmp(reply,"APPLY",5)!=0) return;
    {
        Canvas canvas={pixels,W};
        rect(&canvas,0,0,W,H,CUI_BG);
        centered(&canvas,320,202,"APPLYING AUDIO SETTINGS",2,CUI_WHITE);
        centered(&canvas,320,254,"THE SPEAKER MAY CLICK BRIEFLY",1,CUI_GREY);
        centered(&canvas,320,279,"RETURNING TO MAIN MENU",1,CUI_GREY);
        present(screen,pixels);
    }
    SDL_Delay(140); /* Ensure at least one visible frame before audio restarts. */
    signal_file(ready,"READY\n");
    begin=SDL_GetTicks();
    while((Uint32)(SDL_GetTicks()-begin)<8500u && !interrupted) {
        if (access(done,F_OK)==0) break;
        SDL_Delay(30);
    }
}
int main(int argc,char **argv) {
    SDL_Surface *screen=NULL;
    uint32_t *pixels=NULL;
    SDL_Event event;
    Uint32 frame_tick=0, status_tick=0;
    bool frame_ok=true;
    int rc=1;
    double start_ms=timing_now_ms(), stage_ms=start_ms;
    (void)argc;(void)argv;
    signal(SIGINT,on_signal);
    signal(SIGTERM,on_signal);
    signal(SIGHUP,on_signal);
    LOG("AudioTune v0.2.9 beta startup");
    vqe_status_path=getenv("AUDIOTUNE_VQE_STATE");
    applied_state_path=getenv("AUDIOTUNE_APPLIED_STATE");
    update_vqe_test_state();
    initialize_wave_table();
    load_settings();
    mp3_candidate=(access("sample.mp3",R_OK)==0);
    LOG("music: sample.mp3 %s; loading deferred until START",
        mp3_candidate?"found":"not found");
    update_audio_eq();
    LOG("timing: settings, tables and music probe %.0f ms",timing_now_ms()-stage_ms);
    stage_ms=timing_now_ms();
    input_fd=open("/dev/input/event0",O_RDONLY|O_NONBLOCK);
    raw_input=input_fd>=0;
    if (!raw_input) {
        LOG("evdev unavailable (%s): SDL fallback A=Space B=LCtrl SELECT=RCtrl",
            strerror(errno));
    }
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0) {
        LOG("SDL_Init video: %s",SDL_GetError());
        goto done;
    }
    LOG("timing: SDL video/timer init %.0f ms",timing_now_ms()-stage_ms);
    stage_ms=timing_now_ms();
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(0,0);
    screen=SDL_SetVideoMode(W,H,32,SDL_SWSURFACE);
    if (!screen) screen=SDL_SetVideoMode(W,H,16,SDL_SWSURFACE);
    if (!screen || !screen->format) {
        LOG("SDL_SetVideoMode: %s",SDL_GetError());
        goto done_sdl;
    }
    pixels=(uint32_t *)calloc((size_t)W*H,sizeof(*pixels));
    if (!pixels) {
        LOG("failed to allocate display buffer");
        goto done_sdl;
    }
    LOG("timing: video mode and buffers %.0f ms",timing_now_ms()-stage_ms);
    stage_ms=timing_now_ms();
    /* Put the UI on screen before the potentially slow SigmaStar audio init. */
    snprintf(audio_error,sizeof(audio_error),"INITIALIZING AUDIO");
    audio_initializing=true;
    { Canvas canvas={pixels,W}; draw(&canvas); present(screen,pixels); }
    LOG("timing: first UI frame %.0f ms since app entry",timing_now_ms()-start_ms);
    audio_initialize();
    audio_initializing=false;
    LOG("timing: SDL/audio subsystem %.0f ms; app ready %.0f ms since entry",
        timing_now_ms()-stage_ms,timing_now_ms()-start_ms);
    while(!interrupted && !quit_requested) {
        Uint32 now=SDL_GetTicks();
        if (now-status_tick>=500u || !status_tick) {
            update_vqe_test_state();
            if (!vqe_test_process) vqe_restore_requested=false;
            status_tick=now?now:1;
        }
        while(SDL_PollEvent(&event)) {
            if (event.type==SDL_QUIT) quit_requested=true;
            else read_sdl(&event);
        }
        if (input_fd>=0) read_evdev();
        repeat_eq_held_keys(now);
        if (now-frame_tick>=(audio_is_playing()?48u:130u) || !frame_tick) {
            Canvas canvas={pixels,W};
            draw(&canvas);
            if (!present(screen,pixels)) { frame_ok=false; break; }
            frame_tick=now?now:1;
        }
        SDL_Delay(15);
    }
    if (interrupted||quit_requested) rc=0;
    LOG("leaving UI; quit=%d signal=%d, video_ok=%d",
        quit_requested,(int)interrupted,frame_ok);
    audio_shutdown();
    audiotune_mp3_close(&music_mp3);
    save_settings();
    if (rc==0 && !interrupted && frame_ok) await_audio_apply(screen,pixels);
    free(pixels);
done_sdl:
    SDL_Quit();
done:
    if (input_fd>=0) close(input_fd);
    LOG("shutdown result=%d",rc);
    return rc;
}
