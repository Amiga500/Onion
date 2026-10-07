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

int main(void)
{
    RUN_TEST(close_all_keeps_imported_total);
    RUN_TEST(close_all_keeps_long_stored_session);
    RUN_TEST(close_all_closes_normal_session);
    RUN_TEST(close_all_discards_clock_jumps);
    RUN_TEST(close_all_removes_negative_rows);
    RUN_TEST(close_rom_keeps_imported_total);
    RUN_TEST(close_rom_only_touches_that_game);

    TEST_REPORT();
    return test_failures;
}
