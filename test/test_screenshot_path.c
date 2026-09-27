/**
 * @file test_screenshot_path.c
 * @brief Unit tests for screenshot path generation logic from screenshot.h
 *
 * Tests the filename numbering and path construction logic used
 * by __get_path_recent() — specifically the numbered suffix
 * generation (_000.png through _999.png) and the base path
 * prefix logic.
 *
 * SDL and process dependencies are stubbed out.
 *
 * Build and run: make -f Makefile.unit test_screenshot_path
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>

/* Production code: screenshot_numberedPath() from system/screenshot_path.h
 * (what __get_path_recent() in screenshot.h calls). */
#include "system/screenshot_path.h"

/* ---- Helper: create empty file ---- */
static void touch_file(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp) fclose(fp);
}

/* ---- Helper: recursive mkdir ---- */
static void mkdir_p(const char *path)
{
    char tmp[512];
    char *p = NULL;
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

/* ---- Wrappers over the production helper ---- */

/* "<dir>/<name>" split at the last '/' into the helper's two arguments. */
static bool find_numbered_path(char *path_out, const char *base_path)
{
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", base_path);
    char *slash = strrchr(dir, '/');
    if (slash == NULL)
        return false;
    *slash = '\0';
    return screenshot_numberedPath(path_out, 512, dir, slash + 1);
}

/* The name part: the helper falls back to "Screenshot". Only the first
 * slot is expected, since these folders do not exist on the host. */
static void build_screenshot_path(char *path_out, const char *dir, const char *name)
{
    screenshot_numberedPath(path_out, 512, dir, name);
}

/* ==== Numbered path tests ==== */

TEST(numbered_path_first_available) {
    system("rm -rf /tmp/test_ss");
    mkdir_p("/tmp/test_ss/Screenshots");

    char path[512];
    bool found = find_numbered_path(path, "/tmp/test_ss/Screenshots/Game");
    ASSERT_TRUE(found);
    /* First file should be _000.png */
    ASSERT_TRUE(strstr(path, "_000.png") != NULL);
}

TEST(numbered_path_skips_existing) {
    system("rm -rf /tmp/test_ss");
    mkdir_p("/tmp/test_ss/Screenshots");

    /* Create _000 and _001 */
    touch_file("/tmp/test_ss/Screenshots/Game_000.png");
    touch_file("/tmp/test_ss/Screenshots/Game_001.png");

    char path[512];
    bool found = find_numbered_path(path, "/tmp/test_ss/Screenshots/Game");
    ASSERT_TRUE(found);
    ASSERT_TRUE(strstr(path, "_002.png") != NULL);
}

TEST(numbered_path_skips_gap) {
    system("rm -rf /tmp/test_ss");
    mkdir_p("/tmp/test_ss/Screenshots");

    /* Create _000, _001, _002 but skip _003 */
    touch_file("/tmp/test_ss/Screenshots/Test_000.png");
    touch_file("/tmp/test_ss/Screenshots/Test_001.png");
    touch_file("/tmp/test_ss/Screenshots/Test_002.png");

    char path[512];
    bool found = find_numbered_path(path, "/tmp/test_ss/Screenshots/Test");
    ASSERT_TRUE(found);
    ASSERT_TRUE(strstr(path, "_003.png") != NULL);
}

TEST(numbered_path_format_three_digits) {
    system("rm -rf /tmp/test_ss");
    mkdir_p("/tmp/test_ss/Screenshots");

    char path[512];
    find_numbered_path(path, "/tmp/test_ss/Screenshots/Game");
    /* Verify it ends with _000.png (3-digit zero-padded format) */
    size_t len = strlen(path);
    ASSERT_TRUE(len >= 8);
    ASSERT_STREQ(path + len - 8, "_000.png");
}

TEST(numbered_path_png_extension) {
    system("rm -rf /tmp/test_ss");
    mkdir_p("/tmp/test_ss/Screenshots");

    char path[512];
    find_numbered_path(path, "/tmp/test_ss/Screenshots/Game");
    /* Must end with .png */
    size_t len = strlen(path);
    ASSERT_TRUE(len >= 4);
    ASSERT_STREQ(path + len - 4, ".png");
}

/* ==== Default name tests ==== */

TEST(build_path_with_game_name) {
    char path[512];
    build_screenshot_path(path, "/mnt/SDCARD/Screenshots","SuperMario");
    ASSERT_STREQ(path, "/mnt/SDCARD/Screenshots/SuperMario_000.png");
}

TEST(build_path_with_gameswitcher) {
    char path[512];
    build_screenshot_path(path, "/mnt/SDCARD/Screenshots","GameSwitcher");
    ASSERT_STREQ(path, "/mnt/SDCARD/Screenshots/GameSwitcher_000.png");
}

TEST(build_path_with_mainui) {
    char path[512];
    build_screenshot_path(path, "/mnt/SDCARD/Screenshots","MainUI");
    ASSERT_STREQ(path, "/mnt/SDCARD/Screenshots/MainUI_000.png");
}

TEST(build_path_default_when_empty) {
    char path[512];
    build_screenshot_path(path, "/mnt/SDCARD/Screenshots","");
    ASSERT_STREQ(path, "/mnt/SDCARD/Screenshots/Screenshot_000.png");
}

TEST(build_path_default_when_null) {
    char path[512];
    build_screenshot_path(path, "/mnt/SDCARD/Screenshots",NULL);
    ASSERT_STREQ(path, "/mnt/SDCARD/Screenshots/Screenshot_000.png");
}

/* ==== Edge cases ==== */

/* A path that does not fit is refused, not truncated. */
TEST(numbered_path_too_long_is_refused) {
    char path[32];
    ASSERT_FALSE(screenshot_numberedPath(path, sizeof(path), "/mnt/SDCARD/Screenshots",
                                         "A very long game name that cannot fit"));
}

TEST(numbered_path_empty_base) {
    system("rm -rf /tmp/test_ss");
    mkdir_p("/tmp/test_ss");

    char path[512];
    bool found = find_numbered_path(path, "/tmp/test_ss/X");
    ASSERT_TRUE(found);
    ASSERT_TRUE(strstr(path, "X_000.png") != NULL);
}

TEST(numbered_path_with_spaces) {
    system("rm -rf /tmp/test_ss");
    mkdir_p("/tmp/test_ss/Screenshots");

    char path[512];
    bool found = find_numbered_path(path, "/tmp/test_ss/Screenshots/Super Mario");
    ASSERT_TRUE(found);
    ASSERT_TRUE(strstr(path, "Super Mario_000.png") != NULL);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== screenshot.h Path Generation Unit Tests ===\n\n");

    /* Numbered path */
    RUN_TEST(numbered_path_first_available);
    RUN_TEST(numbered_path_skips_existing);
    RUN_TEST(numbered_path_skips_gap);
    RUN_TEST(numbered_path_format_three_digits);
    RUN_TEST(numbered_path_png_extension);

    /* Default name */
    RUN_TEST(build_path_with_game_name);
    RUN_TEST(build_path_with_gameswitcher);
    RUN_TEST(build_path_with_mainui);
    RUN_TEST(build_path_default_when_empty);
    RUN_TEST(build_path_default_when_null);

    /* Edge cases */
    RUN_TEST(numbered_path_too_long_is_refused);
    RUN_TEST(numbered_path_empty_base);
    RUN_TEST(numbered_path_with_spaces);

    /* Cleanup */
    system("rm -rf /tmp/test_ss");

    TEST_REPORT();
    return test_failures;
}
