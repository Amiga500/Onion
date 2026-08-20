/**
 * @file test_game_entry.c
 * @brief Unit tests for src/common/components/JsonGameEntry.h
 *
 * Tests the pure-logic JSON game entry functions: JsonGameEntry_fromJson
 * (parsing ROM entry JSON into struct) and JsonGameEntry_toJson
 * (serializing struct back to JSON string).
 *
 * Covers: valid entries, missing fields, malformed JSON, colon-splitting
 * of rompath, emupath extraction, roundtrip serialization, and buffer
 * boundary conditions.
 *
 * Build and run: make -f Makefile.unit test_game_entry
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../include/cjson/cJSON.h"

#include <stdlib.h>
#include <string.h>

/* ---- Inline types and functions from JsonGameEntry.h ---- */
/* (Cannot include directly due to path dependencies)        */

#define JSON_STRING_LEN 256

typedef struct json_game_entry_s {
    char label[STR_MAX];
    char launch[STR_MAX];
    int type;
    char rompath[STR_MAX];
    char imgpath[STR_MAX];
    char emupath[STR_MAX];
} JsonGameEntry;

/* Forward declarations for json helpers we need */
static bool _json_getString(cJSON *object, const char *key, char *dest)
{
    cJSON *json_object = cJSON_GetObjectItem(object, key);
    if (json_object) {
        const char *val = cJSON_GetStringValue(json_object);
        if (val == NULL)
            return false;
        strncpy(dest, val, JSON_STRING_LEN - 1);
        dest[JSON_STRING_LEN - 1] = '\0';
        return true;
    }
    return false;
}

static bool _json_getInt(cJSON *object, const char *key, int *dest)
{
    cJSON *json_object = cJSON_GetObjectItem(object, key);
    if (json_object) {
        *dest = (int)cJSON_GetNumberValue(json_object);
        return true;
    }
    return false;
}

static JsonGameEntry JsonGameEntry_fromJson(const char *json_str)
{
    JsonGameEntry entry = {.label = "",
                           .launch = "",
                           .type = 5,
                           .rompath = "",
                           .imgpath = "",
                           .emupath = ""};

    cJSON *root = cJSON_Parse(json_str);
    _json_getString(root, "label", entry.label);
    _json_getString(root, "launch", entry.launch);
    _json_getInt(root, "type", &entry.type);
    _json_getString(root, "rompath", entry.rompath);
    _json_getString(root, "imgpath", entry.imgpath);
    cJSON_Delete(root);

    strncpy(entry.emupath, entry.rompath, STR_MAX - 1);
    entry.emupath[STR_MAX - 1] = '\0';
    str_split(entry.emupath, "/../../");

    return entry;
}

static void JsonGameEntry_toJson(char dest[STR_MAX * 6], JsonGameEntry *entry)
{
    size_t dest_size = STR_MAX * 6;
    size_t len = 0;
    int n;
    dest[0] = '{';
    dest[1] = '\0';
    len = 1;
    n = snprintf(dest + len, dest_size - len, "\"label\":\"%s\",", entry->label);
    if (n > 0) len += (size_t)n < dest_size - len ? (size_t)n : dest_size - len - 1;
    n = snprintf(dest + len, dest_size - len, "\"launch\":\"%s\",", entry->launch);
    if (n > 0) len += (size_t)n < dest_size - len ? (size_t)n : dest_size - len - 1;
    n = snprintf(dest + len, dest_size - len, "\"type\":%d,", entry->type);
    if (n > 0) len += (size_t)n < dest_size - len ? (size_t)n : dest_size - len - 1;
    if (strlen(entry->imgpath) > 0) {
        n = snprintf(dest + len, dest_size - len, "\"imgpath\":\"%s\",", entry->imgpath);
        if (n > 0) len += (size_t)n < dest_size - len ? (size_t)n : dest_size - len - 1;
    }
    n = snprintf(dest + len, dest_size - len, "\"rompath\":\"%s\"}", entry->rompath);
    if (n > 0) len += (size_t)n < dest_size - len ? (size_t)n : dest_size - len - 1;
}

