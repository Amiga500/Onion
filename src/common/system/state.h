#ifndef SYSTEM_STATE_H__
#define SYSTEM_STATE_H__

#include <unistd.h>

#include "utils/file.h"
#include "utils/flags.h"
#include "utils/hash.h"
#include "utils/log.h"
#include "utils/process.h"
#include "utils/str.h"

#include "./display.h"
#include "./settings.h"

#include "./state_decide.h"

static SystemState system_state = MODE_UNKNOWN;
static pid_t system_state_pid = 0;

bool check_isRetroArch(void)
{
    bool rc = false;
    if (!exists(CMD_TO_RUN_PATH))
        return false;
    char *cmd = file_read(CMD_TO_RUN_PATH);
    if (cmd == NULL)
        return false;
    if (strstr(cmd, "retroarch") != NULL ||
        strstr(cmd, "/mnt/SDCARD/Emu/") != NULL ||
        strstr(cmd, "/mnt/SDCARD/RApp/") != NULL) {
        pid_t pid;
        if ((pid = process_searchpid("retroarch")) != 0 ||
            (pid = process_searchpid("ra32")) != 0) {
            system_state_pid = pid;
            rc = true;
        }
    }
    free(cmd);
    return rc;
}

bool check_isMainUI(void)
{
    pid_t pid;
    if (!exists(CMD_TO_RUN_PATH) && (pid = process_searchpid("MainUI")) != 0) {
        system_state_pid = pid;
        return true;
    }
    return false;
}

bool check_isAdvMenu(void)
{
    pid_t pid;
    if (!exists(CMD_TO_RUN_PATH) && (pid = process_searchpid("advmenu")) != 0) {
        system_state_pid = pid;
        return true;
    }
    return false;
}

bool check_isGameSwitcher(void)
{
    pid_t pid;
    if (exists("/mnt/SDCARD/.tmp_update/.runGameSwitcher") &&
        (pid = process_searchpid("gameSwitcher")) != 0) {
        system_state_pid = pid;
        return true;
    }
    return false;
}

bool check_isDrastic(void)
{
    pid_t pid;
    if (exists(CMD_TO_RUN_PATH) && (pid = process_searchpid("drastic")) != 0) {
        system_state_pid = pid;
        return true;
    }
    return false;
}

void system_state_update(void)
{
    // Same decision order as the check_is*() helpers above, but /proc is
    // scanned once for all candidates instead of once per candidate
    // (up to 5 full scans before), and cmd_to_run.sh is read at most once.
    static const char *const names[STATE_P_COUNT] = {
        "gameSwitcher", "retroarch", "ra32", "MainUI", "advmenu", "drastic", "green"};
    pid_t pids[STATE_P_COUNT];

    bool switcher_flag = exists("/mnt/SDCARD/.tmp_update/.runGameSwitcher");
    bool has_cmd = exists(CMD_TO_RUN_PATH);
    bool ra_cmd = false;

    if (has_cmd) {
        char *cmd = file_read(CMD_TO_RUN_PATH);
        if (cmd != NULL) {
            ra_cmd = strstr(cmd, "retroarch") != NULL ||
                     strstr(cmd, "/mnt/SDCARD/Emu/") != NULL ||
                     strstr(cmd, "/mnt/SDCARD/RApp/") != NULL;
            free(cmd);
        }
    }

    process_searchpids(names, pids, STATE_P_COUNT);
    pid_t pid = 0;
    system_state = system_state_decide(switcher_flag, has_cmd, ra_cmd, pids, &pid);
    if (pid)
        system_state_pid = pid;

#ifdef LOG_DEBUG
    switch (system_state) {
    case MODE_MAIN_UI:
        print_debug("System state: Main UI");
        break;
    case MODE_SWITCHER:
        print_debug("System state: Game Switcher");
        break;
    case MODE_GAME:
        print_debug("System state: RetroArch");
        break;
    case MODE_APPS:
        print_debug("System state: Apps");
        break;
    default:
        print_debug("System state: Unknown");
        break;
    }
#endif
}

