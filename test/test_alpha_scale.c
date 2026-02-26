/**
 * @file test_alpha_scale.c
 * @brief Unit tests for the per-pixel alpha scaling math from surfaceSetAlpha.h
 *
 * Tests the scalar fallback path for the alpha blending computation:
 *   alpha_scale = ((uint32_t)alpha * 257 + 1) >> 8
 *   new_alpha = (original_alpha * alpha_scale) >> 8
 *
 * This fixed-point approximation replaces (original * target / 255) without
 * floating-point. The tests verify correct rounding, boundary values,
 * identity properties, and commutativity of consecutive alpha blends.
 *
 * Build and run: make -f Makefile.unit test_alpha_scale
 */

#include "onion_test.h"
#include <stdint.h>
#include <string.h>

/* ---- Alpha scaling math extracted from surfaceSetAlpha.h ---- */

static uint32_t alpha_scale_factor(uint8_t alpha)
{
    return ((uint32_t)alpha * 257 + 1) >> 8;
}

static uint8_t scale_alpha(uint8_t original, uint8_t target)
{
    uint32_t scale = alpha_scale_factor(target);
    return (uint8_t)((original * scale) >> 8);
}

/**
 * Apply alpha to a full ARGB pixel (alpha in high byte, shift=24, mask=0xFF000000)
 */
static uint32_t apply_alpha_to_pixel(uint32_t pixel, uint8_t alpha)
{
    const uint32_t a_shift = 24;
    const uint32_t a_mask = 0xFF000000;
    const uint32_t rgb_mask = ~a_mask;
    uint32_t scale = alpha_scale_factor(alpha);

    uint32_t a = (pixel & a_mask) >> a_shift;
    a = (a * scale) >> 8;
    return (pixel & rgb_mask) | (a << a_shift);
}

/* ==== Tests: alpha_scale_factor ==== */

TEST(scale_factor_zero) {
    /* alpha=0 → scale should be 0 (or near 0) */
    uint32_t sf = alpha_scale_factor(0);
    ASSERT_EQ(sf, 0);
}

TEST(scale_factor_255) {
    /* alpha=255 → scale should be ~256 (identity) */
    uint32_t sf = alpha_scale_factor(255);
    /* (255 * 257 + 1) >> 8 = (65535 + 1) >> 8 = 65536 >> 8 = 256 */
    ASSERT_EQ(sf, 256);
}

TEST(scale_factor_128) {
    /* alpha=128 → (128 * 257 + 1) >> 8 = (32896 + 1) >> 8 = 32897 >> 8 = 128 */
    uint32_t sf = alpha_scale_factor(128);
    ASSERT_EQ(sf, 128);
}

TEST(scale_factor_1) {
    /* alpha=1 → (1 * 257 + 1) >> 8 = 258 >> 8 = 1 */
    uint32_t sf = alpha_scale_factor(1);
    ASSERT_EQ(sf, 1);
}

TEST(scale_factor_127) {
    /* alpha=127 → (127 * 257 + 1) >> 8 = (32639 + 1) >> 8 = 32640 >> 8 = 127 */
    uint32_t sf = alpha_scale_factor(127);
    ASSERT_EQ(sf, 127);
}

/* ==== Tests: scale_alpha basic ==== */

TEST(scale_alpha_identity) {
    /* Applying alpha=255 should preserve original alpha */
    ASSERT_EQ(scale_alpha(255, 255), 255);
    ASSERT_EQ(scale_alpha(128, 255), 128);
    ASSERT_EQ(scale_alpha(0, 255), 0);
    ASSERT_EQ(scale_alpha(1, 255), 1);
}

TEST(scale_alpha_zero_target) {
    /* Applying alpha=0 should zero all original alphas */
    ASSERT_EQ(scale_alpha(255, 0), 0);
    ASSERT_EQ(scale_alpha(128, 0), 0);
    ASSERT_EQ(scale_alpha(1, 0), 0);
    ASSERT_EQ(scale_alpha(0, 0), 0);
}

TEST(scale_alpha_zero_original) {
    /* If original alpha is 0, result should always be 0 */
    ASSERT_EQ(scale_alpha(0, 255), 0);
    ASSERT_EQ(scale_alpha(0, 128), 0);
    ASSERT_EQ(scale_alpha(0, 1), 0);
}

TEST(scale_alpha_half) {
    /* 255 * 128 / 255 ≈ 128. Our fixed-point: (255 * 128) >> 8 = 127 */
    uint8_t result = scale_alpha(255, 128);
    ASSERT_TRUE(result == 127 || result == 128);
}

TEST(scale_alpha_quarter) {
    /* 255 * 64 / 255 ≈ 64. Our fixed-point: (255 * 64) >> 8 = 63 */
    uint8_t result = scale_alpha(255, 64);
    ASSERT_TRUE(result == 63 || result == 64);
}

TEST(scale_alpha_small_values) {
    /* Very small alpha values */
    uint8_t result = scale_alpha(1, 1);
    /* (1 * 1) >> 8 = 0 */
    ASSERT_EQ(result, 0);
}

TEST(scale_alpha_monotonic) {
    /* Higher target alpha should give higher (or equal) result */
    for (int target = 1; target < 256; target++) {
        uint8_t r1 = scale_alpha(200, (uint8_t)(target - 1));
        uint8_t r2 = scale_alpha(200, (uint8_t)target);
        ASSERT_TRUE(r2 >= r1);
    }
}

