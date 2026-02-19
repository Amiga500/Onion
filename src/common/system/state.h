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

/*
 * Maximum byte size needed to read one line from recentlist.json.
 * Format: {"label":"L","launch":"X","type":17,"imgpath":"I","rompath":"R"}
 * Boilerplate: 64 bytes; 4 fields × (STR_MAX-1)=255 chars = 1020 bytes;
 * '\n' and '\0' = 2 bytes; 68 bytes headroom.
 * Total: 64 + 1020 + 2 + 68 = 1154 = STR_MAX * 4 + 130.
 */
#define JSON_RECENTS_LINE_MAX (STR_MAX * 4 + 130)

typedef enum system_state_e {
    MODE_UNKNOWN,
    MODE_MAIN_UI,
    MODE_SWITCHER,
    MODE_GAME,
    MODE_APPS,
    MODE_ADVMENU,
    MODE_DRASTIC
} SystemState;
static SystemState system_state = MODE_UNKNOWN;
static pid_t system_state_pid = 0;

bool check_isRetroArch(void)
{
    bool rc = false;
    if (!exists(CMD_TO_RUN_PATH))
        return false;
    char *cmd = file_read(CMD_TO_RUN_PATH);
    if (cmd != NULL && (strstr(cmd, "retroarch") != NULL ||
        strstr(cmd, "/mnt/SDCARD/Emu/") != NULL ||
        strstr(cmd, "/mnt/SDCARD/RApp/") != NULL)) {
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
    if (check_isGameSwitcher())
        system_state = MODE_SWITCHER;
    else if (check_isRetroArch())
        system_state = MODE_GAME;
    else if (check_isMainUI())
        system_state = MODE_MAIN_UI;
    else if (check_isAdvMenu())
        system_state = MODE_ADVMENU;
    else if (check_isDrastic())
        system_state = MODE_DRASTIC;
    else
        system_state = MODE_APPS;

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

    str += 19;
    end = (char *)strchr(str, ';');

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
        main_page_start = main_total - 4;
    main_page_end = main_page_start + 3;

    if (currpos + page_size > total)
        page_start = total - page_size;
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
//    [onion] get recent rom path from miyoo recent list
//
char *history_getRecentPath(char *rom_path)
{
    FILE *file;
    char line[JSON_RECENTS_LINE_MAX];

    file = fopen(getMiyooRecentFilePath(), "r");

    if (file == NULL) {
        return NULL;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char romPathSearch[STR_MAX];
        int type;

        const char *typePtr = strstr(line, "\"type\":");
        if (typePtr == NULL)
            continue;
        sscanf(typePtr + 7, "%d", &type);

        if ((type != 5) && (type != 17)) {
            fclose(file);
            return NULL;
        }

        const char *rompathPtr = strstr(line, "\"rompath\":\"");
        if (rompathPtr == NULL) {
            fclose(file);
            return NULL;
        }
        const char *rompathStart = rompathPtr + 11;
        const char *rompathEnd = strchr(rompathStart, '\"');
        if (rompathEnd == NULL) {
            fclose(file);
            return NULL;
        }

        size_t romPathLength = (size_t)(rompathEnd - rompathStart);
        if (romPathLength >= sizeof(romPathSearch)) romPathLength = sizeof(romPathSearch) - 1;
        strncpy(romPathSearch, rompathStart, romPathLength);
        romPathSearch[romPathLength] = '\0';

        // Game launched with the search panel
        char *colonPosition = strchr(romPathSearch, ':');
        if (colonPosition != NULL) {
            size_t suffixLen = strlen(colonPosition + 1);
            memmove(romPathSearch, colonPosition + 1, suffixLen + 1);
        }

        printf_debug("romPathSearch : %s\n", romPathSearch);

        if (!exists(romPathSearch)) {
            fclose(file);
            return NULL;
        }

        strncpy(rom_path, romPathSearch, STR_MAX - 1);
        rom_path[STR_MAX - 1] = '\0';

        fclose(file);
        return rom_path;
    }

    fclose(file);
    return NULL;
}

bool history_getRomscreenPath(char *path_out)
{
    char filename[STR_MAX] = "";
    char file_path[STR_MAX];

    if (history_getRecentPath(file_path) != NULL) {
        snprintf(filename, sizeof(filename), "%" PRIu32, FNV1A_Pippip_Yurii(file_path, strlen(file_path)));
    }
    print_debug(file_path);
    if (strlen(filename) > 0) {
        /* Max: 44-char prefix + 10-digit hash + ".png" + NUL = 59 B; fits in STR_MAX. */
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

    char jsonContent[JSON_RECENTS_LINE_MAX];
    int validGameCount = -1;
    int lineCount = 0;

    while (fgets(jsonContent, sizeof(jsonContent), file) != NULL) {
        char label[256] = {'\0'};
        char rompath[256] = {'\0'};
        char imgpath[256] = {'\0'};
        char launch[256] = {'\0'};
        lineCount++;

        const char *typePtr = strstr(jsonContent, "\"type\":");
        if (typePtr == NULL)
            continue;
        sscanf(typePtr + 7, "%d", &type);

        if ((type != 5) && (type != 17))
            continue;

        const char *labelStart = strstr(jsonContent, "\"label\":\"");
        if (labelStart != NULL) {
            labelStart += 9;
            const char *labelEnd = strchr(labelStart, '\"');
            if (labelEnd != NULL) {
                size_t labelLength = (size_t)(labelEnd - labelStart);
                if (labelLength >= sizeof(label)) labelLength = sizeof(label) - 1;
                strncpy(label, labelStart, labelLength);
                label[labelLength] = '\0';
            }
        }
        printf_debug("label: %s\n", label);
        const char *rompathStart = strstr(jsonContent, "\"rompath\":\"");
        if (rompathStart != NULL) {
            rompathStart += 11;
            const char *rompathEnd = strchr(rompathStart, '\"');
            if (rompathEnd != NULL) {
                size_t rompathLength = (size_t)(rompathEnd - rompathStart);
                if (rompathLength >= sizeof(rompath)) rompathLength = sizeof(rompath) - 1;
                strncpy(rompath, rompathStart, rompathLength);
                rompath[rompathLength] = '\0';
            }
        }
        printf_debug("rompath: %s\n", rompath);
        const char *imgpathStart = strstr(jsonContent, "\"imgpath\":\"");
        if (imgpathStart != NULL) {
            imgpathStart += 11;
            const char *imgpathEnd = strchr(imgpathStart, '\"');
            if (imgpathEnd != NULL) {
                size_t imgpathLength = (size_t)(imgpathEnd - imgpathStart);
                if (imgpathLength >= sizeof(imgpath)) imgpathLength = sizeof(imgpath) - 1;
                strncpy(imgpath, imgpathStart, imgpathLength);
                imgpath[imgpathLength] = '\0';
            }
        }

        char *colonPosition = strchr(rompath, ':');
        if (colonPosition != NULL) {

            int position = (int)(colonPosition - rompath);

            char firstPart[256];
            strncpy(firstPart, rompath, position);
            firstPart[position] = '\0';

            size_t suffixLen = strlen(colonPosition + 1);
            char secondPart[256];
            if (suffixLen >= sizeof(secondPart))
                suffixLen = sizeof(secondPart) - 1;
            memmove(secondPart, colonPosition + 1, suffixLen);
            secondPart[suffixLen] = '\0';

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
                    size_t launchLength = (size_t)(launchEnd - launchStart);
                    if (launchLength >= sizeof(launch)) launchLength = sizeof(launch) - 1;
                    strncpy(launch, launchStart, launchLength);
                    launch[launchLength] = '\0';
                }
            }
        }

        if (!exists(rompath) || !exists(launch))
            continue;

        ++validGameCount;

        if (validGameCount == index) {

            FILE *fp;
            char LaunchCommand[STR_MAX * 2 + 64];

            fclose(file);
            file = NULL;
            snprintf(LaunchCommand, sizeof(LaunchCommand), "LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so \"%s\" \"%s\"", launch, rompath);

            remove("/mnt/SDCARD/.tmp_update/.runGameSwitcher");

            // move selected rom to top of recent list for quick switch
            if (lineCount > 1) {
                temp_flag_set("quick_switch", true);

                char *line_n = file_read_lineN(recentPath, lineCount);
                file_add_line_to_beginning(recentPath, line_n);
                file_delete_line(recentPath, lineCount + 1);
                free(line_n);
            }

            file_put_sync(fp, CMD_TO_RUN_PATH, "%s", LaunchCommand);
            printf_debug("resume game: %s\n", LaunchCommand);

            temp_flag_set("force_auto_load_state", true);

            sync();
            break;
        }
    }

    if (file != NULL) {
        fclose(file);
    }
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
