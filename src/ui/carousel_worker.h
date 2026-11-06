/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_CAROUSEL_WORKER_H
#define MAINUI_CAROUSEL_WORKER_H
#include "catalog/catalog.h"
#include <SDL.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#define MAINUI_CAROUSEL_READY_CODE 0x4341

typedef struct {
    char thumb[MAINUI_PATH_MAX];
    char icon[MAINUI_PATH_MAX];
    char selected[MAINUI_PATH_MAX];
    char fallback[MAINUI_PATH_MAX];
    int height, scale, opacity, crop_width, crop_height;
    uint64_t key;
} MainUICarouselJob;

typedef struct {
    SDL_Surface *icon, *selected, *small;
    bool thumb;
} MainUICarouselDecoded;

typedef struct {
    SDL_Thread *thread;
    atomic_bool done;
    MainUICarouselJob job;         /* Immutable until the worker is joined. */
    MainUICarouselDecoded decoded; /* Worker writes; UI reads only after done. */
    bool refuse;                   /* tests: start fails, as when no thread can be created */
} MainUICarouselWorker;

/* The worker reads only copied paths and produces software surfaces. */
/* Load a theme console icon, including the theme's default-icon fallback.
 * The returned surface is owned by the caller and has a software format. */
SDL_Surface *mainui_carousel_theme_icon(const char *path, const char *fallback);
void mainui_carousel_decode(const MainUICarouselJob *job, MainUICarouselDecoded *out);
bool mainui_carousel_worker_start(MainUICarouselWorker *worker, const MainUICarouselJob *job);
bool mainui_carousel_worker_take(MainUICarouselWorker *worker, MainUICarouselJob *job,
                                 MainUICarouselDecoded *decoded);
void mainui_carousel_worker_close(MainUICarouselWorker *worker);
void mainui_carousel_decoded_free(MainUICarouselDecoded *decoded);
#endif
