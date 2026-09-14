#include "onion_test.h"

#include "utils/hash.h"

#include <string.h>

/* hash.h warns: add 8 extra bytes to avoid OOB reads. */
static uint32_t hash_padded(const char *s, size_t n)
{
    char buf[256];
    ASSERT_TRUE(n + 8 < sizeof(buf));
    memset(buf, 0, sizeof(buf));
    memcpy(buf, s, n);
    return FNV1A_Pippip_Yurii(buf, n);
}

TEST(same_input_same_hash)
{
    uint32_t a = hash_padded("Onion", 5);
    uint32_t b = hash_padded("Onion", 5);
    ASSERT_EQ(a, b);
}

TEST(different_input_different_hash)
{
    uint32_t a = hash_padded("Onion", 5);
    uint32_t b = hash_padded("onion", 5);
    ASSERT_TRUE(a != b);
}

TEST(len_matters)
{
    uint32_t a = hash_padded("abc", 3);
    uint32_t b = hash_padded("abcd", 4);
    ASSERT_TRUE(a != b);
}

TEST(short_le_8)
{
    uint32_t h = hash_padded("12345678", 8);
    ASSERT_TRUE(h != 0);
}

TEST(long_gt_8)
{
    const char *s = "0123456789abcdefXYZ";
    uint32_t h = hash_padded(s, strlen(s));
    ASSERT_TRUE(h != 0);
    ASSERT_EQ(h, hash_padded(s, strlen(s)));
}

TEST(golden_onion)
{
    /* Pinned on x86_64 little-endian host; this is the current algorithm. */
    ASSERT_EQ(hash_padded("Onion", 5), 0x60fec03fu);
}

int main(void)
{
    printf("\n=== hash.h unit tests ===\n\n");
    RUN_TEST(same_input_same_hash);
    RUN_TEST(different_input_different_hash);
    RUN_TEST(len_matters);
    RUN_TEST(short_le_8);
    RUN_TEST(long_gt_8);
    RUN_TEST(golden_onion);
    return onion_test_report("test_hash");
}
