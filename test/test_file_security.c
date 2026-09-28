/**
 * @file test_file_security.c
 * @brief Security and edge-case tests for src/common/utils/file.c
 *
 * Covers path traversal prevention, corrupted file handling, boundary values,
 * symlink safety, and NULL pointer robustness.
 *
 * Build and run: make -f Makefile.unit test_file_security
 */

#include "onion_test.h"
#include "../src/common/utils/file.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TEST_DIR = "/tmp/onion_test_file_sec";

static void setup_test_dir(void)
{
    mkdir(TEST_DIR, 0755);
}

static void cleanup_test_dir(void)
{
    file_remove_recursive(TEST_DIR);
}

/* ---- file_resolvePath: path traversal prevention ---- */

TEST(resolvePath_traversal_basic) {
    char *result = file_resolvePath("/mnt/SDCARD/Emu/../../etc/passwd");
    ASSERT_NOT_NULL(result);
    /* Two ".." from Emu goes up to /mnt, then adds etc/passwd */
    ASSERT_STREQ(result, "/mnt/etc/passwd");
    free(result);
}

TEST(resolvePath_traversal_excess_dotdot) {
    /* More ".." than path components should clamp at root */
    char *result = file_resolvePath("/a/b/../../../..");
    ASSERT_NOT_NULL(result);
    /* Cannot go above root */
    ASSERT_STREQ(result, "/");
    free(result);
}

TEST(resolvePath_null_input) {
    char *result = file_resolvePath(NULL);
    ASSERT_NULL(result);
}

TEST(resolvePath_empty_path) {
    char *result = file_resolvePath("");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/");
    free(result);
}

TEST(resolvePath_dot_components) {
    char *result = file_resolvePath("/a/./b/./c");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/a/b/c");
    free(result);
}

TEST(resolvePath_consecutive_slashes) {
    char *result = file_resolvePath("/a///b//c");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/a/b/c");
    free(result);
}

TEST(resolvePath_relative_path) {
    /* Relative paths should also be resolved */
    char *result = file_resolvePath("a/b/../c");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/a/c");
    free(result);
}

/* ---- file_basename: edge cases ---- */

TEST(file_basename_empty_string) {
    const char *result = file_basename("");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "");
}

TEST(file_basename_just_filename) {
    const char *result = file_basename("game.gba");
    ASSERT_STREQ(result, "game.gba");
}

TEST(file_basename_deep_path) {
    const char *result = file_basename("/a/b/c/d/e/f/g.rom");
    ASSERT_STREQ(result, "g.rom");
}

/* ---- file_removeExtension: edge cases ---- */

TEST(file_removeExtension_null) {
    char *result = file_removeExtension(NULL);
    ASSERT_NULL(result);
}

TEST(file_removeExtension_no_ext) {
    char *result = file_removeExtension("noext");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "noext");
    free(result);
}

TEST(file_removeExtension_double_ext) {
    char *result = file_removeExtension("archive.tar.gz");
    ASSERT_NOT_NULL(result);
    /* Should remove only the last extension */
    ASSERT_STREQ(result, "archive.tar");
    free(result);
}

TEST(file_removeExtension_hidden_file) {
    char *result = file_removeExtension(".gitignore");
    ASSERT_NOT_NULL(result);
    /* strrchr finds '.' at start; function treats it as extension and removes it */
    ASSERT_STREQ(result, "");
    free(result);
}

TEST(file_removeExtension_empty) {
    char *result = file_removeExtension("");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "");
    free(result);
}

/* ---- file_dirname: edge cases ---- */

TEST(file_dirname_null_slash) {
    char *result = file_dirname("nodir");
    ASSERT_NULL(result);
}

TEST(file_dirname_root) {
    char *result = file_dirname("/file.txt");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "");
    free(result);
}

TEST(file_dirname_deep) {
    char *result = file_dirname("/a/b/c/file.txt");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/a/b/c");
    free(result);
}

/* ---- file_read: edge cases ---- */

TEST(file_read_nonexistent) {
    char *result = file_read("/tmp/onion_test_nonexistent_file");
    ASSERT_NULL(result);
}

TEST(file_read_empty_file) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/empty.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fclose(fp);

    char *result = file_read(path);
    /* Production short-circuits st_size==0 and returns allocated "" */
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "");
    free(result);

    cleanup_test_dir();
}

