/**
 * @file test_osd_panel.c
 * @brief The volume/brightness panel (system/osd_panel.h): its size and
 * place on the screen, the rounded corners and the picture.
 *
 * Build and run: make -f Makefile.unit test_osd_panel
 */

#include "onion_test.h"
#include <stdlib.h>

#include "system/osd_panel.h"

#define W 240
#define H 40

static uint32_t px[W * H];

static void render(int kind, int value, int max, bool mute, uint32_t color)
{
    osd_panel_render(px, W, H, kind, value, max, mute, color);
}

static int count_color(uint32_t color)
{
    int n = 0;
    for (int i = 0; i < W * H; i++)
        if (px[i] == (OSD_PANEL_OPAQUE | color))
            n++;
    return n;
}

/* ==== Size and place ==== */

TEST(geometry_640x480) {
    osd_panel_rect_t r = osd_panel_geometry(640, 480, false);
    ASSERT_EQ(r.w, 240);
    ASSERT_EQ(r.h, 40);
    ASSERT_EQ(r.x, 200);
    ASSERT_EQ(r.y, 480 - 40 - 52);
}

TEST(geometry_top) {
    osd_panel_rect_t r = osd_panel_geometry(640, 480, true);
    ASSERT_EQ(r.x, 200);
    ASSERT_EQ(r.y, 52);
}

TEST(geometry_752x560_scaled_and_centred) {
    osd_panel_rect_t r = osd_panel_geometry(752, 560, false);
    ASSERT_EQ(r.w, 280);
    ASSERT_EQ(r.h, 47);
    ASSERT_EQ(r.x, (752 - 280) / 2);
    ASSERT_EQ(r.y, 560 - 47 - 61);
    r = osd_panel_geometry(752, 560, true);
    ASSERT_EQ(r.y, 61);
}

TEST(geometry_stays_on_a_tiny_screen) {
    for (int top = 0; top <= 1; top++) {
        osd_panel_rect_t r = osd_panel_geometry(100, 40, top);
        ASSERT_TRUE(r.w <= 100);
        ASSERT_TRUE(r.h <= 40);
        ASSERT_TRUE(r.x >= 0);
        ASSERT_TRUE(r.y >= 0);
        ASSERT_TRUE(r.y + r.h <= 40);
    }
}

/* ==== Rounded corners ==== */

TEST(row_inset_round_ends) {
    ASSERT_TRUE(osd_panel_row_inset(W, H, 0) > 5);
    ASSERT_EQ(osd_panel_row_inset(W, H, H / 2), 0);
    for (int y = 0; y < H; y++)
        ASSERT_EQ(osd_panel_row_inset(W, H, y), osd_panel_row_inset(W, H, H - 1 - y));
}

TEST(corners_are_clear_middle_is_panel) {
    render(OSD_PANEL_VOLUME, 10, 20, false, 0x001CD577);
    ASSERT_EQ(px[0], 0u);
    ASSERT_EQ(px[W - 1], 0u);
    ASSERT_EQ(px[(H - 1) * W], 0u);
    ASSERT_TRUE(px[(H / 2) * W + W / 2] & OSD_PANEL_OPAQUE);
    /* Every pixel of a row's span is panel, none outside it. */
    for (int y = 0; y < H; y++) {
        int inset = osd_panel_row_inset(W, H, y);
        for (int x = 0; x < W; x++) {
            bool in = x >= inset && x < W - inset;
            ASSERT_EQ(in, (px[y * W + x] & OSD_PANEL_OPAQUE) != 0);
        }
    }
}

/* ==== The bar ==== */

TEST(empty_bar_has_no_fill) {
    render(OSD_PANEL_VOLUME, 0, 20, false, 0x001CD577);
    ASSERT_EQ(count_color(0x001CD577), 0);
    ASSERT_TRUE(count_color(OSD_PANEL_TRACK) > 0);
}

TEST(fill_grows_with_the_value) {
    render(OSD_PANEL_BRIGHTNESS, 3, 10, false, 0x00FF0000);
    int low = count_color(0x00FF0000);
    render(OSD_PANEL_BRIGHTNESS, 8, 10, false, 0x00FF0000);
    int high = count_color(0x00FF0000);
    ASSERT_TRUE(low > 0);
    ASSERT_TRUE(high > low * 2);
}

TEST(full_bar_has_no_track) {
    render(OSD_PANEL_VOLUME, 20, 20, false, 0x001CD577);
    ASSERT_EQ(count_color(OSD_PANEL_TRACK), 0);
}

TEST(value_out_of_range_is_clamped) {
    render(OSD_PANEL_VOLUME, 20, 20, false, 0x001CD577);
    int full = count_color(0x001CD577);
    render(OSD_PANEL_VOLUME, 99, 20, false, 0x001CD577);
    ASSERT_EQ(count_color(0x001CD577), full);
    render(OSD_PANEL_VOLUME, -5, 20, false, 0x001CD577);
    ASSERT_EQ(count_color(0x001CD577), 0);
}

