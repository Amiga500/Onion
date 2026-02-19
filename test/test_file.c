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
#include <fcntl.h>
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

/* ---- file_readLastLine ---- */

TEST(file_readLastLine_single_line) {
    const char *tmpfile = "/tmp/onion_test_readlast.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "single line");
    fclose(fp);
    
    char result[256] = {0};
    file_readLastLine(tmpfile, result);
    ASSERT_STREQ(result, "single line");
    
    unlink(tmpfile);
}

TEST(file_readLastLine_multiple_lines) {
    const char *tmpfile = "/tmp/onion_test_readlast2.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "first line\nsecond line\nthird line");
    fclose(fp);
    
    char result[256] = {0};
    file_readLastLine(tmpfile, result);
    ASSERT_STREQ(result, "third line");
    
    unlink(tmpfile);
}

TEST(file_readLastLine_empty_file) {
    const char *tmpfile = "/tmp/onion_test_readlast3.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);
    
    char result[256] = "unchanged";
    file_readLastLine(tmpfile, result);
    ASSERT_STREQ(result, "unchanged");
    
    unlink(tmpfile);
}

TEST(file_readLastLine_with_trailing_newline) {
    const char *tmpfile = "/tmp/onion_test_readlast4.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "line1\nline2\n");
    fclose(fp);
    
    char result[256] = {0};
    file_readLastLine(tmpfile, result);
    ASSERT_STREQ(result, "line2");
    
    unlink(tmpfile);
}

/* ---- file_write ---- */

TEST(file_write_basic) {
    const char *tmpfile = "/tmp/onion_test_write.txt";
    const char *content = "test content";
    
    // Create the file first since file_write doesn't create it
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);
    
    bool result = file_write(tmpfile, content, strlen(content));
    ASSERT_TRUE(result);
    ASSERT_TRUE(exists(tmpfile));
    
    char buffer[256] = {0};
    fp = fopen(tmpfile, "r");
    ASSERT_NOT_NULL(fp);
    fread(buffer, 1, strlen(content), fp);
    fclose(fp);
    ASSERT_STREQ(buffer, content);
    
    unlink(tmpfile);
}

TEST(file_write_overwrite) {
    const char *tmpfile = "/tmp/onion_test_write2.txt";
    
    // Create file with initial content
    FILE *fp = fopen(tmpfile, "w");
    fprintf(fp, "old");
    fclose(fp);
    
    file_write(tmpfile, "new", 3);
    
    char buffer[256] = {0};
    fp = fopen(tmpfile, "r");
    fread(buffer, 1, 3, fp);
    fclose(fp);
    ASSERT_STREQ(buffer, "new");
    
    unlink(tmpfile);
}

/* ---- file_copy ---- */

TEST(file_copy_basic) {
    const char *src = "/tmp/onion_test_copy_src.txt";
    const char *dest = "/tmp/onion_test_copy_dest.txt";
    
    // Create source file
    FILE *fp = fopen(src, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "copy test content");
    fclose(fp);
    
    // Copy file
    file_copy(src, dest);
    
    // Verify destination exists and has same content
    ASSERT_TRUE(exists(dest));
    char buffer[256] = {0};
    fp = fopen(dest, "r");
    ASSERT_NOT_NULL(fp);
    fread(buffer, 1, 17, fp);
    fclose(fp);
    ASSERT_STREQ(buffer, "copy test content");
    
    // Cleanup
    unlink(src);
    unlink(dest);
}

/* ---- file_isModified ---- */

TEST(file_isModified_initially_modified) {
    const char *tmpfile = "/tmp/onion_test_modified.txt";
    
    // Create file
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "test");
    fclose(fp);
    
    // Initial check with old time should return true
    time_t old_mtime = 0;
    ASSERT_TRUE(file_isModified(tmpfile, &old_mtime));
    ASSERT_GT(old_mtime, 0);
    
    // Immediate check again should return false
    ASSERT_FALSE(file_isModified(tmpfile, &old_mtime));
    
    unlink(tmpfile);
}

TEST(file_isModified_nonexistent) {
    time_t old_mtime = 0;
    ASSERT_FALSE(file_isModified("/tmp/nonexistent_file_12345.txt", &old_mtime));
}

/* ---- file_cleanName ---- */

TEST(file_cleanName_basic) {
    char result[256];
    file_cleanName(result, "Game (USA).gba");
    ASSERT_STREQ(result, "Game");
}

TEST(file_cleanName_brackets) {
    char result[256];
    file_cleanName(result, "Game [Rev 1].rom");
    ASSERT_STREQ(result, "Game");
}

TEST(file_cleanName_no_parens) {
    char result[256];
    file_cleanName(result, "SimpleName.zip");
    ASSERT_STREQ(result, "SimpleName");
}

TEST(file_cleanName_with_underscores) {
    char result[256];
    file_cleanName(result, "Game_Name_Test.gba");
    ASSERT_STREQ(result, "Game Name Test");
}

/* ---- file_changeKeyValue ---- */

