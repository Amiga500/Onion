/**
 * @file test_display_rotate.c
 * @brief display_readOrWriteBuffer() from system/display.h (production
 *        header) against a pixel-by-pixel reference
 *
 * The rotated, non-masked path (every GameSwitcher frame) copies whole
 * reversed rows with neon_reverse_copy_u32(). It must give exactly the
 * pixels of the generic per-pixel loop, for writes and reads, any buffer
 * index, a framebuffer stride wider than xres, and rects that do or do
 * not fit horizontally (those keep the generic loop).
 *
 * Build and run: make -f Makefile.unit test_display_rotate
 */

#include "onion_test.h"

#include <stdlib.h>

#include "system/display.h"

/* Reference: the generic per-pixel loop of display_readOrWriteBuffer
 * (no fast paths), rotate/mask/write semantics unchanged. */
static void ref_readOrWriteBuffer(int index, display_t *display, uint32_t *pixels,
                                  rect_t rect, bool rotate, bool mask, bool write)
{
    int bufferPos = index * display->vinfo.yres;
    int stride = (int)(display->finfo.line_length / (int)sizeof(uint32_t));
    if (stride < (int)display->vinfo.xres)
        stride = (int)display->vinfo.xres;
    for (int oy = 0; oy < rect.h; oy++) {
        int y = rect.y + oy;
        if (y < 0 || y >= (int)display->vinfo.yres)
            continue;
        int virtualY = bufferPos + (rotate ? (int)(display->vinfo.yres - 1) - y : y);
        for (int ox = 0; ox < rect.w; ox++) {
            int x = rect.x + ox;
            if (rotate)
                x = (display->vinfo.xres - 1) - x;
            if (x < 0 || x >= (int)display->vinfo.xres)
                continue;
            long offset = (long)virtualY * stride + x;
            int i = oy * rect.w + ox;
            if (write) {
                if (mask) {
                    if (pixels[i] != 0)
                        display->fb_addr[offset] = 0;
                }
                else
                    display->fb_addr[offset] = pixels[i];
            }
            else {
                if (mask)
                    pixels[i] = display->fb_addr[offset] == 0 ? 1 : 0;
                else
                    pixels[i] = display->fb_addr[offset];
            }
        }
    }
}

static void setup(display_t *d, int xres, int yres, int pages, int stride_px)
{
    memset(d, 0, sizeof(*d));
    d->vinfo.xres = xres;
    d->vinfo.yres = yres;
    d->vinfo.yres_virtual = yres * pages;
    d->finfo.line_length = stride_px * 4;
    d->fb_addr = calloc((size_t)stride_px * yres * pages, 4);
}

static void fill_random(uint32_t *p, size_t n, unsigned *seed)
{
    for (size_t i = 0; i < n; i++)
        p[i] = (uint32_t)rand_r(seed) * 2654435761u;
}

/* Runs the production function and the reference on identical copies and
 * compares framebuffer and pixel buffer. Returns the number of mismatches. */
static int compare_case(int xres, int yres, int pages, int stride_px, rect_t r,
                        int index, bool write, unsigned seed)
{
    display_t a, b;
    setup(&a, xres, yres, pages, stride_px);
    setup(&b, xres, yres, pages, stride_px);
    size_t fb_n = (size_t)stride_px * yres * pages;
    size_t px_n = (size_t)r.w * r.h;
    uint32_t *pa = malloc(px_n * 4 + 4), *pb = malloc(px_n * 4 + 4);

    unsigned s1 = seed, s2 = seed + 7;
    fill_random(a.fb_addr, fb_n, &s1);
    memcpy(b.fb_addr, a.fb_addr, fb_n * 4);
    fill_random(pa, px_n, &s2);
    memcpy(pb, pa, px_n * 4);

    display_readOrWriteBuffer(index, &a, pa, r, true, false, write);
    ref_readOrWriteBuffer(index, &b, pb, r, true, false, write);

    int bad = memcmp(a.fb_addr, b.fb_addr, fb_n * 4) != 0;
    bad += memcmp(pa, pb, px_n * 4) != 0;
    free(a.fb_addr);
    free(b.fb_addr);
    free(pa);
    free(pb);
    return bad;
}

TEST(reverse_copy_matches_scalar_all_lengths)
{
    uint32_t src[64], dst[64], ref[64];
    for (int i = 0; i < 64; i++)
        src[i] = 0x01000000u * i + i;
    for (int n = 0; n <= 64; n++) {
        memset(dst, 0xAB, sizeof(dst));
        memset(ref, 0xAB, sizeof(ref));
        neon_reverse_copy_u32(dst, src, n);
        neon_reverse_copy_u32_scalar(ref, src, n);
        ASSERT_EQ(0, memcmp(dst, ref, sizeof(dst)));
    }
    neon_reverse_copy_u32(dst, src, 5);
    ASSERT_EQ(src[4], dst[0]);
    ASSERT_EQ(src[0], dst[4]);
}

TEST(rotated_full_frame_write_all_buffers)
{
    for (int index = 0; index < 3; index++) {
        ASSERT_EQ(0, compare_case(640, 480, 3, 640, (rect_t){0, 0, 640, 480}, index, true, 11 + index));
        ASSERT_EQ(0, compare_case(752, 560, 2, 752, (rect_t){0, 0, 752, 560}, index % 2, true, 21 + index));
    }
}

TEST(rotated_full_frame_read)
{
    ASSERT_EQ(0, compare_case(640, 480, 3, 640, (rect_t){0, 0, 640, 480}, 1, false, 5));
    ASSERT_EQ(0, compare_case(752, 560, 2, 752, (rect_t){0, 0, 752, 560}, 0, false, 6));
}

/* line_length wider than xres (padded rows) */
TEST(rotated_with_padded_stride)
{
    ASSERT_EQ(0, compare_case(640, 480, 2, 672, (rect_t){0, 0, 640, 480}, 1, true, 31));
    ASSERT_EQ(0, compare_case(640, 480, 2, 672, (rect_t){13, 7, 301, 99}, 0, false, 32));
}

/* Rects inside the screen use the fast path; rects crossing the left or
 * right edge, or rows off screen, keep the generic loop. */
TEST(rotated_partial_rects)
{
    unsigned seed = 1234;
    const rect_t rects[] = {
        {0, 0, 1, 1},
        {639, 479, 1, 1},
        {5, 3, 7, 9},
        {100, 50, 440, 60},
        {0, 420, 640, 60},
        {-10, 0, 50, 20},
        {600, 10, 80, 20},
        {-5, -5, 700, 500},
        {320, 470, 64, 30},
    };
    for (unsigned i = 0; i < sizeof(rects) / sizeof(rects[0]); i++) {
        ASSERT_EQ(0, compare_case(640, 480, 2, 640, rects[i], i % 2, true, seed + i));
        ASSERT_EQ(0, compare_case(640, 480, 2, 640, rects[i], i % 2, false, seed + 100 + i));
    }
}

int main(void)
{
    printf("\n=== display.h rotated buffer copy Unit Tests ===\n\n");
    RUN_TEST(reverse_copy_matches_scalar_all_lengths);
    RUN_TEST(rotated_full_frame_write_all_buffers);
    RUN_TEST(rotated_full_frame_read);
    RUN_TEST(rotated_with_padded_stride);
    RUN_TEST(rotated_partial_rects);
    TEST_REPORT();
    return test_failures;
}
