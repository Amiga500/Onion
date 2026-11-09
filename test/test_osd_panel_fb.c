/**
 * @file test_osd_panel_fb.c
 * @brief The volume/brightness panel on a fake Miyoo framebuffer: drawn
 * upside down (the screen is turned 180 degrees) at the bottom centre of
 * every buffer, only over its rounded shape, and the screen behind it put
 * back when it goes, as the program last drew it.
 *
 * Built with PLATFORM_MIYOOMINI, on the host.
 * Build and run: make -f Makefile.unit test_osd_panel_fb
 */

#include "onion_test.h"
#include <stdlib.h>

#include "system/osd.h"

void SDL_FreeSurface(SDL_Surface *surface) { (void)surface; }

#define SW 640
#define SH 480
#define NBUF 3

#define GUARD 4096
#define GUARD_PIXEL 0x5A5A5A5Au

/* Room for a padded pitch too, and a guard after the mapping. */
static uint32_t fb[(SW + 16) * SH * NBUF + GUARD];

static void fb_setup_pitch(int stride)
{
    for (int i = 0; i < stride * SH * NBUF; i++)
        fb[i] = (uint32_t)(i * 2654435761u) & 0x00FFFFFF; /* a "busy" screen */
    for (int i = stride * SH * NBUF; i < (int)(sizeof(fb) / sizeof(fb[0])); i++)
        fb[i] = GUARD_PIXEL;
    g_display.fb_addr = fb;
    g_display.width = SW;
    g_display.height = SH;
    g_display.finfo.line_length = stride * sizeof(uint32_t);
    g_display.fb_size = (long)stride * SH * NBUF * (long)sizeof(uint32_t);
}

static void fb_setup(void) { fb_setup_pitch(SW); }

static bool guard_intact(int stride)
{
    for (int i = stride * SH * NBUF; i < (int)(sizeof(fb) / sizeof(fb[0])); i++)
        if (fb[i] != GUARD_PIXEL)
            return false;
    return true;
}

/* The pixel seen at screen (x, y) of buffer b. */
static uint32_t seen(int b, int x, int y)
{
    return fb[(size_t)b * SW * SH + (size_t)(SH - 1 - y) * SW + (SW - 1 - x)];
}

TEST(panel_drawn_upside_down_on_every_buffer_then_restored) {
    fb_setup();
    static uint32_t before[SW * SH * NBUF];
    for (int i = 0; i < SW * SH * NBUF; i++)
        before[i] = fb[i];

    _osd_panel = true;
    _bar_kind = OSD_PANEL_VOLUME;
    _bar_value = 10;
    _bar_max = 20;
    _bar_color = OSD_VOLUME_COLOR;
    _bar_mute = false;
    _bar_saveBufferBehind();
    _print_bar();

    osd_panel_rect_t r = osd_panel_geometry(SW, SH);
    static uint32_t want[200 * 32];
    ASSERT_EQ(r.w * r.h, 200 * 32);
    osd_panel_render(want, r.w, r.h, OSD_PANEL_VOLUME, 10, 20, false, OSD_VOLUME_COLOR);

    for (int b = 0; b < NBUF; b++) {
        for (int y = 0; y < SH; y++) {
            for (int x = 0; x < SW; x++) {
                bool in = x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
                uint32_t p = in ? want[(y - r.y) * r.w + (x - r.x)] : 0;
                size_t i = (size_t)b * SW * SH + (size_t)(SH - 1 - y) * SW + (SW - 1 - x);
                if (p & OSD_PANEL_OPAQUE)
                    ASSERT_EQ(seen(b, x, y), p);
                else
                    ASSERT_EQ(fb[i], before[i]);
            }
        }
    }

    /* The volume goes up while it shows: drawn again. */
    _bar_value = 20;
    _print_bar();
    osd_panel_render(want, r.w, r.h, OSD_PANEL_VOLUME, 20, 20, false, OSD_VOLUME_COLOR);
    int mid = r.y + r.h / 2;
    ASSERT_EQ(seen(1, r.x + r.w - 20, mid), want[(r.h / 2) * r.w + r.w - 20]);

    /* Gone: each buffer gets its own picture back. */
    _bar_restoreBufferBehind();
    for (int b = 0; b < NBUF; b++)
        for (int y = r.y; y < r.y + r.h; y++)
            for (int x = r.x; x < r.x + r.w; x++)
                ASSERT_EQ(seen(b, x, y),
                          before[(size_t)b * SW * SH + (size_t)(SH - 1 - y) * SW + (SW - 1 - x)]);
    ASSERT_TRUE(_panel_px == NULL);
}

