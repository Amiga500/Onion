/**
 * @file test_theme_config.c
 * @brief Unit tests for src/common/theme/config.h
 *
 * Tests the pure-logic theme configuration functions: json_color,
 * json_fontStyle, and theme_applyConfig (via temp files).
 *
 * SDL types and theme load constants are stubbed to avoid pulling in
 * the full SDL dependency.
 *
 * Build and run: make -f Makefile.unit test_theme_config
 */

#include "onion_test.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* ---- Stub SDL types (same approach as test_color.c) ---- */

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char unused;
} SDL_Color;

typedef unsigned int Uint32;

/* ---- Stub SDL headers to satisfy includes ---- */
#define _SDL_H
#define _SDL_image_h
#define _SDL_TTF_H

/* ---- Provide constants from theme/load.h ---- */
#define FALLBACK_FONT "/customer/app/Exo-2-Bold-Italic.ttf"
#define FALLBACK_PATH "/mnt/SDCARD/miyoo/app/"
#define THEME_OVERRIDES "/mnt/SDCARD/Saves/CurrentProfile/theme"

/* ---- Include the real source dependencies ---- */
#include "../src/common/utils/str.h"
#include "../src/common/utils/json.h"

/* ---- Inline color.h pure functions ---- */

static SDL_Color hex2sdl(char *input)
{
    char *ptr;
    if (input[0] == '#')
        input++;
    unsigned long value = strtoul(input, &ptr, 16);
    SDL_Color color = {(value >> 16) & 0xff, (value >> 8) & 0xff,
                       (value >> 0) & 0xff, 0};
    return color;
}

/* ---- Inline theme/config.h structs and pure functions ---- */

typedef enum TextAlign {
    LEFT,
    CENTER,
    RIGHT
} Theme_TextAlign;

typedef struct Theme_BatteryPercentage {
    bool visible;
    char font[STR_MAX];
    int size;
    SDL_Color color;
    int offsetX;
    int offsetY;
    Theme_TextAlign textAlign;
    bool fixed;
} BatteryPercentage_s;

typedef struct Theme_Frame {
    int border_left;
    int border_right;
} Frame_s;

typedef struct Theme_HideLabels {
    bool icons;
    bool hints;
} HideLabels_s;

typedef struct Theme_FontStyle {
    char font[STR_MAX];
    int size;
    SDL_Color color;
} FontStyle_s;

typedef struct Theme_GridStyle {
    char font[STR_MAX];
    int grid1x4;
    int grid3x4;
    SDL_Color color;
    SDL_Color selectedcolor;
} GridStyle_s;

typedef struct Theme {
    char path[STR_MAX];
    char name[STR_MAX];
    char author[STR_MAX];
    char description[STR_MAX];
    HideLabels_s hideLabels;
    BatteryPercentage_s batteryPercentage;
    Frame_s frame;
    FontStyle_s title;
    FontStyle_s hint;
    FontStyle_s currentpage;
    FontStyle_s total;
    GridStyle_s grid;
    FontStyle_s list;
} Theme_s;

/* ---- Inline the pure-logic functions from theme/config.h ---- */

static bool json_color(cJSON *root, const char *key, SDL_Color *dest)
{
    cJSON *json_object = cJSON_GetObjectItem(root, key);
    if (json_object) {
        *dest = hex2sdl(cJSON_GetStringValue(json_object));
        return true;
    }
    return false;
}

static void json_fontStyle(cJSON *root, FontStyle_s *dest, FontStyle_s *fallback)
{
    if (!json_getString(root, "font", dest->font) && fallback) {
        strncpy(dest->font, fallback->font, STR_MAX - 1);
        dest->font[STR_MAX - 1] = '\0';
    }
    if (!json_getInt(root, "size", &dest->size) && fallback)
        dest->size = fallback->size;
    if (!json_color(root, "color", &dest->color) && fallback)
        dest->color = fallback->color;
}

