/**
 * @file test_textbox_fit.c
 * @brief Font size a message is shrunk to so it fits the screen
 *
 * Runs textbox_fit_size from theme/render/textbox_fit.h, used by
 * theme_textboxSurfaceFit for prompt, infoPanel and dialogs: a message that
 * is wider or taller than its space gets a smaller font instead of being cut
 * off by the screen edge (robcodedev/onionos-mainui-opensource#21).
 *
 * Build and run: make -f Makefile.unit test_textbox_fit
 */

#include "onion_test.h"

#include "theme/render/textbox_fit.h"

TEST(fitting_text_keeps_its_size)
{
    ASSERT_EQ(textbox_fit_size(30, 600, 100, 600, 280, 14), 30);
    ASSERT_EQ(textbox_fit_size(30, 10, 10, 600, 280, 14), 30);
}

TEST(too_wide_shrinks_in_proportion)
{
    /* 800 px wide in 600: 30 * 600/800 = 22.5 -> 22. */
    ASSERT_EQ(textbox_fit_size(30, 800, 100, 600, 280, 14), 22);
}

TEST(too_tall_shrinks_in_proportion)
{
    /* 400 px tall in 280: 30 * 0.7 = 21. */
    ASSERT_EQ(textbox_fit_size(30, 500, 400, 600, 280, 14), 21);
}

TEST(both_too_big_uses_the_tighter_side)
{
    /* width 0.5, height 0.7: the width wins. */
    ASSERT_EQ(textbox_fit_size(30, 1200, 400, 600, 280, 14), 15);
}

TEST(always_at_least_one_point_smaller)
{
    /* 601 in 600 rounds back to 30: take 29 so the loop makes progress. */
    ASSERT_EQ(textbox_fit_size(30, 601, 100, 600, 280, 14), 29);
}

TEST(never_below_the_minimum)
{
    ASSERT_EQ(textbox_fit_size(30, 6000, 100, 600, 280, 14), 14);
    ASSERT_EQ(textbox_fit_size(14, 6000, 100, 600, 280, 14), 14);
    ASSERT_EQ(textbox_fit_size(12, 6000, 100, 600, 280, 14), 12);
}

TEST(shrinking_converges_in_a_few_steps)
{
    /* The loop in theme_textboxSurfaceFit re-measures after each step; text
     * width follows the font size, so it reaches a fitting size quickly. */
    int size = 40, steps = 0;
    for (; steps < 8; steps++) {
        int w = 20 * size, h = 4 * size; /* text measured at this size */
        int next = textbox_fit_size(size, w, h, 600, 280, 14);
        if (next == size)
            break;
        size = next;
    }
    ASSERT_TRUE(steps < 8);
    ASSERT_TRUE(20 * size <= 600);
}

TEST(bad_measurements_change_nothing)
{
    ASSERT_EQ(textbox_fit_size(30, 0, 0, 600, 280, 14), 30);
    ASSERT_EQ(textbox_fit_size(30, -5, 50, 600, 280, 14), 30);
}

int main(void)
{
    RUN_TEST(fitting_text_keeps_its_size);
    RUN_TEST(too_wide_shrinks_in_proportion);
    RUN_TEST(too_tall_shrinks_in_proportion);
    RUN_TEST(both_too_big_uses_the_tighter_side);
    RUN_TEST(always_at_least_one_point_smaller);
    RUN_TEST(never_below_the_minimum);
    RUN_TEST(shrinking_converges_in_a_few_steps);
    RUN_TEST(bad_measurements_change_nothing);
    TEST_REPORT();
    return test_failures;
}
