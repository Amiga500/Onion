/**
 * @file test_rom_image_path.c
 * @brief Unit tests for get_rom_image_path() from playActivityDB.h
 *        (production header)
 *
 * Maps a ROM path relative to Roms/ to the picture Play Activity shows:
 * 1. .png (PICO-8 .p8.png carts) -> the cart itself
 * 2. .p8 -> <cart>.png next to it (PICO-8 imgpath is the ROM folder),
 *    else the Imgs/ path below
 * 3. Other extensions -> Roms/<folder>/Imgs/<name_no_ext>.png
 *
 * Build and run: make -f Makefile.unit test_rom_image_path
 */

#define ROMS_FOLDER "/tmp/onion_test_rom_image_path/Roms"

#include "onion_test.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Production code. sqlite3 calls link against stubs/sqlite3_stub.c; the
 * function under test never touches the database. */
#include "utils/file.h"
#include "utils/log.h"
#include "utils/str.h"
#include "../src/playActivity/playActivityDB.h"

#define TEST_ROOT "/tmp/onion_test_rom_image_path"

static void touch(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp != NULL)
        fclose(fp);
}

/* ==== Tests: .p8 (PICO-8) files ==== */

/* A .p8 cart is text: never return the cart itself as the picture. */
TEST(rom_image_p8_without_picture_uses_imgs) {
    char out[STR_MAX] = {0};
    get_rom_image_path("PICO/celeste.p8", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/PICO/Imgs/celeste.png");
}

TEST(rom_image_p8_picture_next_to_cart) {
    char out[STR_MAX] = {0};
    touch(ROMS_FOLDER "/PICO/celeste.png");
    get_rom_image_path("PICO/celeste.p8", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/PICO/celeste.png");
    remove(ROMS_FOLDER "/PICO/celeste.png");
}

TEST(rom_image_p8_with_spaces) {
    char out[STR_MAX] = {0};
    touch(ROMS_FOLDER "/PICO/my game.png");
    get_rom_image_path("PICO/my game.p8", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/PICO/my game.png");
    remove(ROMS_FOLDER "/PICO/my game.png");
}

/* ==== Tests: .png files ==== */

/* A .p8.png cart is its own picture. */
TEST(rom_image_p8_png_cart) {
    char out[STR_MAX] = {0};
    get_rom_image_path("PICO/celeste.p8.png", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/PICO/celeste.p8.png");
}

TEST(rom_image_png_direct) {
    char out[STR_MAX] = {0};
    get_rom_image_path("PICO/game_screenshot.png", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/PICO/game_screenshot.png");
}

/* ==== Tests: standard ROM files → Imgs/ subfolder ==== */

TEST(rom_image_gba_standard) {
    char out[STR_MAX] = {0};
    get_rom_image_path("GBA/Pokemon Fire Red.gba", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/GBA/Imgs/Pokemon Fire Red.png");
}

TEST(rom_image_snes_standard) {
    char out[STR_MAX] = {0};
    get_rom_image_path("SFC/Super Mario World.sfc", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/SFC/Imgs/Super Mario World.png");
}

TEST(rom_image_nes_standard) {
    char out[STR_MAX] = {0};
    get_rom_image_path("FC/Mega Man 2.nes", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/FC/Imgs/Mega Man 2.png");
}

TEST(rom_image_genesis_standard) {
    char out[STR_MAX] = {0};
    get_rom_image_path("MD/Sonic The Hedgehog.md", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/MD/Imgs/Sonic The Hedgehog.png");
}

TEST(rom_image_ps1_bin) {
    char out[STR_MAX] = {0};
    get_rom_image_path("PS/Final Fantasy VII (Disc 1).bin", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/PS/Imgs/Final Fantasy VII (Disc 1).png");
}

/* ==== Tests: folder extraction logic ==== */

TEST(rom_image_folder_extraction) {
    /* strtok splits on '/' and returns the first part */
    char out[STR_MAX] = {0};
    get_rom_image_path("GB/Tetris.gb", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/GB/Imgs/Tetris.png");
}

/* ==== Tests: special characters ==== */

TEST(rom_image_parentheses_in_name) {
    char out[STR_MAX] = {0};
    get_rom_image_path("GBA/[BIOS] GBA (World).gba", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/GBA/Imgs/[BIOS] GBA (World).png");
}

TEST(rom_image_multiple_dots) {
    char out[STR_MAX] = {0};
    get_rom_image_path("GBA/Game v1.2.gba", out);
    ASSERT_STREQ(out, ROMS_FOLDER "/GBA/Imgs/Game v1.2.png");
}

/* ==== Tests: edge cases ==== */

TEST(rom_image_no_folder_prefix) {
    /* When there's no '/' at all, strtok returns the whole string */
    char out[STR_MAX] = {0};
    get_rom_image_path("game.gba", out);
    /* rom_folder = "game.gba" (strtok on "/" with no "/" returns whole string) */
    ASSERT_STREQ(out, ROMS_FOLDER "/game.gba/Imgs/game.png");
}

/* strtok used to cut the caller's string at the first '/', corrupting
 * rom->file_path in play_activity_find_all(). */
TEST(rom_image_preserves_input) {
    char rom_file[STR_MAX] = "GBA/game.gba";
    char out[STR_MAX] = {0};
    get_rom_image_path(rom_file, out);
    ASSERT_STREQ(rom_file, "GBA/game.gba");
    ASSERT_STREQ(out, ROMS_FOLDER "/GBA/Imgs/game.png");
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== playActivityDB get_rom_image_path Unit Tests ===\n\n");

    mkdir(TEST_ROOT, 0755);
    mkdir(ROMS_FOLDER, 0755);
    mkdir(ROMS_FOLDER "/PICO", 0755);

    /* .p8 files */
    RUN_TEST(rom_image_p8_without_picture_uses_imgs);
    RUN_TEST(rom_image_p8_picture_next_to_cart);
    RUN_TEST(rom_image_p8_with_spaces);

    /* .png files */
    RUN_TEST(rom_image_p8_png_cart);
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
    RUN_TEST(rom_image_preserves_input);

    rmdir(ROMS_FOLDER "/PICO");
    rmdir(ROMS_FOLDER);
    rmdir(TEST_ROOT);

    TEST_REPORT();
    return test_failures;
}
