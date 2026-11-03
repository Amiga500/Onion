/**
 * @file test_tweaks_game_lists.c
 * @brief Tweaks > Game lists and Home screen, on the real files
 *
 * Runs the production code of tweaks/game_lists_core.h and tweaks/main_menu.h
 * against a temporary config folder:
 *  - title scrolling Off keeps .romListTitleScroll ("-1,<speed>"), so an
 *    update can't add the default back and turn scrolling on again;
 *  - the scroll speeds offered are the ones Open MainUI renders exactly;
 *  - Game Info auto scroll: no file is Off (and keeps the speed for later),
 *    an empty file is 1500 ms at 40 px/s;
 *  - console-view.json: Games and Expert views, other keys kept, the grid
 *    for both removes the file, a damaged file is not overwritten;
 *    Console pictures writes "thumbs" (and makes the folder);
 *  - key repeat at the stock values removes its file;
 *  - Main menu defaults show Apps (not Expert) and follow .showRecents and
 *    .showExpert, as Open MainUI reads them, and a first change keeps Apps;
 *  - Search shows off while its app is missing, as Open MainUI hides it,
 *    and main-menu.json still enables it for when the app is installed;
 *    the same for a custom action without its label and launch.
 *
 * Build and run: make -f Makefile.unit test_tweaks_game_lists
 */

#include "onion_test.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_DIR "/tmp/onion_test_tweaks_game_lists"
#define GAMELISTS_CONFIG_DIR TEST_DIR
#define MAINMENU_CONFIG_DIR TEST_DIR
#define MAINMENU_SD_DIR TEST_DIR "/sd"
#define CONSOLEVIEW_THUMBS_PARENT TEST_DIR "/app"

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
static List _menu_game_lists;
static List _menu_home_screen, _menu_main_menu_items, _menu_main_context_items, _menu_console_view,
    _menu_carousel_look, _menu_carousel_movement, _menu_game_menus, _menu_game_info_menu, _menu_saved_games,
    _menu_list_section, _menu_glo;
static bool test_glo_network = true;
#define GLO_HAS_NETWORK test_glo_network
#include "../src/tweaks/main_menu.h"

/* game_lists.h: its menus, with appstate.h's lists declared here. */
#define TWEAKS_APPSTATE_H__
static List _menu_game_lists_layout, _menu_game_lists_scrolling, _menu_game_lists_previews;
#include "../src/tweaks/game_lists.h"

void SDL_FreeSurface(SDL_Surface *surface) { (void)surface; }

static bool source_has(const char *path, const char *text);

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
    ASSERT_STREQ(label, "1000 ms (default)");
    gamelists_label_scroll_speed(gamelists_value_scroll_speed(), label, sizeof(label));
    ASSERT_STREQ(label, "50 px/s (default)");
}

/* "(default)": the value a new install starts with, on every number of
   the Game lists menus; the shipped title scroll file says which. */
TEST(default_values_are_marked)
{
    char label[64];
    FILE *fp = fopen("../static/configs/.tmp_update/config/.romListTitleScroll", "r");
    ASSERT_TRUE(fp != NULL);
    int delay = 0, speed = 0;
    ASSERT_EQ(fscanf(fp, "%d,%d", &delay, &speed), 2);
    fclose(fp);
    ASSERT_EQ(delay, GAMELISTS_SCROLL_DEFAULT_DELAY);
    ASSERT_EQ(speed, GAMELISTS_SCROLL_DEFAULT_SPEED);

    gamelists_label_rows(0, label, sizeof(label));
    ASSERT_STREQ(label, "6 (default)");
    gamelists_label_rows(1, label, sizeof(label));
    ASSERT_STREQ(label, "7 rows");
    gamelists_label_scroll_delay(0, label, sizeof(label));
    ASSERT_STREQ(label, "Off");
    gamelists_label_scroll_delay(12, label, sizeof(label));
    ASSERT_STREQ(label, "1250 ms");
    gamelists_label_scroll_speed(1, label, sizeof(label));
    ASSERT_STREQ(label, "25 px/s");
    gamelists_label_repeat_delay(8, label, sizeof(label));
    ASSERT_STREQ(label, "500 ms (default)");
    gamelists_label_repeat_delay(9, label, sizeof(label));
    ASSERT_STREQ(label, "550 ms");
    gamelists_label_repeat_interval(7, label, sizeof(label));
    ASSERT_STREQ(label, "100 ms (default)");
    gamelists_label_repeat_interval(8, label, sizeof(label));
    ASSERT_STREQ(label, "120 ms");
    gamelists_label_video_preview(1, label, sizeof(label));
    ASSERT_STREQ(label, "Immediate");
    gamelists_label_info_scroll_speed(3, label, sizeof(label));
    ASSERT_STREQ(label, "20 px/s (default)");
    gamelists_label_info_scroll_speed(4, label, sizeof(label));
    ASSERT_STREQ(label, "25 px/s");
    ASSERT_TRUE(source_has("../src/tweaks/game_lists.h", "{\"Rolling (default)\", \"Bounce\"}"));
}

/* Open MainUI 1.1.0: "<delay>,<speed>,bounce". Rolling stays two values. */
TEST(scroll_style_bounce_is_read)
{
    reset_dir();
    gamelists_write_text(GAMELISTS_SCROLL_PATH, "700,125,bounce\n");
    ASSERT_EQ(gamelists_scroll_style(), GAMELISTS_SCROLL_BOUNCE);
    int delay, speed;
    gamelists_load_scroll(&delay, &speed);
    ASSERT_EQ(delay, 700);
    ASSERT_EQ(speed, 125);
    gamelists_write_text(GAMELISTS_SCROLL_PATH, "700,125,rolling\n");
    ASSERT_EQ(gamelists_scroll_style(), GAMELISTS_SCROLL_ROLLING);
    gamelists_load_scroll(&delay, &speed);
    ASSERT_EQ(delay, 700);
    gamelists_write_text(GAMELISTS_SCROLL_PATH, "700,125\n");
    ASSERT_EQ(gamelists_scroll_style(), GAMELISTS_SCROLL_ROLLING);
    gamelists_write_text(GAMELISTS_SCROLL_PATH, "700, 125 , bounce \n");
    ASSERT_EQ(gamelists_scroll_style(), GAMELISTS_SCROLL_BOUNCE);
    gamelists_load_scroll(&delay, &speed);
    ASSERT_EQ(speed, 125);
    gamelists_write_text(GAMELISTS_SCROLL_PATH, "700,125,bouncy\n");
    ASSERT_EQ(gamelists_scroll_style(), GAMELISTS_SCROLL_ROLLING);
}

TEST(scroll_style_written_and_kept)
{
    reset_dir();
    char text[64];
    gamelists_store_scroll(1000, 50);
    ASSERT_TRUE(gamelists_set_scroll_style(GAMELISTS_SCROLL_BOUNCE));
    read_file(GAMELISTS_SCROLL_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "1000,50,bounce\n");
    /* A new delay or speed keeps Bounce, Off too. */
    gamelists_store_scroll(500, 50);
    gamelists_store_scroll(500, 150);
    read_file(GAMELISTS_SCROLL_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "500,150,bounce\n");
    gamelists_store_scroll(GAMELISTS_SCROLL_OFF, 150);
    read_file(GAMELISTS_SCROLL_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "-1,150,bounce\n");
    int delay, speed;
    gamelists_load_scroll(&delay, &speed);
    ASSERT_EQ(delay, GAMELISTS_SCROLL_OFF);
    ASSERT_EQ(speed, 150);
    /* Rolling goes back to two values, as older launchers read them. */
    gamelists_store_scroll(500, 150);
    ASSERT_TRUE(gamelists_set_scroll_style(GAMELISTS_SCROLL_ROLLING));
    read_file(GAMELISTS_SCROLL_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "500,150\n");
}

TEST(scroll_style_without_file)
{
    reset_dir();
    ASSERT_EQ(gamelists_scroll_style(), GAMELISTS_SCROLL_ROLLING);
    ASSERT_TRUE(gamelists_set_scroll_style(GAMELISTS_SCROLL_BOUNCE));
    char text[64];
    read_file(GAMELISTS_SCROLL_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "-1,50,bounce\n");
}

/* Open MainUI 1.1.0: .romListVideoPreview, a delay in ms, 1000 when empty. */
TEST(video_preview_off_without_file)
{
    reset_dir();
    ASSERT_EQ(gamelists_load_video_preview(), GAMELISTS_VIDEO_OFF);
    char label[64];
    gamelists_label_video_preview(gamelists_value_video_preview(), label, sizeof(label));
    ASSERT_STREQ(label, "Off (default)");
}

TEST(video_preview_written_and_read)
{
    reset_dir();
    char text[32], label[64];
    ASSERT_TRUE(gamelists_store_video_preview(1500));
    read_file(GAMELISTS_VIDEO_PREVIEW_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "1500\n");
    ASSERT_EQ(gamelists_load_video_preview(), 1500);
    gamelists_label_video_preview(gamelists_value_video_preview(), label, sizeof(label));
    ASSERT_STREQ(label, "After 1500 ms");
    ASSERT_TRUE(gamelists_store_video_preview(0));
    gamelists_label_video_preview(gamelists_value_video_preview(), label, sizeof(label));
    ASSERT_STREQ(label, "Immediate");
    ASSERT_TRUE(gamelists_store_video_preview(GAMELISTS_VIDEO_OFF));
    ASSERT_EQ(access(GAMELISTS_VIDEO_PREVIEW_PATH, F_OK), -1);
}

