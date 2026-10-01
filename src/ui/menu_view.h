/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_MENU_VIEW_H
#define MAINUI_MENU_VIEW_H
#include "catalog/catalog.h"
#include "menus/menu.h"
#include "ui/theme.h"

/* Owns decoded icons/text for one visible console page. The theme and catalog
 * are borrowed and must outlive the view. All access stays on the UI thread.
 */
typedef struct {
    MainUITheme *theme;
    SDL_Surface *home_icons[MAINUI_MENU_SECTIONS][2];
    SDL_Surface *console_icons[9][2];
    SDL_Surface *console_labels[9][2];
    int cached_start;
} MainUIMenuView;

void mainui_menu_view_open(MainUIMenuView *view, MainUITheme *theme);
void mainui_menu_view_close(MainUIMenuView *view);
void mainui_menu_draw_home(MainUIMenuView *view, SDL_Surface *screen, const MainUIMenu *menu,
                           const MainUIViewport *position);
void mainui_menu_draw_systems(MainUIMenuView *view, SDL_Surface *screen, MainUICatalog *catalog,
                              const MainUIViewport *position);
#endif
