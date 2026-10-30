#ifndef TWEAKS_MAIN_MENU_H__
#define TWEAKS_MAIN_MENU_H__

// SPDX-License-Identifier: GPL-3.0-only
//
// Tweaks controls for the menu and SELECT-menu visibility handled by the
// optional patched MainUI main-menu.json reader. Existing custom actions,
// unknown/unsupported root fields and recognized item order are kept. The
// root hotkey setting is editable separately from the context-menu entries.
//
// From robcodedev/onionos-mainui-patcher (tools/src/tweaks/main_menu.h), for
// Open MainUI. OnionPlus changes: the default main menu shows Apps (the
// patcher's default turned on Expert in its place, so the first change
// written to main-menu.json hid Apps), and Recents and Expert default to
// Onion's .showRecents and .showExpert, as Open MainUI reads them when
// main-menu.json doesn't name them.

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "components/list.h"
#include "utils/json.h"

#ifndef MAINMENU_CONFIG_DIR
#define MAINMENU_CONFIG_DIR "/mnt/SDCARD/.tmp_update/config"
#endif
#define MAINMENU_CONFIG_PATH MAINMENU_CONFIG_DIR "/main-menu.json"
#ifndef MAINMENU_SD_DIR
#define MAINMENU_SD_DIR "/mnt/SDCARD"
#endif
#define MAINMENU_CONFIG_MAX_SIZE (128 * 1024)

#define MAINMENU_MENU_COUNT 6
#define MAINMENU_CONTEXT_COUNT 14
#define MAINMENU_CONTEXT_LIST_COUNT (MAINMENU_CONTEXT_COUNT + 1)

static const char *mainmenu_menu_keys[MAINMENU_MENU_COUNT] = {
    "recents", "favorites", "games", "expert", "apps", "settings"};

static const char *mainmenu_menu_labels[MAINMENU_MENU_COUNT] = {
    "Recents", "Favorites", "Games", "Expert", "Apps", "Settings"};

static const char *mainmenu_context_keys[MAINMENU_CONTEXT_COUNT] = {
    "shutdown", "refresh", "search", "recents", "favorites", "games",
    "apps", "settings", "expert", "themes", "tweaks", "custom1",
    "custom2", "custom3"};

static const char *mainmenu_context_labels[MAINMENU_CONTEXT_COUNT] = {
    "Shutdown", "Refresh all roms", "Search", "Recents", "Favorites", "Games",
    "Apps", "Settings", "Expert", "Themes", "Tweaks", "Custom 1",
    "Custom 2", "Custom 3"};

// Onion's legacy visibility markers for Recents and Expert.
static bool mainmenu_legacy_marker(int index)
{
    if (index == 0)
        return access(MAINMENU_CONFIG_DIR "/.showRecents", F_OK) == 0;
    if (index == 3)
        return access(MAINMENU_CONFIG_DIR "/.showExpert", F_OK) == 0;
    return false;
}

static cJSON *mainmenu_load_root(bool *exists_out, bool *valid_out);

// A custom action Open MainUI can show: main-menu.json "custom" gives it a
// label (under 128 bytes) and a launch command (under 512), as Open MainUI
// requires (src/menus/context.c); without them it drops the entry.
static bool mainmenu_custom_defined(int index)
{
    bool exists = false, valid = false;
    cJSON *root = mainmenu_load_root(&exists, &valid);
    cJSON *custom = valid ? cJSON_GetObjectItemCaseSensitive(root, "custom") : NULL;
    cJSON *spec = cJSON_GetObjectItemCaseSensitive(custom, mainmenu_context_keys[index]);
    cJSON *label = cJSON_GetObjectItemCaseSensitive(spec, "label");
    cJSON *launch = cJSON_GetObjectItemCaseSensitive(spec, "launch");
    bool defined = cJSON_IsString(label) && label->valuestring[0] &&
                   strlen(label->valuestring) < 128 && cJSON_IsString(launch) &&
                   launch->valuestring[0] && strlen(launch->valuestring) < 512;
    cJSON_Delete(root);
    return defined;
}

// Open MainUI leaves Search and Tweaks out of the context menu when their
// app is missing (App/<name>/launch.sh), and a custom action without its
// definition, whatever main-menu.json enables, so Tweaks shows them off
// then. main-menu.json keeps what it says: once the app is installed or the
// action defined, the entry shows as the file has it.
static bool mainmenu_context_available(int index)
{
    if (index == 2)
        return access(MAINMENU_SD_DIR "/App/Search/launch.sh", F_OK) == 0;
    if (index == 10)
        return access(MAINMENU_SD_DIR "/App/Tweaks/launch.sh", F_OK) == 0;
    if (index >= 11 && index <= 13)
        return mainmenu_custom_defined(index);
    return true;
}

static void mainmenu_set_default_menu(bool states[MAINMENU_MENU_COUNT])
{
    memset(states, 0, sizeof(bool) * MAINMENU_MENU_COUNT);
    states[0] = mainmenu_legacy_marker(0); // Recents
    states[1] = true;                      // Favorites
    states[2] = true;                      // Games
    states[3] = mainmenu_legacy_marker(3); // Expert
    states[4] = true;                      // Apps
    states[5] = true;                      // Settings
}

static void mainmenu_set_default_context(bool states[MAINMENU_CONTEXT_COUNT])
{
    memset(states, 0, sizeof(bool) * MAINMENU_CONTEXT_COUNT);
    states[1] = true;  // Refresh all roms
    states[2] = true;  // Search
    states[10] = true; // Tweaks
}

static int mainmenu_menu_key_index(const char *key)
{
    if (key == NULL)
        return -1;
    if (strcmp(key, "recents") == 0 || strcmp(key, "recent") == 0)
        return 0;
    if (strcmp(key, "favorites") == 0 || strcmp(key, "favorite") == 0 ||
        strcmp(key, "favourite") == 0 || strcmp(key, "favourites") == 0 ||
        strcmp(key, "favs") == 0)
        return 1;
    for (int i = 2; i < MAINMENU_MENU_COUNT; i++) {
        if (strcmp(key, mainmenu_menu_keys[i]) == 0)
            return i;
    }
    return -1;
}

static int mainmenu_context_key_index(const char *key)
{
    if (key == NULL)
        return -1;
    if (strcmp(key, "recents") == 0 || strcmp(key, "recent") == 0)
        return 3;
    if (strcmp(key, "favorites") == 0 || strcmp(key, "favorite") == 0 ||
        strcmp(key, "favourite") == 0 || strcmp(key, "favourites") == 0 ||
        strcmp(key, "favs") == 0)
        return 4;
    for (int i = 0; i < MAINMENU_CONTEXT_COUNT; i++) {
        if (i == 3 || i == 4)
            continue;
        if (strcmp(key, mainmenu_context_keys[i]) == 0)
            return i;
    }
    return -1;
}

static bool mainmenu_ensure_config_dir(void)
{
    if (mkdir(MAINMENU_CONFIG_DIR, 0755) == 0)
        return true;
    if (errno != EEXIST)
        return false;

    struct stat st;
    return stat(MAINMENU_CONFIG_DIR, &st) == 0 && S_ISDIR(st.st_mode);
}

