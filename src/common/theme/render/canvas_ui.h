#ifndef RENDER_CANVAS_UI_H__
#define RENDER_CANVAS_UI_H__

// Unified RedOnion look (test branch) for apps that draw into their own
// 640x480 32-bit RGB buffer instead of SDL surfaces: SD Card Doctor and
// AudioTune. Same pieces as Tweaks: dark header with an accent badge and the
// title on the left, battery on the right, flat panels, accent selection,
// round button hints, Exo 2 text (SDL_ttf).
//
// Everything is static inline so an app includes only what it uses.
// Pixels are 0x00RRGGBB, like those apps' Canvas.

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "./ui_icons_data.h"

#define CUI_W 640
#define CUI_H 480

#define CUI_RGB(r, g, b) ((uint32_t)(((r) << 16) | ((g) << 8) | (b)))
#define CUI_BG CUI_RGB(28, 28, 30)
#define CUI_HEAD CUI_RGB(30, 30, 30)
#define CUI_PANEL CUI_RGB(46, 46, 49)
#define CUI_TRACK CUI_RGB(58, 58, 61)
#define CUI_ACCENT CUI_RGB(158, 44, 69)
#define CUI_WHITE CUI_RGB(244, 244, 245)
#define CUI_GREY CUI_RGB(163, 163, 168)
#define CUI_DIM CUI_RGB(110, 110, 115)
#define CUI_GOOD CUI_RGB(126, 201, 152)
#define CUI_WARN CUI_RGB(242, 179, 91)
#define CUI_BAD CUI_RGB(240, 120, 130)

#ifndef CUI_FONT_PATH
#define CUI_FONT_PATH "/customer/app/Exo-2-Bold-Italic.ttf"
#endif
#ifndef CUI_FONT_FALLBACK
#define CUI_FONT_FALLBACK "/mnt/SDCARD/miyoo/app/Exo-2-Bold-Italic_Universal.ttf"
#endif

typedef struct {
    uint32_t *px;
    int pitch;
} CuiCanvas;

static inline uint32_t cui_mix(uint32_t a, uint32_t b, float t)
{
    if (t <= 0)
        return a;
    if (t >= 1)
        return b;
    unsigned ar = a >> 16 & 255, ag = a >> 8 & 255, ab = a & 255;
    unsigned br = b >> 16 & 255, bg = b >> 8 & 255, bb = b & 255;
    return CUI_RGB((unsigned)(ar + (br - (float)ar) * t + 0.5f),
                   (unsigned)(ag + (bg - (float)ag) * t + 0.5f),
                   (unsigned)(ab + (bb - (float)ab) * t + 0.5f));
}

static inline void cui_blend(CuiCanvas *c, int x, int y, uint32_t color, float a)
{
    if ((unsigned)x >= CUI_W || (unsigned)y >= CUI_H || a <= 0)
        return;
    uint32_t *p = c->px + y * c->pitch + x;
    *p = cui_mix(*p, color, a);
}

/* Rounded rectangle with anti-aliased edges (signed distance). */
static inline void cui_rrect(CuiCanvas *c, float x, float y, float w, float h, float r, uint32_t color)
{
    if (w <= 0 || h <= 0)
        return;
    if (r > w / 2)
        r = w / 2;
    if (r > h / 2)
        r = h / 2;
    int x0 = (int)floorf(x), y0 = (int)floorf(y), x1 = (int)ceilf(x + w), y1 = (int)ceilf(y + h);
    float cx = x + w / 2, cy = y + h / 2, hw = w / 2 - r, hh = h / 2 - r;
    for (int yy = y0; yy < y1; yy++) {
        if ((unsigned)yy >= CUI_H)
            continue;
        for (int xx = x0; xx < x1; xx++) {
            float dx = fabsf(xx + 0.5f - cx) - hw, dy = fabsf(yy + 0.5f - cy) - hh;
            float ox = dx > 0 ? dx : 0, oy = dy > 0 ? dy : 0;
            float d = sqrtf(ox * ox + oy * oy) + (dx > dy ? (dx < 0 ? dx : 0) : (dy < 0 ? dy : 0)) - r;
            float a = 0.5f - d;
            cui_blend(c, xx, yy, color, a > 1 ? 1 : a);
        }
    }
}

