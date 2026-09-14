#include "onion_test.h"

#include "utils/str.h"

TEST(getLastNumber_simple)
{
    long val = 0;
    char buf[] = "track12";
    ASSERT_TRUE(str_getLastNumber(buf, &val));
    ASSERT_EQ(val, 12);
}

TEST(getLastNumber_last_wins)
{
    long val = 0;
    char buf[] = "v1.2.30";
    ASSERT_TRUE(str_getLastNumber(buf, &val));
    ASSERT_EQ(val, 30);
}

TEST(getLastNumber_none)
{
    long val = 99;
    char buf[] = "no-digits";
    ASSERT_FALSE(str_getLastNumber(buf, &val));
    ASSERT_EQ(val, 99);
}

TEST(split_found)
{
    char buf[] = "head::tail";
    char *tail = str_split(buf, "::");
    ASSERT_STREQ(buf, "head");
    ASSERT_STREQ(tail, "tail");
}

TEST(split_missing)
{
    char buf[] = "no-delim";
    ASSERT_NULL(str_split(buf, "::"));
    ASSERT_STREQ(buf, "no-delim");
}

TEST(replace_all)
{
    char *out = str_replace("a_b_c", "_", " ");
    ASSERT_NOT_NULL(out);
    ASSERT_STREQ(out, "a b c");
    free(out);
}

TEST(replace_none)
{
    char *out = str_replace("abc", "_", " ");
    ASSERT_NOT_NULL(out);
    ASSERT_STREQ(out, "abc");
    free(out);
}

TEST(replace_null_orig)
{
    ASSERT_NULL(str_replace(NULL, "_", " "));
}

TEST(replace_empty_rep)
{
    ASSERT_NULL(str_replace("abc", "", "x"));
}

TEST(replace_null_with_is_empty)
{
    char *out = str_replace("a_b", "_", NULL);
    ASSERT_NOT_NULL(out);
    ASSERT_STREQ(out, "ab");
    free(out);
}

TEST(trim_spaces_first_false)
{
    char out[32];
    size_t n = str_trim(out, sizeof(out), "  hello  ", false);
    ASSERT_STREQ(out, "hello");
    ASSERT_EQ(n, 5);
}

TEST(trim_already_clean)
{
    char out[32];
    size_t n = str_trim(out, sizeof(out), "hello", false);
    ASSERT_STREQ(out, "hello");
    ASSERT_EQ(n, 5);
}

TEST(trim_zero_len)
{
    char out[8] = "keep";
    ASSERT_EQ(str_trim(out, 0, "hello", false), 0);
}

TEST(endsWith_true)
{
    ASSERT_TRUE(str_endsWith("game.gba", ".gba"));
}

TEST(endsWith_false)
{
    ASSERT_FALSE(str_endsWith("game.gba", ".zip"));
}

TEST(endsWith_null)
{
    ASSERT_FALSE(str_endsWith(NULL, ".gba"));
    ASSERT_FALSE(str_endsWith("game.gba", NULL));
}

TEST(removeParentheses_round_and_square)
{
    char out[STR_MAX];
    str_removeParentheses(out, "Zelda (USA) [!]");
    ASSERT_STREQ(out, "Zelda");
}

TEST(serializeTime_seconds)
{
    char out[32];
    str_serializeTime(out, 9);
    ASSERT_STREQ(out, "9s");
}

TEST(serializeTime_minutes)
{
    char out[32];
    str_serializeTime(out, 125);
    ASSERT_STREQ(out, "2m 5s");
}

TEST(serializeTime_hours)
{
    char out[32];
    str_serializeTime(out, 3661);
    ASSERT_STREQ(out, "1h 1m");
}

TEST(count_char_plain)
{
    ASSERT_EQ(str_count_char("a/b/c", '/'), 2);
}

TEST(count_char_none)
{
    ASSERT_EQ(str_count_char("abc", '/'), 0);
}

TEST(includeCJK_ascii)
{
    char buf[] = "Mario";
    ASSERT_FALSE(includeCJK(buf));
}

TEST(includeCJK_high_bit)
{
    /* Current implementation treats any byte >= 0x80 as a hit. */
    char buf[] = {(char)0xC3, (char)0xA9, 0};
    ASSERT_TRUE(includeCJK(buf));
}

int main(void)
{
    printf("\n=== str.c unit tests ===\n\n");
    RUN_TEST(getLastNumber_simple);
    RUN_TEST(getLastNumber_last_wins);
    RUN_TEST(getLastNumber_none);
    RUN_TEST(split_found);
    RUN_TEST(split_missing);
    RUN_TEST(replace_all);
    RUN_TEST(replace_none);
    RUN_TEST(replace_null_orig);
    RUN_TEST(replace_empty_rep);
    RUN_TEST(replace_null_with_is_empty);
    RUN_TEST(trim_spaces_first_false);
    RUN_TEST(trim_already_clean);
    RUN_TEST(trim_zero_len);
    RUN_TEST(endsWith_true);
    RUN_TEST(endsWith_false);
    RUN_TEST(endsWith_null);
    RUN_TEST(removeParentheses_round_and_square);
    RUN_TEST(serializeTime_seconds);
    RUN_TEST(serializeTime_minutes);
    RUN_TEST(serializeTime_hours);
    RUN_TEST(count_char_plain);
    RUN_TEST(count_char_none);
    RUN_TEST(includeCJK_ascii);
    RUN_TEST(includeCJK_high_bit);
    return onion_test_report("test_str");
}
