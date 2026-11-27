#ifndef RENDER_UNIFIED_H__
#define RENDER_UNIFIED_H__

// Unified look (test branch): the shapes the RedOnion apps share, drawn by
// code with the active theme's own colours, so every theme keeps its colours:
// - accent: the selected row (the middle of skin/bg-list-s.png),
// - bar: the header (the middle of skin/bg-title.png).
// Rounded boxes, the badge before the title, the on/off switch and the
// chevrons are drawn here instead of taken from theme images.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "theme/config.h"
#include "theme/resources.h"

#define UI_GREY_R 163
#define UI_GREY_G 163
#define UI_GREY_B 168

static inline Uint32 _ui_getPixel(SDL_Surface *s, int x, int y)
{
    if (s == NULL || x < 0 || y < 0 || x >= s->w || y >= s->h)
        return 0;
    Uint8 bpp = s->format->BytesPerPixel;
    Uint8 *p = (Uint8 *)s->pixels + y * s->pitch + x * bpp;
    switch (bpp) {
    case 1:
        return *p;
    case 2:
        return *(Uint16 *)p;
    case 3:
        return SDL_BYTEORDER == SDL_BIG_ENDIAN ? (p[0] << 16 | p[1] << 8 | p[2])
                                               : (p[0] | p[1] << 8 | p[2] << 16);
    case 4:
        return *(Uint32 *)p;
    default:
        return 0;
    }
}

// The colour in the middle of a theme image; fallback when it is missing
// or mostly transparent.
static inline SDL_Color ui_sampleColor(SDL_Surface *s, SDL_Color fallback)
{
    if (s == NULL || s->pixels == NULL)
        return fallback;
    if (SDL_MUSTLOCK(s) && SDL_LockSurface(s) != 0)
        return fallback;
    Uint8 r, g, b, a = 255;
    SDL_GetRGBA(_ui_getPixel(s, s->w / 2, s->h / 2), s->format, &r, &g, &b, &a);
    if (SDL_MUSTLOCK(s))
        SDL_UnlockSurface(s);
    if (a < 128)
        return fallback;
    return (SDL_Color){r, g, b};
}

// Sampled once; ui_resetColors() samples them again (Tweaks, after the
// accent colour changes the theme images).
static bool _ui_accent_done = false, _ui_bar_done = false;

// Tweaks' accent colour preview: while a colour is chosen, what is drawn
// in the accent (the selection, switches, badges) shows it at once; its
// pictures follow once it is applied (ui_resetColors ends the preview).
static bool _ui_accent_preview = false;
static SDL_Color _ui_accent_preview_colour;

static inline void ui_previewAccent(SDL_Color c)
{
    _ui_accent_preview = true;
    _ui_accent_preview_colour = c;
}

static inline SDL_Color ui_accent(void)
{
    static SDL_Color c;
    if (_ui_accent_preview)
        return _ui_accent_preview_colour;
    if (!_ui_accent_done) {
        c = ui_sampleColor(resource_getSurface(BG_LIST_S), (SDL_Color){168, 41, 74});
        _ui_accent_done = true;
    }
    return c;
}

static inline SDL_Color ui_bar(void)
{
    static SDL_Color c;
    if (!_ui_bar_done) {
        c = ui_sampleColor(resource_getSurface(BG_TITLE), (SDL_Color){28, 28, 30});
        _ui_bar_done = true;
    }
    return c;
}

static inline void ui_resetColors(void)
{
    _ui_accent_done = false;
    _ui_bar_done = false;
    _ui_accent_preview = false;
}

static inline Uint32 ui_map(SDL_Surface *screen, SDL_Color c)
{
    return SDL_MapRGB(screen->format, c.r, c.g, c.b);
}

// Filled box with rounded corners (radius in screen pixels).
static inline void ui_fillRoundRect(SDL_Surface *screen, SDL_Rect r, int radius, Uint32 color)
{
    if (r.w <= 0 || r.h <= 0)
        return;
    if (radius * 2 > r.w)
        radius = r.w / 2;
    if (radius * 2 > r.h)
        radius = r.h / 2;
    for (int y = 0; y < r.h; y++) {
        int inset = 0;
        int row = y < radius ? y : (y >= r.h - radius ? r.h - 1 - y : -1);
        if (row >= 0) {
            // Doubled to stay in integers: the row's middle is (radius - row
            // - 0.5) from the corner circle's centre.
            int yy = 2 * (radius - row) - 1;
            int span = 4 * radius * radius - yy * yy, x2 = 0;
            while ((x2 + 1) * (x2 + 1) <= span)
                x2++;
            inset = radius - x2 / 2;
        }
        SDL_Rect line = {r.x + inset, r.y + y, r.w - inset * 2, 1};
        if (line.w > 0)
            SDL_FillRect(screen, &line, color);
    }
}

// A chevron, ">" (dir 1) or "<" (dir -1), its tip at x when dir is 1 and
// its open side at x when dir is -1, centred on cy.
static inline void ui_drawChevron(SDL_Surface *screen, int x, int cy, int half, int dir, Uint32 color)
{
    int t = half / 3 > 2 ? half / 3 : 2;
    for (int k = 0; k <= half; k++) {
        int px = dir > 0 ? x - half + k : x - k;
        SDL_Rect a = {px - t / 2, cy - half + k - t / 2, t, t};
        SDL_Rect b = {px - t / 2, cy + half - k - t / 2, t, t};
        SDL_FillRect(screen, &a, color);
        SDL_FillRect(screen, &b, color);
    }
}

// The on/off switch, its right end at x_right. Width it uses.
static inline int ui_drawSwitch(SDL_Surface *screen, int x_right, int cy, bool on,
                         bool selected, bool disabled)
{
    int w = 52.0 * g_scale, h = 28.0 * g_scale, knob = 22.0 * g_scale,
        gap = 3.0 * g_scale;
    SDL_Color accent = ui_accent();
    SDL_Color track, dot;
    if (on) {
        track = selected ? (SDL_Color){255, 255, 255} : accent;
        dot = selected ? accent : (SDL_Color){255, 255, 255};
    }
    else {
        track = (SDL_Color){58, 58, 61};
        dot = (SDL_Color){UI_GREY_R, UI_GREY_G, UI_GREY_B};
    }
    if (disabled) {
        track = (SDL_Color){track.r / 2, track.g / 2, track.b / 2};
        dot = (SDL_Color){dot.r / 2, dot.g / 2, dot.b / 2};
    }
    SDL_Rect tr = {x_right - w, cy - h / 2, w, h};
    ui_fillRoundRect(screen, tr, h / 2, ui_map(screen, track));
    SDL_Rect kr = {on ? tr.x + w - gap - knob : tr.x + gap, cy - knob / 2, knob, knob};
    ui_fillRoundRect(screen, kr, knob / 2, ui_map(screen, dot));
    return w;
}

// "Startup..." opens a page: the label without its dots, and a chevron.
static inline bool ui_opensPage(const char *label)
{
    size_t n = label ? strlen(label) : 0;
    return n > 3 && strcmp(label + n - 3, "...") == 0;
}

// A page's title without the "..." of the entry that opened it ("Edit console
// icon..." is titled "Edit console icon"): s itself, or a copy in buf.
static inline const char *ui_pageTitle(const char *s, char *buf, size_t size)
{
    if (!ui_opensPage(s) || size == 0)
        return s;
    size_t n = strlen(s) - 3;
    if (n >= size)
        n = size - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    return buf;
}

#endif // RENDER_UNIFIED_H__
