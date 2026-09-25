#ifndef GAME_SWITCHER_HISTORY_H
#define GAME_SWITCHER_HISTORY_H

#include <SDL/SDL.h>
#include <pthread.h>
#include <stdbool.h>

#include "SDL/SDL_rotozoom.h"
#include "system/display.h"
#include "system/screenshot.h"
#include "system/state.h"
#include "utils/file.h"
#include "utils/json.h"
#include "utils/log.h"
#include "utils/str.h"

#include "../playActivity/cacheDB.h"
#include "../playActivity/playActivityDB.h"

#include "gs_model.h"
#include "gs_retroarch.h"
#include "gs_romscreen.h"

bool parseJsonToRecentItem(const char *jsonStr, RecentItem *recentItem, int lineNo)
{
    cJSON *json = cJSON_Parse(jsonStr);
    if (json == NULL) {
        print_debug("Error parsing JSON");
        return false;
    }

    cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (!cJSON_IsNumber(type) || (type->valueint != 5 && type->valueint != 17)) {
        cJSON_Delete(json);
        return false;
    }

    cJSON *label = cJSON_GetObjectItemCaseSensitive(json, "label");
    cJSON *rompath = cJSON_GetObjectItemCaseSensitive(json, "rompath");
    cJSON *imgpath = cJSON_GetObjectItemCaseSensitive(json, "imgpath");
    cJSON *launch = cJSON_GetObjectItemCaseSensitive(json, "launch");

    if (cJSON_IsString(label) && (label->valuestring != NULL)) {
        strncpy(recentItem->label, label->valuestring, sizeof(recentItem->label) - 1);
    }
    if (cJSON_IsString(rompath) && (rompath->valuestring != NULL)) {
        strncpy(recentItem->rompath, rompath->valuestring, sizeof(recentItem->rompath) - 1);
    }
    if (cJSON_IsString(imgpath) && (imgpath->valuestring != NULL)) {
        strncpy(recentItem->imgpath, imgpath->valuestring, sizeof(recentItem->imgpath) - 1);
    }
    if (cJSON_IsString(launch) && (launch->valuestring != NULL)) {
        strncpy(recentItem->launch, launch->valuestring, sizeof(recentItem->launch) - 1);
    }
    recentItem->type = type->valueint;
    recentItem->lineNo = lineNo;

    // Check if rompath contains a colon (':') and split it into launch and rompath
    char *colonPosition = strchr(recentItem->rompath, ':');
    if (colonPosition != NULL) {
        int position = (int)(colonPosition - recentItem->rompath);

        char firstPart[position + 1];
        strncpy(firstPart, recentItem->rompath, position);
        firstPart[position] = '\0';

        char secondPart[strlen(recentItem->rompath) - position];
        strcpy(secondPart, colonPosition + 1);

        strcpy(recentItem->launch, firstPart);
        strcpy(recentItem->rompath, secondPart);
    }

    cJSON_Delete(json);
    return true;
}

void setEntryDefaultValues(Game_s *game, int index)
{
    game->romScreen = NULL;
    game->totalTime[0] = '\0';
    game->processed = false;
    game->is_running = false;
    game->meta_state = GAME_META_NEW;
    game->romscreen_busy = false;

    strcpy(game->name, "");
    strcpy(game->shortname, "");
    strcpy(game->core_name, "");
    strcpy(game->core_path, "");
    game->index = index;
}

/**
 * @brief History extraction
 *
 */
