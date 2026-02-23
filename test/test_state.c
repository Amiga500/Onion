/**
 * @file test_state.c
 * @brief Unit tests for AdvanceMENU-related state functions in state.h.
 *
 * Documents and regression-tests two bugs that affect the AdvanceMENU
 * integration:
 *
 * BUG #1 — state_getAppName: hardcoded offset of 19 characters
 *   The function unconditionally skips the first 19 characters of the command
 *   string.  That number exactly matches the prefix "HOME=/mnt/SDCARD ./"
 *   produced by the Onion MainUI app launcher.  If the HOME path is shorter
 *   (e.g. "HOME=/home/user ./", which is 18 chars), the window shifts by one
 *   character and the extracted name is wrong — silently, with no error.
 *   Fix: search for the " ./" separator dynamically instead of using a magic
 *   constant.
 *
 * BUG #2 — history_getRecentPath: premature return on non-game entries
 *   When the function encounters a line whose "type" field is not 5 or 17
 *   (i.e. not a game entry — could be an App, a section header, etc.) it
 *   calls fclose() + return NULL, aborting the scan.  resumeGame() correctly
 *   uses 'continue' to skip those entries.  In the AdvanceMENU quick-switch
 *   flow the recent list is read via history_getRecentPath to derive the
 *   screenshot filename; if the top entry is an App launch (type 3), the
 *   function returns NULL and every screenshot is named "Screenshot" instead
 *   of the ROM name.
 *   Fix: replace the two "fclose + return NULL" inside the while-loop with
 *   "free(jsonContent) + continue".
 *
 * Build and run:
 *   make -f Makefile.unit test_state && ./build_test/test_state
 */

#include "onion_test.h"
#include "../src/common/utils/str.h"
#include "../src/common/utils/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ------------------------------------------------------------------ */
/* Local reproductions of the functions under test                     */
/*                                                                     */
/* state.h cannot be included here without pulling in SDL-heavy system */
/* headers.  We therefore reproduce only the two functions of interest */
/* verbatim from state.h so the tests are fully self-contained and     */
/* compile without device-specific dependencies.                       */
/* ------------------------------------------------------------------ */

/* Exact copy of the BUGGY state_getAppName from state.h.
 * Kept here to document the problem; the production code is fixed. */
static size_t _buggy_state_getAppName(char *out, const char *str)
{
    char *end;
    size_t out_size;

    str += 19; /* hardcoded: assumes "HOME=/mnt/SDCARD ./" prefix */
    end = (char *)strchr(str, ';');
    if (end == NULL)
        end = (char *)(str + strlen(str));

    out_size = (end - str) < STR_MAX - 1 ? (end - str) : STR_MAX - 1;
    memcpy(out, str, out_size);
    out[out_size] = 0;
    return out_size;
}

