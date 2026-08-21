/**
 * @file test_neon.c
 * @brief Unit tests for src/common/utils/neon_pixel.h
 *
 * Exercises every function's scalar fallback code path.
 * (On ARM hardware the NEON paths run; these tests verify correctness
 * of the portable C scalar paths that are compiled on the host.)
 *
 * Build and run:
 *   make -f Makefile.unit test_neon && ../build_test/test_neon
 */

#include "onion_test.h"
#include "../src/common/utils/neon_pixel.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* ---- neon_swap_rb_inplace ---- */

TEST(swap_rb_inplace_single) {
    uint32_t px = 0xFF112233; /* A=FF R=11 G=22 B=33, mem=[33,22,11,FF] */
    neon_swap_rb_inplace(&px, 1);
    /* After swap: A=FF R=33 G=22 B=11 */
    ASSERT_EQ(px, 0xFF332211u);
}

TEST(swap_rb_inplace_zero_alpha) {
    uint32_t px = 0x00AABBCC;
    neon_swap_rb_inplace(&px, 1);
    ASSERT_EQ(px, 0x00CCBBAAu);
}

TEST(swap_rb_inplace_identity) {
    /* Swapping twice returns to original */
    uint32_t px = 0xDEADBEEF;
    neon_swap_rb_inplace(&px, 1);
    neon_swap_rb_inplace(&px, 1);
    ASSERT_EQ(px, 0xDEADBEEFu);
}

TEST(swap_rb_inplace_16px) {
    uint32_t buf[16];
    for (int i = 0; i < 16; i++)
        buf[i] = 0xFF000000 | (i << 16) | (0x44 << 8) | (i ^ 0xFF);
    /* Save originals */
    uint32_t orig[16];
    memcpy(orig, buf, sizeof(buf));
    neon_swap_rb_inplace(buf, 16);
    for (int i = 0; i < 16; i++) {
        uint32_t expected = (orig[i] & 0xFF00FF00u)
                          | ((orig[i] & 0x00FF0000u) >> 16)
                          | ((orig[i] & 0x000000FFu) << 16);
        ASSERT_EQ(buf[i], expected);
    }
}

TEST(swap_rb_inplace_17px_tail) {
    /* 17 = 16 + 1: exercises the scalar tail */
    uint32_t buf[17];
    for (int i = 0; i < 17; i++)
        buf[i] = 0x10203040u + i;
    uint32_t orig[17];
    memcpy(orig, buf, sizeof(buf));
    neon_swap_rb_inplace(buf, 17);
    for (int i = 0; i < 17; i++) {
        uint32_t expected = (orig[i] & 0xFF00FF00u)
                          | ((orig[i] & 0x00FF0000u) >> 16)
                          | ((orig[i] & 0x000000FFu) << 16);
        ASSERT_EQ(buf[i], expected);
    }
}

/* ---- neon_argb_to_rgba ---- */

TEST(argb_to_rgba_single) {
    uint32_t src = 0xFF112233;
    uint32_t dst = 0;
    neon_argb_to_rgba(&dst, &src, 1);
    ASSERT_EQ(dst, 0xFF332211u);
}

TEST(argb_to_rgba_preserves_alpha) {
    uint32_t src = 0x80ABCDEF;
    uint32_t dst = 0;
    neon_argb_to_rgba(&dst, &src, 1);
    ASSERT_EQ(dst, 0x80EFCDABu);
}

TEST(argb_to_rgba_16px) {
    uint32_t src[16], dst[16];
    for (int i = 0; i < 16; i++)
        src[i] = 0xFF000000 | (i << 16) | (i << 8) | i;
    neon_argb_to_rgba(dst, src, 16);
    for (int i = 0; i < 16; i++) {
        uint32_t expected = (src[i] & 0xFF00FF00u)
                          | ((src[i] & 0x00FF0000u) >> 16)
                          | ((src[i] & 0x000000FFu) << 16);
        ASSERT_EQ(dst[i], expected);
    }
}

/* ---- neon_argb_to_rgba_alpha ---- */

TEST(argb_to_rgba_alpha_opaque) {
    uint32_t src = 0xFF112233;
    uint32_t dst = 0;
    neon_argb_to_rgba_alpha(&dst, &src, 1);
    ASSERT_EQ(dst, 0xFF332211u);
}

TEST(argb_to_rgba_alpha_transparent) {
    uint32_t src = 0x00112233;
    uint32_t dst = 0xDEADBEEF; /* should be overwritten with 0 */
    neon_argb_to_rgba_alpha(&dst, &src, 1);
    ASSERT_EQ(dst, 0x00000000u);
}

TEST(argb_to_rgba_alpha_semi) {
    uint32_t src = 0x80AABBCC;
    uint32_t dst = 0;
    neon_argb_to_rgba_alpha(&dst, &src, 1);
    ASSERT_EQ(dst, 0x80CCBBAAu);
}

