/**
 * @file test_playactivity_close.c
 * @brief Closing play sessions keeps the stored play history
 *
 * Runs the SQL from playActivitySql.h on an in-memory SQLite database (the
 * repository's amalgamation): only open sessions are checked for an
 * impossible length; stored rows, such as totals imported from the old
 * format, survive whatever their length.
 *
 * Build and run: make -f Makefile.unit test_playactivity_close
 */

#include "onion_test.h"
#include <sqlite3/sqlite3.h>
#include <stdio.h>

#include "../src/playActivity/playActivitySql.h"

static sqlite3 *db;

static void setup(void)
{
    sqlite3_open(":memory:", &db);
    sqlite3_exec(db,
                 "CREATE TABLE play_activity(rom_id INTEGER, play_time INTEGER, "
                 "created_at INTEGER DEFAULT (strftime('%s', 'now')), updated_at INTEGER);"
                 /* 1: total imported from the old format, 100 hours */
                 "INSERT INTO play_activity VALUES(1, 360000, 0, 0);"
                 /* 2: open session started 10 minutes ago */
                 "INSERT INTO play_activity(rom_id, created_at) VALUES(2, strftime('%s', 'now') - 600);"
                 /* 3: open session started over a day ago: clock jump */
                 "INSERT INTO play_activity(rom_id, created_at) VALUES(3, strftime('%s', 'now') - 90000);"
                 /* 4: open session starting in the future: clock went back */
                 "INSERT INTO play_activity(rom_id, created_at) VALUES(4, strftime('%s', 'now') + 600);"
                 /* 5: stored row with a negative play time */
                 "INSERT INTO play_activity VALUES(5, -5, 1000, 1000);"
                 /* 6: stored session of 30 hours */
                 "INSERT INTO play_activity VALUES(6, 108000, 1000, 1000);",
                 NULL, NULL, NULL);
}

static int count(const char *where)
{
    char sql[256];
    sqlite3_stmt *stmt;
    int n = -1;
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM play_activity WHERE %s;", where);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            n = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return n;
}

static int play_time(int rom_id)
{
    char where[64];
    sqlite3_stmt *stmt;
    char sql[128];
    int t = -1;
    snprintf(where, sizeof(where), "rom_id = %d AND play_time IS NOT NULL", rom_id);
    snprintf(sql, sizeof(sql), "SELECT play_time FROM play_activity WHERE %s;", where);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            t = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return t;
}

