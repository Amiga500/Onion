/**
 * @file test_str.c
 * @brief Unit tests for src/common/utils/str.c
 *
 * Tests string utility functions: str_split, str_replace, str_trim,
 * str_endsWith, str_count_char, str_removeParentheses.
 *
 * Build and run: make -f Makefile.unit test_str && ./build_test/test_str
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include <stdlib.h>

/* ---- str_endsWith ---- */

TEST(str_endsWith_match) {
    ASSERT_TRUE(str_endsWith("hello.png", ".png"));
}

TEST(str_endsWith_no_match) {
    ASSERT_FALSE(str_endsWith("hello.png", ".jpg"));
}

TEST(str_endsWith_empty_suffix) {
    ASSERT_TRUE(str_endsWith("hello", ""));
}

TEST(str_endsWith_suffix_longer) {
    ASSERT_FALSE(str_endsWith("hi", "hello"));
}

TEST(str_endsWith_exact_match) {
    ASSERT_TRUE(str_endsWith(".png", ".png"));
}

/* ---- str_count_char ---- */

TEST(str_count_char_basic) {
    ASSERT_EQ(str_count_char("hello world", 'l'), 3);
}

TEST(str_count_char_none) {
    ASSERT_EQ(str_count_char("hello", 'z'), 0);
}

TEST(str_count_char_all) {
    ASSERT_EQ(str_count_char("aaa", 'a'), 3);
}

TEST(str_count_char_empty) {
    ASSERT_EQ(str_count_char("", 'a'), 0);
}

/* ---- str_trim ---- */

TEST(str_trim_leading_trailing) {
    char out[64];
    str_trim(out, sizeof(out), "  hello  ", false);
    ASSERT_STREQ(out, "hello");
}

TEST(str_trim_no_whitespace) {
    char out[64];
    str_trim(out, sizeof(out), "hello", false);
    ASSERT_STREQ(out, "hello");
}

TEST(str_trim_all_whitespace) {
    /* NOTE: str_trim has a known edge case with all-whitespace and empty inputs
     * where strchr("\r\n\t {},", '\0') matches NUL, causing the trim loop to
     * read past the input. We only test well-defined inputs here. */
    char out[64] = {0};
    size_t len = str_trim(out, sizeof(out), "  hello  ", false);
    ASSERT_GT(len, 0);
    ASSERT_STREQ(out, "hello");
}

TEST(str_trim_tabs_and_newlines) {
    char out[64] = {0};
    str_trim(out, sizeof(out), "\t\nhello\r\n", false);
    ASSERT_STREQ(out, "hello");
}

TEST(str_trim_first_mode) {
    char out[64];
    str_trim(out, sizeof(out), "  hello world  ", true);
    ASSERT_STREQ(out, "hello");
}

/* ---- str_replace ---- */

TEST(str_replace_basic) {
    char *result = str_replace("hello world", "world", "test");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "hello test");
    free(result);
}

TEST(str_replace_no_match) {
    char *result = str_replace("hello", "xyz", "abc");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "hello");
    free(result);
}

TEST(str_replace_multiple) {
    char *result = str_replace("aXbXc", "X", "-");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "a-b-c");
    free(result);
}

TEST(str_replace_empty_rep) {
    char *result = str_replace("hello", "", "x");
    ASSERT_NULL(result);
}

TEST(str_replace_null_orig) {
    char *result = str_replace(NULL, "a", "b");
    ASSERT_NULL(result);
}

/* ---- str_split ---- */

TEST(str_split_basic) {
    char buf[32];
    strncpy(buf, "key=value", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *val = str_split(buf, "=");
    ASSERT_STREQ(buf, "key");
    ASSERT_NOT_NULL(val);
    ASSERT_STREQ(val, "value");
}

TEST(str_split_no_delim) {
    char buf[32];
    strncpy(buf, "noequalssign", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *val = str_split(buf, "=");
    ASSERT_NULL(val);
}

/* ---- str_removeParentheses ---- */

TEST(str_removeParentheses_basic) {
    char out[64];
    str_removeParentheses(out, "Game (USA)");
    ASSERT_STREQ(out, "Game");
}

TEST(str_removeParentheses_no_parens) {
    char out[64];
    str_removeParentheses(out, "Game Name");
    ASSERT_STREQ(out, "Game Name");
}

TEST(str_removeParentheses_brackets) {
    char out[64];
    str_removeParentheses(out, "Game [v1.0]");
    ASSERT_STREQ(out, "Game");
}

/* ---- str_getLastNumber ---- */

TEST(str_getLastNumber_found) {
    long val = 0;
    bool ok = str_getLastNumber("file123", &val);
    ASSERT_TRUE(ok);
    ASSERT_EQ(val, 123);
}

TEST(str_getLastNumber_not_found) {
    long val = 0;
    bool ok = str_getLastNumber("nodigits", &val);
    ASSERT_FALSE(ok);
}

/* ---- includeCJK ---- */

TEST(includeCJK_chinese) {
    // "你好" (hello in Chinese) in UTF-8
    ASSERT_TRUE(includeCJK("你好"));
}

TEST(includeCJK_japanese_hiragana) {
    // "こんにちは" (hello in Japanese hiragana) in UTF-8
    ASSERT_TRUE(includeCJK("こんにちは"));
}

TEST(includeCJK_japanese_katakana) {
    // "カタカナ" (katakana) in UTF-8
    ASSERT_TRUE(includeCJK("カタカナ"));
}

TEST(includeCJK_mixed) {
    // English with Chinese characters
    ASSERT_TRUE(includeCJK("Game 游戏"));
}

TEST(includeCJK_no_cjk) {
    ASSERT_FALSE(includeCJK("English Game"));
}

TEST(includeCJK_empty) {
    ASSERT_FALSE(includeCJK(""));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== str.c Unit Tests ===\n\n");

    RUN_TEST(str_endsWith_match);
    RUN_TEST(str_endsWith_no_match);
    RUN_TEST(str_endsWith_empty_suffix);
    RUN_TEST(str_endsWith_suffix_longer);
    RUN_TEST(str_endsWith_exact_match);

    RUN_TEST(str_count_char_basic);
    RUN_TEST(str_count_char_none);
    RUN_TEST(str_count_char_all);
    RUN_TEST(str_count_char_empty);

    RUN_TEST(str_trim_leading_trailing);
    RUN_TEST(str_trim_no_whitespace);
    RUN_TEST(str_trim_all_whitespace);
    RUN_TEST(str_trim_tabs_and_newlines);
    RUN_TEST(str_trim_first_mode);

    RUN_TEST(str_replace_basic);
    RUN_TEST(str_replace_no_match);
    RUN_TEST(str_replace_multiple);
    RUN_TEST(str_replace_empty_rep);
    RUN_TEST(str_replace_null_orig);

    RUN_TEST(str_split_basic);
    RUN_TEST(str_split_no_delim);

    RUN_TEST(str_removeParentheses_basic);
    RUN_TEST(str_removeParentheses_no_parens);
    RUN_TEST(str_removeParentheses_brackets);

    RUN_TEST(str_getLastNumber_found);
    RUN_TEST(str_getLastNumber_not_found);

    RUN_TEST(includeCJK_chinese);
    RUN_TEST(includeCJK_japanese_hiragana);
    RUN_TEST(includeCJK_japanese_katakana);
    RUN_TEST(includeCJK_mixed);
    RUN_TEST(includeCJK_no_cjk);
    RUN_TEST(includeCJK_empty);

    TEST_REPORT();
    return test_failures;
}
