/**
 * Unit Tests for String Utilities
 * 
 * Tests the str.c utility functions for correctness and edge cases
 */

#include "../unity/unity.h"
#include "../../src/common/utils/str.h"
#include <string.h>
#include <stdlib.h>

/* Test: str_trim removes leading and trailing whitespace */
void test_str_trim_basic(void)
{
    char test1[] = "  hello  ";
    str_trim(test1);
    TEST_ASSERT_EQUAL_STRING("hello", test1);
    
    char test2[] = "\t\nworld\r\n";
    str_trim(test2);
    TEST_ASSERT_EQUAL_STRING("world", test2);
    
    char test3[] = "nospace";
    str_trim(test3);
    TEST_ASSERT_EQUAL_STRING("nospace", test3);
}

/* Test: str_trim handles empty and whitespace-only strings */
void test_str_trim_edge_cases(void)
{
    char test1[] = "";
    str_trim(test1);
    TEST_ASSERT_EQUAL_STRING("", test1);
    
    char test2[] = "   ";
    str_trim(test2);
    TEST_ASSERT_EQUAL_STRING("", test2);
    
    char test3[] = "\t\n\r";
    str_trim(test3);
    TEST_ASSERT_EQUAL_STRING("", test3);
}

/* Test: str_split correctly splits strings */
void test_str_split_basic(void)
{
    const char *input = "apple,banana,cherry";
    char **parts = str_split((char *)input, ',');
    
    TEST_ASSERT_NOT_NULL(parts);
    TEST_ASSERT_EQUAL_STRING("apple", parts[0]);
    TEST_ASSERT_EQUAL_STRING("banana", parts[1]);
    TEST_ASSERT_EQUAL_STRING("cherry", parts[2]);
    TEST_ASSERT_NULL(parts[3]);
    
    /* Cleanup */
    for (int i = 0; parts[i] != NULL; i++) {
        free(parts[i]);
    }
    free(parts);
}

/* Test: str_split handles empty parts */
void test_str_split_empty_parts(void)
{
    const char *input = "a,,c";
    char **parts = str_split((char *)input, ',');
    
    TEST_ASSERT_NOT_NULL(parts);
    TEST_ASSERT_EQUAL_STRING("a", parts[0]);
    TEST_ASSERT_EQUAL_STRING("", parts[1]);
    TEST_ASSERT_EQUAL_STRING("c", parts[2]);
    TEST_ASSERT_NULL(parts[3]);
    
    /* Cleanup */
    for (int i = 0; parts[i] != NULL; i++) {
        free(parts[i]);
    }
    free(parts);
}

/* Test: str_startsWith correctly identifies prefixes */
void test_str_startsWith(void)
{
    TEST_ASSERT_TRUE(str_startsWith("hello world", "hello"));
    TEST_ASSERT_TRUE(str_startsWith("hello", "hello"));
    TEST_ASSERT_FALSE(str_startsWith("hello", "world"));
    TEST_ASSERT_FALSE(str_startsWith("hi", "hello"));
    TEST_ASSERT_TRUE(str_startsWith("test", ""));
}

/* Test: str_endsWith correctly identifies suffixes */
void test_str_endsWith(void)
{
    TEST_ASSERT_TRUE(str_endsWith("hello world", "world"));
    TEST_ASSERT_TRUE(str_endsWith("hello", "hello"));
    TEST_ASSERT_FALSE(str_endsWith("hello", "world"));
    TEST_ASSERT_FALSE(str_endsWith("hi", "hello"));
    TEST_ASSERT_TRUE(str_endsWith("test", ""));
}

/* Test: str_replace replaces all occurrences */
void test_str_replace_all(void)
{
    char buffer[100];
    
    strcpy(buffer, "hello world world");
    str_replace(buffer, "world", "there");
    TEST_ASSERT_EQUAL_STRING("hello there there", buffer);
    
    strcpy(buffer, "aaa");
    str_replace(buffer, "a", "b");
    TEST_ASSERT_EQUAL_STRING("bbb", buffer);
}

/* Test: str_replace handles no matches */
void test_str_replace_no_match(void)
{
    char buffer[100];
    
    strcpy(buffer, "hello world");
    str_replace(buffer, "xyz", "abc");
    TEST_ASSERT_EQUAL_STRING("hello world", buffer);
}

/* Main test runner */
int main(void)
{
    UNITY_BEGIN();
    
    /* Run all tests */
    RUN_TEST(test_str_trim_basic);
    RUN_TEST(test_str_trim_edge_cases);
    RUN_TEST(test_str_split_basic);
    RUN_TEST(test_str_split_empty_parts);
    RUN_TEST(test_str_startsWith);
    RUN_TEST(test_str_endsWith);
    RUN_TEST(test_str_replace_all);
    RUN_TEST(test_str_replace_no_match);
    
    return UNITY_END();
}
