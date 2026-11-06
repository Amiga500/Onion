/* SPDX-License-Identifier: GPL-3.0-only */
#include "menus/console_view.h"
#include "cJSON.h"
#include "menus/menu.h"
#include "platform/files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int mode(const cJSON *root, const char *key)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(value)) {
        return MAINUI_CONSOLE_GRID;
    }
    return !strcmp(value->valuestring, "carousel") ? MAINUI_CONSOLE_CAROUSEL
           : !strcmp(value->valuestring, "list")   ? MAINUI_CONSOLE_LIST
                                                   : MAINUI_CONSOLE_GRID;
}

/* "#RRGGBB", opaque, or "#RRGGBBAA", in either case. */
static bool colour(const char *text, SDL_Color *out, Uint8 *alpha)
{
    size_t length = strlen(text);
    if (text[0] != '#' || (length != 7 && length != 9)) {
        return false;
    }
    unsigned value[4] = {0, 0, 0, 255};
    for (size_t i = 1; i < length; i++) {
        char ch = text[i];
        int digit = ch >= '0' && ch <= '9'   ? ch - '0'
                    : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10
                    : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10
                                             : -1;
        if (digit < 0) {
            return false;
        }
        unsigned *part = &value[(i - 1) / 2];
        *part = (i - 1) % 2 ? (*part << 4 | (unsigned)digit) : (unsigned)digit;
    }
    *out = (SDL_Color){(Uint8)value[0], (Uint8)value[1], (Uint8)value[2], 0};
    *alpha = (Uint8)value[3];
    return true;
}

/* A number of object's, times factor and rounded, kept within low..high;
 * otherwise fallback. */
static int number(const cJSON *object, const char *key, double factor, int low, int high,
                  int fallback)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsNumber(value)) {
        return fallback;
    }
    double scaled = value->valuedouble * factor;
    scaled = scaled < low ? low : scaled > high ? high : scaled;
    return (int)(scaled + (scaled < 0 ? -0.5 : 0.5));
}

void mainui_console_view_defaults(MainUIConsoleView *view)
{
    *view = (MainUIConsoleView){.scale_unselected = 70,
                                .opacity_unselected = 50,
                                .gap = 40,
                                .duration_ms = 180,
                                .slide = true,
                                .vsync = true,
                                .selection_padding = 12,
                                .carousel_wrap = true};
}

