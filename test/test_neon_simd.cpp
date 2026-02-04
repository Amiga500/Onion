/**
 * @file test_neon_simd.cpp
 * @brief Unit tests for NEON SIMD optimizations
 *
 * These tests verify that the NEON-optimized pixel operations produce
 * correct results compared to scalar implementations.
 */

#include "gtest/gtest.h"
#include <cstring>
#include <cstdint>

// Include the NEON SIMD header (it has scalar fallbacks for non-ARM platforms)
#include "../src/common/utils/neon_simd.h"

// Test RGB888 to ARGB8888 conversion
TEST(NeonSimdTest, RGB888ToARGB8888)
{
    // Test data: 16 RGB pixels
    const uint32_t width = 16;
    uint8_t src[width * 3];
    uint32_t dst[width];
    uint32_t expected[width];

    // Fill source with test pattern
    for (uint32_t i = 0; i < width; i++) {
        src[i * 3 + 0] = (uint8_t)(i * 10);       // R
        src[i * 3 + 1] = (uint8_t)(i * 10 + 50);  // G
        src[i * 3 + 2] = (uint8_t)(i * 10 + 100); // B

        // Expected ARGB8888 format: 0xAARRGGBB
        expected[i] = 0xFF000000u |
                      ((uint32_t)src[i * 3 + 0] << 16) |
                      ((uint32_t)src[i * 3 + 1] << 8) |
                      src[i * 3 + 2];
    }

    // Run NEON-optimized conversion
    memset(dst, 0, sizeof(dst));
    neon_rgb888_to_argb8888(dst, src, width);

    // Verify results
    for (uint32_t i = 0; i < width; i++) {
        EXPECT_EQ(expected[i], dst[i]) << "Mismatch at pixel " << i;
    }
}

// Test RGB888 to ARGB8888 with non-aligned width (tests scalar fallback)
TEST(NeonSimdTest, RGB888ToARGB8888NonAligned)
{
    // Test data: 13 RGB pixels (not multiple of 8)
    const uint32_t width = 13;
    uint8_t src[width * 3];
    uint32_t dst[width];
    uint32_t expected[width];

    for (uint32_t i = 0; i < width; i++) {
        src[i * 3 + 0] = (uint8_t)(255 - i * 15);
        src[i * 3 + 1] = (uint8_t)(128);
        src[i * 3 + 2] = (uint8_t)(i * 20);

        expected[i] = 0xFF000000u |
                      ((uint32_t)src[i * 3 + 0] << 16) |
                      ((uint32_t)src[i * 3 + 1] << 8) |
                      src[i * 3 + 2];
    }

    memset(dst, 0, sizeof(dst));
    neon_rgb888_to_argb8888(dst, src, width);

    for (uint32_t i = 0; i < width; i++) {
        EXPECT_EQ(expected[i], dst[i]) << "Mismatch at pixel " << i;
    }
}

// Test grayscale to ARGB8888 conversion
TEST(NeonSimdTest, GrayToARGB8888)
{
    const uint32_t width = 16;
    uint8_t src[width];
    uint32_t dst[width];
    uint32_t expected[width];

    for (uint32_t i = 0; i < width; i++) {
        src[i] = (uint8_t)(i * 16);
        // Gray -> ARGB: same value in R, G, B with A=0xFF
        expected[i] = 0xFF000000u |
                      ((uint32_t)src[i] << 16) |
                      ((uint32_t)src[i] << 8) |
                      src[i];
    }

    memset(dst, 0, sizeof(dst));
    neon_gray_to_argb8888(dst, src, width);

    for (uint32_t i = 0; i < width; i++) {
        EXPECT_EQ(expected[i], dst[i]) << "Mismatch at pixel " << i;
    }
}

// Test grayscale+alpha to ARGB8888 conversion
TEST(NeonSimdTest, GrayAlphaToARGB8888)
{
    const uint32_t width = 16;
    uint8_t src[width * 2];
    uint32_t dst[width];
    uint32_t expected[width];

    for (uint32_t i = 0; i < width; i++) {
        src[i * 2 + 0] = (uint8_t)(i * 16);       // Gray
        src[i * 2 + 1] = (uint8_t)(255 - i * 16); // Alpha

        expected[i] = ((uint32_t)src[i * 2 + 1] << 24) | // A
                      ((uint32_t)src[i * 2 + 0] << 16) | // R (gray)
                      ((uint32_t)src[i * 2 + 0] << 8) |  // G (gray)
                      src[i * 2 + 0];                    // B (gray)
    }

    memset(dst, 0, sizeof(dst));
    neon_graya_to_argb8888(dst, src, width);

    for (uint32_t i = 0; i < width; i++) {
        EXPECT_EQ(expected[i], dst[i]) << "Mismatch at pixel " << i;
    }
}

