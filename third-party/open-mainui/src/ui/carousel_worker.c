/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/carousel_worker.h"
#include "menus/console_view.h"
#include "platform/timing.h"
#include "ui/artwork.h"
#include "ui/image_scale.h"
#include <stdio.h>
#include <string.h>

static SDL_Surface *software_file(const char *path)
{
    if (!*path) {
        return NULL;
    }
    SDL_Surface *raw = mainui_artwork_load(path);
    SDL_Surface *image = mainui_artwork_software(raw);
    if (raw) {
        SDL_FreeSurface(raw);
    }
    return image;
}

/* Match mainui_theme_console_icon's default-icon fallback without borrowing
 * theme state or invoking video-surface-dependent functions on the worker. */
SDL_Surface *mainui_carousel_theme_icon(const char *path, const char *fallback)
{
    SDL_Surface *icon = software_file(path);
    if (icon || !*path || !*fallback) {
        return icon;
    }
    const char *name = strrchr(path, '/');
    const char *pack = strstr(path, "/Icons/Default/");
    if (pack) {
        name = pack + strlen("/Icons/Default");
    }
    if (!name) {
        return NULL;
    }
    char file[MAINUI_PATH_MAX];
    int length = snprintf(file, sizeof file, "%s/../../Icons/Default/%s", fallback, name + 1);
    return length > 0 && length < (int)sizeof file ? software_file(file) : NULL;
}

/* Never enlarge artwork and retain its aspect ratio. Fast filtered
 * bilinear reduction avoids the first-open latency of large area averages. */
static SDL_Surface *resize(SDL_Surface *source, int max_w, int max_h)
{
    if (!source || (source->w <= max_w && source->h <= max_h)) {
        return source;
    }
    int width = max_w;
    int height = (int)(((int64_t)source->h * max_w + source->w / 2) / source->w);
    if (height > max_h) {
        height = max_h;
        width = (int)(((int64_t)source->w * max_h + source->h / 2) / source->h);
    }
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }
    SDL_Surface *out = mainui_image_bilinear_copy(source, width, height);
    if (!out) {
        out = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, source->format->Rmask,
                                   source->format->Gmask, source->format->Bmask,
                                   source->format->Amask);
        if (!out || SDL_SoftStretch(source, NULL, out, NULL)) {
            if (out) {
                SDL_FreeSurface(out);
            }
            return source;
        }
    }
    SDL_FreeSurface(source);
    return out;
}

static SDL_Surface *crop(SDL_Surface *source, int width, int height)
{
    if (!source || width <= 0 || height <= 0 || (source->w <= width && source->h <= height)) {
        return source;
    }
    int w = source->w < width ? source->w : width;
    int h = source->h < height ? source->h : height;
    SDL_Surface *out =
        SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, source->format->Rmask, source->format->Gmask,
                             source->format->Bmask, source->format->Amask);
    if (!out) {
        return source;
    }
    int x = (source->w - w) / 2, y = (source->h - h) / 2;
    for (int row = 0; row < h; row++) {
        memcpy((Uint8 *)out->pixels + row * out->pitch,
               (const Uint8 *)source->pixels + (y + row) * source->pitch + x * 4, (size_t)w * 4);
    }
    SDL_FreeSurface(source);
    return out;
}

/* The neighbours' picture: a copy at opacity percent, its alpha multiplied
 * once here so drawing it costs no more than any transparent picture. SDL 1.2
 * ignores a surface's own alpha for pictures with an alpha channel, so it
 * cannot be set at drawing time. All worker surfaces are 32-bit ARGB. */
static SDL_Surface *faded(SDL_Surface *source, int opacity, bool copy)
{
    if (!source || opacity >= 100 || !source->format->Amask) {
        return copy ? NULL : source;
    }
    SDL_Surface *out = source;
    if (copy) {
        out = SDL_CreateRGBSurface(SDL_SWSURFACE, source->w, source->h, 32, source->format->Rmask,
                                   source->format->Gmask, source->format->Bmask,
                                   source->format->Amask);
        if (!out) {
            return NULL;
        }
        for (int y = 0; y < source->h; y++) {
            memcpy((Uint8 *)out->pixels + y * out->pitch,
                   (const Uint8 *)source->pixels + y * source->pitch, (size_t)source->w * 4);
        }
    }
    Uint32 mask = out->format->Amask;
    int shift = out->format->Ashift;
    for (int y = 0; y < out->h; y++) {
        Uint32 *row = (Uint32 *)((Uint8 *)out->pixels + y * out->pitch);
        for (int x = 0; x < out->w; x++) {
            Uint32 alpha = (row[x] & mask) >> shift;
            row[x] = (row[x] & ~mask) | ((alpha * (Uint32)opacity / 100) << shift & mask);
        }
    }
    return out;
}

