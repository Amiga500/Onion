/**
 * @file test_romscreen_window.c
 * @brief Tests for src/gameSwitcher/gs_romscreen_window.h (production header)
 *
 * The UI thread frees romscreens outside a +/-5 window around the current
 * entry. The overlay's autosave thread reads game_list[0].romScreen while
 * encoding it to PNG, so a pinned entry must never be evicted.
 *
 * Build and run: make -f Makefile.unit test_romscreen_window
 */

#include "onion_test.h"
#include "../src/gameSwitcher/gs_romscreen_window.h"

#define WINDOW 5

TEST(inside_window_is_kept) {
    for (int i = 0; i <= 10; i++)
        ASSERT_FALSE(romscreen_shouldEvict(i, 5, WINDOW, -1));
}

TEST(outside_window_is_evicted) {
    ASSERT_TRUE(romscreen_shouldEvict(0, 6, WINDOW, -1));
    ASSERT_TRUE(romscreen_shouldEvict(12, 6, WINDOW, -1));
    ASSERT_FALSE(romscreen_shouldEvict(1, 6, WINDOW, -1));
    ASSERT_FALSE(romscreen_shouldEvict(11, 6, WINDOW, -1));
}

/* The use-after-free case: user scrolled to entry 6+ while entry 0 is
 * being encoded by the autosave thread. */
TEST(pinned_entry_is_never_evicted) {
    ASSERT_FALSE(romscreen_shouldEvict(0, 6, WINDOW, 0));
    ASSERT_FALSE(romscreen_shouldEvict(0, 40, WINDOW, 0));
    /* Other entries are unaffected by the pin. */
    ASSERT_TRUE(romscreen_shouldEvict(1, 40, WINDOW, 0));
}

int main(void)
{
    printf("\n=== gs_romscreen_window.h Unit Tests ===\n\n");
    RUN_TEST(inside_window_is_kept);
    RUN_TEST(outside_window_is_evicted);
    RUN_TEST(pinned_entry_is_never_evicted);
    TEST_REPORT();
    return test_failures;
}
