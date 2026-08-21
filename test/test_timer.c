/**
 * @file test_timer.c
 * @brief Unit tests for src/common/utils/timer.h
 *
 * Tests the START_TIMER / END_TIMER macros: elapsed time measurement,
 * variable naming isolation, and sequential timer usage.
 *
 * Build and run: make -f Makefile.unit test_timer && ./build_test/test_timer
 */

#include "onion_test.h"
#include <unistd.h>

#include "../src/common/utils/timer.h"

/* ---- START_TIMER / END_TIMER basic elapsed time ---- */

TEST(timer_basic_elapsed) {
    START_TIMER(basic);
    usleep(20000); /* 20 ms */
    END_TIMER(basic);

    ASSERT_GE(basic_milliseconds, 15);
}

/* ---- zero-ish elapsed for trivial work ---- */

TEST(timer_trivial_work) {
    START_TIMER(trivial);
    int dummy = 0;
    (void)dummy;
    END_TIMER(trivial);

    /* Should complete in well under 100 ms */
    ASSERT_GE(trivial_milliseconds, 0);
    ASSERT_TRUE(trivial_milliseconds < 100);
}

/* ---- two independent timers don't interfere ---- */

TEST(timer_two_independent) {
    START_TIMER(first);
    usleep(20000); /* 20 ms */
    END_TIMER(first);

    START_TIMER(second);
    usleep(10000); /* 10 ms */
    END_TIMER(second);

    /* first should be >= second */
    ASSERT_GE(first_milliseconds, second_milliseconds);
}

/* ---- nested timers (outer > inner) ---- */

TEST(timer_nested) {
    START_TIMER(outer);
    usleep(10000); /* 10 ms */
    START_TIMER(inner);
    usleep(10000); /* 10 ms */
    END_TIMER(inner);
    END_TIMER(outer);

    ASSERT_GE(outer_milliseconds, inner_milliseconds);
    ASSERT_GE(inner_milliseconds, 5);
}

/* ---- milliseconds value is non-negative ---- */

TEST(timer_nonnegative) {
    START_TIMER(nonneg);
    END_TIMER(nonneg);

    ASSERT_GE(nonneg_milliseconds, 0);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== timer.h Unit Tests ===\n\n");

    RUN_TEST(timer_basic_elapsed);
    RUN_TEST(timer_trivial_work);
    RUN_TEST(timer_two_independent);
    RUN_TEST(timer_nested);
    RUN_TEST(timer_nonnegative);

    TEST_REPORT();
    return test_failures;
}
