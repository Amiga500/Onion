/**
 * @file test_hash_equiv.c
 * @brief The new FNV1A_Pippip_Yurii gives the hashes the original gave.
 *
 * The hash names the GameSwitcher pictures (romScreens/<hash>.png): if it
 * changed, every picture would be lost after an update. hash_old.h is
 * upstream's utils/hash.h (made by the Makefile with git show). The original
 * is undefined for an empty string (a shift by 64), so lengths start at 1.
 *
 * Build and run: make -f Makefile.unit test_hash_equiv
 */

#include "onion_test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FNV1A_Pippip_Yurii FNV1A_Pippip_Yurii_upstream
#pragma GCC diagnostic ignored "-Wcast-align"
#include "hash_old.h"
#undef FNV1A_Pippip_Yurii
#undef UTILS_HASH_H__

#include "utils/hash.h"

static uint32_t rng_state = 0x12345678u;
static uint32_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

// len bytes of src at `offset` inside a buffer with 16 bytes of `fill`
// after them (the original reads up to 8 bytes past a short string).
static uint8_t *make(const uint8_t *src, size_t len, size_t offset, uint8_t fill, uint8_t **base_out)
{
    uint8_t *base = (uint8_t *)malloc(offset + len + 16);
    memset(base, fill, offset + len + 16);
    memcpy(base + offset, src, len);
    *base_out = base;
    return base + offset;
}

TEST(equal_to_upstream_for_every_length)
{
    static const uint8_t fills[3] = {0x00, 0xFF, 0xA5};
    int mismatches = 0;
    uint8_t src[300];

    for (size_t len = 1; len <= 300; len++) {
        for (int round = 0; round < 8; round++) {
            for (size_t i = 0; i < len; i++)
                src[i] = (uint8_t)rng();
            uint8_t *base;
            uint8_t *p = make(src, len, rng() % 8, fills[round % 3], &base);
            uint32_t a = FNV1A_Pippip_Yurii_upstream((const char *)p, len);
            uint32_t b = FNV1A_Pippip_Yurii((const char *)p, len);
            if (a != b) {
                fprintf(stderr, "    mismatch: len %zu round %d: %08x vs %08x\n", len, round, a, b);
                mismatches++;
            }
            free(base);
        }
    }
    ASSERT_EQ(mismatches, 0);
}

TEST(equal_for_path_like_strings)
{
    static const char *paths[] = {
        "a",
        "/mnt/SDCARD/Roms/GBA/Pokemon - Emerald (USA).gba",
        "/mnt/SDCARD/Roms/PS/Final Fantasy VII (USA) (Disc 1).cue",
        "/mnt/SDCARD/Roms/MD/Sonic $ Knuckles [!].md",
        "/mnt/SDCARD/Roms/SFC/Pok\xc3\xa9mon: \"Quoted\" & Co..sfc",
        "../../Roms/NES/x.nes",
        "12345678",
        "123456789",
        "1234567890123456",
        "12345678901234567",
    };
    int mismatches = 0;
    for (size_t k = 0; k < sizeof(paths) / sizeof(paths[0]); k++) {
        size_t len = strlen(paths[k]);
        uint8_t *base;
        uint8_t *p = make((const uint8_t *)paths[k], len, 0, 0xFF, &base);
        if (FNV1A_Pippip_Yurii_upstream((const char *)p, len) != FNV1A_Pippip_Yurii((const char *)p, len))
            mismatches++;
        free(base);
    }
    ASSERT_EQ(mismatches, 0);
}

TEST(new_hash_does_not_read_past_the_string)
{
    // Exactly len bytes, nothing after: under ASan a read past the end fails
    // here. The result must be the one computed on a padded copy.
    uint8_t src[64];
    int mismatches = 0;
    for (size_t len = 1; len <= 64; len++) {
        for (size_t i = 0; i < len; i++)
            src[i] = (uint8_t)rng();
        uint8_t *exact = (uint8_t *)malloc(len);
        memcpy(exact, src, len);
        uint8_t *base;
        uint8_t *p = make(src, len, 0, 0x5A, &base);
        if (FNV1A_Pippip_Yurii((const char *)exact, len) != FNV1A_Pippip_Yurii((const char *)p, len))
            mismatches++;
        free(exact);
        free(base);
    }
    ASSERT_EQ(mismatches, 0);
}

TEST(empty_string_is_defined)
{
    const char *empty = "";
    ASSERT_TRUE(FNV1A_Pippip_Yurii(empty, 0) == FNV1A_Pippip_Yurii(empty, 0));
}

int main(void)
{
    printf("\n=== hash: same values as upstream ===\n\n");
    RUN_TEST(equal_to_upstream_for_every_length);
    RUN_TEST(equal_for_path_like_strings);
    RUN_TEST(new_hash_does_not_read_past_the_string);
    RUN_TEST(empty_string_is_defined);
    TEST_REPORT();
    return test_failures > 0 ? 1 : 0;
}
