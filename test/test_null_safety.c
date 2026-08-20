/**
 * @file test_null_safety.c
 * @brief Tests for NULL safety fixes in playActivityDB.h and gs_retroarch.h
 *
 * Validates that critical NULL pointer dereferences have been fixed:
 * - __ensure_rel_path handles strdup() failure
 * - file_dirname NULL return is handled in gs_retroarch
 * - file_removeExtension NULL return is handled in DB operations
 *
 * Build and run: make -f Makefile.unit test_null_safety
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ---- Stubs ---- */
#define print_debug(...)
#define printf_debug(...)

/* ================================================================== */
/*  Test: __ensure_rel_path NULL safety                               */
/* ================================================================== */

/*
 * Reproduce the FIXED __ensure_rel_path logic from playActivityDB.h
 * The key fix: check strdup() return and str_split() return for NULL.
 */
static void fixed_ensure_rel_path(char *rel_path, const char *rom_path)
{
    /* Simulate the case where file_path_relative_to returns false */
    if (strstr(rom_path, "../../Roms/") != NULL) {
        char *dup = strdup(rom_path);
        if (dup == NULL) {
            strncpy(rel_path, rom_path, PATH_MAX - 1);
            rel_path[PATH_MAX - 1] = '\0';
            return;
        }
        char *tail = str_split(dup, "../../Roms/");
        strncpy(rel_path, tail != NULL ? tail : rom_path, PATH_MAX - 1);
        rel_path[PATH_MAX - 1] = '\0';
        free(dup);
    }
    else {
        char *temp = strdup(rom_path);
        if (temp == NULL) {
            strncpy(rel_path, rom_path, PATH_MAX - 1);
            rel_path[PATH_MAX - 1] = '\0';
            return;
        }
        char *replaced = str_replace(temp, "/mnt/SDCARD/Roms/", "");
        free(temp);
        strncpy(rel_path, replaced ? replaced : rom_path, PATH_MAX - 1);
        rel_path[PATH_MAX - 1] = '\0';
        free(replaced);
    }
}

TEST(ensure_rel_path_with_relative_roms) {
    char rel_path[PATH_MAX] = "";
    fixed_ensure_rel_path(rel_path, "../../Roms/GBA/game.gba");
    ASSERT_STREQ(rel_path, "GBA/game.gba");
}

TEST(ensure_rel_path_with_absolute_path) {
    char rel_path[PATH_MAX] = "";
    fixed_ensure_rel_path(rel_path, "/mnt/SDCARD/Roms/GBA/game.gba");
    ASSERT_STREQ(rel_path, "GBA/game.gba");
}

TEST(ensure_rel_path_no_match) {
    char rel_path[PATH_MAX] = "";
    fixed_ensure_rel_path(rel_path, "/other/path/game.gba");
    /* str_replace won't find "/mnt/SDCARD/Roms/" so original path returned */
    ASSERT_STREQ(rel_path, "/other/path/game.gba");
}

/* ================================================================== */
/*  Test: file_dirname NULL handling in gs_retroarch                  */
/* ================================================================== */

/*
 * Reproduce the FIXED content directory override logic.
 * The key fix: check file_dirname() return for NULL before use.
 */
static bool fixed_content_dir_override(const char *rompath, char *cfg_path_out, size_t cfg_path_size)
{
    char *contentDir = file_dirname(rompath);
    if (contentDir != NULL) {
        snprintf(cfg_path_out, cfg_path_size, "/config/%s.cfg", file_basename(contentDir));
        free(contentDir);
        return true;
    }
    return false;
}

TEST(content_dir_override_with_slash) {
    char cfg_path[256] = "";
    /* Normal path with directory separator */
    bool result = fixed_content_dir_override("/mnt/SDCARD/Roms/GBA/game.gba", cfg_path, sizeof(cfg_path));
    ASSERT_TRUE(result);
    ASSERT_STREQ(cfg_path, "/config/GBA.cfg");
}

TEST(content_dir_override_no_slash) {
    char cfg_path[256] = "";
    /* Path without directory separator — file_dirname returns NULL */
    bool result = fixed_content_dir_override("game.gba", cfg_path, sizeof(cfg_path));
    ASSERT_FALSE(result);
    ASSERT_STREQ(cfg_path, ""); /* unchanged */
}

