/**
 * @file test_critical_fixes.c
 * @brief Tests for critical bug fixes (round 2)
 *
 * Validates that file_getExtension() handles NULL input.
 *
 * Build and run: make -f Makefile.unit test_critical_fixes
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ---- Stubs ---- */
#define print_debug(...)
#define printf_debug(...)

/* ================================================================== */
/*  Test: file_getExtension NULL safety                               */
/* ================================================================== */

TEST(file_getExtension_normal) {
    ASSERT_STREQ(file_getExtension("game.gba"), "gba");
}

TEST(file_getExtension_no_dot) {
    ASSERT_STREQ(file_getExtension("game"), "");
}

TEST(file_getExtension_null_input) {
    /* After fix: should return "" instead of crashing */
    const char *ext = file_getExtension(NULL);
    ASSERT_NOT_NULL(ext);
    ASSERT_STREQ(ext, "");
}

TEST(file_getExtension_dot_only) {
    /* filename starts with dot and that's the only dot: no extension */
    ASSERT_STREQ(file_getExtension(".hidden"), "");
}

TEST(file_getExtension_empty_string) {
    ASSERT_STREQ(file_getExtension(""), "");
}

TEST(file_getExtension_multiple_dots) {
    ASSERT_STREQ(file_getExtension("game.backup.gba"), "gba");
}

/* ================================================================== */
/*  main                                                              */
/* ================================================================== */

int main(void)
{
    printf("\n=== Critical Bug Fix Tests (Round 2) ===\n\n");

    /* file_getExtension NULL safety */
    RUN_TEST(file_getExtension_normal);
    RUN_TEST(file_getExtension_no_dot);
    RUN_TEST(file_getExtension_null_input);
    RUN_TEST(file_getExtension_dot_only);
    RUN_TEST(file_getExtension_empty_string);
    RUN_TEST(file_getExtension_multiple_dots);

    TEST_REPORT();
    return test_failures;
}
