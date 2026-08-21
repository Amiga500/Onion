/**
 * @file test_config_security.c
 * @brief Security and edge-case tests for config utility functions
 *
 * Tests config path handling, format string safety, and boundary
 * conditions in key-value operations from file.h and config.h.
 *
 * Focuses on file_parseKeyValue and file_changeKeyValue which handle
 * user-controlled config files (themes, emulator settings, etc.).
 *
 * Build and run: make -f Makefile.unit test_config_security
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TEST_DIR = "/tmp/onion_test_config_sec";

static void setup_test_dir(void)
{
    mkdir(TEST_DIR, 0755);
}

static void cleanup_test_dir(void)
{
    file_remove_recursive(TEST_DIR);
}

/* ---- Tests: file_parseKeyValue ---- */

TEST(parseKV_basic_equals) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/basic.cfg", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "key1=value1\nkey2=value2\nkey3=value3\n");
    fclose(fp);

    char val[256] = {0};
    char *result = file_parseKeyValue(path, "key2", val, '=', 0);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(val, "value2");

    cleanup_test_dir();
}

TEST(parseKV_colon_divider) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/colon.cfg", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "host: localhost\nport: 8080\n");
    fclose(fp);

    char val[256] = {0};
    char *result = file_parseKeyValue(path, "port", val, ':', 0);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(val, "8080");

    cleanup_test_dir();
}

TEST(parseKV_missing_key) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/missing.cfg", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "key1=value1\n");
    fclose(fp);

    char val[256] = {0};
    char *result = file_parseKeyValue(path, "nonexistent", val, '=', 0);
    ASSERT_NULL(result);

    cleanup_test_dir();
}

TEST(parseKV_nonexistent_file) {
    char val[256] = {0};
    char *result = file_parseKeyValue("/tmp/no_such_file_cfg", "key", val, '=', 0);
    ASSERT_NULL(result);
}

TEST(parseKV_empty_value) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/empty_val.cfg", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "key1=\nkey2=something\n");
    fclose(fp);

    char val[256] = {0};
    /* empty value after = won't match the scanf pattern properly */
    char *result = file_parseKeyValue(path, "key2", val, '=', 0);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(val, "something");

    cleanup_test_dir();
}

TEST(parseKV_whitespace_in_key) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/ws.cfg", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "  key_trimmed  = value_trimmed  \n");
    fclose(fp);

    char val[256] = {0};
    char *result = file_parseKeyValue(path, "key_trimmed", val, '=', 0);
    ASSERT_NOT_NULL(result);
    /* str_trim should trim the value */
    ASSERT_STREQ(val, "value_trimmed");

    cleanup_test_dir();
}

TEST(parseKV_select_index) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/multi.cfg", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "key=first\nkey=second\nkey=third\n");
    fclose(fp);

    /* Get 2nd occurrence (index 1) */
    char val[256] = {0};
    char *result = file_parseKeyValue(path, "key", val, '=', 1);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(val, "second");

    cleanup_test_dir();
}

/* ---- Tests: file_changeKeyValue ---- */

TEST(changeKV_update_existing) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/change.cfg", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "key1=old_value\nkey2=keep_this\n");
    fclose(fp);

    file_changeKeyValue(path, "key1", "key1=new_value");

    char val[256] = {0};
    file_parseKeyValue(path, "key1", val, '=', 0);
    ASSERT_STREQ(val, "new_value");

    /* Other key should be preserved */
    char val2[256] = {0};
    file_parseKeyValue(path, "key2", val2, '=', 0);
    ASSERT_STREQ(val2, "keep_this");

    cleanup_test_dir();
}

TEST(changeKV_append_new_key) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/append.cfg", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "existing=value\n");
    fclose(fp);

    file_changeKeyValue(path, "new_key", "new_key=new_value");

    char val[256] = {0};
    file_parseKeyValue(path, "new_key", val, '=', 0);
    ASSERT_STREQ(val, "new_value");

    /* Original key should still exist */
    char val2[256] = {0};
    file_parseKeyValue(path, "existing", val2, '=', 0);
    ASSERT_STREQ(val2, "value");

    cleanup_test_dir();
}

TEST(changeKV_empty_file) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/empty.cfg", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fclose(fp);

    file_changeKeyValue(path, "key", "key=value");

    char val[256] = {0};
    file_parseKeyValue(path, "key", val, '=', 0);
    ASSERT_STREQ(val, "value");

    cleanup_test_dir();
}

