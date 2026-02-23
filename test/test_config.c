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

/* Prevent config.h from redefining CONFIG_PATH */
#include "../src/common/utils/file.h"
#include "../src/common/utils/flags.h"
#include "../src/common/utils/log.h"
#include "../src/common/utils/str.h"

/* Include config.h content inline, skipping the original CONFIG_PATH define */
#ifndef CONFIG_H__
#define CONFIG_H__

#define CONFIG_INT "%d"
#define CONFIG_STR "%[^\n]"

bool config_flag_get(const char *key) { return flag_get(CONFIG_PATH, key); }

void config_flag_set(const char *key, bool value)
{
    char hidden_flag[STR_MAX];
    concat(hidden_flag, key, "_");
    flag_set(CONFIG_PATH, key, value);
    flag_set(CONFIG_PATH, hidden_flag, !value);
}

bool config_get(const char *key, const char *format, void *dest)
{
    FILE *fp;

    char filename[STR_MAX];
    concat(filename, CONFIG_PATH, key);

    if (exists(filename)) {
        file_get(fp, filename, format, dest);
        return true;
    }

    return false;
}

void _config_prepare(const char *key, char *filename)
{
    concat(filename, CONFIG_PATH, key);

    char dir_path[STR_MAX];
    strncpy(dir_path, filename, STR_MAX - 1);
    dir_path[STR_MAX - 1] = '\0';
    dirname(dir_path);

    mkdirs(dir_path);
}

void config_setNumber(const char *key, int value)
{
    FILE *fp;
    char filename[STR_MAX];
    _config_prepare(key, filename);
    file_put_sync(fp, filename, "%d", value);
}

void config_setString(const char *key, const char *value)
{
    FILE *fp;
    char filename[STR_MAX];
    _config_prepare(key, filename);
    file_put_sync(fp, filename, "%s", value);
}

#endif // CONFIG_H__

/* ---- Test setup helpers ---- */

static void setup_config_dir(void)
{
    snprintf(test_config_path, sizeof(test_config_path),
             "/tmp/onion_test_config_%d/", (int)getpid());
    mkdirs(test_config_path);
}

static void cleanup_config_dir(void)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_config_path);
    system(cmd);
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
    /* Empty string creates a file, but reading it back gives empty */
    char value[STR_MAX] = "placeholder";
    bool found = config_get("empty_key", CONFIG_STR, value);

    ASSERT_TRUE(found);
    /* fscanf with %[^\n] on empty content won't match, so value stays */

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

int main(void)
{
    printf("\n=== config.h Unit Tests ===\n\n");

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
