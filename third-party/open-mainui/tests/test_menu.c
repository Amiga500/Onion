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
    /* Vertical boundaries must not jump sideways to the first/last item. */
    mainui_grid_restore(&view, 10, 3, 4, 2); /* top row, column 4 */
    mainui_grid_move(&view, 0, -1, 0, 4, 2);
    assert(view.selected == 3 && view.start == 0);
    mainui_grid_restore(&view, 10, 0, 4, 2);
    mainui_grid_move(&view, 0, -1, 0, 4, 2);
    assert(view.selected == 0);
    mainui_grid_restore(&view, 10, 9, 4, 2); /* last row, column 2 */
    mainui_grid_move(&view, 0, 1, 0, 4, 2);
    assert(view.selected == 9 && view.start == 8);
    mainui_grid_restore(&view, 10, 6, 4, 2); /* no matching cell below */
    mainui_grid_move(&view, 0, 1, 0, 4, 2);
    assert(view.selected == 6 && view.start == 0);
    mainui_grid_restore(&view, 10, 5, 4, 2); /* matching cell below exists */
    mainui_grid_move(&view, 0, 1, 0, 4, 2);
    assert(view.selected == 9 && view.start == 8);
    mainui_grid_move(&view, 0, -1, 0, 4, 2); /* move back across page */
    assert(view.selected == 5 && view.start == 0);
    /* Expert 3x3 uses the same rule on its incomplete last page. */
    mainui_grid_restore(&view, 11, 8, 3, 3);
    mainui_grid_move(&view, 0, 1, 0, 3, 3);
    assert(view.selected == 8);
    mainui_grid_restore(&view, 11, 7, 3, 3);
    mainui_grid_move(&view, 0, 1, 0, 3, 3);
    assert(view.selected == 10 && view.start == 9);
    /* Page jumps remain clamped instead of using vertical edge rules. */
    mainui_grid_move(&view, 0, 0, 1, 3, 3);
    assert(view.selected == 10);
    mainui_grid_move(&view, 0, 0, -2, 3, 3);
    assert(view.selected == 0);
    /* Horizontal traversal and wrap are untouched. */
    mainui_grid_restore(&view, 10, 9, 4, 2);
    mainui_grid_move(&view, 1, 0, 0, 4, 2);
    assert(view.selected == 0);
    mainui_grid_restore(&view, 0, 12, 4, 2);
    mainui_grid_move(&view, 1, 1, 1, 4, 2);
    assert(view.selected == -1 && view.end == -1);
    /* Stock's Right and Left: Games 4x2, 16 consoles; index 3 is the top
     * right of the first page. */
    mainui_grid_restore(&view, 16, 3, 4, 2);
    mainui_grid_move(&view, 1, 0, 0, 4, 2);
    assert(view.selected == 4);
    mainui_grid_restore(&view, 16, 3, 4, 2);
    mainui_grid_move_stock(&view, 1, 4, 2, true);
    assert(view.selected == 8 && view.start == 8);
    mainui_grid_move_stock(&view, -1, 4, 2, true); /* back to the row's end */
    assert(view.selected == 3 && view.start == 0);
    mainui_grid_restore(&view, 16, 1, 4, 2); /* along the row */
    mainui_grid_move_stock(&view, 1, 4, 2, true);
    assert(view.selected == 2);
    mainui_grid_restore(&view, 16, 7, 4, 2); /* bottom row: same row */
    mainui_grid_move_stock(&view, 1, 4, 2, true);
    assert(view.selected == 12);
    /* Past the last page round to the first, or, without wrap, nowhere. */
    mainui_grid_restore(&view, 16, 15, 4, 2);
    mainui_grid_move_stock(&view, 1, 4, 2, true);
    assert(view.selected == 4 && view.start == 0);
    mainui_grid_restore(&view, 16, 15, 4, 2);
    mainui_grid_move_stock(&view, 1, 4, 2, false);
    assert(view.selected == 15);
    mainui_grid_restore(&view, 16, 0, 4, 2);
    mainui_grid_move_stock(&view, -1, 4, 2, false);
    assert(view.selected == 0);
    mainui_grid_move_stock(&view, -1, 4, 2, true); /* to the last page's top row end */
    assert(view.selected == 11 && view.start == 8);
    /* A short last page: 10 consoles. The row's end is its last console,
     * and a missing row lands on the last console. */
    mainui_grid_restore(&view, 10, 9, 4, 2);
    mainui_grid_move_stock(&view, 1, 4, 2, true);
    assert(view.selected == 0);
    mainui_grid_restore(&view, 10, 7, 4, 2);
    mainui_grid_move_stock(&view, 1, 4, 2, true);
    assert(view.selected == 9);
    mainui_grid_restore(&view, 10, 4, 4, 2);
    mainui_grid_move_stock(&view, -1, 4, 2, true);
    assert(view.selected == 9);
    /* Expert's 3x3. */
    mainui_grid_restore(&view, 20, 5, 3, 3);
    mainui_grid_move_stock(&view, 1, 3, 3, true);
    assert(view.selected == 12 && view.start == 9);
    mainui_grid_move_stock(&view, -1, 3, 3, true);
    assert(view.selected == 5);
    /* Stock vertical movement wraps only within the same page and column. */
    mainui_grid_restore(&view, 16, 2, 4, 2); /* Games: top row, column 3 */
    mainui_grid_move_stock_vertical(&view, -1, 4, 2, true);
    assert(view.selected == 6 && view.start == 0);
    mainui_grid_move_stock_vertical(&view, 1, 4, 2, true);
    assert(view.selected == 2 && view.start == 0);
    mainui_grid_restore(&view, 16, 14, 4, 2); /* second page, bottom row */
    mainui_grid_move_stock_vertical(&view, 1, 4, 2, true);
    assert(view.selected == 10 && view.start == 8);
    mainui_grid_move_stock_vertical(&view, -1, 4, 2, true);
    assert(view.selected == 14 && view.start == 8);
    /* Games' short last page: only columns 1-2 have items. */
    mainui_grid_restore(&view, 10, 9, 4, 2);
    mainui_grid_move_stock_vertical(&view, 1, 4, 2, true);
    assert(view.selected == 9 && view.start == 8);
    mainui_grid_move_stock_vertical(&view, -1, 4, 2, true);
    assert(view.selected == 9 && view.start == 8);
    /* Expert: three rows, wrapping top to bottom and bottom to top. */
    mainui_grid_restore(&view, 18, 1, 3, 3);
    mainui_grid_move_stock_vertical(&view, -1, 3, 3, true);
    assert(view.selected == 7 && view.start == 0);
    mainui_grid_move_stock_vertical(&view, 1, 3, 3, true);
    assert(view.selected == 1 && view.start == 0);
    mainui_grid_restore(&view, 18, 13, 3, 3); /* second page, middle */
    mainui_grid_move_stock_vertical(&view, 1, 3, 3, true);
    assert(view.selected == 16 && view.start == 9);
    mainui_grid_move_stock_vertical(&view, 1, 3, 3, true);
    assert(view.selected == 10 && view.start == 9);
    /* Without wrap the column's ends stop it, a short last page's too. */
    mainui_grid_restore(&view, 16, 2, 4, 2);
    mainui_grid_move_stock_vertical(&view, -1, 4, 2, false);
    assert(view.selected == 2 && view.start == 0);
    mainui_grid_move_stock_vertical(&view, 1, 4, 2, false);
    assert(view.selected == 6 && view.start == 0);
    mainui_grid_move_stock_vertical(&view, 1, 4, 2, false);
    assert(view.selected == 6 && view.start == 0);
    mainui_grid_restore(&view, 10, 9, 4, 2);
    mainui_grid_move_stock_vertical(&view, 1, 4, 2, false);
    assert(view.selected == 9 && view.start == 8);
    mainui_grid_restore(&view, 18, 13, 3, 3);
    mainui_grid_move_stock_vertical(&view, 1, 3, 3, false);
    assert(view.selected == 16 && view.start == 9);
    mainui_grid_move_stock_vertical(&view, 1, 3, 3, false);
    assert(view.selected == 16 && view.start == 9);
    /* Expert short page: column 1 has two rows, column 3 just one. */
    mainui_grid_restore(&view, 14, 9, 3, 3);
    mainui_grid_move_stock_vertical(&view, -1, 3, 3, true);
    assert(view.selected == 12 && view.start == 9);
    mainui_grid_move_stock_vertical(&view, 1, 3, 3, true);
    assert(view.selected == 9 && view.start == 9);
    mainui_grid_restore(&view, 14, 11, 3, 3);
    mainui_grid_move_stock_vertical(&view, 1, 3, 3, true);
    assert(view.selected == 11 && view.start == 9);
    /* Empty lists remain unselected; horizontal wrap is irrelevant to Up/Down. */
    mainui_grid_restore(&view, 0, 0, 4, 2);
    mainui_grid_move_stock_vertical(&view, -1, 4, 2, true);
    assert(view.selected == -1);
    puts("Menu configuration and grid navigation checks passed");
    return 0;
}
