/**
 * @file test_romscreen_find.c
 * @brief Tests for findRomScreenPaths() from
 *        src/gameSwitcher/gs_romscreen_find.h (production header)
 *
 * The GameSwitcher shows a game's own capture, ROM_SCREENS_DIR/<FNV hash
 * of the ROM path>.png, first, then its artwork, else nothing.
 *
 * Build and run: make -f Makefile.unit test_romscreen_find
 */

#define ROM_SCREENS_DIR "/tmp/onion_test_romscreens"

#include "../src/gameSwitcher/gs_romscreen_find.h"
#include "onion_test.h"

#include <sys/stat.h>
#include <unistd.h>

#define ART_DIR "/tmp/onion_test_romscreens_art"

static void touch(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp != NULL)
        fclose(fp);
}

static void capture_path(const char *rompath, char *out, size_t size)
{
    snprintf(out, size, ROM_SCREENS_DIR "/%" PRIu32 ".png",
             FNV1A_Pippip_Yurii(rompath, strlen(rompath)));
}

TEST(romscreen_enum_values)
{
    ASSERT_EQ(ROM_SCREEN_NONE, 0);
    ASSERT_EQ(ROM_SCREEN_STATE, 1);
    ASSERT_EQ(ROM_SCREEN_HASH, 2);
    ASSERT_EQ(ROM_SCREEN_ARTWORK, 3);
}

TEST(romscreen_hash_found)
{
    const char *rom = "/mnt/SDCARD/Roms/GBA/Pokemon.gba";
    char expected[512], picture[512];
    capture_path(rom, expected, sizeof(expected));
    touch(expected);
    ASSERT_EQ(ROM_SCREEN_HASH, findRomScreenPaths(rom, ART_DIR "/Pokemon.png", picture, sizeof(picture)));
    ASSERT_STREQ(expected, picture);
    remove(expected);
}

TEST(romscreen_artwork_found)
{
    char picture[512];
    touch(ART_DIR "/Zelda.png");
    ASSERT_EQ(ROM_SCREEN_ARTWORK,
              findRomScreenPaths("/mnt/SDCARD/Roms/SNES/Zelda.sfc", ART_DIR "/Zelda.png", picture, sizeof(picture)));
    ASSERT_STREQ(ART_DIR "/Zelda.png", picture);
    remove(ART_DIR "/Zelda.png");
}

TEST(romscreen_none_found)
{
    char picture[512];
    ASSERT_EQ(ROM_SCREEN_NONE,
              findRomScreenPaths("/mnt/SDCARD/Roms/GBA/Unknown.gba", ART_DIR "/Unknown.png", picture, sizeof(picture)));
    /* The last path tried is left in the buffer */
    ASSERT_STREQ(ART_DIR "/Unknown.png", picture);
}

TEST(romscreen_hash_takes_priority_over_artwork)
{
    const char *rom = "/mnt/SDCARD/Roms/GBA/Game.gba";
    char capture[512], picture[512];
    capture_path(rom, capture, sizeof(capture));
    touch(capture);
    touch(ART_DIR "/Game.png");
    ASSERT_EQ(ROM_SCREEN_HASH, findRomScreenPaths(rom, ART_DIR "/Game.png", picture, sizeof(picture)));
    ASSERT_STREQ(capture, picture);
    remove(capture);
    remove(ART_DIR "/Game.png");
}

/* The capture name depends on the ROM path only. */
TEST(romscreen_different_roms_different_captures)
{
    char a[512], b[512];
    capture_path("/mnt/SDCARD/Roms/GBA/A.gba", a, sizeof(a));
    capture_path("/mnt/SDCARD/Roms/GBA/B.gba", b, sizeof(b));
    ASSERT_STRNE(a, b);
}

TEST(romscreen_no_output_buffer)
{
    char picture[4] = "abc";
    ASSERT_EQ(ROM_SCREEN_NONE, findRomScreenPaths("/r", "/i", NULL, 16));
    ASSERT_EQ(ROM_SCREEN_NONE, findRomScreenPaths("/r", "/i", picture, 0));
    ASSERT_STREQ("abc", picture);
}

/* A small buffer truncates instead of overflowing. */
TEST(romscreen_small_buffer_truncates)
{
    char picture[8];
    findRomScreenPaths("/mnt/SDCARD/Roms/GBA/Game.gba", "/a/very/long/artwork/path.png", picture, sizeof(picture));
    ASSERT_EQ(7, (int)strlen(picture));
}

int main(void)
{
    printf("\n=== gs_romscreen_find.h Unit Tests ===\n\n");
    mkdir(ROM_SCREENS_DIR, 0755);
    mkdir(ART_DIR, 0755);

    RUN_TEST(romscreen_enum_values);
    RUN_TEST(romscreen_hash_found);
    RUN_TEST(romscreen_artwork_found);
    RUN_TEST(romscreen_none_found);
    RUN_TEST(romscreen_hash_takes_priority_over_artwork);
    RUN_TEST(romscreen_different_roms_different_captures);
    RUN_TEST(romscreen_no_output_buffer);
    RUN_TEST(romscreen_small_buffer_truncates);

    rmdir(ROM_SCREENS_DIR);
    rmdir(ART_DIR);
    TEST_REPORT();
    return test_failures;
}
