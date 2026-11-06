/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_GAME_MENU_H
#define MAINUI_GAME_MENU_H
#include "cJSON.h"
#include "menus/context.h"

/* The game-list context menus .tmp_update/config/game-menu.json can reorder
 * and hide, and the console page's, which console-view.json's "menu" orders
 * and hides the same way. Apps and the main menu are not among them. */
typedef enum {
    MAINUI_GAME_MENU_ROMS,
    MAINUI_GAME_MENU_FAVORITES,
    MAINUI_GAME_MENU_RECENTS,
    MAINUI_GAME_MENU_SEARCH_RESULTS,
    MAINUI_GAME_MENU_SECTIONS, /* game-menu.json's sections end here */
    MAINUI_GAME_MENU_CONSOLES = MAINUI_GAME_MENU_SECTIONS,
    MAINUI_GAME_MENU_ALL
} MainUIGameMenuSection;

#define MAINUI_GAME_MENU_ITEMS 6

/* Game Info's media menu ("details"): its rows, in their default order. */
enum {
    MAINUI_DETAILS_GAME_INFO,
    MAINUI_DETAILS_GALLERY,
    MAINUI_DETAILS_SAVED_GAMES,
    MAINUI_DETAILS_MAPS,
    MAINUI_DETAILS_WALKTHROUGH,
    MAINUI_DETAILS_MANUAL,
    MAINUI_DETAILS_ROWS
};

/* Per section, the IDs to show in order, as indices into that section's own
 * list of IDs. A section not configured keeps the menu exactly as built. */
#define MAINUI_SAVES_DISABLED_MAX 32

typedef struct {
    bool configured[MAINUI_GAME_MENU_ALL];
    int count[MAINUI_GAME_MENU_ALL];
    int order[MAINUI_GAME_MENU_ALL][MAINUI_GAME_MENU_ITEMS];
    /* "details": the media menu's rows (MAINUI_DETAILS_*) in order; all of
     * them in the default order when not configured. Saved Games, and Left
     * in game lists, only when listed. */
    int details_count, details[MAINUI_DETAILS_ROWS];
    /* "saved_games": {"resume_mode": ...}: MAINUI_RESUME_*, backup when
     * missing or unknown; "left_shortcut": true turns on Left's Saved Games
     * in game lists, off unless true; "upscale_preview": false keeps a save's
     * picture at its own size when it fits, enlarged unless false;
     * "show_auto": true lists Auto Save among them, off unless true;
     * "correct_aspect_ratio": false keeps a Saved Games PNG's own shape,
     * the console's display aspect unless false. */
    int resume_mode;
    bool saves_left, saves_upscale, saves_auto, saves_correct_aspect;
    /* "layout": "overlay" shows a save over the whole screen under the
     * header with its labels on boxes; anything else is "framed", the
     * default, fitted between the header and a footer of its own. */
    bool saves_framed;
    /* "disabled_rom_dirs": the ROM folders, by name in any case, whose games
     * have no Saved Games at all; ["ARCADE"] unless given, [] none. */
    int saves_disabled_count;
    char saves_disabled[MAINUI_SAVES_DISABLED_MAX][64];
    /* "gallery": {"right_shortcut": false} turns off Right's gallery in
     * Game Info, on unless false; the media menu still offers it.
     * "upscale_images": false keeps a smaller image at its own size,
     * enlarged to the screen unless false; larger ones still shrink. */
    bool gallery_right, gallery_upscale;
} MainUIGameMenu;

/* Nothing configured: every menu as built, the media menu in its default
 * order, and Saved Games with the rolling backup. */
void mainui_game_menu_defaults(MainUIGameMenu *menu);
/* Parse the file's text. NULL is a missing file and returns true; text that
 * is not a JSON object returns false, and both leave the defaults. A section
 * that is missing or not an array keeps its default; unknown and repeated
 * IDs are skipped; an empty array means no menu. */
bool mainui_game_menu_parse(MainUIGameMenu *menu, const char *text);
/* The media menu lists this row (MAINUI_DETAILS_*). */
bool mainui_game_menu_details(const MainUIGameMenu *menu, int row);
/* Whether rom_root's folder, by its last name, has Saved Games turned off. */
bool mainui_game_menu_saves_disabled(const MainUIGameMenu *menu, const char *rom_root);
/* One section's list of IDs, as the file's are read; anything but an array
 * leaves the section as built. */
void mainui_game_menu_parse_list(MainUIGameMenu *menu, MainUIGameMenuSection section,
                                 const cJSON *list);
/* Read directory/game-menu.json once; a malformed file logs one line. Never
 * writes the file. */
void mainui_game_menu_load(MainUIGameMenu *menu, const char *directory);
/* Keep and order the entries a builder offered for the selection, by the
 * section's IDs. An ID whose entry the builder did not offer adds nothing:
 * the selection still decides what can show. Move stands for Move here
 * during a move, favorite for Add or Remove. */
void mainui_game_menu_apply(MainUIContext *context, const MainUIGameMenu *menu,
                            MainUIGameMenuSection section);
#endif
