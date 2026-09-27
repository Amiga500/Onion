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

/* Production code: ra_getBoolFromConfig() from gameSwitcher/gs_ra_files.h,
 * with the real file_parseKeyValue() from utils/file.c. */
#include "../src/gameSwitcher/gs_ra_files.h"

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

/* ---- Tests for ra_loadHistory ---- */

TEST(history_missing_file) {
    ASSERT_FALSE(ra_loadHistory("/tmp/nonexistent_ra_history_xyz.lpl"));
    ASSERT_NULL(g_cachedRetroArchHistory);
}

TEST(history_empty_file) {
    const char *path = create_temp_cfg("");
    ASSERT_FALSE(ra_loadHistory(path));
    ASSERT_NULL(g_cachedRetroArchHistory);
    cleanup_temp_cfg();
}

TEST(history_valid_file) {
    const char *path = create_temp_cfg("{\"items\":[{\"path\":\"/a.gba\"}]}");
    ASSERT_TRUE(ra_loadHistory(path));
    ASSERT_NOT_NULL(g_cachedRetroArchHistory);
    ASSERT_TRUE(cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(g_cachedRetroArchHistory, "items")));
    ra_freeHistory();
    ASSERT_NULL(g_cachedRetroArchHistory);
    cleanup_temp_cfg();
}

TEST(history_invalid_json) {
    const char *path = create_temp_cfg("{not json");
    ASSERT_FALSE(ra_loadHistory(path));
    ASSERT_NULL(g_cachedRetroArchHistory);
    cleanup_temp_cfg();
}

/* A directory opens but has no readable size. */
TEST(history_directory) {
    ASSERT_FALSE(ra_loadHistory("/tmp"));
    ASSERT_NULL(g_cachedRetroArchHistory);
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

    RUN_TEST(history_missing_file);
    RUN_TEST(history_empty_file);
    RUN_TEST(history_valid_file);
    RUN_TEST(history_invalid_json);
    RUN_TEST(history_directory);

    TEST_REPORT();
    return test_failures;
}
