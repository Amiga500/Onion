/* SPDX-License-Identifier: GPL-3.0-only */
#include "menus/console_view.h"
#include "menus/context.h"
#include "menus/favorite_context.h"
#include "menus/game_menu.h"
#include "platform/save_states.h"
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

static void actions_are(const MainUIContext *context, const MainUIContextAction *want, int count)
{
    assert(context->count == count && context->visible_count == count);
    for (int i = 0; i < count; i++) {
        assert(context->entries[context->visible[i]].action == want[i]);
        assert(context->entries[i].enabled);
    }
}

/* console-view.json: "carousel" or "list" for games or expert; anything
 * else, a missing key, a missing file or a file that is not an object, the
 * grid. "thumbs", a folder of <emu folder>.png in lowercase. */
static void console_views(void)
{
    MainUIConsoleView view;
    assert(mainui_console_view_parse(&view, NULL) && view.games == MAINUI_CONSOLE_GRID &&
           view.expert == MAINUI_CONSOLE_GRID && !*view.thumbs);
    assert(mainui_console_view_parse(&view, "{// comment\n\"games\": \"carousel\",}") &&
           view.games == MAINUI_CONSOLE_CAROUSEL);
    assert(mainui_console_view_parse(&view, "{\"games\": \"carousel\"}") &&
           view.games == MAINUI_CONSOLE_CAROUSEL && view.expert == MAINUI_CONSOLE_GRID);
    assert(mainui_console_view_parse(&view, "{\"games\": \"list\", \"expert\": \"carousel\", "
                                            "\"thumbs\": \"/mnt/SDCARD/Thumbs\"}") &&
           view.games == MAINUI_CONSOLE_LIST && view.expert == MAINUI_CONSOLE_CAROUSEL &&
           !strcmp(view.thumbs, "/mnt/SDCARD/Thumbs"));
    /* Names, counts, folder icons and the list's covers shown unless false;
     * "labels" in "carousel", "folder_icon" and "thumbs" in "list" only. */
    assert(!view.hide_labels && !view.hide_count && !view.hide_folder_icon &&
           !view.hide_list_thumbs && view.selection == MAINUI_CONSOLE_SELECTION_DEFAULT &&
           !view.menu.configured[0]);
    assert(mainui_console_view_parse(&view, "{\"count\": false, \"carousel\": {\"labels\": "
                                            "false}, \"list\": {\"folder_icon\": false}}") &&
           view.hide_labels && view.hide_count && view.hide_folder_icon && !view.hide_list_thumbs);
    /* The list's "thumbs": false hides its covers and keeps the folder. */
    assert(mainui_console_view_parse(&view, "{\"thumbs\": \"Thumbs\", \"list\": "
                                            "{\"thumbs\": false}}") &&
           view.hide_list_thumbs && !strcmp(view.thumbs, "Thumbs"));
    assert(mainui_console_view_parse(&view, "{\"list\": {\"thumbs\": 0}}") &&
           !view.hide_list_thumbs);
    assert(
        mainui_console_view_parse(&view, "{\"labels\": false, \"folder_icon\": false, "
                                         "\"carousel\": {\"labels\": 0}, \"count\": \"false\"}") &&
        !view.hide_labels && !view.hide_count && !view.hide_folder_icon && !view.hide_list_thumbs);
    /* The selection: false or "" for none, a colour, or else the default. */
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"selection\": false}}") &&
           view.selection == MAINUI_CONSOLE_SELECTION_NONE);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"selection\": \"\"}}") &&
           view.selection == MAINUI_CONSOLE_SELECTION_NONE);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"selection\": \"#1a2B3c\"}}") &&
           view.selection == MAINUI_CONSOLE_SELECTION_COLOUR && view.selection_colour.r == 0x1a &&
           view.selection_colour.g == 0x2b && view.selection_colour.b == 0x3c &&
           view.selection_alpha == 255);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"selection\": \"#FFFFFF38\"}}") &&
           view.selection == MAINUI_CONSOLE_SELECTION_COLOUR && view.selection_alpha == 0x38);
    const char *defaults[] = {"true", "\"#FFF\"", "\"#GG0000\"", "\"FFFFFF\"", "\"#FFFFFF3\"", "1"};
    for (size_t i = 0; i < sizeof defaults / sizeof *defaults; i++) {
        char text[64];
        snprintf(text, sizeof text, "{\"carousel\": {\"selection\": %s}}", defaults[i]);
        assert(mainui_console_view_parse(&view, text) &&
               view.selection == MAINUI_CONSOLE_SELECTION_DEFAULT);
    }
    /* The carousel's and grid's numbers and switches: defaults, ranges and
     * anything else as missing. */
    assert(mainui_console_view_parse(&view, NULL) && view.scale_unselected == 70 &&
           view.opacity_unselected == 50 && view.gap == 40 && view.duration_ms == 180 &&
           view.repeat_ms == 0 && !view.smooth_easing && view.selection_padding == 12 &&
           !view.offset_y && view.slide && view.carousel_wrap && view.vsync && !view.grid_stock &&
           !view.grid_no_wrap);
    assert(mainui_console_view_parse(
               &view,
               "{\"carousel\": {\"scale_unselected\": 0.85, \"gap\": 32, \"animation\": "
               "\"slide\", \"duration_ms\": 200, \"wrap\": false, \"selection_padding\": 4, "
               "\"offset_y\": -20}, \"grid\": {\"navigation\": \"stock\", \"wrap\": false}}") &&
           view.scale_unselected == 85 && view.gap == 32 && view.slide && view.duration_ms == 200 &&
           !view.carousel_wrap && view.selection_padding == 4 && view.offset_y == -20 &&
           view.grid_stock && view.grid_no_wrap);
    assert(mainui_console_view_parse(
               &view,
               "{\"carousel\": {\"scale_unselected\": 0.2, \"gap\": 999, \"duration_ms\": 1, "
               "\"selection_padding\": -5, \"offset_y\": 500, \"opacity_unselected\": 2}}") &&
           view.scale_unselected == 60 && view.gap == 100 && view.duration_ms == 80 &&
           !view.selection_padding && view.offset_y == 60 && view.opacity_unselected == 100);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"opacity_unselected\": 0, "
                                            "\"duration_ms\": 2000}}") &&
           !view.opacity_unselected && view.duration_ms == 500);
    assert(mainui_console_view_parse(
               &view, "{\"carousel\": {\"scale_unselected\": \"0.9\", \"animation\": \"Slide\", "
                      "\"wrap\": 1}, \"grid\": {\"navigation\": \"Stock\", \"wrap\": 0}}") &&
           view.scale_unselected == 70 && view.slide && view.carousel_wrap && !view.grid_stock &&
           !view.grid_no_wrap);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"vsync\": true}}") && view.vsync);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"vsync\": false}}") && !view.vsync);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"vsync\": 1}}") && view.vsync);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"repeat_ms\": 150}}") &&
           view.repeat_ms == 150);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"repeat_ms\": -20}}") &&
           view.repeat_ms == 0);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"repeat_ms\": 600}}") &&
           view.repeat_ms == 400);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"repeat_ms\": \"150\"}}") &&
           view.repeat_ms == 0);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"animation\": \"slide\", "
                                            "\"easing\": \"smooth\"}}") &&
           view.slide && view.smooth_easing);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"easing\": \"Smooth\"}}") &&
           !view.smooth_easing);
    /* A slide unless "animation": "none". */
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"animation\": \"none\"}}") &&
           !view.slide);
    assert(mainui_console_view_parse(&view, "{\"carousel\": {\"duration_ms\": 499}}") &&
           view.duration_ms == 499);
    /* "menu": the console page's rows, as game-menu.json's sections. */
    const MainUIContextAction rows[] = {CONTEXT_REFRESH, CONTEXT_REFRESH_SYSTEM};
    MainUIContext menu;
    assert(mainui_console_view_parse(&view, "{\"menu\": [\"refresh\", \"bogus\", "
                                            "\"refresh_all\", \"refresh\"]}"));
    mainui_context_rows(&menu, rows, 2);
    mainui_game_menu_apply(&menu, &view.menu, MAINUI_GAME_MENU_CONSOLES);
    const MainUIContextAction swapped[] = {CONTEXT_REFRESH_SYSTEM, CONTEXT_REFRESH};
    actions_are(&menu, swapped, 2);
    mainui_context_rows(&menu, rows, 1); /* the Search console: no Refresh roms */
    mainui_game_menu_apply(&menu, &view.menu, MAINUI_GAME_MENU_CONSOLES);
    actions_are(&menu, rows, 1);
    assert(mainui_console_view_parse(&view, "{\"menu\": []}"));
    mainui_context_rows(&menu, rows, 2);
    mainui_game_menu_apply(&menu, &view.menu, MAINUI_GAME_MENU_CONSOLES);
    assert(!menu.visible_count);
    assert(mainui_console_view_parse(&view, "{\"menu\": \"refresh\"}"));
    mainui_context_rows(&menu, rows, 2);
    mainui_game_menu_apply(&menu, &view.menu, MAINUI_GAME_MENU_CONSOLES);
    actions_are(&menu, rows, 2);
    /* game-menu.json has no console section. */
    MainUIGameMenu games;
    assert(mainui_game_menu_parse(&games, "{\"menu\": [], \"consoles\": []}"));
    assert(!games.configured[MAINUI_GAME_MENU_CONSOLES]);
    const char *grid[] = {"{}", "{\"games\": \"Carousel\"}", "{\"games\": true}",
                          "{\"games\": \"lists\", \"expert\": 1, \"thumbs\": 3}"};
    for (size_t i = 0; i < sizeof grid / sizeof *grid; i++) {
        assert(mainui_console_view_parse(&view, grid[i]) && view.games == MAINUI_CONSOLE_GRID &&
               view.expert == MAINUI_CONSOLE_GRID && !*view.thumbs);
    }
    const char *broken[] = {"{", "[\"carousel\"]", "\"carousel\"", ""};
    for (size_t i = 0; i < sizeof broken / sizeof *broken; i++) {
        assert(!mainui_console_view_parse(&view, broken[i]) && view.games == MAINUI_CONSOLE_GRID);
    }
    /* A console's thumbnail: its Emu folder's name in lowercase. */
    char path[MAINUI_PATH_MAX];
    MainUIEntry entry = {.config = "/sd/Emu/SFC/config.json"};
    assert(mainui_console_thumb(path, "/sd/Thumbs", &entry) && !strcmp(path, "/sd/Thumbs/sfc.png"));
    char fallback[MAINUI_PATH_MAX];
    assert(mainui_console_thumb_default(fallback, path) &&
           !strcmp(fallback, "/sd/Thumbs/_default.png"));
    assert(!mainui_console_thumb_default(fallback, "") &&
           !mainui_console_thumb_default(fallback, "file.png"));
    assert(!mainui_console_thumb(path, "", &entry) && !mainui_console_thumb(path, NULL, &entry));
    entry.config = NULL;
    assert(!mainui_console_thumb(path, "/sd/Thumbs", &entry));
}

