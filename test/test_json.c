/**
 * @file test_json.c
 * @brief Unit tests for src/common/utils/json.h
 *
 * Tests JSON utility functions: json_getString, json_getInt, json_getBool,
 * json_getDouble, json_setString, json_forceSetString, json_load, json_save.
 *
 * Build and run: make -f Makefile.unit test_json && ./build_test/test_json
 */

#include "onion_test.h"
#include "../src/common/utils/json.h"
#include <stdlib.h>
#include <unistd.h>

/* ---- json_getString ---- */

TEST(json_getString_exists) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key1", "value1");
    
    char result[JSON_STRING_LEN] = {0};
    bool success = json_getString(json, "key1", result);
    
    ASSERT_TRUE(success);
    ASSERT_STREQ(result, "value1");
    
    cJSON_Delete(json);
}

TEST(json_getString_not_exists) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key1", "value1");
    
    char result[JSON_STRING_LEN] = {0};
    bool success = json_getString(json, "key2", result);
    
    ASSERT_FALSE(success);
    
    cJSON_Delete(json);
}

TEST(json_getString_empty) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key1", "");
    
    char result[JSON_STRING_LEN] = {0};
    bool success = json_getString(json, "key1", result);
    
    ASSERT_TRUE(success);
    ASSERT_STREQ(result, "");
    
    cJSON_Delete(json);
}

/* ---- json_getInt ---- */

TEST(json_getInt_exists) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "age", 42);
    
    int result = 0;
    bool success = json_getInt(json, "age", &result);
    
    ASSERT_TRUE(success);
    ASSERT_EQ(result, 42);
    
    cJSON_Delete(json);
}

TEST(json_getInt_not_exists) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "age", 42);
    
    int result = 0;
    bool success = json_getInt(json, "missing", &result);
    
    ASSERT_FALSE(success);
    
    cJSON_Delete(json);
}

TEST(json_getInt_zero) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "zero", 0);
    
    int result = 999;
    bool success = json_getInt(json, "zero", &result);
    
    ASSERT_TRUE(success);
    ASSERT_EQ(result, 0);
    
    cJSON_Delete(json);
}

TEST(json_getInt_negative) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "neg", -100);
    
    int result = 0;
    bool success = json_getInt(json, "neg", &result);
    
    ASSERT_TRUE(success);
    ASSERT_EQ(result, -100);
    
    cJSON_Delete(json);
}

/* ---- json_getBool ---- */

TEST(json_getBool_true) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "flag", true);
    
    bool result = false;
    bool success = json_getBool(json, "flag", &result);
    
    ASSERT_TRUE(success);
    ASSERT_TRUE(result);
    
    cJSON_Delete(json);
}

TEST(json_getBool_false) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "flag", false);
    
    bool result = true;
    bool success = json_getBool(json, "flag", &result);
    
    ASSERT_TRUE(success);
    ASSERT_FALSE(result);
    
    cJSON_Delete(json);
}

TEST(json_getBool_not_exists) {
    cJSON *json = cJSON_CreateObject();
    
    bool result = false;
    bool success = json_getBool(json, "missing", &result);
    
    ASSERT_FALSE(success);
    
    cJSON_Delete(json);
}

/* ---- json_getDouble ---- */

TEST(json_getDouble_exists) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "pi", 3.14159);
    
    double result = 0.0;
    bool success = json_getDouble(json, "pi", &result);
    
    ASSERT_TRUE(success);
    // Check that result is close to 3.14159 (within 0.01 tolerance)
    ASSERT_GE((long)(result * 100000), 314150);
    ASSERT_GE(314170, (long)(result * 100000));
    
    cJSON_Delete(json);
}

TEST(json_getDouble_not_exists) {
    cJSON *json = cJSON_CreateObject();
    
    double result = 0.0;
    bool success = json_getDouble(json, "missing", &result);
    
    ASSERT_FALSE(success);
    
    cJSON_Delete(json);
}

