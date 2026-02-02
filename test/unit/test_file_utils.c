/**
 * Unit Tests for File Utilities
 * 
 * Tests the file.c utility functions for correctness and safety
 */

#include "../unity/unity.h"
#include "../../src/common/utils/file.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* Test fixture setup/teardown */
static const char *TEST_DIR = "/tmp/onion_test";
static const char *TEST_FILE = "/tmp/onion_test/test.txt";

void setUp(void)
{
    /* Create test directory */
    mkdir(TEST_DIR, 0755);
}

void tearDown(void)
{
    /* Clean up test files */
    remove(TEST_FILE);
    rmdir(TEST_DIR);
}

/* Test: exists() correctly identifies existing files */
void test_exists_file(void)
{
    /* Create a test file */
    FILE *fp = fopen(TEST_FILE, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fprintf(fp, "test content");
    fclose(fp);
    
    /* Test exists */
    TEST_ASSERT_TRUE(exists(TEST_FILE));
    TEST_ASSERT_FALSE(exists("/tmp/nonexistent_file_xyz.txt"));
}

/* Test: is_file() distinguishes files from directories */
void test_is_file_vs_directory(void)
{
    /* Create a test file */
    FILE *fp = fopen(TEST_FILE, "w");
    TEST_ASSERT_NOT_NULL(fp);
    fclose(fp);
    
    /* Test is_file */
    TEST_ASSERT_TRUE(is_file(TEST_FILE));
    TEST_ASSERT_FALSE(is_file(TEST_DIR));  /* Directory, not file */
}

/* Test: is_dir() correctly identifies directories */
void test_is_dir(void)
{
    TEST_ASSERT_TRUE(is_dir(TEST_DIR));
    TEST_ASSERT_FALSE(is_dir("/tmp/nonexistent_dir_xyz"));
}

/* Test: mkdirs() creates nested directories safely */
void test_mkdirs_nested(void)
{
    const char *nested_dir = "/tmp/onion_test/level1/level2/level3";
    
    /* Should create all levels */
    bool created = mkdirs(nested_dir);
    TEST_ASSERT_TRUE(created);
    TEST_ASSERT_TRUE(is_dir(nested_dir));
    
    /* Calling again should return false (already exists) */
    created = mkdirs(nested_dir);
    TEST_ASSERT_FALSE(created);
    
    /* Cleanup */
    rmdir("/tmp/onion_test/level1/level2/level3");
    rmdir("/tmp/onion_test/level1/level2");
    rmdir("/tmp/onion_test/level1");
}

/* Test: mkdirs() safely handles long paths */
void test_mkdirs_long_path(void)
{
    /* Test that overly long paths are rejected (security fix) */
    char long_path[600];
    memset(long_path, 'a', sizeof(long_path) - 1);
    long_path[sizeof(long_path) - 1] = '\0';
    
    /* Should fail safely without buffer overflow */
    bool created = mkdirs(long_path);
    TEST_ASSERT_FALSE(created);
}

/* Test: mkdirs() handles NULL and empty strings */
void test_mkdirs_invalid_input(void)
{
    TEST_ASSERT_FALSE(mkdirs(NULL));
    TEST_ASSERT_FALSE(mkdirs(""));
}

/* Test: file_basename() extracts filename from path */
void test_file_basename(void)
{
    TEST_ASSERT_EQUAL_STRING("file.txt", file_basename("/path/to/file.txt"));
    TEST_ASSERT_EQUAL_STRING("file.txt", file_basename("file.txt"));
    TEST_ASSERT_EQUAL_STRING("", file_basename("/path/to/"));
    TEST_ASSERT_EQUAL_STRING("single", file_basename("single"));
}

/* Main test runner */
int main(void)
{
    UNITY_BEGIN();
    
    /* Run all tests with setup/teardown */
    setUp();
    RUN_TEST(test_exists_file);
    tearDown();
    
    setUp();
    RUN_TEST(test_is_file_vs_directory);
    tearDown();
    
    setUp();
    RUN_TEST(test_is_dir);
    tearDown();
    
    setUp();
    RUN_TEST(test_mkdirs_nested);
    tearDown();
    
    setUp();
    RUN_TEST(test_mkdirs_long_path);
    tearDown();
    
    setUp();
    RUN_TEST(test_mkdirs_invalid_input);
    tearDown();
    
    setUp();
    RUN_TEST(test_file_basename);
    tearDown();
    
    return UNITY_END();
}