TEST(video_preview_empty_file_is_1000ms)
{
    reset_dir();
    touch(GAMELISTS_VIDEO_PREVIEW_PATH);
    ASSERT_EQ(gamelists_load_video_preview(), 1000);
    gamelists_write_text(GAMELISTS_VIDEO_PREVIEW_PATH, "abc\n");
    ASSERT_EQ(gamelists_load_video_preview(), 1000);
    gamelists_write_text(GAMELISTS_VIDEO_PREVIEW_PATH, "1200\n");
    char label[64];
    gamelists_label_video_preview(gamelists_value_video_preview(), label, sizeof(label));
    ASSERT_STREQ(label, "After 1000 ms"); /* nearest offered */
}

/* Open MainUI 1.1.0 rc4: "<delay>,fullscreen" plays the video full screen,
 * "<delay>" or "<delay>,cover" where the cover is. */
TEST(video_preview_size_read)
{
    reset_dir();
    gamelists_video_size_pending = GAMELISTS_VIDEO_COVER;
    int size;
    gamelists_write_text(GAMELISTS_VIDEO_PREVIEW_PATH, "1000,fullscreen\n");
    ASSERT_EQ(gamelists_read_video_preview(&size), 1000);
    ASSERT_EQ(size, GAMELISTS_VIDEO_FULLSCREEN);
    gamelists_write_text(GAMELISTS_VIDEO_PREVIEW_PATH, "500,cover\n");
    ASSERT_EQ(gamelists_read_video_preview(&size), 500);
    ASSERT_EQ(size, GAMELISTS_VIDEO_COVER);
    gamelists_write_text(GAMELISTS_VIDEO_PREVIEW_PATH, "750, fullscreen \n");
    ASSERT_EQ(gamelists_read_video_preview(&size), 750);
    ASSERT_EQ(size, GAMELISTS_VIDEO_FULLSCREEN);
    gamelists_write_text(GAMELISTS_VIDEO_PREVIEW_PATH, ",fullscreen\n");
    ASSERT_EQ(gamelists_read_video_preview(&size), 1000); /* no delay: 1000 ms */
    ASSERT_EQ(size, GAMELISTS_VIDEO_FULLSCREEN);
    gamelists_write_text(GAMELISTS_VIDEO_PREVIEW_PATH, "250,fullscreenx\n");
    ASSERT_EQ(gamelists_read_video_preview(&size), 250);
    ASSERT_EQ(size, GAMELISTS_VIDEO_COVER); /* only the exact word */
    gamelists_write_text(GAMELISTS_VIDEO_PREVIEW_PATH, "250\n");
    ASSERT_EQ(gamelists_read_video_preview(&size), 250);
    ASSERT_EQ(size, GAMELISTS_VIDEO_COVER);
}

TEST(video_preview_size_and_delay_keep_each_other)
{
    reset_dir();
    gamelists_video_size_pending = GAMELISTS_VIDEO_COVER;
    char text[32];
    ASSERT_TRUE(gamelists_store_video_preview(500));
    ASSERT_TRUE(gamelists_set_video_size(GAMELISTS_VIDEO_FULLSCREEN));
    read_file(GAMELISTS_VIDEO_PREVIEW_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "500,fullscreen\n");
    ASSERT_TRUE(gamelists_store_video_preview(2000));
    read_file(GAMELISTS_VIDEO_PREVIEW_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "2000,fullscreen\n");
    ASSERT_TRUE(gamelists_set_video_size(GAMELISTS_VIDEO_COVER));
    read_file(GAMELISTS_VIDEO_PREVIEW_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "2000\n"); /* cover as the delay alone, read by every rc */
    ASSERT_EQ(gamelists_video_size(), GAMELISTS_VIDEO_COVER);
}

TEST(video_preview_size_waits_while_off)
{
    reset_dir();
    gamelists_video_size_pending = GAMELISTS_VIDEO_COVER;
    char text[32];
    /* Off, then Full screen: no file appears, the size waits for a delay. */
    ASSERT_TRUE(gamelists_set_video_size(GAMELISTS_VIDEO_FULLSCREEN));
    ASSERT_EQ(access(GAMELISTS_VIDEO_PREVIEW_PATH, F_OK), -1);
    ASSERT_EQ(gamelists_video_size(), GAMELISTS_VIDEO_FULLSCREEN);
    ASSERT_TRUE(gamelists_store_video_preview(1000));
    read_file(GAMELISTS_VIDEO_PREVIEW_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "1000,fullscreen\n");
    /* Off and on again keeps Full screen. */
    ASSERT_TRUE(gamelists_store_video_preview(GAMELISTS_VIDEO_OFF));
    ASSERT_EQ(access(GAMELISTS_VIDEO_PREVIEW_PATH, F_OK), -1);
    ASSERT_EQ(gamelists_video_size(), GAMELISTS_VIDEO_FULLSCREEN);
    ASSERT_TRUE(gamelists_store_video_preview(250));
    read_file(GAMELISTS_VIDEO_PREVIEW_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "250,fullscreen\n");
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

static void install_app(const char *name)
{
    char command[256];
    snprintf(command, sizeof(command), "mkdir -p " TEST_DIR "/sd/App/%s && touch " TEST_DIR "/sd/App/%s/launch.sh", name, name);
    (void)!system(command);
}

TEST(search_without_app_shows_off)
{
    reset_dir();
    install_app("Tweaks");
    bool menu[MAINMENU_MENU_COUNT], shown[MAINMENU_CONTEXT_COUNT], stored[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_shown_states(menu, shown);
    ASSERT_FALSE(shown[2]); /* Search: no App/Search/launch.sh */
    ASSERT_TRUE(shown[1]);  /* Refresh all roms */
    ASSERT_TRUE(shown[10]); /* Tweaks */
    mainmenu_load_states(menu, stored);
    ASSERT_TRUE(stored[2]); /* the default itself is unchanged */

    install_app("Search");
    mainmenu_load_shown_states(menu, shown);
    ASSERT_TRUE(shown[2]);
}

TEST(search_named_in_file_without_app_shows_off)
{
    reset_dir();
    gamelists_write_text(TEST_DIR "/main-menu.json", "{\"context\": {\"search\": true}}");
    bool menu[MAINMENU_MENU_COUNT], shown[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_shown_states(menu, shown);
    ASSERT_FALSE(shown[2]);
    install_app("Search");
    mainmenu_load_shown_states(menu, shown);
    ASSERT_TRUE(shown[2]);
}

TEST(other_change_keeps_search_for_later_install)
{
    reset_dir();
    ListItem item = {.item_type = TOGGLE, .value = 1, .action_id = 0}; /* Shutdown */
    mainmenu_action_context_item(&item);
    ASSERT_EQ(item.value, 1);

    char text[2048];
    read_file(TEST_DIR "/main-menu.json", text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"search\":\ttrue") != NULL);
    install_app("Search");
    bool menu[MAINMENU_MENU_COUNT], shown[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_shown_states(menu, shown);
    ASSERT_TRUE(shown[2]);
}

TEST(search_toggle_without_app_stays_off)
{
    reset_dir();
    ListItem item = {.item_type = TOGGLE, .value = 1, .action_id = 2};
    mainmenu_action_context_item(&item);
    ASSERT_EQ(item.value, 0);
}

TEST(custom_action_without_definition_shows_off)
{
    reset_dir();
    gamelists_write_text(TEST_DIR "/main-menu.json",
                         "{\"context\": {\"custom1\": true, \"custom2\": true},"
                         " \"custom\": {\"custom2\": {\"label\": \"Wi-Fi\","
                         " \"launch\": \"/mnt/SDCARD/App/wifi.sh\"}}}");
    bool menu[MAINMENU_MENU_COUNT], shown[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_shown_states(menu, shown);
    ASSERT_FALSE(shown[11]); /* custom1: enabled, not defined */
    ASSERT_TRUE(shown[12]);  /* custom2: enabled and defined */
    ASSERT_FALSE(shown[13]); /* custom3: neither */

    ListItem item = {.item_type = TOGGLE, .value = 1, .action_id = 13};
    mainmenu_action_context_item(&item);
    ASSERT_EQ(item.value, 0);
}

TEST(custom_action_needs_label_and_launch)
{
    reset_dir();
    gamelists_write_text(TEST_DIR "/main-menu.json",
                         "{\"context\": {\"custom1\": true},"
                         " \"custom\": {\"custom1\": {\"label\": \"\", \"launch\": \"x.sh\"}}}");
    bool menu[MAINMENU_MENU_COUNT], shown[MAINMENU_CONTEXT_COUNT];
    mainmenu_load_shown_states(menu, shown);
    ASSERT_FALSE(shown[11]);
}

/* ---- Game Info auto scroll ---- */

TEST(info_scroll_off_without_file)
{
    reset_dir();
    int delay, speed;
    gamelists_load_info_scroll(&delay, &speed);
    ASSERT_EQ(delay, GAMELISTS_INFO_SCROLL_OFF);
    ASSERT_EQ(gamelists_value_info_scroll_delay(), 0);
    char label[64];
    gamelists_label_info_scroll_delay(0, label, sizeof(label));
    ASSERT_STREQ(label, "Off (default)");
}

TEST(info_scroll_empty_file_is_2000ms_20px)
{
    reset_dir();
    touch(GAMELISTS_INFO_SCROLL_PATH);
    int delay, speed;
    gamelists_load_info_scroll(&delay, &speed);
    ASSERT_EQ(delay, 2000);
    ASSERT_EQ(speed, 20);
}

TEST(info_scroll_written_and_read)
{
    reset_dir();
    ASSERT_TRUE(gamelists_store_info_scroll(2000, 30));
    char text[64];
    read_file(GAMELISTS_INFO_SCROLL_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "2000,30\n");
    int delay, speed;
    gamelists_load_info_scroll(&delay, &speed);
    ASSERT_EQ(delay, 2000);
    ASSERT_EQ(speed, 30);
    gamelists_write_text(GAMELISTS_INFO_SCROLL_PATH, "99999,500\n");
    gamelists_load_info_scroll(&delay, &speed);
    ASSERT_EQ(delay, 30000);
    ASSERT_EQ(speed, 150);
    gamelists_write_text(GAMELISTS_INFO_SCROLL_PATH, "800\n");
    gamelists_load_info_scroll(&delay, &speed);
    ASSERT_EQ(delay, 800);
    ASSERT_EQ(speed, 20);
}

TEST(info_scroll_off_removes_the_file_and_keeps_the_speed)
{
    reset_dir();
    gamelists_store_info_scroll(1000, 75);
    ASSERT_TRUE(gamelists_store_info_scroll(GAMELISTS_INFO_SCROLL_OFF, 75));
    ASSERT_TRUE(access(GAMELISTS_INFO_SCROLL_PATH, F_OK) != 0);
    int delay, speed;
    gamelists_load_info_scroll(&delay, &speed);
    ASSERT_EQ(delay, GAMELISTS_INFO_SCROLL_OFF);
    ASSERT_EQ(speed, 75);
    gamelists_store_info_scroll(500, speed);
    char text[64];
    read_file(GAMELISTS_INFO_SCROLL_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "500,75\n");
}

/* ---- Console view ---- */

TEST(console_view_missing_file_is_grid)
{
    reset_dir();
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_GAMES), CONSOLEVIEW_GRID);
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_EXPERT), CONSOLEVIEW_GRID);
}

