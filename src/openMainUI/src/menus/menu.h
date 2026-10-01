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
 * vertical moves retain columns where possible. Empty lists stay unselected.
 */
void mainui_grid_restore(MainUIViewport *view, int total, int selected, int columns, int rows);
void mainui_grid_move(MainUIViewport *view, int horizontal, int vertical, int pages, int columns,
                      int rows);
#endif