static void game_menus(void)
{
    const MainUIContextAction rom[] = {CONTEXT_START, CONTEXT_ADD_FAVORITE, CONTEXT_DELETE_ROM,
                                       CONTEXT_SEARCH, CONTEXT_REFRESH_SYSTEM};
    MainUIGameMenu menu;
    MainUIContext context, built;
    /* No file, a malformed one, or one without the section: menus as built. */
    const char *unchanged[] = {NULL,
                               "{",
                               "[\"roms\"]",
                               "\"roms\"",
                               "{}",
                               "{\"roms\":\"launch\"}",
                               "{\"roms\":{\"launch\":true}}",
                               "{\"Roms\":[]}"};
    for (size_t i = 0; i < sizeof unchanged / sizeof *unchanged; i++) {
        bool ok = mainui_game_menu_parse(&menu, unchanged[i]);
        assert(ok == (i == 0 || i >= 4));
        mainui_context_rows(&built, rom, 5);
        context = built;
        mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_ROMS);
        assert(!memcmp(&context, &built, sizeof context));
    }
    /* Reordered and hidden; unknown, repeated and wrong-type IDs skipped. */
    assert(mainui_game_menu_parse(
        &menu, "// comment\n{\"roms\": [\"refresh\", \"Launch\", \"bogus\", 3, \"launch\", "
               "\"favorite\", \"refresh\", \"launch\"], \"recents\": []}"));
    mainui_context_rows(&context, rom, 5);
    mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_ROMS);
    const MainUIContextAction reordered[] = {CONTEXT_REFRESH_SYSTEM, CONTEXT_START,
                                             CONTEXT_ADD_FAVORITE};
    actions_are(&context, reordered, 3);
    /* The selection still decides: a folder offers only Search and Refresh,
     * and a console that cannot be refreshed no Refresh. */
    const MainUIContextAction folder[] = {CONTEXT_SEARCH, CONTEXT_REFRESH_SYSTEM};
    mainui_context_rows(&context, folder, 2);
    mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_ROMS);
    actions_are(&context, folder + 1, 1);
    mainui_context_rows(&context, rom, 4);
    mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_ROMS);
    actions_are(&context, reordered + 1, 2);
    /* "favorite" stands for Remove too. */
    const MainUIContextAction removing[] = {CONTEXT_START, CONTEXT_REMOVE_FAVORITE};
    mainui_context_rows(&context, removing, 2);
    mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_ROMS);
    const MainUIContextAction removing_order[] = {CONTEXT_START, CONTEXT_REMOVE_FAVORITE};
    actions_are(&context, removing_order, 2);
    /* An empty array: no menu. Other sections keep theirs. */
    const MainUIContextAction recent[] = {CONTEXT_START, CONTEXT_REMOVE_RECENT,
                                          CONTEXT_CLEAR_RECENT};
    mainui_context_rows(&context, recent, 3);
    mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_RECENTS);
    assert(context.count == 0 && context.visible_count == 0);
    mainui_context_rows(&context, recent, 3);
    mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_SEARCH_RESULTS);
    actions_are(&context, recent, 3);
    /* An ID from another section does nothing here. */
    assert(mainui_game_menu_parse(&menu, "{\"search_results\":[\"delete\",\"favorite\"]}"));
    mainui_context_rows(&context, rom, 2);
    mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_SEARCH_RESULTS);
    actions_are(&context, rom + 1, 1);
    /* Favorites: the default order gives today's game and folder menus, and
     * "move" is Move here during a move. */
    MainUILibrary *library = calloc(1, sizeof *library);
    assert(library);
    library->current = -1;
    library->visible_count = 1;
    library->visible[0] = 0;
    assert(mainui_game_menu_parse(&menu, "{\"favorites\":[\"move\",\"create_folder\",\"sort\","
                                         "\"remove_favorite\",\"rename_folder\","
                                         "\"delete_folder\"]}"));
    for (int row = -1; row <= 0; row++) {
        for (int moving = 0; moving <= 1; moving++) {
            library->visible[0] = row;
            mainui_favorite_context(&built, library, 0, moving);
            context = built;
            mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_FAVORITES);
            assert(!memcmp(&context, &built, sizeof context));
        }
    }
    assert(mainui_game_menu_parse(&menu, "{\"favorites\":[\"remove_favorite\",\"delete_folder\","
                                         "\"move\"]}"));
    library->visible[0] = 0;
    mainui_favorite_context(&context, library, 0, true);
    mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_FAVORITES);
    const MainUIContextAction pasting[] = {CONTEXT_REMOVE_FAVORITE, CONTEXT_FAVORITE_PASTE};
    actions_are(&context, pasting, 2);
    library->visible[0] = -1;
    mainui_favorite_context(&context, library, 0, false);
    mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_FAVORITES);
    const MainUIContextAction folder_menu[] = {CONTEXT_FAVORITE_DELETE, CONTEXT_FAVORITE_MOVE};
    actions_are(&context, folder_menu, 2);
    free(library);
    /* Every ID once, in reverse: all of them still fit. */
    assert(mainui_game_menu_parse(&menu, "{\"favorites\":[\"delete_folder\",\"rename_folder\","
                                         "\"remove_favorite\",\"sort\",\"create_folder\","
                                         "\"move\",\"move\"]}"));
    assert(menu.count[MAINUI_GAME_MENU_FAVORITES] == 6);
    mainui_game_menu_defaults(&menu);
    mainui_context_rows(&context, rom, 5);
    mainui_game_menu_apply(&context, &menu, MAINUI_GAME_MENU_ROMS);
    actions_are(&context, rom, 5);
    mainui_game_menu_apply(&context, NULL, MAINUI_GAME_MENU_ROMS);
    actions_are(&context, rom, 5);
    /* Game Info's media menu ("details"): every row by default, Saved Games
     * with the rolling backup; an explicit list orders and hides them, once
     * each, unknown IDs skipped; anything but an array keeps the default. */
    mainui_game_menu_defaults(&menu);
    assert(menu.details_count == MAINUI_DETAILS_ROWS &&
           menu.details[2] == MAINUI_DETAILS_SAVED_GAMES);
    assert(menu.resume_mode == MAINUI_RESUME_BACKUP && !menu.saves_left && menu.saves_upscale &&
           !menu.saves_auto && menu.saves_correct_aspect && menu.gallery_right);
    assert(mainui_game_menu_details(&menu, MAINUI_DETAILS_SAVED_GAMES));
    assert(mainui_game_menu_parse(&menu, "{\"details\": [\"maps\", \"nope\", \"gallery\", "
                                         "\"maps\"], \"saved_games\": {\"resume_mode\": "
                                         "\"swap\"}}"));
    assert(menu.details_count == 2 && menu.details[0] == MAINUI_DETAILS_MAPS &&
           menu.details[1] == MAINUI_DETAILS_GALLERY);
    assert(!mainui_game_menu_details(&menu, MAINUI_DETAILS_SAVED_GAMES));
    assert(menu.resume_mode == MAINUI_RESUME_SWAP);
    assert(mainui_game_menu_parse(&menu, "{\"details\": \"saved_games\", \"saved_games\": "
                                         "{\"resume_mode\": \"overwrite\"}}"));
    assert(menu.details_count == MAINUI_DETAILS_ROWS &&
           menu.resume_mode == MAINUI_RESUME_OVERWRITE);
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"resume_mode\": \"Swap\"}}"));
    assert(menu.resume_mode == MAINUI_RESUME_BACKUP && !menu.saves_left);
    /* left_shortcut: only true turns Left's Saved Games on; upscale_preview
     * only false turns enlarging off. */
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"left_shortcut\": true, "
                                         "\"upscale_preview\": false}}"));
    assert(menu.saves_left && !menu.saves_upscale && menu.resume_mode == MAINUI_RESUME_BACKUP);
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"left_shortcut\": 1, "
                                         "\"upscale_preview\": 0}}"));
    assert(!menu.saves_left && menu.saves_upscale);
    /* correct_aspect_ratio is on by default; only false turns it off. */
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"correct_aspect_ratio\": false}}"));
    assert(!menu.saves_correct_aspect && menu.saves_upscale);
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"correct_aspect_ratio\": 0}}"));
    assert(menu.saves_correct_aspect);
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"correct_aspect_ratio\": true}}"));
    assert(menu.saves_correct_aspect);
    /* disabled_rom_dirs: ARCADE unless given; a list replaces it, [] none;
     * entries that are no folder names, or repeat one, are skipped. */
    assert(mainui_game_menu_parse(&menu, "{}"));
    assert(menu.saves_disabled_count == 1 &&
           mainui_game_menu_saves_disabled(&menu, "/mnt/SDCARD/Roms/ARCADE") &&
           mainui_game_menu_saves_disabled(&menu, "/mnt/SDCARD/Roms/arcade/") &&
           !mainui_game_menu_saves_disabled(&menu, "/mnt/SDCARD/Roms/FBNEO") &&
           !mainui_game_menu_saves_disabled(&menu, "/mnt/SDCARD/Roms/ARCADE2"));
    assert(mainui_game_menu_parse(
        &menu, "{\"saved_games\": {\"disabled_rom_dirs\": [\"MAME\", \"scummvm\", \"mame\", "
               "\"\", 3, \"a/b\", \"..\", \"My Console\"]}}"));
    assert(menu.saves_disabled_count == 3 &&
           mainui_game_menu_saves_disabled(&menu, "/mnt/SDCARD/Roms/MAME") &&
           mainui_game_menu_saves_disabled(&menu, "/mnt/SDCARD/Roms/SCUMMVM") &&
           mainui_game_menu_saves_disabled(&menu, "/mnt/SDCARD/Roms/My Console") &&
           !mainui_game_menu_saves_disabled(&menu, "/mnt/SDCARD/Roms/ARCADE"));
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"disabled_rom_dirs\": []}}"));
    assert(!menu.saves_disabled_count &&
           !mainui_game_menu_saves_disabled(&menu, "/mnt/SDCARD/Roms/ARCADE"));
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"disabled_rom_dirs\": \"MAME\"}}"));
    assert(menu.saves_disabled_count == 1 && !strcmp(menu.saves_disabled[0], "ARCADE"));
    /* layout: framed unless "overlay". */
    assert(menu.saves_framed);
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"layout\": \"overlay\"}}"));
    assert(!menu.saves_framed);
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"layout\": \"Overlay\"}}"));
    assert(menu.saves_framed);
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"layout\": \"framed\"}}"));
    assert(menu.saves_framed);
    /* show_auto: only true lists Auto Save. */
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"show_auto\": true}}"));
    assert(menu.saves_auto);
    assert(mainui_game_menu_parse(&menu, "{\"saved_games\": {\"show_auto\": \"yes\"}}"));
    assert(!menu.saves_auto);
    /* Only a JSON boolean changes the default-on Gallery shortcut. */
    assert(mainui_game_menu_parse(&menu, "{\"gallery\": {\"right_shortcut\": false}}"));
    assert(!menu.gallery_right);
    assert(mainui_game_menu_parse(&menu, "{\"gallery\": {\"right_shortcut\": true}}"));
    assert(menu.gallery_right);
    assert(mainui_game_menu_parse(&menu, "{\"gallery\": {\"right_shortcut\": 0}}"));
    assert(menu.gallery_right);
    assert(mainui_game_menu_parse(&menu, "{\"gallery\": {\"right_shortcut\": \"false\"}}"));
    assert(menu.gallery_right);
    /* upscale_images is on unless the JSON false. */
    assert(menu.gallery_upscale);
    assert(mainui_game_menu_parse(&menu, "{\"gallery\": {\"upscale_images\": false}}"));
    assert(!menu.gallery_upscale && menu.gallery_right);
    assert(mainui_game_menu_parse(&menu, "{\"gallery\": {\"upscale_images\": 0}}"));
    assert(menu.gallery_upscale);
    assert(mainui_game_menu_parse(&menu, "{\"details\": []}") && !menu.details_count);
    assert(!mainui_game_menu_parse(&menu, "[]") && menu.details_count == MAINUI_DETAILS_ROWS);
}

