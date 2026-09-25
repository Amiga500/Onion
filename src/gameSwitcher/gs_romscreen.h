#ifndef GAME_SWITCHER_ROMSCREEN_H
#define GAME_SWITCHER_ROMSCREEN_H

#include <SDL/SDL_image.h>
#include <stddef.h>
#include <stdio.h>

#include "system/screenshot.h"

#include "gs_model.h"
#include "gs_retroarch.h"

// Romscreens are decoded outside the lock by a persistent worker that
// prefetches the entries next to the current one, so scrolling no longer
// waits for PNG decoding on the UI thread. Only the UI thread frees
// surfaces (window eviction), so a surface it is displaying is never
// released under its feet.
#define ROMSCREEN_WINDOW 5   // keep entries within +/- this distance loaded
#define ROMSCREEN_PREFETCH 2 // decode ahead +/- this distance

static pthread_t romscreen_thread_pt;
static bool romscreen_thread_started = false;
static pthread_t romscreen_ui_thread;
static bool romscreen_ui_thread_set = false;
static pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t romscreen_cond = PTHREAD_COND_INITIALIZER;
// Serialises name/core lookups: cache_db and the RetroArch history cache
// are process-wide globals and not safe to use from two threads at once.
static pthread_mutex_t meta_mutex = PTHREAD_MUTEX_INITIALIZER;

static int romscreen_center = 0;         // current entry (UI thread)
static int romscreen_request_center = 0; // -1 = initial fill from the top
static bool romscreen_request = false;
static bool romscreen_quit = false;
static int romscreen_inflight = 0; // worker jobs touching a game_list entry
// Also fetch the play time during name/core lookup (mirrors show_time)
static bool romscreen_prefetch_play_time = false;

// Defined in gs_history.h
void processItemMetaWork(Game_s *game);

void unloadRomScreen(int index)
{
    if (index < 0 || index >= game_list_len)
        return;
    Game_s *game = &game_list[index];

    if (game->romScreen != NULL && !game->romscreen_busy) {
        SDL_FreeSurface(game->romScreen);
        game->romScreen = NULL;
    }
}

typedef enum {
    ROM_SCREEN_NONE = 0,
    ROM_SCREEN_STATE,
    ROM_SCREEN_HASH,
    ROM_SCREEN_ARTWORK
} RomScreenType_e;

RomScreenType_e findRomScreen(const Game_s *game, char *currPicture, size_t currPicture_size)
{
    if (currPicture == NULL || currPicture_size == 0)
        return ROM_SCREEN_NONE;

    // Check if hashed rom screen exists
    uint32_t hash = FNV1A_Pippip_Yurii(game->recentItem.rompath, strlen(game->recentItem.rompath));
    snprintf(currPicture, currPicture_size, ROM_SCREENS_DIR "/%" PRIu32 ".png", hash);
    printf_debug("Checking for hashed rom screen: %s\n", currPicture);
    if (exists(currPicture)) {
        return ROM_SCREEN_HASH;
    }

    // Check if artwork exists
    snprintf(currPicture, currPicture_size, "%s", game->recentItem.imgpath);
    printf_debug("Checking for artwork: %s\n", currPicture);
    if (exists(currPicture)) {
        return ROM_SCREEN_ARTWORK;
    }

    return ROM_SCREEN_NONE;
}

typedef struct {
    bool keepAspect;
    bool integerScaling;
} ScalingMode_s;

// Returns the scaled surface and frees `src` (NULL if zooming failed).
SDL_Surface *scaleRomScreenSurface(SDL_Surface *src, ScalingMode_s mode)
{
    if (src == NULL || src->w <= 0 || src->h <= 0)
        return src;

    // Zoom the image to fit the screen
    double zx = (double)(DISPLAY_WIDTH) / src->w;
    double zy = (double)(DISPLAY_HEIGHT) / src->h;

    if (mode.integerScaling) {
        zx = (int)zx;
        zy = (int)zy;
    }

    // Scale the image to fit application window
    zx *= (double)g_display.width / (double)(DISPLAY_WIDTH);
    zy *= (double)g_display.height / (double)(DISPLAY_HEIGHT);

    if (mode.keepAspect) {
        if (zx < zy)
            zy = zx;
        else
            zx = zy;
    }

    SDL_Surface *zoomed = zoomSurface(src, zx, zy, SMOOTHING_OFF);
    SDL_FreeSurface(src);
    return zoomed;
}

