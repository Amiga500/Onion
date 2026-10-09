/* SPDX-License-Identifier: GPL-3.0-only */
#include "menus/context.h"
#include "menus/favorite_context.h"
#include <limits.h>
#include <stdlib.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void favorite_popups(void)
{
    MainUILibrary *library = calloc(1, sizeof *library);
    assert(library);
    library->current = -1;
    library->visible_count = 1;
    library->visible[0] = 0;
    MainUIContext context;
    mainui_favorite_context(&context, library, 0, false);
    const MainUIContextAction game[] = {CONTEXT_FAVORITE_MOVE, CONTEXT_FAVORITE_CREATE,
                                        CONTEXT_FAVORITE_SORT, CONTEXT_REMOVE_FAVORITE};
    const char *labels[] = {"Move selected", "Create folder", "Sort A-Z", "Remove Favorite"};
    assert(context.visible_count == 4);
    for (int i = 0; i < 4; i++) {
        assert(context.entries[i].action == game[i]);
        assert(!strcmp(mainui_context_label(&context.entries[i]), labels[i]));
    }
    library->visible[0] = -1;
    mainui_favorite_context(&context, library, 0, false);
    assert(context.visible_count == 4);
    assert(context.entries[2].action == CONTEXT_FAVORITE_RENAME);
    assert(context.entries[3].action == CONTEXT_FAVORITE_DELETE);
    library->visible[0] = INT_MIN;
    mainui_favorite_context(&context, library, 0, false);
    assert(context.visible_count == 1 && context.entries[0].action == CONTEXT_FAVORITE_CREATE);
    library->visible_count = 0;
    mainui_favorite_context(&context, library, -1, false);
    assert(context.visible_count == 1);
    mainui_favorite_context(&context, library, -1, true);
    assert(context.visible_count == 2 && context.entries[0].action == CONTEXT_FAVORITE_PASTE);
    library->current = 2;
    library->folders[2].parent = 1;
    library->folders[1].parent = 0;
    library->folders[0].parent = -1;
    mainui_favorite_context(&context, library, -1, false);
    assert(context.visible_count == 0);
    library->visible_count = 1;
    library->visible[0] = 0;
    mainui_favorite_context(&context, library, 0, false);
    assert(context.visible_count == 3);
    assert(context.entries[1].action == CONTEXT_FAVORITE_SORT);
    assert(context.entries[2].action == CONTEXT_REMOVE_FAVORITE);
    free(library);
}

int mainui_suite_context(void)
{
    favorite_popups();
    MainUIContext c;
    mainui_context_parse(&c, NULL, false, false);
    assert(c.visible_count == 1 && c.entries[0].action == CONTEXT_REFRESH && c.hotkey);
    mainui_context_parse(&c, "{}", true, true);
    assert(c.visible_count == 3 && c.entries[2].action == CONTEXT_TWEAKS && c.hotkey);
    /* Root hotkey: omitted or literal true enables; any other present value disables. */
    mainui_context_parse(&c, "{\"hotkey\":true}", false, false);
    assert(c.hotkey);
    static const char *const disabled[] = {"{\"hotkey\":false}",    "{\"hotkey\":null}",
                                           "{\"hotkey\":0}",        "{\"hotkey\":1}",
                                           "{\"hotkey\":\"true\"}", "{\"hotkey\":\"false\"}"};
    for (size_t i = 0; i < sizeof disabled / sizeof *disabled; ++i) {
        mainui_context_parse(&c, disabled[i], false, false);
        assert(!c.hotkey);
    }
    mainui_context_parse(&c, "{\"context\":[]}", true, true);
    assert(c.count == 0);
    mainui_context_parse(&c,
                         "{\"context\":{\"settings\":false,\"favs\":true,\"settings\":true,"
                         "\"recent\":1},\"hotkey\":false}",
                         false, false);
    assert(!c.hotkey && c.count == 3 && c.visible_count == 2);
    assert(c.entries[0].action == CONTEXT_SETTINGS && c.entries[0].enabled);
    assert(c.entries[1].action == CONTEXT_FAVORITES && !c.entries[2].enabled);
    mainui_context_reveal(&c);
    assert(c.visible_count == 3);
    mainui_context_parse(
        &c,
        "{\"context\":[\"custom1\",\"custom2\",\"games\",\"games\"],\"custom\":{\"custom1\":{"
        "\"label\":\"Literal %s\",\"launch\":\"/mnt/SDCARD/tool.sh\",\"type\":3.5}}}",
        false, false);
    assert(c.count == 2 && c.entries[0].type == 3);
    assert(!strcmp(mainui_context_label(&c.entries[0]), "Literal %s"));
    assert(mainui_context_section(c.entries[1].action) == MAINUI_MENU_GAMES);
    puts("Context defaults, order, aliases, reveal and bounded custom action checks passed");
    return 0;
}
