#ifndef BATMON_DB_H
#define BATMON_DB_H
#define BATTERY_LOG_FILE "/mnt/SDCARD/Saves/Common/battery_logs/battery_logs.sqlite"
// start_time values before this (2020-01-01) mean an unset clock
#define BATTERY_LOG_MIN_TIME 1577836800LL

#include <time.h>

sqlite3 *bat_log_db = NULL;

// Tables are created with IF NOT EXISTS, once per process. They used to be
// made only when the file did not exist yet, so a first creation cut short
// (power off) left a file without tables, and the log never worked again.
static bool bat_log_schema_ready = false;

int open_battery_log_db(void)
{
    if (!is_file(BATTERY_LOG_FILE)) {
        mkdir("/mnt/SDCARD/Saves/Common", 0777);
        mkdir("/mnt/SDCARD/Saves/Common/battery_logs", 0777);
    }

    if (sqlite3_open(BATTERY_LOG_FILE, &bat_log_db) != SQLITE_OK) {
        printf("%s\n", sqlite3_errmsg(bat_log_db));
        sqlite3_close(bat_log_db);
        bat_log_db = NULL;
        return -1;
    }

    if (!bat_log_schema_ready &&
        sqlite3_exec(bat_log_db,
                     "CREATE TABLE IF NOT EXISTS bat_activity(id INTEGER PRIMARY KEY, device_serial TEXT, bat_level INTEGER, duration INTEGER, is_charging INTEGER);"
                     "CREATE INDEX IF NOT EXISTS bat_activity_device_SN_index ON bat_activity(device_serial);"
                     "CREATE TABLE IF NOT EXISTS device_specifics(id INTEGER PRIMARY KEY, device_serial TEXT, best_session INTEGER);"
                     "CREATE INDEX IF NOT EXISTS device_specifics_index ON device_specifics(device_serial);",
                     NULL, NULL, NULL) == SQLITE_OK)
        bat_log_schema_ready = true;

    // start_time (Unix time when the entry began, NULL when unknown) came later: add it to
    // older logs, once per process. Appended last, so readers that use SELECT * with the
    // first five columns, and writers that name their columns, keep working. The error
    // when the column already exists is expected and ignored.
    static bool start_time_checked = false;
    if (!start_time_checked) {
        sqlite3_exec(bat_log_db, "ALTER TABLE bat_activity ADD COLUMN start_time INTEGER;", NULL, NULL, NULL);
        start_time_checked = true;
    }

    return 1;
}

void close_battery_log_db(void)
{
    sqlite3_close(bat_log_db);
    bat_log_db = NULL;
}

int get_best_session_time(void)
{
    int best_time = 0;
    if (open_battery_log_db() == 1) {
        if (bat_log_db != NULL) {
            const char *sql = "SELECT * FROM device_specifics WHERE device_serial = ? ORDER BY id LIMIT 1;";
            sqlite3_stmt *stmt;

            int rc = sqlite3_prepare_v2(bat_log_db, sql, -1, &stmt, 0);
            if (rc == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, DEVICE_SN, -1, SQLITE_STATIC);
                rc = sqlite3_step(stmt);
                if (rc == SQLITE_ROW) {
                    best_time = sqlite3_column_int(stmt, 2);
                }
                else {
                    char *sql2 = sqlite3_mprintf("INSERT INTO device_specifics(device_serial, best_session) VALUES(%Q, %d);", DEVICE_SN, 0);
                    sqlite3_exec(bat_log_db, sql2, NULL, NULL, NULL);
                    sqlite3_free(sql2);
                }
            }
            sqlite3_finalize(stmt);
        }
        close_battery_log_db();
    }
    return best_time;
}

#endif // BATMON_DB_H