/* ---- json_setString ---- */

TEST(json_setString_exists) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key", "old_value");
    
    bool success = json_setString(json, "key", "new_value");
    ASSERT_TRUE(success);
    
    char result[JSON_STRING_LEN] = {0};
    json_getString(json, "key", result);
    ASSERT_STREQ(result, "new_value");
    
    cJSON_Delete(json);
}

TEST(json_setString_not_exists) {
    cJSON *json = cJSON_CreateObject();
    
    bool success = json_setString(json, "key", "value");
    ASSERT_FALSE(success);
    
    cJSON_Delete(json);
}

/* ---- json_forceSetString ---- */

TEST(json_forceSetString_exists) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "key", "old_value");
    
    bool success = json_forceSetString(json, "key", "new_value");
    ASSERT_TRUE(success);
    
    char result[JSON_STRING_LEN] = {0};
    json_getString(json, "key", result);
    ASSERT_STREQ(result, "new_value");
    
    cJSON_Delete(json);
}

TEST(json_forceSetString_not_exists) {
    cJSON *json = cJSON_CreateObject();
    
    bool success = json_forceSetString(json, "key", "value");
    ASSERT_TRUE(success);
    
    char result[JSON_STRING_LEN] = {0};
    json_getString(json, "key", result);
    ASSERT_STREQ(result, "value");
    
    cJSON_Delete(json);
}

/* ---- json_load and json_save ---- */

TEST(json_save_and_load) {
    const char *tmpfile = "/tmp/onion_test_json.json";
    
    // Create and save JSON
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "name", "test");
    cJSON_AddNumberToObject(json, "value", 123);
    cJSON_AddBoolToObject(json, "enabled", true);
    
    json_save(json, (char *)tmpfile);
    cJSON_Delete(json);
    
    // Load and verify
    cJSON *loaded = json_load(tmpfile);
    ASSERT_NOT_NULL(loaded);
    
    char name[JSON_STRING_LEN] = {0};
    int value = 0;
    bool enabled = false;
    
    ASSERT_TRUE(json_getString(loaded, "name", name));
    ASSERT_STREQ(name, "test");
    
    ASSERT_TRUE(json_getInt(loaded, "value", &value));
    ASSERT_EQ(value, 123);
    
    ASSERT_TRUE(json_getBool(loaded, "enabled", &enabled));
    ASSERT_TRUE(enabled);
    
    cJSON_Delete(loaded);
    unlink(tmpfile);
}

TEST(json_save_null_object) {
    // Should not crash with NULL object - if we reach this point, it didn't crash
    json_save(NULL, (char *)"/tmp/test.json");
    // Test passes if no crash occurs
}

TEST(json_save_null_path) {
    cJSON *json = cJSON_CreateObject();
    // Should not crash with NULL path - if we reach this point, it didn't crash
    json_save(json, NULL);
    cJSON_Delete(json);
    // Test passes if no crash occurs
}

TEST(json_load_nonexistent) {
    cJSON *json = json_load("/tmp/nonexistent_file_12345.json");
    ASSERT_NULL(json);
}

/* ---- extra coverage ---- */

TEST(json_load_invalid_json) {
    const char *tmpfile = "/tmp/onion_test_invalid.json";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "{ this is not valid JSON !!! }");
    fclose(fp);

    cJSON *json = json_load(tmpfile);
    ASSERT_NULL(json);

    unlink(tmpfile);
}

