/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_CONSOLE_VIEW_H
#define MAINUI_CONSOLE_VIEW_H
#include "catalog/catalog.h"
#include "menus/game_menu.h"
#include <SDL.h>
#include <stdbool.h>

/* How Games and Expert show their consoles, from
 * .tmp_update/config/console-view.json:
 *
 *   {"games": "list", "expert": "carousel", "thumbs": "/mnt/SDCARD/Thumbs",
 *    "count": true, "menu": ["refresh_all", "refresh"],
 *    "carousel": {"labels": true, "selection": "#FFFFFF38"},
 *    "list": {"folder_icon": true}}
 *
 * "grid", the default, is the grid as before; "carousel" a horizontal row,
 * the selected console centred; "list" a list like a game list. "thumbs" is
 * a folder of one PNG per console, named by its Emu folder in lowercase
 * (sfc.png, fc.png): missing files use _default.png, then the theme's
 * console icon. The list shows it as a game's cover, unless "list":
 * {"thumbs": false}, the carousel in place of the console's icon. "count" and "menu" are for every view, the
 * "carousel" and "list" objects for theirs. Read once at startup. */
enum {
    MAINUI_CONSOLE_GRID,
    MAINUI_CONSOLE_CAROUSEL,
    MAINUI_CONSOLE_LIST
};

typedef struct {
    int games, expert;            /* MAINUI_CONSOLE_* */
    char thumbs[MAINUI_PATH_MAX]; /* as the file gives it, "" for none */
    bool hide_count;              /* "count": false: no numbers of games, in any view */
    /* "menu": the console page's context menu, as game-menu.json's sections
     * (MAINUI_GAME_MENU_CONSOLES); not configured, as built. */
    MainUIGameMenu menu;
    bool hide_labels; /* "carousel": {"labels": false}: no names */
    /* "carousel": {"selection": ...}: the selected console's background,
     * the theme's (or the plain default) for none given, nothing for false
     * or "", and for a thumbnail the colour of "#RRGGBB" (opaque) or
     * "#RRGGBBAA". */
    int selection;
    SDL_Color selection_colour;
    Uint8 selection_alpha;
    bool hide_folder_icon; /* "list": {"folder_icon": false} */
    /* "list": {"thumbs": false}: no console cover in the list, neither a
     * thumbnail, _default.png nor the theme icon; the carousel keeps them. */
    bool hide_list_thumbs;
    /* "carousel": the neighbours' thumbnails at scale_unselected percent
     * (60..100, 70) and their pictures at opacity_unselected percent (0..100,
     * 50), gap pixels apart (0..100, 40), sliding for duration_ms (80..500,
     * 240) unless "animation": "none", round the ends unless "wrap": false;
     * the plain selection selection_padding round its thumbnail (0..40, 12),
     * and everything offset_y lower (-60..60, 0). */
    int scale_unselected, opacity_unselected, gap, duration_ms, selection_padding, offset_y;
    int repeat_ms; /* Minimum interval for Carousel Left/Right; slides enforce duration too. */
    bool slide, carousel_wrap;
    bool smooth_easing; /* "carousel": {"easing": "smooth"} for an ease-in/out slide. */
    bool vsync;         /* "carousel": {"vsync": false} turns off the device's VSync wait. */
    /* "grid": {"navigation": "stock"} pages with Right and Left at a row's
     * ends; "wrap": false stops them at the first and last console. */
    bool grid_stock, grid_no_wrap;
} MainUIConsoleView;

enum {
    MAINUI_CONSOLE_SELECTION_DEFAULT,
    MAINUI_CONSOLE_SELECTION_NONE,
    MAINUI_CONSOLE_SELECTION_COLOUR
};

/* The defaults: every console page as the grid, as without the file. */
void mainui_console_view_defaults(MainUIConsoleView *view);
/* Parse the file's text. NULL is a missing file and returns true; text that
 * is not a JSON object returns false. Both, a missing key and any value but
 * "carousel" or "list" give the grid; a "thumbs" that is not a non-empty
 * string, no thumbnails; only false hides what "count", "labels",
 * "folder_icon" and the list's "thumbs" show; a "selection" that is not false, "" or a colour as
 * above is the default; numbers are kept within their ranges, and anything
 * else is the default. */
bool mainui_console_view_parse(MainUIConsoleView *view, const char *text);
/* Read directory/console-view.json; a malformed file logs one line. Never
 * writes the file. */
void mainui_console_view_load(MainUIConsoleView *view, const char *directory);
/* The thumbnail of a console of the console page, in the thumbs folder
 * (resolved, on this host): <thumbs>/<its Emu folder in lowercase>.png. False
 * without thumbs or a folder to name it by; the file is not looked at. */
bool mainui_console_thumb(char out[MAINUI_PATH_MAX], const char *thumbs, const MainUIEntry *entry);
/* Derive the fallback filename without filesystem operations on the UI thread. */
bool mainui_console_thumb_default(char out[MAINUI_PATH_MAX], const char *primary);
#endif
