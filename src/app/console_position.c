/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/console_position.h"
#include "cJSON.h"
#include "platform/files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const keys[MAINUI_POSITION_SECTIONS] = {"games", "expert"};

static bool location(char out[4096], const char *directory)
{
    int n = directory ? snprintf(out, 4096, "%s/console-position.json", directory) : -1;
    return n > 0 && n < 4096;
}

const char *mainui_console_position_identity(const MainUIEntry *entry)
{
    return entry->config ? entry->config : entry->path;
}

void mainui_console_position_load(MainUIConsolePosition *position, const char *directory)
{
    *position = (MainUIConsolePosition){0};
    char file[4096];
    char *text = location(file, directory) ? mainui_read_text(file, 64 * 1024) : NULL;
    cJSON *root = text ? cJSON_Parse(text) : NULL;
    free(text);
    for (int i = 0; i < MAINUI_POSITION_SECTIONS; ++i) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(root, keys[i]);
        if (cJSON_IsString(value) && *value->valuestring &&
            strlen(value->valuestring) < sizeof position->console[i]) {
            strcpy(position->console[i], value->valuestring);
        }
    }
    cJSON_Delete(root);
}

bool mainui_console_position_save(const MainUIConsolePosition *position, const char *directory)
{
    char file[4096];
    cJSON *root = cJSON_CreateObject();
    bool ok = root && location(file, directory);
    for (int i = 0; ok && i < MAINUI_POSITION_SECTIONS; ++i) {
        ok = !*position->console[i] || cJSON_AddStringToObject(root, keys[i], position->console[i]);
    }
    char *text = ok ? cJSON_Print(root) : NULL;
    cJSON_Delete(root);
    ok = text && mainui_write_text_atomic(file, text);
    if (text && !ok) {
        fprintf(stderr, "Cannot save the selected console to %s\n", file);
    }
    free(text);
    return ok;
}
