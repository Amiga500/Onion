/**
 * @file test_gs_frame.c
 * @brief Tests for src/gameSwitcher/gs_frame.h (production header)
 *
 * The GameSwitcher redraws the whole screen only when its state changed,
 * the name bar alone while a visible name scrolls, and nothing otherwise.
 *
 * Build and run: make -f Makefile.unit test_gs_frame
 */

#include "../src/gameSwitcher/gs_frame.h"
#include "onion_test.h"

TEST(changed_is_full_frame)
{
    ASSERT_EQ(GS_FRAME_FULL, gs_frameKind(true, false, false));
    ASSERT_EQ(GS_FRAME_FULL, gs_frameKind(true, true, true));
    ASSERT_EQ(GS_FRAME_FULL, gs_frameKind(true, false, true));
}

TEST(visible_scrolling_name_is_name_only)
{
    ASSERT_EQ(GS_FRAME_NAME_ONLY, gs_frameKind(false, true, true));
}

/* Full-screen view or pop menu open: the name is not drawn, so a long
 * name must not cost a frame. */
TEST(hidden_scrolling_name_is_skipped)
{
    ASSERT_EQ(GS_FRAME_SKIP, gs_frameKind(false, false, true));
}

TEST(idle_is_skipped)
{
    ASSERT_EQ(GS_FRAME_SKIP, gs_frameKind(false, true, false));
    ASSERT_EQ(GS_FRAME_SKIP, gs_frameKind(false, false, false));
}

int main(void)
{
    printf("\n=== gs_frame.h Unit Tests ===\n\n");
    RUN_TEST(changed_is_full_frame);
    RUN_TEST(visible_scrolling_name_is_name_only);
    RUN_TEST(hidden_scrolling_name_is_skipped);
    RUN_TEST(idle_is_skipped);
    TEST_REPORT();
    return test_failures;
}
