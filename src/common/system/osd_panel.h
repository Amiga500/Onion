#ifndef SYSTEM_OSD_PANEL_H__
#define SYSTEM_OSD_PANEL_H__

// The volume/brightness panel: a dark rounded box at the bottom of the
// screen with an icon (sun or speaker) and a horizontal bar, drawn by keymon
// over whatever is on screen while the keys are used (as in AlliumOS).
//
// Only the picture is made here, in screen orientation, with no SDL or
// framebuffer: osd.h copies it to the screen. A pixel is 0x00RRGGBB with
// OSD_PANEL_OPAQUE set when it belongs to the panel; the rounded corners
// are left clear (0), so the screen shows through there.

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define OSD_PANEL_OPAQUE 0xFF000000u

#define OSD_PANEL_BRIGHTNESS 0
#define OSD_PANEL_VOLUME 1

#define OSD_PANEL_BG 0x00202020u
#define OSD_PANEL_TRACK 0x00505050u
#define OSD_PANEL_ICON 0x00FFFFFFu

typedef struct {
    int x, y, w, h;
} osd_panel_rect_t;

// Size and place of the panel on a screen of screen_w x screen_h, scaled
// from 640x480: 240x40, centred, 52 px from the bottom edge or, with top,
// from the top edge (below the menus' top bar). It keeps its look on the
// 752x560 screens.
static inline osd_panel_rect_t osd_panel_geometry(int screen_w, int screen_h, bool top)
{
    double s = screen_h > 0 ? screen_h / 480.0 : 1.0;
    osd_panel_rect_t r;
    r.w = (int)(240 * s + 0.5);
    r.h = (int)(40 * s + 0.5);
    if (r.w > screen_w)
        r.w = screen_w;
    if (r.h > screen_h)
        r.h = screen_h;
    r.x = (screen_w - r.w) / 2;
    int margin = (int)(52 * s + 0.5);
    r.y = top ? margin : screen_h - r.h - margin;
    if (r.y + r.h > screen_h)
        r.y = screen_h - r.h;
    if (r.y < 0)
        r.y = 0;
    return r;
}

// How many of the panel's pixels in row y (0 = top) are clear on each side,
// for the rounded corners: the panel row is [inset, w - inset).
static inline int osd_panel_row_inset(int w, int h, int y)
{
    double r = h * 0.5;
    if (r > w * 0.5)
        r = w * 0.5;
    double dy = 0;
    if (y + 0.5 < r)
        dy = r - (y + 0.5);
    else if (y + 0.5 > h - r)
        dy = (y + 0.5) - (h - r);
    if (dy <= 0)
        return 0;
    if (dy >= r)
        return (int)r;
    double dx = r - sqrt(r * r - dy * dy);
    return (int)(dx + 0.5);
}

static inline double _osd_seg_dist(double px, double py, double ax, double ay, double bx, double by)
{
    double vx = bx - ax, vy = by - ay;
    double wx = px - ax, wy = py - ay;
    double l2 = vx * vx + vy * vy;
    double t = l2 > 0 ? (wx * vx + wy * vy) / l2 : 0;
    if (t < 0)
        t = 0;
    else if (t > 1)
        t = 1;
    double dx = wx - t * vx, dy = wy - t * vy;
    return sqrt(dx * dx + dy * dy);
}

// Is (px, py) inside the convex polygon (clockwise or not)?
static inline bool _osd_in_convex(double px, double py, const double *xs, const double *ys, int n)
{
    int sign = 0;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        double c = (xs[j] - xs[i]) * (py - ys[i]) - (ys[j] - ys[i]) * (px - xs[i]);
        int s = c > 0 ? 1 : (c < 0 ? -1 : 0);
        if (s == 0)
            continue;
        if (sign == 0)
            sign = s;
        else if (s != sign)
            return false;
    }
    return true;
}

