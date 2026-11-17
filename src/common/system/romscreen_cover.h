#ifndef ROMSCREEN_COVER_H__
#define ROMSCREEN_COVER_H__

// The GameSwitcher screenshot as a game's cover, when it has none.
//
// Tweaks > Game lists > Previews > "Screenshot as missing cover" (the flag
// below). When a game is left (MENU, GameSwitcher), keymon saves the frame
// as the GameSwitcher picture; with the option on and no picture yet in the
// console's Imgs folder, the same frame is saved there too, as
// Roms/<console>/Imgs/<ROM name>.png, the name Open MainUI looks for.
// An existing picture (scraped box art, one added by hand, or one made here
// earlier) is never replaced: delete it to get a new one.
//
// The frame is shrunk to the cover box (250 wide, at most 360 high; Open
// MainUI only ever shrinks), with black borders around the game cut off
// first, so a Game Boy screen fills the cover instead of sitting small in
// the middle of a black 640x480 frame.
//
// Ideas: VisitOtherwise1557 (Reddit), robcodedev.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define ROMSCREEN_COVER_FLAG ".romscreenCover"
#define ROMSCREEN_COVER_MAX_W 250
#define ROMSCREEN_COVER_MAX_H 360

#ifndef ROMSCREEN_COVER_ROMS
#define ROMSCREEN_COVER_ROMS "/mnt/SDCARD/Roms"
#endif

// The cover path for a ROM, from its path as the recent list holds it
// (e.g. /mnt/SDCARD/Emu/GB/../../Roms/GB/sub/Tetris.gb ->
// /mnt/SDCARD/Roms/GB/Imgs/Tetris.png; Open MainUI looks for the ROM's name
// in its console's Imgs folder, whatever subfolder the ROM is in).
// False for a path outside Roms/<console>/, and for PICO-8: its images are
// the carts (.p8.png) in the ROM folder, where a new .png would be listed as
// a game.
static inline bool romscreen_cover_path(const char *rom_path, char *out, size_t size)
{
    if (rom_path == NULL || out == NULL || size == 0)
        return false;

    // The first /Roms/: the SD card's. A later one is a folder inside a
    // console's (Roms/GB/Roms/x.gb is still a GB game).
    const char *rel = strstr(rom_path, "/Roms/");
    if (rel == NULL)
        return false;
    rel += strlen("/Roms/");

    const char *slash = strchr(rel, '/');
    if (slash == NULL || slash == rel || slash[1] == '\0')
        return false; // not inside a console folder
    size_t console_len = (size_t)(slash - rel);
    if (console_len == 4 && strncasecmp(rel, "PICO", 4) == 0)
        return false;

    const char *name = strrchr(rom_path, '/') + 1;
    size_t name_len = strlen(name);
    const char *dot = strrchr(name, '.');
    if (dot != NULL && dot != name) {
        if (strcasecmp(dot, ".png") == 0)
            return false; // a picture as a ROM (PICO-8 carts elsewhere)
        name_len = (size_t)(dot - name);
    }
    if (name_len == 0)
        return false;

    int n = snprintf(out, size, "%s/%.*s/Imgs/%.*s.png", ROMSCREEN_COVER_ROMS,
                     (int)console_len, rel, (int)name_len, name);
    return n > 0 && (size_t)n < size;
}

// Pixel (x, y) of the picture as seen, in a frame buffer that may be
// upside down (the Miyoo's is: screenshots are rotated 180 degrees).
static inline uint32_t _romscreen_cover_px(const uint32_t *src, int w, int h, int stride,
                                           bool rotate180, int x, int y)
{
    if (rotate180) {
        x = w - 1 - x;
        y = h - 1 - y;
    }
    return src[(size_t)y * (size_t)stride + (size_t)x];
}

static inline bool _romscreen_cover_dark(uint32_t argb)
{
    int r = (argb >> 16) & 0xFF, g = (argb >> 8) & 0xFF, b = argb & 0xFF;
    return r < 16 && g < 16 && b < 16;
}

static inline bool _romscreen_cover_row_dark(const uint32_t *src, int w, int h, int stride,
                                      bool rotate180, int y)
{
    int lit = 0;
    for (int x = 0; x < w; x++)
        lit += !_romscreen_cover_dark(_romscreen_cover_px(src, w, h, stride, rotate180, x, y));
    return lit <= w / 64;
}

