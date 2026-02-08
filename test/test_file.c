/**
 * @file test_file.c
 * @brief Unit tests for src/common/utils/file.c
 *
 * Tests file utility functions: file_basename, file_removeExtension,
 * file_dirname, file_getExtension, file_resolvePath, exists, is_file, is_dir.
 *
 * Build and run: make -f Makefile.unit test_file && ./build_test/test_file
 */

#include "onion_test.h"
#include "../src/common/utils/file.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- file_basename ---- */

TEST(file_basename_simple) {
    const char *result = file_basename("/path/to/file.txt");
    ASSERT_STREQ(result, "file.txt");
}

TEST(file_basename_no_dir) {
    const char *result = file_basename("file.txt");
    ASSERT_STREQ(result, "file.txt");
}

TEST(file_basename_trailing_slash) {
    const char *result = file_basename("/path/to/dir/");
    ASSERT_STREQ(result, "");
}

TEST(file_basename_root) {
    const char *result = file_basename("/");
    ASSERT_STREQ(result, "");
}

TEST(file_basename_multiple_slashes) {
    const char *result = file_basename("/path//to///file.txt");
    ASSERT_STREQ(result, "file.txt");
}

/* ---- file_getExtension ---- */

TEST(file_getExtension_basic) {
    const char *ext = file_getExtension("file.txt");
    ASSERT_STREQ(ext, "txt");
}

TEST(file_getExtension_multiple_dots) {
    const char *ext = file_getExtension("archive.tar.gz");
    ASSERT_STREQ(ext, "gz");
}

TEST(file_getExtension_no_extension) {
    const char *ext = file_getExtension("file");
    ASSERT_STREQ(ext, "");
}

TEST(file_getExtension_dot_only) {
    const char *ext = file_getExtension("file.");
    ASSERT_STREQ(ext, "");
}

TEST(file_getExtension_hidden_file) {
    const char *ext = file_getExtension(".hidden");
    ASSERT_STREQ(ext, "");
}

TEST(file_getExtension_hidden_with_ext) {
    const char *ext = file_getExtension(".hidden.txt");
    ASSERT_STREQ(ext, "txt");
}

TEST(file_getExtension_path) {
    const char *ext = file_getExtension("/path/to/file.png");
    ASSERT_STREQ(ext, "png");
}

/* ---- file_removeExtension ---- */

TEST(file_removeExtension_basic) {
    char *result = file_removeExtension("game.gba");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "game");
    free(result);
}

TEST(file_removeExtension_no_ext) {
    char *result = file_removeExtension("noext");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "noext");
    free(result);
}

TEST(file_removeExtension_multiple_dots) {
    char *result = file_removeExtension("archive.tar.gz");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "archive.tar");
    free(result);
}

TEST(file_removeExtension_null) {
    char *result = file_removeExtension(NULL);
    ASSERT_NULL(result);
}

TEST(file_removeExtension_path) {
    char *result = file_removeExtension("/path/to/game.rom");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/path/to/game");
    free(result);
}

TEST(file_removeExtension_space_after_dot) {
    // Special case: ". " is not considered an extension
    char *result = file_removeExtension("file. x");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "file. x");
    free(result);
}

/* ---- file_dirname ---- */

TEST(file_dirname_basic) {
    char *result = file_dirname("/path/to/file.txt");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/path/to");
    free(result);
}

TEST(file_dirname_root) {
    char *result = file_dirname("/file.txt");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "");
    free(result);
}

TEST(file_dirname_no_slash) {
    char *result = file_dirname("file.txt");
    ASSERT_NULL(result);
}

TEST(file_dirname_trailing_slash) {
    char *result = file_dirname("/path/to/dir/");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/path/to/dir");
    free(result);
}

TEST(file_dirname_multiple_levels) {
    char *result = file_dirname("/a/b/c/d/e/f.txt");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/a/b/c/d/e");
    free(result);
}

/* ---- file_resolvePath ---- */

TEST(file_resolvePath_basic) {
    char *result = file_resolvePath("/path/to/file.txt");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/path/to/file.txt");
    free(result);
}

TEST(file_resolvePath_parent_dir) {
    char *result = file_resolvePath("/path/to/../file.txt");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/path/file.txt");
    free(result);
}

TEST(file_resolvePath_multiple_parent) {
    char *result = file_resolvePath("/a/b/c/../../d/file.txt");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/a/d/file.txt");
    free(result);
}

TEST(file_resolvePath_current_dir) {
    char *result = file_resolvePath("/path/./to/./file.txt");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/path/to/file.txt");
    free(result);
}

