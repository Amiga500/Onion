/**
 * @file test_state.c
 * @brief Unit tests for state_getAppName() and the recent-list readers of
 *        src/common/system/state.h (AdvanceMENU, screenshots, romscreens).
 *
 * Includes the production header. (This file used to test copies of the
 * functions, a "buggy" and a "fixed" one, which had drifted from the
 * source.) ROM paths in the fixtures are real files: the source skips a ROM
 * that is not on the card.
 *
 * The two bugs these tests were written for:
 *  - state_getAppName() skipped a fixed 19 characters ("HOME=/mnt/SDCARD ./"),
 *    so a shorter HOME cut the app name: it looks for " ./" now.
 *  - history_getRecentPath() stopped at the first entry that was not a game
 *    (an App launch): screenshots were named "Screenshot". It goes on now.
 *
 * Build and run: make -f Makefile.unit test_state
 */

#include "onion_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char recent_path[64], recent_hidden_path[64];
#define RECENTLIST_PATH recent_path
#define RECENTLIST_HIDDEN_PATH recent_hidden_path

#include "system/state.h"

static char dir[] = "/tmp/onion_test_state_XXXXXX";
static char rom_a[96], rom_b[96];

static void write_recent(const char *lines)
{
    FILE *fp = fopen(recent_path, "w");
    fputs(lines, fp);
    fclose(fp);
}

static void game_line(char *out, size_t size, const char *label, int type, const char *rom)
{
    snprintf(out, size,
             "{\"label\":\"%s\",\"type\":%d,\"rompath\":\"%s\","
             "\"launch\":\"/mnt/SDCARD/Emu/GBA/launch.sh\"}\n",
             label, type, rom);
}

/* ---- state_getAppName ---- */

TEST(state_getAppName_standard_sdcard_prefix) {
    char out[STR_MAX] = {0};
    state_getAppName(out, "HOME=/mnt/SDCARD ./Tweaks; chmod 777 Tweaks");
    ASSERT_STREQ(out, "Tweaks");
}

TEST(state_getAppName_no_semicolon) {
    char out[STR_MAX] = {0};
    state_getAppName(out, "HOME=/mnt/SDCARD ./Tweaks");
    ASSERT_STREQ(out, "Tweaks");
}

TEST(state_getAppName_app_name_with_space) {
    char out[STR_MAX] = {0};
    state_getAppName(out, "HOME=/mnt/SDCARD ./My App; chmod 777 MyApp");
    ASSERT_STREQ(out, "My App");
}

TEST(state_getAppName_no_prefix_returns_empty) {
    char out[STR_MAX] = "x";
    ASSERT_EQ((int)state_getAppName(out, "invalid_command_without_prefix"), 0);
    ASSERT_EQ((int)strlen(out), 0);
}

/* 18 characters of prefix: the old fixed offset gave "ppName". */
TEST(state_getAppName_shorter_home_prefix) {
    char out[STR_MAX] = {0};
    state_getAppName(out, "HOME=/home/user ./AppName; chmod 777 AppName");
    ASSERT_STREQ(out, "AppName");
}

/* ---- history_getRecentPath (the recent list chosen by the source) ---- */

TEST(history_getRecentPath_single_game_entry) {
    char line[STR_MAX * 2], rom_path[STR_MAX] = "";
    game_line(line, sizeof(line), "MyGame", 5, rom_a);
    write_recent(line);
    ASSERT_NOT_NULL(history_getRecentPath(rom_path));
    ASSERT_STREQ(rom_path, rom_a);
}

TEST(history_getRecentPath_type17_game_entry) {
    char line[STR_MAX * 2], rom_path[STR_MAX] = "";
    game_line(line, sizeof(line), "GameViaSearch", 17, rom_b);
    write_recent(line);
    ASSERT_NOT_NULL(history_getRecentPath(rom_path));
    ASSERT_STREQ(rom_path, rom_b);
}

TEST(history_getRecentPath_missing_file) {
    char rom_path[STR_MAX] = "";
    remove(recent_path);
    ASSERT_NULL(history_getRecentPath(rom_path));
}

TEST(history_getRecentPath_empty_file) {
    char rom_path[STR_MAX] = "";
    write_recent("");
    ASSERT_NULL(history_getRecentPath(rom_path));
}

TEST(history_getRecentPath_malformed_line_then_game) {
    char line[STR_MAX * 2], all[STR_MAX * 3], rom_path[STR_MAX] = "";
    game_line(line, sizeof(line), "Game2", 5, rom_b);
    snprintf(all, sizeof(all), "this is not valid json\n%s", line);
    write_recent(all);
    ASSERT_NOT_NULL(history_getRecentPath(rom_path));
    ASSERT_STREQ(rom_path, rom_b);
}

