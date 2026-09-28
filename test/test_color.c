/**
 * @file test_color.c
 * @brief Unit tests for src/common/theme/color.h
 *
 * Tests the pure-logic color conversion functions: hex2sdl,
 * colorToUint, and uintToColor.
 *
 * SDL types are stubbed to avoid pulling in the SDL dependency.
 *
 * Build and run: make -f Makefile.unit test_color
 */

#include "onion_test.h"
#include <stdlib.h>
#include <string.h>

/* Production code: theme/color.h, with SDL_Color from the SDL stub
 * (test/stubs/SDL/SDL.h, same layout as SDL 1.2). */
#include "theme/color.h"

/* ---- Tests ---- */

/* ---- hex2sdl ---- */

TEST(hex2sdl_red) {
    SDL_Color c = hex2sdl("FF0000");
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);
}

TEST(hex2sdl_green) {
    SDL_Color c = hex2sdl("00FF00");
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 255);
    ASSERT_EQ(c.b, 0);
}

TEST(hex2sdl_blue) {
    SDL_Color c = hex2sdl("0000FF");
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 255);
}

TEST(hex2sdl_white) {
    SDL_Color c = hex2sdl("FFFFFF");
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 255);
    ASSERT_EQ(c.b, 255);
}

TEST(hex2sdl_black) {
    SDL_Color c = hex2sdl("000000");
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);
}

TEST(hex2sdl_with_hash_prefix) {
    SDL_Color c = hex2sdl("#FF8800");
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 136);
    ASSERT_EQ(c.b, 0);
}

TEST(hex2sdl_mixed_case) {
    SDL_Color c = hex2sdl("aAbBcC");
    ASSERT_EQ(c.r, 0xAA);
    ASSERT_EQ(c.g, 0xBB);
    ASSERT_EQ(c.b, 0xCC);
}

TEST(hex2sdl_arbitrary_color) {
    SDL_Color c = hex2sdl("1CD577");
    ASSERT_EQ(c.r, 0x1C);
    ASSERT_EQ(c.g, 0xD5);
    ASSERT_EQ(c.b, 0x77);
}

/* ---- colorToUint ---- */

TEST(color_to_uint_red) {
    SDL_Color c = {255, 0, 0, 0};
    ASSERT_EQ(colorToUint(c), (Uint32)0x00FF0000);
}

TEST(color_to_uint_green) {
    SDL_Color c = {0, 255, 0, 0};
    ASSERT_EQ(colorToUint(c), (Uint32)0x0000FF00);
}

TEST(color_to_uint_blue) {
    SDL_Color c = {0, 0, 255, 0};
    ASSERT_EQ(colorToUint(c), (Uint32)0x000000FF);
}

TEST(color_to_uint_white) {
    SDL_Color c = {255, 255, 255, 0};
    ASSERT_EQ(colorToUint(c), (Uint32)0x00FFFFFF);
}

TEST(color_to_uint_black) {
    SDL_Color c = {0, 0, 0, 0};
    ASSERT_EQ(colorToUint(c), (Uint32)0x00000000);
}

TEST(color_to_uint_arbitrary) {
    SDL_Color c = {0x1C, 0xD5, 0x77, 0};
    ASSERT_EQ(colorToUint(c), (Uint32)0x001CD577);
}

/* ---- uintToColor ---- */

TEST(uint_to_color_red) {
    SDL_Color c = uintToColor(0x00FF0000);
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);
    ASSERT_EQ(c.unused, 255);
}

TEST(uint_to_color_green) {
    SDL_Color c = uintToColor(0x0000FF00);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 255);
    ASSERT_EQ(c.b, 0);
}

TEST(uint_to_color_blue) {
    SDL_Color c = uintToColor(0x000000FF);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 255);
}

TEST(uint_to_color_white) {
    SDL_Color c = uintToColor(0x00FFFFFF);
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 255);
    ASSERT_EQ(c.b, 255);
}

TEST(uint_to_color_black) {
    SDL_Color c = uintToColor(0x00000000);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);
}

TEST(uint_to_color_arbitrary) {
    SDL_Color c = uintToColor(0x001CD577);
    ASSERT_EQ(c.r, 0x1C);
    ASSERT_EQ(c.g, 0xD5);
    ASSERT_EQ(c.b, 0x77);
}

/* ---- Roundtrip: hex -> SDL_Color -> Uint32 -> SDL_Color ---- */

TEST(roundtrip_hex_to_uint_to_color) {
    SDL_Color c1 = hex2sdl("#F80355");
    Uint32 u = colorToUint(c1);
    SDL_Color c2 = uintToColor(u);

    ASSERT_EQ(c1.r, c2.r);
    ASSERT_EQ(c1.g, c2.g);
    ASSERT_EQ(c1.b, c2.b);
}

TEST(roundtrip_uint_to_color_to_uint) {
    Uint32 original = 0x00DCFF62;
    SDL_Color c = uintToColor(original);
    Uint32 result = colorToUint(c);

    ASSERT_EQ(result, original);
}

/* ---- OSD color constants from osd.h ---- */

TEST(osd_color_white_roundtrip) {
    SDL_Color c = uintToColor(0x00FFFFFF);
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 255);
    ASSERT_EQ(c.b, 255);
    ASSERT_EQ(colorToUint(c), (Uint32)0x00FFFFFF);
}

TEST(osd_color_red_roundtrip) {
    SDL_Color c = uintToColor(0x00F80355);
    ASSERT_EQ(c.r, 0xF8);
    ASSERT_EQ(c.g, 0x03);
    ASSERT_EQ(c.b, 0x55);
    ASSERT_EQ(colorToUint(c), (Uint32)0x00F80355);
}

TEST(osd_color_green_roundtrip) {
    SDL_Color c = uintToColor(0x001CD577);
    ASSERT_EQ(c.r, 0x1C);
    ASSERT_EQ(c.g, 0xD5);
    ASSERT_EQ(c.b, 0x77);
    ASSERT_EQ(colorToUint(c), (Uint32)0x001CD577);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== color.h Unit Tests ===\n\n");

    RUN_TEST(hex2sdl_red);
    RUN_TEST(hex2sdl_green);
    RUN_TEST(hex2sdl_blue);
    RUN_TEST(hex2sdl_white);
    RUN_TEST(hex2sdl_black);
    RUN_TEST(hex2sdl_with_hash_prefix);
    RUN_TEST(hex2sdl_mixed_case);
    RUN_TEST(hex2sdl_arbitrary_color);

    RUN_TEST(color_to_uint_red);
    RUN_TEST(color_to_uint_green);
    RUN_TEST(color_to_uint_blue);
    RUN_TEST(color_to_uint_white);
    RUN_TEST(color_to_uint_black);
    RUN_TEST(color_to_uint_arbitrary);

    RUN_TEST(uint_to_color_red);
    RUN_TEST(uint_to_color_green);
    RUN_TEST(uint_to_color_blue);
    RUN_TEST(uint_to_color_white);
    RUN_TEST(uint_to_color_black);
    RUN_TEST(uint_to_color_arbitrary);

    RUN_TEST(roundtrip_hex_to_uint_to_color);
    RUN_TEST(roundtrip_uint_to_color_to_uint);

    RUN_TEST(osd_color_white_roundtrip);
    RUN_TEST(osd_color_red_roundtrip);
    RUN_TEST(osd_color_green_roundtrip);

    TEST_REPORT();
    return test_failures;
}
