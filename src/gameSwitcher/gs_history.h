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

#include "gs_game_name.h"
#include "gs_model.h"
#include "gs_retroarch.h"
#include "gs_romscreen.h"

/**
 * @brief History extraction
 *
 * gs_readRecentFile() (gs_recent_item.h) reads the list, drops the
 * duplicates from the file and numbers the lines.
 */
void readHistory()
{
    // ~200 KB, only while reading (a large malloc goes back to the system)
    RecentItem *items = (RecentItem *)malloc(sizeof(RecentItem) * MAX_HISTORY);
    if (items == NULL)
        return; // keep what readFirstEntry() found
    int count = gs_readRecentFile(getMiyooRecentFilePath(), items, MAX_HISTORY);
    if (count < 0) {
        free(items);
        return; // unreadable: keep what readFirstEntry() found
    }

    for (int i = 0; i < count; i++) {
        Game_s *game = &game_list[i];
        // readFirstEntry() has prepared entry 0 (name, picture, running
        // state): keep that only if it is the same game, or the first game
        // would show another one's name and picture.
        bool same = game->processed && gs_sameRecentItem(&game->recentItem, &items[i]);
        if (!same && game->romScreen != NULL) {
            SDL_FreeSurface(game->romScreen);
            game->romScreen = NULL;
        }
        game->recentItem = items[i];
        if (!same)
            setEntryDefaultValues(game, i);
    }

    for (int i = count; i < game_list_len; i++) {
        if (game_list[i].romScreen != NULL) {
            SDL_FreeSurface(game_list[i].romScreen);
            game_list[i].romScreen = NULL;
        }
    }

    game_list_len = count;
    free(items);
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
    gs_fillShortName(game->shortname, sizeof(game->shortname), game->name,
                     game->rom_name, game->recentItem.label);

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
 * Same rules as readHistory() (ROM and launcher present), so that entry 0
 * is the game readHistory() puts first.
 */
void readFirstEntry()
{
    RecentItem item;
    if (gs_readRecentFile(getMiyooRecentFilePath(), &item, 1) != 1)
        return;

    Game_s *game = &game_list[0];
    game->recentItem = item;
    setEntryDefaultValues(game, 0);
    processItem(game);
    game_list_len = 1;
}

void getLaunchCommand(Game_s *game, char *launchCommand)
{
    snprintf(launchCommand, 4096, "LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so \"%s\" \"%s\"", game->recentItem.launch, game->recentItem.rompath);
}

// Fast handoff: GameSwitcher already has the launch paths and physical line
// number. Check the line still matches before moving it: history may have
// been edited while the overlay was open.
static bool resumeGameItem(Game_s *game)
{
    if (game == NULL || !gs_recentItemUsable(&game->recentItem))
        return false;

    const char *recentPath = getMiyooRecentFilePath();
    if (!gs_recentLineMatches(recentPath, &game->recentItem))
        return false;

    // Write the launch command first, atomically (temp + fsync + rename +
    // directory fsync). If it fails nothing has been changed yet, so the
    // caller's resumeGame() fallback starts from a clean state.
    char launchCommand[4096];
    getLaunchCommand(game, launchCommand);
    if (!file_atomic_write(CMD_TO_RUN_PATH, launchCommand, strlen(launchCommand))) {
        print_debug("resumeGameItem: could not write cmd_to_run.sh");
        return false;
    }

    // Reuse OnionPlus' atomic one-pass mover; don't reimplement history I/O.
    // The game is already set to launch, so a failed move only costs the
    // history order: log it rather than undo the launch.
    if (game->recentItem.lineNo > 1) {
        temp_flag_set("quick_switch", true);
        if (!file_move_line_to_top(recentPath, game->recentItem.lineNo))
            print_debug("resumeGameItem: could not move history entry to top");
    }

    remove("/mnt/SDCARD/.tmp_update/.runGameSwitcher");
    file_sync_dir("/mnt/SDCARD/.tmp_update"); // persist the removal
    printf_debug("resume game (direct): %s\n", launchCommand);
    temp_flag_set("force_auto_load_state", true);
    return true;
}

#endif // GAME_SWITCHER_HISTORY_H