size_t state_getAppName(char *out, const char *str)
{
    char *end;
    size_t out_size;

    /* Skip the "HOME=<path> ./" prefix to reach the app name.
     * Using a dynamic search instead of a hardcoded offset avoids
     * mis-parsing when HOME is not exactly "/mnt/SDCARD". */
    const char *prefix_end = strstr(str, " ./");
    if (prefix_end == NULL) {
        *out = '\0';
        return 0;
    }
    str = prefix_end + 3; /* skip " ./" */

    end = (char *)strchr(str, ';');
    if (end == NULL)
        end = (char *)(str + strlen(str));

    out_size = (end - str) < STR_MAX - 1 ? (end - str) : STR_MAX - 1;
    memcpy(out, str, out_size);
    out[out_size] = 0;

    return out_size;
}

typedef enum mainui_states {
    MAIN_MENU,
    RECENTS,
    FAVORITES,
    GAMES,
    EXPERT,
    APPS
} MainUIState;

void write_mainui_state(MainUIState state, int currpos, int total)
{
    FILE *fp;
    char state_str[STR_MAX];
    int title_num = 0, page_type = 0, page_size = 6, page_start = 0, page_end,
        main_currpos = 0, main_page_start = 0, main_page_end;

    switch (state) {
    case MAIN_MENU:
        remove("/tmp/state.json");
        return;
    case RECENTS:
        title_num = 18;
        page_type = 10;
        main_currpos = 0;
        break;
    case FAVORITES:
        title_num = 1;
        page_type = 2;
        main_currpos = 1;
        break;
    case GAMES:
        title_num = 2;
        page_type = 1;
        page_size = 8;
        main_currpos = 2;
        break;
    case EXPERT:
        title_num = 0;
        page_type = 16;
        page_size = 9;
        main_currpos = 3;
        break;
    case APPS:
        title_num = 107;
        page_type = 3;
        page_size = 4;
        main_currpos = 4;
        break;
    default:
        return;
    }

    int main_total = 6;
    if (!settings.show_recents) {
        if (main_currpos > 0)
            main_currpos--;
        main_total--;
    }
    if (!settings.show_expert) {
        if (state == APPS)
            main_currpos--;
        main_total--;
    }

    if (main_currpos + 4 > main_total)
        main_page_start = (main_total >= 4) ? main_total - 4 : 0;
    main_page_end = main_page_start + 3;

    if (currpos + page_size > total)
        page_start = (total >= page_size) ? total - page_size : 0;
    else
        page_start = currpos;
    page_end = page_start + page_size - 1;

    snprintf(state_str, sizeof(state_str),
             "{\"list\":[{\"title\":157,\"type\":0,\"currpos\":%d,\"pagestart\":"
             "%d,\"pageend\":%d},{\"title\":%d,\"type\":%d,\"currpos\":%d,"
             "\"pagestart\":%d,\"pageend\":%d}]}",
             main_currpos, main_page_start, main_page_end, title_num, page_type,
             currpos, page_start, page_end);

    file_put_sync(fp, "/tmp/state.json", "%s", state_str);
}

//
//    [onion] get miyoo recent file path
//

char *getMiyooRecentFilePath()
{
    static char filename[STR_MAX];

    if (exists(RECENTLIST_HIDDEN_PATH))
        snprintf(filename, sizeof(filename), "%s", RECENTLIST_HIDDEN_PATH);
    else
        snprintf(filename, sizeof(filename), "%s", RECENTLIST_PATH);

    return filename;
}

