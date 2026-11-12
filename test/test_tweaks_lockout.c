/**
 * @file test_tweaks_lockout.c
 * @brief Tweaks > Home screen: the last way back to Tweaks stays on
 *
 * Tweaks is opened from the Apps tab, from the SELECT menu's Tweaks entry or
 * with L1+R1+L2+R2 (which shows every entry). With all three off it could
 * only be brought back by editing main-menu.json on a PC. The production
 * actions of tweaks/main_menu.h run here on a temporary config folder.
 *
 * Build and run: make -f Makefile.unit test_tweaks_lockout
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_DIR "/tmp/onion_test_tweaks_lockout"
#define MAINMENU_CONFIG_DIR TEST_DIR
#define MAINMENU_SD_DIR TEST_DIR "/sd"
#define CONSOLEVIEW_THUMBS_PARENT TEST_DIR "/app"

/* main_menu.h uses the Tweaks menu state from appstate.h. */
#define SYSTEM_LANG_H__
typedef int lang_hash;
static char **lang_list = NULL;
#ifndef LANG_MAX
#define LANG_MAX 400
#endif
#include "components/list.h"
static List *menu_stack[5];
static int menu_level = 0;
static bool header_changed = false;
static List _menu_game_lists;
static List _menu_home_screen, _menu_main_menu_items, _menu_main_context_items, _menu_console_view,
    _menu_carousel_look, _menu_carousel_movement, _menu_game_menus, _menu_game_info_menu, _menu_saved_games,
    _menu_list_section, _menu_glo;
static bool test_glo_network = true;
#define GLO_HAS_NETWORK test_glo_network
#include "../src/tweaks/main_menu.h"

void SDL_FreeSurface(SDL_Surface *surface) { (void)surface; }

#define TAB_RECENTS 0
#define TAB_APPS 4
#define CTX_SEARCH 2
#define CTX_TWEAKS 10

static void touch(const char *path)
{
    FILE *f = fopen(path, "w");
    if (f != NULL)
        fclose(f);
}

/* No main-menu.json, Search and Tweaks installed. */
static void fresh(void)
{
    if (system("rm -rf " TEST_DIR " && mkdir -p " TEST_DIR "/sd/App/Tweaks " TEST_DIR "/sd/App/Search") != 0)
        fprintf(stderr, "setup failed\n");
    touch(TEST_DIR "/sd/App/Tweaks/launch.sh");
    touch(TEST_DIR "/sd/App/Search/launch.sh");
}

static ListItem item_of(int action_id, int value)
{
    ListItem item;
    memset(&item, 0, sizeof(item));
    item.action_id = action_id;
    item.value = value;
    return item;
}

/* Switch a tab, a SELECT menu entry or the hotkey; the value the switch
 * shows afterwards (the one asked for, or put back when refused). */
static int set_tab(int tab, int value)
{
    ListItem item = item_of(tab, value);
    mainmenu_action_menu_item(&item);
    return item.value;
}

static int set_context(int entry, int value)
{
    ListItem item = item_of(entry, value);
    mainmenu_action_context_item(&item);
    return item.value;
}

static int set_hotkey(int value)
{
    ListItem item = item_of(0, value);
    mainmenu_action_hotkey(&item);
    return item.value;
}

