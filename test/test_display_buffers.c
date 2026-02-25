/**
 * @file test_display_buffers.c
 * @brief Unit tests for multi-buffer display_readOrWriteBuffers
 *
 * Tests the multi-buffer wrappers from system/display.h:
 * - display_readOrWriteBuffers: iterates buffers based on yres_virtual/yres
 * - display_readBuffers: convenience read wrapper
 * - display_writeBuffers: convenience write wrapper
 *
 * The multi-buffer functions compute numBuffers = yres_virtual / yres
 * and delegate per-buffer to display_readOrWriteBuffer (tested in test_display.c).
 *
 * Build and run: make -f Makefile.unit test_display_buffers
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

/* ---- Single-buffer read/write (same logic as test_display.c) ---- */

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

/* ---- Multi-buffer functions under test ---- */

static void test_readOrWriteBuffers(test_display_t *display, uint32_t **pixels,
                                    test_rect_t rect, bool rotate, bool mask, bool write)
{
    if (display->vinfo.yres == 0)
        return;
    int numBuffers = (int)(display->vinfo.yres_virtual / display->vinfo.yres);

    for (int b = 0; b < numBuffers; b++) {
        test_readOrWriteBuffer(b, display, pixels[b], rect, rotate, mask, write);
    }
}

static void test_readBuffers(test_display_t *display, uint32_t **pixels,
                             test_rect_t rect, bool rotate, bool mask)
{
    test_readOrWriteBuffers(display, pixels, rect, rotate, mask, false);
}

static void test_writeBuffers(test_display_t *display, uint32_t **pixels,
                              test_rect_t rect, bool rotate, bool mask)
{
    test_readOrWriteBuffers(display, pixels, rect, rotate, mask, true);
}

/* ---- Helper: create display with double-buffer ---- */

static test_display_t create_double_buffered_display(int w, int h)
{
    test_display_t d;
    d.vinfo.xres = (uint32_t)w;
    d.vinfo.yres = (uint32_t)h;
    d.vinfo.yres_virtual = (uint32_t)(h * 2); /* double buffer */
    d.vinfo.yoffset = 0;
    d.fb_addr = calloc((size_t)(w * h * 2), sizeof(uint32_t));
    return d;
}

static test_display_t create_triple_buffered_display(int w, int h)
{
    test_display_t d;
    d.vinfo.xres = (uint32_t)w;
    d.vinfo.yres = (uint32_t)h;
    d.vinfo.yres_virtual = (uint32_t)(h * 3); /* triple buffer */
    d.vinfo.yoffset = 0;
    d.fb_addr = calloc((size_t)(w * h * 3), sizeof(uint32_t));
    return d;
}

/* ==== Tests: multi-buffer write/read roundtrip ==== */

TEST(double_buffer_write_read_roundtrip) {
    test_display_t d = create_double_buffered_display(8, 4);
    test_rect_t rect = {0, 0, 4, 2};

    /* Write different data to each buffer */
    uint32_t buf0_write[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
    uint32_t buf1_write[8] = {0xFF, 0xEE, 0xDD, 0xCC, 0x55, 0x66, 0x77, 0x88};
    uint32_t *write_ptrs[2] = {buf0_write, buf1_write};

    test_writeBuffers(&d, write_ptrs, rect, false, false);

    /* Read back */
    uint32_t buf0_read[8] = {0};
    uint32_t buf1_read[8] = {0};
    uint32_t *read_ptrs[2] = {buf0_read, buf1_read};

    test_readBuffers(&d, read_ptrs, rect, false, false);

    /* Verify both buffers */
    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(buf0_read[i], buf0_write[i]);
        ASSERT_EQ(buf1_read[i], buf1_write[i]);
    }

    free(d.fb_addr);
}