static bool theme_applyConfig(Theme_s *config, const char *config_path,
                               bool use_fallbacks)
{
    char *json_str = NULL;

    if (!exists(config_path) || !(json_str = file_read(config_path)))
        return false;

    cJSON *json_root = cJSON_Parse(json_str);
    free(json_str);
    cJSON *json_batteryPercentage =
        cJSON_GetObjectItem(json_root, "batteryPercentage");
    cJSON *json_hideLabels = cJSON_GetObjectItem(json_root, "hideLabels");
    cJSON *json_frame = cJSON_GetObjectItem(json_root, "frame");
    cJSON *json_title = cJSON_GetObjectItem(json_root, "title");
    cJSON *json_hint = cJSON_GetObjectItem(json_root, "hint");
    cJSON *json_currentpage = cJSON_GetObjectItem(json_root, "currentpage");
    cJSON *json_total = cJSON_GetObjectItem(json_root, "total");
    cJSON *json_grid = cJSON_GetObjectItem(json_root, "grid");
    cJSON *json_list = cJSON_GetObjectItem(json_root, "list");

    json_getString(json_root, "name", config->name);
    json_getString(json_root, "author", config->author);
    json_getString(json_root, "description", config->description);

    if (json_hideLabels) {
        json_getBool(json_hideLabels, "icons", &config->hideLabels.icons);
        json_getBool(json_hideLabels, "hints", &config->hideLabels.hints);
    }
    else {
        bool value = false;
        if (json_getBool(json_root, "hideIconTitle", &value)) {
            config->hideLabels.icons = value;
            config->hideLabels.hints = value;
        }
    }

    json_fontStyle(json_title, &config->title, NULL);
    json_fontStyle(json_hint, &config->hint, use_fallbacks ? &config->title : NULL);
    json_fontStyle(json_currentpage, &config->currentpage, use_fallbacks ? &config->hint : NULL);
    json_fontStyle(json_total, &config->total, use_fallbacks ? &config->hint : NULL);
    json_fontStyle(json_list, &config->list, use_fallbacks ? &config->title : NULL);

    json_getString(json_grid, "font", config->grid.font);
    json_getInt(json_grid, "grid1x4", &config->grid.grid1x4);
    json_getInt(json_grid, "grid3x4", &config->grid.grid3x4);
    json_color(json_grid, "color", &config->grid.color);
    json_color(json_grid, "selectedcolor", &config->grid.selectedcolor);

    json_getBool(json_batteryPercentage, "visible", &config->batteryPercentage.visible);

    if (!json_getString(json_batteryPercentage, "font", config->batteryPercentage.font) && use_fallbacks) {
        strncpy(config->batteryPercentage.font, config->hint.font, STR_MAX - 1);
        config->batteryPercentage.font[STR_MAX - 1] = '\0';
    }

    json_getInt(json_batteryPercentage, "size", &config->batteryPercentage.size);

    if (!json_color(json_batteryPercentage, "color", &config->batteryPercentage.color) && use_fallbacks)
        config->batteryPercentage.color = config->hint.color;

    json_getInt(json_batteryPercentage, "offsetX", &config->batteryPercentage.offsetX);
    json_getInt(json_batteryPercentage, "offsetY", &config->batteryPercentage.offsetY);

    char textAlign_str[JSON_STRING_LEN];
    if (json_getString(json_batteryPercentage, "textAlign", textAlign_str)) {
        if (strcmp("center", textAlign_str) == 0)
            config->batteryPercentage.textAlign = CENTER;
        else if (strcmp("right", textAlign_str) == 0)
            config->batteryPercentage.textAlign = RIGHT;
        else
            config->batteryPercentage.textAlign = LEFT;
    }
    else {
        bool depr_onleft = false;
        if (json_getBool(json_batteryPercentage, "onleft", &depr_onleft))
            config->batteryPercentage.textAlign = depr_onleft ? RIGHT : LEFT;
    }

    json_getBool(json_batteryPercentage, "fixed", &config->batteryPercentage.fixed);

    json_getInt(json_frame, "border-left", &config->frame.border_left);
    json_getInt(json_frame, "border-right", &config->frame.border_right);

    cJSON_Delete(json_root);

    return true;
}

