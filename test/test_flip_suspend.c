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

/* flipSuspend_step: the lid wakes a charging Flip (known issue in beta 8). */

static int charging_calls;
static bool charging_yes(void)
{
    charging_calls++;
    return true;
}
static bool charging_no(void)
{
    charging_calls++;
    return false;
}

TEST(lid_open_wakes_while_charging)
{
    int saved = 0;
    charging_calls = 0;
    ASSERT_TRUE(flipSuspend_step(1, &saved, false, charging_yes) == FLIP_SUSPEND_WAKE);
    ASSERT_TRUE(charging_calls == 0);
    saved = 0;
    ASSERT_TRUE(flipSuspend_step(1, &saved, true, charging_yes) == FLIP_SUSPEND_WAKE);
    ASSERT_TRUE(charging_calls == 0);
}

TEST(lid_open_wakes_off_charger)
{
    int saved = 0;
    ASSERT_TRUE(flipSuspend_step(1, &saved, false, charging_no) == FLIP_SUSPEND_WAKE);
}

TEST(idle_iteration_skips_charger_check)
{
    int saved = 0;
    charging_calls = 0;
    ASSERT_TRUE(flipSuspend_step(0, &saved, false, charging_yes) == FLIP_SUSPEND_STAY);
    ASSERT_TRUE(charging_calls == 0);
}

TEST(lid_change_while_charging_is_recorded)
{
    /* Closed again after an unreadable read: the saved state follows, so a
     * later opening is seen as one. */
    int saved = -1;
    charging_calls = 0;
    ASSERT_TRUE(flipSuspend_step(0, &saved, false, charging_yes) == FLIP_SUSPEND_STAY);
    ASSERT_TRUE(saved == 0);
    ASSERT_TRUE(charging_calls == 0);
    ASSERT_TRUE(flipSuspend_step(1, &saved, false, charging_yes) == FLIP_SUSPEND_WAKE);
}

TEST(timeout_powers_off_only_off_charger)
{
    int saved = 0;
    ASSERT_TRUE(flipSuspend_step(0, &saved, true, charging_yes) == FLIP_SUSPEND_STAY);
    ASSERT_TRUE(flipSuspend_step(0, &saved, true, charging_no) == FLIP_SUSPEND_POWER_OFF);
    saved = 1;
    ASSERT_TRUE(flipSuspend_step(1, &saved, true, charging_no) == FLIP_SUSPEND_POWER_OFF);
}

/* Lid close action vs Power "single press: Shutdown" (#228). */

TEST(lid_suspend_ignores_power_shutdown_setting)
{
    /* The #228 combination: lid = Suspend, Power = Shutdown. */
    ASSERT_TRUE(flipLid_onClose(FLIP_LID_SUSPEND, true) == FLIP_LID_SUSPEND);
    ASSERT_TRUE(flipLid_onClose(FLIP_LID_SUSPEND, false) == FLIP_LID_SUSPEND);
}

TEST(lid_action_matrix)
{
    const int actions[] = {FLIP_LID_SUSPEND, FLIP_LID_SHUTDOWN, FLIP_LID_NOTHING};
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(flipLid_onClose(actions[i], false) == (FlipLidAction)actions[i]);
        ASSERT_TRUE(flipLid_onClose(actions[i], true) == (FlipLidAction)actions[i]);
    }
}

TEST(lid_unknown_action_does_nothing)
{
    ASSERT_TRUE(flipLid_onClose(-1, false) == FLIP_LID_NOTHING);
    ASSERT_TRUE(flipLid_onClose(3, true) == FLIP_LID_NOTHING);
    ASSERT_TRUE(flipLid_onClose(99, false) == FLIP_LID_NOTHING);
}

int main(void)
{
    printf("\n=== flip_suspend.h Unit Tests ===\n\n");
    RUN_TEST(unchanged_lid_before_timeout_is_idle);
    RUN_TEST(lid_opened_needs_check);
    RUN_TEST(lid_closed_or_unreadable_change_needs_check);
    RUN_TEST(timeout_needs_check);
    RUN_TEST(lid_open_wakes_while_charging);
    RUN_TEST(lid_open_wakes_off_charger);
    RUN_TEST(idle_iteration_skips_charger_check);
    RUN_TEST(lid_change_while_charging_is_recorded);
    RUN_TEST(timeout_powers_off_only_off_charger);
    RUN_TEST(lid_suspend_ignores_power_shutdown_setting);
    RUN_TEST(lid_action_matrix);
    RUN_TEST(lid_unknown_action_does_nothing);
    TEST_REPORT();
    return test_failures;
}
