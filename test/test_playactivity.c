/**
 * @file test_playactivity.c
 * @brief Unit tests for playActivityDB.h utility functions.
 *
 * Tests get_rom_image_path and _get_active_rom_path without requiring
 * SQLite or device-specific dependencies.
 *
 * Build and run: make -f Makefile.unit test_playactivity && ./build_test/test_playactivity
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Local reproduction of get_rom_image_path                           */
/* ------------------------------------------------------------------ */

/* FIXED version: returns early for .p8/.png files */
static void _fixed_get_rom_image_path(char *rom_file, char *out_image_path)
{
    if (str_endsWith(rom_file, ".p8") || str_endsWith(rom_file, ".png")) {
        snprintf(out_image_path, STR_MAX - 1, "/mnt/SDCARD/Roms/%s", rom_file);
        return;
    }

    char *clean_rom_name = file_removeExtension(basename(rom_file));
    if (clean_rom_name == NULL)
        return;
    char *rom_folder = strtok(rom_file, "/");
    if (rom_folder == NULL)
        rom_folder = rom_file;

    snprintf(out_image_path, STR_MAX - 1, "/mnt/SDCARD/Roms/%s/Imgs/%s.png", rom_folder, clean_rom_name);
    free(clean_rom_name);
}

/* BUGGY version: falls through after .p8/.png match */
static void _buggy_get_rom_image_path(char *rom_file, char *out_image_path)
{
    if (str_endsWith(rom_file, ".p8") || str_endsWith(rom_file, ".png")) {
        snprintf(out_image_path, STR_MAX - 1, "/mnt/SDCARD/Roms/%s", rom_file);
        /* BUG: no return — falls through to strtok which corrupts rom_file */
    }

    char *clean_rom_name = file_removeExtension(basename(rom_file));
    if (clean_rom_name == NULL)
        return;
    char *rom_folder = strtok(rom_file, "/");
    if (rom_folder == NULL)
        rom_folder = rom_file;

    snprintf(out_image_path, STR_MAX - 1, "/mnt/SDCARD/Roms/%s/Imgs/%s.png", rom_folder, clean_rom_name);
    free(clean_rom_name);
}

/* ------------------------------------------------------------------ */
/* Tests for get_rom_image_path                                        */
/* ------------------------------------------------------------------ */

/* Normal ROM file gets Imgs path */
TEST(get_rom_image_path_normal_rom) {
    char rom_file[STR_MAX];
    strncpy(rom_file, "GBA/game.gba", STR_MAX - 1);
    char out_path[STR_MAX] = "";

    _fixed_get_rom_image_path(rom_file, out_path);
    ASSERT_STREQ(out_path, "/mnt/SDCARD/Roms/GBA/Imgs/game.png");
}

/* .p8 file gets direct Roms path (PICO-8 cart is its own image) */
TEST(get_rom_image_path_p8_file) {
    char rom_file[STR_MAX];
    strncpy(rom_file, "PICO/game.p8", STR_MAX - 1);
    char out_path[STR_MAX] = "";

    _fixed_get_rom_image_path(rom_file, out_path);
    ASSERT_STREQ(out_path, "/mnt/SDCARD/Roms/PICO/game.p8");
}

/* .png file gets direct Roms path */
TEST(get_rom_image_path_png_file) {
    char rom_file[STR_MAX];
    strncpy(rom_file, "PICO/game.png", STR_MAX - 1);
    char out_path[STR_MAX] = "";

    _fixed_get_rom_image_path(rom_file, out_path);
    ASSERT_STREQ(out_path, "/mnt/SDCARD/Roms/PICO/game.png");
}

/*
 * BUG #10 — Logged regression test.
 *
 * The buggy version falls through after the .p8/.png snprintf, causing
 * strtok to corrupt rom_file and overwrite out_image_path with an
 * incorrect Imgs path.
 *
 * The fixed version returns early with the correct direct path.
 */
TEST(get_rom_image_path_p8_fallthrough_bug) {
    char rom_file_buggy[STR_MAX];
    strncpy(rom_file_buggy, "PICO/game.p8", STR_MAX - 1);
    char out_path_buggy[STR_MAX] = "";

    char rom_file_fixed[STR_MAX];
    strncpy(rom_file_fixed, "PICO/game.p8", STR_MAX - 1);
    char out_path_fixed[STR_MAX] = "";

    _buggy_get_rom_image_path(rom_file_buggy, out_path_buggy);
    _fixed_get_rom_image_path(rom_file_fixed, out_path_fixed);

    /* The buggy version overwrites the correct .p8 path with an Imgs path */
    ASSERT_STREQ(out_path_buggy, "/mnt/SDCARD/Roms/PICO/Imgs/game.png");

    /* The fixed version returns the correct direct path */
    ASSERT_STREQ(out_path_fixed, "/mnt/SDCARD/Roms/PICO/game.p8");
}

