#ifndef GAME_SWITCHER_RETROARCH_H
#define GAME_SWITCHER_RETROARCH_H

#include "cjson/cJSON.h"

#include "gs_model.h"
#include "gs_ra_files.h"

bool ra_findItemInRetroArchHistory(Game_s *game)
{
    if (g_cachedRetroArchHistory == NULL && !ra_loadHistory(HISTORY_PATH)) {
        print_debug("Error loading RetroArch history");
        return false;
    }

    cJSON *items = cJSON_GetObjectItemCaseSensitive(g_cachedRetroArchHistory, "items");
    if (!cJSON_IsArray(items)) {
        return false;
    }

    char *cleanPath = file_resolvePath(game->recentItem.rompath);
    if (cleanPath == NULL) {
        return false;
    }

    printf_debug("Searching for: %s\n", cleanPath);

    cJSON *item;
    cJSON_ArrayForEach(item, items)
    {
        cJSON *path = cJSON_GetObjectItemCaseSensitive(item, "path");
        if (cJSON_IsString(path) && (path->valuestring != NULL)) {
            if (strcmp(path->valuestring, cleanPath) == 0) {
                // Found the matching item
                cJSON *core_path = cJSON_GetObjectItemCaseSensitive(item, "core_path");

                if (cJSON_IsString(core_path) && (core_path->valuestring != NULL)) {
                    strncpy(game->core_path, core_path->valuestring, sizeof(game->core_path) - 1);
                }

                printf_debug("Found item in RetroArch history: %s\n", path->valuestring);
                printf_debug("Core path: %s\n", game->core_path);

                free(cleanPath);
                return true;
            }
        }
    }

    free(cleanPath);
    return false;
}

void ra_getCoreNameFromInfo(Game_s *game)
{
    if (strlen(game->core_name) == 0) {
        if (strlen(game->core_path) == 0) {
            return;
        }

        char *basePath = file_removeExtension(game->core_path);
        if (basePath != NULL) {
            char infoPath[STR_MAX * 2];
            snprintf(infoPath, sizeof(infoPath), "%s.info", basePath);
            file_parseKeyValue(infoPath, "corename", game->core_name, '=', 0);
            printf_debug("Core name: %s\n", game->core_name);
            free(basePath);
        }
    }
}
bool ra_getConfigOverrideOption(const Game_s *game, const char *key, bool defaultValue)
{
    bool result = false;
    char cfg_path[4096];

    // Game override
    snprintf(cfg_path, sizeof(cfg_path), CONFIG_DIR "/%s/%s.cfg", game->core_name, game->rom_name);
    if (ra_getBoolFromConfig(cfg_path, &result, key)) {
        printf_debug("Game override: %s=%s\n", key, result ? "true" : "false");
        return result;
    }

    // Content directory override
    char *contentDir = file_dirname(game->recentItem.rompath);
    snprintf(cfg_path, sizeof(cfg_path), CONFIG_DIR "/%s/%s.cfg", game->core_name, file_basename(contentDir));
    free(contentDir);
    if (ra_getBoolFromConfig(cfg_path, &result, key)) {
        printf_debug("Content directory override: %s=%s\n", key, result ? "true" : "false");
        return result;
    }

    // Core override
    snprintf(cfg_path, sizeof(cfg_path), CONFIG_DIR "/%s/%s.cfg", game->core_name, game->core_name);
    if (ra_getBoolFromConfig(cfg_path, &result, key)) {
        printf_debug("Core override: %s=%s\n", key, result ? "true" : "false");
        return result;
    }

    // Global override
    if (ra_getBoolFromConfig(RETROARCH_CONFIG_PATH, &result, key)) {
        return result;
    }

    return defaultValue;
}

#endif // GAME_SWITCHER_RETROARCH_H