static char *mainmenu_read_config_file(const char *path, bool *exists_out)
{
    *exists_out = false;

    struct stat st;
    if (stat(path, &st) != 0)
        return NULL;
    *exists_out = true;
    if (!S_ISREG(st.st_mode) || st.st_size < 0 ||
        st.st_size > MAINMENU_CONFIG_MAX_SIZE)
        return NULL;

    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return NULL;

    size_t size = (size_t)st.st_size;
    char *text = malloc(size + 1);
    if (text == NULL) {
        fclose(fp);
        return NULL;
    }

    bool ok = fread(text, 1, size, fp) == size;
    if (fclose(fp) != 0)
        ok = false;
    if (!ok) {
        free(text);
        return NULL;
    }
    text[size] = '\0';
    return text;
}

static void mainmenu_relax_json(char *text)
{
    bool in_string = false;
    bool escaped = false;
    bool line_comment = false;
    bool block_comment = false;

    for (size_t i = 0; text[i] != '\0'; i++) {
        char ch = text[i];
        char next = text[i + 1];

        if (line_comment) {
            if (ch == '\n' || ch == '\r')
                line_comment = false;
            else
                text[i] = ' ';
            continue;
        }
        if (block_comment) {
            if (ch == '*' && next == '/') {
                text[i] = ' ';
                text[i + 1] = ' ';
                i++;
                block_comment = false;
            }
            else if (ch != '\n' && ch != '\r') {
                text[i] = ' ';
            }
            continue;
        }
        if (in_string) {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                in_string = false;
            continue;
        }
        if (ch == '"') {
            in_string = true;
            continue;
        }
        if (ch == '#') {
            text[i] = ' ';
            line_comment = true;
            continue;
        }
        if (ch == '/' && next == '/') {
            text[i] = ' ';
            text[i + 1] = ' ';
            i++;
            line_comment = true;
            continue;
        }
        if (ch == '/' && next == '*') {
            text[i] = ' ';
            text[i + 1] = ' ';
            i++;
            block_comment = true;
        }
    }

    in_string = false;
    escaped = false;
    for (size_t i = 0; text[i] != '\0'; i++) {
        char ch = text[i];
        if (in_string) {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                in_string = false;
            continue;
        }
        if (ch == '"') {
            in_string = true;
            continue;
        }
        if (ch != ',')
            continue;

        size_t next = i + 1;
        while (text[next] != '\0' && isspace((unsigned char)text[next]))
            next++;
        if (text[next] == '}' || text[next] == ']')
            text[i] = ' ';
    }
}

static cJSON *mainmenu_load_root_file(const char *path, bool *exists_out, bool *valid_out)
{
    char *text = mainmenu_read_config_file(path, exists_out);
    if (!*exists_out) {
        *valid_out = true;
        return cJSON_CreateObject();
    }
    if (text == NULL) {
        *valid_out = false;
        return NULL;
    }

    mainmenu_relax_json(text);
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        *valid_out = false;
        return NULL;
    }

    *valid_out = true;
    return root;
}

static cJSON *mainmenu_load_root(bool *exists_out, bool *valid_out)
{
    return mainmenu_load_root_file(MAINMENU_CONFIG_PATH, exists_out, valid_out);
}

static void mainmenu_read_menu_states(cJSON *root,
                                      bool states[MAINMENU_MENU_COUNT])
{
    bool seen[MAINMENU_MENU_COUNT] = {false};
    memset(states, 0, sizeof(bool) * MAINMENU_MENU_COUNT);
    cJSON *menu = cJSON_GetObjectItemCaseSensitive(root, "menu");
    if (cJSON_IsObject(menu)) {
        for (cJSON *item = menu->child; item != NULL; item = item->next) {
            int index = mainmenu_menu_key_index(item->string);
            if (index >= 0) {
                states[index] = cJSON_IsTrue(item);
                seen[index] = true;
            }
        }
    }

    bool any = false;
    for (int i = 0; i < MAINMENU_MENU_COUNT; i++)
        any = any || states[i];
    if (!any) {
        mainmenu_set_default_menu(states);
        // Open MainUI: an explicit false still wins after the fallback.
        for (int i = 0; i < MAINMENU_MENU_COUNT; i++)
            if (seen[i] && (i == 0 || i == 3))
                states[i] = false;
        return;
    }
    // As Open MainUI: a section the file doesn't name follows its marker.
    for (int i = 0; i < MAINMENU_MENU_COUNT; i++)
        if (!seen[i] && mainmenu_legacy_marker(i))
            states[i] = true;
}

static bool mainmenu_menu_object_has_enabled(cJSON *menu)
{
    if (!cJSON_IsObject(menu))
        return false;
    for (cJSON *item = menu->child; item != NULL; item = item->next) {
        if (mainmenu_menu_key_index(item->string) >= 0 && cJSON_IsTrue(item))
            return true;
    }
    return false;
}

static void mainmenu_read_context_states(cJSON *root,
                                         bool states[MAINMENU_CONTEXT_COUNT])
{
    memset(states, 0, sizeof(bool) * MAINMENU_CONTEXT_COUNT);
    cJSON *context = cJSON_GetObjectItemCaseSensitive(root, "context");
    if (context == NULL) {
        mainmenu_set_default_context(states);
        return;
    }

    if (cJSON_IsObject(context)) {
        for (cJSON *item = context->child; item != NULL; item = item->next) {
            int index = mainmenu_context_key_index(item->string);
            if (index >= 0)
                states[index] = cJSON_IsTrue(item);
        }
    }
    else if (cJSON_IsArray(context)) {
        int count = cJSON_GetArraySize(context);
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_GetArrayItem(context, i);
            if (!cJSON_IsString(item))
                continue;
            int index = mainmenu_context_key_index(item->valuestring);
            if (index >= 0)
                states[index] = true;
        }
    }
}

static bool mainmenu_read_hotkey_state(cJSON *root)
{
    cJSON *hotkey = cJSON_GetObjectItemCaseSensitive(root, "hotkey");
    return hotkey == NULL || cJSON_IsTrue(hotkey);
}

static bool mainmenu_load_hotkey_state(void)
{
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root(&exists, &valid);
    if (!valid || root == NULL) {
        cJSON_Delete(root);
        return true;
    }

    bool enabled = mainmenu_read_hotkey_state(root);
    cJSON_Delete(root);
    return enabled;
}

static void mainmenu_load_states(bool menu_states[MAINMENU_MENU_COUNT],
                                 bool context_states[MAINMENU_CONTEXT_COUNT])
{
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root(&exists, &valid);
    if (!valid || root == NULL) {
        mainmenu_set_default_menu(menu_states);
        mainmenu_set_default_context(context_states);
        cJSON_Delete(root);
        return;
    }

    mainmenu_read_menu_states(root, menu_states);
    mainmenu_read_context_states(root, context_states);
    cJSON_Delete(root);
}

// What Open MainUI shows: the stored states, without an app that is missing.
static void mainmenu_load_shown_states(bool menu_states[MAINMENU_MENU_COUNT],
                                       bool context_states[MAINMENU_CONTEXT_COUNT])
{
    mainmenu_load_states(menu_states, context_states);
    for (int i = 0; i < MAINMENU_CONTEXT_COUNT; i++)
        context_states[i] = context_states[i] && mainmenu_context_available(i);
}