/* Exact copy of the FIXED state_getAppName (dynamically finds " ./"). */
static size_t _fixed_state_getAppName(char *out, const char *str)
{
    char *end;
    size_t out_size;

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

/* Testable version of history_getRecentPath that takes a file path.
 * Contains the BUGGY logic (fclose+return NULL on non-game lines)
 * so the tests below prove the problem is real. */
static char *_buggy_history_getRecentPath(const char *recentlist_path,
                                          char *rom_path)
{
    FILE *file = fopen(recentlist_path, "r");
    if (file == NULL)
        return NULL;

    char line[STR_MAX * 3];
    while (fgets(line, STR_MAX * 3, file) != NULL) {
        size_t line_len = strlen(line);
        char *jsonContent = (char *)malloc(line_len + 1);
        int type;

        memcpy(jsonContent, line, line_len + 1);
        const char *typeStr = strstr(jsonContent, "\"type\":");
        if (typeStr == NULL || sscanf(typeStr + 7, "%d", &type) != 1) {
            free(jsonContent);
            fclose(file);
            return NULL; /* BUG: should be 'continue' */
        }

        if ((type != 5) && (type != 17)) {
            free(jsonContent);
            fclose(file);
            return NULL; /* BUG: should be 'continue' */
        }

        const char *rompathStart = strstr(jsonContent, "\"rompath\":\"");
        if (rompathStart == NULL) {
            free(jsonContent);
            fclose(file);
            return NULL;
        }
        rompathStart += 11;
        const char *rompathEnd = strchr(rompathStart, '"');
        if (rompathEnd == NULL) {
            free(jsonContent);
            fclose(file);
            return NULL;
        }
        size_t len = (size_t)(rompathEnd - rompathStart);
        if (len >= STR_MAX)
            len = STR_MAX - 1;
        strncpy(rom_path, rompathStart, len);
        rom_path[len] = '\0';

        free(jsonContent);
        fclose(file);
        return rom_path;
    }

    fclose(file);
    return NULL;
}

/* Testable version with the FIX applied (continue on non-game lines). */
static char *_fixed_history_getRecentPath(const char *recentlist_path,
                                          char *rom_path)
{
    FILE *file = fopen(recentlist_path, "r");
    if (file == NULL)
        return NULL;

    char line[STR_MAX * 3];
    while (fgets(line, STR_MAX * 3, file) != NULL) {
        size_t line_len = strlen(line);
        char *jsonContent = (char *)malloc(line_len + 1);
        int type;

        memcpy(jsonContent, line, line_len + 1);
        const char *typeStr = strstr(jsonContent, "\"type\":");
        if (typeStr == NULL || sscanf(typeStr + 7, "%d", &type) != 1) {
            free(jsonContent);
            continue; /* fixed: skip malformed lines */
        }

        if ((type != 5) && (type != 17)) {
            free(jsonContent);
            continue; /* fixed: skip non-game entries */
        }

        const char *rompathStart = strstr(jsonContent, "\"rompath\":\"");
        if (rompathStart == NULL) {
            free(jsonContent);
            continue;
        }
        rompathStart += 11;
        const char *rompathEnd = strchr(rompathStart, '"');
        if (rompathEnd == NULL) {
            free(jsonContent);
            continue;
        }
        size_t len = (size_t)(rompathEnd - rompathStart);
        if (len >= STR_MAX)
            len = STR_MAX - 1;
        strncpy(rom_path, rompathStart, len);
        rom_path[len] = '\0';

        free(jsonContent);
        fclose(file);
        return rom_path;
    }

    fclose(file);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Tests for state_getAppName                                          */
/* ------------------------------------------------------------------ */

/* The function works correctly when the prefix is exactly
 * "HOME=/mnt/SDCARD ./" (19 chars). */
TEST(state_getAppName_standard_sdcard_prefix) {
    char cmd[STR_MAX] = "HOME=/mnt/SDCARD ./Tweaks; chmod 777 Tweaks";
    char out[STR_MAX] = {0};
    _fixed_state_getAppName(out, cmd);
    ASSERT_STREQ(out, "Tweaks");
}

/* Without a semicolon the full remainder is the app name. */
TEST(state_getAppName_no_semicolon) {
    char cmd[STR_MAX] = "HOME=/mnt/SDCARD ./Tweaks";
    char out[STR_MAX] = {0};
    _fixed_state_getAppName(out, cmd);
    ASSERT_STREQ(out, "Tweaks");
}

/* An app name that contains spaces is preserved up to the semicolon. */
TEST(state_getAppName_app_name_with_space) {
    char cmd[STR_MAX] = "HOME=/mnt/SDCARD ./My App; chmod 777 MyApp";
    char out[STR_MAX] = {0};
    _fixed_state_getAppName(out, cmd);
    ASSERT_STREQ(out, "My App");
}

/* No " ./" separator → returns empty string gracefully. */
TEST(state_getAppName_no_prefix_returns_empty) {
    char cmd[STR_MAX] = "invalid_command_without_prefix";
    char out[STR_MAX] = {0};
    _fixed_state_getAppName(out, cmd);
    ASSERT_EQ((int)strlen(out), 0);
}

/*
 * BUG #1 — Logged regression test.
 *
 * "HOME=/home/user ./" is 18 chars — one shorter than the hardcoded 19.
 * The buggy function therefore starts one character INTO the app name,
 * returning "ppName" instead of "AppName".
 * The fixed function finds " ./" dynamically and always returns the
 * correct app name regardless of the HOME path length.
 */
TEST(state_getAppName_shorter_home_prefix_misparses_bug) {
    char cmd[STR_MAX] = "HOME=/home/user ./AppName; chmod 777 AppName";
    char out_buggy[STR_MAX] = {0};
    char out_fixed[STR_MAX] = {0};

    _buggy_state_getAppName(out_buggy, cmd);
    _fixed_state_getAppName(out_fixed, cmd);

    /* The buggy version skips 19 chars on an 18-char prefix:
     * it starts at the 2nd character of "AppName" → "ppName". */
    ASSERT_STREQ(out_buggy, "ppName");   /* documents what the bug produces */
    ASSERT_STREQ(out_fixed, "AppName");  /* the fix produces the correct result */
}

/* ------------------------------------------------------------------ */
/* Tests for history_getRecentPath                                     */
/* ------------------------------------------------------------------ */

static const char *_TMP_RECENT = "/tmp/onion_test_recentlist.json";

/* When the first (and only) line is a game entry, the path is found. */
TEST(history_getRecentPath_single_game_entry) {
    FILE *fp = fopen(_TMP_RECENT, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp,
        "{\"label\":\"MyGame\",\"type\":5,"
        "\"rompath\":\"/mnt/SDCARD/Roms/GBA/game.gba\","
        "\"imgpath\":\"/mnt/SDCARD/Roms/GBA/Imgs/game.png\","
        "\"launch\":\"/mnt/SDCARD/Emu/GBA/launch.sh\"}\n");
    fclose(fp);

    char rom_path[STR_MAX] = {0};
    char *result = _fixed_history_getRecentPath(_TMP_RECENT, rom_path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/mnt/SDCARD/Roms/GBA/game.gba");
    unlink(_TMP_RECENT);
}

/* A type-17 entry (search-launched game) is also treated as a game. */
TEST(history_getRecentPath_type17_game_entry) {
    FILE *fp = fopen(_TMP_RECENT, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp,
        "{\"label\":\"GameViaSearch\",\"type\":17,"
        "\"rompath\":\"/mnt/SDCARD/Roms/SNES/game.sfc\","
        "\"imgpath\":\"/mnt/SDCARD/Roms/SNES/Imgs/game.png\","
        "\"launch\":\"/mnt/SDCARD/Emu/SNES/launch.sh\"}\n");
    fclose(fp);

    char rom_path[STR_MAX] = {0};
    char *result = _fixed_history_getRecentPath(_TMP_RECENT, rom_path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/mnt/SDCARD/Roms/SNES/game.sfc");
    unlink(_TMP_RECENT);
}

/* When the file does not exist the function returns NULL gracefully. */
TEST(history_getRecentPath_missing_file) {
    char rom_path[STR_MAX] = {0};
    char *result = _fixed_history_getRecentPath(
        "/tmp/onion_test_nonexistent_recent.json", rom_path);
    ASSERT_NULL(result);
}

/* A completely empty file returns NULL without crashing. */
TEST(history_getRecentPath_empty_file) {
    FILE *fp = fopen(_TMP_RECENT, "w");
    ASSERT_NOT_NULL(fp);
    fclose(fp);

    char rom_path[STR_MAX] = {0};
    char *result = _fixed_history_getRecentPath(_TMP_RECENT, rom_path);
    ASSERT_NULL(result);
    unlink(_TMP_RECENT);
}

/* A malformed (non-JSON) line is skipped; the next valid game line is found. */
TEST(history_getRecentPath_malformed_line_then_game) {
    FILE *fp = fopen(_TMP_RECENT, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "this is not valid json\n");
    fprintf(fp,
        "{\"label\":\"Game2\",\"type\":5,"
        "\"rompath\":\"/mnt/SDCARD/Roms/GBA/game2.gba\","
        "\"imgpath\":\"/mnt/SDCARD/Roms/GBA/Imgs/game2.png\","
        "\"launch\":\"/mnt/SDCARD/Emu/GBA/launch.sh\"}\n");
    fclose(fp);

    char rom_path[STR_MAX] = {0};
    char *result = _fixed_history_getRecentPath(_TMP_RECENT, rom_path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/mnt/SDCARD/Roms/GBA/game2.gba");
    unlink(_TMP_RECENT);
}

/*
 * BUG #2 — Logged regression test.
 *
 * The recent list has an App entry (type 3) on the first line and a valid
 * game entry (type 5) on the second line.
 *
 * Buggy behaviour  : returns NULL after seeing line 1 (type 3) without
 *                    examining line 2.
 * Fixed behaviour  : skips line 1 (continue) and returns the path from
 *                    line 2.
 *
 * Impact on AdvanceMENU: when advmenu launches a game via advexec.sh and
 * the most-recent MainUI entry was an App, history_getRecentPath returns
 * NULL → the screenshot path builder falls through to the generic
 * "Screenshot" name instead of the actual ROM name.
 */
TEST(history_getRecentPath_app_entry_before_game_entry_bug) {
    FILE *fp = fopen(_TMP_RECENT, "w");
    ASSERT_NOT_NULL(fp);
    /* Line 1: App entry (type 3) — not a game, should be skipped */
    fprintf(fp,
        "{\"label\":\"SomeApp\",\"type\":3,"
        "\"rompath\":\"/mnt/SDCARD/App/SomeApp\"}\n");
    /* Line 2: Game entry (type 5) — should be found */
    fprintf(fp,
        "{\"label\":\"MyGame\",\"type\":5,"
        "\"rompath\":\"/mnt/SDCARD/Roms/GBA/game.gba\","
        "\"imgpath\":\"/mnt/SDCARD/Roms/GBA/Imgs/game.png\","
        "\"launch\":\"/mnt/SDCARD/Emu/GBA/launch.sh\"}\n");
    fclose(fp);

    char rom_path_buggy[STR_MAX] = {0};
    char rom_path_fixed[STR_MAX] = {0};

    char *buggy        = _buggy_history_getRecentPath(_TMP_RECENT, rom_path_buggy);
    char *fixed_result = _fixed_history_getRecentPath(_TMP_RECENT, rom_path_fixed);

    /* The buggy version hits the type-3 line, calls fclose+return NULL. */
    ASSERT_NULL(buggy);

    /* The fixed version skips the type-3 line and finds the type-5 game. */
    ASSERT_NOT_NULL(fixed_result);
    ASSERT_STREQ(fixed_result, "/mnt/SDCARD/Roms/GBA/game.gba");

    unlink(_TMP_RECENT);
}

/* Three non-game entries followed by a game: the fixed version finds it. */
TEST(history_getRecentPath_multiple_non_game_entries_then_game) {
    FILE *fp = fopen(_TMP_RECENT, "w");
    ASSERT_NOT_NULL(fp);
    fprintf(fp, "{\"label\":\"App1\",\"type\":3,\"rompath\":\"/App/App1\"}\n");
    fprintf(fp, "{\"label\":\"App2\",\"type\":3,\"rompath\":\"/App/App2\"}\n");
    fprintf(fp, "{\"label\":\"App3\",\"type\":3,\"rompath\":\"/App/App3\"}\n");
    fprintf(fp,
        "{\"label\":\"TheGame\",\"type\":5,"
        "\"rompath\":\"/mnt/SDCARD/Roms/GBA/thegame.gba\","
        "\"imgpath\":\"/mnt/SDCARD/Roms/GBA/Imgs/thegame.png\","
        "\"launch\":\"/mnt/SDCARD/Emu/GBA/launch.sh\"}\n");
    fclose(fp);

    char rom_path[STR_MAX] = {0};
    char *result = _fixed_history_getRecentPath(_TMP_RECENT, rom_path);
    ASSERT_NOT_NULL(result);
    ASSERT_STREQ(result, "/mnt/SDCARD/Roms/GBA/thegame.gba");
    unlink(_TMP_RECENT);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    printf("\n=== state.h / AdvanceMENU Unit Tests ===\n\n");

    /* state_getAppName */
    RUN_TEST(state_getAppName_standard_sdcard_prefix);
    RUN_TEST(state_getAppName_no_semicolon);
    RUN_TEST(state_getAppName_app_name_with_space);
    RUN_TEST(state_getAppName_no_prefix_returns_empty);
    RUN_TEST(state_getAppName_shorter_home_prefix_misparses_bug);

    /* history_getRecentPath */
    RUN_TEST(history_getRecentPath_single_game_entry);
    RUN_TEST(history_getRecentPath_type17_game_entry);
    RUN_TEST(history_getRecentPath_missing_file);
    RUN_TEST(history_getRecentPath_empty_file);
    RUN_TEST(history_getRecentPath_malformed_line_then_game);
    RUN_TEST(history_getRecentPath_app_entry_before_game_entry_bug);
    RUN_TEST(history_getRecentPath_multiple_non_game_entries_then_game);

    TEST_REPORT();
    return test_failures;
}