//
//    [onion] get recent rom path from a recent-list file
//
//    top_only = false: first game entry whose ROM exists (skips apps and
//      missing ROMs). Used to name screenshots.
//    top_only = true: the most recent entry only, NULL if it is not a game
//      or its ROM is missing (upstream behavior). Used for the romscreen,
//      which must belong to the game that is running: falling through to an
//      older entry overwrote that game's GameSwitcher image with the screen
//      of whatever was running (e.g. the RetroArch app).
//
static char *_history_getRecentPath(const char *recent_path, char *rom_path, bool top_only)
{
    FILE *file = fopen(recent_path, "r");
    if (file == NULL)
        return NULL;

    // getline(): whole lines of any length. With fgets() into 768 bytes a
    // long entry was split, its "rompath" (the last field) landed in a
    // second piece without "type", and the game was skipped.
    char *line = NULL;
    size_t line_cap = 0;
    char *found = NULL;

    while (getline(&line, &line_cap, file) != -1) {
        char romPathSearch[STR_MAX];
        int type;

        const char *typeStr = strstr(line, "\"type\":");
        if (typeStr == NULL || sscanf(typeStr + 7, "%d", &type) != 1)
            continue;

        if ((type != 5) && (type != 17)) {
            if (top_only)
                break;
            continue;
        }

        const char *rompathStart = strstr(line, "\"rompath\":\"");
        const char *rompathEnd = rompathStart != NULL ? strchr(rompathStart + 11, '\"') : NULL;
        if (rompathEnd == NULL) {
            if (top_only)
                break;
            continue;
        }
        rompathStart += 11;

        size_t romPathLen = (size_t)(rompathEnd - rompathStart);
        if (romPathLen >= STR_MAX)
            romPathLen = STR_MAX - 1;
        memcpy(romPathSearch, rompathStart, romPathLen);
        romPathSearch[romPathLen] = '\0';

        // Game launched with the search panel
        char *colonPosition = strchr(romPathSearch, ':');
        if (colonPosition != NULL)
            memmove(romPathSearch, colonPosition + 1, strlen(colonPosition + 1) + 1);

        printf_debug("romPathSearch : %s\n", romPathSearch);

        if (!exists(romPathSearch)) {
            if (top_only)
                break;
            continue;
        }

        strncpy(rom_path, romPathSearch, STR_MAX - 1);
        rom_path[STR_MAX - 1] = '\0';
        found = rom_path;
        break;
    }

    free(line);
    fclose(file);
    return found;
}

char *history_getRecentPathFromPath(const char *recent_path, char *rom_path)
{
    return _history_getRecentPath(recent_path, rom_path, false);
}

char *history_getTopRecentPathFromPath(const char *recent_path, char *rom_path)
{
    return _history_getRecentPath(recent_path, rom_path, true);
}

char *history_getRecentPath(char *rom_path)
{
    return history_getRecentPathFromPath(getMiyooRecentFilePath(), rom_path);
}

char *history_getTopRecentPath(char *rom_path)
{
    return history_getTopRecentPathFromPath(getMiyooRecentFilePath(), rom_path);
}

bool history_getRomscreenPath(char *path_out)
{
    char filename[32];
    char file_path[STR_MAX] = "";

    filename[0] = '\0';
    // Only the most recent entry: see _history_getRecentPath().
    if (history_getTopRecentPath(file_path) != NULL) {
        snprintf(filename, sizeof(filename), "%" PRIu32, FNV1A_Pippip_Yurii(file_path, strlen(file_path)));
    }
    print_debug(file_path);
    if (strlen(filename) > 0) {
        snprintf(path_out, STR_MAX, "/mnt/SDCARD/Saves/CurrentProfile/romScreens/%s.png", filename);
        return true;
    }

    return false;
}