TEST(console_view_written_and_read)
{
    reset_dir();
    ASSERT_TRUE(consoleview_store_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_CAROUSEL));
    ASSERT_TRUE(consoleview_store_layout(CONSOLEVIEW_EXPERT, CONSOLEVIEW_LIST));
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_GAMES), CONSOLEVIEW_CAROUSEL);
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_EXPERT), CONSOLEVIEW_LIST);
    char text[256];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"games\":\t\"carousel\"") != NULL);
    ASSERT_TRUE(strstr(text, "\"expert\":\t\"list\"") != NULL);
}

TEST(console_view_keeps_the_other_keys)
{
    reset_dir();
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH,
                         "{\n  // mine\n  \"thumbs\": \"/mnt/SDCARD/miyoo/app/consoles\",\n"
                         "  \"count\": false,\n  \"carousel\": {\"labels\": true},\n}\n");
    ASSERT_TRUE(consoleview_store_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_CAROUSEL));
    char text[512];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "/mnt/SDCARD/miyoo/app/consoles") != NULL);
    ASSERT_TRUE(strstr(text, "\"count\":\tfalse") != NULL);
    ASSERT_TRUE(strstr(text, "\"labels\":\ttrue") != NULL);
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_GAMES), CONSOLEVIEW_CAROUSEL);
    /* Back to the grid keeps the file, as it has other keys. */
    ASSERT_TRUE(consoleview_store_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_GRID));
    ASSERT_TRUE(access(CONSOLEVIEW_CONFIG_PATH, F_OK) == 0);
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_GAMES), CONSOLEVIEW_GRID);
}

TEST(console_view_grid_for_both_removes_the_file)
{
    reset_dir();
    consoleview_store_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_LIST);
    consoleview_store_layout(CONSOLEVIEW_EXPERT, CONSOLEVIEW_CAROUSEL);
    consoleview_store_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_GRID);
    ASSERT_TRUE(access(CONSOLEVIEW_CONFIG_PATH, F_OK) == 0);
    consoleview_store_layout(CONSOLEVIEW_EXPERT, CONSOLEVIEW_GRID);
    ASSERT_TRUE(access(CONSOLEVIEW_CONFIG_PATH, F_OK) != 0);
}

TEST(console_view_damaged_file_is_left_alone)
{
    reset_dir();
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"games\": \"list\"");
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_GAMES), CONSOLEVIEW_GRID);
    ASSERT_FALSE(consoleview_store_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_CAROUSEL));
    char text[64];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "{\"games\": \"list\"");
}

TEST(console_view_unknown_value_is_grid)
{
    reset_dir();
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"games\": \"Carousel\", \"expert\": 1}");
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_GAMES), CONSOLEVIEW_GRID);
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_EXPERT), CONSOLEVIEW_GRID);
}

TEST(console_pictures_on_writes_the_folder_and_creates_it)
{
    reset_dir();
    ASSERT_FALSE(consoleview_has_thumbs());
    ASSERT_TRUE(consoleview_store_thumbs(true));
    ASSERT_TRUE(consoleview_has_thumbs());
    char text[256];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"thumbs\":\t\"" CONSOLEVIEW_THUMBS_DIR "\"") != NULL);
    struct stat st;
    ASSERT_TRUE(stat(CONSOLEVIEW_THUMBS_DIR, &st) == 0 && S_ISDIR(st.st_mode));
    /* The views keep it. */
    ASSERT_TRUE(consoleview_store_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_CAROUSEL));
    ASSERT_TRUE(consoleview_has_thumbs());
}

TEST(console_pictures_keep_a_folder_already_named)
{
    reset_dir();
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"thumbs\": \"/mnt/SDCARD/Mine\"}");
    ASSERT_TRUE(consoleview_has_thumbs());
    ASSERT_TRUE(consoleview_store_thumbs(true));
    char text[256];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "/mnt/SDCARD/Mine") != NULL);
}

TEST(console_pictures_off_removes_the_key)
{
    reset_dir();
    consoleview_store_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_LIST);
    consoleview_store_thumbs(true);
    ASSERT_TRUE(consoleview_store_thumbs(false));
    ASSERT_FALSE(consoleview_has_thumbs());
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_GAMES), CONSOLEVIEW_LIST);
    /* Grid and no pictures is stock: no file. */
    consoleview_store_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_GRID);
    ASSERT_TRUE(access(CONSOLEVIEW_CONFIG_PATH, F_OK) != 0);
}

/* list_addItem() drops items past the capacity a menu is created with, so
 * every Tweaks menu built with a fixed number must have room for all the
 * items it may add. Each list_addItem up to the menu's push is counted,
 * also the ones behind a device check: the bound holds on every model. */
static int menus_overflowing(const char *path, int *menus)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return 1;
    static char src[1 << 20];
    size_t n = fread(src, 1, sizeof(src) - 1, fp);
    fclose(fp);
    src[n] = '\0';

    int overflows = 0;
    for (char *p = src; (p = strstr(p, "list_create")) != NULL; p++) {
        char *open = p + strlen("list_create");
        if (strncmp(open, "WithTitle", 9) == 0)
            open += 9;
        else if (strncmp(open, "WithSticky", 10) == 0)
            open += 10;
        if (*open != '(' || !isdigit((unsigned char)open[1]))
            continue; /* not a call with a number */
        char *after = NULL;
        int capacity = (int)strtol(open + 1, &after, 10);
        while (*after == ' ')
            after++;
        if (*after != ',')
            continue; /* a computed size: 1 + n */
        char *end = strstr(open, "menu_stack[++menu_level]");
        char *main_end = strstr(open, "menu_stack[0]");
        if (end == NULL || (main_end != NULL && main_end < end))
            end = main_end;
        if (end == NULL)
            continue;
        int items = 0;
        for (char *q = open; (q = strstr(q, "list_addItem")) != NULL && q < end; q++) {
            char *line = q;
            while (line > src && line[-1] != '\n')
                line--;
            while (*line == ' ')
                line++;
            if (strncmp(line, "//", 2) != 0) /* not commented out */
                items++;
        }
        (*menus)++;
        if (items > capacity) {
            fprintf(stderr, "    %s: a menu made for %d adds %d items\n", path, capacity, items);
            overflows++;
        }
    }
    return overflows;
}

/* The main menu's second lines are drawn in the theme's list font and are
 * not scrolled: 34 characters fit with the wider theme fonts (a tester's
 * theme cut a 40-character one). */
