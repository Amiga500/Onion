/**
 * @file test_lcd_colors.c
 * @brief Tests for src/lcdColors/lcd_colors.h (production header)
 *
 * The Mini Flip LCD gets the Settings › Display colours at boot (lcdColors,
 * started by runtime.sh) with the mapping stock MainUI uses.
 *
 * Build and run: make -f Makefile.unit test_lcd_colors
 */

#include "../src/lcdColors/lcd_colors.h"
#include "onion_test.h"

TEST(parse_accepts_0_to_20)
{
    ASSERT_EQ(lcdColors_parse("0", 7), 0);
    ASSERT_EQ(lcdColors_parse("13", 7), 13);
    ASSERT_EQ(lcdColors_parse("20", 7), 20);
}

TEST(parse_falls_back_on_bad_values)
{
    ASSERT_EQ(lcdColors_parse("21", 7), 7);
    ASSERT_EQ(lcdColors_parse("-1", 10), 10);
    ASSERT_EQ(lcdColors_parse("", 10), 10);
    ASSERT_EQ(lcdColors_parse(NULL, 10), 10);
    ASSERT_EQ(lcdColors_parse("5x", 10), 10);
    ASSERT_EQ(lcdColors_parse("null", 10), 10);
}

TEST(fill_uses_stock_mapping)
{
    uint32_t p[LCD_PARAM_WORDS] = {3, 0, 0, 0, 0, 9};
    const int values[LCD_COLORS_COUNT] = {3, 15, 18, 5}; // L, H, S, C
    lcdColors_fill(p, values);
    ASSERT_EQ((int)p[1], 38); // luma: L + 35
    ASSERT_EQ((int)p[2], 40); // contrast: (C + 15) * 2
    ASSERT_EQ((int)p[3], 75); // hue: H * 5
    ASSERT_EQ((int)p[4], 90); // saturation: S * 5
}

TEST(fill_keeps_matrix_and_sharpness)
{
    uint32_t p[LCD_PARAM_WORDS] = {3, 0, 0, 0, 0, 9};
    lcdColors_fill(p, lcdColors_defaults);
    ASSERT_EQ((int)p[0], 3);
    ASSERT_EQ((int)p[5], 9);
    ASSERT_EQ((int)p[1], 42); // defaults 7/10/10/10
    ASSERT_EQ((int)p[2], 50);
    ASSERT_EQ((int)p[3], 50);
    ASSERT_EQ((int)p[4], 50);
}

int main(void)
{
    printf("\n=== lcd_colors.h Unit Tests ===\n\n");
    RUN_TEST(parse_accepts_0_to_20);
    RUN_TEST(parse_falls_back_on_bad_values);
    RUN_TEST(fill_uses_stock_mapping);
    RUN_TEST(fill_keeps_matrix_and_sharpness);
    TEST_REPORT();
    return test_failures;
}
