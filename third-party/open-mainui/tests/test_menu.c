/* SPDX-License-Identifier: GPL-3.0-only */
#include "menus/menu.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

static void default_order(void)
{
    const MainUIMenuSection expected[] = {MAINUI_MENU_RECENTS, MAINUI_MENU_FAVORITES,
                                          MAINUI_MENU_GAMES,   MAINUI_MENU_EXPERT,
                                          MAINUI_MENU_APPS,    MAINUI_MENU_SETTINGS};
    const char *missing[] = {NULL, "{}", "{\"menu\":{}}", "{broken"};
    for (unsigned source = 0; source < sizeof missing / sizeof *missing; ++source) {
        for (int flags = 0; flags < 4; ++flags) {
            MainUIMenu menu;
            mainui_menu_parse(&menu, missing[source], (flags & 1) != 0, (flags & 2) != 0);
            int visible = 0;
            for (unsigned i = 0; i < sizeof expected / sizeof *expected; ++i) {
                if ((expected[i] == MAINUI_MENU_RECENTS && !(flags & 1)) ||
                    (expected[i] == MAINUI_MENU_EXPERT && !(flags & 2))) {
                    continue;
                }
                assert(visible < menu.count && menu.sections[visible++] == expected[i]);
            }
            assert(menu.count == visible);
        }
    }
}

int mainui_suite_menu(void)
{
    default_order();
    MainUIMenu menu;
    mainui_menu_parse(&menu, NULL, false, false);
    assert(menu.count == 4 && menu.sections[0] == MAINUI_MENU_FAVORITES);
    assert(menu.sections[1] == MAINUI_MENU_GAMES && menu.sections[3] == MAINUI_MENU_SETTINGS);
    mainui_menu_parse(&menu,
                      "{ /* custom order */ \"menu\": {\"games\":true, # legacy alias\n"
                      "\"favs\":false,\"favorite\":true,\"settings\":1,},}",
                      false, false);
    assert(menu.count == 2 && menu.sections[0] == MAINUI_MENU_GAMES);
    assert(menu.sections[1] == MAINUI_MENU_FAVORITES);
    mainui_menu_parse(&menu, "{\"menu\":{\"games\":true,\"expert\":false}}", true, true);
    assert(menu.count == 2 && menu.sections[1] == MAINUI_MENU_RECENTS);
    mainui_menu_parse(&menu, "{\"menu\":{\"recents\":false,\"expert\":false}}", true, true);
    assert(menu.count == 4 && menu.sections[0] == MAINUI_MENU_FAVORITES);
    mainui_menu_parse(&menu, "{/* unterminated", true, true);
    assert(menu.count == 6 && menu.sections[0] == MAINUI_MENU_RECENTS);
    mainui_menu_parse(&menu, "{\"menu\":{\"games\":true},\"ignored\":\"# /* , }\"}", false, false);
    assert(menu.count == 1 && menu.sections[0] == MAINUI_MENU_GAMES);
    mainui_menu_parse(&menu,
                      "{\"menu\":{\"games\":true},\"deep\":[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]}",
                      false, false);
    assert(menu.count == 4);

    MainUIViewport view;
    mainui_grid_restore(&view, 10, 0, 4, 2);
    mainui_grid_move(&view, 0, 1, 0, 4, 2);
    assert(view.selected == 4 && view.start == 0 && view.end == 7);
    mainui_grid_move(&view, 0, 1, 0, 4, 2);
    assert(view.selected == 8 && view.start == 8 && view.end == 9);
    mainui_grid_move(&view, 1, 0, 0, 4, 2);
    mainui_grid_move(&view, 1, 0, 0, 4, 2);
    assert(view.selected == 0 && view.start == 0);
    mainui_grid_move(&view, -1, 0, 0, 4, 2);
    assert(view.selected == 9 && view.start == 8);
    mainui_grid_move(&view, 0, 0, -1, 4, 2);
    assert(view.selected == 1 && view.start == 0);
    mainui_grid_restore(&view, 0, 12, 4, 2);
    mainui_grid_move(&view, 1, 1, 1, 4, 2);
    assert(view.selected == -1 && view.end == -1);
    puts("Menu configuration and grid navigation checks passed");
    return 0;
}
