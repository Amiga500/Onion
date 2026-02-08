/**
 * @file test_hash.c
 * @brief Unit tests for src/common/utils/hash.h
 *
 * Tests FNV1A_Pippip_Yurii hash function
 *
 * Build and run: make -f Makefile.unit test_hash && ./build_test/test_hash
 */

#include "onion_test.h"
#include "../src/common/utils/hash.h"
#include <string.h>
#include <stdlib.h>

/* The hash function requires 8 extra bytes in the buffer to prevent
 * out of boundary reads as documented in hash.h */
#define HASH_BUFFER_EXTRA 8

/* ---- FNV1A_Pippip_Yurii ---- */

TEST(hash_basic_string) {
    // Add extra buffer as required by hash function
    char *str = (char *)malloc(strlen("hello") + 1 + HASH_BUFFER_EXTRA);
    strcpy(str, "hello");
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    // Hash should be deterministic - same input produces same output
    uint32_t hash2 = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_EQ(hash, hash2);
    ASSERT_GT(hash, 0);
    free(str);
}

TEST(hash_different_strings) {
    // Different strings should produce different hashes (in most cases)
    char *str1 = (char *)malloc(strlen("hello") + 1 + HASH_BUFFER_EXTRA);
    char *str2 = (char *)malloc(strlen("world") + 1 + HASH_BUFFER_EXTRA);
    strcpy(str1, "hello");
    strcpy(str2, "world");
    
    uint32_t hash1 = FNV1A_Pippip_Yurii(str1, strlen(str1));
    uint32_t hash2 = FNV1A_Pippip_Yurii(str2, strlen(str2));
    
    ASSERT_NE(hash1, hash2);
    free(str1);
    free(str2);
}

TEST(hash_empty_string) {
    char *str = (char *)malloc(1 + HASH_BUFFER_EXTRA);
    str[0] = '\0';
    uint32_t hash = FNV1A_Pippip_Yurii(str, 0);
    ASSERT_GT(hash, 0);
    free(str);
}

TEST(hash_long_string) {
    // Test with string longer than 8 bytes
    const char *text = "this is a longer string for testing";
    char *str = (char *)malloc(strlen(text) + 1 + HASH_BUFFER_EXTRA);
    strcpy(str, text);
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_GT(hash, 0);
    free(str);
}

TEST(hash_deterministic) {
    // Verify hash is deterministic across multiple calls
    const char *text = "test_string_123";
    char *str = (char *)malloc(strlen(text) + 1 + HASH_BUFFER_EXTRA);
    strcpy(str, text);
    
    uint32_t hash1 = FNV1A_Pippip_Yurii(str, strlen(str));
    uint32_t hash2 = FNV1A_Pippip_Yurii(str, strlen(str));
    uint32_t hash3 = FNV1A_Pippip_Yurii(str, strlen(str));
    
    ASSERT_EQ(hash1, hash2);
    ASSERT_EQ(hash2, hash3);
    free(str);
}

TEST(hash_case_sensitive) {
    // Hash should be case-sensitive
    char *str1 = (char *)malloc(strlen("Hello") + 1 + HASH_BUFFER_EXTRA);
    char *str2 = (char *)malloc(strlen("hello") + 1 + HASH_BUFFER_EXTRA);
    strcpy(str1, "Hello");
    strcpy(str2, "hello");
    
    uint32_t hash1 = FNV1A_Pippip_Yurii(str1, strlen(str1));
    uint32_t hash2 = FNV1A_Pippip_Yurii(str2, strlen(str2));
    
    ASSERT_NE(hash1, hash2);
    free(str1);
    free(str2);
}

TEST(hash_short_strings) {
    // Test with strings <= 8 bytes (uses different code path)
    const char *short_strs[] = {"a", "ab", "abc", "abcd", "abcde", "abcdef", "abcdefg", "abcdefgh"};
    size_t num_strings = sizeof(short_strs) / sizeof(short_strs[0]);
    
    for (size_t i = 0; i < num_strings; i++) {
        char *str = (char *)malloc(strlen(short_strs[i]) + 1 + HASH_BUFFER_EXTRA);
        strcpy(str, short_strs[i]);
        uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
        ASSERT_GT(hash, 0);
        free(str);
    }
}

TEST(hash_slightly_different) {
    // Small changes should produce different hashes
    char *str1 = (char *)malloc(strlen("test123") + 1 + HASH_BUFFER_EXTRA);
    char *str2 = (char *)malloc(strlen("test124") + 1 + HASH_BUFFER_EXTRA);
    strcpy(str1, "test123");
    strcpy(str2, "test124");
    
    uint32_t hash1 = FNV1A_Pippip_Yurii(str1, strlen(str1));
    uint32_t hash2 = FNV1A_Pippip_Yurii(str2, strlen(str2));
    
    ASSERT_NE(hash1, hash2);
    free(str1);
    free(str2);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== hash.h Unit Tests ===\n\n");

    RUN_TEST(hash_basic_string);
    RUN_TEST(hash_different_strings);
    RUN_TEST(hash_empty_string);
    RUN_TEST(hash_long_string);
    RUN_TEST(hash_deterministic);
    RUN_TEST(hash_case_sensitive);
    RUN_TEST(hash_short_strings);
    RUN_TEST(hash_slightly_different);

    TEST_REPORT();
    return test_failures;
}