TEST(argb_to_rgba_alpha_8px) {
    uint32_t src[8], dst[8];
    for (int i = 0; i < 8; i++)
        src[i] = (i % 2 == 0) ? 0x00112233u : 0xFF334455u;
    neon_argb_to_rgba_alpha(dst, src, 8);
    for (int i = 0; i < 8; i++) {
        if ((src[i] & 0xFF000000u) == 0) {
            ASSERT_EQ(dst[i], 0x00000000u);
        } else {
            uint32_t expected = (src[i] & 0xFF00FF00u)
                              | ((src[i] & 0x00FF0000u) >> 16)
                              | ((src[i] & 0x000000FFu) << 16);
            ASSERT_EQ(dst[i], expected);
        }
    }
}

/* ---- neon_rgb888_to_argb ---- */

TEST(rgb888_to_argb_single) {
    uint8_t src[3] = {0x11, 0x22, 0x33}; /* R, G, B */
    uint32_t dst = 0;
    neon_rgb888_to_argb(&dst, src, 1);
    /* Expected: A=FF, R=0x11, G=0x22, B=0x33 → 0xFF112233 */
    ASSERT_EQ(dst, 0xFF112233u);
}

TEST(rgb888_to_argb_black) {
    uint8_t src[3] = {0, 0, 0};
    uint32_t dst = 0xDEADBEEF;
    neon_rgb888_to_argb(&dst, src, 1);
    ASSERT_EQ(dst, 0xFF000000u);
}

TEST(rgb888_to_argb_white) {
    uint8_t src[3] = {0xFF, 0xFF, 0xFF};
    uint32_t dst = 0;
    neon_rgb888_to_argb(&dst, src, 1);
    ASSERT_EQ(dst, 0xFFFFFFFFu);
}

TEST(rgb888_to_argb_16px) {
    uint8_t src[48];
    uint32_t dst[16];
    for (int i = 0; i < 16; i++) {
        src[i * 3 + 0] = (uint8_t)(i * 5);      /* R */
        src[i * 3 + 1] = (uint8_t)(i * 10);     /* G */
        src[i * 3 + 2] = (uint8_t)(255 - i * 5);/* B */
    }
    neon_rgb888_to_argb(dst, src, 16);
    for (int i = 0; i < 16; i++) {
        uint32_t expected = 0xFF000000u
                          | ((uint32_t)src[i * 3 + 0] << 16)
                          | ((uint32_t)src[i * 3 + 1] << 8)
                          | src[i * 3 + 2];
        ASSERT_EQ(dst[i], expected);
    }
}

TEST(rgb888_to_argb_17px_tail) {
    uint8_t src[51];
    uint32_t dst[17];
    for (int i = 0; i < 17; i++) {
        src[i * 3 + 0] = (uint8_t)i;
        src[i * 3 + 1] = (uint8_t)(i + 1);
        src[i * 3 + 2] = (uint8_t)(i + 2);
    }
    neon_rgb888_to_argb(dst, src, 17);
    for (int i = 0; i < 17; i++) {
        uint32_t expected = 0xFF000000u
                          | ((uint32_t)src[i * 3 + 0] << 16)
                          | ((uint32_t)src[i * 3 + 1] << 8)
                          | src[i * 3 + 2];
        ASSERT_EQ(dst[i], expected);
    }
}

/* ---- neon_gray8_to_argb ---- */

TEST(gray8_to_argb_black) {
    uint8_t src = 0x00;
    uint32_t dst = 0xDEADBEEF;
    neon_gray8_to_argb(&dst, &src, 1);
    ASSERT_EQ(dst, 0xFF000000u);
}

TEST(gray8_to_argb_white) {
    uint8_t src = 0xFF;
    uint32_t dst = 0;
    neon_gray8_to_argb(&dst, &src, 1);
    ASSERT_EQ(dst, 0xFFFFFFFFu);
}

TEST(gray8_to_argb_mid_gray) {
    uint8_t src = 0x80;
    uint32_t dst = 0;
    neon_gray8_to_argb(&dst, &src, 1);
    /* R=G=B=0x80, A=0xFF */
    ASSERT_EQ(dst, 0xFF808080u);
}

TEST(gray8_to_argb_alpha_always_ff) {
    uint8_t src[3] = {0x00, 0x80, 0xFF};
    uint32_t dst[3];
    neon_gray8_to_argb(dst, src, 3);
    for (int i = 0; i < 3; i++) {
        /* Alpha must be 0xFF */
        ASSERT_EQ((dst[i] >> 24) & 0xFF, 0xFFu);
        /* R == G == B == gray */
        uint32_t r = (dst[i] >> 16) & 0xFF;
        uint32_t g = (dst[i] >> 8)  & 0xFF;
        uint32_t b = dst[i]          & 0xFF;
        ASSERT_EQ(r, (uint32_t)src[i]);
        ASSERT_EQ(g, (uint32_t)src[i]);
        ASSERT_EQ(b, (uint32_t)src[i]);
    }
}

