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
#include "../src/common/utils/flags.h"
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

TEST(file_removeExtension_trailing_dot) {
    // Regression: "file." must not cause out-of-bounds read at *(lastExt + 2)
    char *result = file_removeExtension("file.");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "file.");
    free(result);
}

TEST(file_removeExtension_single_char_ext) {
    // Single-char extensions are intentionally kept (avoids stripping "Game 1.3")
    char *result = file_removeExtension("file.a");
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "file.a");
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

TEST(file_readLastLine_tiny_file) {
    const char *tmpfile = "/tmp/onion_test_readlast_tiny.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "hi");
    fclose(fp);
    
    char result[256] = {0};
    file_readLastLine(tmpfile, result);
    ASSERT_STREQ(result, "hi");
    
    unlink(tmpfile);
}

TEST(file_readLastLine_one_byte) {
    const char *tmpfile = "/tmp/onion_test_readlast_1byte.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "x");
    fclose(fp);
    
    char result[256] = {0};
    file_readLastLine(tmpfile, result);
    ASSERT_STREQ(result, "x");
    
    unlink(tmpfile);
}

TEST(file_readLastLine_exact_254_bytes) {
    const char *tmpfile = "/tmp/onion_test_readlast_254.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    /* Write 246 bytes total: "first\n" (6 bytes) + 240 'A' chars */
    fprintf(fp, "first\n");
    for (int i = 0; i < 240; i++) fputc('A', fp);
    fclose(fp);
    
    char result[256] = {0};
    file_readLastLine(tmpfile, result);
    /* Should not crash - the fseek underflow bug would cause UB here */
    ASSERT_EQ(strlen(result), 240);
    ASSERT_EQ(result[0], 'A');
    
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

/* ---- file_isLocked ---- */

TEST(file_isLocked_existing_file) {
    const char *tmpfile = "/tmp/onion_test_locked.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    /* An existing readable file must not report itself as locked */
    ASSERT_FALSE(file_isLocked(tmpfile));

    unlink(tmpfile);
}

TEST(file_isLocked_uncreateable_path) {
    /* A path inside a non-existent directory cannot be opened/created */
    ASSERT_TRUE(file_isLocked("/tmp/nonexistent_dir_xyz/file.txt"));
}

/* ---- file_path_relative_to ---- */

TEST(file_path_relative_to_same_dir) {
    /* Create two real paths under /tmp so realpath() works */
    mkdir("/tmp/onion_relpath_test", 0755);
    FILE *fp = fopen("/tmp/onion_relpath_test/file.txt", "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    char out[PATH_MAX] = {0};
    bool ok = file_path_relative_to(out, "/tmp/onion_relpath_test",
                                    "/tmp/onion_relpath_test/file.txt");
    ASSERT_TRUE(ok);
    ASSERT_STREQ(out, "file.txt");

    unlink("/tmp/onion_relpath_test/file.txt");
    rmdir("/tmp/onion_relpath_test");
}

TEST(file_path_relative_to_subdirectory) {
    mkdir("/tmp/onion_rp_base", 0755);
    mkdir("/tmp/onion_rp_base/sub", 0755);
    FILE *fp = fopen("/tmp/onion_rp_base/sub/file.txt", "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    char out[PATH_MAX] = {0};
    bool ok = file_path_relative_to(out, "/tmp/onion_rp_base",
                                    "/tmp/onion_rp_base/sub/file.txt");
    ASSERT_TRUE(ok);
    ASSERT_STREQ(out, "sub/file.txt");

    unlink("/tmp/onion_rp_base/sub/file.txt");
    rmdir("/tmp/onion_rp_base/sub");
    rmdir("/tmp/onion_rp_base");
}

TEST(file_path_relative_to_nonexistent) {
    /* Both paths must exist for realpath() to succeed */
    char out[PATH_MAX] = {0};
    bool ok = file_path_relative_to(out, "/tmp/nonexistent_dir_abc",
                                    "/tmp/nonexistent_dir_abc/file.txt");
    ASSERT_FALSE(ok);
}

TEST(file_path_relative_to_shared_prefix) {
    /* Paths that share a common prefix but diverge mid-component */
    mkdir("/tmp/onion_rp_abc", 0755);
    mkdir("/tmp/onion_rp_abcdef", 0755);
    FILE *fp = fopen("/tmp/onion_rp_abcdef/file.txt", "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    char out[PATH_MAX] = {0};
    bool ok = file_path_relative_to(out, "/tmp/onion_rp_abc",
                                    "/tmp/onion_rp_abcdef/file.txt");
    ASSERT_TRUE(ok);
    ASSERT_STREQ(out, "../onion_rp_abcdef/file.txt");

    unlink("/tmp/onion_rp_abcdef/file.txt");
    rmdir("/tmp/onion_rp_abcdef");
    rmdir("/tmp/onion_rp_abc");
}

/* ---- file_open_ensure_path ---- */

TEST(file_open_ensure_path_creates_dirs) {
    const char *deep = "/tmp/onion_oep/a/b/c/file.txt";
    file_remove_recursive("/tmp/onion_oep");

    FILE *fp = file_open_ensure_path(deep, "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    ASSERT_TRUE(exists(deep));

    file_remove_recursive("/tmp/onion_oep");
}

/* ---- file_findNewest ---- */

TEST(file_findNewest_basic) {
    ASSERT_EQ(file_remove_recursive("/tmp/onion_newest"), 0);
    mkdir("/tmp/onion_newest", 0755);

    /* Create both files, then touch the second one again after a 1-second delay
     * so its mtime is strictly greater on filesystems with 1-second resolution. */
    FILE *fp = fopen("/tmp/onion_newest/a.txt", "w");
    ASSERT_NOT_NULL(fp);
    fputs("a", fp);
    fclose(fp);

    fp = fopen("/tmp/onion_newest/b.txt", "w");
    ASSERT_NOT_NULL(fp);
    fputs("b", fp);
    fclose(fp);

    sleep(1);

    /* Re-write b.txt to guarantee its mtime > a.txt's mtime */
    fp = fopen("/tmp/onion_newest/b.txt", "w");
    ASSERT_NOT_NULL(fp);
    fputs("b", fp);
    fclose(fp);

    char newest[256] = {0};
    bool found = file_findNewest("/tmp/onion_newest", newest, sizeof(newest));
    ASSERT_TRUE(found);
    ASSERT_STREQ(newest, "b.txt");

    unlink("/tmp/onion_newest/a.txt");
    unlink("/tmp/onion_newest/b.txt");
    rmdir("/tmp/onion_newest");
}

TEST(file_findNewest_empty_dir) {
    mkdir("/tmp/onion_newest_empty", 0755);
    char newest[256] = {0};
    bool found = file_findNewest("/tmp/onion_newest_empty", newest, sizeof(newest));
    ASSERT_FALSE(found);
    rmdir("/tmp/onion_newest_empty");
}

TEST(file_findNewest_nonexistent_dir) {
    char newest[256] = {0};
    bool found = file_findNewest("/tmp/nonexistent_dir_987654", newest, sizeof(newest));
    ASSERT_FALSE(found);
}

/* ---- file_remove_recursive ---- */

TEST(file_remove_recursive_removes_tree) {
    mkdir("/tmp/onion_rmrec", 0755);
    mkdir("/tmp/onion_rmrec/sub", 0755);
    FILE *fp = fopen("/tmp/onion_rmrec/a.txt", "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);
    fp = fopen("/tmp/onion_rmrec/sub/b.txt", "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    int ret = file_remove_recursive("/tmp/onion_rmrec");
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(exists("/tmp/onion_rmrec"));
}

TEST(file_remove_recursive_null) {
    ASSERT_EQ(file_remove_recursive(NULL), -1);
}

TEST(file_remove_recursive_nonexistent) {
    /* Non-existent path must return 0 without error */
    ASSERT_EQ(file_remove_recursive("/tmp/nonexistent_xyz_998877"), 0);
}

/* ---- file_read_lineN (direct) ---- */

TEST(file_read_lineN_first_line) {
    const char *tmpfile = "/tmp/onion_readln.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "line1\nline2\nline3\n");
    fclose(fp);

    char *line = file_read_lineN(tmpfile, 1);
    ASSERT_NOT_NULL(line);
    ASSERT_TRUE(strncmp(line, "line1", 5) == 0);
    free(line);

    unlink(tmpfile);
}

TEST(file_read_lineN_last_line) {
    const char *tmpfile = "/tmp/onion_readln2.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "alpha\nbeta\ngamma\n");
    fclose(fp);

    char *line = file_read_lineN(tmpfile, 3);
    ASSERT_NOT_NULL(line);
    ASSERT_TRUE(strncmp(line, "gamma", 5) == 0);
    free(line);

    unlink(tmpfile);
}

TEST(file_read_lineN_out_of_range) {
    const char *tmpfile = "/tmp/onion_readln3.txt";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "only one line\n");
    fclose(fp);

    char *line = file_read_lineN(tmpfile, 99);
    ASSERT_NULL(line);

    unlink(tmpfile);
}

/* ---- file_parseKeyValue (extra) ---- */

TEST(file_parseKeyValue_colon_divider) {
    const char *tmpfile = "/tmp/onion_parsekv_colon.cfg";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "host: localhost\nport: 8080\n");
    fclose(fp);

    char result[256] = {0};
    file_parseKeyValue(tmpfile, "port", result, ':', 0);
    ASSERT_STREQ(result, "8080");

    unlink(tmpfile);
}

TEST(file_parseKeyValue_key_not_found) {
    const char *tmpfile = "/tmp/onion_parsekv_miss.cfg";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "brightness=50\n");
    fclose(fp);

    char result[256] = {0};
    char *ret = file_parseKeyValue(tmpfile, "missing_key", result, '=', 0);
    ASSERT_NULL(ret);

    unlink(tmpfile);
}

TEST(file_parseKeyValue_select_index) {
    /* When a key appears multiple times, select_index picks which occurrence */
    const char *tmpfile = "/tmp/onion_parsekv_idx.cfg";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "plugin=alpha\nplugin=beta\nplugin=gamma\n");
    fclose(fp);

    char r0[256] = {0}, r1[256] = {0}, r2[256] = {0};
    file_parseKeyValue(tmpfile, "plugin", r0, '=', 0);
    file_parseKeyValue(tmpfile, "plugin", r1, '=', 1);
    file_parseKeyValue(tmpfile, "plugin", r2, '=', 2);
    ASSERT_STREQ(r0, "alpha");
    ASSERT_STREQ(r1, "beta");
    ASSERT_STREQ(r2, "gamma");

    unlink(tmpfile);
}

/* ---- file_cleanName (extra) ---- */

TEST(file_cleanName_numbered_prefix) {
    /* "01. Name.rom" → numbered prefix stripped, extension removed */
    char result[256];
    file_cleanName(result, "01. Super Mario.sfc");
    ASSERT_STREQ(result, "Super Mario");
}

/* ---- file_write (extra) ---- */

TEST(file_write_nonexistent_file) {
    /* file_write opens with O_WRONLY, so it fails for a nonexistent path */
    bool ret = file_write("/tmp/nonexistent_write_test_xyz.txt", "data", 4);
    ASSERT_FALSE(ret);
}

/* ---- file_copy (extra) ---- */

TEST(file_copy_nonexistent_src) {
    /* Must not crash when source doesn't exist */
    file_copy("/tmp/nonexistent_src_xyz.txt", "/tmp/onion_copy_dest.txt");
    /* Destination must NOT have been created */
    ASSERT_FALSE(exists("/tmp/onion_copy_dest.txt"));
}

/* ---- file_read ---- */

TEST(file_read_basic) {
    const char *tmpfile = "/tmp/onion_test_file_read.txt";
    const char *content = "hello file_read";

    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fputs(content, fp);
    fclose(fp);

    char *result = file_read(tmpfile);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, content);
    free(result);
    unlink(tmpfile);
}

TEST(file_read_nonexistent) {
    char *result = file_read("/tmp/no_such_file_xyz_12345.txt");
    ASSERT_NULL(result);
}

TEST(file_read_empty_file) {
    const char *tmpfile = "/tmp/onion_test_file_read_empty.txt";

    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    /* file_read returns NULL for an empty file (st_size == 0 → total <= 0) */
    char *result = file_read(tmpfile);
    ASSERT_NULL(result);
    unlink(tmpfile);
}

TEST(file_read_multiline) {
    const char *tmpfile = "/tmp/onion_test_file_read_multi.txt";

    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "line1\nline2\nline3");
    fclose(fp);

    char *result = file_read(tmpfile);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "line1\nline2\nline3");
    free(result);
    unlink(tmpfile);
}

