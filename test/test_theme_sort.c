/**
 * @file test_theme_sort.c
 * @brief Unit tests for _comp_themes() from installTheme.h
 *
 * Tests the theme name comparison function used by qsort() to
 * sort theme directory names case-insensitively. The function
 * uses strcasecmp, which provides locale-independent alphabetical
 * ordering for ASCII strings.
 *
 * Build and run: make -f Makefile.unit test_theme_sort
 */

#include "onion_test.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define STR_MAX 256

/* ---- Inline the function under test ---- */

static int _comp_themes(const void *a, const void *b)
{
    return strcasecmp((const char *)a, (const char *)b);
}

/* ==== Tests: basic comparison ==== */

TEST(comp_themes_equal_strings) {
    char a[STR_MAX] = "Minimal";
    char b[STR_MAX] = "Minimal";
    ASSERT_EQ(_comp_themes(a, b), 0);
}

TEST(comp_themes_case_insensitive_equal) {
    char a[STR_MAX] = "minimal";
    char b[STR_MAX] = "MINIMAL";
    ASSERT_EQ(_comp_themes(a, b), 0);
}

TEST(comp_themes_a_before_b) {
    char a[STR_MAX] = "Alpha";
    char b[STR_MAX] = "Beta";
    ASSERT_TRUE(_comp_themes(a, b) < 0);
}

TEST(comp_themes_b_before_a) {
    char a[STR_MAX] = "Zeta";
    char b[STR_MAX] = "Alpha";
    ASSERT_TRUE(_comp_themes(a, b) > 0);
}

TEST(comp_themes_case_insensitive_ordering) {
    char a[STR_MAX] = "alpha";
    char b[STR_MAX] = "BETA";
    ASSERT_TRUE(_comp_themes(a, b) < 0);
}

/* ==== Tests: qsort integration ==== */

TEST(comp_themes_qsort_three_items) {
    char themes[3][STR_MAX];
    strncpy(themes[0], "Zebra", STR_MAX);
    strncpy(themes[1], "alpha", STR_MAX);
    strncpy(themes[2], "Beta", STR_MAX);

    qsort(themes, 3, STR_MAX, _comp_themes);

    ASSERT_STREQ(themes[0], "alpha");
    ASSERT_STREQ(themes[1], "Beta");
    ASSERT_STREQ(themes[2], "Zebra");
}

TEST(comp_themes_qsort_five_items) {
    char themes[5][STR_MAX];
    strncpy(themes[0], "GreenDark", STR_MAX);
    strncpy(themes[1], "Minimal", STR_MAX);
    strncpy(themes[2], "default", STR_MAX);
    strncpy(themes[3], "RetroClassic", STR_MAX);
    strncpy(themes[4], "BlueLight", STR_MAX);

    qsort(themes, 5, STR_MAX, _comp_themes);

    ASSERT_STREQ(themes[0], "BlueLight");
    ASSERT_STREQ(themes[1], "default");
    ASSERT_STREQ(themes[2], "GreenDark");
    ASSERT_STREQ(themes[3], "Minimal");
    ASSERT_STREQ(themes[4], "RetroClassic");
}

TEST(comp_themes_qsort_single_item) {
    char themes[1][STR_MAX];
    strncpy(themes[0], "OnlyTheme", STR_MAX);

    qsort(themes, 1, STR_MAX, _comp_themes);
    ASSERT_STREQ(themes[0], "OnlyTheme");
}

/* ==== Tests: edge cases ==== */

TEST(comp_themes_empty_strings) {
    char a[STR_MAX] = "";
    char b[STR_MAX] = "";
    ASSERT_EQ(_comp_themes(a, b), 0);
}

TEST(comp_themes_empty_vs_nonempty) {
    char a[STR_MAX] = "";
    char b[STR_MAX] = "something";
    ASSERT_TRUE(_comp_themes(a, b) < 0);
}

TEST(comp_themes_numeric_names) {
    char a[STR_MAX] = "Theme2";
    char b[STR_MAX] = "Theme10";
    /* strcasecmp is lexicographic: "2" > "1", so "Theme2" > "Theme10" */
    ASSERT_TRUE(_comp_themes(a, b) > 0);
}

TEST(comp_themes_special_chars) {
    char a[STR_MAX] = "_special";
    char b[STR_MAX] = "Normal";
    /* Underscore comes after uppercase letters in ASCII, but
       strcasecmp depends on locale; typically '_' < 'a' in C locale */
    int result = _comp_themes(a, b);
    /* Just verify it doesn't crash and returns consistent value */
    ASSERT_TRUE(result != 0);
}

TEST(comp_themes_with_spaces) {
    char a[STR_MAX] = "Dark Theme";
    char b[STR_MAX] = "dark theme";
    ASSERT_EQ(_comp_themes(a, b), 0);
}

TEST(comp_themes_prefix_relationship) {
    /* "Theme" is a prefix of "Theme Extra" → "Theme" < "Theme Extra" */
    char a[STR_MAX] = "Theme";
    char b[STR_MAX] = "Theme Extra";
    ASSERT_TRUE(_comp_themes(a, b) < 0);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== installTheme.h _comp_themes Unit Tests ===\n\n");

    /* Basic comparison */
    RUN_TEST(comp_themes_equal_strings);
    RUN_TEST(comp_themes_case_insensitive_equal);
    RUN_TEST(comp_themes_a_before_b);
    RUN_TEST(comp_themes_b_before_a);
    RUN_TEST(comp_themes_case_insensitive_ordering);

    /* qsort integration */
    RUN_TEST(comp_themes_qsort_three_items);
    RUN_TEST(comp_themes_qsort_five_items);
    RUN_TEST(comp_themes_qsort_single_item);

    /* Edge cases */
    RUN_TEST(comp_themes_empty_strings);
    RUN_TEST(comp_themes_empty_vs_nonempty);
    RUN_TEST(comp_themes_numeric_names);
    RUN_TEST(comp_themes_special_chars);
    RUN_TEST(comp_themes_with_spaces);
    RUN_TEST(comp_themes_prefix_relationship);

    TEST_REPORT();
    return test_failures;
}