// Is the point (u, v), in icon units (0..1 across the icon square), on the
// icon? waves: 0, 1 or 2 sound waves; mute draws an X instead of them.
static inline bool _osd_icon_hit(int kind, double u, double v, int waves, bool mute)
{
    if (kind == OSD_PANEL_BRIGHTNESS) {
        double dx = u - 0.5, dy = v - 0.5;
        if (dx * dx + dy * dy <= 0.2 * 0.2)
            return true;
        // The 8 rays, every 45 degrees.
        static const double rc[8] = {1, 0.70710678, 0, -0.70710678, -1, -0.70710678, 0, 0.70710678};
        static const double rs[8] = {0, 0.70710678, 1, 0.70710678, 0, -0.70710678, -1, -0.70710678};
        for (int i = 0; i < 8; i++) {
            double c = rc[i], s = rs[i];
            if (_osd_seg_dist(u, v, 0.5 + 0.32 * c, 0.5 + 0.32 * s, 0.5 + 0.46 * c, 0.5 + 0.46 * s) <= 0.05)
                return true;
        }
        return false;
    }

    // Speaker: a box and a cone.
    if (u >= 0.06 && u <= 0.26 && v >= 0.36 && v <= 0.64)
        return true;
    const double cx[4] = {0.26, 0.52, 0.52, 0.26};
    const double cy[4] = {0.36, 0.12, 0.88, 0.64};
    if (_osd_in_convex(u, v, cx, cy, 4))
        return true;

    if (mute) {
        return _osd_seg_dist(u, v, 0.66, 0.36, 0.94, 0.64) <= 0.055 ||
               _osd_seg_dist(u, v, 0.66, 0.64, 0.94, 0.36) <= 0.055;
    }

    // Sound waves: arcs around the cone's mouth, within +-50 degrees.
    double dx = u - 0.52, dy = v - 0.5;
    if (dx <= 0)
        return false;
    double d = sqrt(dx * dx + dy * dy);
    if (fabs(dy) > dx * 1.19) // tan(50 degrees)
        return false;
    if (waves >= 1 && fabs(d - 0.18) <= 0.05)
        return true;
    if (waves >= 2 && fabs(d - 0.36) <= 0.05)
        return true;
    return false;
}

// Where the icon and the bar are in a panel w x h, from the 32 px tall
// panel: a 20 px icon 12 px from the left, then 10 px, then the bar (6 px
// tall) to 14 px from the right.
typedef struct {
    int kind, value, max, waves;
    bool mute;
    uint32_t color, icon_color;
    double icon, icon_x, icon_y;
    double bar_x0, bar_x1, bar_h, bar_y0, bar_r, fill_x1;
} osd_panel_layout_t;

static inline osd_panel_layout_t osd_panel_layout(int w, int h, int kind, int value, int max, bool mute,
                                                  uint32_t color)
{
    osd_panel_layout_t l;
    if (max <= 0)
        max = 1;
    if (value < 0)
        value = 0;
    if (value > max)
        value = max;
    double s = h / 32.0;
    l.kind = kind;
    l.value = value;
    l.max = max;
    l.waves = value == 0 ? 0 : (value * 2 <= max ? 1 : 2);
    l.mute = mute;
    l.color = color;
    l.icon_color = mute ? color : OSD_PANEL_ICON;
    l.icon = 20 * s;
    l.icon_x = 12 * s;
    l.icon_y = (h - l.icon) / 2;
    l.bar_x0 = l.icon_x + l.icon + 10 * s;
    l.bar_x1 = w - 14 * s;
    l.bar_h = 6 * s;
    l.bar_y0 = (h - l.bar_h) / 2;
    l.bar_r = l.bar_h / 2;
    l.fill_x1 = l.bar_x0 + (l.bar_x1 - l.bar_x0) * value / max;
    return l;
}

#define OSD_PANEL_SS 4 // 4x4 samples per pixel, for smooth edges

