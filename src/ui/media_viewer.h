/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_MEDIA_VIEWER_H
#define MAINUI_MEDIA_VIEWER_H
#include "menus/game_menu.h"
#include "platform/save_states.h"
#include "platform/video.h"
#include "ui/media_maps.h"
#include "ui/media_text.h"
#include "ui/theme.h"
#include <SDL.h>
#include <stdbool.h>

/* Game Info's fullscreen gallery: a game's front and back covers, 3D box,
 * screenshots and video from the media folder of its console's ROM folder,
 * matched by the ROM file's name without its extension:
 *
 *   media/box2dfront/<name>.png
 *   media/box2dback/<name>.png
 *   media/box3d/<name>.png
 *   media/screenshot/<name>.png, then <name>-1.png, <name>-2.png, ...
 *   media/videos/<name>.(mp4|mkv|avi|mov|webm|m4v), extension case-insensitive
 *   media/maps/<name>.png, <name>-1.png, <name>-2.png, ... (separate viewer)
 *   media/manuals/<name>.txt (separate walkthrough reader)
 *   media/manuals/<name>.pdf (optional external Green reader)
 *
 * The folders are named as Skraper names them by default, so its media can
 * be copied as it comes.
 *
 * Imgs, the list's and Game Info's cover, is not part of it. Separate from
 * the list's cover cache: one image or video at a time, read when shown and
 * freed when another is shown or the gallery closes. UI thread only. */

enum {
    MAINUI_MEDIA_IMAGE,
    MAINUI_MEDIA_VIDEO
};

/* L and R jump this far in a video. */
#define MAINUI_MEDIA_SEEK_MS 5000

/* At most 20 screenshots: <name>.png and <name>-1.png on. */
#define MAINUI_MEDIA_SCREENSHOTS 20
#define MAINUI_MEDIA_MAX (MAINUI_MEDIA_SCREENSHOTS + 4)

typedef struct {
    int kind;
    char path[4096];
} MainUIMediaItem;

enum {
    MEDIA_RETURN_DETAILS,
    MEDIA_RETURN_GALLERY,
    MEDIA_RETURN_MAPS,
    MEDIA_RETURN_TEXT,
    MEDIA_RETURN_SAVES
};

/* How media opens: Right in Game Info (the gallery, else the menu), Select
 * there (the menu), or Left in a game list (Saved Games). */
enum {
    MAINUI_MEDIA_START_GALLERY,
    MAINUI_MEDIA_START_MENU,
    MAINUI_MEDIA_START_SAVES
};

/* What game-menu.json and the game add: the menu's rows in order
 * (MAINUI_DETAILS_*; Saved Games only when listed), the game's launcher (a
 * host path) to find its core and save states by, whether the rolling
 * backup's Previous Session is offered, whether Auto Save is, and whether a
 * save's picture smaller than the screen is enlarged to it, and whether it
 * is framed between the header and a footer rather than over the screen. */
typedef struct {
    int order_count, order[MAINUI_DETAILS_ROWS];
    const char *launch;
    bool previous, autosave, upscale, correct_aspect, framed;
    bool gallery_upscale; /* a gallery image smaller than the screen enlarged to it */
    /* disabled_rom_dirs: a game in one of these folders has no Saved Games */
    const MainUIGameMenu *game_menu;
} MainUIMediaOptions;

typedef struct {
    bool open, image_failed;
    int count, index;
    /* Gallery, media selection, or game maps. Maps are kept separate so
     * existing screenshot/video order and video preview remain unchanged. */
    int mode, menu_selection, map_count, map_index, map_pan_x, map_pan_y;
    bool map_zoom;
    /* Where B and Select in the menu go back to (MEDIA_RETURN_*), whether B
     * in a map or the walkthrough leads back to the gallery rather than Game
     * Info, and whether the menu paused the gallery's video. */
    int menu_return;
    bool menu_paused;
    /* The walkthrough's search: its text, kept while it is open; A asks the
     * application for the keyboard (search_requested), and the match found
     * is at byte search_at, moved to the top when pending. */
    char search[128];
    bool search_requested, search_found, search_pending, search_missing;
    /* The menu's rows in order (MAINUI_DETAILS_*), the game's save states
     * (read when media opens, NULL without any), the one shown, and A on it
     * (resume_requested, for the application). from_list: opened by Left in a
     * game list, so B there goes back to the list. key_held: the key given to
     * mainui_media_key() is a held key's repeat. edge_exit: opened by Right or
     * Left, not from the media menu, so Left or Right at the far end leaves.
     * held_left: the Left that opened Saved Games may still repeat.
     * save_upscale: a save's picture is enlarged to the screen, not only
     * fitted; save_auto: Auto Save is listed; save_aspect_width/height:
     * optional console display ratio for save previews only (zero retains PNG
     * ratio); save_framed: a save is fitted between the header and its own
     * footer, else shown over the whole screen. */
    int order_count, order[MAINUI_DETAILS_ROWS];
    MainUISaves *saves;
    int save_index;
    bool resume_requested, from_list, edge_exit, key_held, held_left, save_upscale, save_auto;
    int save_aspect_width, save_aspect_height;
    bool gallery_upscale;
    bool save_framed;
    size_t search_at;
    Uint32 search_missing_at;
    char manual_path[4096]; /* exact media/manuals path, if present */
    char pdf_path[4096];    /* exact PDF path; no reader loaded inside MainUI */
    bool pdf_requested;     /* A on Manual; consumed by the application */
    MainUITextReader *text; /* allocated and read only when opened */
    int text_scroll;        /* queued lines, applied after layout in draw */
    MainUIMap *maps;        /* owned while open, up to MAINUI_MEDIA_MAPS_MAX */
    MainUIMediaItem *items; /* owned while open */
    SDL_Surface *image;     /* the shown image, fitted to the screen */
    MainUIVideo *video;     /* the shown video while it plays */
    bool hooked;            /* its sound replaces the theme's music */
    bool sleeping;          /* the device sleeps: a video stands still */
    Uint32 shown_at;
    /* A video shown waits MAINUI_MEDIA_VIDEO_DELAY_MS before it starts, so
     * moving past it never opens a decoder; any move cancels it. */
    bool video_pending;
    Uint32 video_at;
    char library_dir[4096]; /* Onion's FFmpeg: <sd>/.tmp_update/lib/parasyte */
} MainUIMediaViewer;