void scaleRomScreen(Game_s *game, ScalingMode_s mode)
{
    game->romScreen = scaleRomScreenSurface(game->romScreen, mode);
}

ScalingMode_s getDynamicScalingMode(const Game_s *game)
{
    return (ScalingMode_s){
        ra_getConfigOverrideOption(game, ASPECT_RATIO_OPTION, true),
        ra_getConfigOverrideOption(game, INTEGER_SCALING_OPTION, false),
    };
}

// Decode and scale a romscreen. Called without thread_mutex held; only
// reads fields that are stable once the entry's meta_state is READY.
static SDL_Surface *_decodeRomScreen(const Game_s *game)
{
    char currPicture[STR_MAX * 2];
    RomScreenType_e romScreenType = findRomScreen(game, currPicture, sizeof(currPicture));
    if (romScreenType == ROM_SCREEN_NONE)
        return NULL;

    SDL_Surface *surface = IMG_Load(currPicture);
    if (surface == NULL) {
        printf_debug("Error loading image: %s\n", currPicture);
        return NULL;
    }

    if (romScreenType == ROM_SCREEN_STATE)
        return scaleRomScreenSurface(surface, getDynamicScalingMode(game));
    return scaleRomScreenSurface(surface, (ScalingMode_s){true, false});
}

static bool _isRomScreenUiThread(void)
{
    return romscreen_ui_thread_set && pthread_equal(pthread_self(), romscreen_ui_thread);
}

// UI thread only: free what is too far from the current entry.
static void _evictRomScreens(int center)
{
    for (int i = 0; i < game_list_len; i++) {
        if (i < center - ROMSCREEN_WINDOW || i > center + ROMSCREEN_WINDOW)
            unloadRomScreen(i);
    }
}

// Blocking load, used by the UI thread for the entry it is about to show.
SDL_Surface *loadRomScreen(int index)
{
    if (index < 0 || index >= game_list_len)
        return NULL;

    Game_s *game = &game_list[index];

    pthread_mutex_lock(&thread_mutex);

    // The worker may be decoding this very entry: wait for its result
    // instead of decoding it a second time.
    while (game->romscreen_busy)
        pthread_cond_wait(&romscreen_cond, &thread_mutex);

    if (game->romScreen == NULL && game->processed) {
        game->romscreen_busy = true;
        pthread_mutex_unlock(&thread_mutex);

        SDL_Surface *surface = _decodeRomScreen(game);

        pthread_mutex_lock(&thread_mutex);
        game->romscreen_busy = false;
        if (game->romScreen == NULL)
            game->romScreen = surface;
        else if (surface != NULL)
            SDL_FreeSurface(surface);
        pthread_cond_broadcast(&romscreen_cond);
    }

    if (_isRomScreenUiThread()) {
        romscreen_center = index;
        _evictRomScreens(index);
    }

    SDL_Surface *result = game->romScreen;
    pthread_mutex_unlock(&thread_mutex);

    return result;
}

// Name/core lookup for one entry, shared by the UI thread and the worker.
// Returns once the entry is READY (waits if the other thread is on it).
void romscreen_ensureMeta(Game_s *game)
{
    pthread_mutex_lock(&thread_mutex);
    while (game->meta_state == GAME_META_BUSY)
        pthread_cond_wait(&romscreen_cond, &thread_mutex);
    if (game->meta_state == GAME_META_READY) {
        pthread_mutex_unlock(&thread_mutex);
        return;
    }
    game->meta_state = GAME_META_BUSY;
    pthread_mutex_unlock(&thread_mutex);

    pthread_mutex_lock(&meta_mutex);
    processItemMetaWork(game);
    pthread_mutex_unlock(&meta_mutex);

    pthread_mutex_lock(&thread_mutex);
    game->processed = true;
    game->meta_state = GAME_META_READY;
    pthread_cond_broadcast(&romscreen_cond);
    pthread_mutex_unlock(&thread_mutex);
}

