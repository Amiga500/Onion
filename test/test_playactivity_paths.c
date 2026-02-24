/**
 * @file test_playactivity_paths.c
 * @brief Unit tests for __ensure_rel_path() path normalization.
 *
 * Tests the logic that converts various ROM path formats into
 * relative paths under the Roms/ folder. The function handles:
 * 1. Paths already relative (via file_path_relative_to, skipped in tests)
 * 2. Paths containing "../../Roms/" prefix (str_split fallback)
 * 3. Paths containing "/mnt/SDCARD/Roms/" prefix (str_replace fallback)
 * 4. Paths that match none of the above (returned as-is)
 *
 * Build and run: make -f Makefile.unit test_playactivity_paths
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Stub file_path_relative_to to always return false ---- */
/* In unit tests, we can't use realpath() on nonexistent device paths.
 * Returning false forces __ensure_rel_path to use its fallback logic,
 * which is the code we actually want to test. */
static bool file_path_relative_to(char *path_out, const char *dir_from, const char *file_to)
{
    (void)dir_from;
    (void)file_to;
    path_out[0] = '\0';
    return false;
}

/* ---- Constants from playActivityDB.h ---- */
#define ROMS_FOLDER "/mnt/SDCARD/Roms"

/* ---- Inline the function under test ---- */

static void __ensure_rel_path(char *rel_path, const char *rom_path)
{
    if (!file_path_relative_to(rel_path, ROMS_FOLDER, rom_path)) {
        if (strstr(rom_path, "../../Roms/") != NULL) {
            char *dup = strdup((const char *)rom_path);
            strncpy(rel_path, str_split(dup, "../../Roms/"), PATH_MAX - 1);
            rel_path[PATH_MAX - 1] = '\0';
            free(dup);
        }
        else {
            char *temp = strdup((const char *)rom_path);
            char *replaced = str_replace(temp, "/mnt/SDCARD/Roms/", "");
            free(temp);
            strncpy(rel_path, replaced ? replaced : (const char *)rom_path, PATH_MAX - 1);
            rel_path[PATH_MAX - 1] = '\0';
            free(replaced);
        }
    }
}

/* ---- Tests: ../../Roms/ prefix paths ---- */

TEST(ensure_rel_path_dotdot_roms_simple) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "../../Roms/GBA/game.gba");
    ASSERT_STREQ(rel, "GBA/game.gba");
}

TEST(ensure_rel_path_dotdot_roms_nested) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "/some/prefix/../../Roms/SNES/subfolder/rom.sfc");
    ASSERT_STREQ(rel, "SNES/subfolder/rom.sfc");
}

TEST(ensure_rel_path_dotdot_roms_deep) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "../../Roms/PS/Final Fantasy VII (Disc 1).bin");
    ASSERT_STREQ(rel, "PS/Final Fantasy VII (Disc 1).bin");
}

/* ---- Tests: /mnt/SDCARD/Roms/ prefix paths ---- */

TEST(ensure_rel_path_sdcard_roms_simple) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "/mnt/SDCARD/Roms/GBA/game.gba");
    ASSERT_STREQ(rel, "GBA/game.gba");
}

TEST(ensure_rel_path_sdcard_roms_nested) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "/mnt/SDCARD/Roms/SNES/RPGs/earthbound.sfc");
    ASSERT_STREQ(rel, "SNES/RPGs/earthbound.sfc");
}

TEST(ensure_rel_path_sdcard_roms_with_spaces) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "/mnt/SDCARD/Roms/GBA/Pokemon - Fire Red (USA).gba");
    ASSERT_STREQ(rel, "GBA/Pokemon - Fire Red (USA).gba");
}

TEST(ensure_rel_path_sdcard_roms_pico8) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "/mnt/SDCARD/Roms/PICO/celeste.p8");
    ASSERT_STREQ(rel, "PICO/celeste.p8");
}

/* ---- Tests: no matching prefix (passthrough) ---- */

TEST(ensure_rel_path_no_prefix_passthrough) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "GBA/game.gba");
    /* No prefix match → str_replace returns NULL → original path used */
    ASSERT_STREQ(rel, "GBA/game.gba");
}

TEST(ensure_rel_path_unknown_prefix) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "/opt/roms/GBA/game.gba");
    /* No prefix match → returned as-is */
    ASSERT_STREQ(rel, "/opt/roms/GBA/game.gba");
}

TEST(ensure_rel_path_empty_string) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "");
    /* Empty input → str_replace returns NULL → empty string used */
    ASSERT_STREQ(rel, "");
}

/* ---- Tests: edge cases ---- */

TEST(ensure_rel_path_just_roms_folder) {
    /* Exactly "/mnt/SDCARD/Roms/" with nothing after → empty relative path */
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "/mnt/SDCARD/Roms/");
    ASSERT_STREQ(rel, "");
}

TEST(ensure_rel_path_special_characters) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "/mnt/SDCARD/Roms/GBA/[BIOS] GBA (World).gba");
    ASSERT_STREQ(rel, "GBA/[BIOS] GBA (World).gba");
}

TEST(ensure_rel_path_japanese_filename) {
    char rel[PATH_MAX];
    __ensure_rel_path(rel, "/mnt/SDCARD/Roms/FC/スーパーマリオ.nes");
    ASSERT_STREQ(rel, "FC/スーパーマリオ.nes");
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== playActivityDB __ensure_rel_path Unit Tests ===\n\n");

    /* ../../Roms/ prefix */
    RUN_TEST(ensure_rel_path_dotdot_roms_simple);
    RUN_TEST(ensure_rel_path_dotdot_roms_nested);
    RUN_TEST(ensure_rel_path_dotdot_roms_deep);

    /* /mnt/SDCARD/Roms/ prefix */
    RUN_TEST(ensure_rel_path_sdcard_roms_simple);
    RUN_TEST(ensure_rel_path_sdcard_roms_nested);
    RUN_TEST(ensure_rel_path_sdcard_roms_with_spaces);
    RUN_TEST(ensure_rel_path_sdcard_roms_pico8);

    /* no matching prefix */
    RUN_TEST(ensure_rel_path_no_prefix_passthrough);
    RUN_TEST(ensure_rel_path_unknown_prefix);
    RUN_TEST(ensure_rel_path_empty_string);

    /* edge cases */
    RUN_TEST(ensure_rel_path_just_roms_folder);
    RUN_TEST(ensure_rel_path_special_characters);
    RUN_TEST(ensure_rel_path_japanese_filename);

    TEST_REPORT();
    return test_failures;
}