static void close_rom(int rom_id)
{
    char *sql = sqlite3_mprintf(PLAY_ACTIVITY_CLOSE_ROM_FMT, rom_id, rom_id, rom_id);
    sqlite3_exec(db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
}

TEST(close_all_keeps_imported_total) {
    setup();
    sqlite3_exec(db, PLAY_ACTIVITY_CLOSE_ALL_SQL, NULL, NULL, NULL);
    ASSERT_EQ(play_time(1), 360000);
    sqlite3_close(db);
}

TEST(close_all_keeps_long_stored_session) {
    setup();
    sqlite3_exec(db, PLAY_ACTIVITY_CLOSE_ALL_SQL, NULL, NULL, NULL);
    ASSERT_EQ(play_time(6), 108000);
    sqlite3_close(db);
}

TEST(close_all_closes_normal_session) {
    setup();
    sqlite3_exec(db, PLAY_ACTIVITY_CLOSE_ALL_SQL, NULL, NULL, NULL);
    int t = play_time(2);
    ASSERT_TRUE(t >= 600 && t < 700);
    sqlite3_close(db);
}

TEST(close_all_discards_clock_jumps) {
    setup();
    sqlite3_exec(db, PLAY_ACTIVITY_CLOSE_ALL_SQL, NULL, NULL, NULL);
    ASSERT_EQ(count("rom_id IN (3, 4)"), 0);
    sqlite3_close(db);
}

TEST(close_all_removes_negative_rows) {
    setup();
    sqlite3_exec(db, PLAY_ACTIVITY_CLOSE_ALL_SQL, NULL, NULL, NULL);
    ASSERT_EQ(count("rom_id = 5"), 0);
    ASSERT_EQ(count("play_time IS NULL"), 0);
    sqlite3_close(db);
}

TEST(close_rom_keeps_imported_total) {
    setup();
    close_rom(1);
    ASSERT_EQ(play_time(1), 360000);
    sqlite3_close(db);
}

TEST(close_rom_only_touches_that_game) {
    setup();
    close_rom(3);
    ASSERT_EQ(count("rom_id = 3"), 0);
    ASSERT_EQ(count("rom_id = 2 AND play_time IS NULL"), 1);
    ASSERT_EQ(count("rom_id = 4 AND play_time IS NULL"), 1);
    ASSERT_EQ(play_time(1), 360000);
    sqlite3_close(db);
}

/* Issue #340: one play of an hour, paused 3 times in it (GameSwitcher in
 * game, sleep), is 1 play with the hour's play time, not 4 plays. */
static int query_int(const char *sql)
{
    sqlite3_stmt *stmt;
    int n = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            n = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    return n;
}

TEST(resumes_add_time_not_plays)
{
    sqlite3_open(":memory:", &db);
    /* an existing database from before the column */
    sqlite3_exec(db,
                 "CREATE TABLE play_activity(rom_id INTEGER, play_time INTEGER, "
                 "created_at INTEGER DEFAULT (strftime('%s', 'now')), updated_at INTEGER);"
                 "INSERT INTO play_activity VALUES(7, 600, 1000, 1600);", /* an older play */
                 NULL, NULL, NULL);
    ASSERT_EQ(sqlite3_exec(db, PLAY_ACTIVITY_ADD_RESUMED_SQL, NULL, NULL, NULL), SQLITE_OK);
    /* run again on every open: fails, changes nothing */
    ASSERT_TRUE(sqlite3_exec(db, PLAY_ACTIVITY_ADD_RESUMED_SQL, NULL, NULL, NULL) != SQLITE_OK);

    /* start, then 3 resumes, each segment 15 minutes */
    sqlite3_exec(db, "INSERT INTO play_activity(rom_id, play_time) VALUES(7, 900);", NULL, NULL, NULL);
    for (int i = 0; i < 3; i++) {
        char *sql = sqlite3_mprintf(PLAY_ACTIVITY_RESUME_FMT, 7);
        sqlite3_exec(db, sql, NULL, NULL, NULL);
        sqlite3_free(sql);
        sqlite3_exec(db, "UPDATE play_activity SET play_time = 900 WHERE play_time IS NULL;", NULL, NULL, NULL);
    }

    ASSERT_EQ(query_int("SELECT " PLAY_ACTIVITY_PLAYS_SQL " FROM play_activity WHERE rom_id = 7;"), 2);
    ASSERT_EQ(query_int("SELECT SUM(play_time) FROM play_activity WHERE rom_id = 7;"), 4200);
    ASSERT_EQ(query_int("SELECT SUM(play_activity.play_time)/" PLAY_ACTIVITY_PLAYS_SQL
                        " FROM play_activity WHERE rom_id = 7;"),
              2100);

    /* only resumed rows left (the start dropped for a clock jump): 1 play */
    sqlite3_exec(db, "DELETE FROM play_activity WHERE IFNULL(resumed, 0) = 0;", NULL, NULL, NULL);
    ASSERT_EQ(query_int("SELECT " PLAY_ACTIVITY_PLAYS_SQL " FROM play_activity WHERE rom_id = 7;"), 1);
    sqlite3_close(db);
}

/* At boot: an open session (cut by a power loss) is dropped, whatever the
 * clock says; every closed row stays. */
TEST(drop_open_at_boot_keeps_closed_rows) {
    setup();
    sqlite3_exec(db, PLAY_ACTIVITY_DROP_OPEN_SQL, NULL, NULL, NULL);
    ASSERT_EQ(count("play_time IS NULL"), 0);
    ASSERT_EQ(count("rom_id IN (2, 3, 4)"), 0);
    ASSERT_EQ(play_time(1), 360000);
    ASSERT_EQ(play_time(6), 108000);
    ASSERT_EQ(count("rom_id = 5"), 1); /* removed only by a close, as before */
    sqlite3_close(db);
}

int main(void)
{
    RUN_TEST(close_all_keeps_imported_total);
    RUN_TEST(close_all_keeps_long_stored_session);
    RUN_TEST(close_all_closes_normal_session);
    RUN_TEST(close_all_discards_clock_jumps);
    RUN_TEST(close_all_removes_negative_rows);
    RUN_TEST(close_rom_keeps_imported_total);
    RUN_TEST(close_rom_only_touches_that_game);
    RUN_TEST(resumes_add_time_not_plays);
    RUN_TEST(drop_open_at_boot_keeps_closed_rows);

    TEST_REPORT();
    return test_failures;
}
