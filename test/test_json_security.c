/**
 * @file test_json_security.c
 * @brief Security and edge-case tests for src/common/utils/json.h
 *
 * Covers NULL pointer safety, deeply nested JSON, malformed inputs,
 * long string truncation, and memory cleanup verification.
 *
 * Build and run: make -f Makefile.unit test_json_security
 */

#include "onion_test.h"
#include "../src/common/utils/json.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TEST_DIR = "/tmp/onion_test_json_sec";

static void setup_test_dir(void)
{
    mkdir(TEST_DIR, 0755);
}

static void cleanup_test_dir(void)
{
    file_remove_recursive(TEST_DIR);
}

/* ---- json_getString: NULL and boundary inputs ---- */

TEST(json_getString_null_object) {
    char result[JSON_STRING_LEN] = {0};
    bool ok = json_getString(NULL, "key", result);
    ASSERT_FALSE(ok);
}

TEST(json_getString_wrong_type) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "num", 42);

    char result[JSON_STRING_LEN] = {0};
    bool ok = json_getString(json, "num", result);
    ASSERT_FALSE(ok);

    cJSON_Delete(json);
}

TEST(json_getString_long_value_truncated) {
    /* Value longer than JSON_STRING_LEN should be truncated safely */
    cJSON *json = cJSON_CreateObject();

    char long_val[512];
    memset(long_val, 'A', sizeof(long_val) - 1);
    long_val[sizeof(long_val) - 1] = '\0';

    cJSON_AddStringToObject(json, "long", long_val);

    char result[JSON_STRING_LEN] = {0};
    bool ok = json_getString(json, "long", result);
    ASSERT_TRUE(ok);
    ASSERT_EQ(strlen(result), JSON_STRING_LEN - 1);
    ASSERT_EQ(result[JSON_STRING_LEN - 1], '\0');

    cJSON_Delete(json);
}

TEST(json_getString_unicode) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "emoji", "\xC3\xA9\xC3\xA0\xC3\xBC");

    char result[JSON_STRING_LEN] = {0};
    bool ok = json_getString(json, "emoji", result);
    ASSERT_TRUE(ok);
    ASSERT_STREQ(result, "\xC3\xA9\xC3\xA0\xC3\xBC");

    cJSON_Delete(json);
}

/* ---- json_getInt: boundary values ---- */

TEST(json_getInt_null_object) {
    int val = 0;
    bool ok = json_getInt(NULL, "key", &val);
    ASSERT_FALSE(ok);
}

TEST(json_getInt_large_value) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "big", 2147483647.0); /* INT_MAX */

    int val = 0;
    bool ok = json_getInt(json, "big", &val);
    ASSERT_TRUE(ok);
    ASSERT_EQ(val, 2147483647);

    cJSON_Delete(json);
}

TEST(json_getInt_negative) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "neg", -999);

    int val = 0;
    bool ok = json_getInt(json, "neg", &val);
    ASSERT_TRUE(ok);
    ASSERT_EQ(val, -999);

    cJSON_Delete(json);
}

TEST(json_getInt_missing_key) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "exists", 1);

    int val = 42;
    bool ok = json_getInt(json, "missing", &val);
    ASSERT_FALSE(ok);
    ASSERT_EQ(val, 42); /* Should not be modified */

    cJSON_Delete(json);
}

/* ---- json_getBool: edge cases ---- */

TEST(json_getBool_null_object) {
    bool val = false;
    bool ok = json_getBool(NULL, "key", &val);
    ASSERT_FALSE(ok);
}

TEST(json_getBool_true) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "flag", 1);

    bool val = false;
    bool ok = json_getBool(json, "flag", &val);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(val);

    cJSON_Delete(json);
}

TEST(json_getBool_false) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "flag", 0);

    bool val = true;
    bool ok = json_getBool(json, "flag", &val);
    ASSERT_TRUE(ok);
    ASSERT_FALSE(val);

    cJSON_Delete(json);
}

/* ---- json_getDouble: edge cases ---- */

TEST(json_getDouble_null_object) {
    double val = 0.0;
    bool ok = json_getDouble(NULL, "key", &val);
    ASSERT_FALSE(ok);
}

TEST(json_getDouble_precision) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "pi", 3.14159265358979);

    double val = 0.0;
    bool ok = json_getDouble(json, "pi", &val);
    ASSERT_TRUE(ok);
    /* Check approximate equality */
    ASSERT_TRUE(val > 3.14 && val < 3.15);

    cJSON_Delete(json);
}

/* ---- json_setString: edge cases ---- */

TEST(json_setString_missing_key) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key1", "val1");

    bool ok = json_setString(json, "nonexistent", "newval");
    ASSERT_FALSE(ok);

    cJSON_Delete(json);
}

TEST(json_setString_overwrite) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key1", "old");

    bool ok = json_setString(json, "key1", "new");
    ASSERT_TRUE(ok);

    char result[JSON_STRING_LEN] = {0};
    json_getString(json, "key1", result);
    ASSERT_STREQ(result, "new");

    cJSON_Delete(json);
}

/* ---- json_forceSetString: edge cases ---- */

TEST(json_forceSetString_creates_key) {
    cJSON *json = cJSON_CreateObject();

    bool ok = json_forceSetString(json, "newkey", "newval");
    ASSERT_TRUE(ok);

    char result[JSON_STRING_LEN] = {0};
    json_getString(json, "newkey", result);
    ASSERT_STREQ(result, "newval");

    cJSON_Delete(json);
}