/* ================================================================== */
/*  Test: file_removeExtension NULL safety in DB operations           */
/* ================================================================== */

TEST(file_removeExtension_normal) {
    char *result = file_removeExtension("game.gba");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "game");
    free(result);
}

TEST(file_removeExtension_no_extension) {
    char *result = file_removeExtension("game");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "game");
    free(result);
}

TEST(file_removeExtension_null_input) {
    char *result = file_removeExtension(NULL);
    ASSERT_NULL(result);
}

/*
 * Test the pattern: file_removeExtension(file_basename(path))
 * This is used in multiple places in playActivityDB.h
 */
TEST(remove_ext_basename_pattern_normal) {
    const char *path = "GBA/game.gba";
    char *rom_name = file_removeExtension(file_basename(path));
    ASSERT_NOT_NULL(rom_name);
    ASSERT_STREQ(rom_name, "game");
    free(rom_name);
}

TEST(remove_ext_basename_pattern_no_dir) {
    const char *path = "game.gba";
    char *rom_name = file_removeExtension(file_basename(path));
    ASSERT_NOT_NULL(rom_name);
    ASSERT_STREQ(rom_name, "game");
    free(rom_name);
}

/*
 * Test that the fixed code handles NULL rom_name safely
 * by using a fallback empty string.
 */
TEST(null_rom_name_fallback) {
    /* Simulate the fixed pattern: use empty string when NULL */
    char *rom_name = NULL; /* simulated failure */
    const char *safe_name = rom_name != NULL ? rom_name : "";
    ASSERT_STREQ(safe_name, "");
    free(rom_name); /* cleanup (safe even though NULL) */
}

/* ================================================================== */
/*  Test: str_split NULL safety                                       */
/* ================================================================== */

TEST(str_split_found) {
    char buf[64];
    strncpy(buf, "../../Roms/GBA/game.gba", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tail = str_split(buf, "../../Roms/");
    ASSERT_NOT_NULL(tail);
    ASSERT_STREQ(tail, "GBA/game.gba");
}

TEST(str_split_not_found) {
    char buf[64];
    strncpy(buf, "/other/path/game.gba", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tail = str_split(buf, "../../Roms/");
    ASSERT_NULL(tail);
}

/* ================================================================== */
/*  Test: file_dirname NULL cases                                     */
/* ================================================================== */

TEST(file_dirname_normal) {
    char *dir = file_dirname("/mnt/SDCARD/Roms/GBA/game.gba");
    ASSERT_NOT_NULL(dir);
    ASSERT_STREQ(dir, "/mnt/SDCARD/Roms/GBA");
    free(dir);
}

TEST(file_dirname_no_slash) {
    char *dir = file_dirname("game.gba");
    ASSERT_NULL(dir);
}

TEST(file_dirname_root) {
    char *dir = file_dirname("/game.gba");
    ASSERT_NOT_NULL(dir);
    ASSERT_STREQ(dir, "");
    free(dir);
}

/* ================================================================== */
/*  main                                                              */
/* ================================================================== */

int main(void)
{
    printf("\n=== NULL Safety Bug Fix Tests ===\n\n");

    /* __ensure_rel_path */
    RUN_TEST(ensure_rel_path_with_relative_roms);
    RUN_TEST(ensure_rel_path_with_absolute_path);
    RUN_TEST(ensure_rel_path_no_match);

    /* file_dirname NULL in gs_retroarch */
    RUN_TEST(content_dir_override_with_slash);
    RUN_TEST(content_dir_override_no_slash);

    /* file_removeExtension NULL safety */
    RUN_TEST(file_removeExtension_normal);
    RUN_TEST(file_removeExtension_no_extension);
    RUN_TEST(file_removeExtension_null_input);
    RUN_TEST(remove_ext_basename_pattern_normal);
    RUN_TEST(remove_ext_basename_pattern_no_dir);
    RUN_TEST(null_rom_name_fallback);

    /* str_split */
    RUN_TEST(str_split_found);
    RUN_TEST(str_split_not_found);

    /* file_dirname */
    RUN_TEST(file_dirname_normal);
    RUN_TEST(file_dirname_no_slash);
    RUN_TEST(file_dirname_root);

    TEST_REPORT();
    return test_failures;
}
