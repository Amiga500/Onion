#ifndef THEME_IMAGE_PATH_H__
#define THEME_IMAGE_PATH_H__

// Theme image lookup and UI scaling, without SDL_image/SDL_ttf, so host
// tests can include it (theme/load.h includes it; behavior unchanged).

#include <SDL/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "utils/file.h"
#include "utils/str.h"

// Host tests point these at temporary folders.
#ifndef FALLBACK_PATH
#define FALLBACK_PATH "/mnt/SDCARD/miyoo/app/"
#endif
#ifndef SYSTEM_RESOURCES
#define SYSTEM_RESOURCES "/mnt/SDCARD/.tmp_update/res/"
#endif
#ifndef THEME_OVERRIDES
#define THEME_OVERRIDES "/mnt/SDCARD/Saves/CurrentProfile/theme"
#endif

static double g_scale = 1.0;

SDL_Rect theme_scaleRect(SDL_Rect rect)
{
    if (g_scale == 1.0)
        return rect;
    rect.x = (double)rect.x * g_scale;
    rect.y = (double)rect.y * g_scale;
    rect.w = (double)rect.w * g_scale;
    rect.h = (double)rect.h * g_scale;
    return rect;
}

int theme_getImagePath(const char *theme_path, const char *name, char *out_path)
{
    int load_mode = 2;
    char rel_path[STR_MAX], image_path[STR_MAX * 2];
    sprintf(rel_path, "skin/%s.png", name);

    sprintf(image_path, THEME_OVERRIDES "/%s", rel_path);
    bool override_exists = exists(image_path);

    if (!override_exists) {
        load_mode = 1;
        sprintf(image_path, "%s%s", theme_path, rel_path);
        bool theme_exists = exists(image_path);

        if (!theme_exists) {
            load_mode = 0;
            if (strncmp(name, "extra/", 6) == 0) {
                sprintf(rel_path, "%s.png", name + 6);
                sprintf(image_path, "%s%s", SYSTEM_RESOURCES, rel_path);
            }
            else {
                sprintf(image_path, "%s%s", FALLBACK_PATH, rel_path);
            }
        }
    }

    if (out_path)
        sprintf(out_path, "%s", image_path);

    return load_mode;
}

// Name (for theme_loadImage) of the X button RedOnion ships for a theme
// without one: "extra/theme-x/<theme folder>", which is found as
// SYSTEM_RESOURCES "theme-x/<theme folder>.png" (.tmp_update/res/theme-x).
// False when the theme path has no folder name.
bool theme_bundledButtonXName(const char *theme_path, char *out, size_t out_size)
{
    if (theme_path == NULL || out == NULL || out_size == 0)
        return false;
    size_t len = strlen(theme_path);
    while (len > 0 && theme_path[len - 1] == '/')
        len--;
    size_t start = len;
    while (start > 0 && theme_path[start - 1] != '/')
        start--;
    if (start == len)
        return false;
    int n = snprintf(out, out_size, "extra/theme-x/%.*s", (int)(len - start), theme_path + start);
    return n > 0 && (size_t)n < out_size;
}

#endif // THEME_IMAGE_PATH_H__