TEST(gray8_to_argb_16px) {
    uint8_t src[16];
    uint32_t dst[16];
    for (int i = 0; i < 16; i++)
        src[i] = (uint8_t)(i * 17); /* 0, 17, 34, ... 255 */
    neon_gray8_to_argb(dst, src, 16);
    for (int i = 0; i < 16; i++) {
        uint32_t g = src[i];
        uint32_t expected = 0xFF000000u | (g << 16) | (g << 8) | g;
        ASSERT_EQ(dst[i], expected);
    }
}

TEST(gray8_to_argb_17px_tail) {
    uint8_t src[17];
    uint32_t dst[17];
    for (int i = 0; i < 17; i++)
        src[i] = (uint8_t)(255 - i);
    neon_gray8_to_argb(dst, src, 17);
    for (int i = 0; i < 17; i++) {
        uint32_t g = src[i];
        uint32_t expected = 0xFF000000u | (g << 16) | (g << 8) | g;
        ASSERT_EQ(dst[i], expected);
    }
}

/* ---- neon_gray8a_to_argb ---- */

TEST(gray8a_to_argb_opaque_black) {
    uint8_t src[2] = {0x00, 0xFF}; /* gray=0, alpha=255 */
    uint32_t dst = 0xDEADBEEF;
    neon_gray8a_to_argb(&dst, src, 1);
    ASSERT_EQ(dst, 0xFF000000u);
}

TEST(gray8a_to_argb_transparent_white) {
    uint8_t src[2] = {0xFF, 0x00}; /* gray=255, alpha=0 */
    uint32_t dst = 0xDEADBEEF;
    neon_gray8a_to_argb(&dst, src, 1);
    ASSERT_EQ(dst, 0x00FFFFFFu);
}

TEST(gray8a_to_argb_mid) {
    uint8_t src[2] = {0x80, 0x40}; /* gray=0x80, alpha=0x40 */
    uint32_t dst = 0;
    neon_gray8a_to_argb(&dst, src, 1);
    ASSERT_EQ(dst, 0x40808080u);
}

TEST(gray8a_to_argb_channels) {
    /* R, G, B must equal gray; A must equal alpha */
    uint8_t src[4] = {0xAB, 0xCD, 0x12, 0x34};
    uint32_t dst[2];
    neon_gray8a_to_argb(dst, src, 2);

    uint32_t r0 = (dst[0] >> 16) & 0xFF;
    uint32_t g0 = (dst[0] >> 8)  & 0xFF;
    uint32_t b0 = dst[0]          & 0xFF;
    uint32_t a0 = (dst[0] >> 24) & 0xFF;
    ASSERT_EQ(r0, 0xABu); ASSERT_EQ(g0, 0xABu); ASSERT_EQ(b0, 0xABu);
    ASSERT_EQ(a0, 0xCDu);

    uint32_t r1 = (dst[1] >> 16) & 0xFF;
    uint32_t g1 = (dst[1] >> 8)  & 0xFF;
    uint32_t b1 = dst[1]          & 0xFF;
    uint32_t a1 = (dst[1] >> 24) & 0xFF;
    ASSERT_EQ(r1, 0x12u); ASSERT_EQ(g1, 0x12u); ASSERT_EQ(b1, 0x12u);
    ASSERT_EQ(a1, 0x34u);
}

TEST(gray8a_to_argb_16px) {
    uint8_t src[32];
    uint32_t dst[16];
    for (int i = 0; i < 16; i++) {
        src[i * 2 + 0] = (uint8_t)(i * 17);       /* gray */
        src[i * 2 + 1] = (uint8_t)(255 - i * 17); /* alpha */
    }
    neon_gray8a_to_argb(dst, src, 16);
    for (int i = 0; i < 16; i++) {
        uint32_t g = src[i * 2];
        uint32_t a = src[i * 2 + 1];
        uint32_t expected = (a << 24) | (g << 16) | (g << 8) | g;
        ASSERT_EQ(dst[i], expected);
    }
}

TEST(gray8a_to_argb_17px_tail) {
    uint8_t src[34];
    uint32_t dst[17];
    for (int i = 0; i < 17; i++) {
        src[i * 2 + 0] = (uint8_t)(i * 15);
        src[i * 2 + 1] = (uint8_t)(i * 10);
    }
    neon_gray8a_to_argb(dst, src, 17);
    for (int i = 0; i < 17; i++) {
        uint32_t g = src[i * 2];
        uint32_t a = src[i * 2 + 1];
        uint32_t expected = (a << 24) | (g << 16) | (g << 8) | g;
        ASSERT_EQ(dst[i], expected);
    }
}

