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
#include "theme/background.h"
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

// Themes other than dark ones: the white icons, the white words on the
// selection and the grey descriptions of the new look could hardly be seen
// on a light list (NanoSwitch: white list, light grey selection, #424242
// text; 4.5.1, Amiga500's photo), and the grey descriptions not at all on a
// coloured one (PSPmini Blue, Poketch...). On a dark list (Silky RedOnion)
// the colours stay as designed; on any other the theme's own list text
// colour is used, which the theme made to be read on it.
static inline int ui_luma(SDL_Color c)
{
    return (c.r * 299 + c.g * 587 + c.b * 114) / 1000;
}

// The mean brightness of the background behind the list (y 60 to 420),
// found once per background picture.
static inline int ui_listBackgroundLuma(void)
{
    static SDL_Surface *seen = NULL;
    static int luma = 0;
    SDL_Surface *bg = theme_background();
    if (bg == NULL)
        return 0;
    if (bg == seen)
        return luma;
    seen = bg;
    luma = 0;
    long sum = 0, n = 0;
    if (SDL_MUSTLOCK(bg) && SDL_LockSurface(bg) != 0)
        return 0;
    int step = 8;
    for (int y = 60.0 * g_scale; y < 420.0 * g_scale && y < bg->h; y += step)
        for (int x = 0; x < bg->w; x += step) {
            Uint8 r, g, b;
            SDL_GetRGB(_ui_getPixel(bg, x, y), bg->format, &r, &g, &b);
            sum += (r * 299 + g * 587 + b * 114) / 1000;
            n++;
        }
    if (SDL_MUSTLOCK(bg))
        SDL_UnlockSurface(bg);
    luma = n ? (int)(sum / n) : 0;
    return luma;
}

// A dark list (the new look's own case): grey and white as designed.
static inline bool ui_darkList(void)
{
    return ui_listBackgroundLuma() < 70;
}

// Kept for the callers: a list that is not dark.
static inline bool ui_lightList(void)
{
    return !ui_darkList();
}

// What is drawn on the selection (accent): white, or dark on a light one.
static inline SDL_Color ui_onAccent(void)
{
    if (ui_luma(ui_accent()) <= 170)
        return (SDL_Color){255, 255, 255};
    SDL_Color t = theme()->list.color;
    return ui_luma(t) < 110 ? t : (SDL_Color){28, 28, 30};
}

// Descriptions and values beside the rows: grey on a dark list, else the
// theme's list text colour.
static inline SDL_Color ui_secondary(void)
{
    return ui_darkList() ? (SDL_Color){UI_GREY_R, UI_GREY_G, UI_GREY_B}
                         : theme()->list.color;
}

// A row's line icon: white on a dark list, else the list's text colour;
// the colour on the selection for the selected row.
static inline SDL_Color ui_iconColour(bool selected)
{
    if (selected)
        return ui_onAccent();
    return ui_darkList() ? (SDL_Color){255, 255, 255} : theme()->list.color;
}

// A white line icon in colour c (a copy, kept): the icon itself when c is
// white, or when the icon is not white (a picture, a coloured icon).
#define UI_TINT_CACHE 32
static struct {
    SDL_Surface *src, *tinted;
    int w, h;
    Uint32 key;
} _ui_tint[UI_TINT_CACHE];
static int _ui_tint_next = 0;

static inline SDL_Surface *ui_tinted(SDL_Surface *icon, SDL_Color c)
{
    if (icon == NULL || (c.r == 255 && c.g == 255 && c.b == 255))
        return icon;
    Uint32 key = (Uint32)c.r << 16 | (Uint32)c.g << 8 | c.b;
    for (int i = 0; i < UI_TINT_CACHE; i++)
        if (_ui_tint[i].src == icon && _ui_tint[i].w == icon->w && _ui_tint[i].h == icon->h &&
            _ui_tint[i].key == key)
            return _ui_tint[i].tinted ? _ui_tint[i].tinted : icon;
    SDL_Surface *out = NULL;
    bool white = false; // every visible pixel near white: a line icon
    if (icon->format->BytesPerPixel == 4 && SDL_LockSurface(icon) == 0) {
        white = true;
        for (int y = 0; y < icon->h && white; y++)
            for (int x = 0; x < icon->w; x++) {
                Uint8 r, g, b, a;
                SDL_GetRGBA(_ui_getPixel(icon, x, y), icon->format, &r, &g, &b, &a);
                if (a > 32 && (r < 200 || g < 200 || b < 200)) {
                    white = false;
                    break;
                }
            }
        SDL_UnlockSurface(icon);
    }
    // Made as resources.h makes the X button (which the device draws
    // right): ABGR, then told to blit with its own alpha. Without
    // SDL_SetAlpha the device's SDL drew it as a solid square (4.5.1,
    // Amiga500's photo): it does not set SDL_SRCALPHA by itself.
    if (white) {
        out = SDL_CreateRGBSurface(SDL_SWSURFACE, icon->w, icon->h, 32, 0x000000ff,
                                   0x0000ff00, 0x00ff0000, 0xff000000);
        if (out)
            SDL_SetAlpha(out, SDL_SRCALPHA, 255);
    }
    if (out && out->format->BytesPerPixel == 4 && SDL_LockSurface(out) == 0) {
        if (SDL_LockSurface(icon) == 0) {
            for (int y = 0; y < icon->h; y++) {
                Uint32 *row = (Uint32 *)((Uint8 *)out->pixels + y * out->pitch);
                for (int x = 0; x < icon->w; x++) {
                    Uint8 r, g, b, a;
                    SDL_GetRGBA(_ui_getPixel(icon, x, y), icon->format, &r, &g, &b, &a);
                    row[x] = SDL_MapRGBA(out->format, c.r, c.g, c.b, a);
                }
            }
            SDL_UnlockSurface(icon);
        }
        SDL_UnlockSurface(out);
    }
    else if (out) {
        SDL_FreeSurface(out);
        out = NULL;
    }
    int i = _ui_tint_next;
    _ui_tint_next = (_ui_tint_next + 1) % UI_TINT_CACHE;
    if (_ui_tint[i].tinted)
        SDL_FreeSurface(_ui_tint[i].tinted);
    _ui_tint[i].src = icon;
    _ui_tint[i].w = icon->w;
    _ui_tint[i].h = icon->h;
    _ui_tint[i].key = key;
    _ui_tint[i].tinted = out;
    return out ? out : icon;
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
