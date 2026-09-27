#ifndef GAME_SWITCHER_RA_FILES_H
#define GAME_SWITCHER_RA_FILES_H

// The RetroArch files the GameSwitcher reads (content history, config
// overrides), without SDL, so host tests include it. gs_retroarch.h
// includes it.

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

    // An unseekable or empty file, a failed allocation or a short read
    // used to go unnoticed: malloc(0 or -1 + 1), a NULL write, or parsing
    // uninitialised bytes.
    long fileSize = -1;
    if (fseek(file, 0, SEEK_END) == 0)
        fileSize = ftell(file);
    if (fileSize <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        print_debug("Error reading JSON file size");
        fclose(file);
        return false;
    }

    char *fileContent = (char *)malloc((size_t)fileSize + 1);
    if (fileContent == NULL) {
        fclose(file);
        return false;
    }
    size_t readSize = fread(fileContent, 1, (size_t)fileSize, file);
    fileContent[readSize] = '\0';
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

// RetroArch names a core's save-state (and config) folder after the core's
// library name, which is not always the "corename" of its .info file:
// Supafaust is "Beetle Supafaust" in the .info but writes to states/Supafaust/.
// When states_dir/<core_name> is not a folder, use the folder that holds this
// ROM's auto save state (every GameSwitcher game has one). Returns true when
// core_name names an existing folder of states_dir afterwards.
static bool ra_resolveCoreDirName(const char *states_dir, const char *rom_name,
                                  char *core_name, size_t core_name_size)
{
    char path[STR_MAX * 4];
    struct stat st;

    if (core_name[0] != '\0') {
        int n = snprintf(path, sizeof(path), "%s/%s", states_dir, core_name);
        if (n > 0 && (size_t)n < sizeof(path) && stat(path, &st) == 0 && S_ISDIR(st.st_mode))
            return true;
    }

    if (rom_name == NULL || rom_name[0] == '\0')
        return false;

    DIR *dir = opendir(states_dir);
    if (dir == NULL)
        return false;

    bool found = false;
    struct dirent *entry;
    while (!found && (entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' || strlen(entry->d_name) >= core_name_size)
            continue;
        int n = snprintf(path, sizeof(path), "%s/%s/%s.state.auto", states_dir, entry->d_name, rom_name);
        if (n < 0 || (size_t)n >= sizeof(path))
            continue;
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
            snprintf(core_name, core_name_size, "%s", entry->d_name);
            found = true;
        }
    }

    closedir(dir);
    return found;
}

#endif // GAME_SWITCHER_RA_FILES_H
