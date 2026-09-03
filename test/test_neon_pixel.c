/**
 * @file test_neon_pixel.c
 * @brief Unit tests for scalar fallback paths in neon_pixel.h
 *
 * Tests all pixel format conversion functions using the scalar (non-NEON)
 * code paths, which are identical in logic to the NEON-accelerated versions.
 * Covers: swap R↔B, ARGB→RGBA, ARGB→RGBA with alpha, RGB888→ARGB,
 * grayscale→ARGB, grayscale+alpha→ARGB, and 180° rotation.
 *
 * Build and run: make -f Makefile.unit test_neon_pixel
 */

#include "onion_test.h"
#include "../src/common/utils/neon_pixel.h"
#include <stdint.h>
#include <string.h>

/* Extra cases on the production header (test_neon.c already includes it). */

/* ==== Tests: neon_swap_rb_inplace ==== */

TEST(swap_rb_red_to_blue) {
    /* ARGB red = 0xFFFF0000 → swap R↔B → 0xFF0000FF */
    uint32_t px[] = {0xFFFF0000};
    neon_swap_rb_inplace(px, 1);
    ASSERT_EQ(px[0], 0xFF0000FF);
}

TEST(swap_rb_blue_to_red) {
    uint32_t px[] = {0xFF0000FF};
    neon_swap_rb_inplace(px, 1);
    ASSERT_EQ(px[0], 0xFFFF0000);
}

TEST(swap_rb_green_unchanged) {
    /* Green channel (and alpha) unaffected */
    uint32_t px[] = {0xFF00FF00};
    neon_swap_rb_inplace(px, 1);
    ASSERT_EQ(px[0], 0xFF00FF00);
}

TEST(swap_rb_white_unchanged) {
    uint32_t px[] = {0xFFFFFFFF};
    neon_swap_rb_inplace(px, 1);
    ASSERT_EQ(px[0], 0xFFFFFFFF);
}

TEST(swap_rb_black_unchanged) {
    uint32_t px[] = {0xFF000000};
    neon_swap_rb_inplace(px, 1);
    ASSERT_EQ(px[0], 0xFF000000);
}

TEST(swap_rb_preserves_alpha) {
    uint32_t px[] = {0x80FF0000};
    neon_swap_rb_inplace(px, 1);
    ASSERT_EQ(px[0], 0x800000FF);
}

TEST(swap_rb_multiple_pixels) {
    uint32_t px[] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF};
    neon_swap_rb_inplace(px, 3);
    ASSERT_EQ(px[0], 0xFF0000FF);
    ASSERT_EQ(px[1], 0xFF00FF00);
    ASSERT_EQ(px[2], 0xFFFF0000);
}

TEST(swap_rb_double_swap_identity) {
    uint32_t px[] = {0xAABBCCDD};
    uint32_t orig = px[0];
    neon_swap_rb_inplace(px, 1);
    neon_swap_rb_inplace(px, 1);
    ASSERT_EQ(px[0], orig);
}

TEST(swap_rb_zero_count) {
    uint32_t px[] = {0xFFFF0000};
    neon_swap_rb_inplace(px, 0);
    ASSERT_EQ(px[0], 0xFFFF0000); /* unchanged */
}

/* ==== Tests: neon_argb_to_rgba ==== */

TEST(argb_to_rgba_basic) {
    uint32_t src[] = {0xFFFF0000};
    uint32_t dst[1];
    neon_argb_to_rgba(dst, src, 1);
    ASSERT_EQ(dst[0], 0xFF0000FF);
    ASSERT_EQ(src[0], 0xFFFF0000); /* src unmodified */
}

TEST(argb_to_rgba_multiple) {
    uint32_t src[] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF, 0x00000000};
    uint32_t dst[4];
    neon_argb_to_rgba(dst, src, 4);
    ASSERT_EQ(dst[0], 0xFF0000FF);
    ASSERT_EQ(dst[1], 0xFF00FF00);
    ASSERT_EQ(dst[2], 0xFFFF0000);
    ASSERT_EQ(dst[3], 0x00000000);
}

/* ==== Tests: neon_argb_to_rgba_alpha ==== */

TEST(argb_to_rgba_alpha_opaque) {
    uint32_t src[] = {0xFFFF0000};
    uint32_t dst[1];
    neon_argb_to_rgba_alpha(dst, src, 1);
    ASSERT_EQ(dst[0], 0xFF0000FF);
}

TEST(argb_to_rgba_alpha_transparent) {
    uint32_t src[] = {0x00FF0000}; /* alpha = 0 */
    uint32_t dst[1];
    neon_argb_to_rgba_alpha(dst, src, 1);
    ASSERT_EQ(dst[0], 0x00000000); /* zeroed entirely */
}

TEST(argb_to_rgba_alpha_semitransparent) {
    uint32_t src[] = {0x80112233};
    uint32_t dst[1];
    neon_argb_to_rgba_alpha(dst, src, 1);
    /* alpha != 0, so swap R↔B: 0x80112233 → 0x80332211 */
    ASSERT_EQ(dst[0], 0x80332211);
}

