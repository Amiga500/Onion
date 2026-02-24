/**
 * @file test_display.c
 * @brief Unit tests for display brightness calculations and buffer operations
 *
 * Tests the pure-math brightness functions from system/display.h:
 * - setBrightness: level (0–10) → raw PWM duty cycle via exponential curve
 * - getBrightnessFromRaw: raw duty cycle → level via logarithmic inverse
 * - display_readOrWriteBuffer: pixel buffer operations with rotation/masking
 *
 * The brightness functions form an inverse pair:
 *   setBrightness(n) produces raw = round(3.0 * exp(0.350656 * n))
 *   getBrightnessFromRaw(raw) produces n = round(log(raw / 3.0) / 0.350656)
 *
 * Build and run: make -f Makefile.unit test_display
 */

#include "onion_test.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- Inline brightness calculations from display.h ---- */

/**
 * Convert brightness level (0–10) to raw PWM duty cycle.
 * Exponential curve: raw = round(3.0 * exp(0.350656 * level))
 */
static int brightness_to_raw(int level)
{
    return round(3.0 * exp(0.350656 * level));
}

/**
 * Convert raw PWM duty cycle back to brightness level (0–10).
 * Logarithmic inverse: level = round(log(raw / 3.0) / 0.350656)
 */
static int brightness_from_raw(int value_raw)
{
    if (value_raw <= 0)
        return 0;
    return round(log(value_raw / 3.0) / 0.350656);
}

/* ---- Minimal display_t for buffer tests ---- */

typedef struct {
    uint32_t xres;
    uint32_t yres;
    uint32_t yres_virtual;
    uint32_t yoffset;
} test_vinfo_t;

typedef struct {
    uint32_t *fb_addr;
    test_vinfo_t vinfo;
} test_display_t;

typedef struct {
    int x, y, w, h;
} test_rect_t;

/**
 * Simplified version of display_readOrWriteBuffer from display.h.
 * Tests pixel read/write operations with rotation and masking.
 */
static void test_readOrWriteBuffer(int index, test_display_t *display,
                                   uint32_t *pixels, test_rect_t rect,
                                   bool rotate, bool mask, bool write)
{
    int bufferPos = index * (int)display->vinfo.yres;

    for (int oy = 0; oy < rect.h; oy++) {
        int y = rect.y + oy;

        if (y < 0 || y >= (int)display->vinfo.yres)
            continue;

        int virtualY = bufferPos + (rotate ? (int)(display->vinfo.yres - 1) - y : y);
        long baseOffset = (long)virtualY * (long)display->vinfo.xres;
        int baseIndex = oy * rect.w;

        /* Fast path: non-rotated, non-masked, contiguous row */
        if (!rotate && !mask && rect.x >= 0 &&
            rect.x + rect.w <= (int)display->vinfo.xres) {
            long rowOffset = baseOffset + (long)rect.x;
            if (write) {
                memcpy(&display->fb_addr[rowOffset], &pixels[baseIndex],
                       rect.w * sizeof(uint32_t));
            }
            else {
                memcpy(&pixels[baseIndex], &display->fb_addr[rowOffset],
                       rect.w * sizeof(uint32_t));
            }
            continue;
        }

        for (int ox = 0; ox < rect.w; ox++) {
            int x = rect.x + ox;

            if (rotate) {
                x = (int)(display->vinfo.xres - 1) - x;
            }

            if (x < 0 || x >= (int)display->vinfo.xres)
                continue;

            long offset = baseOffset + (long)x;
            int idx = baseIndex + ox;
            if (write) {
                if (mask) {
                    if (pixels[idx] != 0) {
                        display->fb_addr[offset] = 0;
                    }
                }
                else {
                    display->fb_addr[offset] = pixels[idx];
                }
            }
            else {
                if (mask) {
                    pixels[idx] = display->fb_addr[offset] == 0 ? 1 : 0;
                }
                else {
                    pixels[idx] = display->fb_addr[offset];
                }
            }
        }
    }
}

/* ---- Tests: brightness curve boundaries ---- */

TEST(brightness_level_0_raw) {
    /* Level 0: round(3.0 * exp(0)) = round(3.0) = 3 */
    ASSERT_EQ(brightness_to_raw(0), 3);
}

TEST(brightness_level_10_raw) {
    /* Level 10: round(3.0 * exp(3.50656)) = round(3.0 * 33.33) ≈ 100 */
    int raw = brightness_to_raw(10);
    ASSERT_EQ(raw, 100);
}

TEST(brightness_level_5_raw) {
    /* Level 5: round(3.0 * exp(1.75328)) ≈ round(17.32) ≈ 17 */
    int raw = brightness_to_raw(5);
    ASSERT_EQ(raw, 17);
}

/* ---- Tests: brightness curve is monotonically increasing ---- */

