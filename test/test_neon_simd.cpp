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

// ============================================================================
// Tests for YUV to RGB Conversion
// ============================================================================

// Test YUV420 to ARGB8888 conversion (single row)
TEST(NeonSimdTest, YUV420ToARGB8888Row)
{
    const uint32_t width = 16;
    uint8_t y_row[width];
    uint8_t u_row[width / 2];
    uint8_t v_row[width / 2];
    uint32_t dst[width];
    uint32_t expected[width];

    // Test pattern: gradient in Y, neutral U/V (should produce grayscale)
    for (uint32_t i = 0; i < width; i++) {
        y_row[i] = (uint8_t)(16 + i * 14);  // Y: 16-235 range
    }
    for (uint32_t i = 0; i < width / 2; i++) {
        u_row[i] = 128;  // Neutral U (no blue/yellow shift)
        v_row[i] = 128;  // Neutral V (no red/green shift)
    }

    // Calculate expected values using BT.601 formula
    for (uint32_t col = 0; col < width; col++) {
        int y = y_row[col] - 16;
        int u = u_row[col / 2] - 128;
        int v = v_row[col / 2] - 128;

        int r = (298 * y + 409 * v + 128) >> 8;
        int g = (298 * y - 100 * u - 208 * v + 128) >> 8;
        int b = (298 * y + 516 * u + 128) >> 8;

        r = r < 0 ? 0 : (r > 255 ? 255 : r);
        g = g < 0 ? 0 : (g > 255 ? 255 : g);
        b = b < 0 ? 0 : (b > 255 ? 255 : b);

        expected[col] = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }

    // Run NEON-optimized conversion
    memset(dst, 0, sizeof(dst));
    NEON_YUV420_TO_ARGB8888_ROW(dst, y_row, u_row, v_row, width);

    // Verify results (allow small rounding differences)
    for (uint32_t i = 0; i < width; i++) {
        uint32_t exp_r = (expected[i] >> 16) & 0xFF;
        uint32_t exp_g = (expected[i] >> 8) & 0xFF;
        uint32_t exp_b = expected[i] & 0xFF;

        uint32_t got_r = (dst[i] >> 16) & 0xFF;
        uint32_t got_g = (dst[i] >> 8) & 0xFF;
        uint32_t got_b = dst[i] & 0xFF;

        EXPECT_NEAR(exp_r, got_r, 2) << "R mismatch at pixel " << i;
        EXPECT_NEAR(exp_g, got_g, 2) << "G mismatch at pixel " << i;
        EXPECT_NEAR(exp_b, got_b, 2) << "B mismatch at pixel " << i;
    }
}

// Test YUV420 to ARGB8888 with color (red tint)
TEST(NeonSimdTest, YUV420ToARGB8888RowColor)
{
    const uint32_t width = 8;
    uint8_t y_row[width];
    uint8_t u_row[width / 2];
    uint8_t v_row[width / 2];
    uint32_t dst[width];

    // Fill with mid-gray Y and high V (should produce red-ish)
    for (uint32_t i = 0; i < width; i++) {
        y_row[i] = 128;
    }
    for (uint32_t i = 0; i < width / 2; i++) {
        u_row[i] = 128;  // Neutral U
        v_row[i] = 200;  // High V = more red
    }

    memset(dst, 0, sizeof(dst));
    NEON_YUV420_TO_ARGB8888_ROW(dst, y_row, u_row, v_row, width);

    // All pixels should have red > blue (high V shifts towards red)
    for (uint32_t i = 0; i < width; i++) {
        uint32_t r = (dst[i] >> 16) & 0xFF;
        uint32_t b = dst[i] & 0xFF;
        EXPECT_GT(r, b) << "R should be greater than B at pixel " << i << " due to high V";
    }
}

// ============================================================================
// Tests for Box Blur
// ============================================================================