/* An App launch on top: the game below it names the screenshot. */
TEST(history_getRecentPath_app_entry_before_game_entry) {
    char line[STR_MAX * 2], all[STR_MAX * 3], rom_path[STR_MAX] = "";
    game_line(line, sizeof(line), "MyGame", 5, rom_a);
    snprintf(all, sizeof(all),
             "{\"label\":\"SomeApp\",\"type\":3,\"rompath\":\"/mnt/SDCARD/App/SomeApp\"}\n%s", line);
    write_recent(all);
    ASSERT_NOT_NULL(history_getRecentPath(rom_path));
    ASSERT_STREQ(rom_path, rom_a);
}

/* A game whose ROM is gone is skipped too. */
TEST(history_getRecentPath_missing_rom_skipped) {
    char l1[STR_MAX * 2], l2[STR_MAX * 2], all[STR_MAX * 4], rom_path[STR_MAX] = "";
    game_line(l1, sizeof(l1), "Gone", 5, "/tmp/onion_test_state_no_such.gba");
    game_line(l2, sizeof(l2), "Here", 5, rom_b);
    snprintf(all, sizeof(all), "%s%s", l1, l2);
    write_recent(all);
    ASSERT_NOT_NULL(history_getRecentPath(rom_path));
    ASSERT_STREQ(rom_path, rom_b);
}

/* The hidden list (Show recents off) is the one read when it exists. */
TEST(history_getRecentPath_hidden_list_first) {
    char line[STR_MAX * 2], rom_path[STR_MAX] = "";
    game_line(line, sizeof(line), "Shown", 5, rom_a);
    write_recent(line);
    game_line(line, sizeof(line), "Hidden", 5, rom_b);
    FILE *fp = fopen(recent_hidden_path, "w");
    fputs(line, fp);
    fclose(fp);
    ASSERT_NOT_NULL(history_getRecentPath(rom_path));
    ASSERT_STREQ(rom_path, rom_b);
    remove(recent_hidden_path);
}

/* ---- history_getRomscreenPath: the top entry only ---- */

TEST(history_getRomscreenPath_no_game_returns_false) {
    char path_out[STR_MAX] = "";
    write_recent("{\"label\":\"App\",\"type\":3,\"rompath\":\"/mnt/SDCARD/App/App1\"}\n");
    ASSERT_FALSE(history_getRomscreenPath(path_out));
}

TEST(history_getRomscreenPath_game_found_returns_path) {
    char line[STR_MAX * 2], path_out[STR_MAX] = "";
    game_line(line, sizeof(line), "MyGame", 5, rom_a);
    write_recent(line);
    ASSERT_TRUE(history_getRomscreenPath(path_out));
    ASSERT_TRUE(str_endsWith(path_out, ".png"));
    ASSERT_NOT_NULL(strstr(path_out, "/romScreens/"));
}

/* An App on top: no romscreen (it would be another game's picture). */
TEST(history_getRomscreenPath_app_on_top_returns_false) {
    char line[STR_MAX * 2], all[STR_MAX * 3], path_out[STR_MAX] = "";
    game_line(line, sizeof(line), "MyGame", 5, rom_a);
    snprintf(all, sizeof(all),
             "{\"label\":\"SomeApp\",\"type\":3,\"rompath\":\"/mnt/SDCARD/App/SomeApp\"}\n%s", line);
    write_recent(all);
    ASSERT_FALSE(history_getRomscreenPath(path_out));
}

int main(void)
{
    printf("\n=== state.h Unit Tests ===\n\n");

    if (mkdtemp(dir) == NULL)
        return 1;
    snprintf(recent_path, sizeof(recent_path), "%s/recentlist.json", dir);
    snprintf(recent_hidden_path, sizeof(recent_hidden_path), "%s/recentlist-hidden.json", dir);
    snprintf(rom_a, sizeof(rom_a), "%s/game a.gba", dir);
    snprintf(rom_b, sizeof(rom_b), "%s/game b.gba", dir);
    fclose(fopen(rom_a, "w"));
    fclose(fopen(rom_b, "w"));

    RUN_TEST(state_getAppName_standard_sdcard_prefix);
    RUN_TEST(state_getAppName_no_semicolon);
    RUN_TEST(state_getAppName_app_name_with_space);
    RUN_TEST(state_getAppName_no_prefix_returns_empty);
    RUN_TEST(state_getAppName_shorter_home_prefix);

    RUN_TEST(history_getRecentPath_single_game_entry);
    RUN_TEST(history_getRecentPath_type17_game_entry);
    RUN_TEST(history_getRecentPath_missing_file);
    RUN_TEST(history_getRecentPath_empty_file);
    RUN_TEST(history_getRecentPath_malformed_line_then_game);
    RUN_TEST(history_getRecentPath_app_entry_before_game_entry);
    RUN_TEST(history_getRecentPath_missing_rom_skipped);
    RUN_TEST(history_getRecentPath_hidden_list_first);

    RUN_TEST(history_getRomscreenPath_no_game_returns_false);
    RUN_TEST(history_getRomscreenPath_game_found_returns_path);
    RUN_TEST(history_getRomscreenPath_app_on_top_returns_false);

    remove(rom_a);
    remove(rom_b);
    remove(recent_path);
    rmdir(dir);
    TEST_REPORT();
    return test_failures;
}
