#ifndef GAME_SWITCHER_OVERLAY_H
#define GAME_SWITCHER_OVERLAY_H

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "system/battery.h"
#include "system/screenshot.h"
#include "utils/msleep.h"
#include "utils/process.h"
#include "utils/str.h"

#include "gs_appState.h"
#include "gs_content_match.h"
#include "gs_model.h"
#include "gs_render.h"
#include "gs_romscreen.h"

static pthread_t autosave_thread_pt;
static pthread_t romscreen_save_thread_pt;
// Thread-start flags belong to the main thread; never use a worker's
// completion flag as a substitute for pthread_join().
static bool autosave_thread_started = false;
static bool romscreen_save_thread_started = false;
static pthread_mutex_t autosave_status_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool autosave_done = false;
// Owned by the snapshot: dimensions are captured with the pixels so the PNG
// writer never consults g_display (which other code may update).
static uint32_t *romscreen_save_pixels = NULL;
static uint32_t romscreen_save_width = 0;
static uint32_t romscreen_save_height = 0;
static char romscreen_save_path[STR_MAX] = {0};
static pid_t overlay_retroarch_pid = 0;

// RetroArch computes the CRC32 of content that the core opens itself (arcade
// zips such as Neo Geo sets) on the first GET_STATUS, reading the whole file
// before it answers: seconds for a large set. Waiting for that one reply
// keeps a slow answer from looking like a missing RetroArch.
#define OVERLAY_STATUS_TIMEOUT_MS 15000

static bool _processPidExists(pid_t pid);
static bool _overlayRetroArchAlive(void *ctx)
{
    (void)ctx;
    return overlay_retroarch_pid <= 0 || _processPidExists(overlay_retroarch_pid);
}

void setFbAsFirstRomScreen(void)
{
    if (game_list_len == 0)
        return;

    Game_s *game = &game_list[0];

    if (game->romScreen != NULL) {
        SDL_FreeSurface(game->romScreen);
        game->romScreen = NULL;
    }

    game->romScreen = SDL_CreateRGBSurface(SDL_SWSURFACE, g_display.width, g_display.height, 32, 0, 0, 0, 0);
    if (game->romScreen == NULL) {
        print_debug("Error creating fb surface\n");
        return;
    }

    rect_t full = {0, 0, g_display.width, g_display.height};

    // A 32 bpp software surface has rows of exactly width pixels: read the
    // framebuffer straight into it (no 1.2 MB staging buffer and copy).
    if ((size_t)game->romScreen->pitch == (size_t)g_display.width * sizeof(uint32_t)) {
        display_readCurrentBuffer(&g_display, (uint32_t *)game->romScreen->pixels, full, true, false);
        return;
    }

    size_t pixel_count = (size_t)g_display.width * (size_t)g_display.height;
    uint32_t *fb_pixels = (uint32_t *)malloc(pixel_count * sizeof(uint32_t));
    if (fb_pixels == NULL) {
        print_debug("Error allocating fb capture buffer\n");
        return;
    }

    display_readCurrentBuffer(&g_display, fb_pixels, full, true, false);

    for (int y = 0; y < g_display.height; y++) {
        memcpy((uint8_t *)game->romScreen->pixels + (size_t)y * (size_t)game->romScreen->pitch,
               fb_pixels + (size_t)y * (size_t)g_display.width,
               (size_t)g_display.width * sizeof(uint32_t));
    }

    free(fb_pixels);
}

// Spawn a detached playActivity process without blocking the UI thread.
// Double-fork: the first child exits immediately, so the waitpid below
// returns at once and the grandchild is reparented to init (no zombies).
static void _playActivityAsync(const char *action)
{
    pid_t pid = fork();
    if (pid == 0) {
        pid_t child = fork();
        if (child == 0) {
            execl("/mnt/SDCARD/.tmp_update/bin/playActivity", "playActivity", action, NULL);
            _exit(127);
        }
        _exit(child > 0 ? 0 : 127);
    }
    else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
    else {
        printf_debug("fork failed for playActivity %s\n", action);
    }
}

static bool _autosavePending(void)
{
    pthread_mutex_lock(&autosave_status_mutex);
    bool pending = !autosave_done;
    pthread_mutex_unlock(&autosave_status_mutex);
    return pending;
}

