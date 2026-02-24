/**
 * @file test_battery.c
 * @brief Unit tests for the battery_isCharging() cache timing logic.
 *
 * The charging cache avoids repeated GPIO reads / subprocess spawns
 * by caching the result for BATTERY_CHARGING_CACHE_MS (2 seconds).
 * This test validates the elapsed_ms calculation used to decide
 * whether to return the cached value or refresh it.
 *
 * Build and run: make -f Makefile.unit test_battery
 */

#include "onion_test.h"
#include <time.h>
#include <stdbool.h>

/* ---- Constants from battery.h ---- */
#define BATTERY_CHARGING_CACHE_MS 2000

/* ---- Extract the pure elapsed_ms calculation from battery_isCharging() ---- */

/**
 * Compute elapsed milliseconds between two timespec values.
 * This is the exact logic from battery_isCharging() in battery.h.
 *
 * Returns: elapsed time in ms, or -1 if cache is invalid.
 */
static long compute_elapsed_ms(struct timespec *cache_ts, struct timespec *now)
{
    long elapsed_ms = (now->tv_sec - cache_ts->tv_sec) * 1000L;
    long ns_diff = now->tv_nsec - cache_ts->tv_nsec;
    if (ns_diff < 0) {
        elapsed_ms -= 1000L;
        ns_diff += 1000000000L;
    }
    elapsed_ms += ns_diff / 1000000L;
    return elapsed_ms;
}

/**
 * Determine if the cache is still valid (elapsed < BATTERY_CHARGING_CACHE_MS).
 */
static bool cache_is_valid(struct timespec *cache_ts, struct timespec *now)
{
    long elapsed_ms = compute_elapsed_ms(cache_ts, now);
    return (elapsed_ms >= 0 && elapsed_ms < BATTERY_CHARGING_CACHE_MS);
}

/* ---- Tests: elapsed_ms calculation ---- */

TEST(elapsed_zero_when_same_time) {
    struct timespec ts = {100, 500000000L};
    long elapsed = compute_elapsed_ms(&ts, &ts);
    ASSERT_EQ(elapsed, 0);
}

TEST(elapsed_1000ms_for_1_second) {
    struct timespec cache = {100, 0};
    struct timespec now = {101, 0};
    long elapsed = compute_elapsed_ms(&cache, &now);
    ASSERT_EQ(elapsed, 1000);
}

TEST(elapsed_500ms) {
    struct timespec cache = {100, 0};
    struct timespec now = {100, 500000000L};
    long elapsed = compute_elapsed_ms(&cache, &now);
    ASSERT_EQ(elapsed, 500);
}

TEST(elapsed_handles_nsec_borrow) {
    /* now.tv_nsec < cache.tv_nsec — requires nsec borrow */
    struct timespec cache = {100, 800000000L}; /* 100.8s */
    struct timespec now = {101, 200000000L};   /* 101.2s → 0.4s elapsed */
    long elapsed = compute_elapsed_ms(&cache, &now);
    ASSERT_EQ(elapsed, 400);
}

TEST(elapsed_handles_nsec_borrow_exact) {
    /* 1 nanosecond past the second boundary */
    struct timespec cache = {100, 999999999L};
    struct timespec now = {101, 0};
    long elapsed = compute_elapsed_ms(&cache, &now);
    /* 1 nanosecond = 0ms when truncated */
    ASSERT_EQ(elapsed, 0);
}

TEST(elapsed_2000ms_exactly) {
    struct timespec cache = {100, 0};
    struct timespec now = {102, 0};
    long elapsed = compute_elapsed_ms(&cache, &now);
    ASSERT_EQ(elapsed, 2000);
}

TEST(elapsed_1999ms) {
    struct timespec cache = {100, 0};
    struct timespec now = {101, 999000000L};
    long elapsed = compute_elapsed_ms(&cache, &now);
    ASSERT_EQ(elapsed, 1999);
}

TEST(elapsed_large_gap) {
    struct timespec cache = {0, 0};
    struct timespec now = {3600, 0}; /* 1 hour */
    long elapsed = compute_elapsed_ms(&cache, &now);
    ASSERT_EQ(elapsed, 3600000L);
}

TEST(elapsed_negative_when_clock_wraps) {
    /* If 'now' is before 'cache', elapsed is negative */
    struct timespec cache = {200, 0};
    struct timespec now = {100, 0};
    long elapsed = compute_elapsed_ms(&cache, &now);
    ASSERT_TRUE(elapsed < 0);
}

/* ---- Tests: cache_is_valid ---- */

TEST(cache_valid_at_0ms) {
    struct timespec cache = {100, 0};
    struct timespec now = {100, 0};
    ASSERT_TRUE(cache_is_valid(&cache, &now));
}

TEST(cache_valid_at_1999ms) {
    struct timespec cache = {100, 0};
    struct timespec now = {101, 999000000L};
    ASSERT_TRUE(cache_is_valid(&cache, &now));
}

TEST(cache_expired_at_2000ms) {
    struct timespec cache = {100, 0};
    struct timespec now = {102, 0};
    ASSERT_FALSE(cache_is_valid(&cache, &now));
}

TEST(cache_expired_at_2001ms) {
    struct timespec cache = {100, 0};
    struct timespec now = {102, 1000000L};
    ASSERT_FALSE(cache_is_valid(&cache, &now));
}

TEST(cache_invalid_when_negative_elapsed) {
    /* Clock went backward — cache should not be considered valid */
    struct timespec cache = {200, 0};
    struct timespec now = {100, 0};
    ASSERT_FALSE(cache_is_valid(&cache, &now));
}

TEST(cache_valid_at_1ms) {
    struct timespec cache = {100, 0};
    struct timespec now = {100, 1000000L};
    ASSERT_TRUE(cache_is_valid(&cache, &now));
}

TEST(cache_valid_with_nsec_borrow) {
    struct timespec cache = {100, 900000000L};
    struct timespec now = {101, 100000000L}; /* 200ms elapsed */
    ASSERT_TRUE(cache_is_valid(&cache, &now));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== battery.h Cache Timing Unit Tests ===\n\n");

    /* elapsed_ms calculation */
    RUN_TEST(elapsed_zero_when_same_time);
    RUN_TEST(elapsed_1000ms_for_1_second);
    RUN_TEST(elapsed_500ms);
    RUN_TEST(elapsed_handles_nsec_borrow);
    RUN_TEST(elapsed_handles_nsec_borrow_exact);
    RUN_TEST(elapsed_2000ms_exactly);
    RUN_TEST(elapsed_1999ms);
    RUN_TEST(elapsed_large_gap);
    RUN_TEST(elapsed_negative_when_clock_wraps);

    /* cache validity */
    RUN_TEST(cache_valid_at_0ms);
    RUN_TEST(cache_valid_at_1999ms);
    RUN_TEST(cache_expired_at_2000ms);
    RUN_TEST(cache_expired_at_2001ms);
    RUN_TEST(cache_invalid_when_negative_elapsed);
    RUN_TEST(cache_valid_at_1ms);
    RUN_TEST(cache_valid_with_nsec_borrow);

    TEST_REPORT();
    return test_failures;
}