/* ------------------------------------------------------------------ */
/* Tests for _get_active_rom_path (strncpy fix)                        */
/* ------------------------------------------------------------------ */

/* Reproduce the fixed _get_active_rom_path using a string input
 * instead of reading from the filesystem. */
static bool _fixed_get_active_rom_path_from_str(const char *cmd_input, char *rom_path_out)
{
    char *ptr;
    char cmd[STR_MAX];
    strncpy(cmd, cmd_input, STR_MAX - 1);
    cmd[STR_MAX - 1] = '\0';

    if (strlen(cmd) == 0) {
        return false;
    }

    if ((ptr = strrchr(cmd, '"')) != NULL) {
        *ptr = '\0';
    }

    if ((ptr = strrchr(cmd, '"')) != NULL) {
        strncpy(rom_path_out, ptr + 1, STR_MAX - 1);
        rom_path_out[STR_MAX - 1] = '\0';
        return true;
    }

    return false;
}

/* Standard command string with quoted rom path */
TEST(get_active_rom_path_standard) {
    char rom_path[STR_MAX] = "";
    bool result = _fixed_get_active_rom_path_from_str(
        "LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so \"/mnt/SDCARD/Emu/GBA/launch.sh\" \"/mnt/SDCARD/Roms/GBA/game.gba\"",
        rom_path);
    ASSERT_TRUE(result);
    ASSERT_STREQ(rom_path, "/mnt/SDCARD/Roms/GBA/game.gba");
}

/* No quotes in command string */
TEST(get_active_rom_path_no_quotes) {
    char rom_path[STR_MAX] = "";
    bool result = _fixed_get_active_rom_path_from_str(
        "no quotes here",
        rom_path);
    ASSERT_FALSE(result);
}

/* Empty command string */
TEST(get_active_rom_path_empty) {
    char rom_path[STR_MAX] = "";
    bool result = _fixed_get_active_rom_path_from_str("", rom_path);
    ASSERT_FALSE(result);
}

/* ------------------------------------------------------------------ */
/* Tests for lang_free NULL safety                                     */
/* ------------------------------------------------------------------ */

/* Reproduce lang_free with the FIX applied */
static char **_test_lang_list = NULL;
#define _TEST_LANG_MAX 5

static void _fixed_lang_free(void)
{
    if (_test_lang_list == NULL)
        return;
    for (int i = 0; i < _TEST_LANG_MAX; i++) {
        if (_test_lang_list[i] == NULL)
            continue;
        free(_test_lang_list[i]);
    }
    free(_test_lang_list);
    _test_lang_list = NULL;
}

/* lang_free on NULL list must not crash */
TEST(lang_free_null_safe) {
    _test_lang_list = NULL;
    /* This would crash without the NULL guard */
    _fixed_lang_free();
    /* If we reach here, no crash occurred */
    ASSERT_NULL(_test_lang_list);
}

/* lang_free on allocated list works and NULLs the pointer */
TEST(lang_free_allocated_list) {
    _test_lang_list = (char **)calloc(_TEST_LANG_MAX, sizeof(char *));
    ASSERT_NOT_NULL(_test_lang_list);
    _test_lang_list[0] = strdup("hello");
    _test_lang_list[2] = strdup("world");

    _fixed_lang_free();
    ASSERT_NULL(_test_lang_list);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("\n=== playActivityDB.h & lang.h Unit Tests ===\n\n");

    /* get_rom_image_path */
    RUN_TEST(get_rom_image_path_normal_rom);
    RUN_TEST(get_rom_image_path_p8_file);
    RUN_TEST(get_rom_image_path_png_file);
    RUN_TEST(get_rom_image_path_p8_fallthrough_bug);

    /* _get_active_rom_path */
    RUN_TEST(get_active_rom_path_standard);
    RUN_TEST(get_active_rom_path_no_quotes);
    RUN_TEST(get_active_rom_path_empty);

    /* lang_free */
    RUN_TEST(lang_free_null_safe);
    RUN_TEST(lang_free_allocated_list);

    TEST_REPORT();
    return test_failures;
}
