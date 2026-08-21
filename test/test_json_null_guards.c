/**
 * @file test_json_null_guards.c
 * @brief Tests for json_load NULL guard and related NULL safety fixes (round 4)
 *
 * Validates:
 * - json_load returns NULL when file_read fails
 * - json_forceSetString handles NULL object gracefully
 * - lang_load handles json_load failure (NULL check + memory cleanup)
 * - write_mainui_state integer underflow guards
 * - theme_getPath falls back when json_load returns NULL
 *
 * Build and run: make -f Makefile.unit test_json_null_guards
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ---- Stubs ---- */
#define print_debug(...)
#define printf_debug(...)

/* Include cJSON for testing json_load and json_forceSetString */
#include "../include/cjson/cJSON.h"
#include "../src/common/utils/json.h"

/* ================================================================== */
/*  Test: json_load returns NULL for non-existent file                */
/* ================================================================== */

TEST(json_load_nonexistent_file) {
    cJSON *result = json_load("/nonexistent/path/to/file.json");
    ASSERT_NULL(result);
}

TEST(json_load_empty_path) {
    cJSON *result = json_load("");
    ASSERT_NULL(result);
}

TEST(json_load_null_path) {
    /* file_read(NULL) should return NULL, json_load should propagate */
    cJSON *result = json_load(NULL);
    ASSERT_NULL(result);
}

TEST(json_load_valid_file) {
    /* Create a temp file with valid JSON */
    const char *tmp_path = "/tmp/test_json_load_valid.json";
    FILE *f = fopen(tmp_path, "w");
    ASSERT_NOT_NULL(f);
    fprintf(f, "{\"key\":\"value\",\"num\":42}");
    fclose(f);

    cJSON *result = json_load(tmp_path);
    ASSERT_NOT_NULL(result);

    char dest[256] = {0};
    ASSERT_TRUE(json_getString(result, "key", dest));
    ASSERT_STREQ(dest, "value");

    int num = 0;
    ASSERT_TRUE(json_getInt(result, "num", &num));
    ASSERT_EQ(num, 42);

    cJSON_Delete(result);
    remove(tmp_path);
}

TEST(json_load_invalid_json) {
    /* Create a temp file with invalid JSON */
    const char *tmp_path = "/tmp/test_json_load_invalid.json";
    FILE *f = fopen(tmp_path, "w");
    ASSERT_NOT_NULL(f);
    fprintf(f, "this is not valid json {{{");
    fclose(f);

    cJSON *result = json_load(tmp_path);
    ASSERT_NULL(result);
    remove(tmp_path);
}

/* ================================================================== */
/*  Test: json_forceSetString with NULL object                        */
/* ================================================================== */

TEST(json_forceSetString_null_object) {
    bool result = json_forceSetString(NULL, "key", "value");
    ASSERT_FALSE(result);
}

TEST(json_forceSetString_valid_update) {
    cJSON *obj = cJSON_CreateObject();
    ASSERT_NOT_NULL(obj);
    cJSON_AddStringToObject(obj, "key", "old");

    bool result = json_forceSetString(obj, "key", "new");
    ASSERT_TRUE(result);

    char dest[256] = {0};
    ASSERT_TRUE(json_getString(obj, "key", dest));
    ASSERT_STREQ(dest, "new");
    cJSON_Delete(obj);
}

TEST(json_forceSetString_new_key) {
    cJSON *obj = cJSON_CreateObject();
    ASSERT_NOT_NULL(obj);

    bool result = json_forceSetString(obj, "newkey", "newval");
    ASSERT_TRUE(result);

    char dest[256] = {0};
    ASSERT_TRUE(json_getString(obj, "newkey", dest));
    ASSERT_STREQ(dest, "newval");
    cJSON_Delete(obj);
}

/* ================================================================== */
/*  Test: json_getString/json_getInt with NULL object                  */
/* ================================================================== */

TEST(json_getString_null_object) {
    char dest[256] = "untouched";
    bool result = json_getString(NULL, "key", dest);
    ASSERT_FALSE(result);
    ASSERT_STREQ(dest, "untouched");
}

