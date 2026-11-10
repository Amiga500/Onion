/**
 * @file test_osd_panel_fb.c
 * @brief The volume/brightness panel on a fake Miyoo framebuffer: drawn
 * upside down (the screen is turned 180 degrees) at the bottom centre of
 * every buffer, only over its rounded shape, and the screen behind it put
 * back when it goes, unless the program covers it by itself.
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

    osd_panel_rect_t r = osd_panel_geometry(SW, SH, false);
    static uint32_t want[240 * 40];
    ASSERT_EQ(r.w * r.h, 240 * 40);
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

/* A game (or a menu with moving titles) draws over the panel all the time:
 * that is noticed with a few pixels, without reading the whole area. When
 * the panel goes, the buffer the game keeps drawing is left to it, the
 * others get their picture back. */
TEST(redrawn_buffer_left_to_the_program) {
    fb_setup();
    _osd_panel = true;
    _osd_panel_top = false;
    _bar_kind = OSD_PANEL_BRIGHTNESS;
    _bar_value = 4;
    _bar_max = 10;
    _bar_color = OSD_BRIGHTNESS_COLOR;
    _bar_mute = false;
    _bar_saveBufferBehind();
    _print_bar();
    osd_panel_rect_t r = osd_panel_geometry(SW, SH, false);

    for (int i = 0; i < SW * SH; i++)
        fb[SW * SH + i] = 0x00111111; /* a frame of the game on buffer 1 */
    _print_bar();
    ASSERT_TRUE(_panel_redrawn[1]);
    ASSERT_TRUE(!_panel_redrawn[0]);
    ASSERT_TRUE(seen(1, r.x + r.w / 2, r.y + r.h / 2) != 0x00111111u); /* panel on top again */

    /* The panel's time is up; the next frame comes. */
    _bar_timer = getMilliseconds() - 5000;
    for (int i = 0; i < SW * SH; i++)
        fb[SW * SH + i] = 0x00222222;
    ASSERT_TRUE(_panel_settle());
    _bar_restoreBufferBehind();
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x; x < r.x + r.w; x++) {
            ASSERT_EQ(seen(1, x, y), 0x00222222u);
            ASSERT_EQ(seen(0, x, y), (uint32_t)((size_t)(SH - 1 - y) * SW + (SW - 1 - x)) * 2654435761u & 0x00FFFFFF);
        }
}

/* Drawn over once, then still: after the wait the saved picture goes back
 * (the best there is: the panel is not left on screen). */
TEST(redrawn_once_then_still_gets_the_saved_picture) {
    fb_setup();
    _osd_panel = true;
    _bar_kind = OSD_PANEL_VOLUME;
    _bar_value = 7;
    _bar_max = 20;
    _bar_color = OSD_VOLUME_COLOR;
    _bar_saveBufferBehind();
    _print_bar();
    for (int i = 0; i < SW * SH; i++)
        fb[i] = 0x00333333;
    _print_bar();
    ASSERT_TRUE(_panel_redrawn[0]);
    _bar_timer = getMilliseconds() - 5000;
    long t0 = getMilliseconds();
    ASSERT_TRUE(_panel_settle());
    ASSERT_TRUE(getMilliseconds() - t0 >= OSD_PANEL_SETTLE_MS - 10);
    _bar_restoreBufferBehind();
    osd_panel_rect_t r = osd_panel_geometry(SW, SH, false);
    int x = r.x + r.w / 2, y = r.y + r.h / 2;
    ASSERT_EQ(seen(0, x, y), (uint32_t)((size_t)(SH - 1 - y) * SW + (SW - 1 - x)) * 2654435761u & 0x00FFFFFF);
}

/* Nothing drew over it: no wait. A key during the wait brings it back. */
TEST(settle_no_wait_when_still_and_stops_for_a_key) {
    fb_setup();
    _osd_panel = true;
    _bar_kind = OSD_PANEL_VOLUME;
    _bar_value = 3;
    _bar_max = 20;
    _bar_color = OSD_VOLUME_COLOR;
    _bar_saveBufferBehind();
    _print_bar();
    _print_bar();
    _bar_timer = getMilliseconds() - 5000;
    long t0 = getMilliseconds();
    ASSERT_TRUE(_panel_settle());
    ASSERT_TRUE(getMilliseconds() - t0 < 5);

    for (int i = 0; i < SW * SH; i++)
        fb[i] = 0x00444444;
    _print_bar();
    _bar_timer = getMilliseconds(); /* a key: the panel is wanted again */
    ASSERT_TRUE(!_panel_settle());
    _bar_restoreBufferBehind();
}