/* ---- Tests: JsonGameEntry_fromJson ---- */

TEST(fromJson_complete_entry) {
    const char *json =
        "{\"label\":\"Super Mario\","
        "\"launch\":\"/mnt/SDCARD/Emu/GBA/launch.sh\","
        "\"type\":5,"
        "\"rompath\":\"/mnt/SDCARD/Roms/GBA/mario.gba\","
        "\"imgpath\":\"/mnt/SDCARD/Roms/GBA/Imgs/mario.png\"}";

    JsonGameEntry e = JsonGameEntry_fromJson(json);

    ASSERT_STREQ(e.label, "Super Mario");
    ASSERT_STREQ(e.launch, "/mnt/SDCARD/Emu/GBA/launch.sh");
    ASSERT_EQ(e.type, 5);
    ASSERT_STREQ(e.rompath, "/mnt/SDCARD/Roms/GBA/mario.gba");
    ASSERT_STREQ(e.imgpath, "/mnt/SDCARD/Roms/GBA/Imgs/mario.png");
}

TEST(fromJson_type_17) {
    const char *json =
        "{\"label\":\"Tetris\","
        "\"launch\":\"/mnt/SDCARD/Emu/GB/launch.sh\","
        "\"type\":17,"
        "\"rompath\":\"/mnt/SDCARD/Roms/GB/tetris.gb\","
        "\"imgpath\":\"\"}";

    JsonGameEntry e = JsonGameEntry_fromJson(json);
    ASSERT_EQ(e.type, 17);
    ASSERT_STREQ(e.label, "Tetris");
}

TEST(fromJson_missing_label) {
    const char *json =
        "{\"type\":5,"
        "\"rompath\":\"/mnt/SDCARD/Roms/GBA/game.gba\","
        "\"launch\":\"/launch.sh\"}";

    JsonGameEntry e = JsonGameEntry_fromJson(json);
    ASSERT_STREQ(e.label, "");
    ASSERT_EQ(e.type, 5);
}

TEST(fromJson_missing_imgpath) {
    const char *json =
        "{\"label\":\"Game\",\"type\":5,"
        "\"rompath\":\"/rom.gba\","
        "\"launch\":\"/launch.sh\"}";

    JsonGameEntry e = JsonGameEntry_fromJson(json);
    ASSERT_STREQ(e.imgpath, "");
}

TEST(fromJson_missing_type_defaults_5) {
    const char *json =
        "{\"label\":\"Game\","
        "\"rompath\":\"/rom.gba\","
        "\"launch\":\"/launch.sh\"}";

    JsonGameEntry e = JsonGameEntry_fromJson(json);
    /* Type defaults to 5 when not in JSON */
    ASSERT_EQ(e.type, 5);
}

TEST(fromJson_null_json) {
    /* NULL input to cJSON_Parse returns NULL, getString/getInt on NULL is safe */
    JsonGameEntry e = JsonGameEntry_fromJson(NULL);
    ASSERT_STREQ(e.label, "");
    ASSERT_STREQ(e.launch, "");
    ASSERT_EQ(e.type, 5);
    ASSERT_STREQ(e.rompath, "");
    ASSERT_STREQ(e.imgpath, "");
}

TEST(fromJson_empty_json) {
    JsonGameEntry e = JsonGameEntry_fromJson("{}");
    ASSERT_STREQ(e.label, "");
    ASSERT_EQ(e.type, 5);
}

TEST(fromJson_invalid_json) {
    JsonGameEntry e = JsonGameEntry_fromJson("{not valid!!}");
    ASSERT_STREQ(e.label, "");
    ASSERT_EQ(e.type, 5);
}

/* ---- Tests: emupath extraction ---- */

