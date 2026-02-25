/**
 * @file test_rom_image_path.c
 * @brief Unit tests for get_rom_image_path() from playActivityDB.h
 *
 * Tests the path construction logic that maps a ROM file path
 * to its corresponding image (thumbnail) path. The function handles:
 * 1. .p8 files → direct /mnt/SDCARD/Roms/<path>
 * 2. .png files → direct /mnt/SDCARD/Roms/<path>
 * 3. Other extensions → /mnt/SDCARD/Roms/<folder>/Imgs/<name_no_ext>.png
 *
 * Build and run: make -f Makefile.unit test_rom_image_path
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STR_MAX 256

/* ---- Inline the function under test ---- */

static void get_rom_image_path(char *rom_file, char *out_image_path)
{
    if (str_endsWith(rom_file, ".p8") || str_endsWith(rom_file, ".png")) {
        snprintf(out_image_path, STR_MAX - 1, "/mnt/SDCARD/Roms/%s", rom_file);
        return;
    }

    char *clean_rom_name = file_removeExtension(file_basename(rom_file));
    if (clean_rom_name == NULL)
        return;
    char rom_file_copy[PATH_MAX];
    strncpy(rom_file_copy, rom_file, sizeof(rom_file_copy) - 1);
    rom_file_copy[sizeof(rom_file_copy) - 1] = '\0';
    char *rom_folder = strtok(rom_file_copy, "/");
    if (rom_folder == NULL)
        rom_folder = rom_file_copy;

    snprintf(out_image_path, STR_MAX - 1, "/mnt/SDCARD/Roms/%s/Imgs/%s.png",
             rom_folder, clean_rom_name);
    free(clean_rom_name);
}

/* ==== Tests: .p8 (PICO-8) files ==== */

TEST(rom_image_p8_simple) {
    char out[STR_MAX] = {0};
    get_rom_image_path("PICO/celeste.p8", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/PICO/celeste.p8");
}

TEST(rom_image_p8_with_spaces) {
    char out[STR_MAX] = {0};
    get_rom_image_path("PICO/my game.p8", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/PICO/my game.p8");
}

/* ==== Tests: .png files ==== */

TEST(rom_image_png_direct) {
    char out[STR_MAX] = {0};
    get_rom_image_path("PICO/game_screenshot.png", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/PICO/game_screenshot.png");
}

/* ==== Tests: standard ROM files → Imgs/ subfolder ==== */

TEST(rom_image_gba_standard) {
    char out[STR_MAX] = {0};
    get_rom_image_path("GBA/Pokemon Fire Red.gba", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/GBA/Imgs/Pokemon Fire Red.png");
}

TEST(rom_image_snes_standard) {
    char out[STR_MAX] = {0};
    get_rom_image_path("SFC/Super Mario World.sfc", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/SFC/Imgs/Super Mario World.png");
}

TEST(rom_image_nes_standard) {
    char out[STR_MAX] = {0};
    get_rom_image_path("FC/Mega Man 2.nes", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/FC/Imgs/Mega Man 2.png");
}

TEST(rom_image_genesis_standard) {
    char out[STR_MAX] = {0};
    get_rom_image_path("MD/Sonic The Hedgehog.md", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/MD/Imgs/Sonic The Hedgehog.png");
}

TEST(rom_image_ps1_bin) {
    char out[STR_MAX] = {0};
    get_rom_image_path("PS/Final Fantasy VII (Disc 1).bin", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/PS/Imgs/Final Fantasy VII (Disc 1).png");
}

/* ==== Tests: folder extraction logic ==== */

TEST(rom_image_folder_extraction) {
    /* strtok splits on '/' and returns the first part */
    char out[STR_MAX] = {0};
    get_rom_image_path("GB/Tetris.gb", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/GB/Imgs/Tetris.png");
}

/* ==== Tests: special characters ==== */

TEST(rom_image_parentheses_in_name) {
    char out[STR_MAX] = {0};
    get_rom_image_path("GBA/[BIOS] GBA (World).gba", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/GBA/Imgs/[BIOS] GBA (World).png");
}

TEST(rom_image_multiple_dots) {
    char out[STR_MAX] = {0};
    get_rom_image_path("GBA/Game v1.2.gba", out);
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/GBA/Imgs/Game v1.2.png");
}

/* ==== Tests: edge cases ==== */

TEST(rom_image_no_folder_prefix) {
    /* When there's no '/' at all, strtok returns the whole string */
    char out[STR_MAX] = {0};
    get_rom_image_path("game.gba", out);
    /* rom_folder = "game.gba" (strtok on "/" with no "/" returns whole string) */
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/game.gba/Imgs/game.png");
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== playActivityDB get_rom_image_path Unit Tests ===\n\n");

    /* .p8 files */
    RUN_TEST(rom_image_p8_simple);
    RUN_TEST(rom_image_p8_with_spaces);

    /* .png files */
    RUN_TEST(rom_image_png_direct);

    /* Standard ROM files */
    RUN_TEST(rom_image_gba_standard);
    RUN_TEST(rom_image_snes_standard);
    RUN_TEST(rom_image_nes_standard);
    RUN_TEST(rom_image_genesis_standard);
    RUN_TEST(rom_image_ps1_bin);

    /* Folder extraction */
    RUN_TEST(rom_image_folder_extraction);

    /* Special characters */
    RUN_TEST(rom_image_parentheses_in_name);
    RUN_TEST(rom_image_multiple_dots);

    /* Edge cases */
    RUN_TEST(rom_image_no_folder_prefix);

    TEST_REPORT();
    return test_failures;
}
