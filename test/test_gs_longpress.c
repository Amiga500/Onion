/**
 * @file test_gs_longpress.c
 * @brief Tests for src/gameSwitcher/gs_longpress.h (production header)
 *
 * The GameSwitcher Y long press (full-screen view) must depend on how long
 * the key is held, not on how many times the main loop ran: counting 75
 * iterations meant ~0.3 s with a 4 ms loop and ~2.5 s with a 33 ms one.
 *
 * Build and run: make -f Makefile.unit test_gs_longpress
 */

#include "../src/gameSwitcher/gs_longpress.h"
#include "onion_test.h"

static void hold(LongPress_s *lp, uint32_t from, uint32_t to, uint32_t step, int *fired)
{
    for (uint32_t t = from; t <= to; t += step)
        if (longPress_held(lp, t, GS_LONG_PRESS_MS))
            (*fired)++;
}

TEST(short_press_is_short)
{
    LongPress_s lp = {0};
    int fired = 0;
    hold(&lp, 1000, 1200, 33, &fired);
    ASSERT_EQ(0, fired);
    ASSERT_TRUE(longPress_release(&lp));
}

/* Same threshold whatever the loop period: 33 ms frames or 4 ms spins. */
TEST(long_press_fires_once_at_threshold_33ms_loop)
{
    LongPress_s lp = {0};
    int fired = 0;
    hold(&lp, 1000, 1290, 33, &fired);
    ASSERT_EQ(0, fired);
    hold(&lp, 1323, 3000, 33, &fired);
    ASSERT_EQ(1, fired);
    ASSERT_FALSE(longPress_release(&lp));
}

TEST(long_press_fires_once_at_threshold_4ms_loop)
{
    LongPress_s lp = {0};
    int fired = 0;
    hold(&lp, 5000, 5296, 4, &fired);
    ASSERT_EQ(0, fired);
    hold(&lp, 5300, 5400, 4, &fired);
    ASSERT_EQ(1, fired);
    ASSERT_FALSE(longPress_release(&lp));
}

/* Release without a held call (key pressed in full-screen view): short. */
TEST(release_without_hold_is_short)
{
    LongPress_s lp = {0};
    ASSERT_TRUE(longPress_release(&lp));
}

TEST(state_resets_between_presses)
{
    LongPress_s lp = {0};
    int fired = 0;
    hold(&lp, 0, 400, 33, &fired);
    ASSERT_EQ(1, fired);
    ASSERT_FALSE(longPress_release(&lp));
    fired = 0;
    hold(&lp, 10000, 10100, 33, &fired);
    ASSERT_EQ(0, fired);
    ASSERT_TRUE(longPress_release(&lp));
}

TEST(tick_wraparound)
{
    LongPress_s lp = {0};
    ASSERT_FALSE(longPress_held(&lp, 0xFFFFFF00u, GS_LONG_PRESS_MS));
    ASSERT_FALSE(longPress_held(&lp, 0x00000010u, GS_LONG_PRESS_MS));
    ASSERT_TRUE(longPress_held(&lp, 0x00000040u, GS_LONG_PRESS_MS));
}

int main(void)
{
    printf("\n=== gs_longpress.h Unit Tests ===\n\n");
    RUN_TEST(short_press_is_short);
    RUN_TEST(long_press_fires_once_at_threshold_33ms_loop);
    RUN_TEST(long_press_fires_once_at_threshold_4ms_loop);
    RUN_TEST(release_without_hold_is_short);
    RUN_TEST(state_resets_between_presses);
    RUN_TEST(tick_wraparound);
    TEST_REPORT();
    return test_failures;
}