static bool mainmenu_write_root_file(const char *path, cJSON *root)
{
    if (root == NULL || !mainmenu_ensure_config_dir())
        return false;

    char *output = cJSON_Print(root);
    if (output == NULL)
        return false;

    size_t length = strlen(output);
    if (length > MAINMENU_CONFIG_MAX_SIZE) {
        cJSON_free(output);
        return false;
    }

    char temporary[256];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) >=
        (int)sizeof(temporary)) {
        cJSON_free(output);
        return false;
    }

    FILE *fp = fopen(temporary, "wb");
    if (fp == NULL) {
        cJSON_free(output);
        return false;
    }

    bool ok = fwrite(output, 1, length, fp) == length;
    if (ok)
        ok = fwrite("\n", 1, 1, fp) == 1;
    if (ok)
        ok = fflush(fp) == 0;
    if (ok)
        ok = fsync(fileno(fp)) == 0;
    if (fclose(fp) != 0)
        ok = false;
    cJSON_free(output);

    if (!ok) {
        remove(temporary);
        return false;
    }
    if (rename(temporary, path) != 0) {
        remove(temporary);
        return false;
    }
    return true;
}

static bool mainmenu_write_root(cJSON *root)
{
    return mainmenu_write_root_file(MAINMENU_CONFIG_PATH, root);
}

static bool mainmenu_replace_root_section(cJSON *root, const char *name,
                                          cJSON *replacement)
{
    cJSON *old = cJSON_GetObjectItemCaseSensitive(root, name);
    if (old != NULL)
        return cJSON_ReplaceItemInObjectCaseSensitive(root, name, replacement);
    return cJSON_AddItemToObject(root, name, replacement);
}

static bool mainmenu_set_object_alias(cJSON *object, int target_index,
                                      bool enabled, bool context)
{
    bool matched = false;
    for (cJSON *item = object->child; item != NULL;) {
        cJSON *next = item->next;
        int index = context ? mainmenu_context_key_index(item->string)
                            : mainmenu_menu_key_index(item->string);
        if (index == target_index) {
            cJSON *value = cJSON_CreateBool(enabled);
            if (value == NULL)
                return false;

            char *key = item->string;
            item->string = NULL;
            value->string = key;
            if (!cJSON_ReplaceItemViaPointer(object, item, value)) {
                value->string = NULL;
                item->string = key;
                cJSON_Delete(value);
                return false;
            }
            matched = true;
        }
        item = next;
    }

    if (matched)
        return true;
    const char *key = context ? mainmenu_context_keys[target_index]
                              : mainmenu_menu_keys[target_index];
    return cJSON_AddBoolToObject(object, key, enabled) != NULL;
}

static bool mainmenu_set_array_context(cJSON *array, int target_index,
                                       bool enabled)
{
    bool found = false;
    for (int i = cJSON_GetArraySize(array) - 1; i >= 0; i--) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        if (!cJSON_IsString(item) ||
            mainmenu_context_key_index(item->valuestring) != target_index)
            continue;
        found = true;
        if (!enabled)
            cJSON_DeleteItemFromArray(array, i);
    }
    if (enabled && !found) {
        cJSON *value = cJSON_CreateString(mainmenu_context_keys[target_index]);
        if (value == NULL)
            return false;
        if (!cJSON_AddItemToArray(array, value)) {
            cJSON_Delete(value);
            return false;
        }
    }
    return true;
}

static bool mainmenu_add_default_sections(cJSON *root)
{
    bool menu_states[MAINMENU_MENU_COUNT];
    bool context_states[MAINMENU_CONTEXT_COUNT];
    mainmenu_set_default_menu(menu_states);
    mainmenu_set_default_context(context_states);

    cJSON *menu = cJSON_CreateObject();
    cJSON *context = cJSON_CreateObject();
    if (menu == NULL || context == NULL) {
        cJSON_Delete(menu);
        cJSON_Delete(context);
        return false;
    }

    for (int i = 0; i < MAINMENU_MENU_COUNT; i++) {
        if (cJSON_AddBoolToObject(menu, mainmenu_menu_keys[i], menu_states[i]) == NULL) {
            cJSON_Delete(menu);
            cJSON_Delete(context);
            return false;
        }
    }
    for (int i = 0; i < MAINMENU_CONTEXT_COUNT; i++) {
        if (!context_states[i])
            continue;
        if (cJSON_AddBoolToObject(context, mainmenu_context_keys[i], true) == NULL) {
            cJSON_Delete(menu);
            cJSON_Delete(context);
            return false;
        }
    }

    if (cJSON_AddBoolToObject(root, "hotkey", true) == NULL) {
        cJSON_Delete(menu);
        cJSON_Delete(context);
        return false;
    }
    if (!cJSON_AddItemToObject(root, "menu", menu)) {
        cJSON_Delete(menu);
        cJSON_Delete(context);
        return false;
    }
    if (!cJSON_AddItemToObject(root, "context", context)) {
        cJSON_Delete(context);
        return false;
    }
    return true;
}

