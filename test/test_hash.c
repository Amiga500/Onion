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

/* hash.h loads exactly wrdlen bytes — no extra slack is required. */

/* ---- FNV1A_Pippip_Yurii ---- */

TEST(hash_basic_string) {
    char *str = (char *)calloc(1, strlen("hello") + 1);
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
    char *str1 = (char *)calloc(1, strlen("hello") + 1);
    char *str2 = (char *)calloc(1, strlen("world") + 1);
    strncpy(str1, "hello", strlen("hello") + 1);
    strncpy(str2, "world", strlen("world") + 1);
    
    uint32_t hash1 = FNV1A_Pippip_Yurii(str1, strlen(str1));
    uint32_t hash2 = FNV1A_Pippip_Yurii(str2, strlen(str2));
    
    ASSERT_NE(hash1, hash2);
    free(str1);
    free(str2);
}

TEST(hash_empty_string) {
    char *str = (char *)calloc(1, 1);
    str[0] = '\0';
    uint32_t hash = FNV1A_Pippip_Yurii(str, 0);
    ASSERT_GT(hash, 0);
    free(str);
}

TEST(hash_long_string) {
    // Test with string longer than 8 bytes
    const char *text = "this is a longer string for testing";
    char *str = (char *)calloc(1, strlen(text) + 1);
    strncpy(str, text, strlen(text) + 1);
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_GT(hash, 0);
    free(str);
}

TEST(hash_deterministic) {
    // Verify hash is deterministic across multiple calls
    const char *text = "test_string_123";
    char *str = (char *)calloc(1, strlen(text) + 1);
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
    char *str1 = (char *)calloc(1, strlen("Hello") + 1);
    char *str2 = (char *)calloc(1, strlen("hello") + 1);
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
        char *str = (char *)calloc(1, strlen(short_strs[i]) + 1);
        strncpy(str, short_strs[i], strlen(short_strs[i]) + 1);
        uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
        ASSERT_GT(hash, 0);
        free(str);
    }
}

TEST(hash_slightly_different) {
    // Small changes should produce different hashes
    char *str1 = (char *)calloc(1, strlen("test123") + 1);
    char *str2 = (char *)calloc(1, strlen("test124") + 1);
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
    char *str = (char *)calloc(1, strlen("hello") + 1);
    strncpy(str, "hello", strlen("hello") + 1);
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_EQ(hash, 0x4E020623u);
    free(str);
}

TEST(hash_known_value_world) {
    /* Verified expected value: FNV1A_Pippip_Yurii("world", 5) == 0x74BFE10F */
    char *str = (char *)calloc(1, strlen("world") + 1);
    strncpy(str, "world", strlen("world") + 1);
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_EQ(hash, 0x74BFE10Fu);
    free(str);
}

TEST(hash_known_value_path) {
    /* Long path (> 8 bytes) exercises the Cycles loop */
    /* Verified: FNV1A_Pippip_Yurii("/mnt/SDCARD/Roms/GBA/game.gba", 30) == 0x165E4B94 */
    const char *text = "/mnt/SDCARD/Roms/GBA/game.gba";
    char *str = (char *)calloc(1, strlen(text) + 1);
    strncpy(str, text, strlen(text) + 1);
    uint32_t hash = FNV1A_Pippip_Yurii(str, strlen(str));
    ASSERT_EQ(hash, 0x165E4B94u);
    free(str);
}

TEST(hash_different_lengths_differ) {
    /* "abc" and "ab" should produce different hashes */
    char *str1 = (char *)calloc(1, 3 + 1);
    char *str2 = (char *)calloc(1, 2 + 1);
    strncpy(str1, "abc", 4);
    strncpy(str2, "ab", 3);
    ASSERT_NE(FNV1A_Pippip_Yurii(str1, 3), FNV1A_Pippip_Yurii(str2, 2));
    free(str1);
    free(str2);
}

/* ---- regression vectors ---- */

/* Hash values end up in on-device file names (romScreens/<hash>.png built by
 * history_getRomscreenPath) and in the play-activity cache databases, so they
 * must stay stable forever: changing them orphans every cached file already on
 * a user's SD card. These vectors were captured from the implementation as it
 * behaved before the alignment/over-read fix and cover both code paths plus
 * every length around the wrdlen == 8 boundary. */
typedef struct {
    const char *input;
    uint32_t expected;
} HashVector;