static void *_romScreenWorker(void *_)
{
    (void)_;
    pthread_mutex_lock(&thread_mutex);

    while (!romscreen_quit) {
        while (!romscreen_quit && !romscreen_request)
            pthread_cond_wait(&romscreen_cond, &thread_mutex);
        if (romscreen_quit)
            break;

        int center = romscreen_request_center;
        romscreen_request = false;

        int order[ROMSCREEN_WINDOW + 1];
        int count = 0;
        if (center < 0) {
            for (int i = 0; i <= ROMSCREEN_WINDOW; i++)
                order[count++] = i;
        }
        else {
            order[count++] = center;
            for (int d = 1; d <= ROMSCREEN_PREFETCH; d++) {
                order[count++] = center + d;
                order[count++] = center - d;
            }
        }

        for (int k = 0; k < count; k++) {
            if (romscreen_quit || romscreen_request)
                break; // a newer request replaces this one

            int idx = order[k];
            if (idx < 0 || idx >= game_list_len)
                continue;
            Game_s *game = &game_list[idx];

            if (game->meta_state == GAME_META_NEW) {
                game->meta_state = GAME_META_BUSY;
                romscreen_inflight++;
                pthread_mutex_unlock(&thread_mutex);

                pthread_mutex_lock(&meta_mutex);
                processItemMetaWork(game);
                pthread_mutex_unlock(&meta_mutex);

                pthread_mutex_lock(&thread_mutex);
                game->processed = true;
                game->meta_state = GAME_META_READY;
                romscreen_inflight--;
                pthread_cond_broadcast(&romscreen_cond);
            }

            if (romscreen_quit || romscreen_request)
                break;

            if (game->romScreen == NULL && game->processed && !game->romscreen_busy) {
                game->romscreen_busy = true;
                romscreen_inflight++;
                pthread_mutex_unlock(&thread_mutex);

                SDL_Surface *surface = _decodeRomScreen(game);

                pthread_mutex_lock(&thread_mutex);
                game->romscreen_busy = false;
                romscreen_inflight--;
                bool in_window = idx >= romscreen_center - ROMSCREEN_WINDOW &&
                                 idx <= romscreen_center + ROMSCREEN_WINDOW;
                if (game->romScreen == NULL && in_window)
                    game->romScreen = surface;
                else if (surface != NULL)
                    SDL_FreeSurface(surface);
                pthread_cond_broadcast(&romscreen_cond);
            }
        }
    }

    pthread_mutex_unlock(&thread_mutex);
    return NULL;
}

// Ask the worker to prefetch around `center` (-1: the first entries).
void romscreen_prefetch(int center)
{
    if (!romscreen_thread_started)
        return;
    pthread_mutex_lock(&thread_mutex);
    romscreen_request_center = center;
    romscreen_request = true;
    pthread_cond_broadcast(&romscreen_cond);
    pthread_mutex_unlock(&thread_mutex);
}

// Called by the UI thread before it reorders game_list: waits until the
// worker is not touching any entry. Must be paired with romscreen_unlock().
void romscreen_lockForUpdate(void)
{
    pthread_mutex_lock(&thread_mutex);
    while (romscreen_inflight > 0)
        pthread_cond_wait(&romscreen_cond, &thread_mutex);
}

void romscreen_unlock(void)
{
    pthread_cond_broadcast(&romscreen_cond);
    pthread_mutex_unlock(&thread_mutex);
}

static void romscreen_stopWorker(void)
{
    if (!romscreen_thread_started)
        return;
    pthread_mutex_lock(&thread_mutex);
    romscreen_quit = true;
    pthread_cond_broadcast(&romscreen_cond);
    pthread_mutex_unlock(&thread_mutex);
    pthread_join(romscreen_thread_pt, NULL);
    romscreen_thread_started = false;
}

void freeRomScreens()
{
    romscreen_stopWorker();

    for (int i = 0; i < game_list_len; i++) {
        Game_s *game = &game_list[i];

        if (game->romScreen != NULL) {
            SDL_FreeSurface(game->romScreen);
            game->romScreen = NULL;
        }
    }
}

// Start the background loader (once) and fill the first entries.
// Call from the UI thread after game_list has been read.
void loadRomScreens()
{
    if (!romscreen_ui_thread_set) {
        romscreen_ui_thread = pthread_self();
        romscreen_ui_thread_set = true;
    }

    if (!romscreen_thread_started) {
        romscreen_quit = false;
        if (pthread_create(&romscreen_thread_pt, NULL, _romScreenWorker, NULL) == 0)
            romscreen_thread_started = true;
    }

    romscreen_prefetch(-1);
}

#endif // GAME_SWITCHER_ROMSCREEN_H
