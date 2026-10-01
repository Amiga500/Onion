/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_PREVIEW_H
#define MAINUI_PREVIEW_H
#include "catalog/catalog.h"
#include "catalog/library.h"
#include "ui/theme.h"
#include <stdatomic.h>

#define MAINUI_THUMBNAIL_CACHE_SIZE 32

/* Owns up to 32 scaled thumbnails (at most about 11 MiB at 32 bpp). The selected
 * image borrows a cache surface. Failed reads occupy a slot too. */
typedef struct {
    SDL_Surface *image;

    struct {
        char key[MAINUI_PATH_MAX];
        SDL_Surface *image;
        uint64_t used;
    } cache[MAINUI_THUMBNAIL_CACHE_SIZE];

    uint64_t clock;
    char key[MAINUI_PATH_MAX];
    SDL_Thread *thread;
    atomic_bool done;
    SDL_Surface *decoded;
    char loading[MAINUI_PATH_MAX];
    Uint32 changed_at;
    bool pending;
    const MainUICatalog *prefetch_catalog;
    const MainUILibrary *prefetch_library;
    char prefetch_directory[MAINUI_PATH_MAX];
    int prefetch_selected, prefetch_folder, prefetch_next;
} MainUIPreview;

/* Decode uncached selection first, then n+1, n+2, n-1 and n-2 on one worker.
 * Neighbors share the bounded cache. Only the UI thread publishes pixels.
 * Synchronous mode is for deterministic snapshots. Close joins outstanding work. */
void mainui_preview_request(MainUIPreview *, MainUICatalog *, const MainUILibrary *, int selected,
                            bool synchronous);
/* Wait up to wait_ms for the selected cover, then fall back to the asynchronous
 * path. 0 never waits; MAINUI_PREVIEW_WAIT_FOREVER decodes synchronously. */
#define MAINUI_PREVIEW_WAIT_FOREVER UINT32_MAX
void mainui_preview_request_within(MainUIPreview *, MainUICatalog *, const MainUILibrary *,
                                   int selected, Uint32 wait_ms);
void mainui_preview_update(MainUIPreview *preview, MainUICatalog *catalog,
                           const MainUILibrary *library, int selected);
int mainui_preview_edge(const MainUITheme *theme);
void mainui_preview_draw(MainUIPreview *preview, const MainUITheme *theme, SDL_Surface *screen);
void mainui_preview_close(MainUIPreview *preview);
#endif