/* The value changes: drawing the new picture is not the program drawing. */
TEST(new_value_is_not_a_redraw) {
    fb_setup();
    _osd_panel = true;
    _bar_kind = OSD_PANEL_VOLUME;
    _bar_value = 3;
    _bar_max = 20;
    _bar_color = OSD_VOLUME_COLOR;
    _bar_saveBufferBehind();
    _print_bar();
    _bar_value = 15;
    _print_bar();
    _print_bar();
    for (int b = 0; b < NBUF; b++)
        ASSERT_TRUE(!_panel_redrawn[b]);
    _bar_restoreBufferBehind();
}

/* At the top, on request. */
TEST(panel_at_the_top) {
    fb_setup();
    _osd_panel = true;
    _osd_panel_top = true;
    _bar_kind = OSD_PANEL_BRIGHTNESS;
    _bar_value = 5;
    _bar_max = 10;
    _bar_color = OSD_BRIGHTNESS_COLOR;
    _bar_saveBufferBehind();
    _print_bar();
    osd_panel_rect_t r = osd_panel_geometry(SW, SH, true);
    static uint32_t want[240 * 40];
    osd_panel_render(want, r.w, r.h, OSD_PANEL_BRIGHTNESS, 5, 10, false, OSD_BRIGHTNESS_COLOR);
    ASSERT_EQ(seen(0, r.x + r.w / 2, r.y + r.h / 2), want[(r.h / 2) * r.w + r.w / 2]);
    _bar_restoreBufferBehind();
    _osd_panel_top = false;
}

/* A key came during the wait, after a buffer had been covered: the panel
 * is drawn there again, so if a key then hides it, that buffer gets its
 * picture back (it is not left with the panel on it). */
TEST(drawn_again_after_the_wait_is_put_back) {
    fb_setup();
    _osd_panel = true;
    _bar_kind = OSD_PANEL_VOLUME;
    _bar_value = 9;
    _bar_max = 20;
    _bar_color = OSD_VOLUME_COLOR;
    _bar_saveBufferBehind();
    _print_bar();
    /* The wait saw the game cover buffer 1, then a key cut it short. */
    for (int i = 0; i < SW * SH; i++)
        fb[SW * SH + i] = 0x00555555;
    _panel_covered[1] = true;
    _print_bar(); /* the panel is drawn there again */
    _bar_restoreBufferBehind();
    osd_panel_rect_t r = osd_panel_geometry(SW, SH, false);
    int x = r.x + r.w / 2, y = r.y + r.h / 2;
    /* Not left with the panel on it: the saved picture is back. */
    ASSERT_EQ(seen(1, x, y),
              (uint32_t)((size_t)SW * SH + (size_t)(SH - 1 - y) * SW + (SW - 1 - x)) * 2654435761u & 0x00FFFFFF);
}

/* A pass draws the panel on a buffer only when it isn't there: not yet,
 * or the program drew over it (a frame); a stray pixel outside the nine
 * checked is not a frame. */
TEST(other_buffers_drawn_only_when_needed) {
    fb_setup();
    _osd_panel = true;
    _osd_panel_top = false;
    _bar_kind = OSD_PANEL_VOLUME;
    _bar_value = 12;
    _bar_max = 20;
    _bar_color = OSD_VOLUME_COLOR;
    _bar_saveBufferBehind();
    _print_bar(); /* first pass: on every buffer */
    osd_panel_rect_t r = osd_panel_geometry(SW, SH, false);
    int x = r.x + r.w / 2, y = r.y + r.h / 2;
    for (int b = 0; b < NBUF; b++)
        ASSERT_TRUE(seen(b, x, y) & 0x00FFFFFF);

    /* One pixel of buffer 2 outside the nine checked: not redrawn. */
    size_t i2 = (size_t)2 * SW * SH + (size_t)(SH - 1 - (r.y + 1)) * SW + (SW - 1 - (r.x + r.w / 2 + 1));
    uint32_t panel_px = fb[i2];
    fb[i2] = 0x00777777;
    _print_bar();
    ASSERT_EQ(fb[i2], 0x00777777u);
    ASSERT_TRUE(!_panel_redrawn[2]);

    /* The game draws a frame on buffer 2: noticed, the panel back on it. */
    for (int i = 0; i < SW * SH; i++)
        fb[2 * SW * SH + i] = 0x00666666;
    _print_bar();
    ASSERT_TRUE(_panel_redrawn[2]);
    ASSERT_EQ(fb[i2], panel_px);

    _bar_restoreBufferBehind();
}

