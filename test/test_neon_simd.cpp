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

// Include assembly header for testing (uses intrinsics as fallback on non-ARM)
#include "../src/common/utils/neon_asm.h"

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

// Test glyph row rendering with foreground pixels
TEST(NeonSimdTest, GlyphRowForeground)
{
    // Test rendering a glyph row with some pixels ON
    uint16_t dst[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const uint16_t fg_color = 0xFFFF;  // White
    const uint16_t ol_color = 0x8410;  // Gray
    const uint8_t glyph_row = 0xAA;    // Pattern: 10101010 (pixels 0,2,4,6 ON)

    neon_render_glyph_row(dst, glyph_row, 0, 0, fg_color, ol_color);

    // Verify foreground pixels are set correctly (MSB = pixel 0)
    EXPECT_EQ(fg_color, dst[0]) << "Pixel 0 should be foreground";
    EXPECT_EQ(fg_color, dst[2]) << "Pixel 2 should be foreground";
    EXPECT_EQ(fg_color, dst[4]) << "Pixel 4 should be foreground";
    EXPECT_EQ(fg_color, dst[6]) << "Pixel 6 should be foreground";
}

// Test glyph row rendering with outline detection
TEST(NeonSimdTest, GlyphRowOutline)
{
    // Test rendering a glyph row with outline detection
    uint16_t dst[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const uint16_t fg_color = 0xFFFF;  // White
    const uint16_t ol_color = 0x8410;  // Gray
    const uint8_t glyph_row = 0x10;    // Pattern: 00010000 (only pixel 3 ON)

    neon_render_glyph_row(dst, glyph_row, 0, 0, fg_color, ol_color);

    // Pixel 3 should be foreground
    EXPECT_EQ(fg_color, dst[3]) << "Pixel 3 should be foreground";

    // Pixels 2 and 4 should be outline (adjacent to pixel 3)
    EXPECT_EQ(ol_color, dst[2]) << "Pixel 2 should be outline";
    EXPECT_EQ(ol_color, dst[4]) << "Pixel 4 should be outline";
}

// Test glyph row with vertical neighbors affecting outline
TEST(NeonSimdTest, GlyphRowVerticalOutline)
{
    // Test outline detection with vertical neighbors
    uint16_t dst[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const uint16_t fg_color = 0xFFFF;
    const uint16_t ol_color = 0x8410;
    const uint8_t glyph_row = 0x00;        // All OFF
    const uint8_t glyph_row_above = 0x10;  // Pixel 3 ON above

    neon_render_glyph_row(dst, glyph_row, glyph_row_above, 0, fg_color, ol_color);

    // Pixels 2, 3, 4 should be outline (below the ON pixel above)
    EXPECT_EQ(ol_color, dst[2]) << "Pixel 2 should be outline (diagonal from above)";
    EXPECT_EQ(ol_color, dst[3]) << "Pixel 3 should be outline (directly below)";
    EXPECT_EQ(ol_color, dst[4]) << "Pixel 4 should be outline (diagonal from above)";
}

// Test full 8x8 glyph rendering
TEST(NeonSimdTest, GlyphRender8x8)
{
    // Simple vertical line in center (pixel 3 in each row)
    uint8_t glyph[8] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10};
    const uint32_t stride = 16;  // 16 pixels per row (wider than glyph)
    uint16_t buffer[16 * 8] = {0};  // 8 rows, 16 pixels wide

    const uint16_t fg_color = 0xFFFF;
    const uint16_t ol_color = 0x8410;

    neon_render_glyph_8x8(buffer, glyph, stride, fg_color, ol_color);

    // Verify center column is foreground
    for (int row = 0; row < 8; row++) {
        EXPECT_EQ(fg_color, buffer[row * stride + 3])
            << "Pixel at row " << row << ", col 3 should be foreground";
    }

    // Verify adjacent columns are outline
    for (int row = 0; row < 8; row++) {
        EXPECT_EQ(ol_color, buffer[row * stride + 2])
            << "Pixel at row " << row << ", col 2 should be outline";
        EXPECT_EQ(ol_color, buffer[row * stride + 4])
            << "Pixel at row " << row << ", col 4 should be outline";
    }
}

// Test empty glyph (no rendering)
TEST(NeonSimdTest, GlyphRenderEmpty)
{
    uint8_t glyph[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // All OFF
    const uint32_t stride = 8;
    uint16_t buffer[8 * 8];

    // Fill with sentinel values
    for (int i = 0; i < 64; i++) {
        buffer[i] = 0xDEAD;
    }

    const uint16_t fg_color = 0xFFFF;
    const uint16_t ol_color = 0x8410;

    neon_render_glyph_8x8(buffer, glyph, stride, fg_color, ol_color);

    // All pixels should remain unchanged (empty glyph, no outline)
    for (int i = 0; i < 64; i++) {
        EXPECT_EQ(0xDEADu, buffer[i]) << "Pixel " << i << " should be unchanged";
    }
}

// ============================================================================
// Tests for NEON Assembly Macros (use intrinsics fallback on non-ARM)
// ============================================================================

// Test NEON_RGB888_TO_ARGB8888 macro
TEST(NeonAsmMacroTest, RGB888ToARGB8888Macro)
{
    const uint32_t width = 16;
    uint8_t src[width * 3];
    uint32_t dst[width];
    uint32_t expected[width];

    for (uint32_t i = 0; i < width; i++) {
        src[i * 3 + 0] = (uint8_t)(i * 10);
        src[i * 3 + 1] = (uint8_t)(i * 10 + 50);
        src[i * 3 + 2] = (uint8_t)(i * 10 + 100);

        expected[i] = 0xFF000000u |
                      ((uint32_t)src[i * 3 + 0] << 16) |
                      ((uint32_t)src[i * 3 + 1] << 8) |
                      src[i * 3 + 2];
    }

    memset(dst, 0, sizeof(dst));
    NEON_RGB888_TO_ARGB8888(dst, src, width);

    for (uint32_t i = 0; i < width; i++) {
        EXPECT_EQ(expected[i], dst[i]) << "Mismatch at pixel " << i;
    }
}

// Test NEON_GRAY_TO_ARGB8888 macro
TEST(NeonAsmMacroTest, GrayToARGB8888Macro)
{
    const uint32_t width = 16;
    uint8_t src[width];
    uint32_t dst[width];
    uint32_t expected[width];

    for (uint32_t i = 0; i < width; i++) {
        src[i] = (uint8_t)(i * 16);
        expected[i] = 0xFF000000u |
                      ((uint32_t)src[i] << 16) |
                      ((uint32_t)src[i] << 8) |
                      src[i];
    }

    memset(dst, 0, sizeof(dst));
    NEON_GRAY_TO_ARGB8888(dst, src, width);

    for (uint32_t i = 0; i < width; i++) {
        EXPECT_EQ(expected[i], dst[i]) << "Mismatch at pixel " << i;
    }
}

// Test NEON_SWAP_RB macro
TEST(NeonAsmMacroTest, SwapRBMacro)
{
    const uint32_t count = 16;
    uint32_t src[count];
    uint32_t dst[count];
    uint32_t expected[count];

    for (uint32_t i = 0; i < count; i++) {
        uint8_t a = (uint8_t)(255 - i * 10);
        uint8_t r = (uint8_t)(i * 15);
        uint8_t g = (uint8_t)(128);
        uint8_t b = (uint8_t)(255 - i * 15);

        src[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        expected[i] = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    }

    memset(dst, 0, sizeof(dst));
    NEON_SWAP_RB(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(expected[i], dst[i]) << "Mismatch at pixel " << i;
    }
}

// Test NEON_FILL32 macro
TEST(NeonAsmMacroTest, Fill32Macro)
{
    const uint32_t count = 100;
    uint32_t dst[count];
    const uint32_t value = 0xCAFEBABE;

    memset(dst, 0, sizeof(dst));
    NEON_FILL32(dst, value, count);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(value, dst[i]) << "Mismatch at index " << i;
    }
}

// Test NEON_MEMCPY macro
TEST(NeonAsmMacroTest, MemCpyMacro)
{
    const size_t bytes = 256;
    uint8_t src[bytes];
    uint8_t dst[bytes];

    for (size_t i = 0; i < bytes; i++) {
        src[i] = (uint8_t)(i & 0xFF);
    }

    memset(dst, 0, sizeof(dst));
    NEON_MEMCPY(dst, src, bytes);

    EXPECT_EQ(0, memcmp(src, dst, bytes)) << "Memory copy mismatch";
}
