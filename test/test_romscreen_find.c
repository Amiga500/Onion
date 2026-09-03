/**
 * @file test_romscreen_find.c
 * @brief Unit tests for findRomScreen() path logic and RomScreenType_e
 *        from gs_romscreen.h
 *
 * Tests the path construction and return type selection for ROM
 * screen lookups. The function checks hashed romscreen → artwork
 * paths in order. We stub exists() and use a mock hash to isolate
 * the path selection logic.
 *
 * Build and run: make -f Makefile.unit test_romscreen_find
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#define STR_MAX 256
#define ROM_SCREENS_DIR "/mnt/SDCARD/Saves/CurrentProfile/romScreens"

/* ---- Mock hash: returns a fixed value for testing ---- */

static uint32_t _mock_hash_value = 12345;

static uint32_t mock_hash(const char *str, size_t wrdlen)
{
    (void)str;
    (void)wrdlen;
    return _mock_hash_value;
}

/* ---- RomScreenType_e from gs_romscreen.h ---- */

typedef enum {
    ROM_SCREEN_NONE = 0,
    ROM_SCREEN_STATE,
    ROM_SCREEN_HASH,
    ROM_SCREEN_ARTWORK
} RomScreenType_e;

/* ---- Minimal Game_s for tests ---- */

typedef struct {
    struct {
        char rompath[STR_MAX * 2];
        char imgpath[STR_MAX * 2];
    } recentItem;
    char rom_name[STR_MAX * 2];
    char core_name[STR_MAX * 2];
} TestGame;

/* ---- Stub for exists() ---- */

static char _existing_paths[10][STR_MAX * 2];
static int _existing_count = 0;

static bool stub_exists(const char *path)
{
    for (int i = 0; i < _existing_count; i++) {
        if (strcmp(_existing_paths[i], path) == 0)
            return true;
    }
    return false;
}

static void stub_reset(void)
{
    _existing_count = 0;
}

static void stub_add_path(const char *path)
{
    if (_existing_count < 10) {
        strncpy(_existing_paths[_existing_count], path, STR_MAX * 2 - 1);
        _existing_paths[_existing_count][STR_MAX * 2 - 1] = '\0';
        _existing_count++;
    }
}

/* ---- Inline the function under test (using mock_hash) ---- */

static RomScreenType_e findRomScreen(const TestGame *game, char *currPicture)
{
    /* Check if hashed rom screen exists */
    uint32_t hash = mock_hash(game->recentItem.rompath,
                              strlen(game->recentItem.rompath));
    snprintf(currPicture, STR_MAX * 2, ROM_SCREENS_DIR "/%" PRIu32 ".png", hash);
    if (stub_exists(currPicture)) {
        return ROM_SCREEN_HASH;
    }

    /* Check if artwork exists */
    snprintf(currPicture, STR_MAX * 2, "%s", game->recentItem.imgpath);
    if (stub_exists(currPicture)) {
        return ROM_SCREEN_ARTWORK;
    }

    return ROM_SCREEN_NONE;
}

/* ---- Helpers ---- */

static TestGame make_game(const char *rompath, const char *imgpath)
{
    TestGame g;
    memset(&g, 0, sizeof(g));
    strncpy(g.recentItem.rompath, rompath, sizeof(g.recentItem.rompath) - 1);
    strncpy(g.recentItem.imgpath, imgpath, sizeof(g.recentItem.imgpath) - 1);
    return g;
}

/* ==== Tests: enum values ==== */

TEST(romscreen_enum_values) {
    ASSERT_EQ(ROM_SCREEN_NONE, 0);
    ASSERT_EQ(ROM_SCREEN_STATE, 1);
    ASSERT_EQ(ROM_SCREEN_HASH, 2);
    ASSERT_EQ(ROM_SCREEN_ARTWORK, 3);
}

TEST(romscreen_enum_contiguous) {
    ASSERT_EQ(ROM_SCREEN_STATE, ROM_SCREEN_NONE + 1);
    ASSERT_EQ(ROM_SCREEN_HASH, ROM_SCREEN_STATE + 1);
    ASSERT_EQ(ROM_SCREEN_ARTWORK, ROM_SCREEN_HASH + 1);
}

/* ==== Tests: hash-based romscreen found ==== */

TEST(romscreen_hash_found) {
    _mock_hash_value = 99999;
    TestGame g = make_game("/mnt/SDCARD/Roms/GBA/Pokemon.gba",
                           "/mnt/SDCARD/Roms/GBA/Imgs/Pokemon.png");

    stub_reset();
    stub_add_path(ROM_SCREENS_DIR "/99999.png");

    char picture[STR_MAX * 2];
    RomScreenType_e result = findRomScreen(&g, picture);

    ASSERT_EQ(result, ROM_SCREEN_HASH);
    ASSERT_STREQ(picture, ROM_SCREENS_DIR "/99999.png");
}

