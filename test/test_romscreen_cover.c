/**
 * @file test_romscreen_cover.c
 * @brief The GameSwitcher picture as a missing cover
 *        (src/common/system/romscreen_cover.h)
 *
 * Path: Roms/<console>/Imgs/<ROM name>.png, whatever subfolder the ROM is
 * in; none for PICO-8 or a path outside Roms/<console>/. Picture: black
 * borders cut off, shrunk to 250 wide (at most 360 high), upright.
 *
 * Build and run: make -f Makefile.unit test_romscreen_cover
 */

#include "onion_test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system/romscreen_cover.h"

#define RED 0xFFFF0000u
#define BLUE 0xFF0000FFu
#define BLACK 0xFF000000u

TEST(path_from_recent_list_rom_path)
{
    char out[512];
    ASSERT_TRUE(romscreen_cover_path("/mnt/SDCARD/Emu/GB/../../Roms/GB/Tetris (World).gb", out, sizeof(out)));
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/GB/Imgs/Tetris (World).png");
}

TEST(path_of_rom_in_subfolder_uses_console_imgs)
{
    char out[512];
    ASSERT_TRUE(romscreen_cover_path("/mnt/SDCARD/Roms/GBA/Zelda/Minish Cap.gba", out, sizeof(out)));
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/GBA/Imgs/Minish Cap.png");
    /* a subfolder named Roms is still inside the console */
    ASSERT_TRUE(romscreen_cover_path("/mnt/SDCARD/Roms/GB/Roms/Tetris.gb", out, sizeof(out)));
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/GB/Imgs/Tetris.png");
}

TEST(path_keeps_dots_in_name)
{
    char out[512];
    ASSERT_TRUE(romscreen_cover_path("/mnt/SDCARD/Roms/MD/Sonic v1.1.zip", out, sizeof(out)));
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/MD/Imgs/Sonic v1.1.png");
}

TEST(path_of_port_shortcut)
{
    char out[512];
    ASSERT_TRUE(romscreen_cover_path("/mnt/SDCARD/Roms/PORTS/Shortcuts/Doom.notfound", out, sizeof(out)));
    ASSERT_STREQ(out, "/mnt/SDCARD/Roms/PORTS/Imgs/Doom.png");
}

TEST(no_path_for_pico8_or_outside_roms)
{
    char out[512];
    ASSERT_FALSE(romscreen_cover_path("/mnt/SDCARD/Roms/PICO/celeste.p8", out, sizeof(out)));
    ASSERT_FALSE(romscreen_cover_path("/mnt/SDCARD/Roms/PICO/celeste.p8.png", out, sizeof(out)));
    ASSERT_FALSE(romscreen_cover_path("/mnt/SDCARD/App/Foo/launch.sh", out, sizeof(out)));
    ASSERT_FALSE(romscreen_cover_path("/mnt/SDCARD/Roms/game.gb", out, sizeof(out)));
    ASSERT_FALSE(romscreen_cover_path(NULL, out, sizeof(out)));
    ASSERT_FALSE(romscreen_cover_path("/mnt/SDCARD/Roms/GB/x.gb", out, 10)); /* too small */
}

TEST(full_frame_shrinks_to_250_wide)
{
    int w = 640, h = 480;
    uint32_t *frame = malloc((size_t)w * h * sizeof(uint32_t));
    for (int i = 0; i < w * h; i++)
        frame[i] = RED;
    int cw = 0, ch = 0;
    uint32_t *cover = romscreen_cover_make(frame, w, h, w, false, &cw, &ch);
    ASSERT_NOT_NULL(cover);
    ASSERT_EQ(cw, 250);
    ASSERT_EQ(ch, 187);
    ASSERT_EQ(cover[0], RED);
    ASSERT_EQ(cover[cw * ch - 1], RED);
    free(cover);
    free(frame);
}