TEST(main_menu_descriptions_fit)
{
    FILE *fp = fopen("../src/tweaks/menus.h", "r");
    ASSERT_TRUE(fp != NULL);
    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        const char *d = strstr(line, ".description = \"");
        if (d == NULL)
            continue;
        d += strlen(".description = \"");
        const char *end = strchr(d, '"');
        ASSERT_TRUE(end != NULL);
        if ((int)(end - d) > 34)
            fprintf(stderr, "    too long for a theme: %.*s\n", (int)(end - d), d);
        ASSERT_TRUE(end - d <= 34);
        found++;
    }
    fclose(fp);
    ASSERT_TRUE(found >= 8);
}

TEST(every_tweaks_menu_has_room_for_its_items)
{
    const char *files[] = {"../src/tweaks/menus.h", "../src/tweaks/main_menu.h",
                           "../src/tweaks/game_lists.h", "../src/tweaks/network.h"};
    int menus = 0, overflows = 0;
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
        overflows += menus_overflowing(files[i], &menus);
    ASSERT_EQ(overflows, 0);
    ASSERT_TRUE(menus >= 30); /* the scan found the menus */
}

TEST(console_view_options_written_and_read)
{
    reset_dir();
    for (int i = 0; i < CONSOLEVIEW_OPTION_COUNT; i++)
        ASSERT_EQ(consoleview_option(i), 0);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_ANIMATION, 2));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_OPACITY, 2));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_SELECTION, 1));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_COUNT, 1));
    char text[512];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"animation\":\t\"slide\"") != NULL);
    ASSERT_TRUE(strstr(text, "\"opacity_unselected\":\t0.5") != NULL);
    ASSERT_TRUE(strstr(text, "\"selection\":\tfalse") != NULL);
    ASSERT_TRUE(strstr(text, "\"count\":\tfalse") != NULL);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_ANIMATION), 2);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_OPACITY), 2);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_SELECTION), 1);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_COUNT), 1);
    /* "None" is written as such: Open MainUI's default changes later. */
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_ANIMATION, 1));
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"animation\":\t\"none\"") != NULL);
}

/* Open MainUI 1.1.0 rc9: easing and duration_ms in the carousel section. */
TEST(console_view_motion_and_slide_time)
{
    reset_dir();
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_EASING, 1));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_DURATION, 5));
    char text[512];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"easing\":\t\"smooth\"") != NULL);
    ASSERT_TRUE(strstr(text, "\"duration_ms\":\t300") != NULL);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_EASING), 1);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_DURATION), 5);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_DURATION, 1));
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"duration_ms\":\t100") != NULL);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_DURATION, 4));
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"duration_ms\":\t240") != NULL);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_DURATION, 6));
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_DURATION), 6);
    ASSERT_FALSE(consoleview_store_option(CONSOLEVIEW_DURATION, 7));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_REPEAT, 2));
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"repeat_ms\":\t200") != NULL);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_REPEAT), 2);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_REPEAT, 0));
    /* Rob's suggested values read back; a time not offered shows as
     * Default and is kept. */
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"carousel\": {\"duration_ms\": 300, \"repeat_ms\": 200}}");
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_DURATION), 5);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_REPEAT), 2);
    /* rc10's suggestions: 180 ms, repeat 100. */
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"carousel\": {\"duration_ms\": 180, \"repeat_ms\": 100}}");
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_DURATION), 3);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_REPEAT), 1);
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"carousel\": {\"duration_ms\": 250, \"repeat_ms\": 200}}");
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_DURATION), 0);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_EASING, 1));
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"duration_ms\":\t250") != NULL);
    ASSERT_TRUE(strstr(text, "\"repeat_ms\":\t200") != NULL);
    /* Both back to Default: the keys go, and the file with them. */
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_EASING, 0));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_DURATION, 0));
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "easing") == NULL);
    ASSERT_TRUE(strstr(text, "\"duration_ms\"") == NULL);
}

/* The rest of the rc8-rc10 keys: sizes, spacing, offset (negative too),
 * padding, VSync, grid navigation, list folder icons, console menu. */
TEST(console_view_layout_keys)
{
    reset_dir();
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_SCALE, 4));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_GAP, 3));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_OFFSET, 1));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_PADDING, 1));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_VSYNC, 1));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_GRID_NAV, 1));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_FOLDER_ICON, 1));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_MENU, 1));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_OPACITY, 3));
    char text[1024];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"scale_unselected\":\t0.6") != NULL);
    ASSERT_TRUE(strstr(text, "\"gap\":\t32") != NULL);
    ASSERT_TRUE(strstr(text, "\"offset_y\":\t-40") != NULL);
    ASSERT_TRUE(strstr(text, "\"selection_padding\":\t0") != NULL);
    ASSERT_TRUE(strstr(text, "\"vsync\":\tfalse") != NULL);
    ASSERT_TRUE(strstr(text, "\"navigation\":\t\"stock\"") != NULL);
    ASSERT_TRUE(strstr(text, "\"folder_icon\":\tfalse") != NULL);
    ASSERT_TRUE(strstr(text, "\"menu\":\t[\"refresh_all\"]") != NULL);
    ASSERT_TRUE(strstr(text, "\"opacity_unselected\":\t0.3") != NULL);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_SCALE), 4);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_GAP), 3);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_OFFSET), 1);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_PADDING), 1);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_VSYNC), 1);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_GRID_NAV), 1);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_FOLDER_ICON), 1);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_MENU), 1);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_OPACITY), 3);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_MENU, 3));
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_MENU), 3);
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"menu\":\t[]") != NULL);
    /* Another list reads as Default and is kept until changed. */
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"menu\": [\"refresh\", \"refresh_all\"]}");
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_MENU), 0);
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"menu\": [\"refresh\"]}");
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_MENU), 2);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_MENU, 0));
    ASSERT_TRUE(access(CONSOLEVIEW_CONFIG_PATH, F_OK) != 0);
}

static ListItem *menu_item(List *list, const char *label)
{
    for (int i = 0; i < list->item_count; i++) {
        if (strcmp(list->items[i].label, label) == 0)
            return &list->items[i];
    }
    return NULL;
}

static ListItem *console_view_item(const char *label)
{
    return menu_item(&_menu_console_view, label);
}

static bool console_view_greyed(const char *label)
{
    ListItem *item = console_view_item(label);
    return item != NULL && item->disabled;
}

static bool home_screen_greyed(const char *label)
{
    ListItem *item = menu_item(&_menu_home_screen, label);
    return item != NULL && item->disabled;
}

static const char *const console_view_checked_labels[] = {
    "Games tab view", "Expert tab view", "Remember last console", "Game count",
    "Console page menu", "Wrap around", "Grid navigation", "Console pictures",
    "List: folder icons", "Carousel look...", "Carousel movement..."};
static const char *const home_screen_checked_labels[] = {
    "Tabs...", "Console page...", "SELECT menu..."};

static void console_view_set_layout(int screen, int layout)
{
    ListItem *item = console_view_item(screen == CONSOLEVIEW_GAMES ? "Games tab view" : "Expert tab view");
    ASSERT_TRUE(item != NULL);
    item->value = layout;
    item->action(item);
}

static void console_view_menus_free(void)
{
    list_free(&_menu_console_view);
    _menu_console_view._created = false;
    list_free(&_menu_home_screen);
    _menu_home_screen._created = false;
    menu_level = 0;
}

static void console_view_menus_build(void)
{
    console_view_menus_free();
    menu_homeScreen(NULL);
    menu_consolePage(NULL);
}

TEST(console_view_option_views)
{
    for (int option = 0; option < CONSOLEVIEW_OPTION_COUNT; option++) {
        int expected;
        switch (option) {
        case CONSOLEVIEW_COUNT:
        case CONSOLEVIEW_MENU:
            expected = CONSOLEVIEW_FOR_ALL;
            break;
        case CONSOLEVIEW_WRAP:
            expected = CONSOLEVIEW_FOR_CAROUSEL | CONSOLEVIEW_FOR_GRID;
            break;
        case CONSOLEVIEW_GRID_NAV:
            expected = CONSOLEVIEW_FOR_GRID;
            break;
        case CONSOLEVIEW_FOLDER_ICON:
            expected = CONSOLEVIEW_FOR_LIST;
            break;
        default: /* the carousel's look and movement */
            expected = CONSOLEVIEW_FOR_CAROUSEL;
            break;
        }
        ASSERT_EQ(consoleview_option_views(option), expected);
    }
}

