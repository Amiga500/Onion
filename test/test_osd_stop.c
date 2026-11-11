/**
 * @file test_osd_stop.c
 * @brief osd_hideBar() ends the indicator's thread at once
 *
 * The thread used to be ended with pthread_cancel(), which only acts at a
 * cancellation point. The panel's wait loop has none when usleep() is coarse
 * (it spins on sched_yield()), so the join in osd_hideBar() blocked keymon
 * until the indicator's 2 s were over: POWER and MENU pressed meanwhile were
 * late or lost. The thread is now asked to stop with a flag.
 *
 * Built with PLATFORM_MIYOOMINI, on the host, on a fake framebuffer.
 * Build and run: make -f Makefile.unit test_osd_stop
 */

#include "onion_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TEST_DIR "/tmp/onion_test_osd_stop"
#define CONFIG_PATH TEST_DIR "/config/"

#include "system/osd.h"

void SDL_FreeSurface(SDL_Surface *surface) { (void)surface; }

#define SW 640
#define SH 480
#define NBUF 3
#define FB_PIXELS (SW * SH * NBUF)
#define GUARD 4096
#define GUARD_PIXEL 0x5A5A5A5Au
#define HIDE_LIMIT_US 500000L /* far above the real time, far below 2 s */

static uint32_t fb[FB_PIXELS + GUARD];
static uint32_t before[FB_PIXELS];

static long now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000L + t.tv_nsec / 1000;
}

static void setup(bool edge_bar)
{
    osd_hideBar(); /* a thread left by a test that failed */
    if (system("rm -rf " TEST_DIR " && mkdir -p " TEST_DIR "/config") != 0)
        fprintf(stderr, "setup failed\n");
    if (edge_bar) {
        FILE *f = fopen(CONFIG_PATH OSD_EDGE_BAR_FLAG, "w");
        if (f != NULL)
            fclose(f);
    }
    for (int i = 0; i < FB_PIXELS; i++)
        fb[i] = before[i] = (uint32_t)(i * 2654435761u) & 0x00FFFFFF;
    for (int i = FB_PIXELS; i < FB_PIXELS + GUARD; i++)
        fb[i] = GUARD_PIXEL;
    g_display.fb_addr = fb;
    g_display.width = SW;
    g_display.height = SH;
    g_display.finfo.line_length = SW * sizeof(uint32_t);
    g_display.fb_size = (long)FB_PIXELS * (long)sizeof(uint32_t);
}

static bool guard_intact(void)
{
    for (int i = FB_PIXELS; i < FB_PIXELS + GUARD; i++)
        if (fb[i] != GUARD_PIXEL)
            return false;
    return true;
}

/* The pixel seen at screen (x, y) of buffer b (the screen is upside down). */
static uint32_t seen(int b, int x, int y)
{
    return fb[(size_t)b * SW * SH + (size_t)(SH - 1 - y) * SW + (SW - 1 - x)];
}

static bool panel_on_screen(void)
{
    osd_panel_rect_t r = osd_panel_geometry(SW, SH, false);
    for (int b = 0; b < NBUF; b++)
        if (!(seen(b, r.x + r.w / 2, r.y + r.h / 2) & OSD_PANEL_OPAQUE))
            return false;
    return true;
}

static bool wait_inactive(int ms)
{
    for (int i = 0; i < ms && osd_thread_active; i++)
        usleep(1000);
    return !osd_thread_active;
}

/* Show the volume panel, let it draw, hide it: the time the hide took. */
static long show_then_hide(bool coarse)
{
    _panel_coarse_sleep = coarse;
    osd_showVolumeBar(10, false);
    usleep(30000);
    long t0 = now_us();
    osd_hideBar();
    return now_us() - t0;
}

TEST(hide_returns_at_once)
{
    setup(false);
    _panel_coarse_sleep = false;
    osd_showVolumeBar(10, false);
    ASSERT_TRUE(osd_thread_active);
    usleep(30000);
    ASSERT_TRUE(panel_on_screen());
    long t0 = now_us();
    osd_hideBar();
    long took = now_us() - t0;
    printf("    hide took %ld us\n", took);
    ASSERT_TRUE(took < HIDE_LIMIT_US);
    ASSERT_FALSE(osd_thread_active);
    ASSERT_FALSE(osd_bar_activated);
}

TEST(hide_returns_at_once_with_a_coarse_sleep)
{
    /* The case that delayed POWER: no cancellation point in the wait. */
    setup(false);
    long took = show_then_hide(true);
    printf("    hide took %ld us\n", took);
    ASSERT_TRUE(took < HIDE_LIMIT_US);
    ASSERT_FALSE(osd_thread_active);
    _panel_coarse_sleep = false;
}