/* A Game Boy screen (160x144 at 3x = 480x432) centred in a black 640x480
 * frame: the cover is the game only. */
TEST(black_borders_are_cut_off)
{
    int w = 640, h = 480;
    uint32_t *frame = malloc((size_t)w * h * sizeof(uint32_t));
    for (int i = 0; i < w * h; i++)
        frame[i] = BLACK;
    for (int y = 24; y < 456; y++)
        for (int x = 80; x < 560; x++)
            frame[y * w + x] = BLUE;
    int cw = 0, ch = 0;
    uint32_t *cover = romscreen_cover_make(frame, w, h, w, false, &cw, &ch);
    ASSERT_NOT_NULL(cover);
    ASSERT_EQ(cw, 250);
    ASSERT_EQ(ch, 225); /* 480x432 -> 250x225 */
    ASSERT_EQ(cover[0], BLUE);
    ASSERT_EQ(cover[cw * ch - 1], BLUE);
    free(cover);
    free(frame);
}

/* A mostly dark scene with a small lit spot is not cropped to the spot. */
TEST(dark_scene_is_kept_whole)
{
    int w = 640, h = 480;
    uint32_t *frame = malloc((size_t)w * h * sizeof(uint32_t));
    for (int i = 0; i < w * h; i++)
        frame[i] = BLACK;
    for (int y = 200; y < 260; y++)
        for (int x = 300; x < 340; x++)
            frame[y * w + x] = RED;
    int cw = 0, ch = 0;
    uint32_t *cover = romscreen_cover_make(frame, w, h, w, false, &cw, &ch);
    ASSERT_NOT_NULL(cover);
    ASSERT_EQ(cw, 250);
    ASSERT_EQ(ch, 187);
    free(cover);
    free(frame);
}

/* keymon's frame is upside down: the cover comes out upright. */
TEST(rotated_frame_comes_out_upright)
{
    int w = 640, h = 480;
    uint32_t *frame = malloc((size_t)w * h * sizeof(uint32_t));
    /* As seen: top half red, bottom half blue. Stored rotated 180 degrees. */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            frame[(h - 1 - y) * w + (w - 1 - x)] = y < h / 2 ? RED : BLUE;
    int cw = 0, ch = 0;
    uint32_t *cover = romscreen_cover_make(frame, w, h, w, true, &cw, &ch);
    ASSERT_NOT_NULL(cover);
    ASSERT_EQ(cover[0], RED);
    ASSERT_EQ(cover[cw * ch - 1], BLUE);
    free(cover);
    free(frame);
}

/* Mini Flip / 752x560 and a tall picture stay within 250x360. */
TEST(size_fits_cover_box)
{
    int cw, ch;
    romscreen_cover_size(752, 560, &cw, &ch);
    ASSERT_EQ(cw, 250);
    ASSERT_EQ(ch, 186);
    romscreen_cover_size(200, 600, &cw, &ch);
    ASSERT_EQ(ch, 360);
    ASSERT_EQ(cw, 120);
    romscreen_cover_size(160, 144, &cw, &ch); /* never enlarged */
    ASSERT_EQ(cw, 160);
    ASSERT_EQ(ch, 144);
}

int main(void)
{
    printf("\n=== romscreen cover Unit Tests ===\n\n");
    RUN_TEST(path_from_recent_list_rom_path);
    RUN_TEST(path_of_rom_in_subfolder_uses_console_imgs);
    RUN_TEST(path_keeps_dots_in_name);
    RUN_TEST(path_of_port_shortcut);
    RUN_TEST(no_path_for_pico8_or_outside_roms);
    RUN_TEST(full_frame_shrinks_to_250_wide);
    RUN_TEST(black_borders_are_cut_off);
    RUN_TEST(dark_scene_is_kept_whole);
    RUN_TEST(rotated_frame_comes_out_upright);
    RUN_TEST(size_fits_cover_box);
    TEST_REPORT();
    return test_failures;
}