/* A menu redraws while the panel shows: what it drew is put back when the
 * panel goes, not the old menu. */
TEST(menu_redrawn_under_the_panel_is_what_comes_back) {
    fb_setup();
    _osd_panel = true;
    _bar_kind = OSD_PANEL_BRIGHTNESS;
    _bar_value = 4;
    _bar_max = 10;
    _bar_color = OSD_BRIGHTNESS_COLOR;
    _bar_mute = false;
    _bar_saveBufferBehind();
    _print_bar();

    /* The menu draws a new screen on buffer 1, over the panel. */
    for (int i = 0; i < SW * SH; i++)
        fb[SW * SH + i] = 0x00123456;
    _print_bar(); /* the panel comes back on top */
    osd_panel_rect_t r = osd_panel_geometry(SW, SH);
    ASSERT_TRUE(seen(1, r.x + r.w / 2, r.y + r.h / 2) != 0x00123456u);

    /* The value changes: drawing the new picture is not the menu drawing. */
    _bar_value = 9;
    _print_bar();
    _print_bar();

    _bar_restoreBufferBehind();
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            ASSERT_EQ(seen(1, x, y), 0x00123456u);
    /* Buffer 0 wasn't redrawn: its own picture is back. */
    ASSERT_EQ(seen(0, r.x + r.w / 2, r.y + r.h / 2),
              (uint32_t)((size_t)(SH - 1 - (r.y + r.h / 2)) * SW + (SW - 1 - (r.x + r.w / 2))) * 2654435761u &
                  0x00FFFFFF);
}

/* Half way through a redraw: the rows the menu hasn't reached yet keep the
 * old back, they don't take the panel as the menu. */
TEST(half_drawn_menu_keeps_no_piece_of_the_panel) {
    fb_setup();
    _osd_panel = true;
    _bar_kind = OSD_PANEL_VOLUME;
    _bar_value = 7;
    _bar_max = 20;
    _bar_color = OSD_VOLUME_COLOR;
    _bar_saveBufferBehind();
    _print_bar();
    osd_panel_rect_t r = osd_panel_geometry(SW, SH);
    /* The menu redrew the top half of the screen (screen rows 0..SH/2):
     * in the framebuffer that is the bottom half, which has no panel. Then
     * the bottom rows of the panel area only. */
    for (int y = r.y + r.h / 2; y < r.y + r.h; y++)
        for (int x = 0; x < SW; x++)
            fb[(size_t)(SH - 1 - y) * SW + (SW - 1 - x)] = 0x00ABCDEF;
    _bar_restoreBufferBehind();
    for (int x = r.x; x < r.x + r.w; x++) {
        ASSERT_EQ(seen(0, x, r.y + r.h - 1), 0x00ABCDEFu);
        uint32_t old = (uint32_t)((size_t)(SH - 1 - r.y) * SW + (SW - 1 - x)) * 2654435761u & 0x00FFFFFF;
        ASSERT_EQ(seen(0, x, r.y), old);
    }
}

/* Rows further apart than the width (finfo.line_length): the panel follows
 * the pitch, as display_readOrWriteBuffer() does. */
