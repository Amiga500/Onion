/**
 * @file test_tweaks_game_lists.c
 * @brief Tweaks > Appearance > Game lists and Main menu, on the real files
 *
 * Runs the production code of tweaks/game_lists_core.h and tweaks/main_menu.h
 * against a temporary config folder:
 *  - title scrolling Off keeps .romListTitleScroll ("-1,<speed>"), so an
 *    update can't add the default back and turn scrolling on again;
 *  - the scroll speeds offered are the ones Open MainUI renders exactly;
 *  - key repeat at the stock values removes its file;
 *  - Main menu defaults show Apps (not Expert) and follow .showRecents and
 *    .showExpert, as Open MainUI reads them, and a first change keeps Apps.
 *
 * Build and run: make -f Makefile.unit test_tweaks_game_lists
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_DIR "/tmp/onion_test_tweaks_game_lists"
#define GAMELISTS_CONFIG_DIR TEST_DIR
#define MAINMENU_CONFIG_DIR TEST_DIR

#include "../src/tweaks/game_lists_core.h"

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
static List _menu_main_menu, _menu_main_menu_items, _menu_main_context_items;
#include "../src/tweaks/main_menu.h"

void SDL_FreeSurface(SDL_Surface *surface) { (void)surface; }

static void reset_dir(void)
{
    (void)!system("rm -rf " TEST_DIR " && mkdir -p " TEST_DIR);
}

static void read_file(const char *path, char *out, size_t size)
{
    out[0] = '\0';
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return;
    size_t n = fread(out, 1, size - 1, fp);
    out[n] = '\0';
    fclose(fp);
}

static void touch(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp != NULL)
        fclose(fp);
}

/* ---- Title scrolling ---- */

TEST(scroll_off_keeps_the_file)
{
    reset_dir();
    gamelists_write_text(GAMELISTS_SCROLL_PATH, "1000,50\n");
    ASSERT_TRUE(gamelists_store_scroll(GAMELISTS_SCROLL_OFF, 50));
    char text[64];
    read_file(GAMELISTS_SCROLL_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "-1,50\n");
}

TEST(scroll_off_reads_back_as_off_with_its_speed)
{
    reset_dir();
    gamelists_store_scroll(GAMELISTS_SCROLL_OFF, 150);
    int delay, speed;
    gamelists_load_scroll(&delay, &speed);
    ASSERT_EQ(delay, GAMELISTS_SCROLL_OFF);
    ASSERT_EQ(speed, 150);
    ASSERT_EQ(gamelists_value_scroll_delay(), 0);
}

TEST(scroll_on_again_keeps_the_speed)
{
    reset_dir();
    gamelists_store_scroll(GAMELISTS_SCROLL_OFF, 150);
    int delay, speed;
    gamelists_load_scroll(&delay, &speed);
    gamelists_store_scroll(1000, speed);
    char text[64];
    read_file(GAMELISTS_SCROLL_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "1000,150\n");
}

TEST(scroll_missing_file_is_off)
{
    reset_dir();
    int delay, speed;
    gamelists_load_scroll(&delay, &speed);
    ASSERT_EQ(delay, GAMELISTS_SCROLL_OFF);
    ASSERT_EQ(speed, GAMELISTS_SCROLL_DEFAULT_SPEED);
}

TEST(scroll_default_file_maps_to_menu)
{
    reset_dir();
    gamelists_write_text(GAMELISTS_SCROLL_PATH, "1000,50");
    char label[64];
    gamelists_label_scroll_delay(gamelists_value_scroll_delay(), label, sizeof(label));
    ASSERT_STREQ(label, "1000 ms");
    gamelists_label_scroll_speed(gamelists_value_scroll_speed(), label, sizeof(label));
    ASSERT_STREQ(label, "50 px/s");
}

TEST(scroll_speeds_are_rendered_exactly)
{
    /* Open MainUI moves a whole number of pixels per 40 ms frame. */
    for (int i = 0; i < GAMELISTS_ARRAY_COUNT(gamelists_scroll_speeds); i++) {
        int speed = gamelists_scroll_speeds[i];
        ASSERT_TRUE(speed == 5 || speed % 25 == 0);
        ASSERT_TRUE(speed >= 5 && speed <= 400);
    }
}

TEST(scroll_speed_snaps_to_offered_value)
{
    reset_dir();
    gamelists_write_text(GAMELISTS_SCROLL_PATH, "700,120");
    char label[64];
    gamelists_label_scroll_speed(gamelists_value_scroll_speed(), label, sizeof(label));
    ASSERT_STREQ(label, "125 px/s");
}

TEST(pair_parser_accepts_open_mainui_forms)
{
    int a, b;
    ASSERT_TRUE(gamelists_parse_pair("1000,50", &a, &b));
    ASSERT_EQ(a, 1000);
    ASSERT_EQ(b, 50);
    ASSERT_TRUE(gamelists_parse_pair("500 25\n", &a, &b));
    ASSERT_EQ(b, 25);
    ASSERT_TRUE(gamelists_parse_pair("-1,120", &a, &b));
    ASSERT_EQ(a, -1);
    ASSERT_FALSE(gamelists_parse_pair("1000", &a, &b));
    ASSERT_FALSE(gamelists_parse_pair("", &a, &b));
    ASSERT_FALSE(gamelists_parse_pair(NULL, &a, &b));
}