TEST(console_view_greys_out_unused_options)
{
    reset_dir();
    console_view_menus_build();
    for (size_t i = 0; i < sizeof(console_view_checked_labels) / sizeof(console_view_checked_labels[0]); i++)
        ASSERT_TRUE(console_view_item(console_view_checked_labels[i]) != NULL);
    for (size_t i = 0; i < sizeof(home_screen_checked_labels) / sizeof(home_screen_checked_labels[0]); i++)
        ASSERT_TRUE(menu_item(&_menu_home_screen, home_screen_checked_labels[i]) != NULL);

    /* No file: Games and Expert are grids. */
    ASSERT_TRUE(console_view_greyed("Carousel look..."));
    ASSERT_TRUE(console_view_greyed("Carousel movement..."));
    ASSERT_FALSE(home_screen_greyed("Tabs..."));
    ASSERT_FALSE(home_screen_greyed("Console page..."));
    ASSERT_FALSE(home_screen_greyed("SELECT menu..."));
    ASSERT_TRUE(console_view_greyed("Console pictures"));
    ASSERT_TRUE(console_view_greyed("List: folder icons"));
    ASSERT_FALSE(console_view_greyed("Wrap around"));
    ASSERT_FALSE(console_view_greyed("Grid navigation"));
    ASSERT_FALSE(console_view_greyed("Game count"));
    ASSERT_FALSE(console_view_greyed("Console page menu"));
    ASSERT_FALSE(console_view_greyed("Remember last console"));
    ASSERT_FALSE(console_view_greyed("Games tab view"));
    ASSERT_FALSE(console_view_greyed("Expert tab view"));

    /* Games as a list, Expert still a grid. */
    console_view_set_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_LIST);
    ASSERT_TRUE(console_view_greyed("Carousel look..."));
    ASSERT_FALSE(console_view_greyed("Console pictures"));
    ASSERT_FALSE(console_view_greyed("List: folder icons"));
    ASSERT_FALSE(console_view_greyed("Grid navigation"));
    ASSERT_FALSE(console_view_greyed("Wrap around"));

    /* Console page: what always applies first, the view options last. */
    for (size_t i = 0; i < sizeof(console_view_checked_labels) / sizeof(console_view_checked_labels[0]); i++)
        ASSERT_STREQ(_menu_console_view.items[i].label, console_view_checked_labels[i]);

    /* Expert as a carousel: no grid left, the carousel menus open. */
    console_view_set_layout(CONSOLEVIEW_EXPERT, CONSOLEVIEW_CAROUSEL);
    ASSERT_FALSE(console_view_greyed("Carousel look..."));
    ASSERT_FALSE(console_view_greyed("Carousel movement..."));
    ASSERT_FALSE(console_view_greyed("Wrap around"));
    ASSERT_TRUE(console_view_greyed("Grid navigation"));
    ASSERT_FALSE(console_view_greyed("List: folder icons"));

    /* Both lists: wrap around only moves the carousel and the grid. */
    console_view_set_layout(CONSOLEVIEW_EXPERT, CONSOLEVIEW_LIST);
    ASSERT_TRUE(console_view_greyed("Wrap around"));
    ASSERT_TRUE(console_view_greyed("Carousel movement..."));
    ASSERT_FALSE(console_view_greyed("Console pictures"));

    /* Built again, the menus grey out the same; a carousel option keeps
       its value while its menu is greyed out. */
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_GAP, 3));
    console_view_menus_build();
    ASSERT_TRUE(console_view_greyed("Carousel look..."));
    ASSERT_FALSE(console_view_greyed("List: folder icons"));
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_GAP), 3);

    console_view_menus_free();
}

/* Game count, Wrap around, List: folder icons, Console names and VSync are
 * switches: on is the option's default (no key), off writes false. */
TEST(console_view_on_off_switches)
{
    reset_dir();
    console_view_menus_build();
    menu_level = 0;
    list_free(&_menu_carousel_look);
    list_free(&_menu_carousel_movement);
    _menu_carousel_look._created = _menu_carousel_movement._created = false;
    menu_carouselLook(NULL);
    menu_level = 0;
    menu_carouselMovement(NULL);
    menu_level = 0;

    struct { List *list; const char *label; int option; const char *key; } sw[] = {
        {&_menu_console_view, "Game count", CONSOLEVIEW_COUNT, "\"count\":\tfalse"},
        {&_menu_console_view, "Wrap around", CONSOLEVIEW_WRAP, "\"wrap\":\tfalse"},
        {&_menu_console_view, "List: folder icons", CONSOLEVIEW_FOLDER_ICON, "\"folder_icon\":\tfalse"},
        {&_menu_carousel_look, "Console names", CONSOLEVIEW_LABELS, "\"labels\":\tfalse"},
        {&_menu_carousel_movement, "VSync", CONSOLEVIEW_VSYNC, "\"vsync\":\tfalse"},
    };
    char text[512];
    for (size_t i = 0; i < sizeof(sw) / sizeof(sw[0]); i++) {
        ListItem *item = menu_item(sw[i].list, sw[i].label);
        ASSERT_TRUE(item != NULL);
        ASSERT_EQ(item->item_type, TOGGLE);
        ASSERT_EQ(item->value, 1); /* no file: on */
        item->value = 0;
        item->action(item);
        ASSERT_EQ(consoleview_option(sw[i].option), 1);
        read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
        ASSERT_TRUE(strstr(text, sw[i].key) != NULL);
        item->value = 1;
        item->action(item);
        ASSERT_EQ(consoleview_option(sw[i].option), 0);
        read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
        ASSERT_TRUE(strstr(text, sw[i].key) == NULL);
    }
    ASSERT_TRUE(access(CONSOLEVIEW_CONFIG_PATH, F_OK) != 0);

    list_free(&_menu_carousel_look);
    list_free(&_menu_carousel_movement);
    _menu_carousel_look._created = _menu_carousel_movement._created = false;
    console_view_menus_free();
}

/* Video preview size, the Game Info text speed and the long-name style and
 * speed are greyed out while what they depend on is Off. */
static ListItem *gamelists_item(List *list, const char *label)
{
    return menu_item(list, label);
}

static void gamelists_set(List *list, const char *label, int value)
{
    ListItem *item = gamelists_item(list, label);
    ASSERT_TRUE(item != NULL);
    item->value = value;
    item->action(item);
}

TEST(game_lists_options_follow_what_they_depend_on)
{
    reset_dir();
    list_free(&_menu_game_lists_scrolling);
    list_free(&_menu_game_lists_previews);
    _menu_game_lists_scrolling._created = _menu_game_lists_previews._created = false;
    menu_level = 0;
    menu_gameListsScrolling(NULL);
    menu_level = 0;
    menu_gameListsPreviews(NULL);
    menu_level = 0;
    List *sc = &_menu_game_lists_scrolling, *pv = &_menu_game_lists_previews;

    /* The switching option comes first, so the menu opens on it. */
    ASSERT_STREQ(sc->items[0].label, "Long names: start after");
    ASSERT_STREQ(pv->items[0].label, "Video preview");

    gamelists_set(pv, "Video preview", 0);
    ASSERT_TRUE(gamelists_item(pv, "Video preview size")->disabled);
    gamelists_set(pv, "Video preview", 5);
    ASSERT_FALSE(gamelists_item(pv, "Video preview size")->disabled);

    gamelists_set(pv, "Game Info text: auto scroll", 0);
    ASSERT_TRUE(gamelists_item(pv, "Game Info text: speed")->disabled);
    gamelists_set(pv, "Game Info text: auto scroll", 2);
    ASSERT_FALSE(gamelists_item(pv, "Game Info text: speed")->disabled);
    ASSERT_FALSE(gamelists_item(pv, "Video preview size")->disabled);

    gamelists_set(sc, "Long names: start after", 0);
    ASSERT_TRUE(gamelists_item(sc, "Long names: style")->disabled);
    ASSERT_TRUE(gamelists_item(sc, "Long names: speed")->disabled);
    ASSERT_FALSE(gamelists_item(sc, "Held button: delay")->disabled);
    ASSERT_FALSE(gamelists_item(sc, "Held button: repeat")->disabled);
    gamelists_set(sc, "Long names: start after", 3);
    ASSERT_FALSE(gamelists_item(sc, "Long names: style")->disabled);
    ASSERT_FALSE(gamelists_item(sc, "Long names: speed")->disabled);

    /* Built again with scrolling Off: greyed out from the start. */
    gamelists_set(sc, "Long names: start after", 0);
    list_free(sc);
    sc->_created = false;
    menu_level = 0;
    menu_gameListsScrolling(NULL);
    menu_level = 0;
    ASSERT_TRUE(gamelists_item(sc, "Long names: speed")->disabled);
    ASSERT_FALSE(sc->items[sc->active_pos].disabled);

    list_free(sc);
    list_free(pv);
    sc->_created = pv->_created = false;
}

/* Slide motion and Slide time need a slide, Selection box padding a box,
 * and LEFT opens Saved Games needs Saved Games in the Game Info menu. */
static void carousel_menus_build(void)
{
    list_free(&_menu_carousel_look);
    list_free(&_menu_carousel_movement);
    _menu_carousel_look._created = _menu_carousel_movement._created = false;
    menu_level = 0;
    menu_carouselLook(NULL);
    menu_level = 0;
    menu_carouselMovement(NULL);
    menu_level = 0;
}

