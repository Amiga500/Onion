/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_MENU_H
#define MAINUI_MENU_H
#include "cJSON.h"
#include "core/core.h"
/* Owned parsed tree; accepts the patched bounded JSON extensions. */
cJSON *mainui_menu_json(const char *text);

/* Order matches the patched default menu including omitted legacy sections. */
typedef enum {
    MAINUI_MENU_RECENTS,
    MAINUI_MENU_FAVORITES,
    MAINUI_MENU_GAMES,
    MAINUI_MENU_EXPERT,
    MAINUI_MENU_APPS,
    MAINUI_MENU_SETTINGS,
    MAINUI_MENU_SECTIONS
} MainUIMenuSection;

typedef struct {
    MainUIMenuSection sections[MAINUI_MENU_SECTIONS];
    int count;
} MainUIMenu;

/* Parse the patched menu object's ordered visibility/aliases. text is nullable;
 * malformed/empty input uses defaults. Only the main menu is consumed here;
 * SELECT/custom actions and settings are parsed by their own modules.
 */
void mainui_menu_parse(MainUIMenu *menu, const char *text, bool recents, bool expert);
/* Read config-directory/main-menu.json once; never rewrite it. */
void mainui_menu_load(MainUIMenu *menu, const char *directory, const MainUIConfig *config);
const char *mainui_menu_label(MainUIMenuSection section);
const char *mainui_menu_icon(MainUIMenuSection section);
/* Normalize paged grid navigation. Horizontal moves wrap over real entries;
 * vertical moves keep their column, staying put where it has no entry above
 * or below; page moves stop at the first and last entry. Empty lists stay
 * unselected. */
void mainui_grid_restore(MainUIViewport *view, int total, int selected, int columns, int rows);
void mainui_grid_move(MainUIViewport *view, int horizontal, int vertical, int pages, int columns,
                      int rows);
/* Right (horizontal 1) or Left (-1) as stock's grid moves: along the row, and
 * from its end to the same row's first console on the next page, or from its
 * start to the same row's last on the page before, the last console when that
 * page is short of it. Past the last page to the first and the reverse when
 * wrap is set, else it stays. */
void mainui_grid_move_stock(MainUIViewport *view, int horizontal, int columns, int rows, bool wrap);
/* Stock's Up and Down: within the column on the page, over the rows a short
 * last page fills in it, round its ends when wrap is set, else stopping
 * there; never sideways or onto another page. */
void mainui_grid_move_stock_vertical(MainUIViewport *view, int vertical, int columns, int rows,
                                     bool wrap);
#endif
