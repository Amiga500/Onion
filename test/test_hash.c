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
    char *str = (char *)calloc(1, strlen("hello") + 1 + HASH_BUFFER_EXTRA);
    strncpy(str, "hello", strlen("hello") + 1);
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    // Hash should be deterministic - same input produces same output
    uint32_t hash2 = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_EQ(hash, hash2);
    ASSERT_GT(hash, 0);
    free(str);
}

TEST(hash_different_strings) {
    // Different strings should produce different hashes (in most cases)
    char *str1 = (char *)calloc(1, strlen("hello") + 1 + HASH_BUFFER_EXTRA);
    char *str2 = (char *)calloc(1, strlen("world") + 1 + HASH_BUFFER_EXTRA);
    strncpy(str1, "hello", strlen("hello") + 1);
    strncpy(str2, "world", strlen("world") + 1);
    
    uint32_t hash1 = FNV1A_Pippip_Yurii(str1, strlen(str1));
    uint32_t hash2 = FNV1A_Pippip_Yurii(str2, strlen(str2));
    
    ASSERT_NE(hash1, hash2);
    free(str1);
    free(str2);
}

TEST(hash_empty_string) {
    char *str = (char *)calloc(1, 1 + HASH_BUFFER_EXTRA);
    str[0] = '\0';
    uint32_t hash = FNV1A_Pippip_Yurii(str, 0);
    ASSERT_GT(hash, 0);
    free(str);
}

TEST(hash_long_string) {
    // Test with string longer than 8 bytes
    const char *text = "this is a longer string for testing";
    char *str = (char *)calloc(1, strlen(text) + 1 + HASH_BUFFER_EXTRA);
    strncpy(str, text, strlen(text) + 1);
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_GT(hash, 0);
    free(str);
}

TEST(hash_deterministic) {
    // Verify hash is deterministic across multiple calls
    const char *text = "test_string_123";
    char *str = (char *)calloc(1, strlen(text) + 1 + HASH_BUFFER_EXTRA);
    strncpy(str, text, strlen(text) + 1);
    
    uint32_t hash1 = FNV1A_Pippip_Yurii(str, strlen(str));
    uint32_t hash2 = FNV1A_Pippip_Yurii(str, strlen(str));
    uint32_t hash3 = FNV1A_Pippip_Yurii(str, strlen(str));
    
    ASSERT_EQ(hash1, hash2);
    ASSERT_EQ(hash2, hash3);
    free(str);
}

TEST(hash_case_sensitive) {
    // Hash should be case-sensitive
    char *str1 = (char *)calloc(1, strlen("Hello") + 1 + HASH_BUFFER_EXTRA);
    char *str2 = (char *)calloc(1, strlen("hello") + 1 + HASH_BUFFER_EXTRA);
    strncpy(str1, "Hello", strlen("Hello") + 1);
    strncpy(str2, "hello", strlen("hello") + 1);
    
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
        char *str = (char *)calloc(1, strlen(short_strs[i]) + 1 + HASH_BUFFER_EXTRA);
        strncpy(str, short_strs[i], strlen(short_strs[i]) + 1);
        uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
        ASSERT_GT(hash, 0);
        free(str);
    }
}

TEST(hash_slightly_different) {
    // Small changes should produce different hashes
    char *str1 = (char *)calloc(1, strlen("test123") + 1 + HASH_BUFFER_EXTRA);
    char *str2 = (char *)calloc(1, strlen("test124") + 1 + HASH_BUFFER_EXTRA);
    strncpy(str1, "test123", strlen("test123") + 1);
    strncpy(str2, "test124", strlen("test124") + 1);
    
    uint32_t hash1 = FNV1A_Pippip_Yurii(str1, strlen(str1));
    uint32_t hash2 = FNV1A_Pippip_Yurii(str2, strlen(str2));
    
    ASSERT_NE(hash1, hash2);
    free(str1);
    free(str2);
}

/* ---- known values ---- */

TEST(hash_known_value_hello) {
    /* Verified expected value: FNV1A_Pippip_Yurii("hello", 5) == 0x4E020623 */
    char *str = (char *)calloc(1, strlen("hello") + 1 + HASH_BUFFER_EXTRA);
    strncpy(str, "hello", strlen("hello") + 1);
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_EQ(hash, 0x4E020623u);
    free(str);
}

TEST(hash_known_value_world) {
    /* Verified expected value: FNV1A_Pippip_Yurii("world", 5) == 0x74BFE10F */
    char *str = (char *)calloc(1, strlen("world") + 1 + HASH_BUFFER_EXTRA);
    strncpy(str, "world", strlen("world") + 1);
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_EQ(hash, 0x74BFE10Fu);
    free(str);
}

TEST(hash_known_value_path) {
    /* Long path (> 8 bytes) exercises the Cycles loop */
    /* Verified: FNV1A_Pippip_Yurii("/mnt/SDCARD/Roms/GBA/game.gba", 30) == 0x165E4B94 */
    const char *text = "/mnt/SDCARD/Roms/GBA/game.gba";
    char *str = (char *)calloc(1, strlen(text) + 1 + HASH_BUFFER_EXTRA);
    strncpy(str, text, strlen(text) + 1);
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_EQ(hash, 0x165E4B94u);
    free(str);
}

TEST(hash_different_lengths_differ) {
    /* "abc" and "ab" should produce different hashes */
    char *str1 = (char *)calloc(1, 3 + 1 + HASH_BUFFER_EXTRA);
    char *str2 = (char *)calloc(1, 2 + 1 + HASH_BUFFER_EXTRA);
    strncpy(str1, "abc", 4);
    strncpy(str2, "ab", 3);
    ASSERT_NE(FNV1A_Pippip_Yurii(str1, 3), FNV1A_Pippip_Yurii(str2, 2));
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
    RUN_TEST(hash_known_value_hello);
    RUN_TEST(hash_known_value_world);
    RUN_TEST(hash_known_value_path);
    RUN_TEST(hash_different_lengths_differ);

    TEST_REPORT();
    return test_failures;
}
