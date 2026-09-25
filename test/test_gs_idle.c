/**
 * @file test_gs_idle.c
 * @brief Unit tests for gs_idleWaitMs (gameSwitcher/gs_idle.h)
 *
 * The GameSwitcher main loop sleeps gs_idleWaitMs() ms in input_waitFor()
 * before each iteration. A pending render step must never be delayed.
 *
 * Build and run: make -f Makefile.unit test_gs_idle
 */

#include "onion_test.h"
#include "../src/gameSwitcher/gs_idle.h"

#define STEP (1000 / 30) /* gs_appState.h time_step */

TEST(fresh_step_waits_full_step) {
    ASSERT_EQ(gs_idleWaitMs(0, STEP), STEP);
}

TEST(partial_step_waits_remainder) {
    ASSERT_EQ(gs_idleWaitMs(1, STEP), STEP - 1);
    ASSERT_EQ(gs_idleWaitMs(STEP - 1, STEP), 1);
}

TEST(due_step_does_not_wait) {
    ASSERT_EQ(gs_idleWaitMs(STEP, STEP), 0);
    ASSERT_EQ(gs_idleWaitMs(STEP + 500, STEP), 0);
    ASSERT_EQ(gs_idleWaitMs(UINT32_MAX, STEP), 0);
}

TEST(wait_never_exceeds_step) {
    for (uint32_t acc = 0; acc < 3 * STEP; acc++) {
        int w = gs_idleWaitMs(acc, STEP);
        ASSERT_GE(w, 0);
        ASSERT_TRUE(w <= STEP);
    }
}

TEST(zero_step_never_waits) {
    ASSERT_EQ(gs_idleWaitMs(0, 0), 0);
}

int main(void)
{
    printf("\n=== gs_idle Unit Tests ===\n\n");

    RUN_TEST(fresh_step_waits_full_step);
    RUN_TEST(partial_step_waits_remainder);
    RUN_TEST(due_step_does_not_wait);
    RUN_TEST(wait_never_exceeds_step);
    RUN_TEST(zero_step_never_waits);

    TEST_REPORT();
    return test_failures;
}
