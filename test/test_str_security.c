#include "onion_test.h"

#include "utils/str.h"

TEST(replace_null_rep)
{
    ASSERT_NULL(str_replace("abc", NULL, "x"));
}

TEST(endsWith_empty_suffix)
{
    ASSERT_TRUE(str_endsWith("abc", ""));
}

TEST(count_char_empty_string)
{
    ASSERT_EQ(str_count_char("", '/'), 0);
}

TEST(split_empty_delim)
{
    char buf[] = "abc";
    /* strstr with empty delim returns the start; then *p=0 wipes the string. */
    char *tail = str_split(buf, "");
    ASSERT_NOT_NULL(tail);
}

int main(void)
{
    printf("\n=== str security/edge tests ===\n\n");
    RUN_TEST(replace_null_rep);
    RUN_TEST(endsWith_empty_suffix);
    RUN_TEST(count_char_empty_string);
    RUN_TEST(split_empty_delim);
    return onion_test_report("test_str_security");
}
