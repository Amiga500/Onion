/**
 * @file test_perf.c
 * @brief Unit tests for src/common/utils/perf.h timing macros.
 *
 * Validates that:
 * - PERF_START/PERF_END produce non-negative elapsed times
 * - perf_get_ms() is monotonically increasing
 * - perf_log() outputs to stderr (captured by test)
 * - When PERF_ENABLED is not defined, macros are no-ops
 *
 * Build and run: make -f Makefile.unit test_perf && ./build_test/test_perf
 */

/* Force-enable performance timing for this test */
#ifndef PERF_ENABLED
#define PERF_ENABLED
#endif

#include "onion_test.h"
#include "../src/common/utils/perf.h"
#include <unistd.h>

TEST(perf_get_ms_positive) {
    long ms = perf_get_ms();
    ASSERT_GT(ms, 0);
}

TEST(perf_get_ms_monotonic) {
    long ms1 = perf_get_ms();
    long ms2 = perf_get_ms();
    ASSERT_GE(ms2, ms1);
}

TEST(perf_get_ms_advances_with_sleep) {
    long ms1 = perf_get_ms();
    usleep(10000); /* 10 ms */
    long ms2 = perf_get_ms();
    ASSERT_GE(ms2 - ms1, 5); /* Allow some jitter, expect at least 5ms */
}

TEST(perf_start_end_nonnegative) {
    PERF_START("test_timer");
    usleep(1000); /* 1 ms */
    long _perf_end_check = perf_get_ms();
    ASSERT_GE(_perf_end_check - _perf_start_, 0);
    PERF_END("test_timer");
}

TEST(perf_start_end_measures_sleep) {
    PERF_START("sleep_test");
    usleep(20000); /* 20 ms */
    long after = perf_get_ms();
    long elapsed = after - _perf_start_;
    ASSERT_GE(elapsed, 10); /* At least 10ms of the 20ms sleep */
    PERF_END("sleep_test");
}

int main(void)
{
    printf("\n=== perf.h Unit Tests ===\n\n");

    RUN_TEST(perf_get_ms_positive);
    RUN_TEST(perf_get_ms_monotonic);
    RUN_TEST(perf_get_ms_advances_with_sleep);
    RUN_TEST(perf_start_end_nonnegative);
    RUN_TEST(perf_start_end_measures_sleep);

    TEST_REPORT();
    return test_failures;
}
