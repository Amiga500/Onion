/**
 * @file test_resources_enum.c
 * @brief Unit tests for theme/resources.h enum definitions and mapping logic.
 *
 * Tests ThemeImages and ThemeFonts enum values, the resource_getPopMenuBg()
 * and resource_getBrightness() switch-case mapping logic (inlined without
 * SDL dependencies), and HIDDEN_ITEM_ALPHA constant.
 *
 * Build and run: make -f Makefile.unit test_resources_enum
 */

#include "onion_test.h"
#include <stdbool.h>

/* Production code: the theme image/font ids and the pop menu and
 * brightness mappings from theme/resource_ids.h (used by
 * resource_getPopMenuBg() and resource_getBrightness()). */
#include "theme/resource_ids.h"

#define popMenuBgImage resource_popMenuBgId
#define brightnessImage resource_brightnessId

/* ==== ThemeImages enum ordering tests ==== */

TEST(null_image_is_zero) {
    ASSERT_EQ(NULL_IMAGE, 0);
}

TEST(images_count_value) {
    /* images_count should be the total number of enum members */
    ASSERT_GT(images_count, 40);
}

TEST(battery_images_are_contiguous) {
    ASSERT_EQ(BATTERY_20, BATTERY_0 + 1);
    ASSERT_EQ(BATTERY_50, BATTERY_0 + 2);
    ASSERT_EQ(BATTERY_80, BATTERY_0 + 3);
    ASSERT_EQ(BATTERY_100, BATTERY_0 + 4);
    ASSERT_EQ(BATTERY_CHARGING, BATTERY_0 + 5);
}

TEST(brightness_images_are_contiguous) {
    ASSERT_EQ(BRIGHTNESS_1, BRIGHTNESS_0 + 1);
    ASSERT_EQ(BRIGHTNESS_2, BRIGHTNESS_0 + 2);
    ASSERT_EQ(BRIGHTNESS_3, BRIGHTNESS_0 + 3);
    ASSERT_EQ(BRIGHTNESS_4, BRIGHTNESS_0 + 4);
    ASSERT_EQ(BRIGHTNESS_5, BRIGHTNESS_0 + 5);
    ASSERT_EQ(BRIGHTNESS_6, BRIGHTNESS_0 + 6);
    ASSERT_EQ(BRIGHTNESS_7, BRIGHTNESS_0 + 7);
    ASSERT_EQ(BRIGHTNESS_8, BRIGHTNESS_0 + 8);
    ASSERT_EQ(BRIGHTNESS_9, BRIGHTNESS_0 + 9);
    ASSERT_EQ(BRIGHTNESS_10, BRIGHTNESS_0 + 10);
}

TEST(pop_menu_images_are_contiguous) {
    ASSERT_EQ(BG_POP_MENU_2, BG_POP_MENU_1 + 1);
    ASSERT_EQ(BG_POP_MENU_3, BG_POP_MENU_1 + 2);
    ASSERT_EQ(BG_POP_MENU_4, BG_POP_MENU_1 + 3);
}

/* ==== ThemeFonts enum tests ==== */

TEST(null_font_is_zero) {
    ASSERT_EQ(NULL_FONT, 0);
}

TEST(fonts_count_value) {
    ASSERT_EQ(fonts_count, 7);
}

TEST(font_enum_ordering) {
    ASSERT_EQ(TITLE, 1);
    ASSERT_EQ(HINT, 2);
    ASSERT_EQ(GRID1x4, 3);
    ASSERT_EQ(GRID3x4, 4);
    ASSERT_EQ(LIST, 5);
    ASSERT_EQ(BATTERY, 6);
}

/* ==== HIDDEN_ITEM_ALPHA ==== */

TEST(hidden_alpha_value) {
    ASSERT_EQ(HIDDEN_ITEM_ALPHA, 60);
}

