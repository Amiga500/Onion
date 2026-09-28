#ifndef ONION_TEST_H__
#define ONION_TEST_H__

/**
 * @file onion_test.h
 * @brief Minimal C unit test framework for Onion OS.
 *
 * A lightweight test harness that runs without Google Test or SDL,
 * making it suitable for CI and host-machine testing.
 *
 * Usage:
 *   #include "onion_test.h"
 *
 *   TEST(my_test) {
 *       ASSERT_EQ(1 + 1, 2);
 *       ASSERT_STREQ("hello", "hello");
 *   }
 *
 *   int main(void) {
 *       RUN_TEST(my_test);
 *       TEST_REPORT();
 *       return test_failures;
 *   }
 */

#include <stdio.h>
#include <string.h>

static int test_count = 0;
static int test_failures = 0;
static int test_assertions = 0;

#define TEST(name) static void test_##name(void)

#define RUN_TEST(name) \
    do { \
        test_count++; \
        int failures_before_ = test_failures; \
        printf("  [RUN ] %s\n", #name); \
        test_##name(); \
        if (test_failures == failures_before_) \
            printf("  [ OK ] %s\n", #name); \
        else \
            printf("  [FAIL] %s\n", #name); \
    } while (0)

#define TEST_REPORT() \
    do { \
        printf("\n========================================\n"); \
        printf("  Tests: %d | Assertions: %d | Failures: %d\n", \
               test_count, test_assertions, test_failures); \
        printf("  Result: %s\n", test_failures == 0 ? "PASSED" : "FAILED"); \
        printf("========================================\n"); \
    } while (0)

#define ASSERT_TRUE(expr) \
    do { \
        test_assertions++; \
        if (!(expr)) { \
            fprintf(stderr, "    FAIL: %s:%d: ASSERT_TRUE(%s)\n", \
                    __FILE__, __LINE__, #expr); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_FALSE(expr) \
    do { \
        test_assertions++; \
        if ((expr)) { \
            fprintf(stderr, "    FAIL: %s:%d: ASSERT_FALSE(%s)\n", \
                    __FILE__, __LINE__, #expr); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        test_assertions++; \
        if ((a) != (b)) { \
            fprintf(stderr, "    FAIL: %s:%d: ASSERT_EQ(%s, %s) -> %ld != %ld\n", \
                    __FILE__, __LINE__, #a, #b, (long)(a), (long)(b)); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        test_assertions++; \
        if ((a) == (b)) { \
            fprintf(stderr, "    FAIL: %s:%d: ASSERT_NE(%s, %s) -> both %ld\n", \
                    __FILE__, __LINE__, #a, #b, (long)(a)); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_STREQ(a, b) \
    do { \
        test_assertions++; \
        if (strcmp((a), (b)) != 0) { \
            fprintf(stderr, "    FAIL: %s:%d: ASSERT_STREQ(\"%s\", \"%s\")\n", \
                    __FILE__, __LINE__, (a), (b)); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_STRNE(a, b) \
    do { \
        test_assertions++; \
        if (strcmp((a), (b)) == 0) { \
            fprintf(stderr, "    FAIL: %s:%d: ASSERT_STRNE(\"%s\", \"%s\")\n", \
                    __FILE__, __LINE__, (a), (b)); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        test_assertions++; \
        if ((ptr) != NULL) { \
            fprintf(stderr, "    FAIL: %s:%d: ASSERT_NULL(%s) -> %p\n", \
                    __FILE__, __LINE__, #ptr, (void *)(ptr)); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        test_assertions++; \
        if ((ptr) == NULL) { \
            fprintf(stderr, "    FAIL: %s:%d: ASSERT_NOT_NULL(%s)\n", \
                    __FILE__, __LINE__, #ptr); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_GT(a, b) \
    do { \
        test_assertions++; \
        if (!((a) > (b))) { \
            fprintf(stderr, "    FAIL: %s:%d: ASSERT_GT(%s, %s) -> %ld <= %ld\n", \
                    __FILE__, __LINE__, #a, #b, (long)(a), (long)(b)); \
            test_failures++; \
            return; \
        } \
    } while (0)

#define ASSERT_GE(a, b) \
    do { \
        test_assertions++; \
        if (!((a) >= (b))) { \
            fprintf(stderr, "    FAIL: %s:%d: ASSERT_GE(%s, %s) -> %ld < %ld\n", \
                    __FILE__, __LINE__, #a, #b, (long)(a), (long)(b)); \
            test_failures++; \
            return; \
        } \
    } while (0)

#endif /* ONION_TEST_H__ */
