/**
 * @file test_gs_popmenu.c
 * @brief Unit tests for gs_popMenu save-state path and slot bounds
 *
 * createSaveStatePathFromNames is the production helper in gs_savestate_path.h
 * (included by gs_popMenu.h). Slot-bounds and .png skip checks remain local
 * copies of the surrounding control flow (SHADOW for those helpers only).
 *
 * Build and run: make -f Makefile.unit test_gs_popmenu
 */

#include "onion_test.h"
#include "../src/gameSwitcher/gs_savestate_path.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define STR_MAX 256

/* SHADOW COPY of SaveStateInfo_s / slot check from gs_popMenu.h (not the path builder). */
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

/**
 * SHADOW COPY: .png extension check from _hasSaveStates/_scanSaveStates.
 * Returns true if the slotStr should be SKIPPED (auto save or png preview).
 */
static bool should_skip_slot_str(const char *slotStr)
{
    size_t slotStrLen = strlen(slotStr);
    if (strncmp(slotStr, ".state.auto", 11) == 0 ||
        (slotStrLen >= 4 && strncmp(slotStr + slotStrLen - 4, ".png", 4) == 0)) {
        return true;
    }
    return false;
}

/* ---- Slot bounds-check tests ---- */

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

/* ---- createSaveStatePathFromNames (production gs_savestate_path.h) ---- */

TEST(path_auto_save) {
    char path[2048];
    ASSERT_TRUE(createSaveStatePathFromNames("gambatte", "tetris", -1, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/gambatte/tetris.state.auto");
}

TEST(path_slot_zero) {
    char path[2048];
    ASSERT_TRUE(createSaveStatePathFromNames("gambatte", "tetris", 0, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/gambatte/tetris.state");
}

TEST(path_slot_numbered) {
    char path[2048];
    ASSERT_TRUE(createSaveStatePathFromNames("gambatte", "tetris", 3, path, sizeof(path)));
    ASSERT_STREQ(path, STATES_DIR "/gambatte/tetris.state3");
}

/* Empty core_name is the _save_thread gate: false, out_path untouched,
 * caller must not poll the path or call retroarch_save. */
TEST(path_empty_core_name_rejected) {
    char path[64];
    memset(path, 0xAA, sizeof(path));
    ASSERT_FALSE(createSaveStatePathFromNames("", "tetris", 1, path, sizeof(path)));
    for (size_t i = 0; i < sizeof(path); i++) {
        ASSERT_EQ((unsigned char)path[i], 0xAAu);
    }
}

TEST(path_null_core_name_rejected) {
    char path[64];
    memset(path, 0xAA, sizeof(path));
    ASSERT_FALSE(createSaveStatePathFromNames(NULL, "tetris", 1, path, sizeof(path)));
    for (size_t i = 0; i < sizeof(path); i++) {
        ASSERT_EQ((unsigned char)path[i], 0xAAu);
    }
}

/* ---- .png extension check tests (buffer underflow prevention) ---- */

TEST(skip_auto_save_state) {
    ASSERT_TRUE(should_skip_slot_str(".state.auto"));
}

TEST(skip_png_preview) {
    ASSERT_TRUE(should_skip_slot_str(".state1.png"));
}

TEST(skip_png_state_zero_preview) {
    ASSERT_TRUE(should_skip_slot_str(".state.png"));
}

TEST(dont_skip_normal_state) {
    ASSERT_FALSE(should_skip_slot_str(".state"));
}

TEST(dont_skip_numbered_state) {
    ASSERT_FALSE(should_skip_slot_str(".state1"));
}

TEST(dont_skip_high_numbered_state) {
    ASSERT_FALSE(should_skip_slot_str(".state99"));
}

TEST(short_string_no_underflow) {
    /* This is the key test: ".st" is only 3 chars, which is shorter than 4.
       Before the fix, `strlen(slotStr) - 4` would underflow as unsigned. */
    ASSERT_FALSE(should_skip_slot_str(".st"));
}

TEST(empty_string_no_underflow) {
    ASSERT_FALSE(should_skip_slot_str(""));
}

TEST(single_char_no_underflow) {
    ASSERT_FALSE(should_skip_slot_str("x"));
}

TEST(exactly_four_chars_png) {
    ASSERT_TRUE(should_skip_slot_str(".png"));
}

TEST(exactly_four_chars_not_png) {
    ASSERT_FALSE(should_skip_slot_str(".abc"));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== gs_popMenu.h Unit Tests ===\n\n");

    /* Slot bounds checks */
    RUN_TEST(negative_slot_rejected);
    RUN_TEST(slot_at_count_rejected);
    RUN_TEST(slot_above_count_rejected);
    RUN_TEST(slot_zero_with_empty_count_rejected);
    RUN_TEST(valid_slot_zero_accepted);
    RUN_TEST(valid_slot_mid_accepted);
    RUN_TEST(valid_slot_last_accepted);
    RUN_TEST(negative_slot_with_zero_count_rejected);

    /* createSaveStatePath */
    RUN_TEST(path_auto_save);
    RUN_TEST(path_slot_zero);
    RUN_TEST(path_slot_numbered);
    RUN_TEST(path_empty_core_name_rejected);
    RUN_TEST(path_null_core_name_rejected);

    /* .png extension check (buffer underflow prevention) */
    RUN_TEST(skip_auto_save_state);
    RUN_TEST(skip_png_preview);
    RUN_TEST(skip_png_state_zero_preview);
    RUN_TEST(dont_skip_normal_state);
    RUN_TEST(dont_skip_numbered_state);
    RUN_TEST(dont_skip_high_numbered_state);
    RUN_TEST(short_string_no_underflow);
    RUN_TEST(empty_string_no_underflow);
    RUN_TEST(single_char_no_underflow);
    RUN_TEST(exactly_four_chars_png);
    RUN_TEST(exactly_four_chars_not_png);

    TEST_REPORT();
    return test_failures;
}
