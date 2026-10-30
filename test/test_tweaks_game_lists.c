/**
 * @file test_tweaks_game_lists.c
 * @brief Tweaks > Appearance > Game lists and Main menu, on the real files
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
static List _menu_main_menu, _menu_main_menu_items, _menu_main_context_items, _menu_console_view;
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
    ASSERT_STREQ(label, "Off");
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

TEST(game_lists_menu_has_room_for_every_item)
{
    /* list_addItem drops an item past the capacity without a word. */
    FILE *fp = fopen("../src/tweaks/game_lists.h", "r");
    ASSERT_TRUE(fp != NULL);
    char line[512];
    int capacity = -1, items = 0;
    bool inside = false;
    while (fgets(line, sizeof(line), fp)) {
        const char *create = strstr(line, "_menu_game_lists = list_createWithTitle(");
        if (create) {
            capacity = atoi(create + strlen("_menu_game_lists = list_createWithTitle("));
            inside = true;
            continue;
        }
        if (inside && strstr(line, "menu_stack[++menu_level]"))
            break;
        if (inside && strstr(line, "list_addItem"))
            items++;
    }
    fclose(fp);
    ASSERT_EQ(items, 14);
    ASSERT_TRUE(items <= capacity);
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
    ASSERT_STREQ(label, "Off");
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

TEST(main_menu_submenu_has_room_for_every_item)
{
    FILE *fp = fopen("../src/tweaks/main_menu.h", "r");
    ASSERT_TRUE(fp != NULL);
    char line[512];
    int capacity = -1, items = 0;
    bool inside = false;
    while (fgets(line, sizeof(line), fp)) {
        const char *create = strstr(line, "_menu_main_menu = list_createWithTitle(");
        if (create) {
            capacity = atoi(create + strlen("_menu_main_menu = list_createWithTitle("));
            inside = true;
            continue;
        }
        if (inside && strstr(line, "menu_stack[++menu_level]"))
            break;
        if (inside && strstr(line, "list_addItem"))
            items++;
    }
    fclose(fp);
    ASSERT_EQ(items, 3);
    ASSERT_TRUE(capacity >= items);
}

TEST(console_view_submenu_has_room_for_every_item)
{
    FILE *fp = fopen("../src/tweaks/main_menu.h", "r");
    ASSERT_TRUE(fp != NULL);
    char line[512];
    int capacity = -1, items = 0;
    bool inside = false;
    while (fgets(line, sizeof(line), fp)) {
        const char *create = strstr(line, "_menu_console_view = list_createWithTitle(");
        if (create) {
            capacity = atoi(create + strlen("_menu_console_view = list_createWithTitle("));
            inside = true;
            continue;
        }
        if (inside && strstr(line, "menu_stack[++menu_level]"))
            break;
        if (inside && strstr(line, "list_addItem"))
            items++;
    }
    fclose(fp);
    ASSERT_EQ(items, 20);
    ASSERT_TRUE(capacity >= items);
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
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_DURATION, 3));
    char text[512];
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"easing\":\t\"smooth\"") != NULL);
    ASSERT_TRUE(strstr(text, "\"duration_ms\":\t300") != NULL);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_EASING), 1);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_DURATION), 3);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_DURATION, 5));
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_DURATION), 5);
    ASSERT_FALSE(consoleview_store_option(CONSOLEVIEW_DURATION, 6));
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_REPEAT, 2));
    read_file(CONSOLEVIEW_CONFIG_PATH, text, sizeof(text));
    ASSERT_TRUE(strstr(text, "\"repeat_ms\":\t200") != NULL);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_REPEAT), 2);
    ASSERT_TRUE(consoleview_store_option(CONSOLEVIEW_REPEAT, 0));
    /* Rob's suggested values read back; a time not offered shows as
     * Default and is kept. */
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"carousel\": {\"duration_ms\": 300, \"repeat_ms\": 200}}");
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_DURATION), 3);
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_REPEAT), 2);
    /* rc10's suggestions: 180 ms, repeat 100. */
    gamelists_write_text(CONSOLEVIEW_CONFIG_PATH, "{\"carousel\": {\"duration_ms\": 180, \"repeat_ms\": 100}}");
    ASSERT_EQ(consoleview_option(CONSOLEVIEW_DURATION), 1);
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
    RUN_TEST(game_lists_menu_has_room_for_every_item);
    RUN_TEST(main_menu_submenu_has_room_for_every_item);
    RUN_TEST(console_view_submenu_has_room_for_every_item);
    RUN_TEST(console_view_options_written_and_read);
    RUN_TEST(console_view_motion_and_slide_time);
    RUN_TEST(console_view_layout_keys);
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