/* ---- neon_rotate180_inplace ---- */

TEST(rotate180_single) {
    uint32_t px = 0xDEADBEEF;
    neon_rotate180_inplace(&px, 1);
    ASSERT_EQ(px, 0xDEADBEEFu);
}

TEST(rotate180_two) {
    uint32_t buf[2] = {0xAAAAAAAAu, 0xBBBBBBBBu};
    neon_rotate180_inplace(buf, 2);
    ASSERT_EQ(buf[0], 0xBBBBBBBBu);
    ASSERT_EQ(buf[1], 0xAAAAAAAAu);
}

TEST(rotate180_even) {
    uint32_t buf[4] = {1, 2, 3, 4};
    neon_rotate180_inplace(buf, 4);
    ASSERT_EQ(buf[0], 4u);
    ASSERT_EQ(buf[1], 3u);
    ASSERT_EQ(buf[2], 2u);
    ASSERT_EQ(buf[3], 1u);
}

TEST(rotate180_odd) {
    uint32_t buf[5] = {10, 20, 30, 40, 50};
    neon_rotate180_inplace(buf, 5);
    ASSERT_EQ(buf[0], 50u);
    ASSERT_EQ(buf[1], 40u);
    ASSERT_EQ(buf[2], 30u);
    ASSERT_EQ(buf[3], 20u);
    ASSERT_EQ(buf[4], 10u);
}

TEST(rotate180_involution) {
    /* Rotating twice restores original order */
    uint32_t buf[10], orig[10];
    for (int i = 0; i < 10; i++)
        buf[i] = orig[i] = (uint32_t)(i * 0x11111111u);
    neon_rotate180_inplace(buf, 10);
    neon_rotate180_inplace(buf, 10);
    for (int i = 0; i < 10; i++)
        ASSERT_EQ(buf[i], orig[i]);
}

TEST(rotate180_16px) {
    uint32_t buf[16], orig[16];
    for (int i = 0; i < 16; i++)
        buf[i] = orig[i] = (uint32_t)i;
    neon_rotate180_inplace(buf, 16);
    for (int i = 0; i < 16; i++)
        ASSERT_EQ(buf[i], orig[15 - i]);
}

TEST(rotate180_17px_tail) {
    uint32_t buf[17], orig[17];
    for (int i = 0; i < 17; i++)
        buf[i] = orig[i] = (uint32_t)(i + 100);
    neon_rotate180_inplace(buf, 17);
    for (int i = 0; i < 17; i++)
        ASSERT_EQ(buf[i], orig[16 - i]);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== neon_pixel.h Unit Tests ===\n\n");

    RUN_TEST(swap_rb_inplace_single);
    RUN_TEST(swap_rb_inplace_zero_alpha);
    RUN_TEST(swap_rb_inplace_identity);
    RUN_TEST(swap_rb_inplace_16px);
    RUN_TEST(swap_rb_inplace_17px_tail);

    RUN_TEST(argb_to_rgba_single);
    RUN_TEST(argb_to_rgba_preserves_alpha);
    RUN_TEST(argb_to_rgba_16px);

    RUN_TEST(argb_to_rgba_alpha_opaque);
    RUN_TEST(argb_to_rgba_alpha_transparent);
    RUN_TEST(argb_to_rgba_alpha_semi);
    RUN_TEST(argb_to_rgba_alpha_8px);

    RUN_TEST(rgb888_to_argb_single);
    RUN_TEST(rgb888_to_argb_black);
    RUN_TEST(rgb888_to_argb_white);
    RUN_TEST(rgb888_to_argb_16px);
    RUN_TEST(rgb888_to_argb_17px_tail);

    RUN_TEST(gray8_to_argb_black);
    RUN_TEST(gray8_to_argb_white);
    RUN_TEST(gray8_to_argb_mid_gray);
    RUN_TEST(gray8_to_argb_alpha_always_ff);
    RUN_TEST(gray8_to_argb_16px);
    RUN_TEST(gray8_to_argb_17px_tail);

    RUN_TEST(gray8a_to_argb_opaque_black);
    RUN_TEST(gray8a_to_argb_transparent_white);
    RUN_TEST(gray8a_to_argb_mid);
    RUN_TEST(gray8a_to_argb_channels);
    RUN_TEST(gray8a_to_argb_16px);
    RUN_TEST(gray8a_to_argb_17px_tail);

    RUN_TEST(rotate180_single);
    RUN_TEST(rotate180_two);
    RUN_TEST(rotate180_even);
    RUN_TEST(rotate180_odd);
    RUN_TEST(rotate180_involution);
    RUN_TEST(rotate180_16px);
    RUN_TEST(rotate180_17px_tail);

    TEST_REPORT();
    return test_failures;
}
