/* SPDX-License-Identifier: GPL-3.0-only */
#include "menus/context.h"
#include "localization/language.h"
#include "platform/files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *keys[] = {"refresh",  "search",   "recents", "favorites", "games",
                             "apps",     "settings", "expert",  "themes",    "tweaks",
                             "shutdown", "custom1",  "custom2", "custom3"};

static int action_for(const char *key)
{
    if (!key) {
        return -1;
    }
    for (int i = 0; i < CONTEXT_COUNT; i++) {
        if (!strcmp(key, keys[i])) {
            return i;
        }
    }
    if (!strcmp(key, "recent")) {
        return CONTEXT_RECENTS;
    }
    if (!strcmp(key, "favorite") || !strcmp(key, "favourite") || !strcmp(key, "favourites") ||
        !strcmp(key, "favs")) {
        return CONTEXT_FAVORITES;
    }
    return -1;
}

static void visible(MainUIContext *context)
{
    context->visible_count = 0;
    for (int i = 0; i < context->count; i++) {
        if (context->entries[i].enabled) {
            context->visible[context->visible_count++] = i;
        }
    }
    context->selected = 0;
}

static void add(MainUIContext *context, int action, bool enabled)
{
    for (int i = 0; i < context->count; i++) {
        if (context->entries[i].action == (MainUIContextAction)action) {
            context->entries[i].enabled = enabled;
            return;
        }
    }
    if (context->count < CONTEXT_COUNT) {
        context->entries[context->count++] =
            (MainUIContextEntry){.action = action, .enabled = enabled, .type = 3};
    }
}

void mainui_context_parse(MainUIContext *context, const char *text, bool search, bool tweaks)
{
    *context = (MainUIContext){.hotkey = true};
    cJSON *root = mainui_menu_json(text);
    const cJSON *object = cJSON_GetObjectItemCaseSensitive(root, "context");
    const cJSON *hotkey = cJSON_GetObjectItemCaseSensitive(root, "hotkey");
    context->hotkey = !hotkey || cJSON_IsTrue(hotkey);
    if (!object) {
        add(context, CONTEXT_REFRESH, true);
        if (search) {
            add(context, CONTEXT_SEARCH, true);
        }
        if (tweaks) {
            add(context, CONTEXT_TWEAKS, true);
        }
    }
    else if (cJSON_IsObject(object) || cJSON_IsArray(object)) {
        const cJSON *item;
        cJSON_ArrayForEach(item, object)
        {
            int action = action_for(cJSON_IsObject(object) ? item->string
                                    : cJSON_IsString(item) ? item->valuestring
                                                           : NULL);
            if (action >= 0) {
                add(context, action, cJSON_IsArray(object) || cJSON_IsTrue(item));
            }
        }
    }
    const cJSON *custom = cJSON_GetObjectItemCaseSensitive(root, "custom");
    for (int i = 0; i < context->count; i++) {
        MainUIContextEntry *entry = &context->entries[i];
        if (entry->action < CONTEXT_CUSTOM1) {
            continue;
        }
        const cJSON *spec = cJSON_GetObjectItemCaseSensitive(custom, keys[entry->action]);
        const cJSON *label = cJSON_GetObjectItemCaseSensitive(spec, "label");
        const cJSON *launch = cJSON_GetObjectItemCaseSensitive(spec, "launch");
        const cJSON *type = cJSON_GetObjectItemCaseSensitive(spec, "type");
        if (!cJSON_IsString(label) || !*label->valuestring || !cJSON_IsString(launch) ||
            !*launch->valuestring || strlen(label->valuestring) >= sizeof entry->label ||
            strlen(launch->valuestring) >= sizeof entry->launch) {
            /* Remove unusable custom actions: a later reveal must not expose them. */
            memmove(entry, entry + 1, (size_t)(context->count - i - 1) * sizeof *entry);
            context->count--;
            i--;
            continue;
        }
        strcpy(entry->label, label->valuestring);
        strcpy(entry->launch, launch->valuestring);
        if (cJSON_IsNumber(type) && type->valuedouble == type->valueint) {
            entry->type = type->valueint;
        }
    }
    cJSON_Delete(root);
    visible(context);
}

static bool launcher(const char *sd, const char *name)
{
    char path[4096];
    int n = snprintf(path, sizeof path, "%s/App/%s/launch.sh", sd, name);
    if (n < 0 || n >= (int)sizeof path) {
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    fclose(file);
    return true;
}

void mainui_context_load(MainUIContext *context, const char *directory, const char *sd)
{
    char path[4096];
    int n = directory ? snprintf(path, sizeof path, "%s/main-menu.json", directory) : -1;
    char *text = n > 0 && n < (int)sizeof path ? mainui_read_text(path, 128 * 1024) : NULL;
    mainui_context_parse(context, text, launcher(sd, "Search"), launcher(sd, "Tweaks"));
    free(text);
}

void mainui_context_reveal(MainUIContext *context)
{
    for (int i = 0; i < context->count; i++) {
        context->entries[i].enabled = true;
    }
    visible(context);
}

const char *mainui_context_label(const MainUIContextEntry *entry)
{
    static const char *labels[] = {
        "Refresh all roms", "Search", "Recents", "Favorites", "Games",   "Apps",
        "Settings",         "Expert", "Themes",  "Tweaks",    "Shutdown"};
    static const int ids[] = {54, 153, 18, 1, 2, 107, 15, 0, 125, 407, 86};
    if (entry->action > CONTEXT_COUNT) {
        static const char *list_labels[] = {
            "Launch",          "Add to favorites", "Remove from list", "Clear recent game list",
            "Remove Favorite", "Delete",           "Refresh roms",     "Move selected",
            "Move here",       "Create folder",    "Rename folder",    "Delete folder",
            "Sort A-Z"};
        static const int list_ids[] = {51, 55, 52, 50, 405, 128, 27, 401, 402, 400, 403, 404, 406};
        int index = entry->action - CONTEXT_START;
        if (index < 0 || index >= (int)(sizeof list_ids / sizeof list_ids[0])) {
            return "";
        }
        return mainui_translate(list_ids[index], list_labels[index]);
    }
    if (entry->action >= CONTEXT_CUSTOM1) {
        return entry->label;
    }
    const char *label = mainui_translate(ids[entry->action], labels[entry->action]);
    return !strcmp(label, " ") ? labels[entry->action] : label;
}

int mainui_context_section(MainUIContextAction action)
{
    switch (action) {
    case CONTEXT_RECENTS:
        return MAINUI_MENU_RECENTS;
    case CONTEXT_FAVORITES:
        return MAINUI_MENU_FAVORITES;
    case CONTEXT_GAMES:
        return MAINUI_MENU_GAMES;
    case CONTEXT_APPS:
        return MAINUI_MENU_APPS;
    case CONTEXT_SETTINGS:
        return MAINUI_MENU_SETTINGS;
    case CONTEXT_EXPERT:
        return MAINUI_MENU_EXPERT;
    default:
        return -1;
    }
}

void mainui_context_rows(MainUIContext *context, const MainUIContextAction *actions, int count)
{
    *context = (MainUIContext){0};
    for (int i = 0; i < count && i < CONTEXT_COUNT; i++) {
        add(context, actions[i], true);
    }
    visible(context);
}
