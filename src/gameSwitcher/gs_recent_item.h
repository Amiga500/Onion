#ifndef GAME_SWITCHER_RECENT_ITEM_H
#define GAME_SWITCHER_RECENT_ITEM_H

// A GameSwitcher entry and the parsing of one line of MainUI's recent list,
// with plain SDL only (SDL_Surface pointer), so host tests include it.
// gs_model.h includes it.

#include <SDL/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cjson/cJSON.h"
#include "utils/file.h"
#include "utils/log.h"
#include "utils/str.h"

typedef struct {
    char label[STR_MAX * 2];
    char rompath[STR_MAX * 2];
    char imgpath[STR_MAX * 2];
    char launch[STR_MAX * 2];
    int type;
    int lineNo;
} RecentItem;

// Game history list
typedef struct {
    RecentItem recentItem;
    SDL_Surface *romScreen;
    char rom_name[STR_MAX * 2];
    char name[STR_MAX * 2];
    char shortname[STR_MAX * 2];
    char core_name[STR_MAX * 2];
    char core_path[STR_MAX * 2];
    char totalTime[100];
    int index;
    bool processed;
    bool is_running;
    int meta_state;         // GAME_META_*: name/core lookup, guarded by thread_mutex
    bool romscreen_busy;    // a thread is decoding this romscreen right now
    bool romscreen_missing; // no capture/artwork (or it failed to load): don't retry
} Game_s;

#define GAME_META_NEW 0
#define GAME_META_BUSY 1
#define GAME_META_READY 2

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
    game->romscreen_missing = false;

    strcpy(game->name, "");
    strcpy(game->shortname, "");
    strcpy(game->core_name, "");
    strcpy(game->core_path, "");
    game->index = index;
}

// The entry can be shown and started: its ROM and its launcher are there.
static inline bool gs_recentItemUsable(const RecentItem *item)
{
    return item != NULL && exists(item->rompath) && exists(item->launch);
}

// Same game: the same ROM through the same launcher.
static inline bool gs_sameRecentItem(const RecentItem *a, const RecentItem *b)
{
    return a != NULL && b != NULL &&
           strcmp(a->rompath, b->rompath) == 0 &&
           strcmp(a->launch, b->launch) == 0;
}

// Line item->lineNo (1-based) of the recent list still holds this game.
// The list can change while the GameSwitcher is open, and a line number
// that went stale would make the caller act on another game's line.
static inline bool gs_recentLineMatches(const char *path, const RecentItem *item)
{
    if (path == NULL || item == NULL || item->lineNo < 1)
        return false;
    char *line = file_read_lineN(path, item->lineNo);
    if (line == NULL)
        return false;
    RecentItem on_disk = {0};
    bool same = parseJsonToRecentItem(line, &on_disk, item->lineNo) &&
                gs_sameRecentItem(&on_disk, item);
    free(line);
    return same;
}

// Remove the game from the recent list, only if its line still holds it.
// False when nothing was removed (the caller keeps its list as it is).
static inline bool gs_deleteRecentLine(const char *path, const RecentItem *item)
{
    if (!gs_recentLineMatches(path, item))
        return false;
    return file_delete_lines(path, &item->lineNo, 1);
}

#define GS_RECENT_DUPLICATES_MAX 256

// Read up to `max` games from MainUI's recent list into out[], in order:
// entries of type 5/17 whose ROM and launcher are there, each ROM once.
// Later lines for a ROM already listed are deleted from the file in one
// rewrite. lineNo is each game's line in the file as it is afterwards: if
// that rewrite fails, the duplicates stay and so do the line numbers.
// Returns the number of games, or -1 if the list cannot be read.
static inline int gs_readRecentFile(const char *path, RecentItem *out, int max)
{
    FILE *file = path != NULL ? fopen(path, "r") : NULL;
    if (file == NULL || out == NULL || max <= 0) {
        if (file != NULL)
            fclose(file);
        print_debug("Error opening the recent list");
        return -1;
    }

    int dup_lines[GS_RECENT_DUPLICATES_MAX];
    int dup_count = 0;
    int count = 0;
    int line_no = 0;
    char *line = NULL;
    size_t line_cap = 0;

    while (count < max && getline(&line, &line_cap, file) != -1) {
        ++line_no;
        RecentItem item = {0};
        if (!parseJsonToRecentItem(line, &item, line_no))
            continue;

        bool duplicate = false;
        for (int i = 0; i < count && !duplicate; i++)
            duplicate = strcmp(out[i].rompath, item.rompath) == 0;
        if (duplicate) {
            if (dup_count < GS_RECENT_DUPLICATES_MAX)
                dup_lines[dup_count++] = line_no;
            continue;
        }

        if (!gs_recentItemUsable(&item))
            continue;

        out[count++] = item;
    }

    free(line);
    fclose(file);

    if (dup_count > 0 && file_delete_lines(path, dup_lines, dup_count)) {
        for (int i = 0; i < count; i++) {
            int removed_before = 0;
            for (int d = 0; d < dup_count; d++) {
                if (dup_lines[d] < out[i].lineNo)
                    removed_before++;
            }
            out[i].lineNo -= removed_before;
        }
    }

    return count;
}

#endif // GAME_SWITCHER_RECENT_ITEM_H