/* ==== The icon ==== */

TEST(icon_is_white_and_differs_by_kind) {
    render(OSD_PANEL_BRIGHTNESS, 5, 10, false, 0x001CD577);
    int sun = count_color(OSD_PANEL_ICON);
    static uint32_t sun_px[W * H];
    for (int i = 0; i < W * H; i++)
        sun_px[i] = px[i];
    render(OSD_PANEL_VOLUME, 5, 10, false, 0x001CD577);
    int speaker = count_color(OSD_PANEL_ICON);
    ASSERT_TRUE(sun > 20);
    ASSERT_TRUE(speaker > 20);
    int differ = 0;
    for (int x = 0; x < 40; x++)
        for (int y = 0; y < H; y++)
            differ += sun_px[y * W + x] != px[y * W + x];
    ASSERT_TRUE(differ > 30);
}

TEST(more_volume_more_waves) {
    render(OSD_PANEL_VOLUME, 0, 20, false, 0x001CD577);
    int none = count_color(OSD_PANEL_ICON);
    render(OSD_PANEL_VOLUME, 5, 20, false, 0x001CD577);
    int one = count_color(OSD_PANEL_ICON);
    render(OSD_PANEL_VOLUME, 18, 20, false, 0x001CD577);
    int two = count_color(OSD_PANEL_ICON);
    ASSERT_TRUE(one > none);
    ASSERT_TRUE(two > one);
}

TEST(mute_draws_the_icon_in_the_bar_color) {
    render(OSD_PANEL_VOLUME, 10, 20, true, 0x00F80355);
    ASSERT_EQ(count_color(OSD_PANEL_ICON), 0);
    int red_icon = 0;
    for (int x = 0; x < 40; x++)
        for (int y = 0; y < H; y++)
            red_icon += px[y * W + x] == (OSD_PANEL_OPAQUE | 0x00F80355);
    ASSERT_TRUE(red_icon > 20);
}

TEST(null_or_empty_buffer_is_ignored) {
    osd_panel_render(NULL, W, H, OSD_PANEL_VOLUME, 1, 2, false, 0);
    osd_panel_render(px, 0, H, OSD_PANEL_VOLUME, 1, 2, false, 0);
    osd_panel_render(px, W, H, OSD_PANEL_VOLUME, 1, 0, false, 0x00FFFFFF);
    ASSERT_TRUE(1);
}

/* ==== The shortcut ==== */

/* Away from the icon and the bar's edges the color is set without
 * sampling: it must be exactly what sampling gives, at every size. */
TEST(render_equals_sampling_every_pixel) {
    static uint32_t fast[300 * 48];
    const int sizes[][2] = {{200, 32}, {233, 37}, {160, 26}, {300, 48}};
    const int values[] = {0, 1, 7, 10, 13, 19, 20};
    for (unsigned si = 0; si < sizeof(sizes) / sizeof(sizes[0]); si++) {
        int w = sizes[si][0], h = sizes[si][1];
        for (int kind = 0; kind <= 1; kind++) {
            for (unsigned vi = 0; vi < sizeof(values) / sizeof(values[0]); vi++) {
                for (int mute = 0; mute <= 1; mute++) {
                    osd_panel_render(fast, w, h, kind, values[vi], 20, mute, 0x001CD577);
                    osd_panel_layout_t l = osd_panel_layout(w, h, kind, values[vi], 20, mute, 0x001CD577);
                    for (int y = 0; y < h; y++) {
                        int inset = osd_panel_row_inset(w, h, y);
                        for (int x = inset; x < w - inset; x++)
                            ASSERT_EQ(fast[y * w + x], osd_panel_sample(&l, x, y));
                    }
                }
            }
        }
    }
}

int main(void)
{
    RUN_TEST(geometry_640x480);
    RUN_TEST(geometry_top);
    RUN_TEST(geometry_752x560_scaled_and_centred);
    RUN_TEST(geometry_stays_on_a_tiny_screen);
    RUN_TEST(row_inset_round_ends);
    RUN_TEST(corners_are_clear_middle_is_panel);
    RUN_TEST(empty_bar_has_no_fill);
    RUN_TEST(fill_grows_with_the_value);
    RUN_TEST(full_bar_has_no_track);
    RUN_TEST(value_out_of_range_is_clamped);
    RUN_TEST(icon_is_white_and_differs_by_kind);
    RUN_TEST(more_volume_more_waves);
    RUN_TEST(mute_draws_the_icon_in_the_bar_color);
    RUN_TEST(null_or_empty_buffer_is_ignored);
    RUN_TEST(render_equals_sampling_every_pixel);

    TEST_REPORT();
    return test_failures;
}