TEST(argb_to_rgba_alpha_mixed) {
    uint32_t src[] = {0xFFAABBCC, 0x00112233, 0x01DDEEFF};
    uint32_t dst[3];
    neon_argb_to_rgba_alpha(dst, src, 3);
    ASSERT_EQ(dst[0], 0xFFCCBBAA); /* opaque, swapped */
    ASSERT_EQ(dst[1], 0x00000000); /* transparent, zeroed */
    ASSERT_EQ(dst[2], 0x01FFEEDD); /* alpha=1, swapped */
}

/* ==== Tests: neon_rgb888_to_argb ==== */

TEST(rgb888_to_argb_red) {
    uint8_t src[] = {0xFF, 0x00, 0x00};
    uint32_t dst[1];
    neon_rgb888_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0], 0xFFFF0000);
}

TEST(rgb888_to_argb_green) {
    uint8_t src[] = {0x00, 0xFF, 0x00};
    uint32_t dst[1];
    neon_rgb888_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0], 0xFF00FF00);
}

TEST(rgb888_to_argb_blue) {
    uint8_t src[] = {0x00, 0x00, 0xFF};
    uint32_t dst[1];
    neon_rgb888_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0], 0xFF0000FF);
}

TEST(rgb888_to_argb_white) {
    uint8_t src[] = {0xFF, 0xFF, 0xFF};
    uint32_t dst[1];
    neon_rgb888_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0], 0xFFFFFFFF);
}

TEST(rgb888_to_argb_alpha_always_ff) {
    uint8_t src[] = {0x11, 0x22, 0x33};
    uint32_t dst[1];
    neon_rgb888_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0] >> 24, 0xFF);
}

TEST(rgb888_to_argb_multiple) {
    uint8_t src[] = {0xFF, 0x00, 0x00,   /* red */
                     0x00, 0xFF, 0x00,   /* green */
                     0x00, 0x00, 0xFF};  /* blue */
    uint32_t dst[3];
    neon_rgb888_to_argb(dst, src, 3);
    ASSERT_EQ(dst[0], 0xFFFF0000);
    ASSERT_EQ(dst[1], 0xFF00FF00);
    ASSERT_EQ(dst[2], 0xFF0000FF);
}

/* ==== Tests: neon_gray8_to_argb ==== */

TEST(gray8_to_argb_black) {
    uint8_t src[] = {0x00};
    uint32_t dst[1];
    neon_gray8_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0], 0xFF000000);
}

TEST(gray8_to_argb_white) {
    uint8_t src[] = {0xFF};
    uint32_t dst[1];
    neon_gray8_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0], 0xFFFFFFFF);
}

TEST(gray8_to_argb_midgray) {
    uint8_t src[] = {0x80};
    uint32_t dst[1];
    neon_gray8_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0], 0xFF808080);
}

TEST(gray8_to_argb_channels_equal) {
    uint8_t src[] = {0x42};
    uint32_t dst[1];
    neon_gray8_to_argb(dst, src, 1);
    uint32_t r = (dst[0] >> 16) & 0xFF;
    uint32_t g = (dst[0] >> 8) & 0xFF;
    uint32_t b = dst[0] & 0xFF;
    ASSERT_EQ(r, 0x42);
    ASSERT_EQ(g, 0x42);
    ASSERT_EQ(b, 0x42);
}

TEST(gray8_to_argb_alpha_ff) {
    uint8_t src[] = {0x55};
    uint32_t dst[1];
    neon_gray8_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0] >> 24, 0xFF);
}

/* ==== Tests: neon_gray8a_to_argb ==== */

TEST(gray8a_to_argb_opaque_white) {
    uint8_t src[] = {0xFF, 0xFF}; /* gray=255, alpha=255 */
    uint32_t dst[1];
    neon_gray8a_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0], 0xFFFFFFFF);
}

TEST(gray8a_to_argb_transparent_black) {
    uint8_t src[] = {0x00, 0x00}; /* gray=0, alpha=0 */
    uint32_t dst[1];
    neon_gray8a_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0], 0x00000000);
}

TEST(gray8a_to_argb_semitransparent) {
    uint8_t src[] = {0x80, 0x40}; /* gray=128, alpha=64 */
    uint32_t dst[1];
    neon_gray8a_to_argb(dst, src, 1);
    ASSERT_EQ(dst[0], 0x40808080);
}

TEST(gray8a_to_argb_channels_replicated) {
    uint8_t src[] = {0xAB, 0xCD};
    uint32_t dst[1];
    neon_gray8a_to_argb(dst, src, 1);
    uint32_t a = (dst[0] >> 24) & 0xFF;
    uint32_t r = (dst[0] >> 16) & 0xFF;
    uint32_t g = (dst[0] >> 8) & 0xFF;
    uint32_t b = dst[0] & 0xFF;
    ASSERT_EQ(a, 0xCD);
    ASSERT_EQ(r, 0xAB);
    ASSERT_EQ(g, 0xAB);
    ASSERT_EQ(b, 0xAB);
}