/* ---- Helper: write string to a temp file ---- */

static char _tmp_path[64];
static char *write_temp_json(const char *json_str)
{
    strcpy(_tmp_path, "/tmp/test_theme_cfg_XXXXXX");
    int fd = mkstemp(_tmp_path);
    if (fd < 0) return NULL;
    write(fd, json_str, strlen(json_str));
    close(fd);
    return _tmp_path;
}

/* ---- Helper: initialize a default Theme_s ---- */

static Theme_s make_default_config(void)
{
    Theme_s config = {
        .path = FALLBACK_PATH,
        .name = "",
        .author = "",
        .description = "",
        .hideLabels = {.icons = false, .hints = false},
        .batteryPercentage = {.visible = false,
                              .font = FALLBACK_FONT,
                              .size = 24,
                              .color = {255, 255, 255},
                              .offsetX = 0,
                              .offsetY = 0,
                              .textAlign = LEFT,
                              .fixed = false},
        .frame = {.border_left = 0, .border_right = 0},
        .title = {.font = FALLBACK_FONT, .size = 36, .color = {255, 255, 255}},
        .hint = {.font = FALLBACK_FONT, .size = 40, .color = {255, 255, 255}},
        .currentpage = {.color = {255, 255, 255}},
        .total = {.color = {255, 255, 255}},
        .grid = {.font = FALLBACK_FONT,
                 .grid1x4 = 24,
                 .grid3x4 = 18,
                 .color = {104, 104, 104},
                 .selectedcolor = {255, 255, 255}},
        .list = {.font = FALLBACK_FONT, .size = 24, .color = {255, 255, 255}}};
    return config;
}

/* ==== json_color tests ==== */

TEST(json_color_valid_hex) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "color", "#FF0000");
    SDL_Color c = {0};
    bool result = json_color(root, "color", &c);
    ASSERT_TRUE(result);
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);
    cJSON_Delete(root);
}

TEST(json_color_valid_no_hash) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "color", "00FF00");
    SDL_Color c = {0};
    bool result = json_color(root, "color", &c);
    ASSERT_TRUE(result);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 255);
    ASSERT_EQ(c.b, 0);
    cJSON_Delete(root);
}

TEST(json_color_white) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "color", "#FFFFFF");
    SDL_Color c = {0};
    bool result = json_color(root, "color", &c);
    ASSERT_TRUE(result);
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 255);
    ASSERT_EQ(c.b, 255);
    cJSON_Delete(root);
}

TEST(json_color_black) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "color", "#000000");
    SDL_Color c = {0};
    bool result = json_color(root, "color", &c);
    ASSERT_TRUE(result);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);
    cJSON_Delete(root);
}

TEST(json_color_missing_key) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "other", "#FF0000");
    SDL_Color c = {0};
    bool result = json_color(root, "color", &c);
    ASSERT_FALSE(result);
    cJSON_Delete(root);
}

TEST(json_color_arbitrary) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "mycolor", "#1CD577");
    SDL_Color c = {0};
    bool result = json_color(root, "mycolor", &c);
    ASSERT_TRUE(result);
    ASSERT_EQ(c.r, 0x1C);
    ASSERT_EQ(c.g, 0xD5);
    ASSERT_EQ(c.b, 0x77);
    cJSON_Delete(root);
}

/* ==== json_fontStyle tests ==== */

TEST(fontStyle_all_fields) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "font", "MyFont.ttf");
    cJSON_AddNumberToObject(root, "size", 28);
    cJSON_AddStringToObject(root, "color", "#FF8800");
    FontStyle_s dest = {0};
    json_fontStyle(root, &dest, NULL);
    ASSERT_STREQ(dest.font, "MyFont.ttf");
    ASSERT_EQ(dest.size, 28);
    ASSERT_EQ(dest.color.r, 0xFF);
    ASSERT_EQ(dest.color.g, 0x88);
    ASSERT_EQ(dest.color.b, 0x00);
    cJSON_Delete(root);
}

