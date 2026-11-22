#ifndef THEME_BUTTON_X_H__
#define THEME_BUTTON_X_H__

// An X button made from the theme's B button, for themes with no X of their
// own (no theme has one: MainUI never shows X). Same button, same colours:
// the B letter is painted over with the button's colour, and an X is drawn
// where it was, in the letter's colour (a letter cut out of the button gives
// an X cut out of it). Plain C on RGBA pixels, so host tests can run it.
//
// It only takes buttons it can read: roughly round or square (not a wide
// "B BACK" label, not four dots), mostly filled, with a letter inside. Any
// other B (a banana, a pad drawing) gives false, and the caller keeps its
// own fallback. An outline around the button (a ring, a border) is part of
// the button, not of the letter, and stays as it is.

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BUTTONX_MAX_SIDE 512

// No libm: not every program that shows buttons links it
static int _buttonx_round(float v) { return (int)(v + 0.5f); }

// Pixels reachable from the seeds (set in out) through pixels of mask,
// 8-connected. queue: room for w * h indices.
static void _buttonx_flood(const uint8_t *mask, uint8_t *out, int *queue,
                           int w, int h)
{
    int head = 0, tail = 0;
    for (int i = 0; i < w * h; i++)
        if (out[i])
            queue[tail++] = i;
    while (head < tail) {
        int i = queue[head++];
        int x = i % w, y = i / w;
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                    continue;
                int n = ny * w + nx;
                if (mask[n] && !out[n]) {
                    out[n] = 1;
                    queue[tail++] = n;
                }
            }
        }
    }
}

// One step of erosion (grow false) or dilation (grow true), 4-connected;
// outside the image counts as false.
static void _buttonx_step(const uint8_t *in, uint8_t *out, int w, int h,
                          bool grow)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            uint8_t up = y > 0 ? in[i - w] : 0;
            uint8_t down = y < h - 1 ? in[i + w] : 0;
            uint8_t left = x > 0 ? in[i - 1] : 0;
            uint8_t right = x < w - 1 ? in[i + 1] : 0;
            out[i] = grow ? (in[i] | up | down | left | right)
                          : (in[i] & up & down & left & right);
        }
    }
}

// Per channel median of the pixels where select is set; false if none.
static bool _buttonx_median(const uint8_t *rgba, const uint8_t *select,
                            int count, uint8_t out[3])
{
    int total = 0;
    int hist[3][256];
    memset(hist, 0, sizeof(hist));
    for (int i = 0; i < count; i++) {
        if (!select[i])
            continue;
        total++;
        for (int c = 0; c < 3; c++)
            hist[c][rgba[i * 4 + c]]++;
    }
    if (total == 0)
        return false;
    for (int c = 0; c < 3; c++) {
        int seen = 0, v = 0;
        while (v < 255 && (seen += hist[c][v]) * 2 < total)
            v++;
        out[c] = (uint8_t)v;
    }
    return true;
}

static int _buttonx_diff(const uint8_t *px, const uint8_t fill[3])
{
    return abs(px[0] - fill[0]) + abs(px[1] - fill[1]) + abs(px[2] - fill[2]);
}