TEST(options_follow_the_option_they_need)
{
    reset_dir();
    ASSERT_TRUE(consoleview_store_layout(CONSOLEVIEW_GAMES, CONSOLEVIEW_CAROUSEL));
    console_view_menus_build();
    carousel_menus_build();
    List *look = &_menu_carousel_look, *move = &_menu_carousel_movement;

    ASSERT_FALSE(menu_item(move, "Slide motion")->disabled);
    ASSERT_FALSE(menu_item(move, "Slide time")->disabled);
    ListItem *anim = menu_item(move, "Animation");
    anim->value = 1; /* None */
    anim->action(anim);
    ASSERT_TRUE(menu_item(move, "Slide motion")->disabled);
    ASSERT_TRUE(menu_item(move, "Slide time")->disabled);
    ASSERT_FALSE(menu_item(move, "Holding: time per move")->disabled);
    ASSERT_FALSE(menu_item(move, "VSync")->disabled);
    anim->value = 2; /* Slide */
    anim->action(anim);
    ASSERT_FALSE(menu_item(move, "Slide time")->disabled);

    ListItem *box = menu_item(look, "Selection box");
    box->value = 1; /* Off */
    box->action(box);
    ASSERT_TRUE(menu_item(look, "Selection box padding")->disabled);
    ASSERT_FALSE(menu_item(look, "Space between consoles")->disabled);
    /* Built again: greyed out from the start. */
    carousel_menus_build();
    ASSERT_TRUE(menu_item(&_menu_carousel_look, "Selection box padding")->disabled);
    box = menu_item(&_menu_carousel_look, "Selection box");
    box->value = 0; /* Theme */
    box->action(box);
    ASSERT_FALSE(menu_item(&_menu_carousel_look, "Selection box padding")->disabled);

    /* LEFT opens Saved Games and the Game Info menu's Saved Games. */
    list_free(&_menu_game_info_menu);
    list_free(&_menu_saved_games);
    _menu_game_info_menu._created = _menu_saved_games._created = false;
    menu_level = 0;
    menu_gameInfoMenu(NULL);
    menu_level = 0;
    menu_savedGames(NULL);
    menu_level = 0;
    ListItem *left = menu_item(&_menu_saved_games, "LEFT opens Saved Games");
    ASSERT_FALSE(left->disabled);
    ListItem *sg = menu_item(&_menu_game_info_menu, "Saved Games");
    sg->value = 0;
    sg->action(sg);
    ASSERT_TRUE(left->disabled);
    ASSERT_FALSE(_menu_saved_games.items[_menu_saved_games.active_pos].disabled);
    sg->value = 1;
    sg->action(sg);
    ASSERT_FALSE(left->disabled);

    /* The whole Saved Games menu in Game lists follows it too. */
    list_free(&_menu_game_lists);
    _menu_game_lists = list_createWithTitle(2, LIST_SMALL, "Game lists");
    list_addItem(&_menu_game_lists, (ListItem){.label = "Game Info menu...", .action = menu_gameInfoMenu});
    list_addItem(&_menu_game_lists, (ListItem){.label = "Saved Games...", .action = menu_savedGames});
    sg->value = 0;
    sg->action(sg);
    ASSERT_TRUE(_menu_game_lists.items[1].disabled);
    ASSERT_FALSE(_menu_game_lists.items[0].disabled);
    sg->value = 1;
    sg->action(sg);
    ASSERT_FALSE(_menu_game_lists.items[1].disabled);
    list_free(&_menu_game_lists);
    _menu_game_lists._created = false;

    list_free(&_menu_carousel_look);
    list_free(&_menu_carousel_movement);
    list_free(&_menu_game_info_menu);
    list_free(&_menu_saved_games);
    _menu_carousel_look._created = _menu_carousel_movement._created = false;
    _menu_game_info_menu._created = _menu_saved_games._created = false;
    console_view_menus_free();
}

/* The menus these tests can't build: the greying is in their source. */
static bool source_has(const char *path, const char *text)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return false;
    static char src[1 << 20];
    size_t n = fread(src, 1, sizeof(src) - 1, fp);
    fclose(fp);
    src[n] = '\0';
    return strstr(src, text) != NULL;
}

TEST(greyed_out_options_in_other_menus)
{
    /* VNC's Framerate while VNC is off (toggling it rebuilds the menus). */
    ASSERT_TRUE(source_has("../src/tweaks/network.h",
                           ".label = \"Framerate\",\n"
                           "                                     .item_type = MULTIVALUE,\n"
                           "                                     .disabled = !network_state.vncserv,"));
    /* The blue light times while the schedule is off (same). */
    ASSERT_TRUE(source_has("../src/tweaks/menus.h",
                           ".label = \"Time (On)\",\n"
                           "                                         .disabled = !settings.blue_light_schedule,"));
    ASSERT_TRUE(source_has("../src/tweaks/menus.h",
                           ".label = \"Time (Off)\",\n"
                           "                                         .disabled = !settings.blue_light_schedule,"));
    /* Battery percentage: the rest while Visible is Off, at once. */
    ASSERT_TRUE(source_has("../src/tweaks/menus.h", ".action = battery_action_visible}"));
    ASSERT_TRUE(source_has("../src/tweaks/menus.h", "_menu_battery_percentage.items[i].disabled = off;"));
    /* "-" follows the theme: off when the theme hides it. */
    ASSERT_TRUE(source_has("../src/tweaks/menus.h",
                           "visible == 1 || (visible == 0 && !resources.theme_back.batteryPercentage.visible)"));
    /* Game lists greys out Saved Games... from the start too. */
    ASSERT_TRUE(source_has("../src/tweaks/menus.h",
                           "        savedgames_update_enabled();\n    }\n    menu_stack[++menu_level] = &_menu_game_lists;"));
}

/* The Game Info menu and Saved Games each get their own toggles. */
TEST(game_info_and_saved_games_menus)
{
    reset_dir();
    list_free(&_menu_game_menus);
    list_free(&_menu_game_info_menu);
    list_free(&_menu_saved_games);
    _menu_game_menus._created = _menu_game_info_menu._created = _menu_saved_games._created = false;
    menu_level = 0;
    menu_listMenus(NULL);
    menu_level = 0;
    menu_gameInfoMenu(NULL);
    menu_level = 0;
    menu_savedGames(NULL);
    menu_level = 0;

    /* List menus: one submenu per list, then GLO; no Game Info here. */
    const char *subs[] = {"Games...", "Favorites...", "Recents...", "Search...", "GLO (game options)..."};
    ASSERT_EQ(_menu_game_menus.item_count, 5);
    for (int i = 0; i < 5; i++)
        ASSERT_STREQ(_menu_game_menus.items[i].label, subs[i]);

    /* Each list's submenu has its own actions, short labels. */
    const char *titles[] = {"Games menu", "Favorites menu", "Recents menu", "Search menu"};
    const int counts[] = {5, 6, 3, 2};
    for (int i = 0; i < 4; i++) {
        menu_level = 0;
        _menu_game_menus.items[i].action(&_menu_game_menus.items[i]);
        ASSERT_TRUE(menu_stack[menu_level] == &_menu_list_section);
        ASSERT_STREQ(_menu_list_section.title, titles[i]);
        ASSERT_EQ(_menu_list_section.item_count, counts[i]);
    }
    menu_level = 0;
    _menu_game_menus.items[0].action(&_menu_game_menus.items[0]);
    ASSERT_STREQ(_menu_list_section.items[0].label, "Launch");
    ListItem *refresh = menu_item(&_menu_list_section, "Refresh");
    ASSERT_TRUE(refresh != NULL);
    /* Its toggle still writes game-menu.json. */
    refresh->value = 0;
    refresh->action(refresh);
    ASSERT_FALSE(gamemenu_shown(refresh->action_id));
    refresh->value = 1;
    refresh->action(refresh);
    ASSERT_TRUE(gamemenu_shown(refresh->action_id));
    menu_level = 0;
    _menu_game_menus.items[3].action(&_menu_game_menus.items[3]);
    ASSERT_TRUE(menu_item(&_menu_list_section, "Favorite") != NULL);
    ASSERT_TRUE(menu_item(&_menu_list_section, "Gallery") == NULL);
    menu_level = 0;
    list_free(&_menu_list_section);

    const char *info[] = {"Gallery", "Saved Games", "Game Maps", "Walkthrough", "Manual", "RIGHT opens the gallery"};
    ASSERT_EQ(_menu_game_info_menu.item_count, 6);
    for (int i = 0; i < 6; i++)
        ASSERT_STREQ(_menu_game_info_menu.items[i].label, info[i]);

    const char *saved[] = {"LEFT opens Saved Games", "Show the Auto Save", "Enlarge small pictures",
                           "Console screen shape", "Playing an older save", "Layout"};
    ASSERT_EQ(_menu_saved_games.item_count, 6);
    for (int i = 0; i < 6; i++)
        ASSERT_STREQ(_menu_saved_games.items[i].label, saved[i]);

    /* Their switches still write the same keys. */
    ListItem *left = menu_item(&_menu_saved_games, "LEFT opens Saved Games");
    left->value = 1;
    left->action(left);
    ASSERT_TRUE(gamemenu_switch(0));
    char text[256];
    read_file(GAMEMENU_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"left_shortcut\":\ttrue") != NULL);

    /* Layout (rc17): Overlay is the default, Framed is written. */
    ListItem *layout = menu_item(&_menu_saved_games, "Layout");
    ASSERT_EQ(layout->value, 0);
    ASSERT_STREQ(layout->value_labels[0], "Overlay (default)");
    layout->value = 1;
    layout->action(layout);
    ASSERT_EQ(gamemenu_layout(), 1);

    list_free(&_menu_game_menus);
    list_free(&_menu_game_info_menu);
    list_free(&_menu_saved_games);
    _menu_game_menus._created = _menu_game_info_menu._created = _menu_saved_games._created = false;
}

/* List menus > GLO (game options): each switch writes or removes
   config/.hideGlo<name>, which game_list_options.sh reads. */
