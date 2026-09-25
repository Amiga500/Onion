/**
 * @file test_bmui_graph_clamp.c
 * @brief Unit tests for graph_clampPercent (batteryMonitorUI/graph_clamp.h)
 *
 * Production header. The row check mirrors the plot arithmetic in
 * batteryMonitorUI.c (row = 480 - y with y from battery_to_pixel) using the
 * constants from batteryMonitorUI.h, which cannot be included here (SDL).
 *
 * Build and run: make -f Makefile.unit test_bmui_graph_clamp
 */

#include "onion_test.h"
#include "../src/batteryMonitorUI/graph_clamp.h"
#include <limits.h>

/* SHADOW COPY of GRAPH_DISPLAY_SIZE_Y / GRAPH_DISPLAY_START_Y and
 * battery_to_pixel() from batteryMonitorUI.{h,c}. */
#define SHADOW_GRAPH_SIZE_Y 324
#define SHADOW_GRAPH_START_Y 79
#define SHADOW_SCREEN_ROWS 480

static int shadow_battery_to_pixel(int battery_perc)
{
    int y = (int)((SHADOW_GRAPH_SIZE_Y * battery_perc) / 100) + SHADOW_GRAPH_START_Y;
    if ((y < 0) || (y > 480))
        return -1;
    return y;
}

TEST(clamp_keeps_valid_range) {
    ASSERT_EQ(graph_clampPercent(0), 0);
    ASSERT_EQ(graph_clampPercent(1), 1);
    ASSERT_EQ(graph_clampPercent(57), 57);
    ASSERT_EQ(graph_clampPercent(100), 100);
}

TEST(clamp_rejects_negative) {
    ASSERT_EQ(graph_clampPercent(-1), 0);
    ASSERT_EQ(graph_clampPercent(-25), 0);
    ASSERT_EQ(graph_clampPercent(INT_MIN), 0);
}

TEST(clamp_rejects_above_100) {
    ASSERT_EQ(graph_clampPercent(101), 100);
    ASSERT_EQ(graph_clampPercent(500), 100); /* charging sentinel */
    ASSERT_EQ(graph_clampPercent(INT_MAX), 100);
}

TEST(unclamped_negative_percent_leaves_screen) {
    /* Documents the defect: -25 maps to -1, plotted at row 481. */
    int y = shadow_battery_to_pixel(-25);
    ASSERT_EQ(y, -1);
    ASSERT_GE(SHADOW_SCREEN_ROWS - y, SHADOW_SCREEN_ROWS);
}

TEST(clamped_percent_rows_stay_on_screen) {
    static const int inputs[] = {INT_MIN, -1000, -25, -1, 0, 50, 100, 101, 500, INT_MAX};
    for (unsigned i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        int y = shadow_battery_to_pixel(graph_clampPercent(inputs[i]));
        int row = SHADOW_SCREEN_ROWS - y;
        ASSERT_GE(y, 0);
        ASSERT_GE(row, 0);
        ASSERT_TRUE(row < SHADOW_SCREEN_ROWS);
    }
}

int main(void)
{
    printf("\n=== batteryMonitorUI graph_clamp Unit Tests ===\n\n");

    RUN_TEST(clamp_keeps_valid_range);
    RUN_TEST(clamp_rejects_negative);
    RUN_TEST(clamp_rejects_above_100);
    RUN_TEST(unclamped_negative_percent_leaves_screen);
    RUN_TEST(clamped_percent_rows_stay_on_screen);

    TEST_REPORT();
    return test_failures;
}