TEST(padded_rows_follow_the_line_length) {
    const int stride = SW + 16;
    fb_setup_pitch(stride);
    _osd_panel = true;
    _bar_kind = OSD_PANEL_VOLUME;
    _bar_value = 10;
    _bar_max = 20;
    _bar_color = OSD_VOLUME_COLOR;
    _bar_mute = false;
    _bar_saveBufferBehind();
    _print_bar();
    osd_panel_rect_t r = osd_panel_geometry(SW, SH);
    static uint32_t want[200 * 32];
    osd_panel_render(want, r.w, r.h, OSD_PANEL_VOLUME, 10, 20, false, OSD_VOLUME_COLOR);
    for (int b = 0; b < NBUF; b++) {
        int x = r.x + r.w / 2, y = r.y + r.h / 2;
        ASSERT_EQ(fb[((size_t)b * SH + (SH - 1 - y)) * stride + (SW - 1 - x)], want[(r.h / 2) * r.w + r.w / 2]);
    }
    _bar_restoreBufferBehind();
    ASSERT_TRUE(guard_intact(stride));
}

/* The mode changes while the panel shows (keymon is told with SIGUSR1):
 * it stays on the screen it was placed on and never writes past the
 * mapping. */
TEST(mode_change_while_shown_stays_in_the_mapping) {
    fb_setup();
    _osd_panel = true;
    _bar_kind = OSD_PANEL_BRIGHTNESS;
    _bar_value = 5;
    _bar_max = 10;
    _bar_color = OSD_BRIGHTNESS_COLOR;
    _bar_saveBufferBehind();
    _print_bar();
    g_display.width = 752;
    g_display.height = 560;
    _bar_value = 6;
    _print_bar();
    _bar_restoreBufferBehind();
    ASSERT_TRUE(guard_intact(SW));
}

/* A mapping with room for one buffer only: drawn there, nowhere else. */
TEST(one_buffer_mapping) {
    fb_setup();
    g_display.fb_size = (long)SW * SH * (long)sizeof(uint32_t);
    _osd_panel = true;
    _bar_kind = OSD_PANEL_VOLUME;
    _bar_value = 3;
    _bar_max = 20;
    _bar_color = OSD_VOLUME_COLOR;
    uint32_t second = fb[SW * SH + 5];
    _bar_saveBufferBehind();
    _print_bar();
    osd_panel_rect_t r = osd_panel_geometry(SW, SH);
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++)
            ASSERT_EQ(fb[(size_t)SW * SH + (size_t)(SH - 1 - y) * SW + (SW - 1 - x)],
                      (uint32_t)(((size_t)SW * SH + (size_t)(SH - 1 - y) * SW + (SW - 1 - x)) * 2654435761u) &
                          0x00FFFFFF);
    _bar_restoreBufferBehind();
    ASSERT_EQ(fb[SW * SH + 5], second);
}

TEST(edge_bar_unchanged_when_chosen) {
    fb_setup();
    _osd_panel = false;
    meterWidth = 4;
    _bar_value = 5;
    _bar_max = 10;
    _bar_color = OSD_BRIGHTNESS_COLOR;
    _bar_saveBufferBehind();
    _print_bar();
    /* Onion's bar: the left edge, from the bottom up to half the height. */
    ASSERT_EQ(seen(0, 0, SH - 1), OSD_BRIGHTNESS_COLOR);
    ASSERT_EQ(seen(0, 3, SH - 10), OSD_BRIGHTNESS_COLOR);
    ASSERT_EQ(seen(0, 0, 10), 0u);
    _bar_restoreBufferBehind();
}

int main(void)
{
    RUN_TEST(panel_drawn_upside_down_on_every_buffer_then_restored);
    RUN_TEST(menu_redrawn_under_the_panel_is_what_comes_back);
    RUN_TEST(half_drawn_menu_keeps_no_piece_of_the_panel);
    RUN_TEST(padded_rows_follow_the_line_length);
    RUN_TEST(mode_change_while_shown_stays_in_the_mapping);
    RUN_TEST(one_buffer_mapping);
    RUN_TEST(edge_bar_unchanged_when_chosen);
    TEST_REPORT();
    return test_failures;
}
