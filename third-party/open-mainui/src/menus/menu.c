/* SPDX-License-Identifier: GPL-3.0-only */
#include "menus/menu.h"
#include "cJSON.h"
#include "localization/language.h"
#include "platform/files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int section_id(const char *key)
{
    const char *keys[] = {"recents", "favorites", "games", "expert", "apps", "settings"};
    for (int i = 0; i < MAINUI_MENU_SECTIONS; i++) {
        if (!strcmp(key, keys[i])) {
            return i;
        }
    }
    if (!strcmp(key, "recent")) {
        return MAINUI_MENU_RECENTS;
    }
    if (!strcmp(key, "favourites") || !strcmp(key, "favorite") || !strcmp(key, "favourite") ||
        !strcmp(key, "favs")) {
        return MAINUI_MENU_FAVORITES;
    }
    return -1;
}

const char *mainui_menu_label(MainUIMenuSection section)
{
    static const char *labels[] = {"Recents", "Favorites", "Games", "Expert", "Apps", "Settings"};
    static const int ids[] = {18, 1, 2, 0, 107, 15};
    return section >= 0 && section < MAINUI_MENU_SECTIONS
               ? mainui_translate(ids[section], labels[section])
               : "";
}

const char *mainui_menu_icon(MainUIMenuSection section)
{
    static const char *icons[] = {"recent", "favorite", "game", "retroarch", "app", "setting"};
    return section >= 0 && section < MAINUI_MENU_SECTIONS ? icons[section] : "";
}

/* Replace comments/trailing commas with spaces, preserving strings and source
 * length. The patch accepts these extensions but cJSON expects ordinary JSON.
 * Count nesting outside strings before calling cJSON to retain the 16-level gate.
 */
static bool clean_json(char *text)
{
    bool quoted = false, escape = false;
    int depth = 0;
    for (size_t i = 0; text[i]; i++) {
        char ch = text[i];
        if (quoted) {
            if (escape) {
                escape = false;
            }
            else if (ch == '\\') {
                escape = true;
            }
            else if (ch == '"') {
                quoted = false;
            }
            continue;
        }
        if (ch == '"') {
            quoted = true;
            continue;
        }
        if (ch == '#' || (ch == '/' && text[i + 1] == '/')) {
            while (text[i] && text[i] != '\n') {
                text[i++] = ' ';
            }
            if (!text[i]) {
                break;
            }
        }
        else if (ch == '/' && text[i + 1] == '*') {
            text[i++] = ' ';
            text[i++] = ' ';
            while (text[i] && !(text[i] == '*' && text[i + 1] == '/')) {
                text[i++] = ' ';
            }
            if (!text[i]) {
                return false;
            }
            text[i++] = ' ';
            text[i] = ' ';
        }
        else if (ch == '{' || ch == '[') {
            if (++depth > 16) {
                return false;
            }
        }
        else if (ch == '}' || ch == ']') {
            if (--depth < 0) {
                return false;
            }
        }
    }
    if (quoted || depth) {
        return false;
    }
    quoted = escape = false;
    for (size_t i = 0; text[i]; i++) {
        if (quoted) {
            if (escape) {
                escape = false;
            }
            else if (text[i] == '\\') {
                escape = true;
            }
            else if (text[i] == '"') {
                quoted = false;
            }
        }
        else if (text[i] == '"') {
            quoted = true;
        }
        else if (text[i] == ',') {
            size_t next = i + 1;
            while (text[next] == ' ' || text[next] == '\t' || text[next] == '\r' ||
                   text[next] == '\n') {
                next++;
            }
            if (text[next] == '}' || text[next] == ']') {
                text[i] = ' ';
            }
        }
    }
    return true;
}

cJSON *mainui_menu_json(const char *text)
{
    cJSON *root = NULL;
    if (text && strlen(text) <= 128 * 1024) {
        char *copy = malloc(strlen(text) + 1);
        if (copy) {
            strcpy(copy, text);
            if (clean_json(copy)) {
                root = cJSON_ParseWithOpts(copy, NULL, true);
            }
            free(copy);
        }
    }
    return root;
}