static void *_writeRomScreenThread(void *arg)
{
    (void)arg;
    if (romscreen_save_pixels != NULL && romscreen_save_path[0] != '\0') {
        if (screenshot_save_buffer_atomic(romscreen_save_pixels, romscreen_save_path,
                                          romscreen_save_width, romscreen_save_height))
            printf_debug("Saved rom screen: %s\n", romscreen_save_path);
        else
            printf_debug("Failed to save rom screen: %s\n", romscreen_save_path);
    }
    free(romscreen_save_pixels);
    romscreen_save_pixels = NULL;
    romscreen_save_width = romscreen_save_height = 0;
    romscreen_save_path[0] = '\0';
    return NULL;
}

static void *_autosaveFirstThread(void *arg)
{
    (void)arg;
    retroarch_autosave();
    pthread_mutex_lock(&autosave_status_mutex);
    autosave_done = true;
    pthread_mutex_unlock(&autosave_status_mutex);

    // PNG compression and fsync proceed independently of RetroArch shutdown.
    // Publication of thread_started is synchronized by joining this thread.
    if (romscreen_save_pixels != NULL) {
        if (pthread_create(&romscreen_save_thread_pt, NULL,
                           _writeRomScreenThread, NULL) == 0)
            romscreen_save_thread_started = true;
        else
            _writeRomScreenThread(NULL); // low-memory fallback
    }
    return NULL;
}

static void _finishAutosave(bool show_saving)
{
    if (!autosave_thread_started)
        return;
    bool pending = _autosavePending();
    if (pending && show_saving)
        render_showFullscreenMessage("SAVING", false);
    pthread_join(autosave_thread_pt, NULL);
    autosave_thread_started = false;
}

// ORDERING INVARIANT: romscreen_save_thread_started is written by the
// autosave worker, so only read it after _finishAutosave() has joined that
// worker. Use overlay_finishWorkers() rather than calling this directly.
static void _finishRomScreen(void)
{
    if (autosave_thread_started)
        _finishAutosave(false); // enforce the invariant above
    if (romscreen_save_thread_started) {
        pthread_join(romscreen_save_thread_pt, NULL);
        romscreen_save_thread_started = false;
    }
}

static void _snapshotRomScreen(Game_s *game)
{
    if (game == NULL || !game->is_running || game->romScreen == NULL)
        return;
    SDL_Surface *src = game->romScreen;
    size_t width = (size_t)g_display.width;
    size_t height = (size_t)g_display.height;
    if (!width || !height || src->format->BitsPerPixel != 32 ||
        src->w < (int)width || src->h < (int)height ||
        (size_t)src->pitch < width * sizeof(uint32_t))
        return;
    if (width > SIZE_MAX / sizeof(uint32_t) / height)
        return;

    uint32_t *copy = (uint32_t *)malloc(width * height * sizeof(uint32_t));
    if (copy == NULL) {
        print_debug("Not enough memory for romscreen snapshot");
        return; // State save still proceeds; don't dereference a freed surface.
    }
    for (size_t y = 0; y < height; y++) {
        memcpy(copy + y * width,
               (const uint8_t *)src->pixels + y * (size_t)src->pitch,
               width * sizeof(uint32_t));
    }
    uint32_t hash = FNV1A_Pippip_Yurii(game->recentItem.rompath,
                                       strlen(game->recentItem.rompath));
    snprintf(romscreen_save_path, sizeof(romscreen_save_path),
             ROM_SCREENS_DIR "/%" PRIu32 ".png", hash);
    romscreen_save_width = (uint32_t)width;
    romscreen_save_height = (uint32_t)height;
    romscreen_save_pixels = copy;
}

// Block until the RetroArch state save has finished (shows SAVING if it has
// not). Idempotent; a no-op when no autosave is running.
void overlay_waitForAutosave(void)
{
    _finishAutosave(true);
}

// Join every worker this file started. Idempotent; call on any path that
// leaves main() so a PNG or state save is never cut off by process exit.
void overlay_finishWorkers(void)
{
    _finishAutosave(false);
    _finishRomScreen();
}

