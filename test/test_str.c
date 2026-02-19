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
    /* After fix: all-whitespace input is safe (no null-byte overread). */
    char out[64] = {0};
    size_t len = str_trim(out, sizeof(out), "   ", false);
    ASSERT_EQ(len, 1); /* str_trim returns 1 when it writes only the null terminator */
    ASSERT_STREQ(out, "");
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

/* Regression: STR_MAX-length input with no parentheses must not write OOB */
TEST(str_removeParentheses_max_length_no_parens) {
    /* Build a string of exactly STR_MAX - 1 'a' chars (plus null terminator) */
    char input[STR_MAX];
    memset(input, 'a', STR_MAX - 1);
    input[STR_MAX - 1] = '\0';

    char out[STR_MAX];
    str_removeParentheses(out, input); /* must not crash or overwrite past out[] */
    /* str_trim(buf, STR_MAX-1, ...) truncates to STR_MAX-2 printable chars */
    ASSERT_EQ((int)strlen(out), STR_MAX - 2);
    for (int i = 0; i < STR_MAX - 2; i++)
        ASSERT_TRUE(out[i] == 'a');
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

/* ---- str_serializeTime ---- */

TEST(str_serializeTime_seconds_only) {
    char result[STR_MAX];
    str_serializeTime(result, 30);
    ASSERT_STREQ(result, "30s");
}

TEST(str_serializeTime_zero) {
    char result[STR_MAX];
    str_serializeTime(result, 0);
    ASSERT_STREQ(result, "0s");
}

TEST(str_serializeTime_one_minute) {
    char result[STR_MAX];
    str_serializeTime(result, 60);
    ASSERT_STREQ(result, "1m 0s");
}

TEST(str_serializeTime_minutes_and_seconds) {
    char result[STR_MAX];
    str_serializeTime(result, 150);
    ASSERT_STREQ(result, "2m 30s");
}

TEST(str_serializeTime_one_hour) {
    char result[STR_MAX];
    str_serializeTime(result, 3600);
    ASSERT_STREQ(result, "1h 0m");
}

TEST(str_serializeTime_hours_and_minutes) {
    char result[STR_MAX];
    str_serializeTime(result, 3900);
    ASSERT_STREQ(result, "1h 5m");
}

TEST(str_serializeTime_multiple_hours) {
    char result[STR_MAX];
    str_serializeTime(result, 7325);
    ASSERT_STREQ(result, "2h 2m");
}

TEST(str_serializeTime_large_value) {
    char result[STR_MAX];
    str_serializeTime(result, 86400);
    ASSERT_STREQ(result, "24h 0m");
}

/* ---- str_endsWith (extra) ---- */

TEST(str_endsWith_case_sensitive) {
    /* Suffix match is case-sensitive */
    ASSERT_FALSE(str_endsWith("HELLO.PNG", ".png"));
    ASSERT_FALSE(str_endsWith("hello.png", ".PNG"));
}

TEST(str_endsWith_null_str) {
    ASSERT_FALSE(str_endsWith(NULL, ".png"));
}

TEST(str_endsWith_null_suffix) {
    ASSERT_FALSE(str_endsWith("hello.png", NULL));
}

/* ---- str_trim (extra) ---- */

TEST(str_trim_quoted_string) {
    /* str_trim strips surrounding double-quotes when the first non-WS char is " */
    char out[64] = {0};
    str_trim(out, sizeof(out), "\"hello world\"", false);
    ASSERT_STREQ(out, "hello world");
}

TEST(str_trim_truncates_to_buffer) {
    /* When output buffer is smaller than the trimmed string it must be truncated */
    char out[4] = {0};
    str_trim(out, sizeof(out), "hello", false);
    /* Only the first 3 printable chars fit plus the NUL */
    ASSERT_EQ((int)strlen(out), 3);
    ASSERT_TRUE(out[3] == '\0');
}

/* ---- str_split (extra) ---- */

TEST(str_split_only_first_occurrence) {
    /* str_split splits on the FIRST occurrence; tail may contain more delimiters */
    char buf[32];
    strncpy(buf, "a=b=c", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tail = str_split(buf, "=");
    ASSERT_STREQ(buf, "a");
    ASSERT_NOT_NULL(tail);
    ASSERT_STREQ(tail, "b=c");
}

TEST(str_split_empty_head) {
    char buf[32];
    strncpy(buf, "=value", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *tail = str_split(buf, "=");
    ASSERT_STREQ(buf, "");
    ASSERT_NOT_NULL(tail);
    ASSERT_STREQ(tail, "value");
}

/* ---- str_replace (extra) ---- */

TEST(str_replace_null_with_means_empty) {
    /* When 'with' is NULL str_replace treats it as "" */
    char *result = str_replace("hello world", "world", NULL);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "hello ");
    free(result);
}

TEST(str_replace_entire_string) {
    char *result = str_replace("abc", "abc", "xyz");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "xyz");
    free(result);
}

/* ---- str_getLastNumber (extra) ---- */

TEST(str_getLastNumber_multiple_numbers) {
    /* Returns the LAST number in the string */
    long val = 0;
    bool ok = str_getLastNumber("track12of34", &val);
    ASSERT_TRUE(ok);
    ASSERT_EQ(val, 34);
}

TEST(str_getLastNumber_leading_number) {
    long val = 0;
    bool ok = str_getLastNumber("99bottles", &val);
    ASSERT_TRUE(ok);
    ASSERT_EQ(val, 99);
}

/* ---- str_count_char (extra) ---- */

TEST(str_count_char_slash) {
    ASSERT_EQ(str_count_char("/mnt/SDCARD/Roms/game.gba", '/'), 4);
}

/* ---- str_removeParentheses (extra) ---- */

TEST(str_removeParentheses_nested_not_supported) {
    /* The function only handles one level: the first ')' closes the paren
     * group, so the second unmatched ')' is preserved in the output. */
    char out[64];
    str_removeParentheses(out, "Game (PAL (v1.1))");
    /* After first ')' closes the outer '(', the trailing ')' is kept → "Game )" */
    ASSERT_STREQ(out, "Game )");
}

TEST(str_removeParentheses_multiple_parens) {
    char out[64];
    str_removeParentheses(out, "Super Game (USA) (Rev 2)");
    ASSERT_STREQ(out, "Super Game");
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
    RUN_TEST(str_removeParentheses_max_length_no_parens);

    RUN_TEST(str_getLastNumber_found);
    RUN_TEST(str_getLastNumber_not_found);

    RUN_TEST(includeCJK_chinese);
    RUN_TEST(includeCJK_japanese_hiragana);
    RUN_TEST(includeCJK_japanese_katakana);
    RUN_TEST(includeCJK_mixed);
    RUN_TEST(includeCJK_no_cjk);
    RUN_TEST(includeCJK_empty);

    RUN_TEST(str_serializeTime_seconds_only);
    RUN_TEST(str_serializeTime_zero);
    RUN_TEST(str_serializeTime_one_minute);
    RUN_TEST(str_serializeTime_minutes_and_seconds);
    RUN_TEST(str_serializeTime_one_hour);
    RUN_TEST(str_serializeTime_hours_and_minutes);
    RUN_TEST(str_serializeTime_multiple_hours);
    RUN_TEST(str_serializeTime_large_value);

    RUN_TEST(str_endsWith_case_sensitive);
    RUN_TEST(str_endsWith_null_str);
    RUN_TEST(str_endsWith_null_suffix);

    RUN_TEST(str_trim_quoted_string);
    RUN_TEST(str_trim_truncates_to_buffer);

    RUN_TEST(str_split_only_first_occurrence);
    RUN_TEST(str_split_empty_head);

    RUN_TEST(str_replace_null_with_means_empty);
    RUN_TEST(str_replace_entire_string);

    RUN_TEST(str_getLastNumber_multiple_numbers);
    RUN_TEST(str_getLastNumber_leading_number);

    RUN_TEST(str_count_char_slash);

    RUN_TEST(str_removeParentheses_nested_not_supported);
    RUN_TEST(str_removeParentheses_multiple_parens);

    TEST_REPORT();
    return test_failures;
}
