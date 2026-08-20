/**
 * @file test_keymap.c
 * @brief Unit tests for keymap_hw.h hardware keymap constants.
 *
 * Verifies the hardware key constant definitions match their
 * expected Linux input.h values, and that all button mappings
 * are unique (no accidental duplicates).
 *
 * Build and run: make -f Makefile.unit test_keymap
 */

#include "onion_test.h"
#include <stdbool.h>
#include <linux/input.h>

/* ---- Inline constants from keymap_hw.h ---- */

#define HW_BTN_UP       KEY_UP
#define HW_BTN_DOWN     KEY_DOWN
#define HW_BTN_LEFT     KEY_LEFT
#define HW_BTN_RIGHT    KEY_RIGHT
#define HW_BTN_A        KEY_SPACE
#define HW_BTN_B        KEY_LEFTCTRL
#define HW_BTN_X        KEY_LEFTSHIFT
#define HW_BTN_Y        KEY_LEFTALT
#define HW_BTN_L1       KEY_E
#define HW_BTN_R1       KEY_T
#define HW_BTN_L2       KEY_TAB
#define HW_BTN_R2       KEY_BACKSPACE
#define HW_BTN_SELECT   KEY_RIGHTCTRL
#define HW_BTN_START    KEY_ENTER
#define HW_BTN_MENU     KEY_ESC
#define HW_BTN_POWER    KEY_POWER
#define HW_BTN_VOLUME_UP   KEY_VOLUMEUP
#define HW_BTN_VOLUME_DOWN KEY_VOLUMEDOWN

/* ==== Individual constant value tests ==== */

TEST(hw_btn_up_value) {
    ASSERT_EQ(HW_BTN_UP, KEY_UP);
}

TEST(hw_btn_down_value) {
    ASSERT_EQ(HW_BTN_DOWN, KEY_DOWN);
}

TEST(hw_btn_left_value) {
    ASSERT_EQ(HW_BTN_LEFT, KEY_LEFT);
}

TEST(hw_btn_right_value) {
    ASSERT_EQ(HW_BTN_RIGHT, KEY_RIGHT);
}

TEST(hw_btn_a_value) {
    ASSERT_EQ(HW_BTN_A, KEY_SPACE);
}

TEST(hw_btn_b_value) {
    ASSERT_EQ(HW_BTN_B, KEY_LEFTCTRL);
}

TEST(hw_btn_x_value) {
    ASSERT_EQ(HW_BTN_X, KEY_LEFTSHIFT);
}

TEST(hw_btn_y_value) {
    ASSERT_EQ(HW_BTN_Y, KEY_LEFTALT);
}

TEST(hw_btn_l1_value) {
    ASSERT_EQ(HW_BTN_L1, KEY_E);
}

TEST(hw_btn_r1_value) {
    ASSERT_EQ(HW_BTN_R1, KEY_T);
}

TEST(hw_btn_l2_value) {
    ASSERT_EQ(HW_BTN_L2, KEY_TAB);
}

TEST(hw_btn_r2_value) {
    ASSERT_EQ(HW_BTN_R2, KEY_BACKSPACE);
}

TEST(hw_btn_select_value) {
    ASSERT_EQ(HW_BTN_SELECT, KEY_RIGHTCTRL);
}

TEST(hw_btn_start_value) {
    ASSERT_EQ(HW_BTN_START, KEY_ENTER);
}

TEST(hw_btn_menu_value) {
    ASSERT_EQ(HW_BTN_MENU, KEY_ESC);
}

TEST(hw_btn_power_value) {
    ASSERT_EQ(HW_BTN_POWER, KEY_POWER);
}

TEST(hw_btn_volume_up_value) {
    ASSERT_EQ(HW_BTN_VOLUME_UP, KEY_VOLUMEUP);
}

TEST(hw_btn_volume_down_value) {
    ASSERT_EQ(HW_BTN_VOLUME_DOWN, KEY_VOLUMEDOWN);
}

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

/* ==== All buttons are non-negative (valid keycodes) ==== */

TEST(hw_all_buttons_nonnegative) {
    ASSERT_GE(HW_BTN_UP, 0);
    ASSERT_GE(HW_BTN_DOWN, 0);
    ASSERT_GE(HW_BTN_LEFT, 0);
    ASSERT_GE(HW_BTN_RIGHT, 0);
    ASSERT_GE(HW_BTN_A, 0);
    ASSERT_GE(HW_BTN_B, 0);
    ASSERT_GE(HW_BTN_X, 0);
    ASSERT_GE(HW_BTN_Y, 0);
    ASSERT_GE(HW_BTN_L1, 0);
    ASSERT_GE(HW_BTN_R1, 0);
    ASSERT_GE(HW_BTN_L2, 0);
    ASSERT_GE(HW_BTN_R2, 0);
    ASSERT_GE(HW_BTN_SELECT, 0);
    ASSERT_GE(HW_BTN_START, 0);
    ASSERT_GE(HW_BTN_MENU, 0);
    ASSERT_GE(HW_BTN_POWER, 0);
    ASSERT_GE(HW_BTN_VOLUME_UP, 0);
    ASSERT_GE(HW_BTN_VOLUME_DOWN, 0);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== keymap_hw.h Unit Tests ===\n\n");

    RUN_TEST(hw_btn_up_value);
    RUN_TEST(hw_btn_down_value);
    RUN_TEST(hw_btn_left_value);
    RUN_TEST(hw_btn_right_value);
    RUN_TEST(hw_btn_a_value);
    RUN_TEST(hw_btn_b_value);
    RUN_TEST(hw_btn_x_value);
    RUN_TEST(hw_btn_y_value);
    RUN_TEST(hw_btn_l1_value);
    RUN_TEST(hw_btn_r1_value);
    RUN_TEST(hw_btn_l2_value);
    RUN_TEST(hw_btn_r2_value);
    RUN_TEST(hw_btn_select_value);
    RUN_TEST(hw_btn_start_value);
    RUN_TEST(hw_btn_menu_value);
    RUN_TEST(hw_btn_power_value);
    RUN_TEST(hw_btn_volume_up_value);
    RUN_TEST(hw_btn_volume_down_value);

    RUN_TEST(hw_face_buttons_unique);
    RUN_TEST(hw_dpad_buttons_unique);
    RUN_TEST(hw_shoulder_buttons_unique);
    RUN_TEST(hw_system_buttons_unique);
    RUN_TEST(hw_volume_buttons_unique);

    RUN_TEST(hw_all_buttons_nonnegative);

    TEST_REPORT();
    return test_failures;
}
