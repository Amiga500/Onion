/**
 * @file test_flip_suspend.c
 * @brief Tests for src/keymon/flip_suspend.h (production header)
 *
 * The Mini Flip suspend loop may skip the charger check (a popen of
 * axp_test) only when nothing can happen in that iteration: lid state
 * unchanged and suspend timeout not elapsed.
 *
 * Build and run: make -f Makefile.unit test_flip_suspend
 */

#include "../src/keymon/flip_suspend.h"
#include "onion_test.h"

TEST(unchanged_lid_before_timeout_is_idle)
{
    ASSERT_TRUE(flipSuspend_nothingToDo(0, 0, false));
    ASSERT_TRUE(flipSuspend_nothingToDo(1, 1, false));
    ASSERT_TRUE(flipSuspend_nothingToDo(-1, -1, false));
}

TEST(lid_opened_needs_check)
{
    ASSERT_FALSE(flipSuspend_nothingToDo(1, 0, false));
}

TEST(lid_closed_or_unreadable_change_needs_check)
{
    /* The saved state is updated in these cases. */
    ASSERT_FALSE(flipSuspend_nothingToDo(0, 1, false));
    ASSERT_FALSE(flipSuspend_nothingToDo(-1, 0, false));
    ASSERT_FALSE(flipSuspend_nothingToDo(0, -1, false));
}

TEST(timeout_needs_check)
{
    ASSERT_FALSE(flipSuspend_nothingToDo(0, 0, true));
    ASSERT_FALSE(flipSuspend_nothingToDo(1, 1, true));
}

/* Lid close action vs Power "single press: Shutdown" (#228). */

TEST(lid_suspend_ignores_power_shutdown_setting)
{
    /* The #228 combination: lid = Suspend, Power = Shutdown. */
    ASSERT_TRUE(flipLid_onClose(FLIP_LID_SUSPEND, true, false) == FLIP_LID_SUSPEND);
    ASSERT_TRUE(flipLid_onClose(FLIP_LID_SUSPEND, false, false) == FLIP_LID_SUSPEND);
}

TEST(lid_action_matrix)
{
    const int actions[] = {FLIP_LID_SUSPEND, FLIP_LID_SHUTDOWN, FLIP_LID_NOTHING};
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(flipLid_onClose(actions[i], false, false) == (FlipLidAction)actions[i]);
        ASSERT_TRUE(flipLid_onClose(actions[i], true, false) == (FlipLidAction)actions[i]);
    }
}

TEST(lid_unknown_action_does_nothing)
{
    ASSERT_TRUE(flipLid_onClose(-1, false, false) == FLIP_LID_NOTHING);
    ASSERT_TRUE(flipLid_onClose(3, true, false) == FLIP_LID_NOTHING);
    ASSERT_TRUE(flipLid_onClose(99, false, false) == FLIP_LID_NOTHING);
}

/* Lid closed right after Power off (zazzagone, Mini Flip): the shutdown
 * already started must win, whatever the lid setting. */
TEST(lid_ignored_once_shutdown_started)
{
    const int actions[] = {FLIP_LID_SUSPEND, FLIP_LID_SHUTDOWN, FLIP_LID_NOTHING, 99};
    for (int i = 0; i < 4; i++) {
        ASSERT_TRUE(flipLid_onClose(actions[i], false, true) == FLIP_LID_NOTHING);
        ASSERT_TRUE(flipLid_onClose(actions[i], true, true) == FLIP_LID_NOTHING);
    }
}

int main(void)
{
    printf("\n=== flip_suspend.h Unit Tests ===\n\n");
    RUN_TEST(unchanged_lid_before_timeout_is_idle);
    RUN_TEST(lid_opened_needs_check);
    RUN_TEST(lid_closed_or_unreadable_change_needs_check);
    RUN_TEST(timeout_needs_check);
    RUN_TEST(lid_suspend_ignores_power_shutdown_setting);
    RUN_TEST(lid_action_matrix);
    RUN_TEST(lid_unknown_action_does_nothing);
    RUN_TEST(lid_ignored_once_shutdown_started);
    TEST_REPORT();
    return test_failures;
}