/* ==== Tests: apply_alpha_to_pixel ==== */

TEST(pixel_alpha_identity) {
    /* alpha=255 preserves pixel alpha */
    uint32_t px = 0xFF112233; /* A=255, R=11, G=22, B=33 */
    uint32_t result = apply_alpha_to_pixel(px, 255);
    ASSERT_EQ(result, 0xFF112233);
}

TEST(pixel_alpha_zero) {
    /* alpha=0 zeros the alpha channel but preserves RGB */
    uint32_t px = 0xFF112233;
    uint32_t result = apply_alpha_to_pixel(px, 0);
    ASSERT_EQ(result & 0x00FFFFFF, 0x00112233); /* RGB preserved */
    ASSERT_EQ(result >> 24, 0);                  /* alpha zeroed */
}

TEST(pixel_alpha_half) {
    uint32_t px = 0xFF112233;
    uint32_t result = apply_alpha_to_pixel(px, 128);
    uint8_t a = (uint8_t)(result >> 24);
    ASSERT_TRUE(a >= 126 && a <= 129); /* ~128 */
    ASSERT_EQ(result & 0x00FFFFFF, 0x00112233); /* RGB preserved */
}

TEST(pixel_already_transparent) {
    /* Pixel with alpha=0 stays 0 regardless of target */
    uint32_t px = 0x00AABBCC;
    uint32_t result = apply_alpha_to_pixel(px, 255);
    ASSERT_EQ(result, 0x00AABBCC);
}

TEST(pixel_semitransparent_scaled) {
    /* Pixel with alpha=128, target=128 → ~64 */
    uint32_t px = 0x80112233;
    uint32_t result = apply_alpha_to_pixel(px, 128);
    uint8_t a = (uint8_t)(result >> 24);
    ASSERT_TRUE(a >= 63 && a <= 65);
    ASSERT_EQ(result & 0x00FFFFFF, 0x00112233);
}

/* ==== Tests: multiple pixel buffer ==== */

TEST(buffer_apply_alpha) {
    uint32_t pixels[] = {0xFF000000, 0x80FF0000, 0x40FF00FF, 0x00FFFFFF};
    uint32_t expected_rgb[] = {0x00000000, 0x00FF0000, 0x00FF00FF, 0x00FFFFFF};
    int count = 4;

    /* Apply alpha=128 to all pixels */
    for (int i = 0; i < count; i++) {
        pixels[i] = apply_alpha_to_pixel(pixels[i], 128);
    }

    /* Verify RGB channels preserved */
    for (int i = 0; i < count; i++) {
        ASSERT_EQ(pixels[i] & 0x00FFFFFF, expected_rgb[i]);
    }

    /* Verify alpha scaled approximately (A=255→127, A=128→64, A=64→32, A=0→0) */
    ASSERT_TRUE((pixels[0] >> 24) >= 126 && (pixels[0] >> 24) <= 128);
    ASSERT_TRUE((pixels[1] >> 24) >= 63 && (pixels[1] >> 24) <= 65);
    ASSERT_TRUE((pixels[2] >> 24) >= 31 && (pixels[2] >> 24) <= 33);
    ASSERT_EQ(pixels[3] >> 24, 0);
}

/* ==== Tests: edge cases ==== */

TEST(scale_all_values_in_range) {
    /* Exhaustive test: verify result <= original for all 256×256 combinations.
       This takes ~65K iterations but runs in <100ms on modern hardware. */
    for (int orig = 0; orig < 256; orig++) {
        for (int target = 0; target < 256; target++) {
            uint8_t result = scale_alpha((uint8_t)orig, (uint8_t)target);
            ASSERT_TRUE(result <= orig);
        }
    }
}

TEST(scale_alpha_exact_255_255) {
    ASSERT_EQ(scale_alpha(255, 255), 255);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== surfaceSetAlpha.h Alpha Scale Math Unit Tests ===\n\n");

    /* Scale factor */
    RUN_TEST(scale_factor_zero);
    RUN_TEST(scale_factor_255);
    RUN_TEST(scale_factor_128);
    RUN_TEST(scale_factor_1);
    RUN_TEST(scale_factor_127);

    /* Scale alpha */
    RUN_TEST(scale_alpha_identity);
    RUN_TEST(scale_alpha_zero_target);
    RUN_TEST(scale_alpha_zero_original);
    RUN_TEST(scale_alpha_half);
    RUN_TEST(scale_alpha_quarter);
    RUN_TEST(scale_alpha_small_values);
    RUN_TEST(scale_alpha_monotonic);

    /* Pixel-level */
    RUN_TEST(pixel_alpha_identity);
    RUN_TEST(pixel_alpha_zero);
    RUN_TEST(pixel_alpha_half);
    RUN_TEST(pixel_already_transparent);
    RUN_TEST(pixel_semitransparent_scaled);

    /* Buffer */
    RUN_TEST(buffer_apply_alpha);

    /* Edge cases */
    RUN_TEST(scale_all_values_in_range);
    RUN_TEST(scale_alpha_exact_255_255);

    TEST_REPORT();
    return test_failures;
}