TEST(hidden_alpha_in_range) {
    ASSERT_GE(HIDDEN_ITEM_ALPHA, 0);
    ASSERT_TRUE(HIDDEN_ITEM_ALPHA <= 255);
}

/* ==== popMenuBg mapping tests ==== */

TEST(pop_menu_bg_size_1) {
    ASSERT_EQ(popMenuBgImage(1), BG_POP_MENU_1);
}

TEST(pop_menu_bg_size_2) {
    ASSERT_EQ(popMenuBgImage(2), BG_POP_MENU_2);
}

TEST(pop_menu_bg_size_3) {
    ASSERT_EQ(popMenuBgImage(3), BG_POP_MENU_3);
}

TEST(pop_menu_bg_size_4) {
    ASSERT_EQ(popMenuBgImage(4), BG_POP_MENU_4);
}

TEST(pop_menu_bg_size_0_returns_null) {
    ASSERT_EQ(popMenuBgImage(0), NULL_IMAGE);
}

TEST(pop_menu_bg_size_5_returns_null) {
    ASSERT_EQ(popMenuBgImage(5), NULL_IMAGE);
}

TEST(pop_menu_bg_negative_returns_null) {
    ASSERT_EQ(popMenuBgImage(-1), NULL_IMAGE);
}

/* ==== brightness mapping tests ==== */

TEST(brightness_0_maps_correctly) {
    ASSERT_EQ(brightnessImage(0), BRIGHTNESS_0);
}

TEST(brightness_5_maps_correctly) {
    ASSERT_EQ(brightnessImage(5), BRIGHTNESS_5);
}

TEST(brightness_10_maps_correctly) {
    ASSERT_EQ(brightnessImage(10), BRIGHTNESS_10);
}

TEST(brightness_all_levels) {
    for (int i = 0; i <= 10; i++) {
        ASSERT_EQ((int)brightnessImage(i), (int)(BRIGHTNESS_0 + i));
    }
}

TEST(brightness_11_returns_null) {
    ASSERT_EQ(brightnessImage(11), NULL_IMAGE);
}

TEST(brightness_negative_returns_null) {
    ASSERT_EQ(brightnessImage(-1), NULL_IMAGE);
}

TEST(brightness_large_returns_null) {
    ASSERT_EQ(brightnessImage(100), NULL_IMAGE);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== resources.h Enum & Mapping Unit Tests ===\n\n");

    /* ThemeImages enum */
    RUN_TEST(null_image_is_zero);
    RUN_TEST(images_count_value);
    RUN_TEST(battery_images_are_contiguous);
    RUN_TEST(brightness_images_are_contiguous);
    RUN_TEST(pop_menu_images_are_contiguous);

    /* ThemeFonts enum */
    RUN_TEST(null_font_is_zero);
    RUN_TEST(fonts_count_value);
    RUN_TEST(font_enum_ordering);

    /* HIDDEN_ITEM_ALPHA */
    RUN_TEST(hidden_alpha_value);
    RUN_TEST(hidden_alpha_in_range);

    /* popMenuBg mapping */
    RUN_TEST(pop_menu_bg_size_1);
    RUN_TEST(pop_menu_bg_size_2);
    RUN_TEST(pop_menu_bg_size_3);
    RUN_TEST(pop_menu_bg_size_4);
    RUN_TEST(pop_menu_bg_size_0_returns_null);
    RUN_TEST(pop_menu_bg_size_5_returns_null);
    RUN_TEST(pop_menu_bg_negative_returns_null);

    /* brightness mapping */
    RUN_TEST(brightness_0_maps_correctly);
    RUN_TEST(brightness_5_maps_correctly);
    RUN_TEST(brightness_10_maps_correctly);
    RUN_TEST(brightness_all_levels);
    RUN_TEST(brightness_11_returns_null);
    RUN_TEST(brightness_negative_returns_null);
    RUN_TEST(brightness_large_returns_null);

    TEST_REPORT();
    return test_failures;
}