TEST(file_changeKeyValue_replaces_existing_key) {
    const char *tmpfile = "/tmp/onion_test_changekv.cfg";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "brightness=50\nvolume=80\ntheme=dark\n");
    fclose(fp);

    file_changeKeyValue(tmpfile, "volume", "volume=60");

    char result[256] = {0};
    file_parseKeyValue(tmpfile, "volume", result, '=', 0);
    ASSERT_STREQ(result, "60");

    /* other keys must be untouched */
    char r2[256] = {0};
    file_parseKeyValue(tmpfile, "brightness", r2, '=', 0);
    ASSERT_STREQ(r2, "50");

    unlink(tmpfile);
}

TEST(file_changeKeyValue_appends_missing_key) {
    const char *tmpfile = "/tmp/onion_test_changekv2.cfg";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "brightness=50\n");
    fclose(fp);

    file_changeKeyValue(tmpfile, "volume", "volume=70");

    char result[256] = {0};
    file_parseKeyValue(tmpfile, "volume", result, '=', 0);
    ASSERT_STREQ(result, "70");

    unlink(tmpfile);
}

/* Regression: last character of a non-matching line must NOT be corrupted */
TEST(file_changeKeyValue_preserves_line_without_trailing_newline) {
    const char *tmpfile = "/tmp/onion_test_changekv3.cfg";
    /* Write a file whose last line intentionally has no trailing newline */
    int fd = open(tmpfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ASSERT_TRUE(fd >= 0);
    const char *content = "key1=aaa\nkey2=bbb";
    write(fd, content, strlen(content));
    close(fd);

    /* Replace key1; key2 (no trailing newline) must survive intact */
    file_changeKeyValue(tmpfile, "key1", "key1=zzz");

    char r1[256] = {0}, r2[256] = {0};
    file_parseKeyValue(tmpfile, "key1", r1, '=', 0);
    file_parseKeyValue(tmpfile, "key2", r2, '=', 0);
    ASSERT_STREQ(r1, "zzz");
    ASSERT_STREQ(r2, "bbb"); /* was corrupted to "bb\n" before the fix */

    unlink(tmpfile);
}

/* Regression: blank lines in a config must NOT cause the following key to be
 * skipped (was caused by a spurious fscanf() call after a failed sscanf()). */
TEST(file_parseKeyValue_skips_blank_lines) {
    const char *tmpfile = "/tmp/onion_test_parsekv_blank.cfg";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "key1=aaa\n\nkey2=bbb\n");
    fclose(fp);

    char r1[256] = {0}, r2[256] = {0};
    file_parseKeyValue(tmpfile, "key1", r1, '=', 0);
    file_parseKeyValue(tmpfile, "key2", r2, '=', 0);
    ASSERT_STREQ(r1, "aaa");
    ASSERT_STREQ(r2, "bbb"); /* was empty string before the fix */

    unlink(tmpfile);
}

/* ---- file_delete_line ---- */

TEST(file_delete_line_removes_correct_line) {
    const char *tmpfile = "/tmp/onion_test_delline.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "line1\nline2\nline3\n");
    fclose(fp);

    file_delete_line(tmpfile, 2);

    char *l1 = file_read_lineN(tmpfile, 1);
    char *l2 = file_read_lineN(tmpfile, 2);
    ASSERT_NOT_NULL(l1);
    ASSERT_TRUE(strncmp(l1, "line1", 5) == 0);
    ASSERT_NOT_NULL(l2);
    ASSERT_TRUE(strncmp(l2, "line3", 5) == 0);
    free(l1);
    free(l2);

    unlink(tmpfile);
}

/* ---- file_add_line_to_beginning ---- */

TEST(file_add_line_to_beginning_prepends_line) {
    const char *tmpfile = "/tmp/onion_test_prepend.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "existing line\n");
    fclose(fp);

    file_add_line_to_beginning(tmpfile, "new first line\n");

    char *l1 = file_read_lineN(tmpfile, 1);
    char *l2 = file_read_lineN(tmpfile, 2);
    ASSERT_NOT_NULL(l1);
    ASSERT_TRUE(strncmp(l1, "new first line", 14) == 0);
    ASSERT_NOT_NULL(l2);
    ASSERT_TRUE(strncmp(l2, "existing line", 13) == 0);
    free(l1);
    free(l2);

    unlink(tmpfile);
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

    RUN_TEST(file_readLastLine_single_line);
    RUN_TEST(file_readLastLine_multiple_lines);
    RUN_TEST(file_readLastLine_empty_file);
    RUN_TEST(file_readLastLine_with_trailing_newline);

    RUN_TEST(file_write_basic);
    RUN_TEST(file_write_overwrite);

    RUN_TEST(file_copy_basic);

    RUN_TEST(file_isModified_initially_modified);
    RUN_TEST(file_isModified_nonexistent);

    RUN_TEST(file_cleanName_basic);
    RUN_TEST(file_cleanName_brackets);
    RUN_TEST(file_cleanName_no_parens);
    RUN_TEST(file_cleanName_with_underscores);

    RUN_TEST(file_changeKeyValue_replaces_existing_key);
    RUN_TEST(file_changeKeyValue_appends_missing_key);
    RUN_TEST(file_changeKeyValue_preserves_line_without_trailing_newline);

    RUN_TEST(file_parseKeyValue_skips_blank_lines);

    RUN_TEST(file_delete_line_removes_correct_line);

    RUN_TEST(file_add_line_to_beginning_prepends_line);

    TEST_REPORT();
    return test_failures;
}