TEST(json_load_empty_file) {
    const char *tmpfile = "/tmp/onion_test_empty.json";
    FILE *fp = fopen(tmpfile, "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    cJSON *json = json_load(tmpfile);
    ASSERT_NULL(json);

    unlink(tmpfile);
}

TEST(json_getString_null_object) {
    char result[JSON_STRING_LEN] = {0};
    bool success = json_getString(NULL, "key", result);
    ASSERT_FALSE(success);
}

TEST(json_getInt_null_object) {
    int result = 99;
    bool success = json_getInt(NULL, "key", &result);
    ASSERT_FALSE(success);
    ASSERT_EQ(result, 99); /* unchanged on failure */
}

TEST(json_getBool_null_object) {
    bool result = true;
    bool success = json_getBool(NULL, "key", &result);
    ASSERT_FALSE(success);
}

TEST(json_nested_object) {
    /* Build {"outer": {"inner": "value"}} and retrieve nested value */
    cJSON *root = cJSON_CreateObject();
    cJSON *inner = cJSON_CreateObject();
    cJSON_AddStringToObject(inner, "inner", "nested_value");
    cJSON_AddItemToObject(root, "outer", inner);

    cJSON *outer_item = cJSON_GetObjectItem(root, "outer");
    ASSERT_NOT_NULL(outer_item);

    char result[JSON_STRING_LEN] = {0};
    bool success = json_getString(outer_item, "inner", result);
    ASSERT_TRUE(success);
    ASSERT_STREQ(result, "nested_value");

    cJSON_Delete(root);
}

TEST(json_forceSetString_update_and_retrieve_twice) {
    /* Calling forceSetString twice on the same key updates the value each time */
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "label", "first");

    json_forceSetString(json, "label", "second");
    char result[JSON_STRING_LEN] = {0};
    json_getString(json, "label", result);
    ASSERT_STREQ(result, "second");

    json_forceSetString(json, "label", "third");
    json_getString(json, "label", result);
    ASSERT_STREQ(result, "third");

    cJSON_Delete(json);
}

TEST(json_getInt_large_value) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "big", 2000000);

    int result = 0;
    bool success = json_getInt(json, "big", &result);
    ASSERT_TRUE(success);
    ASSERT_EQ(result, 2000000);

    cJSON_Delete(json);
}

TEST(json_save_roundtrip_unicode) {
    const char *tmpfile = "/tmp/onion_test_unicode.json";

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "title", "ゲーム");  /* Japanese "game" */
    json_save(json, (char *)tmpfile);
    cJSON_Delete(json);

    cJSON *loaded = json_load(tmpfile);
    ASSERT_NOT_NULL(loaded);

    char result[JSON_STRING_LEN] = {0};
    ASSERT_TRUE(json_getString(loaded, "title", result));
    ASSERT_STREQ(result, "ゲーム");

    cJSON_Delete(loaded);
    unlink(tmpfile);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== json.h Unit Tests ===\n\n");

    RUN_TEST(json_getString_exists);
    RUN_TEST(json_getString_not_exists);
    RUN_TEST(json_getString_empty);

    RUN_TEST(json_getInt_exists);
    RUN_TEST(json_getInt_not_exists);
    RUN_TEST(json_getInt_zero);
    RUN_TEST(json_getInt_negative);

    RUN_TEST(json_getBool_true);
    RUN_TEST(json_getBool_false);
    RUN_TEST(json_getBool_not_exists);

    RUN_TEST(json_getDouble_exists);
    RUN_TEST(json_getDouble_not_exists);

    RUN_TEST(json_setString_exists);
    RUN_TEST(json_setString_not_exists);

    RUN_TEST(json_forceSetString_exists);
    RUN_TEST(json_forceSetString_not_exists);

    RUN_TEST(json_save_and_load);
    RUN_TEST(json_save_null_object);
    RUN_TEST(json_save_null_path);
    RUN_TEST(json_load_nonexistent);

    RUN_TEST(json_load_invalid_json);
    RUN_TEST(json_load_empty_file);
    RUN_TEST(json_getString_null_object);
    RUN_TEST(json_getInt_null_object);
    RUN_TEST(json_getBool_null_object);
    RUN_TEST(json_nested_object);
    RUN_TEST(json_forceSetString_update_and_retrieve_twice);
    RUN_TEST(json_getInt_large_value);
    RUN_TEST(json_save_roundtrip_unicode);

    TEST_REPORT();
    return test_failures;
}
