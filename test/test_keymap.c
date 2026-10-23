/**
 * @file test_keymap.c
 * @brief Unit tests for keymap_hw.h hardware keymap constants.
 *
 * Verifies that all button mappings are unique (no accidental duplicates).
 *
 * Build and run: make -f Makefile.unit test_keymap
 */

#include "onion_test.h"
#include <stdbool.h>
#include <linux/input.h>

/* Production code: the hardware key codes from system/keymap_hw.h. */
#include "system/keymap_hw.h"

/* ==== Uniqueness tests: no two face/shoulder buttons share a code ==== */

TEST(hw_face_buttons_unique) {
    int codes[] = { HW_BTN_A, HW_BTN_B, HW_BTN_X, HW_BTN_Y };
    int n = sizeof(codes) / sizeof(codes[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            ASSERT_NE(codes[i], codes[j]);
        }
    }
}

TEST(hw_dpad_buttons_unique) {
    int codes[] = { HW_BTN_UP, HW_BTN_DOWN, HW_BTN_LEFT, HW_BTN_RIGHT };
    int n = sizeof(codes) / sizeof(codes[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            ASSERT_NE(codes[i], codes[j]);
        }
    }
}

TEST(hw_shoulder_buttons_unique) {
    int codes[] = { HW_BTN_L1, HW_BTN_R1, HW_BTN_L2, HW_BTN_R2 };
    int n = sizeof(codes) / sizeof(codes[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            ASSERT_NE(codes[i], codes[j]);
        }
    }
}

TEST(hw_system_buttons_unique) {
    int codes[] = { HW_BTN_SELECT, HW_BTN_START, HW_BTN_MENU, HW_BTN_POWER };
    int n = sizeof(codes) / sizeof(codes[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            ASSERT_NE(codes[i], codes[j]);
        }
    }
}

TEST(hw_volume_buttons_unique) {
    ASSERT_NE(HW_BTN_VOLUME_UP, HW_BTN_VOLUME_DOWN);
}

/* ==== Every key code is distinct, the lid switch included ==== */

TEST(hw_all_buttons_unique) {
    int codes[] = {HW_BTN_UP, HW_BTN_DOWN, HW_BTN_LEFT, HW_BTN_RIGHT,
                   HW_BTN_A, HW_BTN_B, HW_BTN_X, HW_BTN_Y,
                   HW_BTN_L1, HW_BTN_R1, HW_BTN_L2, HW_BTN_R2,
                   HW_BTN_SELECT, HW_BTN_START, HW_BTN_MENU, HW_BTN_POWER,
                   HW_BTN_VOLUME_UP, HW_BTN_VOLUME_DOWN, HW_BTN_LID_CLOSE};
    int n = sizeof(codes) / sizeof(codes[0]);
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            ASSERT_NE(codes[i], codes[j]);
        }
    }
}

/* ==== Physical layout: a swap here passes every uniqueness test ==== */

TEST(hw_buttons_match_device_layout) {
    ASSERT_EQ(HW_BTN_A, KEY_SPACE);
    ASSERT_EQ(HW_BTN_B, KEY_LEFTCTRL);
    ASSERT_EQ(HW_BTN_X, KEY_LEFTSHIFT);
    ASSERT_EQ(HW_BTN_Y, KEY_LEFTALT);
    ASSERT_EQ(HW_BTN_L1, KEY_E);
    ASSERT_EQ(HW_BTN_R1, KEY_T);
    ASSERT_EQ(HW_BTN_L2, KEY_TAB);
    ASSERT_EQ(HW_BTN_R2, KEY_BACKSPACE);
    ASSERT_EQ(HW_BTN_SELECT, KEY_RIGHTCTRL);
    ASSERT_EQ(HW_BTN_START, KEY_ENTER);
    ASSERT_EQ(HW_BTN_MENU, KEY_ESC);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== keymap_hw.h Unit Tests ===\n\n");

    RUN_TEST(hw_face_buttons_unique);
    RUN_TEST(hw_dpad_buttons_unique);
    RUN_TEST(hw_shoulder_buttons_unique);
    RUN_TEST(hw_system_buttons_unique);
    RUN_TEST(hw_volume_buttons_unique);
    RUN_TEST(hw_all_buttons_unique);
    RUN_TEST(hw_buttons_match_device_layout);

    TEST_REPORT();
    return test_failures;
}
