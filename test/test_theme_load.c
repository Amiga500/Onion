/**
 * @file test_theme_load.c
 * @brief Unit tests for src/common/theme/load.h
 *
 * Tests the pure-logic theme loading functions: theme_scaleRect
 * (scaling math) and theme_getImagePath (path resolution with
 * override/theme/fallback priority).
 *
 * Includes the production header (SDL_Rect from test/stubs/SDL).
 *
 * Build and run: make -f Makefile.unit test_theme_load
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Production code: theme_scaleRect() and theme_getImagePath() from
 * theme/image_path.h (included by theme/load.h), with SDL_Rect from the
 * test SDL stub and the three lookup folders redirected to /tmp. */
#define FALLBACK_PATH "/tmp/test_theme_load/fallback/"
#define SYSTEM_RESOURCES "/tmp/test_theme_load/res/"
#define THEME_OVERRIDES "/tmp/test_theme_load/overrides"

#include "theme/image_path.h"

/* ---- Helpers ---- */

static void mkdir_p(const char *path)
{
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static void touch_file(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (fp)
        fclose(fp);
}

/* ==== theme_scaleRect tests ==== */

TEST(scaleRect_no_scale) {
    g_scale = 1.0;
    SDL_Rect r = {10, 20, 100, 200};
    SDL_Rect out = theme_scaleRect(r);
    ASSERT_EQ(out.x, 10);
    ASSERT_EQ(out.y, 20);
    ASSERT_EQ(out.w, 100);
    ASSERT_EQ(out.h, 200);
}

TEST(scaleRect_double) {
    g_scale = 2.0;
    SDL_Rect r = {10, 20, 100, 200};
    SDL_Rect out = theme_scaleRect(r);
    ASSERT_EQ(out.x, 20);
    ASSERT_EQ(out.y, 40);
    ASSERT_EQ(out.w, 200);
    ASSERT_EQ(out.h, 400);
}

TEST(scaleRect_half) {
    g_scale = 0.5;
    SDL_Rect r = {10, 20, 100, 200};
    SDL_Rect out = theme_scaleRect(r);
    ASSERT_EQ(out.x, 5);
    ASSERT_EQ(out.y, 10);
    ASSERT_EQ(out.w, 50);
    ASSERT_EQ(out.h, 100);
}

TEST(scaleRect_zero_rect) {
    g_scale = 2.0;
    SDL_Rect r = {0, 0, 0, 0};
    SDL_Rect out = theme_scaleRect(r);
    ASSERT_EQ(out.x, 0);
    ASSERT_EQ(out.y, 0);
    ASSERT_EQ(out.w, 0);
    ASSERT_EQ(out.h, 0);
}

TEST(scaleRect_fractional_scale) {
    g_scale = 1.5;
    SDL_Rect r = {10, 20, 100, 200};
    SDL_Rect out = theme_scaleRect(r);
    ASSERT_EQ(out.x, 15);
    ASSERT_EQ(out.y, 30);
    ASSERT_EQ(out.w, 150);
    ASSERT_EQ(out.h, 300);
}

TEST(scaleRect_identity_when_exactly_1) {
    g_scale = 1.0;
    SDL_Rect r = {33, 44, 55, 66};
    SDL_Rect out = theme_scaleRect(r);
    /* When scale is exactly 1.0, the function returns early without modifying */
    ASSERT_EQ(out.x, 33);
    ASSERT_EQ(out.y, 44);
    ASSERT_EQ(out.w, 55);
    ASSERT_EQ(out.h, 66);
}

TEST(scaleRect_negative_coords) {
    g_scale = 2.0;
    SDL_Rect r = {-5, -10, 100, 50};
    SDL_Rect out = theme_scaleRect(r);
    ASSERT_EQ(out.x, -10);
    ASSERT_EQ(out.y, -20);
    ASSERT_EQ(out.w, 200);
    ASSERT_EQ(out.h, 100);
    g_scale = 1.0;
}

/* ==== theme_getImagePath tests ==== */

/* Setup test directories */
static void setup_test_dirs(void)
{
    /* Clean and recreate */
    system("rm -rf /tmp/test_theme_load");
    mkdir_p("/tmp/test_theme_load/overrides/skin");
    mkdir_p("/tmp/test_theme_load/mytheme/skin");
    mkdir_p("/tmp/test_theme_load/fallback/skin");
    mkdir_p("/tmp/test_theme_load/res");
}

TEST(getImagePath_fallback_when_no_files) {
    setup_test_dirs();
    char out[STR_MAX * 2] = {0};
    int mode = theme_getImagePath("/tmp/test_theme_load/mytheme/", "bg-title", out);
    ASSERT_EQ(mode, 0);
    /* Should fall back to FALLBACK_PATH */
    ASSERT_NOT_NULL(strstr(out, FALLBACK_PATH));
    ASSERT_NOT_NULL(strstr(out, "skin/bg-title.png"));
}

TEST(getImagePath_theme_file_found) {
    setup_test_dirs();
    touch_file("/tmp/test_theme_load/mytheme/skin/bg-title.png");
    char out[STR_MAX * 2] = {0};
    int mode = theme_getImagePath("/tmp/test_theme_load/mytheme/", "bg-title", out);
    ASSERT_EQ(mode, 1);
    ASSERT_NOT_NULL(strstr(out, "/tmp/test_theme_load/mytheme/skin/bg-title.png"));
}

TEST(getImagePath_override_takes_priority) {
    setup_test_dirs();
    touch_file("/tmp/test_theme_load/mytheme/skin/bg-title.png");
    touch_file("/tmp/test_theme_load/overrides/skin/bg-title.png");
    char out[STR_MAX * 2] = {0};
    int mode = theme_getImagePath("/tmp/test_theme_load/mytheme/", "bg-title", out);
    ASSERT_EQ(mode, 2);
    ASSERT_NOT_NULL(strstr(out, THEME_OVERRIDES));
}

TEST(getImagePath_extra_prefix_uses_system_resources) {
    setup_test_dirs();
    char out[STR_MAX * 2] = {0};
    int mode = theme_getImagePath("/tmp/test_theme_load/mytheme/", "extra/myicon", out);
    ASSERT_EQ(mode, 0);
    /* extra/ prefix strips "extra/" and uses SYSTEM_RESOURCES */
    ASSERT_NOT_NULL(strstr(out, SYSTEM_RESOURCES));
    ASSERT_NOT_NULL(strstr(out, "myicon.png"));
}

TEST(getImagePath_null_outpath) {
    setup_test_dirs();
    /* Passing NULL for out_path should not crash */
    int mode = theme_getImagePath("/tmp/test_theme_load/mytheme/", "bg-title", NULL);
    ASSERT_EQ(mode, 0);
}

TEST(getImagePath_override_only) {
    setup_test_dirs();
    /* Only override exists, not theme or fallback */
    touch_file("/tmp/test_theme_load/overrides/skin/icon-sel.png");
    char out[STR_MAX * 2] = {0};
    int mode = theme_getImagePath("/tmp/test_theme_load/mytheme/", "icon-sel", out);
    ASSERT_EQ(mode, 2);
    ASSERT_NOT_NULL(strstr(out, "overrides/skin/icon-sel.png"));
}

TEST(getImagePath_theme_priority_over_fallback) {
    setup_test_dirs();
    /* Theme exists, fallback also exists, but theme should win */
    touch_file("/tmp/test_theme_load/mytheme/skin/btn-ok.png");
    touch_file("/tmp/test_theme_load/fallback/skin/btn-ok.png");
    char out[STR_MAX * 2] = {0};
    int mode = theme_getImagePath("/tmp/test_theme_load/mytheme/", "btn-ok", out);
    ASSERT_EQ(mode, 1);
    ASSERT_NOT_NULL(strstr(out, "/tmp/test_theme_load/mytheme/skin/btn-ok.png"));
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== theme/load.h Unit Tests ===\n\n");

    /* theme_scaleRect */
    RUN_TEST(scaleRect_no_scale);
    RUN_TEST(scaleRect_double);
    RUN_TEST(scaleRect_half);
    RUN_TEST(scaleRect_zero_rect);
    RUN_TEST(scaleRect_fractional_scale);
    RUN_TEST(scaleRect_identity_when_exactly_1);
    RUN_TEST(scaleRect_negative_coords);

    /* theme_getImagePath */
    RUN_TEST(getImagePath_fallback_when_no_files);
    RUN_TEST(getImagePath_theme_file_found);
    RUN_TEST(getImagePath_override_takes_priority);
    RUN_TEST(getImagePath_extra_prefix_uses_system_resources);
    RUN_TEST(getImagePath_null_outpath);
    RUN_TEST(getImagePath_override_only);
    RUN_TEST(getImagePath_theme_priority_over_fallback);

    /* Cleanup test directories */
    system("rm -rf /tmp/test_theme_load");

    TEST_REPORT();
    return test_failures;
}
