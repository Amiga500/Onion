/**
 * @file test_batmon_start_time.c
 * @brief batmon's log_new_percentage(), cut from batmon.c by the Makefile and
 * run with stand-ins for sqlite and the clock.
 *
 * A battery log entry gets start_time, the clock when it begins. Battery
 * Monitor needs it for the length of a sleep, the drain rate while asleep
 * and charge times on the clock. batmonDB.h added the column, but the INSERT
 * did not fill it.
 *
 * Build and run: make -f Makefile.unit test_batmon_start_time
 */

#include "onion_test.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ---- stand-ins ---- */

typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
#define SQLITE_OK 0
#define SQLITE_ROW 100
#define FILO_MIN_SIZE 1000
#define DEVICE_SN "TESTSERIAL"

static int fake_db_token;
static sqlite3 *bat_log_db = NULL;
static bool db_opens = true;
static int db_closes = 0;

static int open_battery_log_db(void)
{
    if (!db_opens)
        return -1;
    bat_log_db = (sqlite3 *)&fake_db_token;
    return 1;
}

static void close_battery_log_db(void)
{
    bat_log_db = NULL;
    db_closes++;
}

static long long fake_now = 0;
#define time(ptr) (fake_now)

/* What the INSERT was built from. */
static char insert_format[256];
static char insert_serial[32];
static int insert_level, insert_duration, insert_charging;
static long long insert_start_time;
static bool insert_has_start_time;
static int inserts_executed = 0;
static char sql_buffer[8];

static char *sqlite3_mprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    snprintf(insert_format, sizeof(insert_format), "%s", format);
    snprintf(insert_serial, sizeof(insert_serial), "%s", va_arg(args, const char *));
    insert_level = va_arg(args, int);
    insert_duration = va_arg(args, int);
    insert_charging = va_arg(args, int);
    insert_has_start_time = strstr(format, "start_time") != NULL;
    insert_start_time = insert_has_start_time ? va_arg(args, long long) : -1;
    va_end(args);
    return sql_buffer;
}

static int sqlite3_exec(sqlite3 *db, const char *sql, void *callback, void *arg, char **errmsg)
{
    (void)db;
    (void)callback;
    (void)arg;
    (void)errmsg;
    if (sql == sql_buffer)
        inserts_executed++;
    return SQLITE_OK;
}

static void sqlite3_free(void *ptr) { (void)ptr; }

static int rows_in_log = 10;
static int deletes = 0;
static bool last_prepared_is_delete = false;

static int sqlite3_prepare_v2(sqlite3 *db, const char *sql, int len, sqlite3_stmt **stmt, const char **tail)
{
    (void)db;
    (void)len;
    (void)stmt;
    (void)tail;
    last_prepared_is_delete = strncmp(sql, "DELETE", 6) == 0;
    return SQLITE_OK;
}

static int sqlite3_step(sqlite3_stmt *stmt)
{
    (void)stmt;
    if (last_prepared_is_delete)
        deletes++;
    return SQLITE_ROW;
}

static int sqlite3_column_int(sqlite3_stmt *stmt, int column)
{
    (void)stmt;
    (void)column;
    return rows_in_log;
}

static int sqlite3_finalize(sqlite3_stmt *stmt)
{
    (void)stmt;
    return SQLITE_OK;
}

#include BATMON_FNS

static void reset(long long now)
{
    fake_now = now;
    db_opens = true;
    db_closes = 0;
    inserts_executed = 0;
    insert_format[0] = '\0';
    insert_has_start_time = false;
    insert_start_time = -1;
    rows_in_log = 10;
    deletes = 0;
}

/* ---- tests ---- */

TEST(entry_gets_the_clock_as_start_time)
{
    reset(1794758400LL); /* 2026-11-15 */
    log_new_percentage(73, 0);
    ASSERT_EQ(inserts_executed, 1);
    ASSERT_TRUE(insert_has_start_time);
    ASSERT_TRUE(insert_start_time == 1794758400LL);
    /* the column is named: older logs get it appended last (batmonDB.h) */
    ASSERT_TRUE(strstr(insert_format, "is_charging, start_time) VALUES(%Q, %d, %d, %d, %lld)") != NULL);
    ASSERT_STREQ(insert_serial, DEVICE_SN);
    ASSERT_EQ(insert_level, 73);
    ASSERT_EQ(insert_duration, 0);
    ASSERT_EQ(insert_charging, 0);
    ASSERT_EQ(db_closes, 1);
}

TEST(charging_entry_gets_it_too)
{
    reset(1794758400LL);
    log_new_percentage(500, 1);
    ASSERT_TRUE(insert_has_start_time);
    ASSERT_EQ(insert_level, 500);
    ASSERT_EQ(insert_charging, 1);
}

TEST(unset_clock_leaves_start_time_null)
{
    /* no RTC and nothing restored yet: the clock starts at 1970 */
    reset(42);
    log_new_percentage(73, 0);
    ASSERT_EQ(inserts_executed, 1);
    ASSERT_FALSE(insert_has_start_time);
    ASSERT_EQ(insert_level, 73);

    reset(BATTERY_LOG_MIN_TIME - 1);
    log_new_percentage(73, 0);
    ASSERT_FALSE(insert_has_start_time);
}

TEST(first_valid_second_is_written)
{
    reset(BATTERY_LOG_MIN_TIME);
    log_new_percentage(73, 0);
    ASSERT_TRUE(insert_has_start_time);
    ASSERT_TRUE(insert_start_time == BATTERY_LOG_MIN_TIME);
}

TEST(nothing_is_written_when_the_log_cannot_be_opened)
{
    reset(1794758400LL);
    db_opens = false;
    log_new_percentage(73, 0);
    ASSERT_EQ(inserts_executed, 0);
    ASSERT_EQ(db_closes, 0);
}

TEST(oldest_entry_still_goes_when_the_log_is_full)
{
    reset(1794758400LL);
    rows_in_log = FILO_MIN_SIZE + 1;
    log_new_percentage(73, 0);
    ASSERT_EQ(inserts_executed, 1);
    ASSERT_EQ(deletes, 1);

    reset(1794758400LL);
    rows_in_log = FILO_MIN_SIZE;
    log_new_percentage(73, 0);
    ASSERT_EQ(deletes, 0);
}

int main(void)
{
    printf("\n=== batmon start_time Unit Tests ===\n\n");
    RUN_TEST(entry_gets_the_clock_as_start_time);
    RUN_TEST(charging_entry_gets_it_too);
    RUN_TEST(unset_clock_leaves_start_time_null);
    RUN_TEST(first_valid_second_is_written);
    RUN_TEST(nothing_is_written_when_the_log_cannot_be_opened);
    RUN_TEST(oldest_entry_still_goes_when_the_log_is_full);
    TEST_REPORT();
    return test_failures;
}
