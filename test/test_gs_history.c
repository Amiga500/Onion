/**
 * @file test_gs_history.c
 * @brief Unit tests for src/gameSwitcher/gs_history.h
 *
 * Tests the pure-logic game history functions: parseJsonToRecentItem
 * JSON parsing and field extraction, colon-splitting of rompath into
 * launch+rompath, and setEntryDefaultValues initialization.
 *
 * SDL and filesystem dependencies are stubbed out.
 *
 * Build and run: make -f Makefile.unit test_gs_history
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Production code: RecentItem, Game_s, parseJsonToRecentItem() and
 * setEntryDefaultValues() from gameSwitcher/gs_recent_item.h (SDL_Surface
 * from the test SDL stub). */
#include "../src/gameSwitcher/gs_recent_item.h"

/* ---- Tests ---- */

/* ---- parseJsonToRecentItem: valid entries ---- */

TEST(parse_valid_type_5) {
    const char *json = "{\"type\":5,\"label\":\"Super Mario\","
                       "\"rompath\":\"/mnt/SDCARD/Roms/GBA/mario.gba\","
                       "\"imgpath\":\"/mnt/SDCARD/Roms/GBA/Imgs/mario.png\","
                       "\"launch\":\"/mnt/SDCARD/Emu/GBA/launch.sh\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));

    ASSERT_TRUE(parseJsonToRecentItem(json, &item, 1));
    ASSERT_EQ(item.type, 5);
    ASSERT_EQ(item.lineNo, 1);
    ASSERT_STREQ(item.label, "Super Mario");
    ASSERT_STREQ(item.rompath, "/mnt/SDCARD/Roms/GBA/mario.gba");
    ASSERT_STREQ(item.imgpath, "/mnt/SDCARD/Roms/GBA/Imgs/mario.png");
    ASSERT_STREQ(item.launch, "/mnt/SDCARD/Emu/GBA/launch.sh");
}

TEST(parse_valid_type_17) {
    const char *json = "{\"type\":17,\"label\":\"Tetris\","
                       "\"rompath\":\"/mnt/SDCARD/Roms/GB/tetris.gb\","
                       "\"imgpath\":\"\","
                       "\"launch\":\"/mnt/SDCARD/Emu/GB/launch.sh\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));

    ASSERT_TRUE(parseJsonToRecentItem(json, &item, 42));
    ASSERT_EQ(item.type, 17);
    ASSERT_EQ(item.lineNo, 42);
    ASSERT_STREQ(item.label, "Tetris");
}

/* ---- parseJsonToRecentItem: invalid entries ---- */

TEST(parse_invalid_json) {
    RecentItem item;
    memset(&item, 0, sizeof(item));
    ASSERT_FALSE(parseJsonToRecentItem("{bad json", &item, 1));
}

TEST(parse_wrong_type) {
    const char *json = "{\"type\":3,\"label\":\"Test\","
                       "\"rompath\":\"/test\",\"launch\":\"/launch\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));
    ASSERT_FALSE(parseJsonToRecentItem(json, &item, 1));
}

TEST(parse_missing_type) {
    const char *json = "{\"label\":\"Test\","
                       "\"rompath\":\"/test\",\"launch\":\"/launch\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));
    ASSERT_FALSE(parseJsonToRecentItem(json, &item, 1));
}

TEST(parse_type_string_not_number) {
    const char *json = "{\"type\":\"5\",\"label\":\"Test\","
                       "\"rompath\":\"/test\",\"launch\":\"/launch\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));
    ASSERT_FALSE(parseJsonToRecentItem(json, &item, 1));
}

/* ---- parseJsonToRecentItem: colon-splitting ---- */

TEST(parse_colon_splits_rompath) {
    const char *json = "{\"type\":5,\"label\":\"Game\","
                       "\"rompath\":\"/mnt/SDCARD/Emu/launch.sh:/mnt/SDCARD/Roms/game.rom\","
                       "\"imgpath\":\"\","
                       "\"launch\":\"\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));

    ASSERT_TRUE(parseJsonToRecentItem(json, &item, 1));
    ASSERT_STREQ(item.launch, "/mnt/SDCARD/Emu/launch.sh");
    ASSERT_STREQ(item.rompath, "/mnt/SDCARD/Roms/game.rom");
}