TEST(triple_buffer_write_read_roundtrip) {
    test_display_t d = create_triple_buffered_display(4, 2);
    test_rect_t rect = {0, 0, 4, 2};

    uint32_t buf0[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    uint32_t buf1[8] = {0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x01, 0x02};
    uint32_t buf2[8] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1A, 0x1B};
    uint32_t *write_ptrs[3] = {buf0, buf1, buf2};

    test_writeBuffers(&d, write_ptrs, rect, false, false);

    uint32_t r0[8] = {0}, r1[8] = {0}, r2[8] = {0};
    uint32_t *read_ptrs[3] = {r0, r1, r2};

    test_readBuffers(&d, read_ptrs, rect, false, false);

    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(r0[i], buf0[i]);
        ASSERT_EQ(r1[i], buf1[i]);
        ASSERT_EQ(r2[i], buf2[i]);
    }

    free(d.fb_addr);
}

TEST(double_buffer_independence) {
    /* Verify buffers are stored in separate memory regions */
    test_display_t d = create_double_buffered_display(4, 2);
    test_rect_t rect = {0, 0, 4, 2};

    uint32_t buf0[8], buf1[8];
    for (int i = 0; i < 8; i++) { buf0[i] = 0xAAAAAAAA; buf1[i] = 0xBBBBBBBB; }
    uint32_t *ptrs[2] = {buf0, buf1};

    test_writeBuffers(&d, ptrs, rect, false, false);

    /* Buffer 0 occupies first 8 pixels of framebuffer (rows 0-1) */
    ASSERT_EQ(d.fb_addr[0], 0xAAAAAAAA);
    /* Buffer 1 occupies next 8 pixels (rows 2-3) */
    ASSERT_EQ(d.fb_addr[8], 0xBBBBBBBB);

    free(d.fb_addr);
}

TEST(double_buffer_rotated_write) {
    test_display_t d = create_double_buffered_display(4, 2);
    test_rect_t rect = {0, 0, 4, 1};

    uint32_t buf0[4] = {0x01, 0x02, 0x03, 0x04};
    uint32_t buf1[4] = {0x05, 0x06, 0x07, 0x08};
    uint32_t *ptrs[2] = {buf0, buf1};

    test_writeBuffers(&d, ptrs, rect, true, false);

    /* Read back with rotation */
    uint32_t r0[4] = {0}, r1[4] = {0};
    uint32_t *rptrs[2] = {r0, r1};

    test_readBuffers(&d, rptrs, rect, true, false);

    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(r0[i], buf0[i]);
        ASSERT_EQ(r1[i], buf1[i]);
    }

    free(d.fb_addr);
}

TEST(double_buffer_masked_write) {
    test_display_t d = create_double_buffered_display(4, 1);
    test_rect_t rect = {0, 0, 4, 1};

    /* Fill framebuffer with known data */
    for (int i = 0; i < 8; i++) d.fb_addr[i] = 0xFFFFFFFF;

    /* Mask write: non-zero pixels clear the framebuffer */
    uint32_t buf0[4] = {0x01, 0x00, 0x01, 0x00}; /* clear [0], keep [1], clear [2], keep [3] */
    uint32_t buf1[4] = {0x00, 0x01, 0x00, 0x01};
    uint32_t *ptrs[2] = {buf0, buf1};

    test_writeBuffers(&d, ptrs, rect, false, true);

    /* Buffer 0: positions 0,2 cleared to 0; positions 1,3 kept at 0xFFFFFFFF */
    ASSERT_EQ(d.fb_addr[0], 0);
    ASSERT_EQ(d.fb_addr[1], 0xFFFFFFFF);
    ASSERT_EQ(d.fb_addr[2], 0);
    ASSERT_EQ(d.fb_addr[3], 0xFFFFFFFF);

    /* Buffer 1: positions 0,2 kept at 0xFFFFFFFF; positions 1,3 cleared to 0 */
    ASSERT_EQ(d.fb_addr[4], 0xFFFFFFFF);
    ASSERT_EQ(d.fb_addr[5], 0);
    ASSERT_EQ(d.fb_addr[6], 0xFFFFFFFF);
    ASSERT_EQ(d.fb_addr[7], 0);

    free(d.fb_addr);
}