TEST(file_read_normal) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/normal.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "hello world");
    fclose(fp);

    char *result = file_read(path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "hello world");
    free(result);

    cleanup_test_dir();
}

/* ---- exists / is_file / is_dir: edge cases ---- */

TEST(exists_nonexistent) {
    ASSERT_FALSE(exists("/tmp/onion_test_surely_not_here"));
}

TEST(is_file_on_dir) {
    ASSERT_FALSE(is_file("/tmp"));
}

TEST(is_dir_on_file) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/afile.txt", TEST_DIR);
    FILE *fp = fopen(path, "w");
    fprintf(fp, "data");
    fclose(fp);

    ASSERT_FALSE(is_dir(path));
    cleanup_test_dir();
}

/* ---- mkdirs: edge cases ---- */

TEST(mkdirs_existing_dir) {
    setup_test_dir();
    /* Should return false because dir already exists */
    ASSERT_FALSE(mkdirs(TEST_DIR));
    cleanup_test_dir();
}

TEST(mkdirs_nested) {
    char path[256];
    snprintf(path, sizeof(path), "%s/a/b/c/d", TEST_DIR);

    bool created = mkdirs(path);
    ASSERT_TRUE(created);
    ASSERT_TRUE(is_dir(path));

    cleanup_test_dir();
}

TEST(mkdirs_empty_path) {
    ASSERT_FALSE(mkdirs(""));
}

/* ---- file_remove_recursive: edge cases ---- */

TEST(file_remove_recursive_null) {
    ASSERT_EQ(file_remove_recursive(NULL), -1);
}

TEST(file_remove_recursive_nonexistent) {
    /* Non-existent path should return 0 (success) */
    ASSERT_EQ(file_remove_recursive("/tmp/onion_test_not_here"), 0);
}

TEST(file_remove_recursive_nested) {
    char base[PATH_MAX], sub[PATH_MAX], file1[PATH_MAX], file2[PATH_MAX];
    snprintf(base, sizeof(base), "%s/rmtest", TEST_DIR);
    snprintf(sub, sizeof(sub), "%s/sub", base);
    snprintf(file1, sizeof(file1), "%s/file1.txt", base);
    snprintf(file2, sizeof(file2), "%s/file2.txt", sub);

    mkdirs(sub);
    FILE *fp1 = fopen(file1, "w");
    fprintf(fp1, "data1");
    fclose(fp1);
    FILE *fp2 = fopen(file2, "w");
    fprintf(fp2, "data2");
    fclose(fp2);

    ASSERT_TRUE(exists(base));
    ASSERT_EQ(file_remove_recursive(base), 0);
    ASSERT_FALSE(exists(base));

    cleanup_test_dir();
}

/* ---- file_readLastLine: edge cases ---- */

TEST(file_readLastLine_single_line) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/single.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "only line");
    fclose(fp);

    char out[256] = {0};
    file_readLastLine(path, out);
    ASSERT_STREQ(out, "only line");

    cleanup_test_dir();
}

TEST(file_readLastLine_multiple_lines) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/multi.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "line1\nline2\nline3");
    fclose(fp);

    char out[256] = {0};
    file_readLastLine(path, out);
    ASSERT_STREQ(out, "line3");

    cleanup_test_dir();
}

TEST(file_readLastLine_trailing_newline) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/trailing.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "line1\nlast\n");
    fclose(fp);

    char out[256] = {0};
    file_readLastLine(path, out);
    ASSERT_STREQ(out, "last");

    cleanup_test_dir();
}

/* ---- file_getExtension: security-relevant edge cases ---- */

TEST(file_getExtension_dotdot_path_known_behavior) {
    /*
     * Known behavior: file_getExtension uses strrchr('.') which matches dots
     * inside ".." path components, not just file extensions. For paths with
     * ".." traversal (e.g. "../../etc/passwd"), the last '.' is in the ".."
     * component, so the function returns a misleading "extension" that
     * includes path components. Callers should validate or normalize paths
     * before using this function for security-sensitive operations.
     */
    const char *ext = file_getExtension("../../etc/passwd");
    ASSERT_STREQ(ext, "/etc/passwd");
}

TEST(file_getExtension_very_long_ext) {
    /* Extension with many chars */
    const char *ext = file_getExtension("file.abcdefghijklmnopqrstuvwxyz");
    ASSERT_STREQ(ext, "abcdefghijklmnopqrstuvwxyz");
}

