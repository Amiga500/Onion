/**
 * @file test_warn_show.c
 * @brief Tests for batteryWarning_wanted() (src/batmon/warn_show.h)
 *
 * The low-battery icon is not shown while charging: on the Mini Plus and
 * Mini Flip the level stays under the threshold for a while after the
 * charger is plugged in, and the icon stayed on (robcodedev).
 *
 * Build and run: make -f Makefile.unit test_warn_show
 */

#include "onion_test.h"

#include "../src/batmon/warn_show.h"

TEST(low_level_shows_it) {
    ASSERT_TRUE(batteryWarning_wanted(false, false, 3, 10));
    ASSERT_TRUE(batteryWarning_wanted(false, false, 9, 10));
    ASSERT_TRUE(batteryWarning_wanted(false, false, 0, 10));
}

TEST(charging_hides_it_at_any_level) {
    ASSERT_FALSE(batteryWarning_wanted(false, true, 3, 10));
    ASSERT_FALSE(batteryWarning_wanted(false, true, 4, 10));
    ASSERT_FALSE(batteryWarning_wanted(false, true, 5, 10));
    ASSERT_FALSE(batteryWarning_wanted(false, true, 0, 10));
}

TEST(unplugged_again_shows_it_again) {
    ASSERT_FALSE(batteryWarning_wanted(false, true, 6, 10));
    ASSERT_TRUE(batteryWarning_wanted(false, false, 6, 10));
}

TEST(standby_and_mini_charging_hide_it) {
    ASSERT_FALSE(batteryWarning_wanted(true, false, 3, 10));
    ASSERT_FALSE(batteryWarning_wanted(false, false, 500, 10));
}

TEST(at_or_above_the_threshold_or_unknown_no_icon) {
    ASSERT_FALSE(batteryWarning_wanted(false, false, 10, 10));
    ASSERT_FALSE(batteryWarning_wanted(false, false, 80, 10));
    ASSERT_FALSE(batteryWarning_wanted(false, false, -1, 10));
    ASSERT_FALSE(batteryWarning_wanted(false, false, 3, 0)); // warning off (0)
}

int main(void)
{
    printf("\n=== warn_show.h Unit Tests ===\n\n");

    RUN_TEST(low_level_shows_it);
    RUN_TEST(charging_hides_it_at_any_level);
    RUN_TEST(unplugged_again_shows_it_again);
    RUN_TEST(standby_and_mini_charging_hide_it);
    RUN_TEST(at_or_above_the_threshold_or_unknown_no_icon);

    TEST_REPORT();
    return test_failures;
}
