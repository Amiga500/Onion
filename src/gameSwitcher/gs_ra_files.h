#ifndef GAME_SWITCHER_RA_FILES_H
#define GAME_SWITCHER_RA_FILES_H

// The RetroArch files the GameSwitcher reads (content history, config
// overrides), without SDL, so host tests include it. gs_retroarch.h
// includes it.

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjson/cJSON.h"
#include "utils/file.h"
#include "utils/log.h"
#include "utils/str.h"

static cJSON *g_cachedRetroArchHistory = NULL;

bool ra_loadHistory(const char *jsonFilePath)
{
    if (g_cachedRetroArchHistory != NULL) {
        cJSON_Delete(g_cachedRetroArchHistory);
        g_cachedRetroArchHistory = NULL;
    }

    FILE *file = fopen(jsonFilePath, "r");
    if (file == NULL) {
        print_debug("Error opening JSON file");
        return false;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *fileContent = (char *)malloc(fileSize + 1);
    fread(fileContent, 1, fileSize, file);
    fileContent[fileSize] = '\0';
    fclose(file);

    g_cachedRetroArchHistory = cJSON_Parse(fileContent);
    free(fileContent);

    if (g_cachedRetroArchHistory == NULL) {
        print_debug("Error parsing JSON");
        return false;
    }

    return true;
}

void ra_freeHistory()
{
    if (g_cachedRetroArchHistory != NULL) {
        cJSON_Delete(g_cachedRetroArchHistory);
        g_cachedRetroArchHistory = NULL;
    }
}

bool ra_getBoolFromConfig(const char *cfg_path, bool *out_value, const char *key)
{
    char value[STR_MAX * 2];
    file_parseKeyValue(cfg_path, key, value, '=', 0);
    if (strcmp(value, "true") == 0) {
        *out_value = true;
        return true;
    }
    else if (strcmp(value, "false") == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

#endif // GAME_SWITCHER_RA_FILES_H