bool mainui_console_view_parse(MainUIConsoleView *view, const char *text)
{
    mainui_console_view_defaults(view);
    if (!text) {
        return true;
    }
    cJSON *root = mainui_menu_json(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }
    view->games = mode(root, "games");
    view->expert = mode(root, "expert");
    const cJSON *thumbs = cJSON_GetObjectItemCaseSensitive(root, "thumbs");
    if (cJSON_IsString(thumbs) && strlen(thumbs->valuestring) < sizeof view->thumbs) {
        strcpy(view->thumbs, thumbs->valuestring);
    }
    view->hide_count = cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(root, "count"));
    mainui_game_menu_parse_list(&view->menu, MAINUI_GAME_MENU_CONSOLES,
                                cJSON_GetObjectItemCaseSensitive(root, "menu"));
    const cJSON *carousel = cJSON_GetObjectItemCaseSensitive(root, "carousel");
    view->hide_labels = cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(carousel, "labels"));
    const cJSON *selection = cJSON_GetObjectItemCaseSensitive(carousel, "selection");
    if (cJSON_IsFalse(selection) || (cJSON_IsString(selection) && !*selection->valuestring)) {
        view->selection = MAINUI_CONSOLE_SELECTION_NONE;
    }
    else if (cJSON_IsString(selection) &&
             colour(selection->valuestring, &view->selection_colour, &view->selection_alpha)) {
        view->selection = MAINUI_CONSOLE_SELECTION_COLOUR;
    }
    view->scale_unselected = number(carousel, "scale_unselected", 100, 60, 100, 70);
    view->opacity_unselected = number(carousel, "opacity_unselected", 100, 0, 100, 50);
    view->gap = number(carousel, "gap", 1, 0, 100, 40);
    view->duration_ms = number(carousel, "duration_ms", 1, 80, 500, 180);
    view->repeat_ms = number(carousel, "repeat_ms", 1, 0, 400, 0);
    view->selection_padding = number(carousel, "selection_padding", 1, 0, 40, 12);
    view->offset_y = number(carousel, "offset_y", 1, -60, 60, 0);
    const cJSON *animation = cJSON_GetObjectItemCaseSensitive(carousel, "animation");
    /* A slide unless "animation": "none". */
    view->slide = !cJSON_IsString(animation) || strcmp(animation->valuestring, "none");
    const cJSON *easing = cJSON_GetObjectItemCaseSensitive(carousel, "easing");
    view->smooth_easing = cJSON_IsString(easing) && !strcmp(easing->valuestring, "smooth");
    /* On unless "vsync": false; device only, and off by itself if refused. */
    view->vsync = !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(carousel, "vsync"));
    view->carousel_wrap = !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(carousel, "wrap"));
    const cJSON *grid = cJSON_GetObjectItemCaseSensitive(root, "grid");
    const cJSON *navigation = cJSON_GetObjectItemCaseSensitive(grid, "navigation");
    view->grid_stock = cJSON_IsString(navigation) && !strcmp(navigation->valuestring, "stock");
    view->grid_no_wrap = cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(grid, "wrap"));
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "list");
    view->hide_folder_icon = cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(list, "folder_icon"));
    view->hide_list_thumbs = cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(list, "thumbs"));
    cJSON_Delete(root);
    return true;
}

void mainui_console_view_load(MainUIConsoleView *view, const char *directory)
{
    char path[4096];
    int n = directory ? snprintf(path, sizeof path, "%s/console-view.json", directory) : -1;
    char *text = n > 0 && n < (int)sizeof path ? mainui_read_text(path, 64 * 1024) : NULL;
    if (!mainui_console_view_parse(view, text)) {
        fprintf(stderr, "[menu] %s is not a JSON object; consoles show in the grid\n", path);
    }
    free(text);
}

bool mainui_console_thumb(char out[MAINUI_PATH_MAX], const char *thumbs, const MainUIEntry *entry)
{
    if (!thumbs || !*thumbs || !entry || !entry->config) {
        return false;
    }
    /* Emu/SFC/config.json: "sfc". */
    const char *end = strrchr(entry->config, '/');
    if (!end || end == entry->config) {
        return false;
    }
    const char *start = end;
    while (start > entry->config && start[-1] != '/') {
        start--;
    }
    char name[256];
    size_t length = (size_t)(end - start);
    if (!length || length >= sizeof name) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        char ch = start[i];
        name[i] = ch >= 'A' && ch <= 'Z' ? (char)(ch - 'A' + 'a') : ch;
    }
    name[length] = 0;
    int n = snprintf(out, MAINUI_PATH_MAX, "%s/%s.png", thumbs, name);
    return n > 0 && n < MAINUI_PATH_MAX;
}

/* A missing or unreadable specific thumbnail uses _default.png from the
 * same folder. Construct the candidate without touching the SD card: image
 * decoding/fallback is handled by the background worker. */
bool mainui_console_thumb_default(char out[MAINUI_PATH_MAX], const char *primary)
{
    if (!primary || !*primary) {
        return false;
    }
    const char *name = strrchr(primary, '/');
    if (!name) {
        return false;
    }
    size_t prefix = (size_t)(name - primary + 1);
    static const char fallback[] = "_default.png";
    if (prefix + sizeof fallback > MAINUI_PATH_MAX) {
        return false;
    }
    memcpy(out, primary, prefix);
    memcpy(out + prefix, fallback, sizeof fallback);
    return true;
}
