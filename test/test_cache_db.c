/**
 * @file test_cache_db.c
 * @brief Unit tests for src/playActivity/cacheDB.h
 *
 * Tests the pure-logic cache path/version detection function:
 * cache_get_path_and_version() which checks for _cache6.db and
 * _cache2.db files and returns the appropriate version number.
 *
 * Also tests cache_get_path() directory traversal logic for
 * finding cache DB files by walking up from a ROM path.
 *
 * Uses temp directories and files to simulate the filesystem.
 *
 * Build and run: make -f Makefile.unit test_cache_db
 */

#include "onion_test.h"
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- Provide STR_MAX ---- */
#define STR_MAX 256

/* ---- Stub out log macros ---- */
#define print_debug(...)
#define printf_debug(...)

/* ---- Stub is_file ---- */
static bool is_file(const char *path)
{
    return access(path, F_OK) == 0;
}

/* ---- Stub file_basename (from file.h) ---- */
static const char *file_basename(const char *filename)
{
    const char *p = strrchr(filename, '/');
    return p ? p + 1 : filename;
}

/* ---- Constants from cacheDB.h ---- */
#define CACHE_NOT_FOUND -1

/* ---- Inline cache_get_path_and_version from cacheDB.h ---- */

static int cache_get_path_and_version(char *cache_db_file_path, const char *cache_dir, const char *dir_name)
{
    snprintf(cache_db_file_path, PATH_MAX - 1, "%s/%s_cache6.db", cache_dir, dir_name);
    if (is_file(cache_db_file_path) == 1) {
        return 6;
    }

    snprintf(cache_db_file_path, PATH_MAX - 1, "%s/%s_cache2.db", cache_dir, dir_name);
    if (is_file(cache_db_file_path) == 1) {
        return 2;
    }

    printf_debug("No cache found at: '%s'\n", cache_db_file_path);
    return CACHE_NOT_FOUND;
}

/* ---- Inline cache_get_path from cacheDB.h ---- */

static int cache_get_path(char *cache_path_out, char *cache_name_out, const char *rom_path)
{
    cache_path_out[0] = '\0';

    int cache_version = CACHE_NOT_FOUND;
    char *rom_path_dup = strdup((char *)rom_path);
    if (rom_path_dup == NULL)
        return CACHE_NOT_FOUND;
    char *cache_dir = dirname(rom_path_dup);

    while (cache_dir[0] != '\0' && strnlen(cache_dir, 17) > 16) {
        strncpy(cache_name_out, file_basename(cache_dir), STR_MAX - 1);
        cache_name_out[STR_MAX - 1] = '\0';
        cache_version = cache_get_path_and_version(cache_path_out, cache_dir, cache_name_out);

        if (cache_version != CACHE_NOT_FOUND) {
            break;
        }

        cache_dir = dirname(cache_dir);

        if (strcmp("Roms", file_basename(cache_dir)) == 0) {
            break;
        }
    }

    free(rom_path_dup);
    return cache_version;
}

/* ---- Helpers ---- */

static void mkdir_p(const char *path)
{
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static void touch_file(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp)
        fclose(fp);
}

/* ---- Setup/teardown ---- */

static void setup(void)
{
    system("rm -rf /tmp/test_cache_db");
    mkdir_p("/tmp/test_cache_db/Roms/GBA");
    mkdir_p("/tmp/test_cache_db/Roms/SNES");
    mkdir_p("/tmp/test_cache_db/Roms/PS/subfolder");
}

/* ==== cache_get_path_and_version tests ==== */

TEST(cache_version_6_found) {
    setup();
    touch_file("/tmp/test_cache_db/Roms/GBA/GBA_cache6.db");

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, "/tmp/test_cache_db/Roms/GBA", "GBA");
    ASSERT_EQ(version, 6);
    ASSERT_STREQ(path, "/tmp/test_cache_db/Roms/GBA/GBA_cache6.db");
}

TEST(cache_version_2_found) {
    setup();
    touch_file("/tmp/test_cache_db/Roms/SNES/SNES_cache2.db");

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, "/tmp/test_cache_db/Roms/SNES", "SNES");
    ASSERT_EQ(version, 2);
    ASSERT_STREQ(path, "/tmp/test_cache_db/Roms/SNES/SNES_cache2.db");
}

TEST(cache_version_6_preferred_over_2) {
    setup();
    /* Both files exist: version 6 should be preferred */
    touch_file("/tmp/test_cache_db/Roms/GBA/GBA_cache6.db");
    touch_file("/tmp/test_cache_db/Roms/GBA/GBA_cache2.db");

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, "/tmp/test_cache_db/Roms/GBA", "GBA");
    ASSERT_EQ(version, 6);
}

TEST(cache_version_not_found) {
    setup();
    /* No cache files at all */

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, "/tmp/test_cache_db/Roms/GBA", "GBA");
    ASSERT_EQ(version, CACHE_NOT_FOUND);
}

TEST(cache_version_wrong_name) {
    setup();
    /* File exists but for different emu name */
    touch_file("/tmp/test_cache_db/Roms/GBA/SNES_cache6.db");

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, "/tmp/test_cache_db/Roms/GBA", "GBA");
    ASSERT_EQ(version, CACHE_NOT_FOUND);
}

TEST(cache_version_nonexistent_dir) {
    setup();

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, "/tmp/test_cache_db/Roms/NONEXISTENT", "NONEXISTENT");
    ASSERT_EQ(version, CACHE_NOT_FOUND);
}

/* ==== cache_get_path tests ==== */

TEST(cache_get_path_finds_in_rom_dir) {
    setup();
    touch_file("/tmp/test_cache_db/Roms/GBA/GBA_cache6.db");

    char cache_path[PATH_MAX];
    char cache_name[STR_MAX];
    int version = cache_get_path(cache_path, cache_name, "/tmp/test_cache_db/Roms/GBA/game.gba");
    ASSERT_EQ(version, 6);
    ASSERT_STREQ(cache_name, "GBA");
}

TEST(cache_get_path_finds_in_parent_dir) {
    setup();
    /* Cache is in the parent (SNES) dir, ROM is in subfolder */
    touch_file("/tmp/test_cache_db/Roms/PS/PS_cache2.db");

    char cache_path[PATH_MAX];
    char cache_name[STR_MAX];
    int version = cache_get_path(cache_path, cache_name, "/tmp/test_cache_db/Roms/PS/subfolder/game.bin");
    ASSERT_EQ(version, 2);
    ASSERT_STREQ(cache_name, "PS");
}

TEST(cache_get_path_not_found) {
    setup();
    /* No cache files anywhere */

    char cache_path[PATH_MAX];
    char cache_name[STR_MAX];
    int version = cache_get_path(cache_path, cache_name, "/tmp/test_cache_db/Roms/GBA/game.gba");
    ASSERT_EQ(version, CACHE_NOT_FOUND);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== cacheDB.h Unit Tests ===\n\n");

    /* cache_get_path_and_version */
    RUN_TEST(cache_version_6_found);
    RUN_TEST(cache_version_2_found);
    RUN_TEST(cache_version_6_preferred_over_2);
    RUN_TEST(cache_version_not_found);
    RUN_TEST(cache_version_wrong_name);
    RUN_TEST(cache_version_nonexistent_dir);

    /* cache_get_path */
    RUN_TEST(cache_get_path_finds_in_rom_dir);
    RUN_TEST(cache_get_path_finds_in_parent_dir);
    RUN_TEST(cache_get_path_not_found);

    /* Cleanup */
    system("rm -rf /tmp/test_cache_db");

    TEST_REPORT();
    return test_failures;
}
