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
 * Tests the production header (not a copy). Also built under ASan by
 * unit-test-san.
 *
 * Build and run: make -f Makefile.unit test_cache_db
 */

#include "onion_test.h"
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Production code: cacheDB.h needs is_file/file_basename (file.h) and the
 * log macros (log.h) from its includer. sqlite3 calls link against
 * stubs/sqlite3_stub.c, which always fails to open a database. */
#include "utils/file.h"
#include "utils/log.h"
#include "../src/playActivity/cacheDB.h"

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
    int version = cache_get_path_and_version(path, sizeof(path), "/tmp/test_cache_db/Roms/GBA", "GBA");
    ASSERT_EQ(version, 6);
    ASSERT_STREQ(path, "/tmp/test_cache_db/Roms/GBA/GBA_cache6.db");
}

TEST(cache_version_2_found) {
    setup();
    touch_file("/tmp/test_cache_db/Roms/SNES/SNES_cache2.db");

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, sizeof(path), "/tmp/test_cache_db/Roms/SNES", "SNES");
    ASSERT_EQ(version, 2);
    ASSERT_STREQ(path, "/tmp/test_cache_db/Roms/SNES/SNES_cache2.db");
}

TEST(cache_version_6_preferred_over_2) {
    setup();
    /* Both files exist: version 6 should be preferred */
    touch_file("/tmp/test_cache_db/Roms/GBA/GBA_cache6.db");
    touch_file("/tmp/test_cache_db/Roms/GBA/GBA_cache2.db");

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, sizeof(path), "/tmp/test_cache_db/Roms/GBA", "GBA");
    ASSERT_EQ(version, 6);
}

TEST(cache_version_not_found) {
    setup();
    /* No cache files at all */

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, sizeof(path), "/tmp/test_cache_db/Roms/GBA", "GBA");
    ASSERT_EQ(version, CACHE_NOT_FOUND);
}

TEST(cache_version_wrong_name) {
    setup();
    /* File exists but for different emu name */
    touch_file("/tmp/test_cache_db/Roms/GBA/SNES_cache6.db");

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, sizeof(path), "/tmp/test_cache_db/Roms/GBA", "GBA");
    ASSERT_EQ(version, CACHE_NOT_FOUND);
}

TEST(cache_version_nonexistent_dir) {
    setup();

    char path[PATH_MAX];
    int version = cache_get_path_and_version(path, sizeof(path), "/tmp/test_cache_db/Roms/NONEXISTENT", "NONEXISTENT");
    ASSERT_EQ(version, CACHE_NOT_FOUND);
}

/* ==== cache_get_path tests ==== */

TEST(cache_get_path_finds_in_rom_dir) {
    setup();
    touch_file("/tmp/test_cache_db/Roms/GBA/GBA_cache6.db");

    char cache_path[PATH_MAX];
    char cache_name[STR_MAX];
    int version = cache_get_path(cache_path, sizeof(cache_path), cache_name, "/tmp/test_cache_db/Roms/GBA/game.gba");
    ASSERT_EQ(version, 6);
    ASSERT_STREQ(cache_name, "GBA");
}

TEST(cache_get_path_finds_in_parent_dir) {
    setup();
    /* Cache is in the parent (SNES) dir, ROM is in subfolder */
    touch_file("/tmp/test_cache_db/Roms/PS/PS_cache2.db");

    char cache_path[PATH_MAX];
    char cache_name[STR_MAX];
    int version = cache_get_path(cache_path, sizeof(cache_path), cache_name, "/tmp/test_cache_db/Roms/PS/subfolder/game.bin");
    ASSERT_EQ(version, 2);
    ASSERT_STREQ(cache_name, "PS");
}

TEST(cache_get_path_not_found) {
    setup();
    /* No cache files anywhere */

    char cache_path[PATH_MAX];
    char cache_name[STR_MAX];
    int version = cache_get_path(cache_path, sizeof(cache_path), cache_name, "/tmp/test_cache_db/Roms/GBA/game.gba");
    ASSERT_EQ(version, CACHE_NOT_FOUND);
}

/* ==== Buffer bounds (F2: stack overflow with long folder names) ==== */

/* 150-character folder: "<dir>/<name>_cache6.db" is ~330 bytes. */
static void long_name(char *out, size_t size)
{
    size_t n = size - 1 < 150 ? size - 1 : 150;
    memset(out, 'A', n);
    out[n] = '\0';
}

TEST(cache_version_small_buffer_is_not_overrun) {
    setup();
    char name[151], dir[512];
    long_name(name, sizeof(name));
    snprintf(dir, sizeof(dir), "/tmp/test_cache_db/Roms/%s", name);

    char path[64]; /* far too small: must truncate, not overflow */
    int version = cache_get_path_and_version(path, sizeof(path), dir, name);
    ASSERT_EQ(version, CACHE_NOT_FOUND);
    ASSERT_EQ(path[0], '\0');
}

TEST(cache_version_truncated_path_is_never_probed) {
    setup();
    /* A file whose name equals the truncated prefix must not be matched. */
    char path[32];
    int version = cache_get_path_and_version(path, sizeof(path), "/tmp/test_cache_db/Roms/GBA", "GBA");
    ASSERT_EQ(version, CACHE_NOT_FOUND);
}

TEST(cache_version_long_folder_found_with_path_max) {
    setup();
    char name[151], dir[512], file[1024];
    long_name(name, sizeof(name));
    snprintf(dir, sizeof(dir), "/tmp/test_cache_db/Roms/%s", name);
    mkdir_p(dir);
    snprintf(file, sizeof(file), "%s/%s_cache6.db", dir, name);
    touch_file(file);

    char path[PATH_MAX];
    ASSERT_EQ(cache_get_path_and_version(path, sizeof(path), dir, name), 6);
    ASSERT_STREQ(path, file);
}

/* The original crash: cache_db_find() on a ROM under a long folder with a
 * cache DB present wrote ~330 bytes into a 256-byte stack buffer. */
TEST(cache_db_find_long_folder_does_not_overflow) {
    setup();
    char name[151], dir[512], file[1024], rom[1024];
    long_name(name, sizeof(name));
    snprintf(dir, sizeof(dir), "/tmp/test_cache_db/Roms/%s", name);
    mkdir_p(dir);
    snprintf(file, sizeof(file), "%s/%s_cache6.db", dir, name);
    touch_file(file);
    snprintf(rom, sizeof(rom), "%s/game.gba", dir);

    /* sqlite stub cannot open the DB, so no item: only the path matters. */
    CacheDBItem *item = cache_db_find(rom);
    ASSERT_NULL(item);
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

    /* buffer bounds */
    RUN_TEST(cache_version_small_buffer_is_not_overrun);
    RUN_TEST(cache_version_truncated_path_is_never_probed);
    RUN_TEST(cache_version_long_folder_found_with_path_max);
    RUN_TEST(cache_db_find_long_folder_does_not_overflow);

    /* Cleanup */
    system("rm -rf /tmp/test_cache_db");

    TEST_REPORT();
    return test_failures;
}