void readHistory()
{
    FILE *file;
    int numRecents = 0;

    const char *recentFilePath = getMiyooRecentFilePath();

    file = fopen(recentFilePath, "r");
    if (file == NULL) {
        print_debug("Error opening file");
        return;
    }

    // Duplicates are collected and removed in ONE rewrite after the scan
    // (was: a full rewrite of the recent list per duplicate). Line numbers
    // stored in each entry already account for the lines deleted before it.
    int dup_lines[MAX_HISTORY * 2];
    int dup_count = 0;
    int orig_line_no = 0;

    char *line = NULL;
    size_t line_cap = 0;

    while (numRecents < MAX_HISTORY && getline(&line, &line_cap, file) != -1) {
        ++orig_line_no;
        int lineNo = orig_line_no - dup_count;

        if (!parseJsonToRecentItem(line, &game_list[numRecents].recentItem, lineNo)) {
            continue;
        }

        // Check for duplicates by looping over the list
        bool isDuplicate = false;
        for (int i = 0; i < numRecents; i++) {
            if (strcmp(game_list[i].recentItem.rompath, game_list[numRecents].recentItem.rompath) == 0) {
                isDuplicate = true;
                break;
            }
        }

        if (isDuplicate) {
            // Only count it as deleted if it will really be deleted, so the
            // numbering of the following entries stays correct.
            if (dup_count < (int)(sizeof(dup_lines) / sizeof(dup_lines[0])))
                dup_lines[dup_count++] = orig_line_no;
            continue;
        }

        if (!exists(game_list[numRecents].recentItem.rompath) || !exists(game_list[numRecents].recentItem.launch)) {
            continue;
        }

        if (!game_list[numRecents].processed) {
            setEntryDefaultValues(&game_list[numRecents], numRecents);
        }

        numRecents++;
    }

    free(line);
    fclose(file);

    if (dup_count > 0)
        file_delete_lines(recentFilePath, dup_lines, dup_count);

    game_list_len = numRecents;
}

bool getGameName(char *name_out, const char *rom_path)
{
    CacheDBItem *cache_item = cache_db_find(rom_path);
    if (cache_item != NULL) {
        strcpy(name_out, cache_item->name);
        free(cache_item);
        return true;
    }
    return false;
}

// Name/core lookup for one entry. Runs under meta_mutex (see
// gs_romscreen.h), either on the UI thread or on the prefetch worker.
void processItemMetaWork(Game_s *game)
{
    char *rom_name = file_removeExtension(file_basename(game->recentItem.rompath));
    if (rom_name != NULL) {
        snprintf(game->rom_name, sizeof(game->rom_name), "%s", rom_name);
        free(rom_name);
    }
    else {
        game->rom_name[0] = '\0';
    }

    if (!getGameName(game->name, game->recentItem.rompath)) {
        snprintf(game->name, sizeof(game->name), "%s", game->rom_name);
    }

    file_cleanName(game->shortname, game->name);

    if (ra_findItemInRetroArchHistory(game)) {
        ra_getCoreNameFromInfo(game);
    }

    // Play time shown in the header: computed here (prefetch worker, under
    // meta_mutex) so the UI thread does not open the database on the SD card
    // the first time each game is shown. renderHeader() still computes it if
    // it is missing (e.g. the time display was switched on later).
    if (romscreen_prefetch_play_time && game->totalTime[0] == '\0')
        str_serializeTime(game->totalTime, play_activity_get_play_time(game->recentItem.rompath));
}

void processItem(Game_s *game)
{
    // Usually already done by the prefetch worker; otherwise done here.
    romscreen_ensureMeta(game);

    if (game->romScreen == NULL) {
        loadRomScreen(game->index);
    }
}

/**
 * @brief Read the first entry from the history file
 *
 */
void readFirstEntry()
{
    FILE *file;
    char line[STR_MAX * 6];

    file = fopen(getMiyooRecentFilePath(), "r");
    if (file == NULL) {
        print_debug("Error opening file");
        return;
    }

    int lineNo = -1;
    bool found = false;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++lineNo;

        if (parseJsonToRecentItem(line, &game_list[0].recentItem, lineNo)) {
            found = true;
            break;
        }
    }

    if (found) {
        Game_s *game = &game_list[0];
        setEntryDefaultValues(game, 0);
        processItem(game);
        game_list_len = 1;
    }

    fclose(file);
}

void getLaunchCommand(Game_s *game, char *launchCommand)
{
    snprintf(launchCommand, 4096, "LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so \"%s\" \"%s\"", game->recentItem.launch, game->recentItem.rompath);
}

#endif // GAME_SWITCHER_HISTORY_H