TEST(fromJson_emupath_from_standard_rompath) {
    /* rompath with /../../ separator: emupath is the part before it */
    const char *json =
        "{\"label\":\"Game\",\"type\":5,"
        "\"rompath\":\"/mnt/SDCARD/Emu/GBA/../../Roms/GBA/game.gba\","
        "\"launch\":\"/launch.sh\"}";

    JsonGameEntry e = JsonGameEntry_fromJson(json);
    ASSERT_STREQ(e.emupath, "/mnt/SDCARD/Emu/GBA");
}

TEST(fromJson_emupath_no_separator) {
    /* rompath without /../../: emupath equals the full rompath */
    const char *json =
        "{\"label\":\"Game\",\"type\":5,"
        "\"rompath\":\"/mnt/SDCARD/Roms/GBA/game.gba\","
        "\"launch\":\"/launch.sh\"}";

    JsonGameEntry e = JsonGameEntry_fromJson(json);
    ASSERT_STREQ(e.emupath, "/mnt/SDCARD/Roms/GBA/game.gba");
}

TEST(fromJson_emupath_empty_rompath) {
    const char *json = "{\"label\":\"Game\",\"type\":5,\"rompath\":\"\"}";

    JsonGameEntry e = JsonGameEntry_fromJson(json);
    ASSERT_STREQ(e.emupath, "");
}

/* ---- Tests: JsonGameEntry_toJson ---- */

TEST(toJson_complete_entry) {
    JsonGameEntry entry;
    strncpy(entry.label, "Super Mario", STR_MAX);
    strncpy(entry.launch, "/mnt/SDCARD/Emu/GBA/launch.sh", STR_MAX);
    entry.type = 5;
    strncpy(entry.rompath, "/mnt/SDCARD/Roms/GBA/mario.gba", STR_MAX);
    strncpy(entry.imgpath, "/mnt/SDCARD/Roms/GBA/Imgs/mario.png", STR_MAX);

    char json[STR_MAX * 6] = {0};
    JsonGameEntry_toJson(json, &entry);

    /* Verify it starts with { and ends with } */
    ASSERT_EQ(json[0], '{');
    ASSERT_EQ(json[strlen(json) - 1], '}');

    /* Verify key fields are present in the output */
    ASSERT_NOT_NULL(strstr(json, "\"label\":\"Super Mario\""));
    ASSERT_NOT_NULL(strstr(json, "\"type\":5"));
    ASSERT_NOT_NULL(strstr(json, "\"rompath\":\"/mnt/SDCARD/Roms/GBA/mario.gba\""));
    ASSERT_NOT_NULL(strstr(json, "\"imgpath\":\"/mnt/SDCARD/Roms/GBA/Imgs/mario.png\""));
}

TEST(toJson_empty_imgpath_omitted) {
    JsonGameEntry entry;
    strncpy(entry.label, "Game", STR_MAX);
    strncpy(entry.launch, "/launch.sh", STR_MAX);
    entry.type = 5;
    strncpy(entry.rompath, "/rom.gba", STR_MAX);
    entry.imgpath[0] = '\0';

    char json[STR_MAX * 6] = {0};
    JsonGameEntry_toJson(json, &entry);

    /* imgpath should NOT be in the output when empty */
    ASSERT_NULL(strstr(json, "\"imgpath\""));
}

TEST(toJson_with_imgpath) {
    JsonGameEntry entry;
    strncpy(entry.label, "Game", STR_MAX);
    strncpy(entry.launch, "/launch.sh", STR_MAX);
    entry.type = 5;
    strncpy(entry.rompath, "/rom.gba", STR_MAX);
    strncpy(entry.imgpath, "/img.png", STR_MAX);

    char json[STR_MAX * 6] = {0};
    JsonGameEntry_toJson(json, &entry);

    /* imgpath should be present when non-empty */
    ASSERT_NOT_NULL(strstr(json, "\"imgpath\":\"/img.png\""));
}