TEST(parse_no_colon_keeps_original) {
    const char *json = "{\"type\":5,\"label\":\"Game\","
                       "\"rompath\":\"/mnt/SDCARD/Roms/game.rom\","
                       "\"imgpath\":\"\","
                       "\"launch\":\"/mnt/SDCARD/Emu/launch.sh\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));

    ASSERT_TRUE(parseJsonToRecentItem(json, &item, 1));
    ASSERT_STREQ(item.rompath, "/mnt/SDCARD/Roms/game.rom");
    ASSERT_STREQ(item.launch, "/mnt/SDCARD/Emu/launch.sh");
}

/* ---- parseJsonToRecentItem: missing optional fields ---- */

TEST(parse_missing_label) {
    const char *json = "{\"type\":5,"
                       "\"rompath\":\"/mnt/SDCARD/Roms/game.rom\","
                       "\"launch\":\"/mnt/SDCARD/Emu/launch.sh\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));

    ASSERT_TRUE(parseJsonToRecentItem(json, &item, 1));
    ASSERT_STREQ(item.label, "");
    ASSERT_STREQ(item.rompath, "/mnt/SDCARD/Roms/game.rom");
}

TEST(parse_missing_imgpath) {
    const char *json = "{\"type\":5,\"label\":\"Game\","
                       "\"rompath\":\"/mnt/SDCARD/Roms/game.rom\","
                       "\"launch\":\"/mnt/SDCARD/Emu/launch.sh\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));

    ASSERT_TRUE(parseJsonToRecentItem(json, &item, 1));
    ASSERT_STREQ(item.imgpath, "");
}

TEST(parse_missing_launch) {
    const char *json = "{\"type\":5,\"label\":\"Game\","
                       "\"rompath\":\"/mnt/SDCARD/Roms/game.rom\","
                       "\"imgpath\":\"/img.png\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));

    ASSERT_TRUE(parseJsonToRecentItem(json, &item, 1));
    ASSERT_STREQ(item.launch, "");
}

/* ---- parseJsonToRecentItem: line number tracking ---- */

TEST(parse_line_number_preserved) {
    const char *json = "{\"type\":5,\"label\":\"L\","
                       "\"rompath\":\"/r\",\"launch\":\"/l\"}";
    RecentItem item;
    memset(&item, 0, sizeof(item));

    ASSERT_TRUE(parseJsonToRecentItem(json, &item, 99));
    ASSERT_EQ(item.lineNo, 99);
}

/* ---- setEntryDefaultValues ---- */

TEST(default_values_index) {
    Game_s game;
    memset(&game, 0xFF, sizeof(game));

    setEntryDefaultValues(&game, 7);

    ASSERT_EQ(game.index, 7);
}

TEST(default_values_clears_strings) {
    Game_s game;
    memset(&game, 0xFF, sizeof(game));

    setEntryDefaultValues(&game, 0);

    ASSERT_EQ(game.totalTime[0], '\0');
    ASSERT_EQ(game.name[0], '\0');
    ASSERT_EQ(game.shortname[0], '\0');
    ASSERT_EQ(game.core_name[0], '\0');
    ASSERT_EQ(game.core_path[0], '\0');
}

TEST(default_values_clears_flags) {
    Game_s game;
    memset(&game, 0xFF, sizeof(game));

    setEntryDefaultValues(&game, 0);

    ASSERT_FALSE(game.processed);
    ASSERT_FALSE(game.is_running);
    ASSERT_NULL(game.romScreen);
}

TEST(default_values_multiple_indices) {
    Game_s games[3];
    for (int i = 0; i < 3; i++) {
        memset(&games[i], 0xFF, sizeof(Game_s));
        setEntryDefaultValues(&games[i], i);
    }
    ASSERT_EQ(games[0].index, 0);
    ASSERT_EQ(games[1].index, 1);
    ASSERT_EQ(games[2].index, 2);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== gs_history.h Unit Tests ===\n\n");

    RUN_TEST(parse_valid_type_5);
    RUN_TEST(parse_valid_type_17);

    RUN_TEST(parse_invalid_json);
    RUN_TEST(parse_wrong_type);
    RUN_TEST(parse_missing_type);
    RUN_TEST(parse_type_string_not_number);

    RUN_TEST(parse_colon_splits_rompath);
    RUN_TEST(parse_no_colon_keeps_original);

    RUN_TEST(parse_missing_label);
    RUN_TEST(parse_missing_imgpath);
    RUN_TEST(parse_missing_launch);

    RUN_TEST(parse_line_number_preserved);

    RUN_TEST(default_values_index);
    RUN_TEST(default_values_clears_strings);
    RUN_TEST(default_values_clears_flags);
    RUN_TEST(default_values_multiple_indices);

    TEST_REPORT();
    return test_failures;
}