static bool mainmenu_store_menu_item(int target_index, bool enabled)
{
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root(&exists, &valid);
    if (!valid || root == NULL)
        return false;

    if (!exists && !mainmenu_add_default_sections(root)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *menu = cJSON_GetObjectItemCaseSensitive(root, "menu");
    bool states[MAINMENU_MENU_COUNT];
    mainmenu_read_menu_states(root, states);
    if (!cJSON_IsObject(menu)) {
        cJSON *replacement = cJSON_CreateObject();
        if (replacement == NULL) {
            cJSON_Delete(root);
            return false;
        }
        for (int i = 0; i < MAINMENU_MENU_COUNT; i++) {
            if (cJSON_AddBoolToObject(replacement, mainmenu_menu_keys[i], states[i]) == NULL) {
                cJSON_Delete(replacement);
                cJSON_Delete(root);
                return false;
            }
        }
        if (!mainmenu_replace_root_section(root, "menu", replacement)) {
            cJSON_Delete(replacement);
            cJSON_Delete(root);
            return false;
        }
        menu = cJSON_GetObjectItemCaseSensitive(root, "menu");
    }
    else if (!mainmenu_menu_object_has_enabled(menu)) {
        for (int i = 0; i < MAINMENU_MENU_COUNT; i++) {
            if (!mainmenu_set_object_alias(menu, i, states[i], false)) {
                cJSON_Delete(root);
                return false;
            }
        }
    }

    bool ok = mainmenu_set_object_alias(menu, target_index, enabled, false) &&
              mainmenu_write_root(root);
    cJSON_Delete(root);
    return ok;
}

static bool mainmenu_store_context_item(int target_index, bool enabled)
{
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root(&exists, &valid);
    if (!valid || root == NULL)
        return false;

    if (!exists && !mainmenu_add_default_sections(root)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *context = cJSON_GetObjectItemCaseSensitive(root, "context");
    bool ok = false;
    if (cJSON_IsObject(context)) {
        ok = mainmenu_set_object_alias(context, target_index, enabled, true);
    }
    else if (cJSON_IsArray(context)) {
        ok = mainmenu_set_array_context(context, target_index, enabled);
    }
    else {
        bool states[MAINMENU_CONTEXT_COUNT];
        mainmenu_read_context_states(root, states);
        states[target_index] = enabled;

        cJSON *replacement = cJSON_CreateObject();
        if (replacement != NULL) {
            ok = true;
            for (int i = 0; i < MAINMENU_CONTEXT_COUNT; i++) {
                if (!states[i])
                    continue;
                if (cJSON_AddBoolToObject(replacement,
                                          mainmenu_context_keys[i], true) == NULL) {
                    ok = false;
                    break;
                }
            }
            if (ok)
                ok = mainmenu_replace_root_section(root, "context", replacement);
        }
        if (!ok)
            cJSON_Delete(replacement);
    }

    if (ok)
        ok = mainmenu_write_root(root);
    cJSON_Delete(root);
    return ok;
}

static bool mainmenu_store_hotkey(bool enabled)
{
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root(&exists, &valid);
    if (!valid || root == NULL)
        return false;

    if (!exists && !mainmenu_add_default_sections(root)) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *value = cJSON_CreateBool(enabled);
    if (value == NULL) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *hotkey = cJSON_GetObjectItemCaseSensitive(root, "hotkey");
    bool ok = hotkey != NULL
                  ? cJSON_ReplaceItemInObjectCaseSensitive(root, "hotkey", value)
                  : cJSON_AddItemToObject(root, "hotkey", value);
    if (!ok)
        cJSON_Delete(value);
    else
        ok = mainmenu_write_root(root);

    cJSON_Delete(root);
    return ok;
}

static void mainmenu_action_hotkey(void *pt)
{
    ListItem *item = (ListItem *)pt;
    if (!mainmenu_store_hotkey(item->value != 0))
        item->value = mainmenu_load_hotkey_state();
}

static void mainmenu_action_menu_item(void *pt)
{
    ListItem *item = (ListItem *)pt;
    int index = item->action_id;
    if (index < 0 || index >= MAINMENU_MENU_COUNT)
        return;
    if (!mainmenu_store_menu_item(index, item->value != 0)) {
        bool menu_states[MAINMENU_MENU_COUNT];
        bool context_states[MAINMENU_CONTEXT_COUNT];
        mainmenu_load_states(menu_states, context_states);
        item->value = menu_states[index];
    }
}

static void mainmenu_action_context_item(void *pt)
{
    ListItem *item = (ListItem *)pt;
    int index = item->action_id;
    if (index < 0 || index >= MAINMENU_CONTEXT_COUNT)
        return;
    if (!mainmenu_store_context_item(index, item->value != 0) ||
        !mainmenu_context_available(index)) {
        bool menu_states[MAINMENU_MENU_COUNT];
        bool context_states[MAINMENU_CONTEXT_COUNT];
        mainmenu_load_shown_states(menu_states, context_states);
        item->value = context_states[index];
    }
}

void menu_mainMenuItems(void *_)
{
    (void)_;
    if (!_menu_main_menu_items._created) {
        bool menu_states[MAINMENU_MENU_COUNT];
        bool context_states[MAINMENU_CONTEXT_COUNT];
        mainmenu_load_states(menu_states, context_states);
        _menu_main_menu_items = list_createWithTitle(
            MAINMENU_MENU_COUNT, LIST_SMALL, "Main menu items");
        for (int i = 0; i < MAINMENU_MENU_COUNT; i++) {
            list_addItem(&_menu_main_menu_items,
                         (ListItem){
                             .label = "",
                             .item_type = TOGGLE,
                             .value = menu_states[i],
                             .action_id = i,
                             .action = mainmenu_action_menu_item});
            strcpy(_menu_main_menu_items.items[i].label, mainmenu_menu_labels[i]);
        }
    }
    menu_stack[++menu_level] = &_menu_main_menu_items;
    header_changed = true;
}

void menu_mainContextItems(void *_)
{
    (void)_;
    if (!_menu_main_context_items._created) {
        bool menu_states[MAINMENU_MENU_COUNT];
        bool context_states[MAINMENU_CONTEXT_COUNT];
        mainmenu_load_shown_states(menu_states, context_states);
        _menu_main_context_items = list_createWithTitle(
            MAINMENU_CONTEXT_LIST_COUNT, LIST_SMALL, "Context menu items");
        for (int i = 0; i < MAINMENU_CONTEXT_COUNT; i++) {
            ListItem item = {.label = "",
                             .item_type = TOGGLE,
                             .value = context_states[i],
                             .action_id = i,
                             .action = mainmenu_action_context_item};
            if (i == 2 && !mainmenu_context_available(i))
                list_addItemWithInfoNote(&_menu_main_context_items, item,
                                         "Search is not installed. Install it\n"
                                         "from the Package Manager (Apps) to\n"
                                         "show it in the context menu.");
            else if (i >= 11 && !mainmenu_context_available(i))
                list_addItemWithInfoNote(&_menu_main_context_items, item,
                                         "Not defined: give it a label and a\n"
                                         "launch command under \"custom\" in\n"
                                         "config/main-menu.json to show it.");
            else
                list_addItem(&_menu_main_context_items, item);
            strcpy(_menu_main_context_items.items[i].label,
                   mainmenu_context_labels[i]);
        }
        list_addItemWithInfoNote(
            &_menu_main_context_items,
            (ListItem){
                .label = "Activate hotkey",
                .item_type = TOGGLE,
                .value = mainmenu_load_hotkey_state(),
                .action = mainmenu_action_hotkey},
            "Enable the MainUI hotkey that opens the\n"
            "context menu. This root setting is not a\n"
            "context-menu item. Default: on.");
    }
    menu_stack[++menu_level] = &_menu_main_context_items;
    header_changed = true;
}

/* ---- Console view: console-view.json (Open MainUI 1.1.0 rc7) ----
 *
 * "games" and "expert" pick how those screens show the consoles: "grid"
 * (the stock grid, also when the file or the key is missing), "carousel"
 * or "list". "thumbs" names the folder of the console pictures used by the
 * carousel and the list; without it they show the theme's pictures. Every
 * other key (count, menu, carousel, list...) is kept as written. A file
 * that can't be read is left alone.
 */
#define CONSOLEVIEW_CONFIG_PATH MAINMENU_CONFIG_DIR "/console-view.json"
#define CONSOLEVIEW_GAMES 0
#define CONSOLEVIEW_EXPERT 1
#define CONSOLEVIEW_GRID 0
#define CONSOLEVIEW_CAROUSEL 1
#define CONSOLEVIEW_LIST 2
#define CONSOLEVIEW_LAYOUT_COUNT 3
#ifndef CONSOLEVIEW_THUMBS_PARENT
#define CONSOLEVIEW_THUMBS_PARENT "/mnt/SDCARD/miyoo/app"
#endif
#define CONSOLEVIEW_THUMBS_DIR CONSOLEVIEW_THUMBS_PARENT "/consoles"

static const char *consoleview_screen_keys[2] = {"games", "expert"};
static const char *consoleview_layout_keys[CONSOLEVIEW_LAYOUT_COUNT] = {
    "grid", "carousel", "list"};

static int consoleview_layout_of(cJSON *root, int screen)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(root, consoleview_screen_keys[screen]);
    if (!cJSON_IsString(value))
        return CONSOLEVIEW_GRID;
    for (int i = 0; i < CONSOLEVIEW_LAYOUT_COUNT; i++)
        if (strcmp(value->valuestring, consoleview_layout_keys[i]) == 0)
            return i;
    return CONSOLEVIEW_GRID;
}

static int consoleview_layout(int screen)
{
    if (screen != CONSOLEVIEW_GAMES && screen != CONSOLEVIEW_EXPERT)
        return CONSOLEVIEW_GRID;
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root_file(CONSOLEVIEW_CONFIG_PATH, &exists, &valid);
    int layout = valid && root != NULL ? consoleview_layout_of(root, screen) : CONSOLEVIEW_GRID;
    cJSON_Delete(root);
    return layout;
}

static bool consoleview_has_thumbs_in(cJSON *root)
{
    cJSON *thumbs = cJSON_GetObjectItemCaseSensitive(root, "thumbs");
    return cJSON_IsString(thumbs) && thumbs->valuestring[0] != '\0';
}

// On when the file names a folder of console pictures (ours or another).
static bool consoleview_has_thumbs(void)
{
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root_file(CONSOLEVIEW_CONFIG_PATH, &exists, &valid);
    bool on = valid && root != NULL && consoleview_has_thumbs_in(root);
    cJSON_Delete(root);
    return on;
}

// True when the file holds nothing but the grid for both screens.
static bool consoleview_is_stock(cJSON *root)
{
    for (cJSON *item = root->child; item != NULL; item = item->next) {
        if (item->string == NULL)
            return false;
        int screen = strcmp(item->string, "games") == 0    ? CONSOLEVIEW_GAMES
                     : strcmp(item->string, "expert") == 0 ? CONSOLEVIEW_EXPERT
                                                           : -1;
        if (screen < 0 || !cJSON_IsString(item) ||
            strcmp(item->valuestring, "grid") != 0)
            return false;
    }
    return true;
}

// Writes the root back, or removes the file when it is only the stock grid.
static bool consoleview_save(cJSON *root)
{
    if (consoleview_is_stock(root))
        return remove(CONSOLEVIEW_CONFIG_PATH) == 0 || errno == ENOENT;
    return mainmenu_write_root_file(CONSOLEVIEW_CONFIG_PATH, root);
}

// On: "thumbs" is our folder (created for the pictures), unless the file
// already names one. Off: "thumbs" is removed.
static bool consoleview_store_thumbs(bool enabled)
{
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root_file(CONSOLEVIEW_CONFIG_PATH, &exists, &valid);
    if (!valid || root == NULL) {
        cJSON_Delete(root);
        return false;
    }

    bool ok = true;
    if (!enabled) {
        cJSON_DeleteItemFromObjectCaseSensitive(root, "thumbs");
    }
    else if (!consoleview_has_thumbs_in(root)) {
        mkdir(CONSOLEVIEW_THUMBS_PARENT, 0755);
        mkdir(CONSOLEVIEW_THUMBS_DIR, 0755);
        cJSON *value = cJSON_CreateString(CONSOLEVIEW_THUMBS_DIR);
        ok = value != NULL;
        if (ok) {
            ok = cJSON_GetObjectItemCaseSensitive(root, "thumbs") != NULL
                     ? cJSON_ReplaceItemInObjectCaseSensitive(root, "thumbs", value)
                     : cJSON_AddItemToObject(root, "thumbs", value);
            if (!ok)
                cJSON_Delete(value);
        }
    }
    if (ok)
        ok = consoleview_save(root);
    cJSON_Delete(root);
    return ok;
}

static bool consoleview_store_layout(int screen, int layout)
{
    if ((screen != CONSOLEVIEW_GAMES && screen != CONSOLEVIEW_EXPERT) ||
        layout < 0 || layout >= CONSOLEVIEW_LAYOUT_COUNT)
        return false;
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root_file(CONSOLEVIEW_CONFIG_PATH, &exists, &valid);
    if (!valid || root == NULL) {
        cJSON_Delete(root);
        return false;
    }

    const char *key = consoleview_screen_keys[screen];
    cJSON *value = cJSON_CreateString(consoleview_layout_keys[layout]);
    bool ok = value != NULL;
    if (ok) {
        ok = cJSON_GetObjectItemCaseSensitive(root, key) != NULL
                 ? cJSON_ReplaceItemInObjectCaseSensitive(root, key, value)
                 : cJSON_AddItemToObject(root, key, value);
        if (!ok)
            cJSON_Delete(value);
    }
    if (ok)
        ok = consoleview_save(root);
    cJSON_Delete(root);
    return ok;
}

/* Look options of console-view.json (Open MainUI 1.1.0 rc8). Each one is a
 * key at the root or in "carousel" (and "grid" for wrap), with the values
 * Tweaks offers: the first has no text, which removes the key so Open MainUI
 * uses its own default (its defaults change between versions). A value is
 * written as a boolean ("true", "false"), a number, or else a string. A key
 * whose value Tweaks doesn't offer shows as the first one and is kept until
 * the option is changed; a section left empty is removed. */
#define CONSOLEVIEW_OPTION_COUNT 17
#define CONSOLEVIEW_COUNT 0
#define CONSOLEVIEW_LABELS 1
#define CONSOLEVIEW_SELECTION 2
#define CONSOLEVIEW_OPACITY 3
#define CONSOLEVIEW_ANIMATION 4
#define CONSOLEVIEW_WRAP 5
#define CONSOLEVIEW_EASING 6
#define CONSOLEVIEW_DURATION 7
#define CONSOLEVIEW_REPEAT 8
#define CONSOLEVIEW_SCALE 9
#define CONSOLEVIEW_GAP 10
#define CONSOLEVIEW_OFFSET 11
#define CONSOLEVIEW_PADDING 12
#define CONSOLEVIEW_VSYNC 13
#define CONSOLEVIEW_GRID_NAV 14
#define CONSOLEVIEW_FOLDER_ICON 15
#define CONSOLEVIEW_MENU 16

typedef struct {
    const char *sections[2]; // NULL: the root
    int section_count;
    const char *key;
    int count;
    const char *values[6];
} ConsoleViewOption;

static const ConsoleViewOption consoleview_options[CONSOLEVIEW_OPTION_COUNT] = {
    {{NULL}, 1, "count", 2, {NULL, "false"}},
    {{"carousel"}, 1, "labels", 2, {NULL, "false"}},
    {{"carousel"}, 1, "selection", 4, {NULL, "false", "#FFFFFF1F", "#FFFFFF38"}},
    {{"carousel"}, 1, "opacity_unselected", 4, {NULL, "1", "0.5", "0.3"}},
    {{"carousel"}, 1, "animation", 3, {NULL, "none", "slide"}},
    {{"carousel", "grid"}, 2, "wrap", 2, {NULL, "false"}},
    {{"carousel"}, 1, "easing", 2, {NULL, "smooth"}},
    {{"carousel"}, 1, "duration_ms", 6, {NULL, "180", "200", "300", "400", "600"}},
    {{"carousel"}, 1, "repeat_ms", 4, {NULL, "100", "200", "300"}},
    {{"carousel"}, 1, "scale_unselected", 5, {NULL, "1", "0.85", "0.7", "0.6"}},
    {{"carousel"}, 1, "gap", 6, {NULL, "0", "16", "32", "48", "64"}},
    {{"carousel"}, 1, "offset_y", 5, {NULL, "-40", "-20", "20", "40"}},
    {{"carousel"}, 1, "selection_padding", 5, {NULL, "0", "8", "16", "24"}},
    {{"carousel"}, 1, "vsync", 2, {NULL, "false"}},
    {{"grid"}, 1, "navigation", 2, {NULL, "stock"}},
    {{"list"}, 1, "folder_icon", 2, {NULL, "false"}},
    {{NULL}, 1, "menu", 4, {NULL, "[\"refresh_all\"]", "[\"refresh\"]", "[]"}},
};

static cJSON *consoleview_value_json(const char *text)
{
    if (text[0] == '[')
        return cJSON_Parse(text);
    if (strcmp(text, "true") == 0)
        return cJSON_CreateTrue();
    if (strcmp(text, "false") == 0)
        return cJSON_CreateFalse();
    if (isdigit((unsigned char)text[0]) || text[0] == '-')
        return cJSON_CreateNumber(atof(text));
    return cJSON_CreateString(text);
}

static bool consoleview_value_matches(const cJSON *item, const char *text)
{
    if (text[0] == '[') {
        cJSON *want = cJSON_Parse(text);
        bool same = want != NULL && cJSON_Compare(item, want, true);
        cJSON_Delete(want);
        return same;
    }
    if (strcmp(text, "true") == 0)
        return cJSON_IsTrue(item);
    if (strcmp(text, "false") == 0)
        return cJSON_IsFalse(item);
    if (isdigit((unsigned char)text[0]) || text[0] == '-') {
        double want = atof(text);
        return cJSON_IsNumber(item) && item->valuedouble > want - 0.001 &&
               item->valuedouble < want + 0.001;
    }
    return cJSON_IsString(item) && strcmp(item->valuestring, text) == 0;
}

static int consoleview_option_of(cJSON *root, int option)
{
    const ConsoleViewOption *o = &consoleview_options[option];
    cJSON *where = o->sections[0] ? cJSON_GetObjectItemCaseSensitive(root, o->sections[0]) : root;
    cJSON *item = cJSON_IsObject(where) ? cJSON_GetObjectItemCaseSensitive(where, o->key) : NULL;
    if (item == NULL)
        return 0;
    for (int i = 1; i < o->count; i++)
        if (consoleview_value_matches(item, o->values[i]))
            return i;
    return 0;
}

static int consoleview_option(int option)
{
    if (option < 0 || option >= CONSOLEVIEW_OPTION_COUNT)
        return 0;
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root_file(CONSOLEVIEW_CONFIG_PATH, &exists, &valid);
    int value = valid && root != NULL ? consoleview_option_of(root, option) : 0;
    cJSON_Delete(root);
    return value;
}

static bool consoleview_set_in(cJSON *object, const char *key, const char *text)
{
    if (text == NULL) {
        cJSON_DeleteItemFromObjectCaseSensitive(object, key);
        return true;
    }
    cJSON *value = consoleview_value_json(text);
    if (value == NULL)
        return false;
    bool ok = cJSON_GetObjectItemCaseSensitive(object, key) != NULL
                  ? cJSON_ReplaceItemInObjectCaseSensitive(object, key, value)
                  : cJSON_AddItemToObject(object, key, value);
    if (!ok)
        cJSON_Delete(value);
    return ok;
}

static bool consoleview_store_option(int option, int value)
{
    if (option < 0 || option >= CONSOLEVIEW_OPTION_COUNT)
        return false;
    const ConsoleViewOption *o = &consoleview_options[option];
    if (value < 0 || value >= o->count)
        return false;
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root_file(CONSOLEVIEW_CONFIG_PATH, &exists, &valid);
    if (!valid || root == NULL) {
        cJSON_Delete(root);
        return false;
    }
    const char *text = o->values[value];
    bool ok = true;
    for (int i = 0; ok && i < o->section_count; i++) {
        const char *name = o->sections[i];
        if (name == NULL) {
            ok = consoleview_set_in(root, o->key, text);
            continue;
        }
        cJSON *section = cJSON_GetObjectItemCaseSensitive(root, name);
        if (section != NULL && !cJSON_IsObject(section)) {
            ok = false; // not ours to replace
            break;
        }
        if (section == NULL) {
            if (text == NULL)
                continue;
            section = cJSON_AddObjectToObject(root, name);
            if (section == NULL) {
                ok = false;
                break;
            }
        }
        ok = consoleview_set_in(section, o->key, text);
        if (ok && section->child == NULL)
            cJSON_DeleteItemFromObjectCaseSensitive(root, name);
    }
    if (ok)
        ok = consoleview_save(root);
    cJSON_Delete(root);
    return ok;
}

static void consoleview_action_option(void *pt)
{
    ListItem *item = (ListItem *)pt;
    if (!consoleview_store_option(item->action_id, item->value))
        item->value = consoleview_option(item->action_id);
}

static void consoleview_action_thumbs(void *pt)
{
    ListItem *item = (ListItem *)pt;
    if (!consoleview_store_thumbs(item->value != 0))
        item->value = consoleview_has_thumbs();
}

static void consoleview_action_layout(void *pt)
{
    ListItem *item = (ListItem *)pt;
    int screen = item->action_id;
    if (!consoleview_store_layout(screen, item->value))
        item->value = consoleview_layout(screen);
}

void menu_consoleView(void *_)
{
    (void)_;
    if (!_menu_console_view._created) {
        _menu_console_view = list_createWithTitle(20, LIST_SMALL, "Console view");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Games view",
                .item_type = MULTIVALUE,
                .value_max = CONSOLEVIEW_LAYOUT_COUNT - 1,
                .value_labels = {"Grid", "Carousel", "List"},
                .value = consoleview_layout(CONSOLEVIEW_GAMES),
                .action_id = CONSOLEVIEW_GAMES,
                .action = consoleview_action_layout},
            "How Games shows the consoles: the grid,\n"
            "a carousel or a list.\n"
            "Needs Open MainUI 1.1.0.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Expert view",
                .item_type = MULTIVALUE,
                .value_max = CONSOLEVIEW_LAYOUT_COUNT - 1,
                .value_labels = {"Grid", "Carousel", "List"},
                .value = consoleview_layout(CONSOLEVIEW_EXPERT),
                .action_id = CONSOLEVIEW_EXPERT,
                .action = consoleview_action_layout},
            "How Expert shows the consoles: the grid,\n"
            "a carousel or a list.\n"
            "Needs Open MainUI 1.1.0.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Console pictures",
                .item_type = TOGGLE,
                .value = consoleview_has_thumbs(),
                .action = consoleview_action_thumbs},
            "Carousel and List show the pictures in\n"
            "miyoo/app/consoles (gba.png, ps.png...,\n"
            "_default.png for the others), else the\n"
            "theme's.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Game count",
                .item_type = MULTIVALUE,
                .value_max = 1,
                .value_labels = {"On", "Off"},
                .value = consoleview_option(CONSOLEVIEW_COUNT),
                .action_id = CONSOLEVIEW_COUNT,
                .action = consoleview_action_option},
            "Show the number of games\n"
            "under each console.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: names",
                .item_type = MULTIVALUE,
                .value_max = 1,
                .value_labels = {"On", "Off"},
                .value = consoleview_option(CONSOLEVIEW_LABELS),
                .action_id = CONSOLEVIEW_LABELS,
                .action = consoleview_action_option},
            "Show the console's name under its\n"
            "picture in the carousel.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: selection box",
                .item_type = MULTIVALUE,
                .value_max = 3,
                .value_labels = {"Theme", "Off", "Light", "Bright"},
                .value = consoleview_option(CONSOLEVIEW_SELECTION),
                .action_id = CONSOLEVIEW_SELECTION,
                .action = consoleview_action_option},
            "The box behind the selected console:\n"
            "the theme's colour, none, or white at\n"
            "12% (Light) or 22% (Bright).");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: selection padding",
                .item_type = MULTIVALUE,
                .value_max = 4,
                .value_labels = {"Default", "0 px", "8 px", "16 px", "24 px"},
                .value = consoleview_option(CONSOLEVIEW_PADDING),
                .action_id = CONSOLEVIEW_PADDING,
                .action = consoleview_action_option},
            "Space between the selected picture and\n"
            "its box. Default: as Open MainUI sets it\n"
            "(12 px).");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: other consoles",
                .item_type = MULTIVALUE,
                .value_max = 3,
                .value_labels = {"Default", "Clear", "Dimmed", "Faint"},
                .value = consoleview_option(CONSOLEVIEW_OPACITY),
                .action_id = CONSOLEVIEW_OPACITY,
                .action = consoleview_action_option},
            "How the consoles beside the selected one\n"
            "look: Clear, Dimmed (50%) or Faint (30%).\n"
            "Default: as Open MainUI sets it (50%).");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: other consoles size",
                .item_type = MULTIVALUE,
                .value_max = 4,
                .value_labels = {"Default", "Same", "85%", "70%", "60%"},
                .value = consoleview_option(CONSOLEVIEW_SCALE),
                .action_id = CONSOLEVIEW_SCALE,
                .action = consoleview_action_option},
            "Size of the consoles beside the selected\n"
            "one. Default: as Open MainUI sets it (70%).");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: spacing",
                .item_type = MULTIVALUE,
                .value_max = 5,
                .value_labels = {"Default", "0 px", "16 px", "32 px", "48 px", "64 px"},
                .value = consoleview_option(CONSOLEVIEW_GAP),
                .action_id = CONSOLEVIEW_GAP,
                .action = consoleview_action_option},
            "Space between the console pictures.\n"
            "Default: as Open MainUI sets it (40 px).");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: height",
                .item_type = MULTIVALUE,
                .value_max = 4,
                .value_labels = {"Default", "Up 40 px", "Up 20 px", "Down 20 px", "Down 40 px"},
                .value = consoleview_option(CONSOLEVIEW_OFFSET),
                .action_id = CONSOLEVIEW_OFFSET,
                .action = consoleview_action_option},
            "Moves the pictures and their names\n"
            "up or down. Default: centred.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: animation",
                .item_type = MULTIVALUE,
                .value_max = 2,
                .value_labels = {"Default", "None", "Slide"},
                .value = consoleview_option(CONSOLEVIEW_ANIMATION),
                .action_id = CONSOLEVIEW_ANIMATION,
                .action = consoleview_action_option},
            "Slide moves the consoles into place.\n"
            "Default: as Open MainUI sets it.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: motion",
                .item_type = MULTIVALUE,
                .value_max = 1,
                .value_labels = {"Default", "Smooth"},
                .value = consoleview_option(CONSOLEVIEW_EASING),
                .action_id = CONSOLEVIEW_EASING,
                .action = consoleview_action_option},
            "How a slide moves. Default: fast, then\n"
            "slowing down. Smooth: speeds up and\n"
            "slows down gently. Only with Slide.\n"
            "Needs Open MainUI 1.1.0.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: slide time",
                .item_type = MULTIVALUE,
                .value_max = 5,
                .value_labels = {"Default", "180 ms", "200 ms", "300 ms", "400 ms", "600 ms"},
                .value = consoleview_option(CONSOLEVIEW_DURATION),
                .action_id = CONSOLEVIEW_DURATION,
                .action = consoleview_action_option},
            "How long a slide takes. Default: as\n"
            "Open MainUI sets it (240 ms).\n"
            "Only with Slide.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: hold speed",
                .item_type = MULTIVALUE,
                .value_max = 3,
                .value_labels = {"Default", "100 ms", "200 ms", "300 ms"},
                .value = consoleview_option(CONSOLEVIEW_REPEAT),
                .action_id = CONSOLEVIEW_REPEAT,
                .action = consoleview_action_option},
            "Time between moves while Left/Right is\n"
            "held. Default: one move per slide.\n"
            "Slides are never closer than the slide\n"
            "time. Needs Open MainUI 1.1.0.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Carousel: VSync",
                .item_type = MULTIVALUE,
                .value_max = 1,
                .value_labels = {"On", "Off"},
                .value = consoleview_option(CONSOLEVIEW_VSYNC),
                .action_id = CONSOLEVIEW_VSYNC,
                .action = consoleview_action_option},
            "Draws the carousel in step with the\n"
            "screen: less tearing. Turns itself off\n"
            "if the screen is too slow for it.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Wrap around",
                .item_type = MULTIVALUE,
                .value_max = 1,
                .value_labels = {"On", "Off"},
                .value = consoleview_option(CONSOLEVIEW_WRAP),
                .action_id = CONSOLEVIEW_WRAP,
                .action = consoleview_action_option},
            "From the last console go on to the\n"
            "first, in the carousel and the grid.\n"
            "Off: stops at the first and last console.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Grid: navigation",
                .item_type = MULTIVALUE,
                .value_max = 1,
                .value_labels = {"Linear", "Stock"},
                .value = consoleview_option(CONSOLEVIEW_GRID_NAV),
                .action_id = CONSOLEVIEW_GRID_NAV,
                .action = consoleview_action_option},
            "Linear: Left/Right go through the consoles\n"
            "in order. Stock: like the original MainUI,\n"
            "a page at a time at the row's end.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "List: folder icons",
                .item_type = MULTIVALUE,
                .value_max = 1,
                .value_labels = {"On", "Off"},
                .value = consoleview_option(CONSOLEVIEW_FOLDER_ICON),
                .action_id = CONSOLEVIEW_FOLDER_ICON,
                .action = consoleview_action_option},
            "Show a folder icon beside each console\n"
            "in the List view.");
        list_addItemWithInfoNote(
            &_menu_console_view,
            (ListItem){
                .label = "Console menu",
                .item_type = MULTIVALUE,
                .value_max = 3,
                .value_labels = {"Default", "Refresh all", "Refresh", "None"},
                .value = consoleview_option(CONSOLEVIEW_MENU),
                .action_id = CONSOLEVIEW_MENU,
                .action = consoleview_action_option},
            "The menu on the Games/Expert console\n"
            "page: both refresh actions, only one,\n"
            "or none.");
    }
    menu_stack[++menu_level] = &_menu_console_view;
    header_changed = true;
}