static inline void cui_rect(CuiCanvas *c, int x, int y, int w, int h, uint32_t color)
{
    for (int yy = y < 0 ? 0 : y; yy < y + h && yy < CUI_H; yy++)
        for (int xx = x < 0 ? 0 : x; xx < x + w && xx < CUI_W; xx++)
            c->px[yy * c->pitch + xx] = color;
}

static inline void cui_disc(CuiCanvas *c, float cx, float cy, float r, uint32_t color)
{
    cui_rrect(c, cx - r, cy - r, 2 * r, 2 * r, r, color);
}

static inline void cui_ring(CuiCanvas *c, float cx, float cy, float r, float width, uint32_t color)
{
    for (int yy = (int)(cy - r - 2); yy <= (int)(cy + r + 2); yy++)
        for (int xx = (int)(cx - r - 2); xx <= (int)(cx + r + 2); xx++) {
            float d = fabsf(hypotf(xx + 0.5f - cx, yy + 0.5f - cy) - r) - width / 2;
            float a = 0.5f - d;
            cui_blend(c, xx, yy, color, a > 1 ? 1 : a);
        }
}

/* Thick line with round ends. */
static inline void cui_line(CuiCanvas *c, float x0, float y0, float x1, float y1, float width, uint32_t color)
{
    float r = width / 2, vx = x1 - x0, vy = y1 - y0, len2 = vx * vx + vy * vy;
    int bx0 = (int)floorf(fminf(x0, x1) - r - 1), bx1 = (int)ceilf(fmaxf(x0, x1) + r + 1);
    int by0 = (int)floorf(fminf(y0, y1) - r - 1), by1 = (int)ceilf(fmaxf(y0, y1) + r + 1);
    for (int yy = by0; yy <= by1; yy++)
        for (int xx = bx0; xx <= bx1; xx++) {
            float px = xx + 0.5f - x0, py = yy + 0.5f - y0;
            float t = len2 > 0 ? (px * vx + py * vy) / len2 : 0;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            float d = hypotf(px - t * vx, py - t * vy) - r;
            float a = 0.5f - d;
            cui_blend(c, xx, yy, color, a > 1 ? 1 : a);
        }
}

static inline void cui_chevron(CuiCanvas *c, float x, float cy, int dir, uint32_t color)
{
    cui_line(c, x - 3.5f * dir, cy - 7, x + 3.5f * dir, cy, 3, color);
    cui_line(c, x + 3.5f * dir, cy, x - 3.5f * dir, cy + 7, 3, color);
}

/* ---- text (Exo 2, SDL_ttf) ---- */

typedef struct {
    int size;
    TTF_Font *font;
    int cap_top, cap_h; /* where the top and the height of a capital are */
} CuiFont;

static CuiFont cui_fonts[16];
static bool cui_text_ok = true;

static inline CuiFont *cui_font(int size)
{
    for (int i = 0; i < (int)(sizeof(cui_fonts) / sizeof(cui_fonts[0])); i++)
        if (cui_fonts[i].size == size)
            return cui_fonts[i].font ? &cui_fonts[i] : NULL;
    if (!cui_text_ok)
        return NULL;
    if (!TTF_WasInit() && TTF_Init() < 0) {
        cui_text_ok = false;
        return NULL;
    }
    for (int i = 0; i < (int)(sizeof(cui_fonts) / sizeof(cui_fonts[0])); i++) {
        if (cui_fonts[i].size)
            continue;
        CuiFont *f = &cui_fonts[i];
        f->size = size;
        f->font = TTF_OpenFont(CUI_FONT_PATH, size);
        if (!f->font)
            f->font = TTF_OpenFont(CUI_FONT_FALLBACK, size);
        if (!f->font) {
            cui_text_ok = false;
            return NULL;
        }
        /* measure a capital once, to centre text on its capitals */
        SDL_Surface *s = TTF_RenderUTF8_Blended(f->font, "H", (SDL_Color){255, 255, 255, 0});
        f->cap_top = size / 4;
        f->cap_h = size * 7 / 10;
        if (s) {
            int top = -1, bottom = -1;
            SDL_PixelFormat *fmt = s->format;
            for (int y = 0; y < s->h; y++)
                for (int x = 0; x < s->w; x++) {
                    uint32_t v = *(uint32_t *)((uint8_t *)s->pixels + y * s->pitch + x * 4);
                    if (((v & fmt->Amask) >> fmt->Ashift) > 128) {
                        if (top < 0)
                            top = y;
                        bottom = y;
                    }
                }
            if (top >= 0) {
                f->cap_top = top;
                f->cap_h = bottom - top + 1;
            }
            SDL_FreeSurface(s);
        }
        return f;
    }
    return NULL;
}

