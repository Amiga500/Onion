/**
 * @file test_overlay_content.c
 * @brief Unit tests for _isContentNameInInfo() from gs_overlay.h
 *
 * Tests the pure string matching function used by the game switcher
 * overlay to detect whether a RetroArch content name appears in the
 * status content_info field. The function checks that the match is
 * bounded by commas: ",<name>,".
 *
 * Build and run: make -f Makefile.unit test_overlay_content
 */

#include "onion_test.h"
#include <stdbool.h>
#include <string.h>

/* ---- Inline the function under test from gs_overlay.h ---- */

static bool _isContentNameInInfo(const char *content_info, const char *content_name)
{
    const char *found = strstr(content_info, content_name);
    if (found != NULL) {
        return found > content_info && *(found - 1) == ',' && *(found + strlen(content_name)) == ',';
    }
    return false;
}

/* ==== Tests: basic matching ==== */

TEST(content_name_found_in_middle) {
    /* Typical RetroArch content_info format: ",name1,name2,name3," */
    ASSERT_TRUE(_isContentNameInInfo(",game1,Super Mario,game3,", "Super Mario"));
}

TEST(content_name_found_at_start_after_comma) {
    ASSERT_TRUE(_isContentNameInInfo(",Pokemon Red,other,", "Pokemon Red"));
}

TEST(content_name_found_at_end_before_comma) {
    ASSERT_TRUE(_isContentNameInInfo(",other,Zelda,", "Zelda"));
}

TEST(content_name_single_entry) {
    ASSERT_TRUE(_isContentNameInInfo(",MyGame,", "MyGame"));
}

/* ==== Tests: non-matching cases ==== */

TEST(content_name_not_found) {
    ASSERT_FALSE(_isContentNameInInfo(",game1,game2,game3,", "NotHere"));
}

TEST(content_name_substring_not_matched) {
    /* "Mario" is a substring of "Super Mario" but not comma-bounded */
    ASSERT_FALSE(_isContentNameInInfo(",Super Mario,", "Mario"));
}

TEST(content_name_prefix_not_matched) {
    ASSERT_FALSE(_isContentNameInInfo(",Super Mario,", "Super"));
}

TEST(content_name_no_leading_comma) {
    /* If the match is at position 0 (no preceding comma), it should fail */
    ASSERT_FALSE(_isContentNameInInfo("game1,game2,", "game1"));
}

TEST(content_name_no_trailing_comma) {
    /* If the match has no trailing comma, it should fail */
    ASSERT_FALSE(_isContentNameInInfo(",game1", "game1"));
}

TEST(content_name_empty_info) {
    ASSERT_FALSE(_isContentNameInInfo("", "anything"));
}

TEST(content_name_empty_name) {
    /* Empty content_name: strstr finds "" at every position, but
       position 0 has no preceding char so it should fail */
    ASSERT_FALSE(_isContentNameInInfo(",game,", ""));
}

/* ==== Tests: edge cases with similar names ==== */

TEST(content_name_exact_match_not_partial) {
    ASSERT_TRUE(_isContentNameInInfo(",Sonic,Sonic 2,Sonic 3,", "Sonic"));
    ASSERT_TRUE(_isContentNameInInfo(",Sonic,Sonic 2,Sonic 3,", "Sonic 2"));
    ASSERT_TRUE(_isContentNameInInfo(",Sonic,Sonic 2,Sonic 3,", "Sonic 3"));
}

TEST(content_name_with_special_chars) {
    ASSERT_TRUE(_isContentNameInInfo(",Game (USA),", "Game (USA)"));
    ASSERT_TRUE(_isContentNameInInfo(",Game [BIOS],", "Game [BIOS]"));
}

TEST(content_name_with_dots) {
    ASSERT_TRUE(_isContentNameInInfo(",game.v1.2,", "game.v1.2"));
}

TEST(content_name_duplicate_entries) {
    /* First occurrence at position 0 has no leading comma — strstr finds
       it first and the function returns false without searching further.
       This is the actual behavior of the production code. */
    ASSERT_FALSE(_isContentNameInInfo("game,game,", "game"));
    /* If properly bounded, both occurrences work */
    ASSERT_TRUE(_isContentNameInInfo(",game,game,", "game"));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== gs_overlay.h _isContentNameInInfo Unit Tests ===\n\n");

    /* Basic matching */
    RUN_TEST(content_name_found_in_middle);
    RUN_TEST(content_name_found_at_start_after_comma);
    RUN_TEST(content_name_found_at_end_before_comma);
    RUN_TEST(content_name_single_entry);

    /* Non-matching */
    RUN_TEST(content_name_not_found);
    RUN_TEST(content_name_substring_not_matched);
    RUN_TEST(content_name_prefix_not_matched);
    RUN_TEST(content_name_no_leading_comma);
    RUN_TEST(content_name_no_trailing_comma);
    RUN_TEST(content_name_empty_info);
    RUN_TEST(content_name_empty_name);

    /* Edge cases */
    RUN_TEST(content_name_exact_match_not_partial);
    RUN_TEST(content_name_with_special_chars);
    RUN_TEST(content_name_with_dots);
    RUN_TEST(content_name_duplicate_entries);

    TEST_REPORT();
    return test_failures;
}
