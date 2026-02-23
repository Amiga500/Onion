/**
 * @file test_clock.c
 * @brief Unit tests for src/common/system/clock.h
 *
 * Tests the host-portable clock utility functions: getMilliseconds, getSeconds.
 *
 * Build and run: make -f Makefile.unit test_clock && ./build_test/test_clock
 */

#include "onion_test.h"
#include <unistd.h>
#include <time.h>

/* Include only the portable parts of clock.h we can test on the host.
 * The RTC / system-clock functions require /dev/rtc0 and are not testable
 * on a CI host, but getMilliseconds() and getSeconds() use
 * CLOCK_MONOTONIC_RAW / CLOCK_MONOTONIC_COARSE which work everywhere. */
#include "../src/common/system/clock.h"

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
    usleep(20000); /* 20 ms */
    long after = getMilliseconds();
    ASSERT_GE(after - before, 15); /* allow small scheduling jitter */
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
    /* Allow up to 5 seconds of drift between the two clocks */
    ASSERT_TRUE(diff >= -5 && diff <= 5);
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