TEST(brightness_curve_monotonic) {
    int prev = brightness_to_raw(0);
    for (int level = 1; level <= 10; level++) {
        int curr = brightness_to_raw(level);
        ASSERT_GT(curr, prev);
        prev = curr;
    }
}

/* ---- Tests: brightness inverse (raw → level) ---- */

TEST(brightness_from_raw_zero) {
    ASSERT_EQ(brightness_from_raw(0), 0);
}

TEST(brightness_from_raw_negative) {
    ASSERT_EQ(brightness_from_raw(-5), 0);
}

/* ---- Tests: brightness roundtrip (level → raw → level) ---- */

TEST(brightness_roundtrip_all_levels) {
    for (int level = 0; level <= 10; level++) {
        int raw = brightness_to_raw(level);
        int recovered = brightness_from_raw(raw);
        if (recovered != level) {
            fprintf(stderr, "    Level %d → raw %d → recovered %d\n",
                    level, raw, recovered);
        }
        ASSERT_EQ(recovered, level);
    }
}

/* ---- Tests: brightness complete curve values ---- */

TEST(brightness_curve_all_values) {
    /* Complete brightness curve - documents the exact duty cycle for each level */
    int expected_raw[] = {
        3,   /* level 0 */
        4,   /* level 1 */
        6,   /* level 2 */
        9,   /* level 3 */
        12,  /* level 4 */
        17,  /* level 5 */
        25,  /* level 6 */
        35,  /* level 7 */
        50,  /* level 8 */
        70,  /* level 9 */
        100, /* level 10 */
    };

    for (int level = 0; level <= 10; level++) {
        int raw = brightness_to_raw(level);
        if (raw != expected_raw[level]) {
            fprintf(stderr, "    Brightness %d: expected raw %d, got %d\n",
                    level, expected_raw[level], raw);
        }
        ASSERT_EQ(raw, expected_raw[level]);
    }
}

/* ---- Tests: display buffer operations ---- */

/* Helper to create a test display with a framebuffer */
static test_display_t create_test_display(int w, int h)
{
    test_display_t d;
    d.vinfo.xres = w;
    d.vinfo.yres = h;
    d.vinfo.yres_virtual = h;
    d.vinfo.yoffset = 0;
    d.fb_addr = calloc(w * h, sizeof(uint32_t));
    return d;
}

static void free_test_display(test_display_t *d)
{
    free(d->fb_addr);
    d->fb_addr = NULL;
}

TEST(buffer_write_simple) {
    test_display_t disp = create_test_display(8, 8);
    uint32_t pixels[4] = {0xAABBCCDD, 0x11223344, 0x55667788, 0x99AABBCC};
    test_rect_t rect = {0, 0, 2, 2};

    test_readOrWriteBuffer(0, &disp, pixels, rect, false, false, true);

    /* Check pixels were written to framebuffer */
    ASSERT_EQ(disp.fb_addr[0], 0xAABBCCDD);  /* (0,0) */
    ASSERT_EQ(disp.fb_addr[1], 0x11223344);  /* (1,0) */
    ASSERT_EQ(disp.fb_addr[8], 0x55667788);  /* (0,1) - row 1 starts at offset 8 */
    ASSERT_EQ(disp.fb_addr[9], 0x99AABBCC);  /* (1,1) */

    free_test_display(&disp);
}

TEST(buffer_read_simple) {
    test_display_t disp = create_test_display(8, 8);
    disp.fb_addr[0] = 0x12345678;
    disp.fb_addr[1] = 0x9ABCDEF0;

    uint32_t pixels[2] = {0};
    test_rect_t rect = {0, 0, 2, 1};

    test_readOrWriteBuffer(0, &disp, pixels, rect, false, false, false);

    ASSERT_EQ(pixels[0], 0x12345678);
    ASSERT_EQ(pixels[1], 0x9ABCDEF0);

    free_test_display(&disp);
}

TEST(buffer_write_with_offset) {
    test_display_t disp = create_test_display(8, 8);
    uint32_t pixels[2] = {0xAAAAAAAA, 0xBBBBBBBB};
    test_rect_t rect = {3, 2, 2, 1};

    test_readOrWriteBuffer(0, &disp, pixels, rect, false, false, true);

    /* Row 2, columns 3-4 */
    ASSERT_EQ(disp.fb_addr[2 * 8 + 3], 0xAAAAAAAA);
    ASSERT_EQ(disp.fb_addr[2 * 8 + 4], 0xBBBBBBBB);
    /* Surrounding pixels should be zero */
    ASSERT_EQ(disp.fb_addr[2 * 8 + 2], 0);
    ASSERT_EQ(disp.fb_addr[2 * 8 + 5], 0);

    free_test_display(&disp);
}

