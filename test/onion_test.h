#ifndef ONION_TEST_H__
#define ONION_TEST_H__

/*
 * Minimal host test harness for OnionRefactor.
 * No gtest, no SDL, no device. Compile and run on the build machine.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_onion_tests_run = 0;
static int g_onion_tests_failed = 0;
static int g_onion_asserts = 0;
static int g_onion_current_failed = 0;

#define TEST(name) static void test_##name(void)

#define RUN_TEST(name)                                                         \
    do {                                                                       \
        g_onion_tests_run++;                                                   \
        g_onion_current_failed = 0;                                            \
        test_##name();                                                         \
        if (g_onion_current_failed) {                                          \
            g_onion_tests_failed++;                                            \
            printf("  FAIL  %s\n", #name);                                     \
        } else {                                                               \
            printf("  ok    %s\n", #name);                                     \
        }                                                                      \
    } while (0)

#define FAIL_AT(msg)                                                           \
    do {                                                                       \
        g_onion_asserts++;                                                     \
        g_onion_current_failed++;                                              \
        fprintf(stderr, "    assertion failed at %s:%d: %s\n", __FILE__,       \
                __LINE__, msg);                                                \
    } while (0)

#define ASSERT_TRUE(cond)                                                      \
    do {                                                                       \
        g_onion_asserts++;                                                     \
        if (!(cond)) {                                                         \
            g_onion_current_failed++;                                          \
            fprintf(stderr, "    ASSERT_TRUE(%s) failed at %s:%d\n", #cond,    \
                    __FILE__, __LINE__);                                       \
        }                                                                      \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_NULL(p) ASSERT_TRUE((p) == NULL)
#define ASSERT_NOT_NULL(p) ASSERT_TRUE((p) != NULL)

#define ASSERT_EQ(a, b)                                                        \
    do {                                                                       \
        g_onion_asserts++;                                                     \
        if ((a) != (b)) {                                                      \
            g_onion_current_failed++;                                          \
            fprintf(stderr,                                                    \
                    "    ASSERT_EQ failed at %s:%d: %s == %s\n", __FILE__,     \
                    __LINE__, #a, #b);                                         \
        }                                                                      \
    } while (0)

#define ASSERT_STREQ(a, b)                                                     \
    do {                                                                       \
        g_onion_asserts++;                                                     \
        const char *_sa = (a);                                                 \
        const char *_sb = (b);                                                 \
        if (_sa == NULL || _sb == NULL || strcmp(_sa, _sb) != 0) {             \
            g_onion_current_failed++;                                          \
            fprintf(stderr,                                                    \
                    "    ASSERT_STREQ failed at %s:%d\n      got:    %s\n"     \
                    "      expect: %s\n",                                      \
                    __FILE__, __LINE__, _sa ? _sa : "(null)",                  \
                    _sb ? _sb : "(null)");                                     \
        }                                                                      \
    } while (0)

static int onion_test_report(const char *suite)
{
    printf("\n%s: %d run, %d failed, %d assertions\n", suite, g_onion_tests_run,
           g_onion_tests_failed, g_onion_asserts);
    return g_onion_tests_failed ? 1 : 0;
}

#endif /* ONION_TEST_H__ */