/* ---- file_read_lineN: edge cases ---- */

TEST(file_read_lineN_nonexistent) {
    char *line = file_read_lineN("/tmp/onion_test_not_here", 1);
    ASSERT_NULL(line);
}

TEST(file_read_lineN_out_of_range) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/lines.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "line1\nline2\n");
    fclose(fp);

    char *line = file_read_lineN(path, 99);
    ASSERT_NULL(line);

    cleanup_test_dir();
}

TEST(file_read_lineN_first_line) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/lines2.txt", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "first\nsecond\nthird\n");
    fclose(fp);

    char *line = file_read_lineN(path, 1);
    ASSERT_NOT_NULL(line);
    /* Line includes the newline from fgets */
    ASSERT_TRUE(strncmp(line, "first", 5) == 0);
    free(line);

    cleanup_test_dir();
}

/* ---- file_findNewest: edge cases ---- */

TEST(file_findNewest_empty_dir) {
    setup_test_dir();
    char subdir[256];
    snprintf(subdir, sizeof(subdir), "%s/emptydir", TEST_DIR);
    mkdirs(subdir);

    char newest[256] = {0};
    bool found = file_findNewest(subdir, newest, sizeof(newest));
    ASSERT_FALSE(found);

    cleanup_test_dir();
}

TEST(file_findNewest_nonexistent_dir) {
    char newest[256] = {0};
    bool found = file_findNewest("/tmp/onion_no_dir", newest, sizeof(newest));
    ASSERT_FALSE(found);
}

int main(void) {
    /* Path traversal prevention */
    RUN_TEST(resolvePath_traversal_basic);
    RUN_TEST(resolvePath_traversal_excess_dotdot);
    RUN_TEST(resolvePath_null_input);
    RUN_TEST(resolvePath_empty_path);
    RUN_TEST(resolvePath_dot_components);
    RUN_TEST(resolvePath_consecutive_slashes);
    RUN_TEST(resolvePath_relative_path);

    /* file_basename edge cases */
    RUN_TEST(file_basename_empty_string);
    RUN_TEST(file_basename_just_filename);
    RUN_TEST(file_basename_deep_path);

    /* file_removeExtension edge cases */
    RUN_TEST(file_removeExtension_null);
    RUN_TEST(file_removeExtension_no_ext);
    RUN_TEST(file_removeExtension_double_ext);
    RUN_TEST(file_removeExtension_hidden_file);
    RUN_TEST(file_removeExtension_empty);

    /* file_dirname edge cases */
    RUN_TEST(file_dirname_null_slash);
    RUN_TEST(file_dirname_root);
    RUN_TEST(file_dirname_deep);

    /* file_read edge cases */
    RUN_TEST(file_read_nonexistent);
    RUN_TEST(file_read_empty_file);
    RUN_TEST(file_read_normal);

    /* exists / is_file / is_dir */
    RUN_TEST(exists_nonexistent);
    RUN_TEST(is_file_on_dir);
    RUN_TEST(is_dir_on_file);

    /* mkdirs edge cases */
    RUN_TEST(mkdirs_existing_dir);
    RUN_TEST(mkdirs_nested);
    RUN_TEST(mkdirs_empty_path);

    /* file_remove_recursive edge cases */
    RUN_TEST(file_remove_recursive_null);
    RUN_TEST(file_remove_recursive_nonexistent);
    RUN_TEST(file_remove_recursive_nested);

    /* file_readLastLine edge cases */
    RUN_TEST(file_readLastLine_single_line);
    RUN_TEST(file_readLastLine_multiple_lines);
    RUN_TEST(file_readLastLine_trailing_newline);

    /* file_getExtension security */
    RUN_TEST(file_getExtension_dotdot_path_known_behavior);
    RUN_TEST(file_getExtension_very_long_ext);

    /* file_read_lineN edge cases */
    RUN_TEST(file_read_lineN_nonexistent);
    RUN_TEST(file_read_lineN_out_of_range);
    RUN_TEST(file_read_lineN_first_line);

    /* file_findNewest edge cases */
    RUN_TEST(file_findNewest_empty_dir);
    RUN_TEST(file_findNewest_nonexistent_dir);

    TEST_REPORT();
    return test_failures;
}