TEST(buffer_write_rotated) {
    test_display_t disp = create_test_display(4, 4);
    uint32_t pixels[1] = {0xDEADBEEF};
    test_rect_t rect = {1, 1, 1, 1};

    test_readOrWriteBuffer(0, &disp, pixels, rect, true, false, true);

    /* With rotation: x becomes (xres-1-x) = 2, y becomes (yres-1-y) = 2 */
    ASSERT_EQ(disp.fb_addr[2 * 4 + 2], 0xDEADBEEF);
    /* Original position should be zero */
    ASSERT_EQ(disp.fb_addr[1 * 4 + 1], 0);

    free_test_display(&disp);
}

TEST(buffer_write_mask_clears_nonzero) {
    test_display_t disp = create_test_display(4, 4);
    /* Fill framebuffer with data */
    for (int i = 0; i < 16; i++)
        disp.fb_addr[i] = 0xFF;

    /* Mask: non-zero pixels in input → clear to 0 in framebuffer */
    uint32_t pixels[2] = {0xFFFFFFFF, 0x00000000};
    test_rect_t rect = {0, 0, 2, 1};

    test_readOrWriteBuffer(0, &disp, pixels, rect, false, true, true);

    ASSERT_EQ(disp.fb_addr[0], 0);    /* Non-zero pixel → cleared */
    ASSERT_EQ(disp.fb_addr[1], 0xFF); /* Zero pixel → unchanged */

    free_test_display(&disp);
}

TEST(buffer_read_mask_inverts) {
    test_display_t disp = create_test_display(4, 4);
    disp.fb_addr[0] = 0;        /* zero → reads as 1 */
    disp.fb_addr[1] = 0xFF;     /* non-zero → reads as 0 */

    uint32_t pixels[2] = {0};
    test_rect_t rect = {0, 0, 2, 1};

    test_readOrWriteBuffer(0, &disp, pixels, rect, false, true, false);

    ASSERT_EQ(pixels[0], 1);  /* FB zero → pixel 1 */
    ASSERT_EQ(pixels[1], 0);  /* FB non-zero → pixel 0 */

    free_test_display(&disp);
}

TEST(buffer_out_of_bounds_y_clipped) {
    test_display_t disp = create_test_display(4, 4);
    uint32_t pixels[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    /* rect starts at y=3, h=2 → only y=3 is valid, y=4 is clipped */
    test_rect_t rect = {0, 3, 4, 2};

    test_readOrWriteBuffer(0, &disp, pixels, rect, false, false, true);

    /* Row 3 should be written */
    ASSERT_EQ(disp.fb_addr[3 * 4 + 0], 0xAA);
    ASSERT_EQ(disp.fb_addr[3 * 4 + 1], 0xBB);
    ASSERT_EQ(disp.fb_addr[3 * 4 + 2], 0xCC);
    ASSERT_EQ(disp.fb_addr[3 * 4 + 3], 0xDD);

    free_test_display(&disp);
}

TEST(buffer_roundtrip_write_then_read) {
    test_display_t disp = create_test_display(8, 8);
    uint32_t write_pixels[6] = {1, 2, 3, 4, 5, 6};
    test_rect_t rect = {1, 1, 3, 2};

    /* Write */
    test_readOrWriteBuffer(0, &disp, write_pixels, rect, false, false, true);

    /* Read back */
    uint32_t read_pixels[6] = {0};
    test_readOrWriteBuffer(0, &disp, read_pixels, rect, false, false, false);

    for (int i = 0; i < 6; i++) {
        ASSERT_EQ(read_pixels[i], write_pixels[i]);
    }

    free_test_display(&disp);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== Display & Brightness Unit Tests ===\n\n");

    /* Brightness curve boundaries */
    RUN_TEST(brightness_level_0_raw);
    RUN_TEST(brightness_level_10_raw);
    RUN_TEST(brightness_level_5_raw);

    /* Brightness curve shape */
    RUN_TEST(brightness_curve_monotonic);
    RUN_TEST(brightness_curve_all_values);

    /* Brightness inverse */
    RUN_TEST(brightness_from_raw_zero);
    RUN_TEST(brightness_from_raw_negative);
    RUN_TEST(brightness_roundtrip_all_levels);

    /* Display buffer operations */
    RUN_TEST(buffer_write_simple);
    RUN_TEST(buffer_read_simple);
    RUN_TEST(buffer_write_with_offset);
    RUN_TEST(buffer_write_rotated);
    RUN_TEST(buffer_write_mask_clears_nonzero);
    RUN_TEST(buffer_read_mask_inverts);
    RUN_TEST(buffer_out_of_bounds_y_clipped);
    RUN_TEST(buffer_roundtrip_write_then_read);

    TEST_REPORT();
    return test_failures;
}
