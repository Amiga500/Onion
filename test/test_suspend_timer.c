/**
 * @file test_suspend_timer.c
 * @brief Tests for src/keymon/suspend_timer.h (production header)
 *
 * Standby on the Mini and Mini Plus: the wait before the power-off counts
 * from the start of the standby, so input events cannot put it off forever.
 *
 * Build and run: make -f Makefile.unit test_suspend_timer
 */

#include "../src/keymon/suspend_timer.h"
#include "onion_test.h"

TEST(full_timeout_at_the_start)
{
    ASSERT_EQ(suspendTimer_remaining(60000, 0), 60000);
    ASSERT_EQ(suspendTimer_remaining(360000, 0), 360000);
}

TEST(events_do_not_restart_the_wait)
{
    /* An event 50 s in: the next poll waits the 10 s left, not 60 s again */
    ASSERT_EQ(suspendTimer_remaining(60000, 50000), 10000);
    ASSERT_EQ(suspendTimer_remaining(60000, 59999), 1);
}

TEST(elapsed_is_zero_left)
{
    ASSERT_EQ(suspendTimer_remaining(60000, 60000), 0);
    ASSERT_EQ(suspendTimer_remaining(60000, 3600000), 0);
}

TEST(no_timeout_stays_awake)
{
    /* /tmp/stay_awake or sleep timer Off: poll(-1) */
    ASSERT_EQ(suspendTimer_remaining(-1, 0), -1);
    ASSERT_EQ(suspendTimer_remaining(-1, 3600000), -1);
}

TEST(clock_gone_back_counts_from_now)
{
    ASSERT_EQ(suspendTimer_remaining(60000, -5000), 60000);
}

int main(void)
{
    printf("\n=== Standby power-off timer ===\n\n");
    RUN_TEST(full_timeout_at_the_start);
    RUN_TEST(events_do_not_restart_the_wait);
    RUN_TEST(elapsed_is_zero_left);
    RUN_TEST(no_timeout_stays_awake);
    RUN_TEST(clock_gone_back_counts_from_now);
    TEST_REPORT();
    return test_failures;
}