void mainui_menu_parse(MainUIMenu *menu, const char *text, bool recents, bool expert)
{
    bool seen[MAINUI_MENU_SECTIONS] = {0}, enabled[MAINUI_MENU_SECTIONS] = {0};
    int order[MAINUI_MENU_SECTIONS], count = 0;
    cJSON *root = mainui_menu_json(text);
    const cJSON *object = cJSON_GetObjectItemCaseSensitive(root, "menu");
    if (cJSON_IsObject(object)) {
        const cJSON *item;
        cJSON_ArrayForEach(item, object)
        {
            int section = section_id(item->string);
            if (section < 0) {
                continue;
            }
            if (!seen[section]) {
                order[count++] = section;
            }
            seen[section] = true;
            enabled[section] = cJSON_IsTrue(item);
        }
    }
    bool any = false;
    for (int i = 0; i < count; i++) {
        any |= enabled[order[i]];
    }
    if (!any) {
        /* Default order follows MainUIMenuSection: Recents, Favorites, Games,
         * Expert, Apps, Settings. Keep the legacy visibility rules below;
         * a usable explicit menu retains its configured order. */
        count = MAINUI_MENU_SECTIONS;
        for (int i = 0; i < count; i++) {
            order[i] = i;
            enabled[i] = i != MAINUI_MENU_RECENTS && i != MAINUI_MENU_EXPERT;
        }
    }
    /* Explicit false wins over readable legacy markers, even after fallback. */
    int legacy[] = {MAINUI_MENU_RECENTS, MAINUI_MENU_EXPERT};
    bool markers[] = {recents, expert};
    for (int i = 0; i < 2; i++) {
        int section = legacy[i];
        if (!seen[section] && markers[i]) {
            enabled[section] = true;
            bool present = false;
            for (int j = 0; j < count; j++) {
                present |= order[j] == section;
            }
            if (!present) {
                order[count++] = section;
            }
        }
    }
    menu->count = 0;
    for (int i = 0; i < count; i++) {
        if (enabled[order[i]]) {
            menu->sections[menu->count++] = (MainUIMenuSection)order[i];
        }
    }
    cJSON_Delete(root);
}

void mainui_menu_load(MainUIMenu *menu, const char *directory, const MainUIConfig *config)
{
    char path[4096];
    char *text = NULL;
    if (directory) {
        int size = snprintf(path, sizeof path, "%s/main-menu.json", directory);
        if (size > 0 && size < (int)sizeof path) {
            text = mainui_read_text(path, 128 * 1024);
        }
    }
    mainui_menu_parse(menu, text, config->show_recents, config->show_expert);
    free(text);
}

void mainui_grid_restore(MainUIViewport *view, int total, int selected, int columns, int rows)
{
    if (columns < 1 || rows < 1 || total <= 0) {
        *view = (MainUIViewport){0, -1, 0, -1};
        return;
    }
    if (selected < 0) {
        selected = 0;
    }
    if (selected >= total) {
        selected = total - 1;
    }
    int capacity = columns * rows;
    int start = (selected / capacity) * capacity;
    int end = total - start < capacity ? total - 1 : start + capacity - 1;
    *view = (MainUIViewport){total, selected, start, end};
}

void mainui_grid_move(MainUIViewport *view, int horizontal, int vertical, int pages, int columns,
                      int rows)
{
    if (view->total <= 0) {
        return;
    }
    int64_t selected = view->selected + (int64_t)horizontal + (int64_t)vertical * columns +
                       (int64_t)pages * columns * rows;
    if (horizontal) {
        if (selected < 0) {
            selected = view->total - 1;
        }
        if (selected >= view->total) {
            selected = 0;
        }
    }
    else {
        if (selected < 0) {
            selected = 0;
        }
        if (selected >= view->total) {
            selected = view->total - 1;
        }
    }
    mainui_grid_restore(view, view->total, (int)selected, columns, rows);
}