// Test box blur with radius 0 (should be identity)
TEST(NeonSimdTest, BoxBlurRadius0)
{
    const uint32_t width = 16;
    uint32_t src[width];
    uint32_t dst[width];

    for (uint32_t i = 0; i < width; i++) {
        src[i] = 0xFF000000u | (i << 16) | (i << 8) | i;
    }

    NEON_BOX_BLUR_ROW(dst, src, width, 0);

    // Should be identical to source
    for (uint32_t i = 0; i < width; i++) {
        EXPECT_EQ(src[i], dst[i]) << "Radius 0 should be identity at pixel " << i;
    }
}

// Test box blur with radius 1 (3-pixel kernel)
TEST(NeonSimdTest, BoxBlurRadius1)
{
    const uint32_t width = 16;
    uint32_t src[width];
    uint32_t dst[width];

    // Create a simple pattern: all zeros except center pixel is white
    for (uint32_t i = 0; i < width; i++) {
        src[i] = 0xFF000000u;  // Black with full alpha
    }
    src[8] = 0xFFFFFFFF;  // White pixel in center

    NEON_BOX_BLUR_ROW(dst, src, width, 1);

    // Center and neighbors should be affected
    // With radius 1, kernel is 3 pixels. Due to boundary mirroring, the actual
    // average depends on neighboring pixels. The center should be diluted.
    uint32_t center_r = (dst[8] >> 16) & 0xFF;
    EXPECT_GT(center_r, 0u) << "Center pixel should be affected by blur";
    EXPECT_LT(center_r, 255u) << "Center pixel should be diluted";

    // Neighbors should also have some white
    uint32_t left_r = (dst[7] >> 16) & 0xFF;
    uint32_t right_r = (dst[9] >> 16) & 0xFF;
    EXPECT_GT(left_r, 0u) << "Left neighbor should be affected";
    EXPECT_GT(right_r, 0u) << "Right neighbor should be affected";
}

// Test box blur preserves uniform color
TEST(NeonSimdTest, BoxBlurUniformColor)
{
    const uint32_t width = 16;
    uint32_t src[width];
    uint32_t dst[width];

    // All pixels same color
    for (uint32_t i = 0; i < width; i++) {
        src[i] = 0xFF808080u;  // Mid-gray
    }

    NEON_BOX_BLUR_ROW(dst, src, width, 2);

    // All pixels should remain the same (within rounding)
    for (uint32_t i = 0; i < width; i++) {
        uint32_t r = (dst[i] >> 16) & 0xFF;
        uint32_t g = (dst[i] >> 8) & 0xFF;
        uint32_t b = dst[i] & 0xFF;
        EXPECT_NEAR(r, 128u, 1) << "R should be preserved at pixel " << i;
        EXPECT_NEAR(g, 128u, 1) << "G should be preserved at pixel " << i;
        EXPECT_NEAR(b, 128u, 1) << "B should be preserved at pixel " << i;
    }
}

// ============================================================================
// Tests for Dithering
// ============================================================================

// Test basic dithering conversion
TEST(NeonSimdTest, DitherARGB8888ToRGB565)
{
    const uint32_t width = 16;
    uint32_t src[width];
    uint16_t dst[width];

    // Create gradient
    for (uint32_t i = 0; i < width; i++) {
        uint8_t gray = (uint8_t)(i * 16);
        src[i] = 0xFF000000u | ((uint32_t)gray << 16) | ((uint32_t)gray << 8) | gray;
    }

    NEON_DITHER_ARGB8888_TO_RGB565(dst, src, width, 0);

    // Verify conversion produces valid RGB565 values
    for (uint32_t i = 0; i < width; i++) {
        uint16_t pixel = dst[i];
        uint8_t r5 = (pixel >> 11) & 0x1F;
        uint8_t g6 = (pixel >> 5) & 0x3F;
        uint8_t b5 = pixel & 0x1F;

        // R and B should be similar (grayscale input)
        EXPECT_NEAR(r5, b5, 2) << "R5 and B5 should be similar at pixel " << i;
        // G should be approximately 2x (6 bits vs 5 bits)
        EXPECT_NEAR(g6, r5 * 2, 3) << "G6 should be ~2x R5 at pixel " << i;
    }
}