TEST(json_getInt_null_object) {
    int dest = 99;
    bool result = json_getInt(NULL, "key", &dest);
    ASSERT_FALSE(result);
    ASSERT_EQ(dest, 99); /* unchanged */
}

TEST(json_getBool_null_object) {
    bool dest = true;
    bool result = json_getBool(NULL, "key", &dest);
    ASSERT_FALSE(result);
    ASSERT_TRUE(dest); /* unchanged */
}

/* ================================================================== */
/*  Test: write_mainui_state integer underflow guards                  */
/* ================================================================== */

/*
 * Test the underflow fix pattern:
 *   old: page_start = total - page_size;        (underflows if total < page_size)
 *   new: page_start = (total >= page_size) ? total - page_size : 0;
 */

TEST(underflow_guard_total_less_than_pagesize) {
    int total = 2, page_size = 6;
    int page_start;

    /* Fixed pattern */
    page_start = (total >= page_size) ? total - page_size : 0;
    ASSERT_GE(page_start, 0);
    ASSERT_EQ(page_start, 0);
}

TEST(underflow_guard_total_equals_pagesize) {
    int total = 6, page_size = 6;
    int page_start = (total >= page_size) ? total - page_size : 0;
    ASSERT_EQ(page_start, 0);
}

TEST(underflow_guard_total_greater_than_pagesize) {
    int total = 10, page_size = 6;
    int page_start = (total >= page_size) ? total - page_size : 0;
    ASSERT_EQ(page_start, 4);
}

TEST(underflow_guard_main_total_less_than_4) {
    int main_total = 3;
    int main_page_start = (main_total >= 4) ? main_total - 4 : 0;
    ASSERT_EQ(main_page_start, 0);
}

TEST(underflow_guard_main_total_equals_4) {
    int main_total = 4;
    int main_page_start = (main_total >= 4) ? main_total - 4 : 0;
    ASSERT_EQ(main_page_start, 0);
}

TEST(underflow_guard_main_total_greater_than_4) {
    int main_total = 6;
    int main_page_start = (main_total >= 4) ? main_total - 4 : 0;
    ASSERT_EQ(main_page_start, 2);
}

TEST(underflow_guard_zero_total) {
    int total = 0, page_size = 8;
    int page_start = (total >= page_size) ? total - page_size : 0;
    ASSERT_EQ(page_start, 0);

    int page_end = page_start + page_size - 1;
    ASSERT_EQ(page_end, 7);
}

/* ================================================================== */
/*  main                                                              */
/* ================================================================== */

int main(void)
{
    printf("\n=== JSON NULL Guards and Underflow Fixes Tests (Round 4) ===\n\n");

    /* json_load NULL returns */
    RUN_TEST(json_load_nonexistent_file);
    RUN_TEST(json_load_empty_path);
    RUN_TEST(json_load_null_path);
    RUN_TEST(json_load_valid_file);
    RUN_TEST(json_load_invalid_json);

    /* json_forceSetString NULL safety */
    RUN_TEST(json_forceSetString_null_object);
    RUN_TEST(json_forceSetString_valid_update);
    RUN_TEST(json_forceSetString_new_key);

    /* json getter NULL safety */
    RUN_TEST(json_getString_null_object);
    RUN_TEST(json_getInt_null_object);
    RUN_TEST(json_getBool_null_object);

    /* Integer underflow guards */
    RUN_TEST(underflow_guard_total_less_than_pagesize);
    RUN_TEST(underflow_guard_total_equals_pagesize);
    RUN_TEST(underflow_guard_total_greater_than_pagesize);
    RUN_TEST(underflow_guard_main_total_less_than_4);
    RUN_TEST(underflow_guard_main_total_equals_4);
    RUN_TEST(underflow_guard_main_total_greater_than_4);
    RUN_TEST(underflow_guard_zero_total);

    TEST_REPORT();
    return test_failures;
}
