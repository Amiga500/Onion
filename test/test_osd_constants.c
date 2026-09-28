/**
 * @file test_osd_constants.c
 * @brief Unit tests for OSD color constants and basic OSD math
 *
 * Tests the color constant definitions from osd.h to ensure they are
 * correct and self-consistent (roundtrip through uintToColor/colorToUint).
 * Also validates the CHR_WIDTH and CHR_HEIGHT display metrics.
 *
 * Build and run: make -f Makefile.unit test_osd_constants
 */

#include "onion_test.h"
#include <stdint.h>

/* Production code: the OSD constants from system/osd.h and the color
 * helpers from theme/color.h. osd.h compiles against the test SDL stub;
 * its drawing code only needs SDL_FreeSurface to link. */
#include "system/osd.h"
#include "theme/color.h"

void SDL_FreeSurface(SDL_Surface *surface) { (void)surface; }

/* ==== Tests: OSD character dimensions ==== */

TEST(chr_width_value) {
    ASSERT_EQ(CHR_WIDTH, 16);
}

TEST(chr_height_value) {
    ASSERT_EQ(CHR_HEIGHT, 20);
}

TEST(chr_dimensions_positive) {
    ASSERT_TRUE(CHR_WIDTH > 0);
    ASSERT_TRUE(CHR_HEIGHT > 0);
}

/* ==== Tests: OSD color constant values ==== */

TEST(osd_white_is_ffffff) {
    ASSERT_EQ(OSD_COLOR_WHITE, 0x00FFFFFF);
}

TEST(osd_red_channels) {
    SDL_Color c = uintToColor(OSD_COLOR_RED);
    ASSERT_EQ(c.r, 0xF8);
    ASSERT_EQ(c.g, 0x03);
    ASSERT_EQ(c.b, 0x55);
}

TEST(osd_green_channels) {
    SDL_Color c = uintToColor(OSD_COLOR_GREEN);
    ASSERT_EQ(c.r, 0x1C);
    ASSERT_EQ(c.g, 0xD5);
    ASSERT_EQ(c.b, 0x77);
}

TEST(osd_cyan_channels) {
    SDL_Color c = uintToColor(OSD_COLOR_CYAN);
    ASSERT_EQ(c.r, 0x00);
    ASSERT_EQ(c.g, 0xFF);
    ASSERT_EQ(c.b, 0xD7);
}

TEST(osd_yellow_channels) {
    SDL_Color c = uintToColor(OSD_COLOR_YELLOW);
    ASSERT_EQ(c.r, 0xDC);
    ASSERT_EQ(c.g, 0xFF);
    ASSERT_EQ(c.b, 0x62);
}

/* ==== Tests: OSD color aliases ==== */

TEST(brightness_uses_white) {
    ASSERT_EQ(OSD_BRIGHTNESS_COLOR, OSD_COLOR_WHITE);
}

TEST(volume_uses_green) {
    ASSERT_EQ(OSD_VOLUME_COLOR, OSD_COLOR_GREEN);
}

TEST(mute_uses_red) {
    ASSERT_EQ(OSD_MUTE_ON_COLOR, OSD_COLOR_RED);
}

/* ==== Tests: roundtrip consistency ==== */

TEST(white_roundtrip) {
    SDL_Color c = uintToColor(OSD_COLOR_WHITE);
    ASSERT_EQ(colorToUint(c), OSD_COLOR_WHITE);
}

TEST(red_roundtrip) {
    SDL_Color c = uintToColor(OSD_COLOR_RED);
    ASSERT_EQ(colorToUint(c), OSD_COLOR_RED);
}

TEST(green_roundtrip) {
    SDL_Color c = uintToColor(OSD_COLOR_GREEN);
    ASSERT_EQ(colorToUint(c), OSD_COLOR_GREEN);
}

TEST(cyan_roundtrip) {
    SDL_Color c = uintToColor(OSD_COLOR_CYAN);
    ASSERT_EQ(colorToUint(c), OSD_COLOR_CYAN);
}

TEST(yellow_roundtrip) {
    SDL_Color c = uintToColor(OSD_COLOR_YELLOW);
    ASSERT_EQ(colorToUint(c), OSD_COLOR_YELLOW);
}

/* ==== Tests: all OSD colors are distinct ==== */

TEST(all_colors_distinct) {
    Uint32 colors[] = {
        OSD_COLOR_WHITE, OSD_COLOR_RED, OSD_COLOR_GREEN,
        OSD_COLOR_CYAN, OSD_COLOR_YELLOW
    };
    int n = sizeof(colors) / sizeof(colors[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            ASSERT_NE(colors[i], colors[j]);
        }
    }
}

/* ==== Tests: no alpha bits set in OSD colors ==== */

TEST(white_no_alpha) {
    ASSERT_EQ(OSD_COLOR_WHITE & 0xFF000000, 0);
}

TEST(red_no_alpha) {
    ASSERT_EQ(OSD_COLOR_RED & 0xFF000000, 0);
}

TEST(green_no_alpha) {
    ASSERT_EQ(OSD_COLOR_GREEN & 0xFF000000, 0);
}

TEST(cyan_no_alpha) {
    ASSERT_EQ(OSD_COLOR_CYAN & 0xFF000000, 0);
}

TEST(yellow_no_alpha) {
    ASSERT_EQ(OSD_COLOR_YELLOW & 0xFF000000, 0);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== osd.h Constants & Color Math Unit Tests ===\n\n");

    /* Character dimensions */
    RUN_TEST(chr_width_value);
    RUN_TEST(chr_height_value);
    RUN_TEST(chr_dimensions_positive);

    /* Color values */
    RUN_TEST(osd_white_is_ffffff);
    RUN_TEST(osd_red_channels);
    RUN_TEST(osd_green_channels);
    RUN_TEST(osd_cyan_channels);
    RUN_TEST(osd_yellow_channels);

    /* Aliases */
    RUN_TEST(brightness_uses_white);
    RUN_TEST(volume_uses_green);
    RUN_TEST(mute_uses_red);

    /* Roundtrips */
    RUN_TEST(white_roundtrip);
    RUN_TEST(red_roundtrip);
    RUN_TEST(green_roundtrip);
    RUN_TEST(cyan_roundtrip);
    RUN_TEST(yellow_roundtrip);

    /* Distinct colors */
    RUN_TEST(all_colors_distinct);

    /* No alpha bits */
    RUN_TEST(white_no_alpha);
    RUN_TEST(red_no_alpha);
    RUN_TEST(green_no_alpha);
    RUN_TEST(cyan_no_alpha);
    RUN_TEST(yellow_no_alpha);

    TEST_REPORT();
    return test_failures;
}