// Test dithering reduces banding
TEST(NeonSimdTest, DitherReducesBanding)
{
    const uint32_t width = 16;
    uint32_t src[width];
    uint16_t dst_y0[width];
    uint16_t dst_y1[width];

    // Create uniform color that would cause banding without dithering
    for (uint32_t i = 0; i < width; i++) {
        src[i] = 0xFF404040u;  // Dark gray
    }

    // Convert two consecutive rows
    NEON_DITHER_ARGB8888_TO_RGB565(dst_y0, src, width, 0);
    NEON_DITHER_ARGB8888_TO_RGB565(dst_y1, src, width, 1);

    // Dithering should produce different patterns on different rows
    int differences = 0;
    for (uint32_t i = 0; i < width; i++) {
        if (dst_y0[i] != dst_y1[i]) {
            differences++;
        }
    }

    // Some pixels should differ due to dithering pattern
    EXPECT_GT(differences, 0) << "Dithering should create pattern differences between rows";
}

// Test dithering with pure colors
TEST(NeonSimdTest, DitherPureColors)
{
    const uint32_t width = 8;
    uint32_t src[width];
    uint16_t dst[width];

    // Pure red
    for (uint32_t i = 0; i < width; i++) {
        src[i] = 0xFFFF0000u;  // Red
    }

    NEON_DITHER_ARGB8888_TO_RGB565(dst, src, width, 0);

    // All pixels should be red-ish (high R, low G, low B)
    for (uint32_t i = 0; i < width; i++) {
        uint8_t r5 = (dst[i] >> 11) & 0x1F;
        uint8_t g6 = (dst[i] >> 5) & 0x3F;
        uint8_t b5 = dst[i] & 0x1F;

        EXPECT_GT(r5, 28u) << "R channel should be high at pixel " << i;
        EXPECT_LT(g6, 5u) << "G channel should be low at pixel " << i;
        EXPECT_LT(b5, 3u) << "B channel should be low at pixel " << i;
    }
}

// ============================================================================
// Tests for Alpha Blending
// ============================================================================

// Test alpha blending with 50% alpha
TEST(NeonSimdTest, AlphaBlend50Percent)
{
    const uint32_t count = 16;
    uint32_t src[count];
    uint32_t dst[count];
    uint32_t expected[count];

    // Create source with 50% alpha (0x80 = 128)
    // Create white source over black destination
    for (uint32_t i = 0; i < count; i++) {
        src[i] = 0x80FFFFFF;  // 50% white
        dst[i] = 0xFF000000;  // Opaque black
        // Expected: ~50% gray (127-128 in each channel)
        // Formula: result = src * alpha + dst * (1-alpha)
        // R = 255 * 128/255 + 0 * 127/255 ≈ 128
        expected[i] = 0x80808080;
    }

    neon_alpha_blend(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t got_r = (dst[i] >> 16) & 0xFF;
        uint32_t got_g = (dst[i] >> 8) & 0xFF;
        uint32_t got_b = dst[i] & 0xFF;
        uint32_t exp_r = (expected[i] >> 16) & 0xFF;

        // Allow small rounding differences
        EXPECT_NEAR(got_r, exp_r, 2) << "R mismatch at pixel " << i;
        EXPECT_NEAR(got_g, exp_r, 2) << "G mismatch at pixel " << i;
        EXPECT_NEAR(got_b, exp_r, 2) << "B mismatch at pixel " << i;
    }
}

