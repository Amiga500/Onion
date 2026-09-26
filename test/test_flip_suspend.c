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

int main(void)
{
    printf("\n=== flip_suspend.h Unit Tests ===\n\n");
    RUN_TEST(unchanged_lid_before_timeout_is_idle);
    RUN_TEST(lid_opened_needs_check);
    RUN_TEST(lid_closed_or_unreadable_change_needs_check);
    RUN_TEST(timeout_needs_check);
    TEST_REPORT();
    return test_failures;
}