/* Game-list context menus: .tmp_update/config/game-menu.json (Open MainUI
 * 1.1.0). Each section is an array of action ids, in menu order; a missing
 * section keeps the built-in menu, an empty one hides it. Tweaks shows one
 * toggle per action. Turning one off removes it from its section; turning
 * it on puts it back after the actions that come before it by default, so
 * an order set by hand is kept. A section back to the default is removed,
 * and the file with the last one. Unknown ids are kept. */
#define GAMEMENU_CONFIG_PATH MAINMENU_CONFIG_DIR "/game-menu.json"
#define GAMEMENU_SECTION_COUNT 4
#define GAMEMENU_ITEM_COUNT 16

typedef struct {
    int section;
    const char *id;
    const char *label;
} GameMenuItem;

static const char *const gamemenu_sections[GAMEMENU_SECTION_COUNT] = {"roms", "favorites", "recents", "search_results"};
static const char *const gamemenu_section_labels[GAMEMENU_SECTION_COUNT] = {"Games", "Favorites", "Recents", "Search"};

static const GameMenuItem gamemenu_items[GAMEMENU_ITEM_COUNT] = {
    {0, "launch", "Launch"},
    {0, "favorite", "Favorite"},
    {0, "delete", "Delete"},
    {0, "search", "Search"},
    {0, "refresh", "Refresh"},
    {1, "move", "Move"},
    {1, "create_folder", "Create folder"},
    {1, "sort", "Sort"},
    {1, "remove_favorite", "Remove favorite"},
    {1, "rename_folder", "Rename folder"},
    {1, "delete_folder", "Delete folder"},
    {2, "launch", "Launch"},
    {2, "remove_recent", "Remove recent"},
    {2, "clear_recents", "Clear recents"},
    {3, "launch", "Launch"},
    {3, "favorite", "Favorite"},
};

