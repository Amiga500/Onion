/**
 * @file test_null_safety.c
 * @brief Tests for the NULL returns playActivityDB.h and gs_retroarch.h handle
 *
 * Validates the NULL returns of file_removeExtension, str_split and
 * file_dirname that playActivityDB.h and gs_retroarch.h rely on.
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

    /* file_removeExtension NULL safety */
    RUN_TEST(file_removeExtension_normal);
    RUN_TEST(file_removeExtension_no_extension);
    RUN_TEST(file_removeExtension_null_input);
    RUN_TEST(remove_ext_basename_pattern_normal);
    RUN_TEST(remove_ext_basename_pattern_no_dir);

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