static bool glo_flag_exists(const char *name)
{
    char path[256];
    snprintf(path, sizeof(path), "%s/.hideGlo%s", TEST_DIR, name);
    return access(path, F_OK) == 0;
}

static void glo_menu_rebuild(void)
{
    list_free(&_menu_glo);
    _menu_glo._created = false;
    menu_level = 0;
    menu_gloEntries(NULL);
    ASSERT_TRUE(menu_stack[menu_level] == &_menu_glo);
    menu_level = 0;
}

TEST(glo_entries_in_list_menus)
{
    reset_dir();
    test_glo_network = true;
    glo_menu_rebuild();

    const char *labels[] = {"Reset game", "Game manual", "Game core", "Hide game",
                            "Filter list", "Netplay", "Scraper"};
    ASSERT_EQ(GLO_ENTRY_COUNT, 7);
    ASSERT_STREQ(_menu_glo.title, "GLO (game options)");
    ASSERT_EQ(_menu_glo.item_count, 7);
    for (int i = 0; i < 7; i++) {
        ListItem *item = &_menu_glo.items[i];
        ASSERT_STREQ(item->label, labels[i]);
        ASSERT_EQ(item->value, 1);
    }

    /* Off writes the flag, On removes it; the names the script uses. */
    const char *flags[] = {"Reset", "Manual", "Core", "Hide", "Filter", "Netplay", "Scraper"};
    for (int i = 0; i < 7; i++) {
        ListItem *item = menu_item(&_menu_glo, labels[i]);
        item->value = 0;
        item->action(item);
        ASSERT_TRUE(glo_flag_exists(flags[i]));
    }
    glo_menu_rebuild();
    for (int i = 0; i < 7; i++)
        ASSERT_EQ(menu_item(&_menu_glo, labels[i])->value, 0);
    ListItem *filter = menu_item(&_menu_glo, "Filter list");
    filter->value = 1;
    filter->action(filter);
    ASSERT_FALSE(glo_flag_exists("Filter"));
    ASSERT_TRUE(glo_flag_exists("Reset"));

    /* No Wi-Fi: no Netplay or Scraper switch. */
    test_glo_network = false;
    glo_menu_rebuild();
    ASSERT_TRUE(menu_item(&_menu_glo, "Netplay") == NULL);
    ASSERT_TRUE(menu_item(&_menu_glo, "Scraper") == NULL);
    ASSERT_TRUE(menu_item(&_menu_glo, "Filter list") != NULL);
    test_glo_network = true;

    /* Game lists itself no longer has the old switch. */
    ASSERT_FALSE(source_has("../src/tweaks/menus.h", "Filter list in game options"));

    list_free(&_menu_glo);
    _menu_glo._created = false;
}

TEST(console_view_remember_flag)
{
    reset_dir();
    ASSERT_FALSE(consoleview_remembers());
    ASSERT_TRUE(consoleview_store_remember(true));
    ASSERT_TRUE(access(TEST_DIR "/.rememberConsole", F_OK) == 0);
    ASSERT_TRUE(consoleview_remembers());
    ASSERT_TRUE(consoleview_store_remember(false));
    ASSERT_FALSE(consoleview_remembers());
    ASSERT_TRUE(consoleview_store_remember(false));
    /* Same flag runtime.sh reads. */
    FILE *fp = fopen("../static/build/.tmp_update/runtime.sh", "r");
    if (fp != NULL) {
        static char buf[1 << 17];
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
        buf[n] = '\0';
        if (strstr(buf, "console_position_flag") != NULL)
            ASSERT_TRUE(strstr(buf, "console_position_flag=\"$sysdir/config/.rememberConsole\"") != NULL);
    }
}

TEST(console_view_selection_colours)
{
    reset_dir();
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_SELECTION, 2));
    char text[256];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"selection\":\t\"#FFFFFF1F\"") != NULL);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_SELECTION), 2);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_SELECTION, 3));
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_SELECTION), 3);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_SELECTION, 1));
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_SELECTION), 1);
}

#define GM_PATH TEST_DIR "/game-menu.json"

TEST(game_menus_default_without_file)
{
    reset_dir();
    for (int i = 0; i < GAMEMENU_ITEM_COUNT; i++)
        ASSERT_TRUE(gamemenu_shown(i));
}

TEST(game_menus_hide_and_show_again)
{
    reset_dir();
    /* Games: Delete (2) off. */
    ASSERT_TRUE(gamemenu_store(2, false));
    char text[1024];
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"roms\":\t[\"launch\", \"favorite\", \"search\", \"refresh\"]") != NULL);
    ASSERT_TRUE(strstr(text, "favorites") == NULL);
    ASSERT_FALSE(gamemenu_shown(2));
    ASSERT_TRUE(gamemenu_shown(3));
    /* Back on: in its place, the section is default again, file gone. */
    ASSERT_TRUE(gamemenu_store(2, true));
    ASSERT_TRUE(access(GM_PATH, F_OK) != 0);
    /* Search: both off makes an empty menu (hidden), not a missing one. */
    ASSERT_TRUE(gamemenu_store(14, false));
    ASSERT_TRUE(gamemenu_store(15, false));
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"search_results\":\t[]") != NULL);
    ASSERT_FALSE(gamemenu_shown(14));
    ASSERT_FALSE(gamemenu_shown(15));
    ASSERT_TRUE(gamemenu_shown(0));
}

TEST(game_menus_keep_a_hand_made_order)
{
    reset_dir();
    gamelists_write_text(GM_PATH, "{\"recents\": [\"clear_recents\", \"launch\", \"other\"]}");
    ASSERT_FALSE(gamemenu_shown(12)); /* remove_recent */
    ASSERT_TRUE(gamemenu_store(12, true));
    char text[512];
    read_file(GM_PATH, text, sizeof(text));
    /* After launch (before it by default), the unknown id kept. */
    ASSERT_TRUE(strstr(text, "[\"clear_recents\", \"launch\", \"remove_recent\", \"other\"]") != NULL);
    ASSERT_TRUE(gamemenu_store(13, false));
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "[\"launch\", \"remove_recent\", \"other\"]") != NULL);
}

/* rc12: the Game Info menu ("details"), where Game Info always stays. */
TEST(game_menus_game_info_menu)
{
    reset_dir();
    ASSERT_TRUE(gamemenu_items[16].fixed);
    ASSERT_TRUE(gamemenu_store(18, false)); /* Saved Games off */
    char text[1024];
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"details\":\t[\"game_info\", \"gallery\", \"maps\", \"walkthrough\", \"manual\"]") != NULL);
    ASSERT_FALSE(gamemenu_shown(18));
    for (int i = 17; i < GAMEMENU_ITEM_COUNT; i++)
        ASSERT_TRUE(gamemenu_store(i, false));
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"details\":\t[\"game_info\"]") != NULL);
    for (int i = 17; i < GAMEMENU_ITEM_COUNT; i++)
        ASSERT_TRUE(gamemenu_store(i, true));
    ASSERT_TRUE(access(GM_PATH, F_OK) != 0);
}

TEST(game_menus_resume_mode)
{
    reset_dir();
    ASSERT_EQ(gamemenu_resume_mode(), 0);
    ASSERT_TRUE(gamemenu_store_resume_mode(2));
    char text[512];
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"resume_mode\":\t\"swap\"") != NULL);
    ASSERT_EQ(gamemenu_resume_mode(), 2);
    ASSERT_TRUE(gamemenu_store_resume_mode(1));
    ASSERT_EQ(gamemenu_resume_mode(), 1);
    /* Kept beside a menu section; Backup (the default) removes it all. */
    ASSERT_TRUE(gamemenu_store(2, false));
    ASSERT_TRUE(gamemenu_store_resume_mode(0));
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "saved_games") == NULL);
    ASSERT_TRUE(strstr(text, "\"roms\"") != NULL);
    ASSERT_TRUE(gamemenu_store(2, true));
    ASSERT_TRUE(access(GM_PATH, F_OK) != 0);
    gamelists_write_text(GM_PATH, "{\"saved_games\": {\"resume_mode\": \"weird\"}}");
    ASSERT_EQ(gamemenu_resume_mode(), 0);
}

