/**
 * @file test_msleep.c
 * @brief Unit tests for src/common/utils/msleep.h
 *
 * Tests the millisecond-precision sleep utility: error handling,
 * timing accuracy, and interrupt flag behavior.
 *
 * Build and run: make -f Makefile.unit test_msleep && ./build_test/test_msleep
 */

#include "onion_test.h"
#include <unistd.h>
#include <time.h>

#include "../src/common/utils/msleep.h"

/* Helper: get current time in milliseconds */
static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* ---- negative input ---- */

TEST(msleep_negative_returns_error) {
    int ret = msleep(-1);
    ASSERT_EQ(ret, -1);
}

/* ---- zero sleep ---- */

TEST(msleep_zero_succeeds) {
    int ret = msleep(0);
    ASSERT_EQ(ret, 0);
}

/* ---- short sleep timing ---- */

TEST(msleep_20ms_timing) {
    long before = now_ms();
    int ret = msleep(20);
    long after = now_ms();
    ASSERT_EQ(ret, 0);
    /* Should sleep at least 15ms (allow jitter) */
    ASSERT_GE(after - before, 15);
}

/* ---- interrupt flag ---- */

TEST(msleep_interrupt_flag_reset) {
    /* Verify the interrupt flag is reset after msleep completes */
    msleep_interrupt = 0;
    msleep(1);
    ASSERT_EQ(msleep_interrupt, 0);
}

TEST(msleep_interrupt_flag_set_before) {
    /* If interrupt flag is set before sleep, behavior should be defined */
    msleep_interrupt = 1;
    /* msleep with interrupt set: nanosleep returns 0 on first try,
     * but if EINTR occurs, the loop exits because flag is set */
    int ret = msleep(1);
    /* Flag should be reset after msleep */
    ASSERT_EQ(msleep_interrupt, 0);
    ASSERT_EQ(ret, 0);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== msleep.h Unit Tests ===\n\n");

    RUN_TEST(msleep_negative_returns_error);
    RUN_TEST(msleep_zero_succeeds);
    RUN_TEST(msleep_20ms_timing);
    RUN_TEST(msleep_interrupt_flag_reset);
    RUN_TEST(msleep_interrupt_flag_set_before);

    TEST_REPORT();
    return test_failures;
}