// Test alpha blending with fully opaque source
TEST(NeonSimdTest, AlphaBlendOpaque)
{
    const uint32_t count = 8;
    uint32_t src[count];
    uint32_t dst[count];

    // Fully opaque source should completely replace destination
    for (uint32_t i = 0; i < count; i++) {
        src[i] = 0xFFFF0000;  // Opaque red
        dst[i] = 0xFF00FF00;  // Opaque green
    }

    neon_alpha_blend(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        // With 100% alpha, destination should be source color
        // Allow small rounding error (254-255 due to (x * 255 + 128) >> 8 approximation)
        uint32_t got_r = (dst[i] >> 16) & 0xFF;
        uint32_t got_g = (dst[i] >> 8) & 0xFF;
        uint32_t got_b = dst[i] & 0xFF;

        EXPECT_NEAR(got_r, 255u, 1) << "R should be ~255 at pixel " << i;
        EXPECT_EQ(0u, got_g) << "G should be 0 at pixel " << i;
        EXPECT_EQ(0u, got_b) << "B should be 0 at pixel " << i;
    }
}

// Test alpha blending with fully transparent source
TEST(NeonSimdTest, AlphaBlendTransparent)
{
    const uint32_t count = 8;
    uint32_t src[count];
    uint32_t dst[count];
    uint32_t original[count];

    // Fully transparent source should not change destination
    for (uint32_t i = 0; i < count; i++) {
        src[i] = 0x00FF0000;  // Transparent red
        dst[i] = 0xFF00FF00;  // Opaque green
        original[i] = dst[i];
    }

    neon_alpha_blend(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        // With 0% alpha, destination RGB should remain unchanged
        // Allow small rounding error (254-255 due to approximation)
        uint32_t got_r = (dst[i] >> 16) & 0xFF;
        uint32_t got_g = (dst[i] >> 8) & 0xFF;
        uint32_t got_b = dst[i] & 0xFF;

        EXPECT_EQ(0u, got_r) << "R should be 0 at pixel " << i;
        EXPECT_NEAR(got_g, 255u, 1) << "G should be ~255 at pixel " << i;
        EXPECT_EQ(0u, got_b) << "B should be 0 at pixel " << i;
    }
}

// ============================================================================
// Tests for Alpha Pre-multiplication
// ============================================================================