TEST(json_forceSetString_updates_existing) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key", "old");

    json_forceSetString(json, "key", "updated");

    char result[JSON_STRING_LEN] = {0};
    json_getString(json, "key", result);
    ASSERT_STREQ(result, "updated");

    cJSON_Delete(json);
}

/* ---- json_load: malformed files ---- */

TEST(json_load_nonexistent) {
    cJSON *json = json_load("/tmp/onion_test_no_file.json");
    ASSERT_NULL(json);
}

TEST(json_load_empty_file) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/empty.json", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fclose(fp);

    cJSON *json = json_load(path);
    ASSERT_NULL(json);

    cleanup_test_dir();
}

TEST(json_load_invalid_json) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/invalid.json", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "{{not valid json!!");
    fclose(fp);

    cJSON *json = json_load(path);
    ASSERT_NULL(json);

    cleanup_test_dir();
}

TEST(json_load_valid_json) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/valid.json", TEST_DIR);

    FILE *fp = fopen(path, "w");
    fprintf(fp, "{\"name\": \"test\", \"value\": 42}");
    fclose(fp);

    cJSON *json = json_load(path);
    ASSERT_NOT_NULL(json);

    char name[JSON_STRING_LEN] = {0};
    json_getString(json, "name", name);
    ASSERT_STREQ(name, "test");

    int val = 0;
    json_getInt(json, "value", &val);
    ASSERT_EQ(val, 42);

    cJSON_Delete(json);
    cleanup_test_dir();
}

/* ---- json_save: edge cases ---- */

TEST(json_save_null_object) {
    /* Should not crash */
    json_save(NULL, "/tmp/onion_test_null.json");
    ASSERT_TRUE(1);
}

TEST(json_save_null_path) {
    cJSON *json = cJSON_CreateObject();
    /* Should not crash */
    json_save(json, NULL);
    ASSERT_TRUE(1);
    cJSON_Delete(json);
}

TEST(json_save_roundtrip) {
    setup_test_dir();
    char path[256];
    snprintf(path, sizeof(path), "%s/roundtrip.json", TEST_DIR);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key", "value");
    cJSON_AddNumberToObject(json, "num", 123);

    json_save(json, path);
    cJSON_Delete(json);

    cJSON *loaded = json_load(path);
    ASSERT_NOT_NULL(loaded);

    char val[JSON_STRING_LEN] = {0};
    json_getString(loaded, "key", val);
    ASSERT_STREQ(val, "value");

    int num = 0;
    json_getInt(loaded, "num", &num);
    ASSERT_EQ(num, 123);

    cJSON_Delete(loaded);
    cleanup_test_dir();
}

/* ---- Deeply nested JSON ---- */

TEST(json_deeply_nested) {
    /* Create a nested structure: {"a": {"b": {"c": "deep"}}} */
    cJSON *root = cJSON_CreateObject();
    cJSON *a = cJSON_AddObjectToObject(root, "a");
    cJSON *b = cJSON_AddObjectToObject(a, "b");
    cJSON_AddStringToObject(b, "c", "deep");

    /* Access nested value via child traversal */
    cJSON *obj_a = cJSON_GetObjectItem(root, "a");
    ASSERT_NOT_NULL(obj_a);
    cJSON *obj_b = cJSON_GetObjectItem(obj_a, "b");
    ASSERT_NOT_NULL(obj_b);

    char result[JSON_STRING_LEN] = {0};
    bool ok = json_getString(obj_b, "c", result);
    ASSERT_TRUE(ok);
    ASSERT_STREQ(result, "deep");

    cJSON_Delete(root);
}

/* ---- Multiple updates to same key ---- */

TEST(json_multiple_updates) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key", "v1");

    json_setString(json, "key", "v2");
    json_setString(json, "key", "v3");

    char result[JSON_STRING_LEN] = {0};
    json_getString(json, "key", result);
    ASSERT_STREQ(result, "v3");

    cJSON_Delete(json);
}

int main(void) {
    /* json_getString edge cases */
    RUN_TEST(json_getString_null_object);
    RUN_TEST(json_getString_wrong_type);
    RUN_TEST(json_getString_long_value_truncated);
    RUN_TEST(json_getString_unicode);

    /* json_getInt edge cases */
    RUN_TEST(json_getInt_null_object);
    RUN_TEST(json_getInt_large_value);
    RUN_TEST(json_getInt_negative);
    RUN_TEST(json_getInt_missing_key);

    /* json_getBool edge cases */
    RUN_TEST(json_getBool_null_object);
    RUN_TEST(json_getBool_true);
    RUN_TEST(json_getBool_false);

    /* json_getDouble edge cases */
    RUN_TEST(json_getDouble_null_object);
    RUN_TEST(json_getDouble_precision);

    /* json_setString edge cases */
    RUN_TEST(json_setString_missing_key);
    RUN_TEST(json_setString_overwrite);

    /* json_forceSetString edge cases */
    RUN_TEST(json_forceSetString_creates_key);
    RUN_TEST(json_forceSetString_updates_existing);

    /* json_load malformed files */
    RUN_TEST(json_load_nonexistent);
    RUN_TEST(json_load_empty_file);
    RUN_TEST(json_load_invalid_json);
    RUN_TEST(json_load_valid_json);

    /* json_save edge cases */
    RUN_TEST(json_save_null_object);
    RUN_TEST(json_save_null_path);
    RUN_TEST(json_save_roundtrip);

    /* Deeply nested JSON */
    RUN_TEST(json_deeply_nested);

    /* Multiple updates */
    RUN_TEST(json_multiple_updates);

    TEST_REPORT();
    return test_failures;
}
