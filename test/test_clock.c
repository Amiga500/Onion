/**
 * @file test_clock.c
 * @brief Unit tests for src/common/system/clock.h
 *
 * Tests getMilliseconds() and getSeconds() timing functions.
 *
 * Build and run: make -f Makefile.unit test_clock && ./build_test/test_clock
 */

#include "onion_test.h"
#include <time.h>
#include <unistd.h>

#include "../src/common/system/clock.h"

/* Maximum allowed drift between CLOCK_MONOTONIC_COARSE and CLOCK_MONOTONIC_RAW */
#define MAX_CLOCK_DRIFT_SECONDS 5

/* ---- getMilliseconds ---- */

TEST(getMilliseconds_positive) {
    long ms = getMilliseconds();
    ASSERT_GT(ms, 0);
}

TEST(getMilliseconds_monotonic) {
    long a = getMilliseconds();
    long b = getMilliseconds();
    ASSERT_GE(b, a);
}

TEST(getMilliseconds_advances_with_sleep) {
    long before = getMilliseconds();
    usleep(20000); /* 20ms */
    long after = getMilliseconds();
    ASSERT_GE(after - before, 10); /* Allow jitter: at least 10ms */
}

/* ---- getSeconds ---- */

TEST(getSeconds_positive) {
    int s = getSeconds();
    ASSERT_GT(s, 0);
}

TEST(getSeconds_monotonic) {
    int a = getSeconds();
    int b = getSeconds();
    ASSERT_GE(b, a);
}

TEST(getSeconds_consistent_with_millis) {
    /* getSeconds() and getMilliseconds() use different clocks
     * (COARSE vs RAW) but should agree within a few seconds. */
    int sec = getSeconds();
    long ms = getMilliseconds();
    long diff = (ms / 1000) - (long)sec;
    ASSERT_TRUE(diff >= -MAX_CLOCK_DRIFT_SECONDS && diff <= MAX_CLOCK_DRIFT_SECONDS);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== clock.h Unit Tests ===\n\n");

    RUN_TEST(getMilliseconds_positive);
    RUN_TEST(getMilliseconds_monotonic);
    RUN_TEST(getMilliseconds_advances_with_sleep);

    RUN_TEST(getSeconds_positive);
    RUN_TEST(getSeconds_monotonic);
    RUN_TEST(getSeconds_consistent_with_millis);

    TEST_REPORT();
    return test_failures;
}
