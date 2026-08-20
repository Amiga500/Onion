/**
 * @file test_theme_scale.c
 * @brief Unit tests for theme_scaleRect() and theme_getImagePath()
 *        from theme/load.h
 *
 * Tests the pure-math rectangle scaling function and the path
 * resolution logic for theme images. theme_scaleRect() multiplies
 * each field of an SDL_Rect by g_scale. theme_getImagePath() checks
 * override → theme → fallback paths in order.
 *
 * Build and run: make -f Makefile.unit test_theme_scale
 */

#include "onion_test.h"
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#define STR_MAX 256

/* ---- Minimal SDL_Rect replacement ---- */

typedef struct {
    int x, y, w, h;
} TestRect;

/* ---- Inline theme_scaleRect from load.h ---- */

static double g_scale = 1.0;

static TestRect theme_scaleRect(TestRect rect)
{
    if (g_scale == 1.0)
        return rect;
    rect.x = (double)rect.x * g_scale;
    rect.y = (double)rect.y * g_scale;
    rect.w = (double)rect.w * g_scale;
    rect.h = (double)rect.h * g_scale;
    return rect;
}

/* ---- Inline theme_getImagePath path logic ---- */

/* Stub for exists() — we control which paths "exist" */
static char _existing_paths[10][STR_MAX * 2];
static int _existing_count = 0;

static bool stub_exists(const char *path)
{
    for (int i = 0; i < _existing_count; i++) {
        if (strcmp(_existing_paths[i], path) == 0)
            return true;
    }
    return false;
}

static void stub_reset(void)
{
    _existing_count = 0;
}

static void stub_add_path(const char *path)
{
    if (_existing_count < 10) {
        strncpy(_existing_paths[_existing_count], path, STR_MAX * 2 - 1);
        _existing_paths[_existing_count][STR_MAX * 2 - 1] = '\0';
        _existing_count++;
    }
}

#define THEME_OVERRIDES "/mnt/SDCARD/Saves/CurrentProfile/theme"
#define SYSTEM_RESOURCES "/mnt/SDCARD/.tmp_update/res/"
#define FALLBACK_PATH "/mnt/SDCARD/miyoo/app/"

static int theme_getImagePath(const char *theme_path, const char *name, char *out_path)
{
    int load_mode = 2;
    char rel_path[STR_MAX], image_path[STR_MAX * 2];
    snprintf(rel_path, sizeof(rel_path), "skin/%s.png", name);

    snprintf(image_path, sizeof(image_path), THEME_OVERRIDES "/%s", rel_path);
    bool override_exists = stub_exists(image_path);

    if (!override_exists) {
        load_mode = 1;
        snprintf(image_path, sizeof(image_path), "%s%s", theme_path, rel_path);
        bool theme_exists = stub_exists(image_path);

        if (!theme_exists) {
            load_mode = 0;
            if (strncmp(name, "extra/", 6) == 0) {
                snprintf(rel_path, sizeof(rel_path), "%s.png", name + 6);
                snprintf(image_path, sizeof(image_path), "%s%s", SYSTEM_RESOURCES, rel_path);
            }
            else {
                snprintf(image_path, sizeof(image_path), "%s%s", FALLBACK_PATH, rel_path);
            }
        }
    }

    if (out_path)
        snprintf(out_path, STR_MAX * 2, "%s", image_path);

    return load_mode;
}

/* ==== Tests: theme_scaleRect ==== */

TEST(scale_rect_identity) {
    g_scale = 1.0;
    TestRect r = {10, 20, 100, 50};
    TestRect s = theme_scaleRect(r);
    ASSERT_EQ(s.x, 10);
    ASSERT_EQ(s.y, 20);
    ASSERT_EQ(s.w, 100);
    ASSERT_EQ(s.h, 50);
}

TEST(scale_rect_double) {
    g_scale = 2.0;
    TestRect r = {10, 20, 100, 50};
    TestRect s = theme_scaleRect(r);
    ASSERT_EQ(s.x, 20);
    ASSERT_EQ(s.y, 40);
    ASSERT_EQ(s.w, 200);
    ASSERT_EQ(s.h, 100);
}

TEST(scale_rect_half) {
    g_scale = 0.5;
    TestRect r = {10, 20, 100, 50};
    TestRect s = theme_scaleRect(r);
    ASSERT_EQ(s.x, 5);
    ASSERT_EQ(s.y, 10);
    ASSERT_EQ(s.w, 50);
    ASSERT_EQ(s.h, 25);
}