// Position of an action among its section's defaults, or -1.
static int gamemenu_default_index(int section, const char *id)
{
    int n = 0;
    for (int i = 0; i < GAMEMENU_ITEM_COUNT; i++) {
        if (gamemenu_items[i].section != section)
            continue;
        if (strcmp(gamemenu_items[i].id, id) == 0)
            return n;
        n++;
    }
    return -1;
}

static bool gamemenu_array_has(const cJSON *array, const char *id)
{
    const cJSON *entry;
    cJSON_ArrayForEach(entry, array)
    {
        if (cJSON_IsString(entry) && strcmp(entry->valuestring, id) == 0)
            return true;
    }
    return false;
}

static bool gamemenu_is_default(int section, const cJSON *array)
{
    const cJSON *entry = array->child;
    for (int i = 0; i < GAMEMENU_ITEM_COUNT; i++) {
        if (gamemenu_items[i].section != section)
            continue;
        if (!cJSON_IsString(entry) || strcmp(entry->valuestring, gamemenu_items[i].id) != 0)
            return false;
        entry = entry->next;
    }
    return entry == NULL;
}

static bool gamemenu_shown(int item)
{
    if (item < 0 || item >= GAMEMENU_ITEM_COUNT)
        return false;
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root_file(GAMEMENU_CONFIG_PATH, &exists, &valid);
    bool shown = true;
    if (valid && root != NULL) {
        const cJSON *array = cJSON_GetObjectItemCaseSensitive(root, gamemenu_sections[gamemenu_items[item].section]);
        if (cJSON_IsArray(array))
            shown = gamemenu_array_has(array, gamemenu_items[item].id);
    }
    cJSON_Delete(root);
    return shown;
}