TEST(fontStyle_fallback_all) {
    cJSON *root = cJSON_CreateObject();
    /* empty root - no font, size, or color */
    FontStyle_s fallback = {.font = "Fallback.ttf", .size = 16, .color = {10, 20, 30}};
    FontStyle_s dest = {0};
    json_fontStyle(root, &dest, &fallback);
    ASSERT_STREQ(dest.font, "Fallback.ttf");
    ASSERT_EQ(dest.size, 16);
    ASSERT_EQ(dest.color.r, 10);
    ASSERT_EQ(dest.color.g, 20);
    ASSERT_EQ(dest.color.b, 30);
    cJSON_Delete(root);
}

TEST(fontStyle_partial_override) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "size", 48);
    /* font and color missing - should use fallback */
    FontStyle_s fallback = {.font = "Base.ttf", .size = 12, .color = {1, 2, 3}};
    FontStyle_s dest = {0};
    json_fontStyle(root, &dest, &fallback);
    ASSERT_STREQ(dest.font, "Base.ttf");
    ASSERT_EQ(dest.size, 48);
    ASSERT_EQ(dest.color.r, 1);
    ASSERT_EQ(dest.color.g, 2);
    ASSERT_EQ(dest.color.b, 3);
    cJSON_Delete(root);
}

TEST(fontStyle_no_fallback_missing_fields) {
    cJSON *root = cJSON_CreateObject();
    /* empty root, no fallback */
    FontStyle_s dest = {.font = "unchanged", .size = 99, .color = {11, 22, 33}};
    json_fontStyle(root, &dest, NULL);
    /* Fields should remain unchanged since no JSON data and no fallback */
    ASSERT_STREQ(dest.font, "unchanged");
    ASSERT_EQ(dest.size, 99);
    ASSERT_EQ(dest.color.r, 11);
    ASSERT_EQ(dest.color.g, 22);
    ASSERT_EQ(dest.color.b, 33);
    cJSON_Delete(root);
}

TEST(fontStyle_font_only_in_json) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "font", "Custom.ttf");
    FontStyle_s fallback = {.font = "Old.ttf", .size = 20, .color = {50, 60, 70}};
    FontStyle_s dest = {0};
    json_fontStyle(root, &dest, &fallback);
    ASSERT_STREQ(dest.font, "Custom.ttf");
    ASSERT_EQ(dest.size, 20);
    ASSERT_EQ(dest.color.r, 50);
    cJSON_Delete(root);
}

TEST(fontStyle_color_only_in_json) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "color", "#AABBCC");
    FontStyle_s fallback = {.font = "Fb.ttf", .size = 14, .color = {0, 0, 0}};
    FontStyle_s dest = {0};
    json_fontStyle(root, &dest, &fallback);
    ASSERT_STREQ(dest.font, "Fb.ttf");
    ASSERT_EQ(dest.size, 14);
    ASSERT_EQ(dest.color.r, 0xAA);
    ASSERT_EQ(dest.color.g, 0xBB);
    ASSERT_EQ(dest.color.b, 0xCC);
    cJSON_Delete(root);
}

/* ==== theme_applyConfig tests ==== */

TEST(applyConfig_nonexistent_file) {
    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, "/tmp/nonexistent_theme_config.json", true);
    ASSERT_FALSE(result);
}

TEST(applyConfig_basic_metadata) {
    const char *json = "{"
        "\"name\": \"TestTheme\","
        "\"author\": \"TestAuthor\","
        "\"description\": \"A test theme\""
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, true);
    ASSERT_TRUE(result);
    ASSERT_STREQ(config.name, "TestTheme");
    ASSERT_STREQ(config.author, "TestAuthor");
    ASSERT_STREQ(config.description, "A test theme");
    unlink(path);
}