TEST(game_menus_switches)
{
    reset_dir();
    /* Open MainUI's defaults with no file (rc15). */
    ASSERT_FALSE(gamemenu_switch(0)); /* left_shortcut */
    ASSERT_FALSE(gamemenu_switch(1)); /* show_auto */
    ASSERT_TRUE(gamemenu_switch(2));  /* upscale_preview */
    ASSERT_TRUE(gamemenu_switch(3));  /* correct_aspect_ratio (on from rc15) */
    ASSERT_TRUE(gamemenu_switch(4));  /* gallery right_shortcut */
    ASSERT_TRUE(gamemenu_store_switch(0, true));
    ASSERT_TRUE(gamemenu_store_switch(4, false));
    ASSERT_TRUE(gamemenu_store_resume_mode(2));
    char text[512];
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"left_shortcut\":\ttrue") != NULL);
    ASSERT_TRUE(strstr(text, "\"right_shortcut\":\tfalse") != NULL);
    ASSERT_TRUE(gamemenu_switch(0));
    ASSERT_FALSE(gamemenu_switch(4));
    /* Defaults remove their key only; the resume mode stays. */
    ASSERT_TRUE(gamemenu_store_switch(0, false));
    ASSERT_TRUE(gamemenu_store_switch(4, true));
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "left_shortcut") == NULL);
    ASSERT_TRUE(strstr(text, "gallery") == NULL);
    ASSERT_TRUE(strstr(text, "\"resume_mode\":\t\"swap\"") != NULL);
    ASSERT_TRUE(gamemenu_store_resume_mode(0));
    ASSERT_TRUE(access(GM_PATH, F_OK) != 0);
    /* Comments (rc14 accepts them) are read; a wrong type is the default. */
    gamelists_write_text(GM_PATH, "{\n// mine\n\"saved_games\": {\"show_auto\": true, \"upscale_preview\": \"no\"},\n}");
    ASSERT_TRUE(gamemenu_switch(1));
    ASSERT_TRUE(gamemenu_switch(2));
    ASSERT_FALSE(gamemenu_store_switch(9, true));
}

TEST(game_menus_saved_games_layout)
{
    reset_dir();
    /* rc17: no key is Overlay; both choices are written out. */
    ASSERT_EQ(gamemenu_layout(), 0);
    ASSERT_TRUE(gamemenu_store_layout(1));
    char text[512];
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"layout\":\t\"framed\"") != NULL);
    ASSERT_EQ(gamemenu_layout(), 1);
    ASSERT_TRUE(gamemenu_store_resume_mode(2));
    ASSERT_TRUE(gamemenu_store_layout(0));
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"layout\":\t\"overlay\"") != NULL);
    ASSERT_TRUE(strstr(text, "\"resume_mode\":\t\"swap\"") != NULL);
    ASSERT_EQ(gamemenu_layout(), 0);
    /* An unknown value or a wrong type shows Overlay; bad indexes are refused. */
    gamelists_write_text(GM_PATH, "{\"saved_games\": {\"layout\": \"tiles\"}}");
    ASSERT_EQ(gamemenu_layout(), 0);
    gamelists_write_text(GM_PATH, "{\"saved_games\": {\"layout\": 1}}");
    ASSERT_EQ(gamemenu_layout(), 0);
    ASSERT_FALSE(gamemenu_store_layout(2));
    ASSERT_FALSE(gamemenu_store_layout(-1));
    /* A saved_games that is not an object is left alone. */
    gamelists_write_text(GM_PATH, "{\"saved_games\": 5}");
    ASSERT_FALSE(gamemenu_store_layout(1));
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "{\"saved_games\": 5}");
}

TEST(game_menus_damaged_file_left_alone)
{
    reset_dir();
    gamelists_write_text(GM_PATH, "{\"roms\": ");
    ASSERT_FALSE(gamemenu_store(0, false));
    gamelists_write_text(GM_PATH, "{\"roms\": 5}");
    ASSERT_FALSE(gamemenu_store(0, false));
    char text[64];
    read_file(GM_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "{\"roms\": 5}");
}

TEST(console_view_wrap_sets_carousel_and_grid)
{
    reset_dir();
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_WRAP, 1));
    char text[512];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"grid\":") != NULL);
    ASSERT_TRUE(strstr(text, "\"carousel\":") != NULL);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_WRAP), 1);
    /* Back to On: both keys and the empty sections go, and so does the file. */
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_WRAP, 0));
    ASSERT_TRUE(access(CONSOLEVIEW_CONFIG_PATH, F_OK) != 0);
}

TEST(console_view_defaults_remove_only_their_key)
{
    reset_dir();
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH,
                         "{\"games\": \"carousel\", \"carousel\": {\"gap\": 32, \"animation\": \"slide\"}}");
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_ANIMATION, 0));
    char text[512];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "animation") == NULL);
    ASSERT_TRUE(strstr(text, "\"gap\":\t32") != NULL);
    ASSERT_EQ(consoleview_layout(CONSOLEVIEW_GAMES), CONSOLEVIEW_CAROUSEL);
}

TEST(console_view_value_not_offered_shows_first)
{
    reset_dir();
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"carousel\": {\"opacity_unselected\": 0.7}}");
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_OPACITY), 0);
    char text[128];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "0.7") != NULL);
}

TEST(console_view_option_damaged_file_left_alone)
{
    reset_dir();
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"carousel\": ");
    ASSERT_FALSE(consoleview_store_option(CONSOLEVIEW_ANIMATION, 2));
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"carousel\": 5}");
    ASSERT_FALSE(consoleview_store_option(CONSOLEVIEW_ANIMATION, 2));
    char text[64];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_STREQ(text, "{\"carousel\": 5}");
}

int main(void)
{
    RUN_TEST(scroll_off_keeps_the_file);
    RUN_TEST(scroll_off_reads_back_as_off_with_its_speed);
    RUN_TEST(scroll_on_again_keeps_the_speed);
    RUN_TEST(scroll_missing_file_is_off);
    RUN_TEST(scroll_default_file_maps_to_menu);
    RUN_TEST(default_values_are_marked);
    RUN_TEST(scroll_speeds_are_rendered_exactly);
    RUN_TEST(scroll_speed_snaps_to_offered_value);
    RUN_TEST(scroll_style_bounce_is_read);
    RUN_TEST(scroll_style_written_and_kept);
    RUN_TEST(scroll_style_without_file);
    RUN_TEST(video_preview_off_without_file);
    RUN_TEST(video_preview_written_and_read);
    RUN_TEST(video_preview_empty_file_is_1000ms);
    RUN_TEST(video_preview_size_read);
    RUN_TEST(video_preview_size_and_delay_keep_each_other);
    RUN_TEST(video_preview_size_waits_while_off);
    RUN_TEST(info_scroll_off_without_file);
    RUN_TEST(info_scroll_empty_file_is_2000ms_20px);
    RUN_TEST(info_scroll_written_and_read);
    RUN_TEST(info_scroll_off_removes_the_file_and_keeps_the_speed);
    RUN_TEST(console_view_missing_file_is_grid);
    RUN_TEST(console_view_written_and_read);
    RUN_TEST(console_view_keeps_the_other_keys);
    RUN_TEST(console_view_grid_for_both_removes_the_file);
    RUN_TEST(console_view_damaged_file_is_left_alone);
    RUN_TEST(console_view_unknown_value_is_grid);
    RUN_TEST(console_pictures_on_writes_the_folder_and_creates_it);
    RUN_TEST(console_pictures_keep_a_folder_already_named);
    RUN_TEST(console_pictures_off_removes_the_key);
    RUN_TEST(every_tweaks_menu_has_room_for_its_items);
    RUN_TEST(main_menu_descriptions_fit);
    RUN_TEST(console_view_options_written_and_read);
    RUN_TEST(console_view_motion_and_slide_time);
    RUN_TEST(console_view_layout_keys);
    RUN_TEST(console_view_option_views);
    RUN_TEST(console_view_greys_out_unused_options);
    RUN_TEST(game_info_and_saved_games_menus);
    RUN_TEST(glo_entries_in_list_menus);
    RUN_TEST(console_view_on_off_switches);
    RUN_TEST(game_lists_options_follow_what_they_depend_on);
    RUN_TEST(options_follow_the_option_they_need);
    RUN_TEST(greyed_out_options_in_other_menus);
    RUN_TEST(console_view_remember_flag);
    RUN_TEST(console_view_selection_colours);
    RUN_TEST(game_menus_default_without_file);
    RUN_TEST(game_menus_hide_and_show_again);
    RUN_TEST(game_menus_keep_a_hand_made_order);
    RUN_TEST(game_menus_game_info_menu);
    RUN_TEST(game_menus_resume_mode);
    RUN_TEST(game_menus_switches);
    RUN_TEST(game_menus_saved_games_layout);
    RUN_TEST(game_menus_damaged_file_left_alone);
    RUN_TEST(console_view_wrap_sets_carousel_and_grid);
    RUN_TEST(console_view_defaults_remove_only_their_key);
    RUN_TEST(console_view_value_not_offered_shows_first);
    RUN_TEST(console_view_option_damaged_file_left_alone);
    RUN_TEST(pair_parser_accepts_open_mainui_forms);
    RUN_TEST(repeat_stock_values_remove_the_file);
    RUN_TEST(rows_and_font_menu_values);
    RUN_TEST(write_leaves_no_temporary_file);
    RUN_TEST(main_menu_default_shows_apps_not_expert);
    RUN_TEST(main_menu_default_follows_legacy_markers);
    RUN_TEST(main_menu_first_change_keeps_apps);
    RUN_TEST(main_menu_unnamed_section_follows_marker);
    RUN_TEST(main_menu_explicit_false_beats_marker);
    RUN_TEST(search_without_app_shows_off);
    RUN_TEST(search_named_in_file_without_app_shows_off);
    RUN_TEST(other_change_keeps_search_for_later_install);
    RUN_TEST(search_toggle_without_app_stays_off);
    RUN_TEST(custom_action_without_definition_shows_off);
    RUN_TEST(custom_action_needs_label_and_launch);

    (void)!system("rm -rf " TEST_DIR);
    TEST_REPORT();
    return test_failures;
}
