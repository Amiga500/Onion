/**
 * @file test_has_changed.c
 * @brief Unit tests for _has_changed() from src/common/system/settings_sync.h
 *
 * Tests the pure-logic value-change tracking function used by the
 * settings synchronization system to detect shared-memory updates.
 *
 * Build and run: make -f Makefile.unit test_has_changed
 */

#include "onion_test.h"
#include <limits.h>
#include <stdbool.h>

/* ---- Inline the pure-logic function from settings_sync.h ---- */

static bool _has_changed(int new_value, int *old_value)
{
    if (new_value != *old_value) {
        *old_value = new_value;
        return true;
    }

    return false;
}

/* ---- Tests ---- */

TEST(has_changed_detects_difference) {
    int old = 5;
    ASSERT_TRUE(_has_changed(10, &old));
}

TEST(has_changed_updates_old_value) {
    int old = 5;
    _has_changed(10, &old);
    ASSERT_EQ(old, 10);
}

TEST(has_changed_no_change) {
    int old = 7;
    ASSERT_FALSE(_has_changed(7, &old));
}

TEST(has_changed_no_change_preserves_value) {
    int old = 7;
    _has_changed(7, &old);
    ASSERT_EQ(old, 7);
}

TEST(has_changed_zero_to_nonzero) {
    int old = 0;
    ASSERT_TRUE(_has_changed(1, &old));
    ASSERT_EQ(old, 1);
}

TEST(has_changed_nonzero_to_zero) {
    int old = 42;
    ASSERT_TRUE(_has_changed(0, &old));
    ASSERT_EQ(old, 0);
}

TEST(has_changed_negative_value) {
    int old = 5;
    ASSERT_TRUE(_has_changed(-3, &old));
    ASSERT_EQ(old, -3);
}

TEST(has_changed_negative_to_negative) {
    int old = -10;
    ASSERT_TRUE(_has_changed(-20, &old));
    ASSERT_EQ(old, -20);
}

TEST(has_changed_same_negative) {
    int old = -5;
    ASSERT_FALSE(_has_changed(-5, &old));
    ASSERT_EQ(old, -5);
}

TEST(has_changed_zero_no_change) {
    int old = 0;
    ASSERT_FALSE(_has_changed(0, &old));
    ASSERT_EQ(old, 0);
}

TEST(has_changed_sequential_updates) {
    int old = 0;

    ASSERT_TRUE(_has_changed(1, &old));
    ASSERT_EQ(old, 1);

    ASSERT_TRUE(_has_changed(2, &old));
    ASSERT_EQ(old, 2);

    ASSERT_FALSE(_has_changed(2, &old));
    ASSERT_EQ(old, 2);

    ASSERT_TRUE(_has_changed(0, &old));
    ASSERT_EQ(old, 0);
}

TEST(has_changed_large_values) {
    int old = 0;
    ASSERT_TRUE(_has_changed(INT_MAX, &old));
    ASSERT_EQ(old, INT_MAX);
}

TEST(has_changed_boundary_min) {
    int old = 0;
    ASSERT_TRUE(_has_changed(INT_MIN, &old));
    ASSERT_EQ(old, INT_MIN);
}

/* ---- Simulated settings field tracking ---- */

TEST(has_changed_brightness_scenario) {
    /* Simulates settings.brightness being tracked via shared memory */
    int brightness = 7;

    /* No change from shared mem */
    ASSERT_FALSE(_has_changed(7, &brightness));

    /* User changes brightness via hardware */
    ASSERT_TRUE(_has_changed(5, &brightness));
    ASSERT_EQ(brightness, 5);

    /* Stays at 5 */
    ASSERT_FALSE(_has_changed(5, &brightness));

    /* Changed again */
    ASSERT_TRUE(_has_changed(10, &brightness));
    ASSERT_EQ(brightness, 10);
}

TEST(has_changed_volume_scenario) {
    int volume = 20;

    ASSERT_TRUE(_has_changed(15, &volume));
    ASSERT_EQ(volume, 15);

    ASSERT_TRUE(_has_changed(0, &volume));
    ASSERT_EQ(volume, 0);

    ASSERT_FALSE(_has_changed(0, &volume));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== settings_sync.h _has_changed Unit Tests ===\n\n");

    RUN_TEST(has_changed_detects_difference);
    RUN_TEST(has_changed_updates_old_value);
    RUN_TEST(has_changed_no_change);
    RUN_TEST(has_changed_no_change_preserves_value);
    RUN_TEST(has_changed_zero_to_nonzero);
    RUN_TEST(has_changed_nonzero_to_zero);
    RUN_TEST(has_changed_negative_value);
    RUN_TEST(has_changed_negative_to_negative);
    RUN_TEST(has_changed_same_negative);
    RUN_TEST(has_changed_zero_no_change);
    RUN_TEST(has_changed_sequential_updates);
    RUN_TEST(has_changed_large_values);
    RUN_TEST(has_changed_boundary_min);
    RUN_TEST(has_changed_brightness_scenario);
    RUN_TEST(has_changed_volume_scenario);

    TEST_REPORT();
    return test_failures;
}