// src and dst: w * h RGBA pixels (dst may not be src). True when dst holds
// the X button; false (dst untouched) when B is not a button it can read.
static bool buttonx_generate(const uint8_t *src, int w, int h, uint8_t *dst)
{
    if (src == NULL || dst == NULL || w < 8 || h < 8 ||
        w > BUTTONX_MAX_SIDE || h > BUTTONX_MAX_SIDE)
        return false;

    const int n = w * h;
    bool ok = false;
    uint8_t *opaque = calloc(n, 1), *outside = calloc(n, 1);
    uint8_t *disc = calloc(n, 1), *inner = calloc(n, 1), *tmp = calloc(n, 1);
    uint8_t *cand = calloc(n, 1), *outline = calloc(n, 1);
    uint8_t *letter = calloc(n, 1), *erase = calloc(n, 1);
    int *queue = malloc(sizeof(int) * n);
    float *cover = calloc(n, sizeof(float));
    if (!opaque || !outside || !disc || !inner || !tmp || !cand || !outline ||
        !letter || !erase || !queue || !cover)
        goto done;

    int opaque_count = 0;
    for (int i = 0; i < n; i++) {
        opaque[i] = src[i * 4 + 3] >= 128;
        opaque_count += opaque[i];
    }
    if (opaque_count == 0)
        goto done;

    // Transparent pixels reached from the edges are outside the button; the
    // other transparent ones are holes in it (a cut out letter).
    for (int i = 0; i < n; i++)
        tmp[i] = !opaque[i];
    for (int x = 0; x < w; x++) {
        outside[x] = tmp[x];
        outside[(h - 1) * w + x] = tmp[(h - 1) * w + x];
    }
    for (int y = 0; y < h; y++) {
        outside[y * w] = tmp[y * w];
        outside[y * w + w - 1] = tmp[y * w + w - 1];
    }
    _buttonx_flood(tmp, outside, queue, w, h);

    int left = w, right = -1, top = h, bottom = -1, disc_count = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            disc[i] = opaque[i] || !outside[i];
            if (!disc[i])
                continue;
            disc_count++;
            if (x < left)
                left = x;
            if (x > right)
                right = x;
            if (y < top)
                top = y;
            if (y > bottom)
                bottom = y;
        }
    }
    int bw = right - left + 1, bh = bottom - top + 1;
    // Round or square, mostly filled (a disc fills 79 % of its box)
    if (bw * 5 < bh * 4 || bw * 4 > bh * 5 || disc_count * 5 < bw * bh * 3)
        goto done;
    int size = bw < bh ? bw : bh;
    float radius = size / 2.0f;

    // The rim: the button's outer band, where an outline or ring is
    int k = _buttonx_round(size * 0.08f);
    if (k < 2)
        k = 2;
    memcpy(inner, disc, n);
    for (int s = 0; s < k; s++) {
        _buttonx_step(inner, tmp, w, h, false);
        memcpy(inner, tmp, n);
    }

    // The button's colour: the commonest colour inside the rim
    int bucket_count[512];
    memset(bucket_count, 0, sizeof(bucket_count));
    int inner_opaque = 0;
    for (int i = 0; i < n; i++) {
        if (!(opaque[i] && inner[i]))
            continue;
        inner_opaque++;
        const uint8_t *p = src + i * 4;
        bucket_count[(p[0] >> 5) * 64 + (p[1] >> 5) * 8 + (p[2] >> 5)]++;
    }
    if (inner_opaque < 8)
        goto done;
    int top_bucket = 0;
    for (int b = 1; b < 512; b++)
        if (bucket_count[b] > bucket_count[top_bucket])
            top_bucket = b;
    for (int i = 0; i < n; i++) {
        const uint8_t *p = src + i * 4;
        tmp[i] = opaque[i] && inner[i] &&
                 (p[0] >> 5) * 64 + (p[1] >> 5) * 8 + (p[2] >> 5) == top_bucket;
    }
    uint8_t fill[3];
    if (!_buttonx_median(src, tmp, n, fill))
        goto done;

    // The letter: holes and pixels far from the button's colour, but not
    // the ones joined to the rim (an outline, a ring)
    for (int i = 0; i < n; i++) {
        bool hole = disc[i] && !opaque[i];
        cand[i] = disc[i] && (hole || _buttonx_diff(src + i * 4, fill) > 70);
        outline[i] = cand[i] && !inner[i];
    }
    _buttonx_flood(cand, outline, queue, w, h);
    int letter_count = 0, letter_holes = 0;
    int ltop = h, lbottom = -1, lleft = w, lright = -1;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            letter[i] = cand[i] && !outline[i] && inner[i];
            if (!letter[i])
                continue;
            letter_count++;
            letter_holes += !opaque[i];
            if (x < lleft)
                lleft = x;
            if (x > lright)
                lright = x;
            if (y < ltop)
                ltop = y;
            if (y > lbottom)
                lbottom = y;
        }
    }
    if (letter_count < 4)
        goto done;
    int letter_h = lbottom - ltop + 1;
    bool cut = letter_holes * 2 > letter_count;

    uint8_t ink[3] = {255, 255, 255};
    if (!cut) {
        int strong = 0;
        for (int i = 0; i < n; i++) {
            tmp[i] = letter[i] && opaque[i] && _buttonx_diff(src + i * 4, fill) > 180;
            strong += tmp[i];
        }
        if (strong <= 3)
            for (int i = 0; i < n; i++)
                tmp[i] = letter[i] && opaque[i];
        _buttonx_median(src, tmp, n, ink);
    }

    // Paint the letter over: it, and its faint edge 2 pixels around it
    memcpy(erase, letter, n);
    for (int s = 0; s < 2; s++) {
        _buttonx_step(erase, tmp, w, h, true);
        memcpy(erase, tmp, n);
    }
    memcpy(dst, src, (size_t)n * 4);
    for (int i = 0; i < n; i++) {
        bool hole = disc[i] && !opaque[i];
        bool faint = erase[i] && inner[i] && !outline[i] &&
                     (hole || _buttonx_diff(src + i * 4, fill) > 25);
        if (!(letter[i] || faint))
            continue;
        memcpy(dst + i * 4, fill, 3);
        dst[i * 4 + 3] = 255;
    }

    // The X, where the letter was: as tall as it (within limits), lines
    // about as thick as its strokes, antialiased (4x4 samples a pixel)
    float cx = (lleft + lright + 1) / 2.0f, cy = (ltop + lbottom + 1) / 2.0f;
    float half = letter_h * 0.42f;
    if (half < radius * 0.24f)
        half = radius * 0.24f;
    if (half > radius * 0.40f)
        half = radius * 0.40f;
    float thick = letter_count / (4.2f * letter_h);
    if (thick > size * 0.2f)
        thick = size * 0.2f;
    if (thick < 1.6f)
        thick = 1.6f;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int hits = 0;
            for (int sy = 0; sy < 4; sy++) {
                for (int sx = 0; sx < 4; sx++) {
                    float u = x + (sx + 0.5f) / 4 - cx;
                    float v = y + (sy + 0.5f) / 4 - cy;
                    float best = 1e18f; // squared distance
                    for (int sign = -1; sign <= 1; sign += 2) {
                        float t = (u + v * sign) / (2 * half);
                        if (t < -1)
                            t = -1;
                        if (t > 1)
                            t = 1;
                        float du = u - t * half, dv = v - t * sign * half;
                        float d = du * du + dv * dv;
                        if (d < best)
                            best = d;
                    }
                    hits += best <= thick * thick / 4;
                }
            }
            cover[y * w + x] = hits / 16.0f;
        }
    }
    for (int i = 0; i < n; i++) {
        float c = cover[i];
        if (c <= 0)
            continue;
        uint8_t *p = dst + i * 4;
        if (cut) {
            p[3] = (uint8_t)_buttonx_round(p[3] * (1 - c));
            continue;
        }
        for (int ch = 0; ch < 3; ch++)
            p[ch] = (uint8_t)_buttonx_round(p[ch] * (1 - c) + ink[ch] * c);
        if (disc[i] && p[3] < c * 255)
            p[3] = (uint8_t)_buttonx_round(c * 255);
    }
    ok = true;

done:
    free(opaque);
    free(outside);
    free(disc);
    free(inner);
    free(tmp);
    free(cand);
    free(outline);
    free(letter);
    free(erase);
    free(queue);
    free(cover);
    return ok;
}

#endif // THEME_BUTTON_X_H__