void overlay_init()
{
    if (!appState.is_overlay) {
        return;
    }

    // Remember the RetroArch instance this overlay belongs to.
    overlay_retroarch_pid = 0;
    for (int i = 0; i < 10 && overlay_retroarch_pid <= 0; i++) {
        overlay_retroarch_pid = process_searchpid("retroarch");
        if (overlay_retroarch_pid <= 0)
            msleep(20);
    }
    if (overlay_retroarch_pid <= 0)
        print_debug("Unable to capture RetroArch PID\n");

    bool retroarch_cmd_ready = retroarch_pause() == 0;
    _playActivityAsync("stop_all");
    setFbAsFirstRomScreen();
    RetroArchStatus_s status;
    int status_result = retroarch_cmd_ready
                            ? retroarch_getStatusWait(&status, OVERLAY_STATUS_TIMEOUT_MS,
                                                      _overlayRetroArchAlive, NULL)
                            : -1;
    if (status_result == -1)
        status_result = retroarch_getStatus(&status);
    if (status_result == -1) {
        print_debug("Error getting RetroArch status");
        return;
    }

    printf_debug("RetroArch status: %d\n", status.state);
    printf_debug("Content info: %s\n", status.content_info);

    if (status.state == RETROARCH_STATE_CONTENTLESS || status.state == RETROARCH_STATE_UNKNOWN) {
        print_debug("RetroArch is not running a game");
        return;
    }

    if (status.state == RETROARCH_STATE_PLAYING) {
        // Listener responded successfully: avoid another VERSION handshake.
        if (retroarch_pauseNoProbe() != 0)
            retroarch_pause();
    }

    Game_s *game = &game_list[0];
    game->is_running = _isContentNameInInfo(status.content_info, game->rom_name);
    printf_debug("Game is running: %d\n", game->is_running);

    // Keep a private pixel copy before any history update/eviction. The PNG
    // worker never accesses game_list or an SDL_Surface afterwards.
    _snapshotRomScreen(game);
    pthread_mutex_lock(&autosave_status_mutex);
    autosave_done = false;
    pthread_mutex_unlock(&autosave_status_mutex);
    if (pthread_create(&autosave_thread_pt, NULL, _autosaveFirstThread, NULL) == 0)
        autosave_thread_started = true;
    else
        _autosaveFirstThread(NULL); // still save on low-memory/thread failure
}

void overlay_resume(void)
{
    if (appState.is_overlay) {
        // Wait only for state safety. The screenshot may still be writing.
        SDL_Surface *screen_backup = NULL;
        if (autosave_thread_started && _autosavePending()) {
            screen_backup = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h,
                                                 32, 0, 0, 0, 0);
            if (screen_backup != NULL)
                SDL_BlitSurface(screen, NULL, screen_backup, NULL);
        }
        _finishAutosave(true);
        if (screen_backup != NULL) {
            SDL_BlitSurface(screen_backup, NULL, screen, NULL);
            SDL_FreeSurface(screen_backup);
        }

        render();
        retroarch_unpause();
        _playActivityAsync("resume");
        _finishRomScreen();
        msleep(200);

        remove("/mnt/SDCARD/.tmp_update/.runGameSwitcher");
    }
}

static bool _processPidExists(pid_t pid)
{
    if (pid <= 0)
        return false;

    if (kill(pid, 0) == 0)
        return true;

    return errno == EPERM;
}

void overlay_exit(void)
{
    if (appState.is_overlay) {
        _finishAutosave(true);

        pid_t retroarch_pid = overlay_retroarch_pid;
        overlay_retroarch_pid = 0;

        if (retroarch_pid <= 0) {
            // The PID was never captured, so fall back to the old broad kill
            // rather than leaving RetroArch running.
            print_debug("No RetroArch PID captured, falling back to killall");
            process_killall_signal("retroarch", SIGTERM);

            // wait up to 5 seconds for RetroArch to exit
            for (int i = 0; i < 10; i++) {
                msleep(500); // 0.5s x 10 = 5s
                if (!process_isRunning("retroarch")) {
                    break; // retroarch is gone
                }
            }

            if (process_isRunning("retroarch")) {
                print_debug("RetroArch still running, force killing...");
                temp_flag_set(".forceKillRetroarch", true);
                process_killall("retroarch");
            }
        }
        else if (_processPidExists(retroarch_pid)) {
            kill(retroarch_pid, SIGTERM);

            // Preserve the five-second grace period; detect normal exit sooner.
            for (int i = 0; i < 25 && _processPidExists(retroarch_pid); i++)
                msleep(20);
            for (int i = 0; i < 45 && _processPidExists(retroarch_pid); i++)
                msleep(100);

            if (_processPidExists(retroarch_pid)) {
                printf_debug("RetroArch PID %d still running, force killing...\n",
                             retroarch_pid);
                temp_flag_set(".forceKillRetroarch", true);
                kill(retroarch_pid, SIGKILL);
            }
        }
        // Do not hold RetroArch shutdown behind PNG encoding / SD fsync.
        _finishRomScreen();
    }
}
#endif // GAME_SWITCHER_OVERLAY_H
