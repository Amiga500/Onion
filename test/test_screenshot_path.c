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

#define STR_MAX 256

/* ---- Stub log macros ---- */
#define print_debug(...)
#define printf_debug(...)

/* ---- Stub exists() ---- */
static bool stub_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

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

/* ---- Inlined numbering logic from __get_path_recent ---- */

/**
 * Given a base path like "/tmp/test_ss/Screenshots/GameName",
 * appends _NNN.png where NNN is the first available number 0-999.
 * Returns true if a free slot was found, false if all 1000 are taken.
 */
static bool find_numbered_path(char *path_out, const char *base_path)
{
    const size_t path_size = 512;
    strncpy(path_out, base_path, path_size - 1);
    path_out[path_size - 1] = '\0';

    char *fnptr = path_out + strlen(path_out);
    uint32_t i;
    for (i = 0; i < 1000; i++) {
        snprintf(fnptr, path_size - (fnptr - path_out), "_%03d.png", i);
        if (!stub_exists(path_out))
            break;
    }
    return i <= 999;
}

/**
 * Test the fallback name logic: if no app/game name is set,
 * "Screenshot" is used as the default.
 */
static void build_screenshot_path(char *path_out, const char *dir, const char *name)
{
    const size_t path_size = 512;
    strncpy(path_out, dir, path_size - 1);
    path_out[path_size - 1] = '\0';

    if (name != NULL && name[0] != '\0') {
        strncat(path_out, name, path_size - strlen(path_out) - 1);
    }
    else {
        strncat(path_out, "Screenshot", path_size - strlen(path_out) - 1);
    }
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
    build_screenshot_path(path, "/mnt/SDCARD/Screenshots/", "SuperMario");
    ASSERT_STREQ(path, "/mnt/SDCARD/Screenshots/SuperMario");
}

TEST(build_path_with_gameswitcher) {
    char path[512];
    build_screenshot_path(path, "/mnt/SDCARD/Screenshots/", "GameSwitcher");
    ASSERT_STREQ(path, "/mnt/SDCARD/Screenshots/GameSwitcher");
}

TEST(build_path_with_mainui) {
    char path[512];
    build_screenshot_path(path, "/mnt/SDCARD/Screenshots/", "MainUI");
    ASSERT_STREQ(path, "/mnt/SDCARD/Screenshots/MainUI");
}

TEST(build_path_default_when_empty) {
    char path[512];
    build_screenshot_path(path, "/mnt/SDCARD/Screenshots/", "");
    ASSERT_STREQ(path, "/mnt/SDCARD/Screenshots/Screenshot");
}

TEST(build_path_default_when_null) {
    char path[512];
    build_screenshot_path(path, "/mnt/SDCARD/Screenshots/", NULL);
    ASSERT_STREQ(path, "/mnt/SDCARD/Screenshots/Screenshot");
}

/* ==== Edge cases ==== */

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
    RUN_TEST(numbered_path_empty_base);
    RUN_TEST(numbered_path_with_spaces);

    /* Cleanup */
    system("rm -rf /tmp/test_ss");

    TEST_REPORT();
    return test_failures;
}
