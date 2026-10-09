/* SPDX-License-Identifier: GPL-3.0-only */
#include "localization/language.h"
#include "platform/files.h"
#include "platform/system_config.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static cJSON *active;

static char *duplicate(const char *text)
{
    size_t size = strlen(text) + 1;
    char *copy = malloc(size);
    if (copy) {
        memcpy(copy, text, size);
    }
    return copy;
}

static cJSON *read_language(const char *path)
{
    char *text = mainui_read_text(path, 1024 * 1024);
    /* Custom packs may have a UTF-8 BOM and private IDs beyond stock's 400 slots. */
    const char *json = text;
    if (text && strlen(text) >= 3 && (unsigned char)text[0] == 0xef &&
        (unsigned char)text[1] == 0xbb && (unsigned char)text[2] == 0xbf) {
        json += 3;
    }
    cJSON *root = json ? cJSON_ParseWithOpts(json, NULL, true) : NULL;
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static void add(MainUILanguages *list, const char *directory, const char *filename)
{
    size_t length = strlen(filename);
    if (length <= 5 || strcmp(filename + length - 5, ".lang") || list->count >= MAINUI_LANG_LIMIT) {
        return;
    }
    for (int i = 0; i < list->count; i++) {
        if (!strcmp(filename, list->entries[i].filename)) {
            return;
        }
    }
    char path[4096];
    int n = snprintf(path, sizeof path, "%s/%s", directory, filename);
    if (n < 0 || n >= (int)sizeof path) {
        return;
    }
    cJSON *root = read_language(path);
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "lang");
    if (cJSON_IsString(name) && *name->valuestring) {
        MainUILanguageEntry entry = {duplicate(path), duplicate(name->valuestring),
                                     duplicate(filename)};
        if (entry.path && entry.name && entry.filename) {
            list->entries[list->count++] = entry;
        }
        else {
            free(entry.path);
            free(entry.name);
            free(entry.filename);
        }
    }
    cJSON_Delete(root);
}

static void scan(MainUILanguages *list, const char *directory)
{
    DIR *directory_handle = opendir(directory);
    if (!directory_handle) {
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(directory_handle))) {
        add(list, directory, entry->d_name);
    }
    closedir(directory_handle);
}

static int compare(const void *a, const void *b)
{
    return strcmp(((const MainUILanguageEntry *)a)->name, ((const MainUILanguageEntry *)b)->name);
}

bool mainui_languages_open(MainUILanguages *list, const char *sd, const char *fallback)
{
    mainui_languages_close(list);
    char directory[4096];
    /* Same order as the startup loader. miyoo/app/lang is the folder stock
     * MainUI reads; when labels are hidden (theme or Tweaks override), Onion
     * blanks those strings there and keeps the originals in lang_backup for
     * its own apps. Earlier folders win for duplicate file names. */
    const char *locations[] = {"miyoo/app/lang", "miyoo/app/lang_backup"};
    for (int i = 0; i < 2; i++) {
        int n = snprintf(directory, sizeof directory, "%s/%s", sd, locations[i]);
        if (n > 0 && n < (int)sizeof directory) {
            scan(list, directory);
        }
    }
    int n = snprintf(directory, sizeof directory, "%s/lang", fallback);
    if (n > 0 && n < (int)sizeof directory) {
        scan(list, directory);
    }
    qsort(list->entries, list->count, sizeof list->entries[0], compare);
    cJSON *system = mainui_system_read(sd);
    const cJSON *selected = cJSON_GetObjectItemCaseSensitive(system, "language");
    const char *filename = cJSON_IsString(selected) ? selected->valuestring : "en.lang";
    for (int i = 0; i < list->count; i++) {
        if (!strcmp(list->entries[i].filename, filename)) {
            list->selected = i;
        }
    }
    cJSON_Delete(system);
    list->start = list->selected >= 6 ? list->selected - 5 : 0;
    return list->count > 0;
}

void mainui_languages_close(MainUILanguages *list)
{
    for (int i = 0; i < list->count; i++) {
        free(list->entries[i].path);
        free(list->entries[i].name);
        free(list->entries[i].filename);
    }
    *list = (MainUILanguages){0};
}

bool mainui_language_select(const MainUILanguages *list, const char *sd)
{
    if (list->selected < 0 || list->selected >= list->count) {
        return false;
    }
    const MainUILanguageEntry *entry = &list->entries[list->selected];
    cJSON *root = read_language(entry->path);
    cJSON *filename = cJSON_CreateString(entry->filename);
    bool ok = root && filename && mainui_system_write(sd, "language", filename);
    cJSON_Delete(filename);
    if (!ok) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_Delete(active);
    active = root;
    return true;
}

bool mainui_language_load(const char *sd, const char *fallback)
{
    cJSON *system = mainui_system_read(sd);
    const cJSON *selected = cJSON_GetObjectItemCaseSensitive(system, "language");
    const char *filename = cJSON_IsString(selected) ? selected->valuestring : "en.lang";
    size_t length = strlen(filename);
    cJSON *root = NULL;
    /* A plain file name only: system.json must not name a path. */
    if (length > 5 && length < 256 && !strcmp(filename + length - 5, ".lang") &&
        !strchr(filename, '/') && !strchr(filename, '\\') && strcmp(filename, "..")) {
        const char *locations[] = {"miyoo/app/lang", "miyoo/app/lang_backup"};
        char path[4096];
        for (int i = 0; !root && i < 3; i++) {
            int n = i < 2 ? snprintf(path, sizeof path, "%s/%s/%s", sd, locations[i], filename)
                          : snprintf(path, sizeof path, "%s/lang/%s", fallback, filename);
            if (n > 0 && n < (int)sizeof path) {
                root = read_language(path);
            }
        }
    }
    if (!root && strcmp(filename, "en.lang")) {
        fprintf(stderr, "Language %.80s unavailable; using built-in English\n", filename);
    }
    cJSON_Delete(system);
    cJSON_Delete(active);
    active = root;
    return root != NULL;
}

const char *mainui_translate(int id, const char *fallback)
{
    char key[24];
    snprintf(key, sizeof key, "%d", id);
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(active, key);
    return cJSON_IsString(value) && *value->valuestring ? value->valuestring : fallback;
}

void mainui_language_close(void)
{
    cJSON_Delete(active);
    active = NULL;
}