/* ---- Key repeat, rows, font ---- */

TEST(repeat_stock_values_remove_the_file)
{
    reset_dir();
    ASSERT_TRUE(gamelists_store_repeat(300, 50));
    ASSERT_EQ(access(GAMELISTS_REPEAT_PATH, F_OK), 0);
    ASSERT_TRUE(gamelists_store_repeat(500, 100));
    ASSERT_TRUE(access(GAMELISTS_REPEAT_PATH, F_OK) != 0);
}

TEST(rows_and_font_menu_values)
{
    reset_dir();
    ASSERT_EQ(gamelists_value_rows(), 0);
    gamelists_write_int(GAMELISTS_ROWS_PATH, 9);
    ASSERT_EQ(gamelists_value_rows(), 3);
    gamelists_write_int(GAMELISTS_ROWS_PATH, 99);
    ASSERT_EQ(gamelists_rows(), GAMELISTS_ROWS_MAX);

    char label[64];
    ASSERT_EQ(gamelists_value_font(), 0);
    gamelists_label_font(0, label, sizeof(label));
    ASSERT_STREQ(label, "Theme default");
    gamelists_write_int(GAMELISTS_FONT_PATH, 24);
    gamelists_label_font(gamelists_value_font(), label, sizeof(label));
    ASSERT_STREQ(label, "24 px");
}

TEST(write_leaves_no_temporary_file)
{
    reset_dir();
    gamelists_write_int(GAMELISTS_ROWS_PATH, 8);
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", GAMELISTS_ROWS_PATH, (long)getpid());
    ASSERT_TRUE(access(tmp, F_OK) != 0);
}

/* ---- Main menu ---- */

TEST(main_menu_default_shows_apps_not_expert)
{
    reset_dir();
    bool menu[MAINMENU_MENU_COUNT], context[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_states(menu, context);
    ASSERT_FALSE(menu[0]); /* Recents */
    ASSERT_TRUE(menu[1]);  /* Favorites */
    ASSERT_TRUE(menu[2]);  /* Games */
    ASSERT_FALSE(menu[3]); /* Expert */
    ASSERT_TRUE(menu[4]);  /* Apps */
    ASSERT_TRUE(menu[5]);  /* Settings */
}

TEST(main_menu_default_follows_legacy_markers)
{
    reset_dir();
    touch(TEST_DIR "/.showRecents");
    touch(TEST_DIR "/.showExpert");
    bool menu[MAINMENU_MENU_COUNT], context[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_states(menu, context);
    ASSERT_TRUE(menu[0]);
    ASSERT_TRUE(menu[3]);
}

TEST(main_menu_first_change_keeps_apps)
{
    reset_dir();
    ASSERT_TRUE(mainmenu_store_menu_item(0, true)); /* show Recents */
    bool menu[MAINMENU_MENU_COUNT], context[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_states(menu, context);
    ASSERT_TRUE(menu[0]);
    ASSERT_TRUE(menu[4]); /* Apps still there */
    ASSERT_FALSE(menu[3]);

    char text[2048];
    read_file(TEST_DIR "/main-menu.json", text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"apps\":\ttrue") != NULL || strstr(text, "\"apps\": true") != NULL);
}

TEST(main_menu_unnamed_section_follows_marker)
{
    reset_dir();
    gamelists_write_text(TEST_DIR "/main-menu.json", "{\"menu\": {\"games\": true, \"apps\": true}}");
    touch(TEST_DIR "/.showExpert");
    bool menu[MAINMENU_MENU_COUNT], context[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_states(menu, context);
    ASSERT_TRUE(menu[3]);  /* not named: marker */
    ASSERT_FALSE(menu[1]); /* not named, no marker: hidden, as Open MainUI */
}

TEST(main_menu_explicit_false_beats_marker)
{
    reset_dir();
    gamelists_write_text(TEST_DIR "/main-menu.json", "{\"menu\": {\"games\": true, \"expert\": false}}");
    touch(TEST_DIR "/.showExpert");
    bool menu[MAINMENU_MENU_COUNT], context[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_states(menu, context);
    ASSERT_FALSE(menu[3]);
}

int main(void)
{
    RUN_TEST(scroll_off_keeps_the_file);
    RUN_TEST(scroll_off_reads_back_as_off_with_its_speed);
    RUN_TEST(scroll_on_again_keeps_the_speed);
    RUN_TEST(scroll_missing_file_is_off);
    RUN_TEST(scroll_default_file_maps_to_menu);
    RUN_TEST(scroll_speeds_are_rendered_exactly);
    RUN_TEST(scroll_speed_snaps_to_offered_value);
    RUN_TEST(pair_parser_accepts_open_mainui_forms);
    RUN_TEST(repeat_stock_values_remove_the_file);
    RUN_TEST(rows_and_font_menu_values);
    RUN_TEST(write_leaves_no_temporary_file);
    RUN_TEST(main_menu_default_shows_apps_not_expert);
    RUN_TEST(main_menu_default_follows_legacy_markers);
    RUN_TEST(main_menu_first_change_keeps_apps);
    RUN_TEST(main_menu_unnamed_section_follows_marker);
    RUN_TEST(main_menu_explicit_false_beats_marker);

    (void)!system("rm -rf " TEST_DIR);
    TEST_REPORT();
    return test_failures;
}