/* The neighbours' copy at scale percent, or NULL at full size. Fast
 * filtered reduction directly from the retained surface, with SDL fallback. */
static SDL_Surface *smaller(SDL_Surface *source, int scale)
{
    if (!source || scale >= 100) {
        return NULL;
    }
    int width = source->w * scale / 100, height = source->h * scale / 100;
    width = width > 0 ? width : 1;
    height = height > 0 ? height : 1;
    SDL_Surface *out = mainui_image_bilinear_copy(source, width, height);
    if (!out) {
        out = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, source->format->Rmask,
                                   source->format->Gmask, source->format->Bmask,
                                   source->format->Amask);
        if (out && SDL_SoftStretch(source, NULL, out, NULL)) {
            SDL_FreeSurface(out);
            out = NULL;
        }
    }
    return out;
}

void mainui_carousel_decode(const MainUICarouselJob *job, MainUICarouselDecoded *out)
{
    *out = (MainUICarouselDecoded){0};
    struct timespec start = mainui_timing_start();
    if (*job->thumb) {
        out->icon = software_file(job->thumb);
        if (!out->icon) {
            char fallback[MAINUI_PATH_MAX];
            if (mainui_console_thumb_default(fallback, job->thumb)) {
                out->icon = software_file(fallback);
            }
        }
        out->thumb = out->icon != NULL;
    }
    if (out->thumb) {
        struct timespec scaling = mainui_timing_start();
        out->icon = resize(out->icon, 560, job->height);
        out->small = smaller(out->icon, job->scale);
        /* Faded in place when it was scaled, else a faded copy. None at 0:
         * the neighbours are not drawn at all then. */
        if (job->opacity > 0) {
            out->small = out->small ? faded(out->small, job->opacity, false)
                                    : faded(out->icon, job->opacity, true);
        }
        else if (out->small) {
            SDL_FreeSurface(out->small);
            out->small = NULL;
        }
        mainui_timing_finish("carousel-scale-ms", scaling);
    }
    else {
        out->icon = crop(mainui_carousel_theme_icon(job->icon, job->fallback), job->crop_width,
                         job->crop_height);
        if (*job->selected && strcmp(job->selected, job->icon)) {
            out->selected = crop(mainui_carousel_theme_icon(job->selected, job->fallback),
                                 job->crop_width, job->crop_height);
        }
        /* A theme icon is a neighbour at scale_unselected too, faded alike;
         * its tile is scaled with it when drawn. */
        out->small = job->opacity > 0 ? smaller(out->icon, job->scale) : NULL;
        if (job->opacity > 0) {
            out->small = out->small ? faded(out->small, job->opacity, false)
                                    : faded(out->icon, job->opacity, true);
        }
    }
    mainui_artwork_blend(out->icon);
    mainui_artwork_blend(out->small);
    mainui_artwork_blend(out->selected);
    mainui_timing_finish("carousel-load-ms", start);
}

static int decoder(void *context)
{
    MainUICarouselWorker *worker = context;
    mainui_carousel_decode(&worker->job, &worker->decoded);
    atomic_store_explicit(&worker->done, true, memory_order_release);
    SDL_Event event = {.type = SDL_USEREVENT};
    event.user.code = MAINUI_CAROUSEL_READY_CODE;
    SDL_PushEvent(&event);
    return 0;
}

bool mainui_carousel_worker_start(MainUICarouselWorker *worker, const MainUICarouselJob *job)
{
    if (worker->thread) {
        return false;
    }
    worker->job = *job;
    worker->decoded = (MainUICarouselDecoded){0};
    atomic_store(&worker->done, false);
    worker->thread = worker->refuse ? NULL : SDL_CreateThread(decoder, worker);
    return worker->thread != NULL;
}

bool mainui_carousel_worker_take(MainUICarouselWorker *worker, MainUICarouselJob *job,
                                 MainUICarouselDecoded *decoded)
{
    if (!worker->thread || !atomic_load_explicit(&worker->done, memory_order_acquire)) {
        return false;
    }
    SDL_WaitThread(worker->thread, NULL);
    worker->thread = NULL;
    *job = worker->job;
    *decoded = worker->decoded;
    worker->decoded = (MainUICarouselDecoded){0};
    return true;
}

void mainui_carousel_decoded_free(MainUICarouselDecoded *decoded)
{
    if (decoded->icon) {
        SDL_FreeSurface(decoded->icon);
    }
    if (decoded->selected) {
        SDL_FreeSurface(decoded->selected);
    }
    if (decoded->small) {
        SDL_FreeSurface(decoded->small);
    }
    *decoded = (MainUICarouselDecoded){0};
}

void mainui_carousel_worker_close(MainUICarouselWorker *worker)
{
    if (worker->thread) {
        SDL_WaitThread(worker->thread, NULL);
        worker->thread = NULL;
    }
    mainui_carousel_decoded_free(&worker->decoded);
}