static inline int cui_text_width(const char *s, int size)
{
    CuiFont *f = cui_font(size);
    int w = 0;
    if (f && s && *s)
        TTF_SizeUTF8(f->font, s, &w, NULL);
    return w;
}

enum { CUI_LEFT, CUI_CENTER, CUI_RIGHT };

/* Draws s with its capitals centred on cy; returns its width (0 without
 * the font: the caller can fall back to its own bitmap font). */
static inline int cui_text(CuiCanvas *c, int x, float cy, const char *s, int size, uint32_t color, int align)
{
    CuiFont *f = cui_font(size);
    if (!f || !s || !*s)
        return 0;
    SDL_Surface *t = TTF_RenderUTF8_Blended(f->font, s, (SDL_Color){color >> 16 & 255, color >> 8 & 255, color & 255, 0});
    if (!t)
        return 0;
    int w = 0;
    TTF_SizeUTF8(f->font, s, &w, NULL);
    int ox = align == CUI_CENTER ? x - w / 2 : align == CUI_RIGHT ? x - w : x;
    int oy = (int)(cy - f->cap_top - f->cap_h / 2.0f + 0.5f);
    SDL_PixelFormat *fmt = t->format;
    for (int y = 0; y < t->h; y++)
        for (int xx = 0; xx < t->w; xx++) {
            uint32_t v = *(uint32_t *)((uint8_t *)t->pixels + y * t->pitch + xx * 4);
            unsigned a = (v & fmt->Amask) >> fmt->Ashift;
            if (a)
                cui_blend(c, ox + xx, oy + y, color, a / 255.0f);
        }
    SDL_FreeSurface(t);
    return w;
}

/* Cuts s with "..." so it fits in max_w pixels at size. */
static inline void cui_fit(const char *s, int size, int max_w, char *out, size_t out_size)
{
    size_t n = strlen(s);
    if (n >= out_size)
        n = out_size - 1;
    memcpy(out, s, n);
    out[n] = 0;
    if (cui_text_width(out, size) <= max_w)
        return;
    while (n > 0) {
        n--;
        if (n + 4 > out_size)
            continue;
        memcpy(out + n, "...", 4);
        if (cui_text_width(out, size) <= max_w)
            return;
    }
}

/* ---- shared screen pieces ---- */

static inline void cui_battery(CuiCanvas *c, int right, int percent)
{
    char s[8];
    snprintf(s, sizeof s, "%d%%", percent < 0 ? 0 : percent > 100 ? 100 : percent);
    int w = cui_text(c, right, 30, s, 18, CUI_WHITE, CUI_RIGHT);
    float x = right - w - 10;
    cui_rrect(c, x - 13, 17, 13, 26, 5, CUI_WHITE);
    cui_rrect(c, x - 11, 19, 9, 22, 3.5f, CUI_HEAD);
    float level = 18.0f * (percent < 0 ? 0 : percent > 100 ? 100 : percent) / 100.0f;
    if (level > 2)
        cui_rrect(c, x - 9.5f, 39.5f - level, 6, level, 2, CUI_WHITE);
}

/* One of the header icons (res/icons/ui), drawn in white, centred on cx,cy.
 * Returns false when there is no icon by that name. */
static inline bool cui_icon(CuiCanvas *c, float cx, float cy, const char *name, uint32_t color)
{
    for (unsigned i = 0; i < sizeof(ui_icon_masks) / sizeof(ui_icon_masks[0]); i++) {
        if (strcmp(ui_icon_masks[i].name, name) != 0)
            continue;
        int x0 = (int)(cx - UI_ICON_SIZE / 2.0f + 0.5f), y0 = (int)(cy - UI_ICON_SIZE / 2.0f + 0.5f);
        for (int y = 0; y < UI_ICON_SIZE; y++)
            for (int x = 0; x < UI_ICON_SIZE; x++) {
                unsigned char a = ui_icon_masks[i].a[y * UI_ICON_SIZE + x];
                if (a)
                    cui_blend(c, x0 + x, y0 + y, color, a / 255.0f);
            }
        return true;
    }
    return false;
}