static bool gamemenu_store(int item, bool shown)
{
    if (item < 0 || item >= GAMEMENU_ITEM_COUNT)
        return false;
    bool exists = false;
    bool valid = false;
    cJSON *root = mainmenu_load_root_file(GAMEMENU_CONFIG_PATH, &exists, &valid);
    if (!valid || root == NULL) {
        cJSON_Delete(root);
        return false;
    }
    int section = gamemenu_items[item].section;
    const char *key = gamemenu_sections[section];
    const char *id = gamemenu_items[item].id;
    cJSON *array = cJSON_GetObjectItemCaseSensitive(root, key);
    bool ok = true;
    if (array != NULL && !cJSON_IsArray(array)) {
        ok = false; // not ours to replace
    }
    else {
        if (array == NULL) {
            array = cJSON_AddArrayToObject(root, key);
            for (int i = 0; array != NULL && i < GAMEMENU_ITEM_COUNT; i++)
                if (gamemenu_items[i].section == section)
                    cJSON_AddItemToArray(array, cJSON_CreateString(gamemenu_items[i].id));
            ok = array != NULL;
        }
        if (ok && !shown) {
            for (int k = cJSON_GetArraySize(array) - 1; k >= 0; k--) {
                const cJSON *entry = cJSON_GetArrayItem(array, k);
                if (cJSON_IsString(entry) && strcmp(entry->valuestring, id) == 0)
                    cJSON_DeleteItemFromArray(array, k);
            }
        }
        else if (ok && !gamemenu_array_has(array, id)) {
            int want = gamemenu_default_index(section, id);
            int at = 0, k = 0;
            const cJSON *entry;
            cJSON_ArrayForEach(entry, array)
            {
                k++;
                if (cJSON_IsString(entry)) {
                    int index = gamemenu_default_index(section, entry->valuestring);
                    if (index >= 0 && index < want)
                        at = k;
                }
            }
            cJSON *value = cJSON_CreateString(id);
            ok = value != NULL && (at < cJSON_GetArraySize(array) ? cJSON_InsertItemInArray(array, at, value)
                                                                    : cJSON_AddItemToArray(array, value));
        }
        if (ok && gamemenu_is_default(section, array))
            cJSON_DeleteItemFromObjectCaseSensitive(root, key);
    }
    if (ok)
        ok = root->child == NULL ? (remove(GAMEMENU_CONFIG_PATH) == 0 || errno == ENOENT)
                                 : mainmenu_write_root_file(GAMEMENU_CONFIG_PATH, root);
    cJSON_Delete(root);
    return ok;
}