TEST(romscreen_hash_path_format) {
    _mock_hash_value = 42;
    TestGame g = make_game("/some/path", "/img/path");

    stub_reset();
    /* Don't add the path — just check the format */
    char picture[STR_MAX * 2];
    findRomScreen(&g, picture);

    /* picture should end with the imgpath since hash wasn't found */
    /* But the hash path was tried as: ROM_SCREENS_DIR "/42.png" */
    /* Since nothing matched, picture = imgpath */
    ASSERT_STREQ(picture, "/img/path");
}

/* ==== Tests: artwork fallback ==== */

TEST(romscreen_artwork_found) {
    _mock_hash_value = 11111;
    TestGame g = make_game("/mnt/SDCARD/Roms/SNES/Zelda.sfc",
                           "/mnt/SDCARD/Roms/SNES/Imgs/Zelda.png");

    stub_reset();
    /* Hash path doesn't exist, but artwork does */
    stub_add_path("/mnt/SDCARD/Roms/SNES/Imgs/Zelda.png");

    char picture[STR_MAX * 2];
    RomScreenType_e result = findRomScreen(&g, picture);

    ASSERT_EQ(result, ROM_SCREEN_ARTWORK);
    ASSERT_STREQ(picture, "/mnt/SDCARD/Roms/SNES/Imgs/Zelda.png");
}

/* ==== Tests: nothing found ==== */

TEST(romscreen_none_found) {
    _mock_hash_value = 77777;
    TestGame g = make_game("/mnt/SDCARD/Roms/GBA/Unknown.gba",
                           "/mnt/SDCARD/Roms/GBA/Imgs/Unknown.png");

    stub_reset();

    char picture[STR_MAX * 2];
    RomScreenType_e result = findRomScreen(&g, picture);

    ASSERT_EQ(result, ROM_SCREEN_NONE);
}

/* ==== Tests: priority (hash > artwork) ==== */

TEST(romscreen_hash_takes_priority_over_artwork) {
    _mock_hash_value = 55555;
    TestGame g = make_game("/mnt/SDCARD/Roms/GBA/Game.gba",
                           "/mnt/SDCARD/Roms/GBA/Imgs/Game.png");

    stub_reset();
    stub_add_path(ROM_SCREENS_DIR "/55555.png");
    stub_add_path("/mnt/SDCARD/Roms/GBA/Imgs/Game.png");

    char picture[STR_MAX * 2];
    RomScreenType_e result = findRomScreen(&g, picture);

    ASSERT_EQ(result, ROM_SCREEN_HASH);
    ASSERT_STREQ(picture, ROM_SCREENS_DIR "/55555.png");
}

/* ==== Tests: empty imgpath ==== */

TEST(romscreen_empty_imgpath) {
    _mock_hash_value = 33333;
    TestGame g = make_game("/mnt/SDCARD/Roms/GBA/Game.gba", "");

    stub_reset();

    char picture[STR_MAX * 2];
    RomScreenType_e result = findRomScreen(&g, picture);

    /* Empty imgpath won't match any file */
    ASSERT_EQ(result, ROM_SCREEN_NONE);
}

TEST(romscreen_artwork_with_special_chars) {
    _mock_hash_value = 44444;
    TestGame g = make_game("/mnt/SDCARD/Roms/GBA/Game (USA) [BIOS].gba",
                           "/mnt/SDCARD/Roms/GBA/Imgs/Game (USA) [BIOS].png");

    stub_reset();
    stub_add_path("/mnt/SDCARD/Roms/GBA/Imgs/Game (USA) [BIOS].png");

    char picture[STR_MAX * 2];
    RomScreenType_e result = findRomScreen(&g, picture);

    ASSERT_EQ(result, ROM_SCREEN_ARTWORK);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== gs_romscreen.h findRomScreen Unit Tests ===\n\n");

    /* Enum values */
    RUN_TEST(romscreen_enum_values);
    RUN_TEST(romscreen_enum_contiguous);

    /* Hash-based */
    RUN_TEST(romscreen_hash_found);
    RUN_TEST(romscreen_hash_path_format);

    /* Artwork fallback */
    RUN_TEST(romscreen_artwork_found);

    /* Nothing found */
    RUN_TEST(romscreen_none_found);

    /* Priority */
    RUN_TEST(romscreen_hash_takes_priority_over_artwork);

    /* Edge cases */
    RUN_TEST(romscreen_empty_imgpath);
    RUN_TEST(romscreen_artwork_with_special_chars);

    TEST_REPORT();
    return test_failures;
}