/* The colour of a status word: green when ready, amber while working, red on
 * an error, grey otherwise (the same everywhere). */
static inline uint32_t cui_status_color(const char *status)
{
    char low[48];
    size_t n = 0;
    for (; status[n] && n < sizeof(low) - 1; n++)
        low[n] = (status[n] >= 'A' && status[n] <= 'Z') ? status[n] + 32 : status[n];
    low[n] = '\0';
    if (strstr(low, "error") || strstr(low, "fail") || strstr(low, "no card") ||
        strstr(low, "offline") || strstr(low, "read only"))
        return CUI_BAD;
    if ((strstr(low, "ing") && !strstr(low, "setting")) || strstr(low, "not saved") || strstr(low, "change"))
        return CUI_WARN;
    if (strstr(low, "ready") || strstr(low, "live") || strstr(low, "mounted") || strstr(low, "in use") ||
        strstr(low, "ok") || strstr(low, "done"))
        return CUI_GOOD;
    return CUI_GREY;
}

/* Status chip: the word in capitals, outlined in its colour. Drawn with its
 * right edge at `right`; returns its width. */
static inline int cui_chip(CuiCanvas *c, int right, float cy, const char *status)
{
    char up[48];
    size_t n = 0;
    for (; status[n] && n < sizeof(up) - 1; n++)
        up[n] = (status[n] >= 'a' && status[n] <= 'z') ? status[n] - 32 : status[n];
    up[n] = '\0';
    uint32_t col = cui_status_color(status);
    int w = cui_text_width(up, 12) + 20;
    cui_rrect(c, right - w, cy - 11, w, 22, 11, col);
    cui_rrect(c, right - w + 1.5f, cy - 9.5f, w - 3, 19, 9.5f, CUI_HEAD);
    cui_text(c, right - w / 2, cy, up, 12, col, CUI_CENTER);
    return w;
}

/* Header like Tweaks: badge with the app's icon (`icon` is an icon name, or
 * a single letter), title; status chip and battery right. battery < 0 hides
 * it. */
static inline void cui_header(CuiCanvas *c, const char *icon, const char *title, const char *status, int battery)
{
    cui_rect(c, 0, 0, CUI_W, 60, CUI_HEAD);
    cui_rrect(c, 16, 14, 32, 32, 8, CUI_ACCENT);
    if (strlen(icon) <= 1 || !cui_icon(c, 32, 30, icon, CUI_WHITE))
        cui_text(c, 32, 30, icon, 22, CUI_WHITE, CUI_CENTER);
    cui_text(c, 60, 30, title, 25, CUI_WHITE, CUI_LEFT);
    int right = 624;
    if (battery >= 0) {
        cui_battery(c, right, battery);
        right -= cui_text_width("100%", 18) + 36;
    }
    if (status && *status)
        cui_chip(c, right, 30, status);
}

/* One button hint: accent circle (or pill for SELECT/START) and its label.
 * Returns the width used, with the gap after it. */
static inline int cui_hint(CuiCanvas *c, int x, float cy, const char *button, const char *label, int label_size)
{
    int w;
    if (strlen(button) > 1) {
        w = cui_text_width(button, 14) + 18;
        cui_rrect(c, x, cy - 12, w, 24, 12, CUI_ACCENT);
        cui_text(c, x + w / 2, cy, button, 14, CUI_WHITE, CUI_CENTER);
    }
    else {
        w = 28;
        cui_disc(c, x + 14, cy, 14, CUI_ACCENT);
        cui_text(c, x + 14, cy, button, 18, CUI_WHITE, CUI_CENTER);
    }
    int lw = cui_text(c, x + w + 7, cy, label, label_size, CUI_WHITE, CUI_LEFT);
    return w + 7 + lw + 20;
}

static inline void cui_progress(CuiCanvas *c, float x, float y, float w, float h, float fraction)
{
    fraction = fraction < 0 ? 0 : fraction > 1 ? 1 : fraction;
    cui_rrect(c, x, y, w, h, h / 2, CUI_TRACK);
    if (fraction > 0)
        cui_rrect(c, x, y, fmaxf(h, w * fraction), h, h / 2, CUI_ACCENT);
}

#endif // RENDER_CANVAS_UI_H__
