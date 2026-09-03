/**
 * @file test_settings_has_changed.c
 * @brief Unit tests for _has_changed() from settings_sync.h
 *
 * Tests the shared-memory change detection helper used by the
 * settings synchronization module. The function compares a new
 * value with an old value stored via pointer; if different, it
 * updates the old value and returns true.
 *
 * Build and run: make -f Makefile.unit test_settings_has_changed
 */

#include "onion_test.h"
#include <stdbool.h>

/* ---- Inline the function under test ---- */

static bool _has_changed(int new_value, int *old_value)
{
    if (new_value != *old_value) {
        *old_value = new_value;
        return true;
    }
    return false;
}

/* ==== Tests: basic behavior ==== */

TEST(has_changed_different_values) {
    int old = 5;
    ASSERT_TRUE(_has_changed(10, &old));
    ASSERT_EQ(old, 10);
}

TEST(has_changed_same_value) {
    int old = 5;
    ASSERT_FALSE(_has_changed(5, &old));
    ASSERT_EQ(old, 5);
}

TEST(has_changed_zero_to_nonzero) {
    int old = 0;
    ASSERT_TRUE(_has_changed(42, &old));
    ASSERT_EQ(old, 42);
}

TEST(has_changed_nonzero_to_zero) {
    int old = 42;
    ASSERT_TRUE(_has_changed(0, &old));
    ASSERT_EQ(old, 0);
}

/* ==== Tests: sequential calls ==== */

TEST(has_changed_first_call_true_second_false) {
    int old = 0;
    ASSERT_TRUE(_has_changed(5, &old));
    ASSERT_FALSE(_has_changed(5, &old));
}

TEST(has_changed_multiple_changes) {
    int old = 0;
    ASSERT_TRUE(_has_changed(1, &old));
    ASSERT_EQ(old, 1);
    ASSERT_TRUE(_has_changed(2, &old));
    ASSERT_EQ(old, 2);
    ASSERT_TRUE(_has_changed(3, &old));
    ASSERT_EQ(old, 3);
    ASSERT_FALSE(_has_changed(3, &old));
    ASSERT_EQ(old, 3);
}

/* ==== Tests: negative values ==== */

TEST(has_changed_negative_to_positive) {
    int old = -5;
    ASSERT_TRUE(_has_changed(5, &old));
    ASSERT_EQ(old, 5);
}

TEST(has_changed_negative_same) {
    int old = -10;
    ASSERT_FALSE(_has_changed(-10, &old));
    ASSERT_EQ(old, -10);
}

/* ==== Tests: boundary values ==== */

TEST(has_changed_int_max) {
    int old = 0;
    ASSERT_TRUE(_has_changed(2147483647, &old));
    ASSERT_EQ(old, 2147483647);
}

TEST(has_changed_int_min) {
    int old = 0;
    ASSERT_TRUE(_has_changed(-2147483647 - 1, &old));
    /* Verify it was stored correctly */
    ASSERT_FALSE(_has_changed(-2147483647 - 1, &old));
}

/* ==== Tests: simulating brightness/volume sync ==== */

TEST(has_changed_brightness_sync) {
    /* Simulates: monitor reads brightness, detects change */
    int brightness = 5;
    ASSERT_TRUE(_has_changed(7, &brightness));   /* user changed brightness */
    ASSERT_EQ(brightness, 7);
    ASSERT_FALSE(_has_changed(7, &brightness));  /* no change next poll */
    ASSERT_TRUE(_has_changed(3, &brightness));   /* changed again */
    ASSERT_EQ(brightness, 3);
}

TEST(has_changed_volume_sync) {
    int volume = 10;
    /* No change */
    ASSERT_FALSE(_has_changed(10, &volume));
    /* Volume up */
    ASSERT_TRUE(_has_changed(11, &volume));
    ASSERT_EQ(volume, 11);
    /* Mute */
    ASSERT_TRUE(_has_changed(0, &volume));
    ASSERT_EQ(volume, 0);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== settings_sync.h _has_changed Unit Tests ===\n\n");

    /* Basic behavior */
    RUN_TEST(has_changed_different_values);
    RUN_TEST(has_changed_same_value);
    RUN_TEST(has_changed_zero_to_nonzero);
    RUN_TEST(has_changed_nonzero_to_zero);

    /* Sequential calls */
    RUN_TEST(has_changed_first_call_true_second_false);
    RUN_TEST(has_changed_multiple_changes);

    /* Negative values */
    RUN_TEST(has_changed_negative_to_positive);
    RUN_TEST(has_changed_negative_same);

    /* Boundary values */
    RUN_TEST(has_changed_int_max);
    RUN_TEST(has_changed_int_min);

    /* Simulated usage */
    RUN_TEST(has_changed_brightness_sync);
    RUN_TEST(has_changed_volume_sync);

    TEST_REPORT();
    return test_failures;
}
