/**
 * @file test_gs_hold.c
 * @brief Unit tests for gs_holdReached (gameSwitcher/gs_hold.h)
 *
 * GameSwitcher: tap Y toggles minimal view, hold Y shows the fullscreen
 * image. The hold used to count loop iterations (75), which tied the
 * threshold to the loop rate; it is now GS_Y_HOLD_FULLSCREEN_MS of ticks.
 *
 * Build and run: make -f Makefile.unit test_gs_hold
 */

#include "onion_test.h"
#include "../src/gameSwitcher/gs_hold.h"

TEST(not_held_never_reached) {
    ASSERT_FALSE(gs_holdReached(false, 0, 100000, GS_Y_HOLD_FULLSCREEN_MS));
}

TEST(tap_below_threshold) {
    ASSERT_FALSE(gs_holdReached(true, 1000, 1000, GS_Y_HOLD_FULLSCREEN_MS));
    ASSERT_FALSE(gs_holdReached(true, 1000, 1000 + GS_Y_HOLD_FULLSCREEN_MS - 1, GS_Y_HOLD_FULLSCREEN_MS));
}

TEST(hold_at_and_after_threshold) {
    ASSERT_TRUE(gs_holdReached(true, 1000, 1000 + GS_Y_HOLD_FULLSCREEN_MS, GS_Y_HOLD_FULLSCREEN_MS));
    ASSERT_TRUE(gs_holdReached(true, 1000, 60000, GS_Y_HOLD_FULLSCREEN_MS));
}

TEST(independent_of_loop_rate) {
    /* 30 Hz loop (33 ms steps): reached on the first step at or past 700 ms. */
    uint32_t down = 5000, now = down;
    int steps = 0;
    while (!gs_holdReached(true, down, now, GS_Y_HOLD_FULLSCREEN_MS)) {
        now += 33;
        steps++;
    }
    ASSERT_EQ(steps, 22); /* 22 * 33 = 726 ms */
    /* Fast loop (1 ms steps): same wall-clock threshold. */
    now = down;
    steps = 0;
    while (!gs_holdReached(true, down, now, GS_Y_HOLD_FULLSCREEN_MS)) {
        now += 1;
        steps++;
    }
    ASSERT_EQ(steps, GS_Y_HOLD_FULLSCREEN_MS);
}

TEST(tick_wraparound) {
    uint32_t down = UINT32_MAX - 100;
    ASSERT_FALSE(gs_holdReached(true, down, down + 699, GS_Y_HOLD_FULLSCREEN_MS));
    ASSERT_TRUE(gs_holdReached(true, down, down + 700, GS_Y_HOLD_FULLSCREEN_MS));
}

int main(void)
{
    printf("\n=== gs_hold Unit Tests ===\n\n");

    RUN_TEST(not_held_never_reached);
    RUN_TEST(tap_below_threshold);
    RUN_TEST(hold_at_and_after_threshold);
    RUN_TEST(independent_of_loop_rate);
    RUN_TEST(tick_wraparound);

    TEST_REPORT();
    return test_failures;
}
