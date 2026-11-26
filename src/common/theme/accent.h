#ifndef THEME_ACCENT_H__
#define THEME_ACCENT_H__

// The accent colour chosen in Tweaks > Look and feel > Accent colour, for
// the programs that draw it without theme images (canvas_ui.h, DisplayCal,
// Battery Monitor...). script/accent_color.sh writes the colour in use to
// config/display/accentRGB as "accent light dark" (RRGGBB): the light one
// for text on the dark panels, the dark one behind a selected slider. It is
// red (the Silky RedOnion theme's own) when the file is missing or another
// theme is active. Read once per program.

#include <stdint.h>
#include <stdio.h>

#ifndef ACCENT_RGB_FILE
#define ACCENT_RGB_FILE "/mnt/SDCARD/.tmp_update/config/display/accentRGB"
#endif

enum { ACCENT_MAIN, ACCENT_LIGHT, ACCENT_DARK };

// 0xRRGGBB
static inline uint32_t accent_rgb(int which)
{
    static uint32_t c[3] = {0x9E2C45, 0xD65670, 0x782036};
    static int loaded = 0;
    if (!loaded) {
        loaded = 1;
        FILE *f = fopen(ACCENT_RGB_FILE, "r");
        if (f) {
            unsigned int a, l, d;
            if (fscanf(f, "%6x %6x %6x", &a, &l, &d) == 3) {
                c[0] = a & 0xFFFFFF;
                c[1] = l & 0xFFFFFF;
                c[2] = d & 0xFFFFFF;
            }
            fclose(f);
        }
    }
    return c[which >= 0 && which <= 2 ? which : 0];
}

// True when a colour other than the theme's red is chosen.
static inline int accent_isCustom(void)
{
    return accent_rgb(ACCENT_MAIN) != 0x9E2C45;
}

// The accent mixed with white (t from 0 to 1), for text tinted with it.
static inline uint32_t accent_tint(int which, float t)
{
    uint32_t c = accent_rgb(which);
    int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
    r += (int)((255 - r) * t + 0.5f);
    g += (int)((255 - g) * t + 0.5f);
    b += (int)((255 - b) * t + 0.5f);
    return (uint32_t)(r << 16 | g << 8 | b);
}

static inline void _accent_toHsv(float r, float g, float b, float *h, float *s, float *v)
{
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float d = mx - mn;
    *v = mx;
    *s = mx > 0 ? d / mx : 0;
    if (d <= 0) {
        *h = 0;
        return;
    }
    if (mx == r)
        *h = (g - b) / d / 6.0f;
    else if (mx == g)
        *h = ((b - r) / d + 2.0f) / 6.0f;
    else
        *h = ((r - g) / d + 4.0f) / 6.0f;
    if (*h < 0)
        *h += 1.0f;
}

static inline void _accent_toRgb(float h, float s, float v, float *r, float *g, float *b)
{
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - i, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    switch (i % 6) {
    case 0: *r = v, *g = t, *b = p; break;
    case 1: *r = q, *g = v, *b = p; break;
    case 2: *r = p, *g = v, *b = t; break;
    case 3: *r = p, *g = q, *b = v; break;
    case 4: *r = t, *g = p, *b = v; break;
    default: *r = v, *g = p, *b = q; break;
    }
}

// Recolours one pixel of a picture made in the theme's red to the chosen
// accent, as tools/accent/gen_accent.py does for the theme images: the red
// hue moved, saturation and brightness scaled, other hues left alone, with
// a soft edge between them. For the programs' own pictures (headers with
// the red badge...). Nothing to do when red is chosen (accent_isCustom).
static inline void accent_recolourPixel(uint8_t *pr, uint8_t *pg, uint8_t *pb)
{
    // The theme's red, #9E2C45, as HSV
    const float red_h = 0.96345f, red_s = 0.72152f, red_v = 0.61961f;
    float h, s, v;
    _accent_toHsv(*pr / 255.0f, *pg / 255.0f, *pb / 255.0f, &h, &s, &v);
    if (s < 0.12f || v < 0.06f)
        return;
    float ws = (s - 0.12f) / 0.18f;
    if (ws > 1)
        ws = 1;
    float d = h > red_h ? h - red_h : red_h - h;
    if (d > 0.5f)
        d = 1.0f - d;
    float wh = d <= 0.07f ? 1.0f : 1.0f - (d - 0.07f) / 0.05f;
    if (wh <= 0)
        return;
    float w = ws * wh;
    uint32_t c = accent_rgb(ACCENT_MAIN);
    float th, ts, tv;
    _accent_toHsv(((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f, &th, &ts, &tv);
    float ns = s * ts / red_s, nv = v * tv / red_v, nr, ng, nb;
    if (ns > 1)
        ns = 1;
    if (nv > 1)
        nv = 1;
    _accent_toRgb(th, ns, nv, &nr, &ng, &nb);
    *pr = (uint8_t)(*pr + (nr * 255.0f - *pr) * w + 0.5f);
    *pg = (uint8_t)(*pg + (ng * 255.0f - *pg) * w + 0.5f);
    *pb = (uint8_t)(*pb + (nb * 255.0f - *pb) * w + 0.5f);
}

#define ACCENT_R(which) ((int)((accent_rgb(which) >> 16) & 0xFF))
#define ACCENT_G(which) ((int)((accent_rgb(which) >> 8) & 0xFF))
#define ACCENT_B(which) ((int)(accent_rgb(which) & 0xFF))

#endif // THEME_ACCENT_H__