/* The game's media in gallery order, without reading file contents. A
 * missing screenshot number from -2 on ends the numbered ones. Returns the
 * count. Video discovery scans the videos directory only when the
 * lowercase .mp4 file is absent; no scanning occurs during drawing. */
int mainui_media_find(const char *rom_root, const char *rom, MainUIMediaItem *items, int max);
/* The preferred regular video in media/videos for the exact ROM basename.
 * Format priority: mp4, mkv, avi, mov, webm, m4v (case-insensitive). */
bool mainui_media_video(const char *rom_root, const char *rom, char path[4096]);
/* Open media as start says (MAINUI_MEDIA_START_*): the gallery on its first
 * item; the menu, when it has a row; or Saved Games on their first entry.
 * False, with nothing open, when the game has none of it. sd is the SD
 * card's root, for Onion's FFmpeg in .tmp_update/lib/parasyte and the save
 * states; options NULL is every row but Saved Games. */
bool mainui_media_open(MainUIMediaViewer *viewer, const char *rom_root, const char *rom,
                       const char *sd, SDL_Surface *screen, const MainUIMediaOptions *options,
                       int start);
/* Search the walkthrough for term (ASCII letters in any case), from the page
 * shown, or with next after the match shown, then round from the start;
 * the match's line goes to the top. Kept as the search for Start. */
void mainui_media_search(MainUIMediaViewer *viewer, const char *term, bool next);
/* True while the media popup is over Game Info rather than the gallery. */
bool mainui_media_over_details(const MainUIMediaViewer *viewer);
/* Every key is taken while media is open. B in the gallery, a map or the
 * walkthrough goes back to Game Info, and Select opens the menu over it,
 * without that view's row and with Game Info's; B or Select there goes back.
 * Gallery: Right the next item, up to the last, Left the previous one and,
 * when Right opened it, from the first back to Game Info; on a video A pauses
 * and goes on, and L and R jump about 5 s back and forward, to a keyframe.
 * Maps: L and R the previous and next map, A toggles fit and native size, L2
 * fits and R2 shows native size, the D-pad moving round it. Walkthrough: Up
 * and Down a line, L and R 15 and L2 and R2 75; A asks for the search keyboard
 * (search_requested) and Start finds the next match. Saved Games: Left and
 * Right move between them, Left stopping at the first and Right at the last,
 * or, unless held, leaving them as B does when Left in a game list opened
 * them; A asks the application to resume the one shown (resume_requested). */
void mainui_media_key(MainUIMediaViewer *viewer, SDLKey key, SDL_Surface *screen);
/* A failed resume or game launch may have changed Auto and its backup.
 * Refresh the currently open save list and screenshot from disk. */
void mainui_media_refresh_saves(MainUIMediaViewer *viewer, SDL_Surface *screen, bool previous);
/* A video starts once it has been shown this long; snapshots start it at
 * once. */
#define MAINUI_MEDIA_VIDEO_DELAY_MS 100
/* snapshot: wait for a video's first frame, to draw a given state. A video
 * shown starts here once its delay is over. */
void mainui_media_draw(MainUIMediaViewer *viewer, MainUITheme *theme, SDL_Surface *screen,
                       bool snapshot);
/* Milliseconds until the playing video's next frame is due, or -1. */
int mainui_media_due(const MainUIMediaViewer *viewer, Uint32 now);
/* While the device sleeps a video reads and decodes nothing and stands
 * still; on waking it goes on from there, or stays paused. Kept across
 * close, so a video shown meanwhile starts held. */
void mainui_media_sleep(MainUIMediaViewer *viewer, bool sleeping, Uint32 now);
/* Free the picture or video shown under the menu, as Green, the PDF reader,
 * takes the screen and the sound once it can start; the menu draws them
 * again on return. */
void mainui_media_suspend_for_pdf(MainUIMediaViewer *viewer);
/* Back from the PDF reader: the menu Manual (PDF) was chosen in closes, as
 * B closes it, onto the view it was opened over (that view drawn again into
 * screen) or Game Info. */
void mainui_media_back_from_pdf(MainUIMediaViewer *viewer, SDL_Surface *screen);
/* Stop any video and free everything. Safe when closed. */
void mainui_media_close(MainUIMediaViewer *viewer);
#endif
