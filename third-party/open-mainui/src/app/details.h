/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_DETAILS_H
#define MAINUI_DETAILS_H
#include "catalog/gamelist.h"
#include "ui/media_viewer.h"
#include "ui/preview.h"

/* Owns copied text and metadata; borrows the list preview and source models.
 * UI-thread only. Close before changing/freeing a source model or theme. */
typedef struct {
    bool open, favorite;
    int scroll_line, lines_per_page, line_count, ordinal, total;
    /* Description is the only scrolling region. Cached UTF-8 line offsets
     * belong to this Game Info session and are freed on close. */
    unsigned short *description_lines;
    char *summary; /* preformatted genre/rating; owned with the Game Info session */
    TTF_Font *description_font;
    int description_y, description_line_height, scroll_px, scroll_max_px;
    int auto_delay, auto_speed;
    Uint32 auto_epoch, auto_hold_at;
    bool auto_started, auto_paused, auto_finished, auto_held;
    SDLKey swallow_page_key;
    char title[4096], system[4096], rom[MAINUI_PATH_MAX];
    MainUIMetadata metadata;
    MainUIPreview *preview;
    /* The shown game, for its cover: borrowed like the list models. */
    MainUICatalog *catalog;
    const MainUILibrary *library;
    int index;
    /* For the media gallery: the ROM on this SD and its console's ROM folder
     * ("" when the console is not known). */
    char host_rom[MAINUI_PATH_MAX], rom_root[MAINUI_PATH_MAX];
    char launch[MAINUI_PATH_MAX]; /* the game's launcher, a host path, "" for none */
    MainUIMediaViewer media;
} MainUIDetails;

bool mainui_details_open(MainUIDetails *details, MainUICatalog *catalog,
                         const MainUILibrary *library, const MainUILibrary *favorites, int row,
                         MainUIPreview *preview, const MainUIConfig *config);
/* The ROM file of a list row's game, on this host, and its console's ROM
 * folder, found as Game Info finds them (the media folder is there). False
 * for folders and rows that are not games. */
/* The game at row's ROM and launcher as host paths, without opening Game
 * Info: false for a folder or a game without a launcher. */
bool mainui_details_launcher(MainUICatalog *catalog, const MainUILibrary *library, int row,
                             char host_rom[MAINUI_PATH_MAX], char launch[MAINUI_PATH_MAX]);
bool mainui_details_locate(MainUICatalog *catalog, const MainUILibrary *library, int row,
                           char host_rom[MAINUI_PATH_MAX], char rom_root[MAINUI_PATH_MAX]);
/* Up/Down wraps over games only and synchronizes the source list viewport.
 * B/Left closes. L2/R2 moves one description page. */
void mainui_details_key(MainUIDetails *details, MainUICatalog *catalog,
                        const MainUILibrary *library, const MainUILibrary *favorites,
                        MainUIViewport *view, int rows, SDLKey key, const MainUIConfig *config);
/* Before drawing: advance the cover request without waiting (wait_ms 0), so a
 * slow decode never holds up input; it is drawn when it is done. Snapshots
 * pass MAINUI_PREVIEW_WAIT_FOREVER. */
void mainui_details_progress(MainUIDetails *details, Uint32 wait_ms);
/* The Favorite markers were reloaded: update the shown star. */
void mainui_details_markers(MainUIDetails *details, const MainUILibrary *favorites);
/* Precompute layout before queued page navigation, even when a frame is skipped. */
void mainui_details_prepare(MainUIDetails *details, MainUITheme *theme);
void mainui_details_draw(MainUIDetails *details, MainUITheme *theme, SDL_Surface *screen);
/* Open the game's media as start says (MAINUI_MEDIA_START_*: Right's gallery,
 * Select's menu, Left's Saved Games; see media_viewer.h), with the menu's
 * order and Saved Games from options, NULL for neither. False, with nothing
 * open, when the game has none of it. */
bool mainui_details_media(MainUIDetails *details, const char *sd, SDL_Surface *screen, int start,
                          const MainUIMediaOptions *options);
/* Reset manual and automatic description scrolling on return from media. */
void mainui_details_reset_scroll(MainUIDetails *details);
/* UI-thread animation. Due returns -1 when idle. Hold freezes its clock for
 * sleep, dialogs and the media gallery; it never starts a worker or timer. */
void mainui_details_auto_hold(MainUIDetails *details, bool hold, Uint32 now);
int mainui_details_auto_due(const MainUIDetails *details, Uint32 now);
bool mainui_details_auto_step(MainUIDetails *details, Uint32 now);
/* Also closes the gallery, stopping any video. */
void mainui_details_close(MainUIDetails *details);
#endif