int mainui_suite_context(void)
{
    favorite_popups();
    game_menus();
    console_views();
    MainUIContext c;
    mainui_context_parse(&c, NULL, false, false, false);
    assert(c.visible_count == 1 && c.entries[0].action == CONTEXT_REFRESH && c.hotkey);
    mainui_context_parse(&c, "{}", true, true, true);
    assert(c.visible_count == 3 && c.entries[2].action == CONTEXT_TWEAKS && c.hotkey);
    /* Root hotkey: omitted or literal true enables; any other present value disables. */
    mainui_context_parse(&c, "{\"hotkey\":true}", false, false, false);
    assert(c.hotkey);
    static const char *const disabled[] = {"{\"hotkey\":false}",    "{\"hotkey\":null}",
                                           "{\"hotkey\":0}",        "{\"hotkey\":1}",
                                           "{\"hotkey\":\"true\"}", "{\"hotkey\":\"false\"}"};
    for (size_t i = 0; i < sizeof disabled / sizeof *disabled; ++i) {
        mainui_context_parse(&c, disabled[i], false, false, false);
        assert(!c.hotkey);
    }
    mainui_context_parse(&c, "{\"context\":[]}", true, true, true);
    assert(c.count == 0);
    mainui_context_parse(&c,
                         "{\"context\":{\"settings\":false,\"favs\":true,\"settings\":true,"
                         "\"recent\":1},\"hotkey\":false}",
                         false, false, false);
    assert(!c.hotkey && c.count == 3 && c.visible_count == 2);
    assert(c.entries[0].action == CONTEXT_SETTINGS && c.entries[0].enabled);
    assert(c.entries[1].action == CONTEXT_FAVORITES && !c.entries[2].enabled);
    mainui_context_reveal(&c);
    assert(c.visible_count == 3);
    mainui_context_parse(
        &c,
        "{\"context\":[\"custom1\",\"custom2\",\"games\",\"games\"],\"custom\":{\"custom1\":{"
        "\"label\":\"Literal %s\",\"launch\":\"/mnt/SDCARD/tool.sh\",\"type\":3.5}}}",
        false, false, false);
    assert(c.count == 2 && c.entries[0].type == 3);
    assert(!strcmp(mainui_context_label(&c.entries[0]), "Literal %s"));
    assert(mainui_context_section(c.entries[1].action) == MAINUI_MENU_GAMES);
    /* Search and Tweaks named in main-menu.json need their app installed:
     * without it they are left out, shown or hidden, and reveal cannot
     * expose them; reinstalled, the same configuration shows them again. */
    const char *apps = "{\"context\":{\"tweaks\":true,\"search\":false,\"games\":true}}";
    mainui_context_parse(&c, apps, false, false, false);
    assert(c.count == 1 && c.entries[0].action == CONTEXT_GAMES);
    mainui_context_reveal(&c);
    assert(c.count == 1 && c.visible_count == 1);
    mainui_context_parse(&c, "{\"context\":[\"search\",\"tweaks\"]}", false, false, false);
    assert(c.count == 0 && c.visible_count == 0);
    mainui_context_parse(&c, apps, true, true, true);
    assert(c.count == 3 && c.visible_count == 2 && c.entries[0].action == CONTEXT_TWEAKS);
    assert(c.entries[1].action == CONTEXT_SEARCH && !c.entries[1].enabled);
    mainui_context_reveal(&c);
    assert(c.visible_count == 3);
    /* Themes needs App/ThemeSwitcher in the same way. */
    const char *themes = "{\"context\":{\"themes\":false,\"games\":true}}";
    mainui_context_parse(&c, themes, true, true, false);
    assert(c.count == 1 && c.entries[0].action == CONTEXT_GAMES);
    mainui_context_reveal(&c);
    assert(c.count == 1 && c.visible_count == 1);
    mainui_context_parse(&c, themes, false, false, true);
    assert(c.count == 2 && c.visible_count == 1 && c.entries[0].action == CONTEXT_THEMES);
    mainui_context_reveal(&c);
    assert(c.visible_count == 2);
    puts("Context defaults, order, aliases, reveal, missing apps and bounded custom action "
         "checks passed");
    return 0;
}