/* The wait between passes: about 100 us when spinning (used when usleep
 * turns out to sleep a whole timer tick), never a tick. */
TEST(spin_wait_is_about_100_us) {
    _panel_coarse_sleep = true;
    long t0 = _osd_now_us();
    for (int i = 0; i < 50; i++)
        _panel_wait();
    long each = (_osd_now_us() - t0) / 50;
    _panel_coarse_sleep = false;
    ASSERT_TRUE(each >= 100);
    ASSERT_TRUE(each < 1000);
}

/* osd_hideBar() while the panel is shown (POWER, MENU, standby): the thread
 * ends at once, even in the spin wait (no cancellation point there: with
 * pthread_cancel the join waited for the rest of the 2 s), and the screen
 * behind the panel is back on every buffer. */
TEST(hide_while_shown_ends_at_once_and_puts_back) {
    fb_setup();
    static uint32_t before[SW * SH * NBUF];
    for (int i = 0; i < SW * SH * NBUF; i++)
        before[i] = fb[i];

    _osd_panel = true;
    _osd_panel_top = false;
    _bar_kind = OSD_PANEL_BRIGHTNESS;
    _bar_value = 5;
    _bar_max = 10;
    _bar_color = OSD_VOLUME_COLOR;
    _bar_mute = false;
    _panel_coarse_sleep = true; /* the spin wait */
    _bar_timer = getMilliseconds();
    _bar_saveBufferBehind();
    ASSERT_TRUE(pthread_create(&osd_pt, NULL, _osd_thread, NULL) == 0);
    osd_thread_joinable = true;
    osd_thread_active = true;
    usleep(50000);

    osd_panel_rect_t r = osd_panel_geometry(SW, SH, false);
    int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
    bool shown = false;
    for (int b = 0; b < NBUF; b++)
        shown = shown || seen(b, cx, cy) != before[(size_t)b * SW * SH + (size_t)(SH - 1 - cy) * SW + (SW - 1 - cx)];
    ASSERT_TRUE(shown);

    long t0 = _osd_now_us();
    osd_hideBar();
    long took = _osd_now_us() - t0;
    _panel_coarse_sleep = false;
    ASSERT_TRUE(took < 100000); /* not the rest of the 2 s */
    ASSERT_TRUE(!osd_thread_active);
    ASSERT_TRUE(!osd_stop);

    bool same = true;
    for (int i = 0; i < SW * SH * NBUF && same; i++)
        same = fb[i] == before[i];
    ASSERT_TRUE(same);
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
    osd_panel_rect_t r = osd_panel_geometry(SW, SH, false);
    static uint32_t want[240 * 40];
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
    osd_panel_rect_t r = osd_panel_geometry(SW, SH, false);
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
    RUN_TEST(redrawn_buffer_left_to_the_program);
    RUN_TEST(redrawn_once_then_still_gets_the_saved_picture);
    RUN_TEST(settle_no_wait_when_still_and_stops_for_a_key);
    RUN_TEST(new_value_is_not_a_redraw);
    RUN_TEST(panel_at_the_top);
    RUN_TEST(drawn_again_after_the_wait_is_put_back);
    RUN_TEST(other_buffers_drawn_only_when_needed);
    RUN_TEST(spin_wait_is_about_100_us);
    RUN_TEST(hide_while_shown_ends_at_once_and_puts_back);
    RUN_TEST(padded_rows_follow_the_line_length);
    RUN_TEST(mode_change_while_shown_stays_in_the_mapping);
    RUN_TEST(one_buffer_mapping);
    RUN_TEST(edge_bar_unchanged_when_chosen);
    TEST_REPORT();
    return test_failures;
}
