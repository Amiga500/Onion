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

/* getSeconds() reads CLOCK_MONOTONIC_COARSE and getMilliseconds()
 * CLOCK_MONOTONIC_RAW. Comparing the two with each other is unreliable on
 * a host: CLOCK_MONOTONIC is slewed by NTP and RAW is not, so on a PC with
 * a long uptime they drift apart by more than any fixed margin. Compare
 * each helper with its own clock instead. */
TEST(getSeconds_matches_monotonic_coarse) {
    struct timespec before, after;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &before);
    int sec = getSeconds();
    clock_gettime(CLOCK_MONOTONIC_COARSE, &after);
    ASSERT_TRUE(sec >= (int)before.tv_sec && sec <= (int)after.tv_sec);
}

TEST(getMilliseconds_matches_monotonic_raw) {
    struct timespec before, after;
    clock_gettime(CLOCK_MONOTONIC_RAW, &before);
    long ms = getMilliseconds();
    clock_gettime(CLOCK_MONOTONIC_RAW, &after);
    long lo = (long)before.tv_sec * 1000L + before.tv_nsec / 1000000L;
    long hi = (long)after.tv_sec * 1000L + after.tv_nsec / 1000000L;
    ASSERT_TRUE(ms >= lo && ms <= hi);
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
    RUN_TEST(getSeconds_matches_monotonic_coarse);
    RUN_TEST(getMilliseconds_matches_monotonic_raw);

    TEST_REPORT();
    return test_failures;
}