TEST(applyConfig_title_font_style) {
    const char *json = "{"
        "\"title\": {"
        "  \"font\": \"title.ttf\","
        "  \"size\": 42,"
        "  \"color\": \"#FF0000\""
        "}"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, true);
    ASSERT_TRUE(result);
    ASSERT_STREQ(config.title.font, "title.ttf");
    ASSERT_EQ(config.title.size, 42);
    ASSERT_EQ(config.title.color.r, 0xFF);
    ASSERT_EQ(config.title.color.g, 0x00);
    ASSERT_EQ(config.title.color.b, 0x00);
    unlink(path);
}

TEST(applyConfig_hint_fallback_to_title) {
    const char *json = "{"
        "\"title\": {"
        "  \"font\": \"title.ttf\","
        "  \"size\": 36,"
        "  \"color\": \"#112233\""
        "}"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    /* hint has defaults; with use_fallbacks=true, hint should inherit title */
    bool result = theme_applyConfig(&config, path, true);
    ASSERT_TRUE(result);
    ASSERT_STREQ(config.hint.font, "title.ttf");
    ASSERT_EQ(config.hint.size, 36);
    ASSERT_EQ(config.hint.color.r, 0x11);
    ASSERT_EQ(config.hint.color.g, 0x22);
    ASSERT_EQ(config.hint.color.b, 0x33);
    unlink(path);
}

TEST(applyConfig_hint_no_fallback) {
    const char *json = "{"
        "\"title\": {"
        "  \"font\": \"title.ttf\","
        "  \"size\": 36,"
        "  \"color\": \"#112233\""
        "}"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    /* use_fallbacks=false: hint should NOT inherit from title */
    bool result = theme_applyConfig(&config, path, false);
    ASSERT_TRUE(result);
    /* hint should remain at default since no hint section and no fallback */
    ASSERT_STREQ(config.hint.font, FALLBACK_FONT);
    unlink(path);
}

TEST(applyConfig_hideLabels) {
    const char *json = "{"
        "\"hideLabels\": {"
        "  \"icons\": true,"
        "  \"hints\": true"
        "}"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, true);
    ASSERT_TRUE(result);
    ASSERT_TRUE(config.hideLabels.icons);
    ASSERT_TRUE(config.hideLabels.hints);
    unlink(path);
}

TEST(applyConfig_hideIconTitle_backward_compat) {
    const char *json = "{"
        "\"hideIconTitle\": true"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, true);
    ASSERT_TRUE(result);
    ASSERT_TRUE(config.hideLabels.icons);
    ASSERT_TRUE(config.hideLabels.hints);
    unlink(path);
}

TEST(applyConfig_frame) {
    const char *json = "{"
        "\"frame\": {"
        "  \"border-left\": 10,"
        "  \"border-right\": 20"
        "}"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, true);
    ASSERT_TRUE(result);
    ASSERT_EQ(config.frame.border_left, 10);
    ASSERT_EQ(config.frame.border_right, 20);
    unlink(path);
}

TEST(applyConfig_battery_percentage) {
    const char *json = "{"
        "\"batteryPercentage\": {"
        "  \"visible\": true,"
        "  \"font\": \"bat.ttf\","
        "  \"size\": 18,"
        "  \"color\": \"#AABB00\","
        "  \"offsetX\": 5,"
        "  \"offsetY\": -3,"
        "  \"textAlign\": \"center\","
        "  \"fixed\": true"
        "}"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, false);
    ASSERT_TRUE(result);
    ASSERT_TRUE(config.batteryPercentage.visible);
    ASSERT_STREQ(config.batteryPercentage.font, "bat.ttf");
    ASSERT_EQ(config.batteryPercentage.size, 18);
    ASSERT_EQ(config.batteryPercentage.color.r, 0xAA);
    ASSERT_EQ(config.batteryPercentage.color.g, 0xBB);
    ASSERT_EQ(config.batteryPercentage.color.b, 0x00);
    ASSERT_EQ(config.batteryPercentage.offsetX, 5);
    ASSERT_EQ(config.batteryPercentage.offsetY, -3);
    ASSERT_EQ(config.batteryPercentage.textAlign, CENTER);
    ASSERT_TRUE(config.batteryPercentage.fixed);
    unlink(path);
}