/* ---- Tests: file_delete_line edge cases ---- */

TEST(delete_line_first) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/del_first.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "line1\nline2\nline3\n");
    fclose(fp);

    file_delete_line(path, 1);

    char *line1 = file_read_lineN(path, 1);
    ASSERT_NOT_NULL(line1);
    ASSERT_TRUE(strncmp(line1, "line2", 5) == 0);
    free(line1);

    cleanup_test_dir();
}

TEST(delete_line_middle) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/del_mid.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "line1\nline2\nline3\n");
    fclose(fp);

    file_delete_line(path, 2);

    char *line1 = file_read_lineN(path, 1);
    ASSERT_NOT_NULL(line1);
    ASSERT_TRUE(strncmp(line1, "line1", 5) == 0);
    free(line1);

    char *line2 = file_read_lineN(path, 2);
    ASSERT_NOT_NULL(line2);
    ASSERT_TRUE(strncmp(line2, "line3", 5) == 0);
    free(line2);

    cleanup_test_dir();
}

TEST(delete_line_last) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/del_last.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "line1\nline2\nline3\n");
    fclose(fp);

    file_delete_line(path, 3);

    char *line3 = file_read_lineN(path, 3);
    ASSERT_NULL(line3);

    char *line2 = file_read_lineN(path, 2);
    ASSERT_NOT_NULL(line2);
    ASSERT_TRUE(strncmp(line2, "line2", 5) == 0);
    free(line2);

    cleanup_test_dir();
}

/* ---- Tests: file_add_line_to_beginning ---- */

TEST(add_line_beginning_normal) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/prepend.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "existing line\n");
    fclose(fp);

    file_add_line_to_beginning(path, "new first line\n");

    char *line1 = file_read_lineN(path, 1);
    ASSERT_NOT_NULL(line1);
    ASSERT_TRUE(strncmp(line1, "new first line", 14) == 0);
    free(line1);

    char *line2 = file_read_lineN(path, 2);
    ASSERT_NOT_NULL(line2);
    ASSERT_TRUE(strncmp(line2, "existing line", 13) == 0);
    free(line2);

    cleanup_test_dir();
}

/* ---- Tests: file_cleanName ---- */

TEST(cleanName_basic) {
    char out[STR_MAX];
    file_cleanName(out, "my_game_rom.gba");
    /* Should replace _ with space and remove extension */
    ASSERT_STREQ(out, "my game rom");
}

TEST(cleanName_no_underscores) {
    char out[STR_MAX];
    file_cleanName(out, "SimpleGame.sfc");
    ASSERT_STREQ(out, "SimpleGame");
}

TEST(cleanName_with_parens) {
    char out[STR_MAX];
    file_cleanName(out, "Game_(USA)_[v1.0].gba");
    /* Underscores → spaces, extension removed, parentheses removed */
    ASSERT_STREQ(out, "Game");
}

TEST(cleanName_empty_string) {
    char out[STR_MAX] = "placeholder";
    file_cleanName(out, "");
    /* Empty input should produce empty output */
    ASSERT_EQ(strlen(out), 0);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== Config & File Security Tests ===\n\n");

    /* file_parseKeyValue */
    RUN_TEST(parseKV_basic_equals);
    RUN_TEST(parseKV_colon_divider);
    RUN_TEST(parseKV_missing_key);
    RUN_TEST(parseKV_nonexistent_file);
    RUN_TEST(parseKV_empty_value);
    RUN_TEST(parseKV_whitespace_in_key);
    RUN_TEST(parseKV_select_index);

    /* file_changeKeyValue */
    RUN_TEST(changeKV_update_existing);
    RUN_TEST(changeKV_append_new_key);
    RUN_TEST(changeKV_empty_file);

    /* file_delete_line */
    RUN_TEST(delete_line_first);
    RUN_TEST(delete_line_middle);
    RUN_TEST(delete_line_last);

    /* file_add_line_to_beginning */
    RUN_TEST(add_line_beginning_normal);

    /* file_cleanName */
    RUN_TEST(cleanName_basic);
    RUN_TEST(cleanName_no_underscores);
    RUN_TEST(cleanName_with_parens);
    RUN_TEST(cleanName_empty_string);

    TEST_REPORT();
    return test_failures;
}
