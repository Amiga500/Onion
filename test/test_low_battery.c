/**
 * @file test_low_battery.c
 * @brief Tests for src/keymon/low_battery.h (production header)
 *
 * "Low battery: save and quit": no reading (-1) never counts as the level
 * reached, and only a game in front is saved and closed.
 *
 * Build and run: make -f Makefile.unit test_low_battery
 */

#include "../src/keymon/low_battery.h"
#include "onion_test.h"

TEST(level_reached_at_or_under_the_setting)
{
    ASSERT_TRUE(lowBattery_levelReached(20, 20));
    ASSERT_TRUE(lowBattery_levelReached(4, 5));
    ASSERT_TRUE(lowBattery_levelReached(0, 1));
    ASSERT_FALSE(lowBattery_levelReached(21, 20));
    ASSERT_FALSE(lowBattery_levelReached(6, 5));
}

TEST(no_reading_is_not_the_level)
{
    /* battery_chargingLevel(): -1 with /tmp/percBat missing or unreadable */
    ASSERT_FALSE(lowBattery_levelReached(-1, 5));
    ASSERT_FALSE(lowBattery_levelReached(-1, 20));
}

TEST(off_never_fires)
{
    ASSERT_FALSE(lowBattery_levelReached(0, 0));
    ASSERT_FALSE(lowBattery_levelReached(-1, 0));
}

TEST(only_a_game_is_closed)
{
    ASSERT_TRUE(lowBattery_inGame(MODE_GAME));
    ASSERT_TRUE(lowBattery_inGame(MODE_DRASTIC));
    ASSERT_FALSE(lowBattery_inGame(MODE_MAIN_UI));
    ASSERT_FALSE(lowBattery_inGame(MODE_SWITCHER));
    ASSERT_FALSE(lowBattery_inGame(MODE_APPS));
    ASSERT_FALSE(lowBattery_inGame(MODE_ADVMENU));
    ASSERT_FALSE(lowBattery_inGame(MODE_UNKNOWN));
}

int main(void)
{
    printf("\n=== Low battery save and quit ===\n\n");
    RUN_TEST(level_reached_at_or_under_the_setting);
    RUN_TEST(no_reading_is_not_the_level);
    RUN_TEST(off_never_fires);
    RUN_TEST(only_a_game_is_closed);
    TEST_REPORT();
    return test_failures;
}