/* ---- flag_set / flag_get ---- */

TEST(flag_set_creates_file_with_correct_permissions) {
    const char *key = "onion_test_flag";
    remove("/tmp/onion_test_flag");

    flag_set("/tmp/", key, true);

    /* Flag file must exist */
    ASSERT_TRUE(flag_get("/tmp/", key));

    /* Verify permissions are 0644 (rw-r--r--), not the old decimal 777 (01411) */
    struct stat st;
    ASSERT_EQ(stat("/tmp/onion_test_flag", &st), 0);
    mode_t perms = st.st_mode & 0777;
    ASSERT_EQ((int)perms, 0644);

    /* Clearing the flag must remove the file */
    flag_set("/tmp/", key, false);
    ASSERT_FALSE(flag_get("/tmp/", key));
}

TEST(flag_set_clear_nonexistent_is_safe) {
    /* Clearing a flag that doesn't exist must not crash */
    flag_set("/tmp/", "onion_test_flag_nonexistent", false);
    ASSERT_FALSE(flag_get("/tmp/", "onion_test_flag_nonexistent"));
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
    RUN_TEST(file_removeExtension_trailing_dot);
    RUN_TEST(file_removeExtension_single_char_ext);

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
    RUN_TEST(file_readLastLine_tiny_file);
    RUN_TEST(file_readLastLine_one_byte);
    RUN_TEST(file_readLastLine_exact_254_bytes);

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

    RUN_TEST(file_isLocked_existing_file);
    RUN_TEST(file_isLocked_uncreateable_path);

    RUN_TEST(file_path_relative_to_same_dir);
    RUN_TEST(file_path_relative_to_subdirectory);
    RUN_TEST(file_path_relative_to_nonexistent);
    RUN_TEST(file_path_relative_to_shared_prefix);

    RUN_TEST(file_open_ensure_path_creates_dirs);

    RUN_TEST(file_findNewest_basic);
    RUN_TEST(file_findNewest_empty_dir);
    RUN_TEST(file_findNewest_nonexistent_dir);

    RUN_TEST(file_remove_recursive_removes_tree);
    RUN_TEST(file_remove_recursive_null);
    RUN_TEST(file_remove_recursive_nonexistent);

    RUN_TEST(file_read_lineN_first_line);
    RUN_TEST(file_read_lineN_last_line);
    RUN_TEST(file_read_lineN_out_of_range);

    RUN_TEST(file_parseKeyValue_colon_divider);
    RUN_TEST(file_parseKeyValue_key_not_found);
    RUN_TEST(file_parseKeyValue_select_index);

    RUN_TEST(file_cleanName_numbered_prefix);

    RUN_TEST(file_write_nonexistent_file);
    RUN_TEST(file_copy_nonexistent_src);

    RUN_TEST(file_read_basic);
    RUN_TEST(file_read_nonexistent);
    RUN_TEST(file_read_empty_file);
    RUN_TEST(file_read_multiline);

    RUN_TEST(flag_set_creates_file_with_correct_permissions);
    RUN_TEST(flag_set_clear_nonexistent_is_safe);

    TEST_REPORT();
    return test_failures;
}