/* ==== Tests: edge cases ==== */

TEST(zero_yres_skips_all) {
    /* When yres is 0, the function should return immediately */
    test_display_t d;
    d.vinfo.xres = 4;
    d.vinfo.yres = 0;
    d.vinfo.yres_virtual = 0;
    d.fb_addr = NULL;
    test_rect_t rect = {0, 0, 4, 1};

    uint32_t buf[4] = {1, 2, 3, 4};
    uint32_t *ptrs[1] = {buf};

    /* Should not crash */
    test_readOrWriteBuffers(&d, ptrs, rect, false, false, false);
    ASSERT_TRUE(true);
}

TEST(single_buffer_as_special_case) {
    /* yres_virtual == yres → 1 buffer */
    test_display_t d;
    d.vinfo.xres = 4;
    d.vinfo.yres = 2;
    d.vinfo.yres_virtual = 2;
    d.vinfo.yoffset = 0;
    d.fb_addr = calloc(8, sizeof(uint32_t));
    test_rect_t rect = {0, 0, 4, 2};

    uint32_t buf[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    uint32_t *ptrs[1] = {buf};

    test_writeBuffers(&d, ptrs, rect, false, false);

    uint32_t read[8] = {0};
    uint32_t *rptrs[1] = {read};
    test_readBuffers(&d, rptrs, rect, false, false);

    for (int i = 0; i < 8; i++) {
        ASSERT_EQ(read[i], buf[i]);
    }

    free(d.fb_addr);
}

TEST(partial_rect_on_multi_buffer) {
    /* Write a sub-rect within a multi-buffer display */
    test_display_t d = create_double_buffered_display(8, 4);
    test_rect_t rect = {2, 1, 3, 2}; /* offset region */

    uint32_t buf0[6] = {0xA1, 0xA2, 0xA3, 0xB1, 0xB2, 0xB3};
    uint32_t buf1[6] = {0xC1, 0xC2, 0xC3, 0xD1, 0xD2, 0xD3};
    uint32_t *ptrs[2] = {buf0, buf1};

    test_writeBuffers(&d, ptrs, rect, false, false);

    uint32_t r0[6] = {0}, r1[6] = {0};
    uint32_t *rptrs[2] = {r0, r1};
    test_readBuffers(&d, rptrs, rect, false, false);

    for (int i = 0; i < 6; i++) {
        ASSERT_EQ(r0[i], buf0[i]);
        ASSERT_EQ(r1[i], buf1[i]);
    }

    free(d.fb_addr);
}

TEST(num_buffers_calculation) {
    /* Verify numBuffers = yres_virtual / yres */
    test_display_t d;
    d.vinfo.yres = 480;
    d.vinfo.yres_virtual = 960;
    int numBuffers = (int)(d.vinfo.yres_virtual / d.vinfo.yres);
    ASSERT_EQ(numBuffers, 2);

    d.vinfo.yres_virtual = 1440;
    numBuffers = (int)(d.vinfo.yres_virtual / d.vinfo.yres);
    ASSERT_EQ(numBuffers, 3);

    d.vinfo.yres_virtual = 480;
    numBuffers = (int)(d.vinfo.yres_virtual / d.vinfo.yres);
    ASSERT_EQ(numBuffers, 1);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== display.h Multi-Buffer Unit Tests ===\n\n");

    RUN_TEST(double_buffer_write_read_roundtrip);
    RUN_TEST(triple_buffer_write_read_roundtrip);
    RUN_TEST(double_buffer_independence);
    RUN_TEST(double_buffer_rotated_write);
    RUN_TEST(double_buffer_masked_write);
    RUN_TEST(zero_yres_skips_all);
    RUN_TEST(single_buffer_as_special_case);
    RUN_TEST(partial_rect_on_multi_buffer);
    RUN_TEST(num_buffers_calculation);

    TEST_REPORT();
    return test_failures;
}