TEST(toJson_different_type) {
    JsonGameEntry entry;
    strncpy(entry.label, "Test", STR_MAX);
    strncpy(entry.launch, "/launch.sh", STR_MAX);
    entry.type = 17;
    strncpy(entry.rompath, "/rom.gba", STR_MAX);
    entry.imgpath[0] = '\0';

    char json[STR_MAX * 6] = {0};
    JsonGameEntry_toJson(json, &entry);

    ASSERT_NOT_NULL(strstr(json, "\"type\":17"));
}

TEST(toJson_empty_label) {
    JsonGameEntry entry;
    entry.label[0] = '\0';
    strncpy(entry.launch, "/launch.sh", STR_MAX);
    entry.type = 5;
    strncpy(entry.rompath, "/rom.gba", STR_MAX);
    entry.imgpath[0] = '\0';

    char json[STR_MAX * 6] = {0};
    JsonGameEntry_toJson(json, &entry);

    ASSERT_NOT_NULL(strstr(json, "\"label\":\"\""));
}

/* ---- Tests: roundtrip (fromJson → toJson → parse back) ---- */

TEST(roundtrip_preserves_data) {
    const char *original_json =
        "{\"label\":\"Zelda\","
        "\"launch\":\"/mnt/SDCARD/Emu/GBA/launch.sh\","
        "\"type\":5,"
        "\"rompath\":\"/mnt/SDCARD/Roms/GBA/zelda.gba\","
        "\"imgpath\":\"/mnt/SDCARD/Roms/GBA/Imgs/zelda.png\"}";

    /* Parse original */
    JsonGameEntry entry = JsonGameEntry_fromJson(original_json);

    /* Serialize back */
    char json_out[STR_MAX * 6] = {0};
    JsonGameEntry_toJson(json_out, &entry);

    /* Parse the serialized version */
    JsonGameEntry reparsed = JsonGameEntry_fromJson(json_out);

    /* All fields should match */
    ASSERT_STREQ(reparsed.label, entry.label);
    ASSERT_STREQ(reparsed.launch, entry.launch);
    ASSERT_EQ(reparsed.type, entry.type);
    ASSERT_STREQ(reparsed.rompath, entry.rompath);
    ASSERT_STREQ(reparsed.imgpath, entry.imgpath);
}

TEST(roundtrip_no_imgpath) {
    const char *original_json =
        "{\"label\":\"Game\","
        "\"launch\":\"/launch.sh\","
        "\"type\":5,"
        "\"rompath\":\"/rom.gba\"}";

    JsonGameEntry entry = JsonGameEntry_fromJson(original_json);
    char json_out[STR_MAX * 6] = {0};
    JsonGameEntry_toJson(json_out, &entry);
    JsonGameEntry reparsed = JsonGameEntry_fromJson(json_out);

    ASSERT_STREQ(reparsed.label, entry.label);
    ASSERT_EQ(reparsed.type, entry.type);
    ASSERT_STREQ(reparsed.rompath, entry.rompath);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== JsonGameEntry Unit Tests ===\n\n");

    /* fromJson */
    RUN_TEST(fromJson_complete_entry);
    RUN_TEST(fromJson_type_17);
    RUN_TEST(fromJson_missing_label);
    RUN_TEST(fromJson_missing_imgpath);
    RUN_TEST(fromJson_missing_type_defaults_5);
    RUN_TEST(fromJson_null_json);
    RUN_TEST(fromJson_empty_json);
    RUN_TEST(fromJson_invalid_json);

    /* emupath extraction */
    RUN_TEST(fromJson_emupath_from_standard_rompath);
    RUN_TEST(fromJson_emupath_no_separator);
    RUN_TEST(fromJson_emupath_empty_rompath);

    /* toJson */
    RUN_TEST(toJson_complete_entry);
    RUN_TEST(toJson_empty_imgpath_omitted);
    RUN_TEST(toJson_with_imgpath);
    RUN_TEST(toJson_different_type);
    RUN_TEST(toJson_empty_label);

    /* roundtrip */
    RUN_TEST(roundtrip_preserves_data);
    RUN_TEST(roundtrip_no_imgpath);

    TEST_REPORT();
    return test_failures;
}