static bool tab_on(int tab)
{
    bool menu[MAINMENU_MENU_COUNT];
    bool context[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_states(menu, context);
    return menu[tab];
}

static bool context_shown(int entry)
{
    bool menu[MAINMENU_MENU_COUNT];
    bool context[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_shown_states(menu, context);
    return context[entry];
}

TEST(defaults_leave_three_ways_in)
{
    fresh();
    ASSERT_TRUE(tab_on(TAB_APPS));
    ASSERT_TRUE(context_shown(CTX_TWEAKS));
    ASSERT_TRUE(mainmenu_load_hotkey_state());
    ASSERT_FALSE(mainmenu_would_lock_out(-1, -1, false));
    /* Any one, and any two, can go. */
    ASSERT_FALSE(mainmenu_would_lock_out(TAB_APPS, -1, false));
    ASSERT_FALSE(mainmenu_would_lock_out(-1, CTX_TWEAKS, false));
    ASSERT_FALSE(mainmenu_would_lock_out(-1, -1, true));
    ASSERT_FALSE(mainmenu_would_lock_out(TAB_APPS, CTX_TWEAKS, false));
    ASSERT_FALSE(mainmenu_would_lock_out(TAB_APPS, -1, true));
    ASSERT_FALSE(mainmenu_would_lock_out(-1, CTX_TWEAKS, true));
    /* All three cannot. */
    ASSERT_TRUE(mainmenu_would_lock_out(TAB_APPS, CTX_TWEAKS, true));
}

TEST(apps_tab_is_kept_when_it_is_the_last_way)
{
    fresh();
    ASSERT_EQ(set_context(CTX_TWEAKS, 0), 0);
    ASSERT_FALSE(context_shown(CTX_TWEAKS));
    ASSERT_EQ(set_hotkey(0), 0);
    ASSERT_FALSE(mainmenu_load_hotkey_state());

    ASSERT_EQ(set_tab(TAB_APPS, 0), 1); /* refused: the switch goes back to On */
    ASSERT_TRUE(tab_on(TAB_APPS));      /* and nothing was written */

    /* Other switches are not held back. */
    ASSERT_EQ(set_tab(TAB_RECENTS, 0), 0);
    ASSERT_EQ(set_context(CTX_SEARCH, 0), 0);
    ASSERT_TRUE(tab_on(TAB_APPS));
}

TEST(hotkey_is_kept_when_it_is_the_last_way)
{
    fresh();
    ASSERT_EQ(set_context(CTX_TWEAKS, 0), 0);
    ASSERT_EQ(set_tab(TAB_APPS, 0), 0);
    ASSERT_FALSE(tab_on(TAB_APPS));

    ASSERT_EQ(set_hotkey(0), 1);
    ASSERT_TRUE(mainmenu_load_hotkey_state());
}

TEST(select_menu_entry_is_kept_when_it_is_the_last_way)
{
    fresh();
    ASSERT_EQ(set_tab(TAB_APPS, 0), 0);
    ASSERT_EQ(set_hotkey(0), 0);

    ASSERT_EQ(set_context(CTX_TWEAKS, 0), 1);
    ASSERT_TRUE(context_shown(CTX_TWEAKS));

    /* Once another way is back, it can go. */
    ASSERT_EQ(set_hotkey(1), 1);
    ASSERT_EQ(set_context(CTX_TWEAKS, 0), 0);
    ASSERT_FALSE(context_shown(CTX_TWEAKS));
}

TEST(a_missing_tweaks_app_is_not_a_way_in)
{
    /* Open MainUI leaves Tweaks out of the SELECT menu without its app,
     * whatever main-menu.json says: that entry can't be counted on. */
    fresh();
    remove(TEST_DIR "/sd/App/Tweaks/launch.sh");
    ASSERT_FALSE(context_shown(CTX_TWEAKS));
    ASSERT_EQ(set_hotkey(0), 0);
    ASSERT_EQ(set_tab(TAB_APPS, 0), 1);
    ASSERT_TRUE(tab_on(TAB_APPS));
}

TEST(turning_things_on_is_never_refused)
{
    fresh();
    ASSERT_EQ(set_context(CTX_TWEAKS, 0), 0);
    ASSERT_EQ(set_hotkey(0), 0);
    ASSERT_EQ(set_tab(TAB_RECENTS, 1), 1);
    ASSERT_EQ(set_context(CTX_TWEAKS, 1), 1);
    ASSERT_EQ(set_hotkey(1), 1);
    ASSERT_TRUE(tab_on(TAB_RECENTS));
    ASSERT_TRUE(context_shown(CTX_TWEAKS));
    ASSERT_TRUE(mainmenu_load_hotkey_state());
}

/* All three ways already off (main-menu.json edited by hand, or left by a
 * version without the guard) and Tweaks reached some other way: the other
 * switches still work. */
TEST(already_locked_out_other_switches_still_work)
{
    fresh();
    FILE *f = fopen(TEST_DIR "/main-menu.json", "w");
    if (f != NULL) {
        fputs("{\"hotkey\":false,"
              "\"menu\":{\"recents\":true,\"favorites\":true,\"games\":true,\"apps\":false,\"settings\":true},"
              "\"context\":{\"refresh\":true,\"search\":true}}",
              f);
        fclose(f);
    }
    ASSERT_EQ(set_tab(TAB_RECENTS, 0), 0);
    ASSERT_FALSE(tab_on(TAB_RECENTS));
    ASSERT_EQ(set_context(CTX_SEARCH, 0), 0);
    ASSERT_FALSE(context_shown(CTX_SEARCH));
    /* and a way in can be switched on again, then is kept as the last one */
    ASSERT_EQ(set_hotkey(1), 1);
    ASSERT_EQ(set_hotkey(0), 1);
}

int main(void)
{
    (void)menu_stack;
    (void)menu_level;
    (void)header_changed;
    (void)lang_list;
    printf("\n=== Tweaks: the last way back to Tweaks ===\n\n");
    RUN_TEST(defaults_leave_three_ways_in);
    RUN_TEST(apps_tab_is_kept_when_it_is_the_last_way);
    RUN_TEST(hotkey_is_kept_when_it_is_the_last_way);
    RUN_TEST(select_menu_entry_is_kept_when_it_is_the_last_way);
    RUN_TEST(a_missing_tweaks_app_is_not_a_way_in);
    RUN_TEST(turning_things_on_is_never_refused);
    RUN_TEST(already_locked_out_other_switches_still_work);
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