TEST(screen_is_put_back_after_hide)
{
    setup(false);
    show_then_hide(false);
    ASSERT_EQ(memcmp(fb, before, sizeof(before)), 0);
    ASSERT_TRUE(_panel_px == NULL);
    ASSERT_TRUE(guard_intact());

    show_then_hide(true);
    ASSERT_EQ(memcmp(fb, before, sizeof(before)), 0);
    ASSERT_TRUE(guard_intact());
    _panel_coarse_sleep = false;
}

TEST(shows_again_after_a_hide)
{
    /* The stop request must not stay set: the next indicator would end
     * before drawing anything. */
    setup(false);
    for (int i = 0; i < 5; i++) {
        _panel_coarse_sleep = (i % 2) == 1;
        osd_showVolumeBar(5 + i, false);
        ASSERT_TRUE(osd_thread_active);
        usleep(30000);
        ASSERT_TRUE(osd_thread_active);
        ASSERT_TRUE(panel_on_screen());
        osd_hideBar();
        ASSERT_FALSE(osd_thread_active);
    }
    _panel_coarse_sleep = false;
    ASSERT_EQ(memcmp(fb, before, sizeof(before)), 0);
}

TEST(expired_indicator_ends_by_itself_then_shows_again)
{
    setup(false);
    _panel_coarse_sleep = false;
    osd_showVolumeBar(10, false);
    usleep(30000);
    _bar_timer = getMilliseconds() - 5000; /* its 2 s are over */
    ASSERT_TRUE(wait_inactive(1000));
    ASSERT_EQ(memcmp(fb, before, sizeof(before)), 0);

    long t0 = now_us();
    osd_hideBar(); /* nothing to stop */
    ASSERT_TRUE(now_us() - t0 < HIDE_LIMIT_US);

    osd_showVolumeBar(12, false); /* joins the thread that ended */
    ASSERT_TRUE(osd_thread_active);
    usleep(30000);
    ASSERT_TRUE(panel_on_screen());
    osd_hideBar();
    ASSERT_FALSE(osd_thread_active);
    ASSERT_EQ(memcmp(fb, before, sizeof(before)), 0);
}

TEST(hide_without_an_indicator_does_nothing)
{
    setup(false);
    long t0 = now_us();
    osd_hideBar();
    osd_hideBar();
    ASSERT_TRUE(now_us() - t0 < HIDE_LIMIT_US);
    ASSERT_FALSE(osd_thread_active);
    ASSERT_EQ(memcmp(fb, before, sizeof(before)), 0);
}

TEST(edge_bar_strip_is_restored_once)
{
    /* The thread restores the strip when it ends. A second restore from
     * osd_hideBar() would paint it black again with nothing left to put
     * back. Only the first buffer's strip is saved (as before). */
    setup(true);
    osd_showBrightnessBar(5);
    ASSERT_FALSE(_osd_panel);
    ASSERT_TRUE(osd_thread_active);
    usleep(30000);
    long t0 = now_us();
    osd_hideBar();
    ASSERT_TRUE(now_us() - t0 < HIDE_LIMIT_US);
    ASSERT_FALSE(osd_thread_active);

    int width = meterWidth;
    ASSERT_TRUE(width > 0);
    int wrong = 0;
    for (int row = 0; row < SH; row++)
        for (int x = SW - width; x < SW; x++)
            if (fb[(size_t)row * SW + x] != before[(size_t)row * SW + x])
                wrong++;
    ASSERT_EQ(wrong, 0);

    /* Nothing outside the strips was touched, on any buffer. */
    wrong = 0;
    for (int b = 0; b < NBUF; b++)
        for (int row = 0; row < SH; row++)
            for (int x = 0; x < SW - width; x++) {
                size_t i = (size_t)b * SW * SH + (size_t)row * SW + x;
                if (fb[i] != before[i])
                    wrong++;
            }
    ASSERT_EQ(wrong, 0);
    ASSERT_TRUE(guard_intact());
}

int main(void)
{
    printf("\n=== osd.h: stopping the indicator ===\n\n");
    RUN_TEST(hide_returns_at_once);
    RUN_TEST(hide_returns_at_once_with_a_coarse_sleep);
    RUN_TEST(screen_is_put_back_after_hide);
    RUN_TEST(shows_again_after_a_hide);
    RUN_TEST(expired_indicator_ends_by_itself_then_shows_again);
    RUN_TEST(hide_without_an_indicator_does_nothing);
    RUN_TEST(edge_bar_strip_is_restored_once);
    osd_hideBar();
    if (system("rm -rf " TEST_DIR) != 0)
        fprintf(stderr, "rm failed\n");
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
