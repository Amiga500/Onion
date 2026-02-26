#ifndef PLAY_ACTIVITY_DB_H
#define PLAY_ACTIVITY_DB_H

#include <dirent.h>
#include <libgen.h>
#include <limits.h>
#include <sqlite3/sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "utils/file.h"
#include "utils/log.h"
#include "utils/str.h"

#include "./cacheDB.h"

#define PLAY_ACTIVITY_DB_NEW_FILE "/mnt/SDCARD/Saves/CurrentProfile/play_activity/play_activity_db.sqlite"
#define ROMS_FOLDER "/mnt/SDCARD/Roms"
#define CMD_TO_RUN "/mnt/SDCARD/.tmp_update/cmd_to_run.sh"
#define ROM_NOT_FOUND -1

typedef struct ROM ROM;
typedef struct PlayActivity PlayActivity;
typedef struct PlayActivities PlayActivities;

struct ROM {
    int id;
    char *type;
    char *name;
    char *file_path;
    char *image_path;
};
struct PlayActivity {
    ROM *rom;
    int play_count;
    int play_time_total;
    int play_time_average;
    char *first_played_at;
    char *last_played_at;
};
struct PlayActivities {
    PlayActivity **play_activity;
    int count;
    int play_time_total;
};

sqlite3 *play_activity_db = NULL;

void get_rom_image_path(char *rom_file, char *out_image_path)
{
    if (str_endsWith(rom_file, ".p8") || str_endsWith(rom_file, ".png")) {
        snprintf(out_image_path, STR_MAX - 1, "/mnt/SDCARD/Roms/%s", rom_file);
        return;
    }

    char *clean_rom_name = file_removeExtension(file_basename(rom_file));
    if (clean_rom_name == NULL)
        return;
    char rom_file_copy[PATH_MAX];
    strncpy(rom_file_copy, rom_file, sizeof(rom_file_copy) - 1);
    rom_file_copy[sizeof(rom_file_copy) - 1] = '\0';
    char *rom_folder = strtok(rom_file_copy, "/");
    if (rom_folder == NULL)
        rom_folder = rom_file_copy;

    snprintf(out_image_path, STR_MAX - 1, "/mnt/SDCARD/Roms/%s/Imgs/%s.png", rom_folder, clean_rom_name);
    free(clean_rom_name);
}

void play_activity_db_close()
{
    sqlite3_close(play_activity_db);
    play_activity_db = NULL;
}

void play_activity_db_open(void)
{
    if (play_activity_db != NULL)
        return;

    bool play_activity_db_created = is_file(PLAY_ACTIVITY_DB_NEW_FILE);

    mkdir("/mnt/SDCARD/Saves/CurrentProfile/play_activity/", 0777);

    if (sqlite3_open(PLAY_ACTIVITY_DB_NEW_FILE, &play_activity_db) != SQLITE_OK) {
        printf("%s\n", sqlite3_errmsg(play_activity_db));
        play_activity_db_close();
        return;
    }

    if (!play_activity_db_created) {
        sqlite3_exec(play_activity_db,
                     "DROP TABLE IF EXISTS rom;"
                     "CREATE TABLE rom(id INTEGER PRIMARY KEY, type TEXT, name TEXT, file_path TEXT, image_path TEXT, created_at INTEGER DEFAULT (strftime('%s', 'now')), updated_at INTEGER);"
                     "CREATE UNIQUE INDEX rom_id_index ON rom(id);",
                     NULL, NULL, NULL);
        sqlite3_exec(play_activity_db,
                     "DROP TABLE IF EXISTS play_activity;"
                     "CREATE TABLE play_activity(rom_id INTEGER, play_time INTEGER, created_at INTEGER DEFAULT (strftime('%s', 'now')), updated_at INTEGER);"
                     "CREATE INDEX play_activity_rom_id_index ON play_activity(rom_id);",
                     NULL, NULL, NULL);
    }
}