TEST(applyConfig_textAlign_right) {
    const char *json = "{"
        "\"batteryPercentage\": {"
        "  \"textAlign\": \"right\""
        "}"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, false);
    ASSERT_TRUE(result);
    ASSERT_EQ(config.batteryPercentage.textAlign, RIGHT);
    unlink(path);
}

TEST(applyConfig_textAlign_left_default) {
    const char *json = "{"
        "\"batteryPercentage\": {"
        "  \"textAlign\": \"left\""
        "}"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, false);
    ASSERT_TRUE(result);
    ASSERT_EQ(config.batteryPercentage.textAlign, LEFT);
    unlink(path);
}

TEST(applyConfig_textAlign_onleft_deprecated) {
    const char *json = "{"
        "\"batteryPercentage\": {"
        "  \"onleft\": true"
        "}"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, false);
    ASSERT_TRUE(result);
    ASSERT_EQ(config.batteryPercentage.textAlign, RIGHT);
    unlink(path);
}

TEST(applyConfig_grid) {
    const char *json = "{"
        "\"grid\": {"
        "  \"font\": \"grid.ttf\","
        "  \"grid1x4\": 30,"
        "  \"grid3x4\": 22,"
        "  \"color\": \"#686868\","
        "  \"selectedcolor\": \"#FFFFFF\""
        "}"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, true);
    ASSERT_TRUE(result);
    ASSERT_STREQ(config.grid.font, "grid.ttf");
    ASSERT_EQ(config.grid.grid1x4, 30);
    ASSERT_EQ(config.grid.grid3x4, 22);
    ASSERT_EQ(config.grid.color.r, 0x68);
    ASSERT_EQ(config.grid.color.g, 0x68);
    ASSERT_EQ(config.grid.color.b, 0x68);
    ASSERT_EQ(config.grid.selectedcolor.r, 0xFF);
    ASSERT_EQ(config.grid.selectedcolor.g, 0xFF);
    ASSERT_EQ(config.grid.selectedcolor.b, 0xFF);
    unlink(path);
}

TEST(applyConfig_full_config) {
    const char *json = "{"
        "\"name\": \"FullTheme\","
        "\"author\": \"Dev\","
        "\"description\": \"Full test\","
        "\"hideLabels\": { \"icons\": false, \"hints\": true },"
        "\"title\": { \"font\": \"t.ttf\", \"size\": 30, \"color\": \"#AA0000\" },"
        "\"hint\": { \"font\": \"h.ttf\", \"size\": 20, \"color\": \"#00AA00\" },"
        "\"currentpage\": { \"size\": 15, \"color\": \"#0000AA\" },"
        "\"total\": { \"color\": \"#CCCCCC\" },"
        "\"list\": { \"font\": \"l.ttf\", \"size\": 22, \"color\": \"#333333\" },"
        "\"grid\": { \"font\": \"g.ttf\", \"grid1x4\": 26, \"grid3x4\": 16, \"color\": \"#444444\", \"selectedcolor\": \"#EEEEEE\" },"
        "\"frame\": { \"border-left\": 5, \"border-right\": 8 },"
        "\"batteryPercentage\": { \"visible\": true, \"font\": \"b.ttf\", \"size\": 12, \"color\": \"#DDDDDD\", \"offsetX\": 2, \"offsetY\": -1, \"textAlign\": \"center\", \"fixed\": true }"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, false);
    ASSERT_TRUE(result);
    ASSERT_STREQ(config.name, "FullTheme");
    ASSERT_STREQ(config.author, "Dev");
    ASSERT_FALSE(config.hideLabels.icons);
    ASSERT_TRUE(config.hideLabels.hints);
    ASSERT_STREQ(config.title.font, "t.ttf");
    ASSERT_EQ(config.title.size, 30);
    ASSERT_STREQ(config.hint.font, "h.ttf");
    ASSERT_EQ(config.hint.size, 20);
    ASSERT_EQ(config.currentpage.size, 15);
    ASSERT_EQ(config.currentpage.color.r, 0x00);
    ASSERT_EQ(config.currentpage.color.g, 0x00);
    ASSERT_EQ(config.currentpage.color.b, 0xAA);
    ASSERT_STREQ(config.list.font, "l.ttf");
    ASSERT_EQ(config.list.size, 22);
    ASSERT_EQ(config.frame.border_left, 5);
    ASSERT_EQ(config.frame.border_right, 8);
    ASSERT_TRUE(config.batteryPercentage.visible);
    ASSERT_STREQ(config.batteryPercentage.font, "b.ttf");
    ASSERT_EQ(config.batteryPercentage.size, 12);
    ASSERT_EQ(config.batteryPercentage.textAlign, CENTER);
    ASSERT_TRUE(config.batteryPercentage.fixed);
    unlink(path);
}

