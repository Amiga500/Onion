/**
 * @file test_flags.c
 * @brief Unit tests for src/common/utils/flags.h
 *
 * Tests the flag_set / flag_get filesystem-backed boolean flag API.
 * Uses temp directory for isolation.
 *
 * Build and run: make -f Makefile.unit test_flags && ./build_test/test_flags
 */

#include "onion_test.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/common/utils/flags.h"

static char test_flag_path[256];

static void setup_flag_dir(void)
{
    snprintf(test_flag_path, sizeof(test_flag_path),
             "/tmp/onion_test_flags_%d/", (int)getpid());
    mkdirs(test_flag_path);
}

static void cleanup_flag_dir(void)
{
    file_remove_recursive(test_flag_path);
}

/* ---- flag_set creates file ---- */

TEST(flag_set_creates_file) {
    setup_flag_dir();

    flag_set(test_flag_path, "feature_a", true);

    char expected[sizeof(test_flag_path) + sizeof("feature_a")];
    snprintf(expected, sizeof(expected), "%s%s", test_flag_path, "feature_a");
    ASSERT_TRUE(exists(expected));

    cleanup_flag_dir();
}

/* ---- flag_get returns true ---- */

TEST(flag_get_returns_true_when_set) {
    setup_flag_dir();

    flag_set(test_flag_path, "active", true);
    ASSERT_TRUE(flag_get(test_flag_path, "active"));

    cleanup_flag_dir();
}

/* ---- flag_get returns false ---- */

TEST(flag_get_returns_false_when_not_set) {
    setup_flag_dir();

    ASSERT_FALSE(flag_get(test_flag_path, "nonexistent"));

    cleanup_flag_dir();
}

/* ---- flag_set false removes file ---- */

TEST(flag_set_false_removes_file) {
    setup_flag_dir();

    flag_set(test_flag_path, "temp", true);
    ASSERT_TRUE(flag_get(test_flag_path, "temp"));

    flag_set(test_flag_path, "temp", false);
    ASSERT_FALSE(flag_get(test_flag_path, "temp"));

    cleanup_flag_dir();
}

/* ---- toggle flag ---- */

TEST(flag_toggle_roundtrip) {
    setup_flag_dir();

    flag_set(test_flag_path, "toggle", false);
    ASSERT_FALSE(flag_get(test_flag_path, "toggle"));

    flag_set(test_flag_path, "toggle", true);
    ASSERT_TRUE(flag_get(test_flag_path, "toggle"));

    flag_set(test_flag_path, "toggle", false);
    ASSERT_FALSE(flag_get(test_flag_path, "toggle"));

    cleanup_flag_dir();
}

/* ---- multiple flags ---- */

TEST(multiple_flags_independent) {
    setup_flag_dir();

    flag_set(test_flag_path, "flag_x", true);
    flag_set(test_flag_path, "flag_y", false);
    flag_set(test_flag_path, "flag_z", true);

    ASSERT_TRUE(flag_get(test_flag_path, "flag_x"));
    ASSERT_FALSE(flag_get(test_flag_path, "flag_y"));
    ASSERT_TRUE(flag_get(test_flag_path, "flag_z"));

    cleanup_flag_dir();
}

/* ---- flag_set true is idempotent ---- */

TEST(flag_set_true_idempotent) {
    setup_flag_dir();

    flag_set(test_flag_path, "idem", true);
    flag_set(test_flag_path, "idem", true);
    ASSERT_TRUE(flag_get(test_flag_path, "idem"));

    cleanup_flag_dir();
}

/* ---- flag_set false on nonexistent ---- */

TEST(flag_set_false_nonexistent_no_crash) {
    setup_flag_dir();

    /* Should not crash even if flag file doesn't exist */
    flag_set(test_flag_path, "doesnt_exist", false);
    ASSERT_FALSE(flag_get(test_flag_path, "doesnt_exist"));

    cleanup_flag_dir();
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== flags.h Unit Tests ===\n\n");

    RUN_TEST(flag_set_creates_file);
    RUN_TEST(flag_get_returns_true_when_set);
    RUN_TEST(flag_get_returns_false_when_not_set);
    RUN_TEST(flag_set_false_removes_file);
    RUN_TEST(flag_toggle_roundtrip);
    RUN_TEST(multiple_flags_independent);
    RUN_TEST(flag_set_true_idempotent);
    RUN_TEST(flag_set_false_nonexistent_no_crash);

    TEST_REPORT();
    return test_failures;
}
