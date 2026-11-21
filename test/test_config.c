/**
 * @file test_config.c
 * @brief Unit tests for src/common/utils/config.h
 *
 * Tests the config key-value store: setNumber/get, setString/get,
 * flag_set/get roundtrip, and edge cases.
 *
 * Build and run: make -f Makefile.unit test_config && ./build_test/test_config
 */

#include "onion_test.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Override CONFIG_PATH to use a temp directory for testing.
 * Must be defined BEFORE including config.h since the functions
 * are header-inline and expand CONFIG_PATH at compile time. */
static char test_config_path[256];
#define CONFIG_PATH test_config_path

/* The production header: it only defines CONFIG_PATH when it is not set,
 * so the test's folder is used. (This file used to carry a copy of it,
 * which had drifted: file_put_sync where the source writes atomically.) */
#include "utils/config.h"

/* ---- Test setup helpers ---- */

static void setup_config_dir(void)
{
    snprintf(test_config_path, sizeof(test_config_path),
             "/tmp/onion_test_config_%d/", (int)getpid());
    mkdirs(test_config_path);
}

static void cleanup_config_dir(void)
{
    file_remove_recursive(test_config_path);
}

/* ---- config_setNumber / config_get ---- */

TEST(config_setNumber_and_get) {
    setup_config_dir();

    config_setNumber("brightness", 42);
    int value = 0;
    bool found = config_get("brightness", CONFIG_INT, &value);

    ASSERT_TRUE(found);
    ASSERT_EQ(value, 42);

    cleanup_config_dir();
}

TEST(config_setNumber_overwrite) {
    setup_config_dir();

    config_setNumber("volume", 10);
    config_setNumber("volume", 75);
    int value = 0;
    config_get("volume", CONFIG_INT, &value);

    ASSERT_EQ(value, 75);

    cleanup_config_dir();
}

TEST(config_setNumber_zero) {
    setup_config_dir();

    config_setNumber("zero_val", 0);
    int value = -1;
    bool found = config_get("zero_val", CONFIG_INT, &value);

    ASSERT_TRUE(found);
    ASSERT_EQ(value, 0);

    cleanup_config_dir();
}

TEST(config_setNumber_negative) {
    setup_config_dir();

    config_setNumber("neg_val", -5);
    int value = 0;
    bool found = config_get("neg_val", CONFIG_INT, &value);

    ASSERT_TRUE(found);
    ASSERT_EQ(value, -5);

    cleanup_config_dir();
}

/* ---- config_setString / config_get ---- */

TEST(config_setString_and_get) {
    setup_config_dir();

    config_setString("name", "Onion OS");
    char value[STR_MAX] = {0};
    bool found = config_get("name", CONFIG_STR, value);

    ASSERT_TRUE(found);
    ASSERT_STREQ(value, "Onion OS");

    cleanup_config_dir();
}

TEST(config_setString_empty) {
    setup_config_dir();

    config_setString("empty_key", "");
    /* File is created but contains empty string; fscanf with %[^\n]
     * won't match on empty content, so config_get returns true
     * (file exists) but value is unchanged. */
    char value[STR_MAX] = "placeholder";
    bool found = config_get("empty_key", CONFIG_STR, value);

    ASSERT_TRUE(found);
    /* fscanf %[^\n] on empty content doesn't write to dest */
    ASSERT_STREQ(value, "placeholder");

    cleanup_config_dir();
}

/* ---- config_get nonexistent key ---- */

TEST(config_get_nonexistent) {
    setup_config_dir();

    int value = 99;
    bool found = config_get("nonexistent_key", CONFIG_INT, &value);

    ASSERT_FALSE(found);
    ASSERT_EQ(value, 99); /* value unchanged */

    cleanup_config_dir();
}

/* ---- config_flag_set / config_flag_get ---- */

TEST(config_flag_set_true) {
    setup_config_dir();

    config_flag_set("feature_x", true);

    ASSERT_TRUE(config_flag_get("feature_x"));

    cleanup_config_dir();
}

TEST(config_flag_set_false) {
    setup_config_dir();

    config_flag_set("feature_y", true);
    config_flag_set("feature_y", false);

    ASSERT_FALSE(config_flag_get("feature_y"));

    cleanup_config_dir();
}

TEST(config_flag_get_nonexistent) {
    setup_config_dir();

    ASSERT_FALSE(config_flag_get("no_such_flag"));

    cleanup_config_dir();
}

TEST(config_flag_toggle) {
    setup_config_dir();

    config_flag_set("toggle_flag", false);
    ASSERT_FALSE(config_flag_get("toggle_flag"));

    config_flag_set("toggle_flag", true);
    ASSERT_TRUE(config_flag_get("toggle_flag"));

    config_flag_set("toggle_flag", false);
    ASSERT_FALSE(config_flag_get("toggle_flag"));

    cleanup_config_dir();
}

/* ---- main ---- */

/* What is written is read back as the same value: a number above 9 (a
 * number written in hex read back as 15 would not pass) and a string. */
TEST(config_values_read_back_exactly) {
    setup_config_dir();
    config_setNumber("startup/addHours", 15);
    int n = 0;
    ASSERT_TRUE(config_get("startup/addHours", CONFIG_INT, &n));
    ASSERT_EQ(n, 15);
    config_setNumber("battery/warnAt", -1);
    ASSERT_TRUE(config_get("battery/warnAt", CONFIG_INT, &n));
    ASSERT_EQ(n, -1);
    config_setString("display/blueLightTime", "21:30");
    char buf[16] = "";
    ASSERT_TRUE(config_getString("display/blueLightTime", buf, sizeof(buf)));
    ASSERT_STREQ(buf, "21:30");
    cleanup_config_dir();
}

int main(void)
{
    printf("\n=== config.h Unit Tests ===\n\n");

    RUN_TEST(config_values_read_back_exactly);
    RUN_TEST(config_setNumber_and_get);
    RUN_TEST(config_setNumber_overwrite);
    RUN_TEST(config_setNumber_zero);
    RUN_TEST(config_setNumber_negative);

    RUN_TEST(config_setString_and_get);
    RUN_TEST(config_setString_empty);

    RUN_TEST(config_get_nonexistent);

    RUN_TEST(config_flag_set_true);
    RUN_TEST(config_flag_set_false);
    RUN_TEST(config_flag_get_nonexistent);
    RUN_TEST(config_flag_toggle);

    TEST_REPORT();
    return test_failures;
}
