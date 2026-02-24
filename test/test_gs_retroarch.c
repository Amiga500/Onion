/**
 * @file test_gs_retroarch.c
 * @brief Unit tests for gs_retroarch.h pure-logic functions
 *
 * Tests ra_getBoolFromConfig parsing logic and ra_loadHistory
 * edge cases (empty file, ftell failure).
 *
 * Build and run: make -f Makefile.unit test_gs_retroarch
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STR_MAX 256

/* ---- Stub out log macros ---- */
#define print_debug(...)
#define printf_debug(...)

/* ---- Inline file_parseKeyValue stub for testing ---- */

/**
 * Simplified stub that reads a "key = value" file format.
 * Matches the interface used in gs_retroarch.h.
 */
static char *file_parseKeyValue(const char *file_path, const char *key_in,
                                char *value_out, char divider, int select_index)
{
    (void)select_index;
    value_out[0] = '\0';

    FILE *f = fopen(file_path, "r");
    if (f == NULL)
        return value_out;

    char line[1024];
    size_t key_len = strlen(key_in);
    while (fgets(line, sizeof(line), f) != NULL) {
        /* Skip lines that don't start with the key */
        if (strncmp(line, key_in, key_len) != 0)
            continue;

        /* Find the divider */
        char *div = strchr(line + key_len, divider);
        if (div == NULL)
            continue;

        /* Skip spaces after divider */
        char *val = div + 1;
        while (*val == ' ' || *val == '\t')
            val++;

        /* Remove trailing whitespace/newline */
        char *end = val + strlen(val) - 1;
        while (end > val && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '"'))
            *end-- = '\0';

        /* Remove leading quote if present */
        if (*val == '"')
            val++;

        strncpy(value_out, val, STR_MAX * 2 - 1);
        value_out[STR_MAX * 2 - 1] = '\0';
        break;
    }

    fclose(f);
    return value_out;
}

/* ---- Inline ra_getBoolFromConfig from gs_retroarch.h ---- */

static bool ra_getBoolFromConfig(const char *cfg_path, bool *out_value, const char *key)
{
    char value[STR_MAX * 2];
    file_parseKeyValue(cfg_path, key, value, '=', 0);
    if (strcmp(value, "true") == 0) {
        *out_value = true;
        return true;
    }
    else if (strcmp(value, "false") == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

/* ---- Helper to create temp config files ---- */

static char tmp_cfg_path[256];
static int tmp_counter = 0;

static const char *create_temp_cfg(const char *content)
{
    snprintf(tmp_cfg_path, sizeof(tmp_cfg_path), "/tmp/test_gs_retroarch_%d_%d.cfg",
             (int)getpid(), tmp_counter++);
    FILE *f = fopen(tmp_cfg_path, "w");
    if (f != NULL) {
        fputs(content, f);
        fclose(f);
    }
    return tmp_cfg_path;
}

static void cleanup_temp_cfg(void)
{
    remove(tmp_cfg_path);
}

/* ---- Tests for ra_getBoolFromConfig ---- */

TEST(bool_config_true) {
    const char *path = create_temp_cfg("video_dingux_ipu_keep_aspect = \"true\"\n");
    bool result = false;
    ASSERT_TRUE(ra_getBoolFromConfig(path, &result, "video_dingux_ipu_keep_aspect"));
    ASSERT_TRUE(result);
    cleanup_temp_cfg();
}

TEST(bool_config_false) {
    const char *path = create_temp_cfg("video_dingux_ipu_keep_aspect = \"false\"\n");
    bool result = true;
    ASSERT_TRUE(ra_getBoolFromConfig(path, &result, "video_dingux_ipu_keep_aspect"));
    ASSERT_FALSE(result);
    cleanup_temp_cfg();
}

TEST(bool_config_unquoted_true) {
    const char *path = create_temp_cfg("video_scale_integer = true\n");
    bool result = false;
    ASSERT_TRUE(ra_getBoolFromConfig(path, &result, "video_scale_integer"));
    ASSERT_TRUE(result);
    cleanup_temp_cfg();
}

TEST(bool_config_missing_key) {
    const char *path = create_temp_cfg("other_option = true\n");
    bool result = false;
    ASSERT_FALSE(ra_getBoolFromConfig(path, &result, "video_dingux_ipu_keep_aspect"));
    ASSERT_FALSE(result); /* unchanged */
    cleanup_temp_cfg();
}

TEST(bool_config_invalid_value) {
    const char *path = create_temp_cfg("video_scale_integer = maybe\n");
    bool result = false;
    ASSERT_FALSE(ra_getBoolFromConfig(path, &result, "video_scale_integer"));
    cleanup_temp_cfg();
}

TEST(bool_config_empty_file) {
    const char *path = create_temp_cfg("");
    bool result = false;
    ASSERT_FALSE(ra_getBoolFromConfig(path, &result, "video_scale_integer"));
    cleanup_temp_cfg();
}

TEST(bool_config_nonexistent_file) {
    bool result = false;
    ASSERT_FALSE(ra_getBoolFromConfig("/tmp/nonexistent_config_xyz.cfg", &result, "key"));
}

TEST(bool_config_multiple_keys) {
    const char *path = create_temp_cfg(
        "video_dingux_ipu_keep_aspect = true\n"
        "video_scale_integer = false\n"
        "other = 42\n");
    bool result1 = false, result2 = true;
    ASSERT_TRUE(ra_getBoolFromConfig(path, &result1, "video_dingux_ipu_keep_aspect"));
    ASSERT_TRUE(result1);
    ASSERT_TRUE(ra_getBoolFromConfig(path, &result2, "video_scale_integer"));
    ASSERT_FALSE(result2);
    cleanup_temp_cfg();
}

/* ---- Tests for ftell validation logic ---- */

/*
 * Test the ftell validation pattern used in ra_loadHistory.
 * We replicate just the validation check since ra_loadHistory
 * requires cJSON which is tested elsewhere.
 */

static bool validate_file_size(const char *path)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
        return false;

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize <= 0) {
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

TEST(ftell_empty_file_rejected) {
    const char *path = create_temp_cfg("");
    ASSERT_FALSE(validate_file_size(path));
    cleanup_temp_cfg();
}

TEST(ftell_nonempty_file_accepted) {
    const char *path = create_temp_cfg("{\"items\":[]}");
    ASSERT_TRUE(validate_file_size(path));
    cleanup_temp_cfg();
}

TEST(ftell_nonexistent_file_rejected) {
    ASSERT_FALSE(validate_file_size("/tmp/nonexistent_ftell_test_xyz.cfg"));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== gs_retroarch.h Unit Tests ===\n\n");

    /* ra_getBoolFromConfig */
    RUN_TEST(bool_config_true);
    RUN_TEST(bool_config_false);
    RUN_TEST(bool_config_unquoted_true);
    RUN_TEST(bool_config_missing_key);
    RUN_TEST(bool_config_invalid_value);
    RUN_TEST(bool_config_empty_file);
    RUN_TEST(bool_config_nonexistent_file);
    RUN_TEST(bool_config_multiple_keys);

    /* ftell validation */
    RUN_TEST(ftell_empty_file_rejected);
    RUN_TEST(ftell_nonempty_file_accepted);
    RUN_TEST(ftell_nonexistent_file_rejected);

    TEST_REPORT();
    return test_failures;
}