// Test premultiply with various alpha values
TEST(NeonSimdTest, PremultiplyAlpha)
{
    const uint32_t count = 16;
    uint32_t src[count];
    uint32_t dst[count];
    uint32_t expected[count];

    for (uint32_t i = 0; i < count; i++) {
        // Create pixels with varying alpha
        uint8_t a = (uint8_t)(i * 16);  // 0, 16, 32, ... 240
        uint8_t r = 200;
        uint8_t g = 100;
        uint8_t b = 50;

        src[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;

        // Expected: each channel multiplied by alpha/255
        uint32_t pr = (r * a + 128) >> 8;
        uint32_t pg = (g * a + 128) >> 8;
        uint32_t pb = (b * a + 128) >> 8;

        expected[i] = ((uint32_t)a << 24) | (pr << 16) | (pg << 8) | pb;
    }

    memset(dst, 0, sizeof(dst));
    neon_premultiply_alpha(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(expected[i], dst[i]) << "Mismatch at pixel " << i;
    }
}

// Test premultiply with opaque pixels (should be nearly unchanged)
TEST(NeonSimdTest, PremultiplyAlphaOpaque)
{
    const uint32_t count = 8;
    uint32_t src[count];
    uint32_t dst[count];

    for (uint32_t i = 0; i < count; i++) {
        // Fully opaque pixels: RGB * 255 / 255 should be nearly unchanged
        // Using (C * 255 + 128) >> 8 gives slightly different results
        src[i] = 0xFFABCDEF;
    }

    neon_premultiply_alpha(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        // Allow small rounding differences due to (C * A + 128) >> 8 approximation
        uint32_t src_r = (src[i] >> 16) & 0xFF;
        uint32_t src_g = (src[i] >> 8) & 0xFF;
        uint32_t src_b = src[i] & 0xFF;
        uint32_t dst_r = (dst[i] >> 16) & 0xFF;
        uint32_t dst_g = (dst[i] >> 8) & 0xFF;
        uint32_t dst_b = dst[i] & 0xFF;

        EXPECT_NEAR(dst_r, src_r, 1) << "R should be nearly unchanged at " << i;
        EXPECT_NEAR(dst_g, src_g, 1) << "G should be nearly unchanged at " << i;
        EXPECT_NEAR(dst_b, src_b, 1) << "B should be nearly unchanged at " << i;
    }
}

// Test premultiply with transparent pixels (should be zeroed RGB)
TEST(NeonSimdTest, PremultiplyAlphaTransparent)
{
    const uint32_t count = 8;
    uint32_t src[count];
    uint32_t dst[count];

    for (uint32_t i = 0; i < count; i++) {
        // Fully transparent pixels should have RGB = 0
        src[i] = 0x00FFFFFF;
    }

    neon_premultiply_alpha(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        EXPECT_EQ(0x00000000u, dst[i]) << "Transparent pixel should be all zeros at " << i;
    }
}

// ============================================================================
// Tests for Nearest-Neighbor Scaling
// ============================================================================

// Test scaling 2x (upscale)
TEST(NeonSimdTest, ScaleNearest2x)
{
    const uint32_t src_w = 4, src_h = 4;
    const uint32_t dst_w = 8, dst_h = 8;
    uint32_t src[src_w * src_h];
    uint32_t dst[dst_w * dst_h];

    // Create 4x4 source with distinct colors in each quadrant
    src[0] = 0xFFFF0000; src[1] = 0xFFFF0000; src[2] = 0xFF00FF00; src[3] = 0xFF00FF00;
    src[4] = 0xFFFF0000; src[5] = 0xFFFF0000; src[6] = 0xFF00FF00; src[7] = 0xFF00FF00;
    src[8] = 0xFF0000FF; src[9] = 0xFF0000FF; src[10] = 0xFFFFFF00; src[11] = 0xFFFFFF00;
    src[12] = 0xFF0000FF; src[13] = 0xFF0000FF; src[14] = 0xFFFFFF00; src[15] = 0xFFFFFF00;

    memset(dst, 0, sizeof(dst));
    neon_scale_nearest(dst, src, dst_w, dst_h, src_w, src_h);

    // Check that scaling preserved colors (each source pixel becomes 2x2 block)
    // Top-left quadrant should be red
    EXPECT_EQ(0xFFFF0000u, dst[0]);
    EXPECT_EQ(0xFFFF0000u, dst[1]);
    EXPECT_EQ(0xFFFF0000u, dst[dst_w]);
    EXPECT_EQ(0xFFFF0000u, dst[dst_w + 1]);

    // Top-right quadrant should be green
    EXPECT_EQ(0xFF00FF00u, dst[4]);
    EXPECT_EQ(0xFF00FF00u, dst[5]);

    // Bottom-left quadrant should be blue
    EXPECT_EQ(0xFF0000FFu, dst[4 * dst_w]);
    EXPECT_EQ(0xFF0000FFu, dst[4 * dst_w + 1]);

    // Bottom-right quadrant should be yellow
    EXPECT_EQ(0xFFFFFF00u, dst[4 * dst_w + 4]);
}

// Test scaling with zero dimensions (should not crash)
TEST(NeonSimdTest, ScaleNearestZeroDimensions)
{
    uint32_t src[16] = {0xFFFFFFFF};
    uint32_t dst[16] = {0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF};

    // Zero destination width - should return without modification
    neon_scale_nearest(dst, src, 0, 4, 4, 4);
    EXPECT_EQ(0xDEADBEEFu, dst[0]) << "Zero dst_width should not modify destination";

    // Zero destination height - should return without modification
    neon_scale_nearest(dst, src, 4, 0, 4, 4);
    EXPECT_EQ(0xDEADBEEFu, dst[0]) << "Zero dst_height should not modify destination";

    // Zero source dimensions - should return without modification
    neon_scale_nearest(dst, src, 4, 4, 0, 4);
    EXPECT_EQ(0xDEADBEEFu, dst[0]) << "Zero src_width should not modify destination";
}

// Test scaling 0.5x (downscale)
TEST(NeonSimdTest, ScaleNearestHalf)
{
    const uint32_t src_w = 8, src_h = 8;
    const uint32_t dst_w = 4, dst_h = 4;
    uint32_t src[src_w * src_h];
    uint32_t dst[dst_w * dst_h];

    // Create 8x8 source with 2x2 blocks of the same color
    for (uint32_t y = 0; y < src_h; y++) {
        for (uint32_t x = 0; x < src_w; x++) {
            // Color based on which quadrant (2x2 blocks)
            uint32_t qx = x / 2;
            uint32_t qy = y / 2;
            uint32_t color = ((qx + qy * 4) * 30) & 0xFF;
            src[y * src_w + x] = 0xFF000000 | (color << 16) | (color << 8) | color;
        }
    }

    memset(dst, 0, sizeof(dst));
    neon_scale_nearest(dst, src, dst_w, dst_h, src_w, src_h);

    // Each destination pixel should be one of the source pixels
    // Just verify we got valid pixels (non-zero)
    for (uint32_t i = 0; i < dst_w * dst_h; i++) {
        EXPECT_NE(0u, dst[i]) << "Destination pixel should not be zero at " << i;
        EXPECT_EQ(0xFFu, (dst[i] >> 24) & 0xFF) << "Alpha should be preserved at " << i;
    }
}

// Test scaling identity (1:1)
TEST(NeonSimdTest, ScaleNearestIdentity)
{
    const uint32_t size = 16;
    uint32_t src[size * size];
    uint32_t dst[size * size];

    // Create gradient pattern
    for (uint32_t i = 0; i < size * size; i++) {
        src[i] = 0xFF000000 | (i * 0x010101);
    }

    neon_scale_nearest(dst, src, size, size, size, size);

    // Identity scaling should produce exact copy
    for (uint32_t i = 0; i < size * size; i++) {
        EXPECT_EQ(src[i], dst[i]) << "Identity scale should be exact copy at " << i;
    }
}

// ============================================================================
// Tests for Pre-multiplied Alpha Blending
// ============================================================================

// Test blending pre-multiplied pixels
TEST(NeonSimdTest, BlendPremultiplied50Percent)
{
    const uint32_t count = 8;
    uint32_t src[count];
    uint32_t dst[count];

    // Create pre-multiplied 50% white over black
    // Pre-multiplied: RGB already multiplied by alpha
    // So 50% white = 0x80808080 (not 0x80FFFFFF)
    for (uint32_t i = 0; i < count; i++) {
        src[i] = 0x80808080;  // Pre-multiplied 50% white
        dst[i] = 0xFF000000;  // Opaque black (pre-mult same as straight)
    }

    neon_blend_premultiplied(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        // Result should be approximately 50% gray
        uint32_t got_r = (dst[i] >> 16) & 0xFF;
        uint32_t got_g = (dst[i] >> 8) & 0xFF;
        uint32_t got_b = dst[i] & 0xFF;

        EXPECT_NEAR(got_r, 128u, 2) << "R mismatch at pixel " << i;
        EXPECT_NEAR(got_g, 128u, 2) << "G mismatch at pixel " << i;
        EXPECT_NEAR(got_b, 128u, 2) << "B mismatch at pixel " << i;
    }
}

// Test blending with opaque pre-multiplied source
TEST(NeonSimdTest, BlendPremultipliedOpaque)
{
    const uint32_t count = 8;
    uint32_t src[count];
    uint32_t dst[count];

    for (uint32_t i = 0; i < count; i++) {
        src[i] = 0xFFFF0000;  // Opaque red (pre-mult same as straight)
        dst[i] = 0xFF00FF00;  // Opaque green
    }

    neon_blend_premultiplied(dst, src, count);

    for (uint32_t i = 0; i < count; i++) {
        // Opaque source should completely replace destination
        uint32_t got_r = (dst[i] >> 16) & 0xFF;
        uint32_t got_g = (dst[i] >> 8) & 0xFF;

        EXPECT_EQ(255u, got_r) << "R should be 255 at pixel " << i;
        EXPECT_EQ(0u, got_g) << "G should be 0 at pixel " << i;
    }
}