// Test R/B swap (RGBA <-> BGRA or ARGB <-> ABGR)
TEST(NeonSimdTest, SwapRB)
{
    const uint32_t count = 16;
    uint32_t src[count];
    uint32_t dst[count];
    uint32_t expected[count];

    for (uint32_t i = 0; i < count; i++) {
        // Create test pattern: each pixel has different ARGB values
        uint8_t a = (uint8_t)(255 - i * 10);
        uint8_t r = (uint8_t)(i * 15);
        uint8_t g = (uint8_t)(128);
        uint8_t b = (uint8_t)(255 - i * 15);

        src[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;

        // After swap: R and B are exchanged
        expected[i] = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    }

    memset(dst, 0, sizeof(dst));
    neon_swap_rb(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(expected[i], dst[i]) << "Mismatch at pixel " << i;
    }
}

// Test RGBA to ARGB conversion
TEST(NeonSimdTest, RGBAToARGB)
{
    const uint32_t count = 16;
    uint32_t src[count];
    uint32_t dst[count];
    uint32_t expected[count];

    for (uint32_t i = 0; i < count; i++) {
        uint8_t r = (uint8_t)(i * 15);
        uint8_t g = (uint8_t)(128);
        uint8_t b = (uint8_t)(255 - i * 15);
        uint8_t a = (uint8_t)(200);

        // RGBA format in little-endian memory: A B G R
        src[i] = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;

        // ARGB result after swap R<->B: A R G B in little-endian
        expected[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

    memset(dst, 0, sizeof(dst));
    neon_rgba_to_argb(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(expected[i], dst[i]) << "Mismatch at pixel " << i;
    }
}

// Test fill32
TEST(NeonSimdTest, Fill32)
{
    const uint32_t count = 100;
    uint32_t dst[count];
    const uint32_t value = 0xDEADBEEF;

    memset(dst, 0, sizeof(dst));
    neon_fill32(dst, value, count);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(value, dst[i]) << "Mismatch at index " << i;
    }
}

// Test neon_memcpy
TEST(NeonSimdTest, MemCpy)
{
    const size_t bytes = 256;
    uint8_t src[bytes];
    uint8_t dst[bytes];

    // Fill source with test pattern
    for (size_t i = 0; i < bytes; i++) {
        src[i] = (uint8_t)(i & 0xFF);
    }

    memset(dst, 0, sizeof(dst));
    neon_memcpy(dst, src, bytes);

    EXPECT_EQ(0, memcmp(src, dst, bytes)) << "Memory copy mismatch";
}

// Test edge case: zero-length operations
TEST(NeonSimdTest, ZeroLength)
{
    uint8_t src_rgb[3] = {0xFF, 0x80, 0x40};
    uint32_t dst[1] = {0xDEADBEEF};

    // Zero-length should not modify destination
    neon_rgb888_to_argb8888(dst, src_rgb, 0);
    EXPECT_EQ(0xDEADBEEFu, dst[0]);

    neon_gray_to_argb8888(dst, src_rgb, 0);
    EXPECT_EQ(0xDEADBEEFu, dst[0]);

    neon_swap_rb(dst, dst, 0);
    EXPECT_EQ(0xDEADBEEFu, dst[0]);

    neon_fill32(dst, 0x12345678, 0);
    EXPECT_EQ(0xDEADBEEFu, dst[0]);
}

// Test single pixel operations (boundary case)
TEST(NeonSimdTest, SinglePixel)
{
    uint8_t src_rgb[3] = {0xAB, 0xCD, 0xEF};
    uint32_t dst[1];

    neon_rgb888_to_argb8888(dst, src_rgb, 1);
    EXPECT_EQ(0xFFABCDEFu, dst[0]);

    uint8_t src_gray[1] = {0x80};
    neon_gray_to_argb8888(dst, src_gray, 1);
    EXPECT_EQ(0xFF808080u, dst[0]);
}
