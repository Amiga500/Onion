/**
 * @file test_history_recent.c
 * @brief Contract tests for history_getRecentPath against src/common/system/state.h
 *
 * Includes the production header (stubs malloc only). ROM paths in fixtures
 * are real temp files because production calls exists() and skips missing ROMs.
 *
 * Build and run: make -f Makefile.unit test_history_recent
 */

#include "onion_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>

/* Intercept malloc used by the production parser in this translation unit.
 * libc fopen/stat keep using the real allocator. */
static int g_fail_malloc = 0;
static void *(*const onion_real_malloc)(size_t) = malloc;
static void *onion_test_malloc(size_t n)
{
    if (g_fail_malloc)
        return NULL;
    return onion_real_malloc(n);
}
#define malloc(n) onion_test_malloc(n)

#include "system/state.h"

static const char *kRecent = "/tmp/onion_test_recent_prod.json";
static const char *kRomA = "/tmp/onion_test_rom_a.gba";
static const char *kRomB = "/tmp/onion_test_rom_b.gba";

static void write_file(const char *path, const char *contents)
{
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs(contents, fp);
        fclose(fp);
    }
}

static void write_ndjson(const char *path, const char *line1, const char *line2)
{
    FILE *fp = fopen(path, "w");
    if (!fp)
        return;
    fputs(line1, fp);
    fputc('\n', fp);
    if (line2 != NULL) {
        fputs(line2, fp);
        fputc('\n', fp);
    }
    fclose(fp);
}

static char *game_line(char *buf, size_t buf_size, int type, const char *rompath)
{
    snprintf(buf, buf_size,
             "{\"label\":\"Game\",\"type\":%d,\"rompath\":\"%s\","
             "\"imgpath\":\"/tmp/img.png\",\"launch\":\"/tmp/launch.sh\"}",
             type, rompath);
    return buf;
}

static char *app_line(char *buf, size_t buf_size)
{
    snprintf(buf, buf_size,
             "{\"label\":\"SomeApp\",\"type\":3,\"rompath\":\"/mnt/SDCARD/App/SomeApp\"}");
    return buf;
}

static void cleanup_fixtures(void)
{
    unlink(kRecent);
    unlink(kRomA);
    unlink(kRomB);
}

/* First non-game line (type 3) must continue, not return NULL. */
TEST(prod_skip_app_then_valid_game) {
    char line_app[256];
    char line_game[512];
    write_file(kRomA, "rom");
    write_ndjson(kRecent, app_line(line_app, sizeof(line_app)),
                 game_line(line_game, sizeof(line_game), 5, kRomA));

    char rom_path[STR_MAX];
    memset(rom_path, 0xAA, sizeof(rom_path));
    char *result = history_getRecentPathFromPath(kRecent, rom_path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, kRomA);
    cleanup_fixtures();
}

/* First game line whose rompath does not exist must continue. */
TEST(prod_skip_missing_rom_then_valid) {
    char line_missing[512];
    char line_ok[512];
    write_file(kRomB, "rom");
    write_ndjson(kRecent,
                 game_line(line_missing, sizeof(line_missing), 5, "/tmp/onion_no_such_rom.gba"),
                 game_line(line_ok, sizeof(line_ok), 5, kRomB));

    char rom_path[STR_MAX] = {0};
    char *result = history_getRecentPathFromPath(kRecent, rom_path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, kRomB);
    cleanup_fixtures();
}

/* First valid game line is returned. */
TEST(prod_first_valid_game) {
    char line_game[512];
    write_file(kRomA, "rom");
    write_ndjson(kRecent, game_line(line_game, sizeof(line_game), 5, kRomA), NULL);

    char rom_path[STR_MAX] = {0};
    char *result = history_getRecentPathFromPath(kRecent, rom_path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, kRomA);
    cleanup_fixtures();
}

/* Type 17 is also a game. */
TEST(prod_type17_game) {
    char line_game[512];
    write_file(kRomA, "rom");
    write_ndjson(kRecent, game_line(line_game, sizeof(line_game), 17, kRomA), NULL);

    char rom_path[STR_MAX] = {0};
    char *result = history_getRecentPathFromPath(kRecent, rom_path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, kRomA);
    cleanup_fixtures();
}

/* Search-panel rompath uses a colon prefix that must be stripped before exists(). */
TEST(prod_strip_colon_search_panel) {
    char line_game[512];
    char rompath_field[STR_MAX];
    write_file(kRomA, "rom");
    snprintf(rompath_field, sizeof(rompath_field), "GBA:%s", kRomA);
    write_ndjson(kRecent, game_line(line_game, sizeof(line_game), 5, rompath_field), NULL);

    char rom_path[STR_MAX] = {0};
    char *result = history_getRecentPathFromPath(kRecent, rom_path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, kRomA);
    cleanup_fixtures();
}

/* Malformed JSON is skipped (continue), next valid game is returned. */
TEST(prod_malformed_json_then_game) {
    char line_game[512];
    write_file(kRomA, "rom");
    write_ndjson(kRecent, "this is not valid json",
                 game_line(line_game, sizeof(line_game), 5, kRomA));

    char rom_path[STR_MAX] = {0};
    char *result = history_getRecentPathFromPath(kRecent, rom_path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, kRomA);
    cleanup_fixtures();
}

/* malloc failure on a line continues rather than crashing; no valid path. */
TEST(prod_malloc_failure_continues) {
    char line_game[512];
    write_file(kRomA, "rom");
    write_ndjson(kRecent, game_line(line_game, sizeof(line_game), 5, kRomA), NULL);

    g_fail_malloc = 1;
    char rom_path[STR_MAX] = {0};
    char *result = history_getRecentPathFromPath(kRecent, rom_path);
    g_fail_malloc = 0;

    ASSERT_NULL(result);
    cleanup_fixtures();
}

TEST(prod_missing_recent_file) {
    unlink(kRecent);
    char rom_path[STR_MAX] = {0};
    char *result = history_getRecentPathFromPath(
        "/tmp/onion_test_nonexistent_recent_prod.json", rom_path);
    ASSERT_NULL(result);
}

TEST(prod_empty_recent_file) {
    write_file(kRecent, "");
    char rom_path[STR_MAX] = {0};
    char *result = history_getRecentPathFromPath(kRecent, rom_path);
    ASSERT_NULL(result);
    unlink(kRecent);
}

TEST(prod_state_getAppName_standard_prefix) {
    char cmd[STR_MAX] = "HOME=/mnt/SDCARD ./Tweaks; chmod 777 Tweaks";
    char out[STR_MAX] = {0};
    ASSERT_EQ((int)state_getAppName(out, cmd), 6);
    ASSERT_STREQ(out, "Tweaks");
}

int main(void)
{
    printf("\n=== state.h production history_getRecentPath ===\n\n");

    RUN_TEST(prod_skip_app_then_valid_game);
    RUN_TEST(prod_skip_missing_rom_then_valid);
    RUN_TEST(prod_first_valid_game);
    RUN_TEST(prod_type17_game);
    RUN_TEST(prod_strip_colon_search_panel);
    RUN_TEST(prod_malformed_json_then_game);
    RUN_TEST(prod_malloc_failure_continues);
    RUN_TEST(prod_missing_recent_file);
    RUN_TEST(prod_empty_recent_file);
    RUN_TEST(prod_state_getAppName_standard_prefix);

    TEST_REPORT();
    return test_failures;
}
