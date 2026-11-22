/**
 * @file test_konami.c
 * @brief Tests for src/keymon/konami.h (production header)
 *
 * The easter egg's code must start the game on the first try, whatever was
 * pressed before it: Onion's index went back to the start after a wrong key
 * and missed a code typed right after an UP or with one UP too many.
 *
 * Build and run: make -f Makefile.unit test_konami
 */

#include "../src/keymon/konami.h"
#include "onion_test.h"

enum { U = 1, D, L, R, B, A, X };
static const int CODE[] = {U, U, D, D, L, R, L, R, B, A};
#define CODE_LEN ((int)(sizeof(CODE) / sizeof(CODE[0])))

static int feed_all(KonamiState *st, const int *keys, int n)
{
    int hits = 0;
    for (int i = 0; i < n; i++)
        hits += konami_feed(st, CODE, CODE_LEN, keys[i]);
    return hits;
}

TEST(the_code_alone)
{
    KonamiState st = {0};
    ASSERT_EQ(feed_all(&st, CODE, CODE_LEN), 1);
}

TEST(right_after_an_up)
{
    /* Onion: UP left the index at 1, then the code's UP UP missed. */
    KonamiState st = {0};
    const int keys[] = {U, U, U, D, D, L, R, L, R, B, A};
    ASSERT_EQ(feed_all(&st, keys, 11), 1);
}

TEST(after_other_keys)
{
    KonamiState st = {0};
    const int keys[] = {A, D, U, R, B, U, U, D, D, L, R, L, R, B, A};
    ASSERT_EQ(feed_all(&st, keys, 15), 1);
}

TEST(completes_only_on_the_last_key)
{
    KonamiState st = {0};
    for (int i = 0; i < CODE_LEN - 1; i++)
        ASSERT_FALSE(konami_feed(&st, CODE, CODE_LEN, CODE[i]));
    ASSERT_TRUE(konami_feed(&st, CODE, CODE_LEN, CODE[CODE_LEN - 1]));
}

TEST(a_wrong_key_inside_needs_the_whole_code_again)
{
    KonamiState st = {0};
    const int keys[] = {U, U, D, D, L, X, L, R, B, A};
    ASSERT_EQ(feed_all(&st, keys, 10), 0);
    ASSERT_EQ(feed_all(&st, CODE, CODE_LEN), 1);
}

TEST(each_time_the_whole_code)
{
    /* After a hit the presses start over: its last keys don't count. */
    KonamiState st = {0};
    ASSERT_EQ(feed_all(&st, CODE, CODE_LEN), 1);
    ASSERT_EQ(feed_all(&st, CODE, CODE_LEN), 1);
    const int tail[] = {D, D, L, R, L, R, B, A};
    ASSERT_EQ(feed_all(&st, tail, 8), 0);
}

TEST(bad_arguments)
{
    KonamiState st = {0};
    ASSERT_FALSE(konami_feed(NULL, CODE, CODE_LEN, U));
    ASSERT_FALSE(konami_feed(&st, NULL, CODE_LEN, U));
    ASSERT_FALSE(konami_feed(&st, CODE, 0, U));
    ASSERT_FALSE(konami_feed(&st, CODE, KONAMI_MAX + 1, U));
    st.count = 99; /* a damaged state starts over */
    ASSERT_EQ(feed_all(&st, CODE, CODE_LEN), 1);
}

int main(void)
{
    printf("\n=== Easter egg code ===\n\n");
    RUN_TEST(the_code_alone);
    RUN_TEST(right_after_an_up);
    RUN_TEST(after_other_keys);
    RUN_TEST(completes_only_on_the_last_key);
    RUN_TEST(a_wrong_key_inside_needs_the_whole_code_again);
    RUN_TEST(each_time_the_whole_code);
    RUN_TEST(bad_arguments);
    TEST_REPORT();
    return test_failures;
}
