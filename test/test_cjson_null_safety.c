/**
 * @file test_cjson_null_safety.c
 * @brief Tests for cJSON_Parse NULL safety fixes (round 3)
 *
 * Validates:
 * - cJSON's own NULL handling
 * - JsonGameEntry_fromJson (the production function) returns defaults on
 *   malformed JSON
 * - file_removeExtension
 *
 * Build and run: make -f Makefile.unit test_cjson_null_safety
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/common/utils/log.h"

/* ================================================================== */
/*  Test: cJSON_Parse NULL handling                                   */
/* ================================================================== */

/*
 * The cJSON library safely handles NULL at the API level:
 * - cJSON_Parse(NULL) returns NULL
 * - cJSON_GetObjectItem(NULL, key) returns NULL
 * - cJSON_Delete(NULL) is a no-op
 *
 * However, callers MUST check cJSON_Parse() return for NULL
 * before using the result. These tests verify the fix pattern.
 */

/* Include cJSON for unit testing */
#include "../include/cjson/cJSON.h"

TEST(cjson_parse_null_returns_null) {
    cJSON *result = cJSON_Parse(NULL);
    ASSERT_NULL(result);
}

TEST(cjson_parse_empty_returns_null) {
    cJSON *result = cJSON_Parse("");
    ASSERT_NULL(result);
    cJSON_Delete(result); /* safe even if NULL */
}

TEST(cjson_parse_garbage_returns_null) {
    cJSON *result = cJSON_Parse("not json at all {{{");
    ASSERT_NULL(result);
    cJSON_Delete(result);
}

TEST(cjson_get_object_item_null_root) {
    /* This must not crash - returns NULL safely */
    cJSON *item = cJSON_GetObjectItem(NULL, "key");
    ASSERT_NULL(item);
}

TEST(cjson_delete_null_safe) {
    /* Must not crash */
    cJSON_Delete(NULL);
    ASSERT_TRUE(1);
}

/* ================================================================== */
/*  Test: Fixed cJSON_Parse guard pattern                             */
/* ================================================================== */

/*
 * Pattern used in theme/config.h, settings.h, JsonGameEntry.h:
 *   cJSON *root = cJSON_Parse(str);
 *   if (root == NULL) return <default>;
 */

TEST(cjson_parse_valid_json) {
    const char *good_json = "{\"vol\":50,\"brightness\":5}";
    cJSON *json_root = cJSON_Parse(good_json);
    ASSERT_NOT_NULL(json_root);

    cJSON *vol = cJSON_GetObjectItem(json_root, "vol");
    ASSERT_NOT_NULL(vol);
    ASSERT_EQ((int)cJSON_GetNumberValue(vol), 50);

    cJSON *brightness = cJSON_GetObjectItem(json_root, "brightness");
    ASSERT_NOT_NULL(brightness);
    ASSERT_EQ((int)cJSON_GetNumberValue(brightness), 5);

    cJSON_Delete(json_root);
}

/* ================================================================== */
/*  Test: JsonGameEntry default values on parse failure               */
/* ================================================================== */

/* The production JsonGameEntry_fromJson() (a reduced copy before). */
#include "components/JsonGameEntry.h"
typedef JsonGameEntry TestGameEntry;
#define test_entry_from_json JsonGameEntry_fromJson

TEST(game_entry_null_json_returns_defaults) {
    TestGameEntry entry = test_entry_from_json(NULL);
    ASSERT_STREQ(entry.label, "");
    ASSERT_EQ(entry.type, 5);
}

TEST(game_entry_malformed_json_returns_defaults) {
    TestGameEntry entry = test_entry_from_json("not valid json");
    ASSERT_STREQ(entry.label, "");
    ASSERT_EQ(entry.type, 5);
}

TEST(game_entry_valid_json) {
    TestGameEntry entry = test_entry_from_json("{\"label\":\"My Game\",\"type\":1}");
    ASSERT_STREQ(entry.label, "My Game");
    ASSERT_EQ(entry.type, 1);
}

/* ================================================================== */
/*  Test: file_removeExtension NULL-safe snprintf pattern             */
/* ================================================================== */

TEST(snprintf_valid_extension_removal) {
    char buf[256] = "";
    const char *dir = "/mnt/SDCARD/Roms/GBA/Imgs";

    char *no_ext = file_removeExtension("game.gba");
    if (no_ext != NULL) {
        snprintf(buf, sizeof(buf), "%s/%s.png", dir, no_ext);
        free(no_ext);
    }
    ASSERT_STREQ(buf, "/mnt/SDCARD/Roms/GBA/Imgs/game.png");
}

/* settings_load() with a broken system.json: test_settings_merge.c (the two
 * tests here re-enacted the pattern on local variables). */

/* ================================================================== */
/*  main                                                              */
/* ================================================================== */

int main(void)
{
    printf("\n=== cJSON NULL Safety Tests (Round 3) ===\n\n");

    /* cJSON library NULL safety */
    RUN_TEST(cjson_parse_null_returns_null);
    RUN_TEST(cjson_parse_empty_returns_null);
    RUN_TEST(cjson_parse_garbage_returns_null);
    RUN_TEST(cjson_get_object_item_null_root);
    RUN_TEST(cjson_delete_null_safe);

    /* Fixed cJSON_Parse guard pattern */
    RUN_TEST(cjson_parse_valid_json);

    /* JsonGameEntry defaults on parse failure */
    RUN_TEST(game_entry_null_json_returns_defaults);
    RUN_TEST(game_entry_malformed_json_returns_defaults);
    RUN_TEST(game_entry_valid_json);

    /* file_removeExtension NULL-safe snprintf */
    RUN_TEST(snprintf_valid_extension_removal);

    /* Settings load pattern */

    TEST_REPORT();
    return test_failures;
}