TEST(scale_rect_zero_fields) {
    g_scale = 2.0;
    TestRect r = {0, 0, 0, 0};
    TestRect s = theme_scaleRect(r);
    ASSERT_EQ(s.x, 0);
    ASSERT_EQ(s.y, 0);
    ASSERT_EQ(s.w, 0);
    ASSERT_EQ(s.h, 0);
}

TEST(scale_rect_fractional) {
    g_scale = 1.5;
    TestRect r = {10, 10, 100, 100};
    TestRect s = theme_scaleRect(r);
    /* 10 * 1.5 = 15, 100 * 1.5 = 150 */
    ASSERT_EQ(s.x, 15);
    ASSERT_EQ(s.y, 15);
    ASSERT_EQ(s.w, 150);
    ASSERT_EQ(s.h, 150);
}

TEST(scale_rect_negative_coords) {
    g_scale = 2.0;
    TestRect r = {-5, -10, 100, 50};
    TestRect s = theme_scaleRect(r);
    ASSERT_EQ(s.x, -10);
    ASSERT_EQ(s.y, -20);
    ASSERT_EQ(s.w, 200);
    ASSERT_EQ(s.h, 100);
}

/* ==== Tests: theme_getImagePath ==== */

TEST(image_path_override_exists) {
    stub_reset();
    stub_add_path(THEME_OVERRIDES "/skin/bg-title.png");

    char out[STR_MAX * 2];
    int mode = theme_getImagePath("/themes/Dark/", "bg-title", out);

    ASSERT_EQ(mode, 2);
    ASSERT_STREQ(out, THEME_OVERRIDES "/skin/bg-title.png");
}

TEST(image_path_theme_exists) {
    stub_reset();
    stub_add_path("/themes/Dark/skin/bg-title.png");

    char out[STR_MAX * 2];
    int mode = theme_getImagePath("/themes/Dark/", "bg-title", out);

    ASSERT_EQ(mode, 1);
    ASSERT_STREQ(out, "/themes/Dark/skin/bg-title.png");
}

TEST(image_path_fallback) {
    stub_reset();
    /* Neither override nor theme exists */

    char out[STR_MAX * 2];
    int mode = theme_getImagePath("/themes/Dark/", "bg-title", out);

    ASSERT_EQ(mode, 0);
    ASSERT_STREQ(out, FALLBACK_PATH "skin/bg-title.png");
}

TEST(image_path_extra_fallback_to_system_resources) {
    stub_reset();
    /* extra/ prefix uses SYSTEM_RESOURCES fallback */

    char out[STR_MAX * 2];
    int mode = theme_getImagePath("/themes/Dark/", "extra/battery", out);

    ASSERT_EQ(mode, 0);
    ASSERT_STREQ(out, SYSTEM_RESOURCES "battery.png");
}

TEST(image_path_override_takes_priority) {
    stub_reset();
    /* Both override and theme exist - override wins */
    stub_add_path(THEME_OVERRIDES "/skin/icon.png");
    stub_add_path("/themes/Dark/skin/icon.png");

    char out[STR_MAX * 2];
    int mode = theme_getImagePath("/themes/Dark/", "icon", out);

    ASSERT_EQ(mode, 2);
    ASSERT_STREQ(out, THEME_OVERRIDES "/skin/icon.png");
}

TEST(image_path_null_out_path) {
    stub_reset();
    /* NULL out_path should not crash */
    int mode = theme_getImagePath("/themes/Dark/", "bg-title", NULL);
    ASSERT_EQ(mode, 0);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== theme/load.h Scale & Path Unit Tests ===\n\n");

    /* Rect scaling */
    RUN_TEST(scale_rect_identity);
    RUN_TEST(scale_rect_double);
    RUN_TEST(scale_rect_half);
    RUN_TEST(scale_rect_zero_fields);
    RUN_TEST(scale_rect_fractional);
    RUN_TEST(scale_rect_negative_coords);

    /* Image path resolution */
    RUN_TEST(image_path_override_exists);
    RUN_TEST(image_path_theme_exists);
    RUN_TEST(image_path_fallback);
    RUN_TEST(image_path_extra_fallback_to_system_resources);
    RUN_TEST(image_path_override_takes_priority);
    RUN_TEST(image_path_null_out_path);

    TEST_REPORT();
    return test_failures;
}