static void gamemenu_action_item(void *pt)
{
    ListItem *item = (ListItem *)pt;
    if (!gamemenu_store(item->action_id, item->value != 0))
        item->value = gamemenu_shown(item->action_id);
}

void menu_gameMenus(void *_)
{
    (void)_;
    if (!_menu_game_menus._created) {
        _menu_game_menus = list_createWithTitle(GAMEMENU_ITEM_COUNT, LIST_SMALL, "Game list menus");
        for (int i = 0; i < GAMEMENU_ITEM_COUNT; i++) {
            ListItem item = {.item_type = TOGGLE,
                             .value = gamemenu_shown(i),
                             .action_id = i,
                             .action = gamemenu_action_item};
            snprintf(item.label, sizeof(item.label), "%s: %s",
                     gamemenu_section_labels[gamemenu_items[i].section], gamemenu_items[i].label);
            list_addItemWithInfoNote(&_menu_game_menus, item,
                                     "Show this action in the game list's\n"
                                     "menu. Actions that don't apply to a\n"
                                     "game stay hidden anyway.\n"
                                     "Needs Open MainUI 1.1.0.");
        }
    }
    menu_stack[++menu_level] = &_menu_game_menus;
    header_changed = true;
}

void menu_mainMenu(void *_)
{
    (void)_;
    if (!_menu_main_menu._created) {
        _menu_main_menu = list_createWithTitle(4, LIST_SMALL, "Main menu");
        list_addItemWithInfoNote(
            &_menu_main_menu,
            (ListItem){.label = "Main menu items...", .action = menu_mainMenuItems},
            "Choose which top-level MainUI sections\n"
            "are visible. Restart MainUI after changes.");
        list_addItemWithInfoNote(
            &_menu_main_menu,
            (ListItem){.label = "Context menu items...", .action = menu_mainContextItems},
            "Choose which SELECT-menu entries are visible\n"
            "and whether its activation hotkey is enabled.\n"
            "Existing custom definitions are preserved.");
        list_addItemWithInfoNote(
            &_menu_main_menu,
            (ListItem){.label = "Console view...", .action = menu_consoleView},
            "Games and Expert as grid, carousel or\n"
            "list, console pictures and the carousel's\n"
            "look. Needs Open MainUI 1.1.0.");
        list_addItemWithInfoNote(
            &_menu_main_menu,
            (ListItem){.label = "Game list menus...", .action = menu_gameMenus},
            "Choose which actions the menus of Games,\n"
            "Favorites, Recents and Search show.\n"
            "Needs Open MainUI 1.1.0.");
    }
    menu_stack[++menu_level] = &_menu_main_menu;
    header_changed = true;
}

#endif // TWEAKS_MAIN_MENU_H__
