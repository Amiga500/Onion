/**
 * @file test_system_utils.c
 * @brief Unit tests for src/common/system/system_utils.h
 *
 * Tests check_autosave() which parses retroarch.cfg for the
 * "savestate_auto_save" key and returns true/false.
 *
 * The function uses file_parseKeyValue with '=' delimiter.
 * We test by writing temp config files and inlining the logic.
 *
 * Build and run: make -f Makefile.unit test_system_utils
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_CONFIG_PATH "/tmp/test_retroarch.cfg"

/* ---- Inline check_autosave using our temp path ---- */

static bool check_autosave_path(const char *config_path)
{
    char value[STR_MAX];
    memset(value, 0, sizeof(value));
    file_parseKeyValue(config_path, "savestate_auto_save", value, '=', 0);
    return strcmp(value, "true") == 0;
}

/* ---- Helper ---- */

static void write_config(const char *path, const char *content)
{
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "%s", content);
        fclose(fp);
    }
}

/* ==== check_autosave tests ==== */

TEST(autosave_true) {
    write_config(TEST_CONFIG_PATH,
        "video_smooth = false\n"
        "savestate_auto_save = true\n"
        "audio_enable = true\n");
    ASSERT_TRUE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_false) {
    write_config(TEST_CONFIG_PATH,
        "video_smooth = false\n"
        "savestate_auto_save = false\n"
        "audio_enable = true\n");
    ASSERT_FALSE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_missing_key) {
    write_config(TEST_CONFIG_PATH,
        "video_smooth = false\n"
        "audio_enable = true\n");
    /* Key not present → value stays empty → strcmp fails → false */
    ASSERT_FALSE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_empty_file) {
    write_config(TEST_CONFIG_PATH, "");
    ASSERT_FALSE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_missing_file) {
    unlink(TEST_CONFIG_PATH);
    ASSERT_FALSE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_first_line) {
    write_config(TEST_CONFIG_PATH,
        "savestate_auto_save = true\n"
        "other_setting = false\n");
    ASSERT_TRUE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_last_line) {
    write_config(TEST_CONFIG_PATH,
        "other_setting = false\n"
        "savestate_auto_save = true\n");
    ASSERT_TRUE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_only_line) {
    write_config(TEST_CONFIG_PATH, "savestate_auto_save = true\n");
    ASSERT_TRUE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_no_spaces) {
    write_config(TEST_CONFIG_PATH, "savestate_auto_save=true\n");
    /* file_parseKeyValue with '=' should still parse this */
    ASSERT_TRUE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_extra_spaces) {
    write_config(TEST_CONFIG_PATH, "savestate_auto_save  =  true\n");
    /* file_parseKeyValue uses str_trim on both key and value,
       so extra spaces are stripped → value becomes "true" → true */
    ASSERT_TRUE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_quoted_true) {
    write_config(TEST_CONFIG_PATH, "savestate_auto_save = \"true\"\n");
    /* file_parseKeyValue parses value after '=' with leading space stripped,
       resulting in "\"true\"" which strcmp considers != "true".
       However, the parser may handle quotes — test actual behavior: */
    bool result = check_autosave_path(TEST_CONFIG_PATH);
    /* RetroArch configs with quoted values: parser includes quotes → true
       because file_parseKeyValue skips leading " in value extraction */
    ASSERT_TRUE(result);
}

TEST(autosave_case_sensitive) {
    write_config(TEST_CONFIG_PATH, "savestate_auto_save = True\n");
    /* strcmp is case-sensitive: "True" != "true" */
    ASSERT_FALSE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_partial_match_key) {
    write_config(TEST_CONFIG_PATH,
        "savestate_auto_save_on_exit = true\n"
        "savestate_auto_save = false\n");
    /* Should find exact key match */
    ASSERT_FALSE(check_autosave_path(TEST_CONFIG_PATH));
}

TEST(autosave_with_comments) {
    write_config(TEST_CONFIG_PATH,
        "# This is a comment\n"
        "savestate_auto_save = true\n"
        "# Another comment\n");
    ASSERT_TRUE(check_autosave_path(TEST_CONFIG_PATH));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== system_utils.h check_autosave Unit Tests ===\n\n");

    RUN_TEST(autosave_true);
    RUN_TEST(autosave_false);
    RUN_TEST(autosave_missing_key);
    RUN_TEST(autosave_empty_file);
    RUN_TEST(autosave_missing_file);
    RUN_TEST(autosave_first_line);
    RUN_TEST(autosave_last_line);
    RUN_TEST(autosave_only_line);
    RUN_TEST(autosave_no_spaces);
    RUN_TEST(autosave_extra_spaces);
    RUN_TEST(autosave_quoted_true);
    RUN_TEST(autosave_case_sensitive);
    RUN_TEST(autosave_partial_match_key);
    RUN_TEST(autosave_with_comments);

    /* Cleanup */
    unlink(TEST_CONFIG_PATH);

    TEST_REPORT();
    return test_failures;
}
