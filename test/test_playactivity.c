/**
 * @file test_playactivity.c
 * @brief Unit tests for src/playActivity/playActivityDB.h, the production
 *        header, on a real SQLite database in /tmp/<pid>.
 *
 * _get_active_rom_path() reads cmd_to_run.sh (it was tested on a copy that
 * took a string). The database functions (start, stop, stop_all, resume,
 * drop_open) ran in no test before: the SQLite stand-in failed every call.
 *
 * Build and run: make -f Makefile.unit test_playactivity
 */

#include "onion_test.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char dir[] = "/tmp/onion_test_pa_XXXXXX";
static char cmd_to_run[96], db_dir[96], db_file[128], rom[128];
#define CMD_TO_RUN cmd_to_run
#define PLAY_ACTIVITY_DB_DIR db_dir
#define PLAY_ACTIVITY_DB_NEW_FILE db_file

#include "utils/file.h"
#include "utils/log.h"
#include "utils/str.h"
#include "../src/playActivity/playActivityDB.h"

static void write_cmd(const char *line)
{
    FILE *fp = fopen(cmd_to_run, "w");
    fputs(line, fp);
    fclose(fp);
}

/* Rows of play_activity matching WHERE, -1 when the table cannot be read */
static int rows(const char *where)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char sql[256];
    int n = -1;
    if (sqlite3_open(db_file, &db) != SQLITE_OK)
        return -1;
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM play_activity WHERE %s;", where);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            n = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return n;
}

static void fresh_db(void)
{
    remove(db_file);
}

/* ---- _get_active_rom_path ---- */

TEST(get_active_rom_path_standard) {
    char rom_path[STR_MAX] = "";
    write_cmd("LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so \"/mnt/SDCARD/Emu/GBA/launch.sh\" \"/mnt/SDCARD/Roms/GBA/game.gba\"\n");
    ASSERT_TRUE(_get_active_rom_path(rom_path));
    ASSERT_STREQ(rom_path, "/mnt/SDCARD/Roms/GBA/game.gba");
}

TEST(get_active_rom_path_no_quotes) {
    char rom_path[STR_MAX] = "";
    write_cmd("no quotes here\n");
    ASSERT_FALSE(_get_active_rom_path(rom_path));
}

TEST(get_active_rom_path_empty) {
    char rom_path[STR_MAX] = "";
    write_cmd("");
    ASSERT_FALSE(_get_active_rom_path(rom_path));
}

TEST(get_active_rom_path_no_file) {
    char rom_path[STR_MAX] = "";
    remove(cmd_to_run);
    ASSERT_FALSE(_get_active_rom_path(rom_path));
}

/* ---- the database ---- */

TEST(start_then_stop_records_one_session) {
    fresh_db();
    play_activity_start(rom);
    ASSERT_EQ(rows("play_time IS NULL"), 1);
    play_activity_stop(rom);
    ASSERT_EQ(rows("play_time IS NULL"), 0);
    ASSERT_EQ(rows("play_time >= 0"), 1);
}

/* An empty database file (a first write that failed) gets its tables. */
TEST(empty_database_file_gets_tables) {
    fresh_db();
    fclose(fopen(db_file, "w"));
    play_activity_start(rom);
    ASSERT_EQ(rows("1"), 1);
    play_activity_stop_all();
}

/* Standby: stop_all closes the session, resume adds a row for the same play. */
TEST(stop_all_then_resume_continues_the_play) {
    char line[STR_MAX * 2];
    fresh_db();
    snprintf(line, sizeof(line), "LD_PRELOAD=x \"/mnt/SDCARD/Emu/GBA/launch.sh\" \"%s\"\n", rom);
    write_cmd(line);
    play_activity_start(rom);
    play_activity_stop_all();
    ASSERT_EQ(rows("play_time IS NULL"), 0);
    play_activity_resume();
    ASSERT_EQ(rows("play_time IS NULL AND resumed = 1"), 1);
    play_activity_stop(rom);
    ASSERT_EQ(rows("1"), 2);
    ASSERT_EQ(rows("resumed = 1"), 1);
}

/* Boot after a power loss: an open session is dropped, closed ones stay. */
TEST(drop_open_keeps_closed_sessions) {
    fresh_db();
    play_activity_start(rom);
    play_activity_stop(rom);
    play_activity_start(rom);
    play_activity_drop_open();
    ASSERT_EQ(rows("play_time IS NULL"), 0);
    ASSERT_EQ(rows("1"), 1);
}

int main(void)
{
    printf("\n=== playActivityDB.h Unit Tests ===\n\n");

    if (mkdtemp(dir) == NULL)
        return 1;
    snprintf(cmd_to_run, sizeof(cmd_to_run), "%s/cmd_to_run.sh", dir);
    snprintf(db_dir, sizeof(db_dir), "%s/play_activity/", dir);
    snprintf(db_file, sizeof(db_file), "%splay_activity_db.sqlite", db_dir);
    snprintf(rom, sizeof(rom), "%s/Tetris.gb", dir);
    fclose(fopen(rom, "w"));

    RUN_TEST(get_active_rom_path_standard);
    RUN_TEST(get_active_rom_path_no_quotes);
    RUN_TEST(get_active_rom_path_empty);
    RUN_TEST(get_active_rom_path_no_file);

    RUN_TEST(start_then_stop_records_one_session);
    RUN_TEST(empty_database_file_gets_tables);
    RUN_TEST(stop_all_then_resume_continues_the_play);
    RUN_TEST(drop_open_keeps_closed_sessions);

    file_remove_recursive(dir);
    TEST_REPORT();
    return test_failures;
}
