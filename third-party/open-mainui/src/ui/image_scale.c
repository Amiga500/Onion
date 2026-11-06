/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/image_scale.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    int first, last;
    unsigned fraction;
} SourceSample;

/* A filtered copy must retain the source surface's per-pixel alpha,
 * otherwise Carousel's faded neighbours break. */
static void copy_flags(SDL_Surface *source, SDL_Surface *out)
{
    if (source->flags & SDL_SRCCOLORKEY) {
        SDL_SetColorKey(out, SDL_SRCCOLORKEY, source->format->colorkey);
    }
    SDL_SetAlpha(out, source->flags & SDL_SRCALPHA, source->format->alpha);
}

static SDL_Surface *destination(SDL_Surface *source, int w, int h)
{
    SDL_PixelFormat *f = source->format;
    return SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, f->Rmask, f->Gmask, f->Bmask, f->Amask);
}

/* Pixel-centred fixed-point bilinear interpolation, for enlarging, also by
 * non-integer factors such as a pixel aspect correction, and for fast
 * reductions such as the Carousel's artwork, where an area average was too
 * slow. Premultiply by alpha during interpolation to avoid dark fringes. */
static SourceSample bilinear_sample(int position, int source_size, int target_size)
{
    int64_t coordinate =
        (((int64_t)2 * position + 1) * source_size * 256) / ((int64_t)2 * target_size) - 128;
    if (coordinate <= 0) {
        return (SourceSample){0, 0, 0};
    }
    int64_t last = (int64_t)(source_size - 1) * 256;
    if (coordinate >= last) {
        return (SourceSample){source_size - 1, source_size - 1, 0};
    }
    int first = (int)(coordinate / 256);
    return (SourceSample){first, first + 1, (unsigned)(coordinate % 256)};
}

/* Area-average enlargement along one axis: the source pixels an output
 * pixel covers, by how much. Enlarged, it covers at most two, so most
 * output pixels take one source pixel's exact colour and only those across
 * a source pixel's edge mix the two: crisp pixels with blended edges. */
static SourceSample area_sample(int position, int source_size, int target_size)
{
    /* In units of 1/target_size source pixels. */
    int64_t start = (int64_t)position * source_size, end = start + source_size;
    int first = (int)(start / target_size);
    int64_t edge = (int64_t)(first + 1) * target_size;
    if (end <= edge || first + 1 >= source_size) {
        return (SourceSample){first, first, 0};
    }
    unsigned fraction = (unsigned)(((end - edge) * 256 + source_size / 2) / source_size);
    if (fraction >= 256) {
        return (SourceSample){first + 1, first + 1, 0};
    }
    return (SourceSample){first, first + 1, fraction};
}

/* Interpolates between the two source pixels per axis that sample gives,
 * weighting by alpha. */
static SDL_Surface *resample(SDL_Surface *image, int w, int h, bool area_x, bool area_y)
{
    if (!image || !image->format || image->format->BytesPerPixel != 4 || w < 1 || h < 1 ||
        (size_t)w > SIZE_MAX / sizeof(SourceSample)) {
        return NULL;
    }
    SDL_Surface *out = destination(image, w, h);
    SourceSample *xs = malloc((size_t)w * sizeof(*xs));
    if (!out || !xs) {
        if (out) {
            SDL_FreeSurface(out);
        }
        free(xs);
        return NULL;
    }
    for (int x = 0; x < w; x++) {
        xs[x] = area_x ? area_sample(x, image->w, w) : bilinear_sample(x, image->w, w);
    }
    if (SDL_LockSurface(image)) {
        SDL_FreeSurface(out);
        free(xs);
        return NULL;
    }
    const int alpha = image->format->Amask ? image->format->Ashift / 8 : -1;
    for (int y = 0; y < h; y++) {
        SourceSample sy = area_y ? area_sample(y, image->h, h) : bilinear_sample(y, image->h, h);
        const Uint8 *row0 = (const Uint8 *)image->pixels + sy.first * image->pitch;
        const Uint8 *row1 = (const Uint8 *)image->pixels + sy.last * image->pitch;
        Uint8 *target = (Uint8 *)out->pixels + y * out->pitch;
        for (int x = 0; x < w; x++) {
            SourceSample sx = xs[x];
            const Uint8 *p[4] = {row0 + sx.first * 4, row0 + sx.last * 4, row1 + sx.first * 4,
                                 row1 + sx.last * 4};
            unsigned weight[4] = {(256 - sx.fraction) * (256 - sy.fraction),
                                  sx.fraction * (256 - sy.fraction),
                                  (256 - sx.fraction) * sy.fraction, sx.fraction * sy.fraction};
            unsigned a[4], total = 0;
            for (int i = 0; i < 4; i++) {
                a[i] = alpha >= 0 ? p[i][alpha] : 255;
                total += a[i] * weight[i];
            }
            Uint8 *dest = target + x * 4;
            for (int c = 0; c < 4; c++) {
                if (c == alpha) {
                    dest[c] = (Uint8)((total + 32768) / 65536);
                }
                else if (total) {
                    uint32_t sum = 0;
                    for (int i = 0; i < 4; i++) {
                        sum += (uint32_t)p[i][c] * a[i] * weight[i];
                    }
                    dest[c] = (Uint8)((sum + total / 2) / total);
                }
                else {
                    dest[c] = 0;
                }
            }
        }
    }
    SDL_UnlockSurface(image);
    free(xs);
    copy_flags(image, out);
    return out;
}

SDL_Surface *mainui_image_bilinear_copy(SDL_Surface *image, int w, int h)
{
    return resample(image, w, h, false, false);
}

SDL_Surface *mainui_image_framed_copy(SDL_Surface *image, int w, int h)
{
    if (!image) {
        return NULL;
    }
    return resample(image, w, h, w >= image->w, h >= image->h);
}

/* Fit within a maximum rectangle without enlargement, the aspect ratio
 * rounded to whole pixels. */
SDL_Surface *mainui_image_fit(SDL_Surface *image, int width, int height)
{
    if (!image || !image->format || width < 1 || height < 1) {
        return NULL;
    }
    double scale = (double)width / image->w < (double)height / image->h ? (double)width / image->w
                                                                        : (double)height / image->h;
    if (scale >= 1 || image->format->BytesPerPixel != 4) {
        return NULL;
    }
    int w = (int)(image->w * scale + 0.5), h = (int)(image->h * scale + 0.5);
    w = w < 1 ? 1 : w > width ? width : w;
    h = h < 1 ? 1 : h > height ? height : h;
    return mainui_image_bilinear_copy(image, w, h);
}