TEST(file_resolvePath_complex) {
    char *result = file_resolvePath("/mnt/SDCARD/Emu/GBA/../../Roms/GBA/game.gba");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/mnt/SDCARD/Roms/GBA/game.gba");
    free(result);
}

TEST(file_resolvePath_null) {
    char *result = file_resolvePath(NULL);
    ASSERT_NULL(result);
}

TEST(file_resolvePath_root) {
    char *result = file_resolvePath("/");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/");
    free(result);
}

TEST(file_resolvePath_parent_at_root) {
    // Going above root should stop at root
    char *result = file_resolvePath("/../../../file.txt");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/file.txt");
    free(result);
}

/* ---- exists, is_file, is_dir ---- */

TEST(exists_and_is_file) {
    // Create a temporary file
    const char *tmpfile = "/tmp/onion_test_file.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "test");
    fclose(fp);
    
    // Test exists and is_file
    ASSERT_TRUE(exists(tmpfile));
    ASSERT_TRUE(is_file(tmpfile));
    ASSERT_FALSE(is_dir(tmpfile));
    
    // Cleanup
    unlink(tmpfile);
}

TEST(exists_and_is_dir) {
    // Create a temporary directory
    const char *tmpdir = "/tmp/onion_test_dir";
    mkdir(tmpdir, 0755);
    
    // Test exists and is_dir
    ASSERT_TRUE(exists(tmpdir));
    ASSERT_TRUE(is_dir(tmpdir));
    ASSERT_FALSE(is_file(tmpdir));
    
    // Cleanup
    rmdir(tmpdir);
}

TEST(not_exists) {
    const char *nonexistent = "/tmp/nonexistent_file_12345.txt";
    ASSERT_FALSE(exists(nonexistent));
    ASSERT_FALSE(is_file(nonexistent));
    ASSERT_FALSE(is_dir(nonexistent));
}

/* ---- mkdirs ---- */

TEST(mkdirs_creates_directory) {
    const char *testdir = "/tmp/onion_test_mkdir/sub1/sub2";
    
    // Clean up if it exists from previous test
    file_remove_recursive("/tmp/onion_test_mkdir");
    
    // Should return true when creating new directories
    ASSERT_TRUE(mkdirs(testdir));
    ASSERT_TRUE(exists(testdir));
    ASSERT_TRUE(is_dir(testdir));
    
    // Should return false when directory already exists
    ASSERT_FALSE(mkdirs(testdir));
    
    // Cleanup
    file_remove_recursive("/tmp/onion_test_mkdir");
}

TEST(mkdirs_empty_path) {
    ASSERT_FALSE(mkdirs(""));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== file.c Unit Tests ===\n\n");

    RUN_TEST(file_basename_simple);
    RUN_TEST(file_basename_no_dir);
    RUN_TEST(file_basename_trailing_slash);
    RUN_TEST(file_basename_root);
    RUN_TEST(file_basename_multiple_slashes);

    RUN_TEST(file_getExtension_basic);
    RUN_TEST(file_getExtension_multiple_dots);
    RUN_TEST(file_getExtension_no_extension);
    RUN_TEST(file_getExtension_dot_only);
    RUN_TEST(file_getExtension_hidden_file);
    RUN_TEST(file_getExtension_hidden_with_ext);
    RUN_TEST(file_getExtension_path);

    RUN_TEST(file_removeExtension_basic);
    RUN_TEST(file_removeExtension_no_ext);
    RUN_TEST(file_removeExtension_multiple_dots);
    RUN_TEST(file_removeExtension_null);
    RUN_TEST(file_removeExtension_path);
    RUN_TEST(file_removeExtension_space_after_dot);

    RUN_TEST(file_dirname_basic);
    RUN_TEST(file_dirname_root);
    RUN_TEST(file_dirname_no_slash);
    RUN_TEST(file_dirname_trailing_slash);
    RUN_TEST(file_dirname_multiple_levels);

    RUN_TEST(file_resolvePath_basic);
    RUN_TEST(file_resolvePath_parent_dir);
    RUN_TEST(file_resolvePath_multiple_parent);
    RUN_TEST(file_resolvePath_current_dir);
    RUN_TEST(file_resolvePath_complex);
    RUN_TEST(file_resolvePath_null);
    RUN_TEST(file_resolvePath_root);
    RUN_TEST(file_resolvePath_parent_at_root);

    RUN_TEST(exists_and_is_file);
    RUN_TEST(exists_and_is_dir);
    RUN_TEST(not_exists);

    RUN_TEST(mkdirs_creates_directory);
    RUN_TEST(mkdirs_empty_path);

    TEST_REPORT();
    return test_failures;
}