static inline bool _romscreen_cover_col_dark(const uint32_t *src, int w, int h, int stride,
                                      bool rotate180, int x, int y0, int y1)
{
    int lit = 0;
    for (int y = y0; y < y1; y++)
        lit += !_romscreen_cover_dark(_romscreen_cover_px(src, w, h, stride, rotate180, x, y));
    return lit <= (y1 - y0) / 64;
}

// The part of the picture inside its black borders: a row or column is a
// border while nearly all of it (all but 1/64) is near black. Used only when
// it keeps at least a quarter of the width and of the height; a dark game
// screen (a night level) then stays whole.
static inline void romscreen_cover_content(const uint32_t *src, int w, int h, int stride,
                                    bool rotate180, int *x0, int *y0, int *x1, int *y1)
{
    *x0 = 0;
    *y0 = 0;
    *x1 = w;
    *y1 = h;
    if (src == NULL || w <= 0 || h <= 0)
        return;

    int top = 0, bottom = h, left = 0, right = w;
    while (top < bottom && _romscreen_cover_row_dark(src, w, h, stride, rotate180, top))
        top++;
    while (bottom > top && _romscreen_cover_row_dark(src, w, h, stride, rotate180, bottom - 1))
        bottom--;
    while (left < right &&
           _romscreen_cover_col_dark(src, w, h, stride, rotate180, left, top, bottom))
        left++;
    while (right > left &&
           _romscreen_cover_col_dark(src, w, h, stride, rotate180, right - 1, top, bottom))
        right--;

    if ((right - left) * 4 < w || (bottom - top) * 4 < h)
        return;
    *x0 = left;
    *y0 = top;
    *x1 = right;
    *y1 = bottom;
}

// The cover size for a picture of w x h: 250 wide, or less to stay within
// 360 high, keeping its shape; never larger than the picture itself.
static inline void romscreen_cover_size(int w, int h, int *out_w, int *out_h)
{
    int cw = w < ROMSCREEN_COVER_MAX_W ? w : ROMSCREEN_COVER_MAX_W;
    int ch = (int)((int64_t)h * cw / w);
    if (ch > ROMSCREEN_COVER_MAX_H) {
        ch = ROMSCREEN_COVER_MAX_H;
        cw = (int)((int64_t)w * ch / h);
    }
    *out_w = cw > 0 ? cw : 1;
    *out_h = ch > 0 ? ch : 1;
}

// The cover picture (tightly packed ARGB, upright), or NULL. Each cover
// pixel is the average of the frame pixels it covers, so text and sprites
// stay readable when shrunk (no skipped rows). Free with free().
static inline uint32_t *romscreen_cover_make(const uint32_t *src, int w, int h, int stride,
                                      bool rotate180, int *out_w, int *out_h)
{
    if (src == NULL || w <= 0 || h <= 0 || out_w == NULL || out_h == NULL)
        return NULL;
    if (stride < w)
        stride = w;

    int x0, y0, x1, y1;
    romscreen_cover_content(src, w, h, stride, rotate180, &x0, &y0, &x1, &y1);
    int cw = x1 - x0, ch = y1 - y0;

    int dw, dh;
    romscreen_cover_size(cw, ch, &dw, &dh);
    uint32_t *dst = (uint32_t *)malloc((size_t)dw * (size_t)dh * sizeof(uint32_t));
    if (dst == NULL)
        return NULL;

    for (int dy = 0; dy < dh; dy++) {
        int sy0 = y0 + (int)((int64_t)dy * ch / dh);
        int sy1 = y0 + (int)((int64_t)(dy + 1) * ch / dh);
        if (sy1 <= sy0)
            sy1 = sy0 + 1;
        for (int dx = 0; dx < dw; dx++) {
            int sx0 = x0 + (int)((int64_t)dx * cw / dw);
            int sx1 = x0 + (int)((int64_t)(dx + 1) * cw / dw);
            if (sx1 <= sx0)
                sx1 = sx0 + 1;
            uint32_t r = 0, g = 0, b = 0, count = 0;
            for (int sy = sy0; sy < sy1; sy++) {
                for (int sx = sx0; sx < sx1; sx++) {
                    uint32_t p = _romscreen_cover_px(src, w, h, stride, rotate180, sx, sy);
                    r += (p >> 16) & 0xFF;
                    g += (p >> 8) & 0xFF;
                    b += p & 0xFF;
                    count++;
                }
            }
            dst[(size_t)dy * (size_t)dw + (size_t)dx] =
                0xFF000000u | ((r / count) << 16) | ((g / count) << 8) | (b / count);
        }
    }
    *out_w = dw;
    *out_h = dh;
    return dst;
}

#endif // ROMSCREEN_COVER_H__