static const HashVector hash_vectors[] = {
    {"", 0x590AF0A6u},
    {"a", 0x2288D439u},
    {"ab", 0xDA8B5217u},
    {"abc", 0x87A649B4u},
    {"abcd", 0x6ED36C75u},
    {"abcde", 0x93305563u},
    {"abcdef", 0x52418612u},
    {"abcdefg", 0x3D1CE94Fu},
    {"abcdefgh", 0x251CF14Fu},
    {"abcdefghi", 0x8918A757u},
    {"abcdefghij", 0x0218FEA7u},
    {"abcdefghijk", 0x8CEA205Fu},
    {"abcdefghijkl", 0x96D273D1u},
    {"abcdefghijklm", 0xF9DBC856u},
    {"abcdefghijklmn", 0x619369AEu},
    {"abcdefghijklmno", 0x10D002F1u},
    {"abcdefghijklmnop", 0xBE8C2CFAu},
    {"abcdefghijklmnopq", 0x374A2517u},
    {"abcdefghijklmnopqrstuvwxyz", 0xA0578CFDu},
    {"hello", 0x4E020623u},
    {"world", 0x74BFE10Fu},
    {"Hello", 0xD7455609u},
    {"test123", 0xBF26FF4Cu},
    {"test124", 0x4A450A2Fu},
    {"test_string_123", 0x6E51C414u},
    {"this is a longer string for testing", 0x8901DFEFu},
    {"/mnt/SDCARD/Roms/GBA/game.gba", 0x165E4B94u},
    {"/mnt/SDCARD/Roms/SFC/Super Mario World.sfc", 0xBFDE8CC4u},
    {"/mnt/SDCARD/Roms/MD/Sonic The Hedgehog 2 (World).md", 0xE18AB410u},
    {"/mnt/SDCARD/Saves/CurrentProfile/romScreens", 0x3704DB55u},
    {"0123456789012345678901234567890123456789012345678901234567890123",
     0x0F0451FCu},
    {"\x01\x02\x03\x04\x05\x06\x07\x08", 0x87F15125u},
    {"\xff\xfe\xfd\xfc\xfb\xfa\xf9\xf8\xf7", 0x4ED25776u},
};

#define HASH_VECTOR_COUNT (sizeof(hash_vectors) / sizeof(hash_vectors[0]))

TEST(hash_regression_vectors) {
    for (size_t i = 0; i < HASH_VECTOR_COUNT; i++) {
        size_t len = strlen(hash_vectors[i].input);
        char *str = (char *)calloc(1, len + 1);
        memcpy(str, hash_vectors[i].input, len);
        uint32_t hash = FNV1A_Pippip_Yurii(str, len);
        free(str);
        ASSERT_EQ(hash, hash_vectors[i].expected);
    }
}

TEST(hash_unaligned_start_offsets) {
    /* ARMv7 LDRD faults on unaligned addresses, so the hash must neither
     * depend on nor assume the alignment of the string it is given. */
    for (size_t i = 0; i < HASH_VECTOR_COUNT; i++) {
        size_t len = strlen(hash_vectors[i].input);
        for (size_t offset = 0; offset < 8; offset++) {
            char *buf = (char *)calloc(1, offset + len + 1);
            memcpy(buf + offset, hash_vectors[i].input, len);
            uint32_t hash = FNV1A_Pippip_Yurii(buf + offset, len);
            free(buf);
            ASSERT_EQ(hash, hash_vectors[i].expected);
        }
    }
}

TEST(hash_exact_size_buffer_no_overread) {
    /* Exact-sized allocation: no terminator and no slack, so a read past
     * wrdlen is a heap overflow under ASan. */
    for (size_t i = 0; i < HASH_VECTOR_COUNT; i++) {
        size_t len = strlen(hash_vectors[i].input);
        if (len == 0)
            continue;
        char *buf = (char *)malloc(len);
        memcpy(buf, hash_vectors[i].input, len);
        uint32_t hash = FNV1A_Pippip_Yurii(buf, len);
        free(buf);
        ASSERT_EQ(hash, hash_vectors[i].expected);
    }
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
    RUN_TEST(hash_regression_vectors);
    RUN_TEST(hash_unaligned_start_offsets);
    RUN_TEST(hash_exact_size_buffer_no_overread);

    TEST_REPORT();
    return test_failures;
}