int play_activity_db_transaction(int (*exec_transaction)(void))
{
    int retval;
    play_activity_db_open();
    retval = exec_transaction();
    play_activity_db_close();
    return retval;
}

int play_activity_db_execute(char *sql)
{
    printf_debug("play_activity_db_execute(%s)\n", sql);
    play_activity_db_open();
    int rc = sqlite3_exec(play_activity_db, sql, NULL, NULL, NULL);
    play_activity_db_close();
    return rc;
}

sqlite3_stmt *play_activity_db_prepare(char *sql)
{
    printf_debug("play_activity_db_prepare(%s)\n", sql);
    if (play_activity_db == NULL) {
        printf("DB is not open");
        return NULL;
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(play_activity_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("%s: %s\n", sqlite3_errmsg(play_activity_db), sql);
    }
    return stmt;
}

int play_activity_get_total_play_time(void)
{
    int total_play_time = 0;
    char *sql =
        "SELECT SUM(play_time_total) FROM (SELECT SUM(play_time) AS play_time_total FROM play_activity GROUP BY rom_id) "
        "WHERE play_time_total > 60;";
    sqlite3_stmt *stmt;

    play_activity_db_open();
    stmt = play_activity_db_prepare(sql);

    if (stmt != NULL && sqlite3_step(stmt) == SQLITE_ROW) {
        total_play_time = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    play_activity_db_close();

    return total_play_time;
}

PlayActivities *play_activity_find_all(void)
{
    PlayActivities *play_activities = NULL;
    char *sql =
        "SELECT * FROM ("
        "    SELECT rom.id, rom.type, rom.name, rom.file_path, "
        "           COUNT(play_activity.ROWID) AS play_count_total, "
        "           SUM(play_activity.play_time) AS play_time_total, "
        "           SUM(play_activity.play_time)/COUNT(play_activity.ROWID) AS play_time_average, "
        "           datetime(MIN(play_activity.created_at), 'unixepoch') AS first_played_at, "
        "           datetime(MAX(play_activity.created_at), 'unixepoch') AS last_played_at "
        "    FROM rom LEFT JOIN play_activity ON rom.id = play_activity.rom_id "
        "    GROUP BY rom.id) "
        "WHERE play_time_total > 60 "
        "ORDER BY play_time_total DESC;";
    sqlite3_stmt *stmt;

    play_activity_db_open();

    stmt = play_activity_db_prepare(sql);

    int play_activity_count = 0;
    while (stmt != NULL && sqlite3_step(stmt) == SQLITE_ROW) {
        play_activity_count++;
    }
    if (stmt != NULL)
        sqlite3_reset(stmt);

    play_activities = (PlayActivities *)malloc(sizeof(PlayActivities));
    if (play_activities == NULL) {
        sqlite3_finalize(stmt);
        play_activity_db_close();
        return NULL;
    }
    play_activities->count = play_activity_count;
    play_activities->play_time_total = 0;
    play_activities->play_activity = (PlayActivity **)malloc(sizeof(PlayActivity *) * play_activities->count);
    if (play_activities->play_activity == NULL) {
        free(play_activities);
        sqlite3_finalize(stmt);
        play_activity_db_close();
        return NULL;
    }

    for (int i = 0; i < play_activities->count; i++) {
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            play_activities->count = i;
            break;
        }

        PlayActivity *entry = play_activities->play_activity[i] = (PlayActivity *)malloc(sizeof(PlayActivity));
        if (entry == NULL) {
            play_activities->count = i;
            break;
        }
        ROM *rom = play_activities->play_activity[i]->rom = (ROM *)malloc(sizeof(ROM));
        if (rom == NULL) {
            free(entry);
            play_activities->play_activity[i] = NULL;
            play_activities->count = i;
            break;
        }
        entry->first_played_at = NULL;
        entry->last_played_at = NULL;
        rom->file_path = NULL;
        rom->image_path = NULL;

        rom->id = sqlite3_column_int(stmt, 0);
        const char *col_type = (const char *)sqlite3_column_text(stmt, 1);
        rom->type = strdup(col_type != NULL ? col_type : "");
        if (rom->type == NULL)
            rom->type = strdup("");
        const char *col_name = (const char *)sqlite3_column_text(stmt, 2);
        rom->name = strdup(col_name != NULL ? col_name : "");
        if (rom->name == NULL)
            rom->name = strdup("");
        if (sqlite3_column_text(stmt, 3) != NULL) {
            rom->file_path = strdup((const char *)sqlite3_column_text(stmt, 3));
            if (rom->file_path != NULL) {
                rom->image_path = malloc(STR_MAX * sizeof(char));
                if (rom->image_path != NULL) {
                    memset(rom->image_path, 0, STR_MAX);
                    get_rom_image_path(rom->file_path, rom->image_path);
                }
            }
        }

        entry->play_count = sqlite3_column_int(stmt, 4);
        entry->play_time_total = sqlite3_column_int(stmt, 5);
        entry->play_time_average = sqlite3_column_int(stmt, 6);
        if (sqlite3_column_text(stmt, 7) != NULL) {
            entry->first_played_at = strdup((const char *)sqlite3_column_text(stmt, 7));
        }
        if (sqlite3_column_text(stmt, 8) != NULL) {
            entry->last_played_at = strdup((const char *)sqlite3_column_text(stmt, 8));
        }

        play_activities->play_time_total += entry->play_time_total;
    }

    sqlite3_finalize(stmt);
    play_activity_db_close();

    return play_activities;
}

void free_play_activities(PlayActivities *pa_ptr)
{
    if (pa_ptr == NULL)
        return;
    for (int i = 0; i < pa_ptr->count; i++) {
        free(pa_ptr->play_activity[i]->first_played_at);
        free(pa_ptr->play_activity[i]->last_played_at);
        // Free ROM structure fields before freeing the structure itself
        if (pa_ptr->play_activity[i]->rom != NULL) {
            free(pa_ptr->play_activity[i]->rom->type);
            free(pa_ptr->play_activity[i]->rom->name);
            free(pa_ptr->play_activity[i]->rom->file_path);
            free(pa_ptr->play_activity[i]->rom->image_path);
            free(pa_ptr->play_activity[i]->rom);
        }
        free(pa_ptr->play_activity[i]);
    }
    free(pa_ptr->play_activity);
    free(pa_ptr);
}

void __ensure_rel_path(char *rel_path, const char *rom_path)
{
    if (!file_path_relative_to(rel_path, ROMS_FOLDER, rom_path)) {
        if (strstr(rom_path, "../../Roms/") != NULL) {
            char *dup = strdup((const char *)rom_path);
            if (dup == NULL) {
                strncpy(rel_path, rom_path, PATH_MAX - 1);
                rel_path[PATH_MAX - 1] = '\0';
                return;
            }
            char *tail = str_split(dup, "../../Roms/");
            strncpy(rel_path, tail != NULL ? tail : rom_path, PATH_MAX - 1);
            rel_path[PATH_MAX - 1] = '\0';
            free(dup);
        }
        else {
            char *temp = strdup((const char *)rom_path);
            if (temp == NULL) {
                strncpy(rel_path, rom_path, PATH_MAX - 1);
                rel_path[PATH_MAX - 1] = '\0';
                return;
            }
            char *replaced = str_replace(temp, "/mnt/SDCARD/Roms/", "");
            free(temp);
            strncpy(rel_path, replaced ? replaced : (const char *)rom_path, PATH_MAX - 1);
            rel_path[PATH_MAX - 1] = '\0';
            free(replaced);
        }
    }
}

int __db_insert_rom(const char *rom_type, const char *rom_name, const char *file_path, const char *image_path)
{
    int rom_id = ROM_NOT_FOUND;

    char rel_path[PATH_MAX];
    __ensure_rel_path(rel_path, file_path);

    char *sql = sqlite3_mprintf("INSERT INTO rom(type, name, file_path, image_path) VALUES(%Q, %Q, %Q, %Q);",
                                rom_type, rom_name, rel_path, image_path);
    sqlite3_exec(play_activity_db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);

    sqlite3_stmt *stmt = play_activity_db_prepare("SELECT id FROM rom WHERE ROWID = last_insert_rowid()");
    if (stmt != NULL && sqlite3_step(stmt) == SQLITE_ROW) {
        rom_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    return rom_id;
}

int __db_insert_rom_from_cache(CacheDBItem *cache_db_item)
{
    return __db_insert_rom(cache_db_item->cache_path, cache_db_item->name, cache_db_item->path, cache_db_item->imgpath);
}

void __db_update_rom(int rom_id, const char *rom_type, const char *rom_name, const char *file_path, const char *image_path)
{
    char rel_path[PATH_MAX];
    __ensure_rel_path(rel_path, file_path);

    char *sql = sqlite3_mprintf("UPDATE rom SET type = %Q, name = %Q, file_path = %Q, image_path = %Q WHERE id = %d;",
                                rom_type, rom_name, rel_path, image_path, rom_id);
    sqlite3_exec(play_activity_db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
}

void __db_update_rom_from_cache(int rom_id, CacheDBItem *cache_db_item)
{
    __db_update_rom(rom_id, cache_db_item->cache_path, cache_db_item->name, cache_db_item->path, cache_db_item->imgpath);
}

int __db_get_orphan_rom_id(const char *rom_path)
{
    int rom_id = ROM_NOT_FOUND;
    char *_file_name = strdup(rom_path);
    if (_file_name == NULL)
        return ROM_NOT_FOUND;
    char *file_name = basename(_file_name);
    char *rom_name = file_removeExtension(file_name);
    if (rom_name == NULL) {
        free(_file_name);
        return ROM_NOT_FOUND;
    }

    char *sql = sqlite3_mprintf("SELECT id FROM rom WHERE (name=%Q OR name=%Q) AND type='ORPHAN' LIMIT 1;", rom_name, file_name);
    sqlite3_stmt *stmt = play_activity_db_prepare(sql);
    sqlite3_free(sql);
    free(rom_name);
    free(_file_name);

    if (stmt != NULL && sqlite3_step(stmt) == SQLITE_ROW) {
        rom_id = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    return rom_id;
}

int __db_get_rom_id_by_path(const char *rom_path)
{
    int rom_id = ROM_NOT_FOUND;

    char rel_path[PATH_MAX];
    __ensure_rel_path(rel_path, rom_path);

    char *sql = sqlite3_mprintf("SELECT id FROM rom WHERE file_path=%Q LIMIT 1;", rel_path);
    sqlite3_stmt *stmt = play_activity_db_prepare(sql);
    sqlite3_free(sql);

    if (stmt != NULL && sqlite3_step(stmt) == SQLITE_ROW) {
        rom_id = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    return rom_id;
}

int __db_rom_find_by_file_path(const char *rom_path, bool create_or_update)
{
    printf_debug("rom_find_by_file_path('%s')\n", rom_path);

    bool update_orphan = false;
    int rom_id = __db_get_rom_id_by_path(rom_path);

    if (rom_id == ROM_NOT_FOUND) {
        rom_id = __db_get_orphan_rom_id(rom_path);
        if (rom_id != ROM_NOT_FOUND) {
            update_orphan = true;
        }
    }
    else if (create_or_update) {
        CacheDBItem *cache_db_item = cache_db_find(rom_path);
        if (cache_db_item != NULL) {
            __db_update_rom_from_cache(rom_id, cache_db_item);
            free(cache_db_item);
        }
    }

    if (update_orphan) {
        CacheDBItem *cache_db_item = cache_db_find(rom_path);

        if (cache_db_item != NULL) {
            __db_update_rom_from_cache(rom_id, cache_db_item);
            free(cache_db_item);
        }
        else {
            char *rom_name = file_removeExtension(file_basename(rom_path));
            __db_update_rom(rom_id, "", rom_name != NULL ? rom_name : "", rom_path, "");
            free(rom_name);
        }
    }
    else if (rom_id == ROM_NOT_FOUND && create_or_update) {
        CacheDBItem *cache_db_item = cache_db_find(rom_path);

        if (cache_db_item != NULL) {
            rom_id = __db_insert_rom_from_cache(cache_db_item);
            free(cache_db_item);
        }
        else {
            char *rom_name = file_removeExtension(file_basename(rom_path));
            rom_id = __db_insert_rom("", rom_name != NULL ? rom_name : "", rom_path, "");
            free(rom_name);
        }
    }

    return rom_id;
}

int play_activity_transaction_rom_find_by_file_path(const char *rom_path, bool create_or_update)
{
    int retval;
    play_activity_db_open();
    retval = __db_rom_find_by_file_path(rom_path, create_or_update);
    play_activity_db_close();
    return retval;
}

int play_activity_get_play_time(const char *rom_path)
{
    int play_time = 0;
    play_activity_db_open();
    int rom_id = __db_rom_find_by_file_path(rom_path, false);
    if (rom_id != ROM_NOT_FOUND) {
        char *sql = sqlite3_mprintf("SELECT SUM(play_time) FROM play_activity WHERE rom_id = %d;", rom_id);
        sqlite3_stmt *stmt = play_activity_db_prepare(sql);
        sqlite3_free(sql);
        if (stmt != NULL && sqlite3_step(stmt) == SQLITE_ROW) {
            play_time = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    play_activity_db_close();
    return play_time;
}

bool _get_active_rom_path(char *rom_path_out)
{
    char *ptr;
    char cmd[STR_MAX] = "";

    FILE *fp;
    file_get(fp, CMD_TO_RUN, CONTENT_STR, cmd);

    if (strlen(cmd) == 0) {
        return false;
    }

    if ((ptr = strrchr(cmd, '"')) != NULL) {
        *ptr = '\0';
    }

    if ((ptr = strrchr(cmd, '"')) != NULL) {
        strncpy(rom_path_out, ptr + 1, STR_MAX - 1);
        rom_path_out[STR_MAX - 1] = '\0';
        return true;
    }

    return false;
}

int __db_get_active_closed_activity(void)
{
    int rom_id = ROM_NOT_FOUND;

    char rom_path[STR_MAX];
    if (!_get_active_rom_path(rom_path)) {
        return ROM_NOT_FOUND;
    }

    printf_debug("Last closed active rom: %s\n", rom_path);

    if ((rom_id = __db_rom_find_by_file_path(rom_path, false)) == ROM_NOT_FOUND) {
        return ROM_NOT_FOUND;
    }

    char *sql = sqlite3_mprintf("SELECT * FROM play_activity WHERE rom_id = %d AND play_time IS NULL;", rom_id);
    sqlite3_stmt *stmt = play_activity_db_prepare(sql);

    if (stmt != NULL && sqlite3_step(stmt) == SQLITE_ROW) {
        // Activity is not closed
        rom_id = ROM_NOT_FOUND;
    }

    sqlite3_free(sql);
    sqlite3_finalize(stmt);

    return rom_id;
}

void play_activity_start(char *rom_file_path)
{
    printf_debug("\n:: play_activity_start(%s)\n", rom_file_path);
    play_activity_db_open();
    int rom_id = __db_rom_find_by_file_path(rom_file_path, true);
    if (rom_id == ROM_NOT_FOUND) {
        play_activity_db_close();
        exit(1);
    }
    char *sql = sqlite3_mprintf("INSERT INTO play_activity(rom_id) VALUES(%d);", rom_id);
    sqlite3_exec(play_activity_db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    play_activity_db_close();
}

void play_activity_resume(void)
{
    print_debug("\n:: play_activity_resume()");
    play_activity_db_open();
    int rom_id = __db_get_active_closed_activity();
    if (rom_id == ROM_NOT_FOUND) {
        play_activity_db_close();
        printf("Error: no active rom\n");
        exit(1);
    }
    char *sql = sqlite3_mprintf("INSERT INTO play_activity(rom_id) VALUES(%d);", rom_id);
    sqlite3_exec(play_activity_db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    play_activity_db_close();
}

void play_activity_stop(char *rom_file_path)
{
    printf_debug("\n:: play_activity_stop(%s)\n", rom_file_path);
    play_activity_db_open();
    int rom_id = __db_rom_find_by_file_path(rom_file_path, false);
    if (rom_id == ROM_NOT_FOUND) {
        play_activity_db_close();
        exit(1);
    }
    char *sql = sqlite3_mprintf("UPDATE play_activity SET play_time = (strftime('%%s', 'now')) - created_at, updated_at = (strftime('%%s', 'now')) WHERE rom_id = %d AND play_time IS NULL;", rom_id);
    sqlite3_exec(play_activity_db, sql, NULL, NULL, NULL);
    sqlite3_free(sql);
    play_activity_db_close();
}

void play_activity_stop_all(void)
{
    print_debug("\n:: play_activity_stop_all()");
    play_activity_db_open();
    sqlite3_exec(play_activity_db,
        "UPDATE play_activity SET play_time = (strftime('%s', 'now')) - created_at, updated_at = (strftime('%s', 'now')) WHERE play_time IS NULL;"
        "DELETE FROM play_activity WHERE play_time < 0;",
        NULL, NULL, NULL);
    play_activity_db_close();
}

void play_activity_fix_paths(void)
{
    print_debug("\n:: play_activity_fix_paths()");
    play_activity_db_open();
    sqlite3_stmt *stmt = play_activity_db_prepare("SELECT id, file_path FROM rom WHERE file_path LIKE '/mnt/SDCARD/%%';");

    while (stmt != NULL && sqlite3_step(stmt) == SQLITE_ROW) {
        int rom_id = sqlite3_column_int(stmt, 0);
        char file_path[PATH_MAX];
        const char *col_file_path = (const char *)sqlite3_column_text(stmt, 1);
        strncpy(file_path, col_file_path != NULL ? col_file_path : "", sizeof(file_path) - 1);
        file_path[sizeof(file_path) - 1] = '\0';

        if (strlen(file_path) == 0) {
            continue;
        }

        char cache_path[PATH_MAX];
        char cache_name[STR_MAX];
        int cache_version = cache_get_path(cache_path, cache_name, file_path);

        char rel_path[PATH_MAX];
        __ensure_rel_path(rel_path, file_path);

        char *sql;
        if (cache_version == CACHE_NOT_FOUND) {
            sql = sqlite3_mprintf("UPDATE rom SET file_path = %Q WHERE id = %d;", rel_path, rom_id);
        }
        else {
            sql = sqlite3_mprintf("UPDATE rom SET file_path = %Q, type = %Q WHERE id = %d;", rel_path, cache_path, rom_id);
        }
        printf_debug("%s\n", sql);
        sqlite3_exec(play_activity_db, sql, NULL, NULL, NULL);
        sqlite3_free(sql);
    }

    sqlite3_finalize(stmt);
    play_activity_db_close();
}

void play_activity_list_all(void)
{
    print_debug("\n:: play_activity_list_all()");
    int total_play_time = play_activity_get_total_play_time();
    PlayActivities *pas = play_activity_find_all();
    if (pas == NULL) {
        printf("\nNo play activity data found.\n");
        return;
    }

    printf("\n");

    for (int i = 0; i < pas->count; i++) {
        PlayActivity *entry = pas->play_activity[i];
        ROM *rom = entry->rom;
        char rom_name[STR_MAX];
        file_cleanName(rom_name, rom->name);
        char play_time[STR_MAX];
        str_serializeTime(play_time, entry->play_time_total);
        printf("%03d: %s (%s) [%s]\n", i + 1, rom_name, play_time, rom->type);
    }

    char total_str[25];
    str_serializeTime(total_str, total_play_time);
    printf("\nTotal: %s\n", total_str);

    free_play_activities(pas);
}

#endif // PLAY_ACTIVITY_DB_H
