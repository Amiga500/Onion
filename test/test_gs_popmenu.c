/**
 * @file test_gs_popmenu.c
 * @brief Unit tests for gs_popMenu.h bounds-check logic
 *
 * Tests the slot validation logic used in action_loadGame and
 * popMenu_deleteSaveState to ensure out-of-range selected_slot
 * values are properly rejected.
 *
 * Build and run: make -f Makefile.unit test_gs_popmenu
 */

#include "onion_test.h"
#include <stdbool.h>
#include <string.h>

/* Inline the SaveStateInfo_s struct from gs_popMenu.h */
typedef struct {
    int slots[10];
    int slot_count;
    int selected_slot;
} SaveStateInfo_s;

/**
 * Replicates the corrected bounds-check logic from action_loadGame:
 *   if (selected_slot < 0 || selected_slot >= slot_count) return true;
 * Returns true if slot is OUT OF RANGE (should be rejected).
 */
static bool slot_is_out_of_range(const SaveStateInfo_s *info)
{
    return info->selected_slot < 0 || info->selected_slot >= info->slot_count;
}

/* ---- Tests ---- */

TEST(negative_slot_rejected) {
    SaveStateInfo_s info = { .slot_count = 3, .selected_slot = -1 };
    ASSERT_TRUE(slot_is_out_of_range(&info));
}

TEST(slot_at_count_rejected) {
    SaveStateInfo_s info = { .slot_count = 3, .selected_slot = 3 };
    ASSERT_TRUE(slot_is_out_of_range(&info));
}

TEST(slot_above_count_rejected) {
    SaveStateInfo_s info = { .slot_count = 3, .selected_slot = 5 };
    ASSERT_TRUE(slot_is_out_of_range(&info));
}

TEST(slot_zero_with_empty_count_rejected) {
    SaveStateInfo_s info = { .slot_count = 0, .selected_slot = 0 };
    ASSERT_TRUE(slot_is_out_of_range(&info));
}

TEST(valid_slot_zero_accepted) {
    SaveStateInfo_s info = { .slot_count = 3, .selected_slot = 0 };
    ASSERT_FALSE(slot_is_out_of_range(&info));
}

TEST(valid_slot_mid_accepted) {
    SaveStateInfo_s info = { .slot_count = 5, .selected_slot = 2 };
    ASSERT_FALSE(slot_is_out_of_range(&info));
}

TEST(valid_slot_last_accepted) {
    SaveStateInfo_s info = { .slot_count = 5, .selected_slot = 4 };
    ASSERT_FALSE(slot_is_out_of_range(&info));
}

TEST(negative_slot_with_zero_count_rejected) {
    SaveStateInfo_s info = { .slot_count = 0, .selected_slot = -1 };
    ASSERT_TRUE(slot_is_out_of_range(&info));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== gs_popMenu.h Bounds-Check Tests ===\n\n");

    RUN_TEST(negative_slot_rejected);
    RUN_TEST(slot_at_count_rejected);
    RUN_TEST(slot_above_count_rejected);
    RUN_TEST(slot_zero_with_empty_count_rejected);
    RUN_TEST(valid_slot_zero_accepted);
    RUN_TEST(valid_slot_mid_accepted);
    RUN_TEST(valid_slot_last_accepted);
    RUN_TEST(negative_slot_with_zero_count_rejected);

    TEST_REPORT();
    return test_failures;
}