void resumeGame(int index)
{
    const char *recentPath = getMiyooRecentFilePath();
    FILE *file = fopen(recentPath, "r");

    int type;

    if (!file) {
        fprintf(stderr, "Can't open file %s\n", recentPath);
        return;
    }

    // getline(): count real lines, like file_read_lineN()/file_delete_line()
    // (a fixed 1 KB fgets() buffer counted a longer line twice, and the
    // quick-switch move-to-top could then delete the wrong entry).
    char *jsonContent = NULL;
    size_t jsonContent_cap = 0;
    int validGameCount = -1;
    int lineCount = 0;

    while (getline(&jsonContent, &jsonContent_cap, file) != -1) {
        char label[256] = "";
        char rompath[256] = "";
        char imgpath[256] = "";
        char launch[256] = "";
        lineCount++;

        const char *typeStr = strstr(jsonContent, "\"type\":");
        if (typeStr == NULL || sscanf(typeStr + 7, "%d", &type) != 1)
            continue;

        if ((type != 5) && (type != 17))
            continue;

        const char *labelStart = strstr(jsonContent, "\"label\":\"");
        if (labelStart != NULL) {
            labelStart += 9;
            const char *labelEnd = strchr(labelStart, '\"');
            if (labelEnd != NULL) {
                size_t len = (size_t)(labelEnd - labelStart);
                if (len >= sizeof(label))
                    len = sizeof(label) - 1;
                memcpy(label, labelStart, len);
                label[len] = '\0';
            }
        }
        printf_debug("label: %s\n", label);
        const char *rompathStart = strstr(jsonContent, "\"rompath\":\"");
        if (rompathStart != NULL) {
            rompathStart += 11;
            const char *rompathEnd = strchr(rompathStart, '\"');
            if (rompathEnd != NULL) {
                size_t len = (size_t)(rompathEnd - rompathStart);
                if (len >= sizeof(rompath))
                    len = sizeof(rompath) - 1;
                memcpy(rompath, rompathStart, len);
                rompath[len] = '\0';
            }
        }
        printf_debug("rompath: %s\n", rompath);
        const char *imgpathStart = strstr(jsonContent, "\"imgpath\":\"");
        if (imgpathStart != NULL) {
            imgpathStart += 11;
            const char *imgpathEnd = strchr(imgpathStart, '\"');
            if (imgpathEnd != NULL) {
                size_t len = (size_t)(imgpathEnd - imgpathStart);
                if (len >= sizeof(imgpath))
                    len = sizeof(imgpath) - 1;
                memcpy(imgpath, imgpathStart, len);
                imgpath[len] = '\0';
            }
        }

        char *colonPosition = strchr(rompath, ':');
        if (colonPosition != NULL) {

            int position = (int)(colonPosition - rompath);

            char firstPart[256];
            if (position > (int)sizeof(firstPart) - 1)
                position = (int)sizeof(firstPart) - 1;
            strncpy(firstPart, rompath, position);
            firstPart[position] = '\0';

            char secondPart[256];
            strncpy(secondPart, colonPosition + 1, sizeof(secondPart) - 1);
            secondPart[sizeof(secondPart) - 1] = '\0';

            strncpy(launch, firstPart, sizeof(launch) - 1);
            launch[sizeof(launch) - 1] = '\0';
            strncpy(rompath, secondPart, sizeof(rompath) - 1);
            rompath[sizeof(rompath) - 1] = '\0';
            printf_debug("launch cutted: %s\n", launch);
            printf_debug("rompath cutted: %s\n", rompath);
        }
        else {
            const char *launchStart = strstr(jsonContent, "\"launch\":\"");
            if (launchStart != NULL) {
                launchStart += 10;
                const char *launchEnd = strchr(launchStart, '\"');
                if (launchEnd != NULL) {
                    size_t len = (size_t)(launchEnd - launchStart);
                    if (len >= sizeof(launch))
                        len = sizeof(launch) - 1;
                    memcpy(launch, launchStart, len);
                    launch[len] = '\0';
                }
            }
        }

        if (!exists(rompath) || !exists(launch))
            continue;

        ++validGameCount;

        if (validGameCount == index) {

            FILE *fp;
            char LaunchCommand[STR_MAX * 3];

            fclose(file);
            file = NULL;
            snprintf(LaunchCommand, sizeof(LaunchCommand), "LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so \"%s\" \"%s\"", launch, rompath);

            remove("/mnt/SDCARD/.tmp_update/.runGameSwitcher");

            // move selected rom to top of recent list for quick switch
            if (lineCount > 1) {
                temp_flag_set("quick_switch", true);

                // one atomic rewrite (was: two full rewrites)
                file_move_line_to_top(recentPath, lineCount);
            }

            // Through a temporary file and a rename: on a quick switch the
            // shell is still running the old cmd_to_run.sh, and a file
            // rewritten in place could be read on from the middle.
            if (!file_atomic_write(CMD_TO_RUN_PATH, LaunchCommand, strlen(LaunchCommand))) {
                file_put_sync(fp, CMD_TO_RUN_PATH, "%s", LaunchCommand);
            }
            printf_debug("resume game: %s\n", LaunchCommand);

            temp_flag_set("force_auto_load_state", true);
            // Tells runtime.sh this is the auto save itself (its GameSwitcher
            // screenshot is right), unlike a Saved Games launch from MainUI.
            temp_flag_set("resume_last_game", true);

            // Was a global sync(). Everything durable here is covered more
            // cheaply: the history move is an fsynced atomic rename (file
            // and directory), cmd_to_run.sh is fsynced by file_put_sync and
            // flags live in /tmp. What remains is the directory entries for
            // the new cmd_to_run.sh and the removed .runGameSwitcher.
            file_sync_dir("/mnt/SDCARD/.tmp_update");
            break;
        }
    }

    if (file != NULL) {
        fclose(file);
    }
    free(jsonContent);
}

void set_resumeGame(void)
{
    resumeGame(0);
}

void set_quickSwitch(void)
{
    resumeGame(1);
}

#endif // SYSTEM_STATE_H__