TEST(applyConfig_battery_fallback_to_hint) {
    const char *json = "{"
        "\"hint\": { \"font\": \"hint.ttf\", \"size\": 32, \"color\": \"#AABBCC\" },"
        "\"batteryPercentage\": { \"visible\": true }"
        "}";
    char *path = write_temp_json(json);
    ASSERT_NOT_NULL(path);

    Theme_s config = make_default_config();
    bool result = theme_applyConfig(&config, path, true);
    ASSERT_TRUE(result);
    /* battery font should fall back to hint font when use_fallbacks=true */
    ASSERT_STREQ(config.batteryPercentage.font, "hint.ttf");
    /* battery color should fall back to hint color */
    ASSERT_EQ(config.batteryPercentage.color.r, 0xAA);
    ASSERT_EQ(config.batteryPercentage.color.g, 0xBB);
    ASSERT_EQ(config.batteryPercentage.color.b, 0xCC);
    unlink(path);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== theme/config.h Unit Tests ===\n\n");

    /* json_color */
    RUN_TEST(json_color_valid_hex);
    RUN_TEST(json_color_valid_no_hash);
    RUN_TEST(json_color_white);
    RUN_TEST(json_color_black);
    RUN_TEST(json_color_missing_key);
    RUN_TEST(json_color_arbitrary);

    /* json_fontStyle */
    RUN_TEST(fontStyle_all_fields);
    RUN_TEST(fontStyle_fallback_all);
    RUN_TEST(fontStyle_partial_override);
    RUN_TEST(fontStyle_no_fallback_missing_fields);
    RUN_TEST(fontStyle_font_only_in_json);
    RUN_TEST(fontStyle_color_only_in_json);

    /* theme_applyConfig */
    RUN_TEST(applyConfig_nonexistent_file);
    RUN_TEST(applyConfig_basic_metadata);
    RUN_TEST(applyConfig_title_font_style);
    RUN_TEST(applyConfig_hint_fallback_to_title);
    RUN_TEST(applyConfig_hint_no_fallback);
    RUN_TEST(applyConfig_hideLabels);
    RUN_TEST(applyConfig_hideIconTitle_backward_compat);
    RUN_TEST(applyConfig_frame);
    RUN_TEST(applyConfig_battery_percentage);
    RUN_TEST(applyConfig_textAlign_right);
    RUN_TEST(applyConfig_textAlign_left_default);
    RUN_TEST(applyConfig_textAlign_onleft_deprecated);
    RUN_TEST(applyConfig_grid);
    RUN_TEST(applyConfig_full_config);
    RUN_TEST(applyConfig_battery_fallback_to_hint);

    TEST_REPORT();
    return test_failures;
}