TEST(gray8a_to_argb_multiple) {
    uint8_t src[] = {0xFF, 0xFF,  0x00, 0x00,  0x80, 0x80};
    uint32_t dst[3];
    neon_gray8a_to_argb(dst, src, 3);
    ASSERT_EQ(dst[0], 0xFFFFFFFF);
    ASSERT_EQ(dst[1], 0x00000000);
    ASSERT_EQ(dst[2], 0x80808080);
}

/* ==== Tests: neon_rotate180_inplace ==== */

TEST(rotate180_single_pixel) {
    uint32_t px[] = {0xAABBCCDD};
    neon_rotate180_inplace(px, 1);
    ASSERT_EQ(px[0], 0xAABBCCDD); /* single element unchanged */
}

TEST(rotate180_two_pixels) {
    uint32_t px[] = {0x11111111, 0x22222222};
    neon_rotate180_inplace(px, 2);
    ASSERT_EQ(px[0], 0x22222222);
    ASSERT_EQ(px[1], 0x11111111);
}

TEST(rotate180_three_pixels) {
    uint32_t px[] = {0xAA, 0xBB, 0xCC};
    neon_rotate180_inplace(px, 3);
    ASSERT_EQ(px[0], 0xCC);
    ASSERT_EQ(px[1], 0xBB); /* middle unchanged */
    ASSERT_EQ(px[2], 0xAA);
}

TEST(rotate180_four_pixels) {
    uint32_t px[] = {1, 2, 3, 4};
    neon_rotate180_inplace(px, 4);
    ASSERT_EQ(px[0], 4);
    ASSERT_EQ(px[1], 3);
    ASSERT_EQ(px[2], 2);
    ASSERT_EQ(px[3], 1);
}

TEST(rotate180_double_is_identity) {
    uint32_t px[] = {0x11, 0x22, 0x33, 0x44, 0x55};
    uint32_t orig[5];
    memcpy(orig, px, sizeof(px));
    neon_rotate180_inplace(px, 5);
    neon_rotate180_inplace(px, 5);
    for (int i = 0; i < 5; i++)
        ASSERT_EQ(px[i], orig[i]);
}

TEST(rotate180_larger_buffer) {
    uint32_t px[8];
    for (int i = 0; i < 8; i++) px[i] = (uint32_t)i;
    neon_rotate180_inplace(px, 8);
    for (int i = 0; i < 8; i++)
        ASSERT_EQ(px[i], (uint32_t)(7 - i));
}

TEST(rotate180_zero_count) {
    uint32_t px[] = {0xAABBCCDDu};
    neon_rotate180_inplace(px, 0);
    ASSERT_EQ(px[0], 0xAABBCCDDu);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== neon_pixel.h Scalar Fallback Unit Tests ===\n\n");

    /* swap R↔B */
    RUN_TEST(swap_rb_red_to_blue);
    RUN_TEST(swap_rb_blue_to_red);
    RUN_TEST(swap_rb_green_unchanged);
    RUN_TEST(swap_rb_white_unchanged);
    RUN_TEST(swap_rb_black_unchanged);
    RUN_TEST(swap_rb_preserves_alpha);
    RUN_TEST(swap_rb_multiple_pixels);
    RUN_TEST(swap_rb_double_swap_identity);
    RUN_TEST(swap_rb_zero_count);

    /* ARGB → RGBA */
    RUN_TEST(argb_to_rgba_basic);
    RUN_TEST(argb_to_rgba_multiple);

    /* ARGB → RGBA with alpha */
    RUN_TEST(argb_to_rgba_alpha_opaque);
    RUN_TEST(argb_to_rgba_alpha_transparent);
    RUN_TEST(argb_to_rgba_alpha_semitransparent);
    RUN_TEST(argb_to_rgba_alpha_mixed);

    /* RGB888 → ARGB */
    RUN_TEST(rgb888_to_argb_red);
    RUN_TEST(rgb888_to_argb_green);
    RUN_TEST(rgb888_to_argb_blue);
    RUN_TEST(rgb888_to_argb_white);
    RUN_TEST(rgb888_to_argb_alpha_always_ff);
    RUN_TEST(rgb888_to_argb_multiple);

    /* Gray8 → ARGB */
    RUN_TEST(gray8_to_argb_black);
    RUN_TEST(gray8_to_argb_white);
    RUN_TEST(gray8_to_argb_midgray);
    RUN_TEST(gray8_to_argb_channels_equal);
    RUN_TEST(gray8_to_argb_alpha_ff);

    /* Gray8+Alpha → ARGB */
    RUN_TEST(gray8a_to_argb_opaque_white);
    RUN_TEST(gray8a_to_argb_transparent_black);
    RUN_TEST(gray8a_to_argb_semitransparent);
    RUN_TEST(gray8a_to_argb_channels_replicated);
    RUN_TEST(gray8a_to_argb_multiple);

    /* Rotate 180° */
    RUN_TEST(rotate180_single_pixel);
    RUN_TEST(rotate180_two_pixels);
    RUN_TEST(rotate180_three_pixels);
    RUN_TEST(rotate180_four_pixels);
    RUN_TEST(rotate180_double_is_identity);
    RUN_TEST(rotate180_larger_buffer);
    RUN_TEST(rotate180_zero_count);

    TEST_REPORT();
    return test_failures;
}