// The color of the panel's pixel (x, y), from its 16 samples.
static inline uint32_t osd_panel_sample(const osd_panel_layout_t *l, int x, int y)
{
    const int ss = OSD_PANEL_SS;
    int icon_hits = 0, fill_hits = 0, track_hits = 0;
    for (int sy = 0; sy < ss; sy++) {
        for (int sx = 0; sx < ss; sx++) {
            double fx = x + (sx + 0.5) / ss, fy = y + (sy + 0.5) / ss;
            double u = (fx - l->icon_x) / l->icon, v = (fy - l->icon_y) / l->icon;
            if (u >= 0 && u <= 1 && v >= 0 && v <= 1 && _osd_icon_hit(l->kind, u, v, l->waves, l->mute)) {
                icon_hits++;
                continue;
            }
            // The bar: a capsule from bar_x0 to bar_x1.
            if (_osd_seg_dist(fx, fy, l->bar_x0 + l->bar_r, l->bar_y0 + l->bar_r, l->bar_x1 - l->bar_r,
                              l->bar_y0 + l->bar_r) <= l->bar_r) {
                if (l->value > 0 && fx <= l->fill_x1)
                    fill_hits++;
                else
                    track_hits++;
            }
        }
    }
    const int total = ss * ss;
    const uint32_t c = OSD_PANEL_BG;
    int bg = total - icon_hits - fill_hits - track_hits;
    // Mix the three colors over the background by their share.
    uint32_t rgb = 0;
    for (int sh = 0; sh <= 16; sh += 8) {
        int sum = ((c >> sh) & 0xFF) * bg + ((l->icon_color >> sh) & 0xFF) * icon_hits +
                  ((l->color >> sh) & 0xFF) * fill_hits + ((OSD_PANEL_TRACK >> sh) & 0xFF) * track_hits;
        rgb |= (uint32_t)((sum + total / 2) / total) << sh;
    }
    return OSD_PANEL_OPAQUE | rgb;
}

// The panel picture, w x h pixels (see the top of the file). value of max
// sets the bar; color is the bar's color (and the icon's when muted).
// Sampled only near the icon and the bar's edges: elsewhere every sample
// of a pixel falls on the same thing, so its color is known (the same
// result as sampling it, as test_osd_panel checks; keymon draws it on the
// device's slow CPU at each step of the keys).
static inline void osd_panel_render(uint32_t *px, int w, int h, int kind, int value, int max, bool mute,
                                    uint32_t color)
{
    if (!px || w <= 0 || h <= 0)
        return;
    const osd_panel_layout_t l = osd_panel_layout(w, h, kind, value, max, mute, color);
    const uint32_t bg = OSD_PANEL_OPAQUE | OSD_PANEL_BG;
    const uint32_t fill = OSD_PANEL_OPAQUE | (color & 0x00FFFFFFu);
    const uint32_t track = OSD_PANEL_OPAQUE | OSD_PANEL_TRACK;
    const double bar_y1 = l.bar_y0 + l.bar_h;

    for (int y = 0; y < h; y++) {
        int inset = osd_panel_row_inset(w, h, y);
        uint32_t *row = px + (size_t)y * w;
        const bool near_icon_rows = y + 1 > l.icon_y && y < l.icon_y + l.icon;
        const bool near_bar_rows = y + 1 > l.bar_y0 && y < bar_y1;
        const bool inside_bar_rows = y >= l.bar_y0 && y + 1 <= bar_y1;
        for (int x = 0; x < w; x++) {
            if (x < inset || x >= w - inset) {
                row[x] = 0;
                continue;
            }
            const bool near_icon = near_icon_rows && x + 1 > l.icon_x && x < l.icon_x + l.icon;
            const bool near_bar = near_bar_rows && x + 1 > l.bar_x0 && x < l.bar_x1;
            if (!near_icon && !near_bar) {
                row[x] = bg;
                continue;
            }
            // Inside the bar's straight part, all on one side of the fill.
            if (!near_icon && inside_bar_rows && x >= l.bar_x0 + l.bar_r && x + 1 <= l.bar_x1 - l.bar_r) {
                if (l.value > 0 && x + 1 <= l.fill_x1) {
                    row[x] = fill;
                    continue;
                }
                if (l.value == 0 || x > l.fill_x1) {
                    row[x] = track;
                    continue;
                }
            }
            row[x] = osd_panel_sample(&l, x, y);
        }
    }
}

#endif // SYSTEM_OSD_PANEL_H__
