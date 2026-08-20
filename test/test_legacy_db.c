/**
 * @file test_legacy_db.c
 * @brief Unit tests for src/playActivity/legacyDB.h
 *
 * Tests the legacy play activity database struct layout and the
 * readLegacyDB() binary file parsing logic by creating temporary
 * binary files with known rom_list_s entries.
 *
 * Build and run: make -f Makefile.unit test_legacy_db
 */

#include "onion_test.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- Provide STR_MAX ---- */
#define STR_MAX 256

/* ---- Stub log macros ---- */
#define print_debug(...)
#define printf_debug(...)

/* ---- Stub is_file ---- */
static bool is_file(const char *path)
{
    return access(path, F_OK) == 0;
}

/* ---- Inline types and constants from legacyDB.h ---- */

#define LEGACY_DB_MAX 1000

typedef struct structRom {
    char name[100];
    int playTime;
} rom_list_s;

static rom_list_s rom_list[LEGACY_DB_MAX];
static int rom_list_len = 0;

/* ---- Inline readLegacyDB (parameterized for testing) ---- */

static int readLegacyDB_path(const char *db_path)
{
    FILE *fp;

    if (is_file(db_path)) {
        if ((fp = fopen(db_path, "rb")) != NULL) {
            if (fread(rom_list, sizeof(rom_list), 1, fp) != 1)
                memset(rom_list, 0, sizeof(rom_list));
            rom_list_len = 0;

            for (int i = 0; i < LEGACY_DB_MAX; i++) {
                if ((strlen(rom_list[i].name) != 0) && (rom_list[i].playTime) != 0)
                    rom_list_len++;
            }

            fclose(fp);
        }
        else {
            return -1;
        }
    }

    return 1;
}

/* ---- Helper to write binary DB ---- */

static void write_test_db(const char *path, rom_list_s *entries, int count)
{
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return;

    /* Write full array (zero-filled) */
    rom_list_s empty_list[LEGACY_DB_MAX];
    memset(empty_list, 0, sizeof(empty_list));
    for (int i = 0; i < count && i < LEGACY_DB_MAX; i++) {
        empty_list[i] = entries[i];
    }
    fwrite(empty_list, sizeof(empty_list), 1, fp);
    fclose(fp);
}

/* ==== Struct layout tests ==== */

TEST(struct_size_name_field) {
    rom_list_s entry;
    ASSERT_EQ(sizeof(entry.name), 100);
}

TEST(struct_size_total) {
    /* name[100] + int playTime = 104 bytes (with typical alignment) */
    ASSERT_EQ(sizeof(rom_list_s), 104);
}

TEST(struct_initialization) {
    rom_list_s entry;
    memset(&entry, 0, sizeof(entry));
    ASSERT_STREQ(entry.name, "");
    ASSERT_EQ(entry.playTime, 0);
}

/* ==== readLegacyDB tests ==== */

TEST(read_db_nonexistent_file) {
    memset(rom_list, 0, sizeof(rom_list));
    rom_list_len = 0;

    /* Non-existent file should return 1 (no error, just empty) */
    int result = readLegacyDB_path("/tmp/test_legacy_db_nonexistent.db");
    ASSERT_EQ(result, 1);
    ASSERT_EQ(rom_list_len, 0);
}

TEST(read_db_single_entry) {
    const char *db_path = "/tmp/test_legacy_db_single.db";

    rom_list_s entries[1];
    memset(entries, 0, sizeof(entries));
    strncpy(entries[0].name, "Super Mario", sizeof(entries[0].name) - 1);
    entries[0].playTime = 3600;

    write_test_db(db_path, entries, 1);

    memset(rom_list, 0, sizeof(rom_list));
    rom_list_len = 0;
    int result = readLegacyDB_path(db_path);

    ASSERT_EQ(result, 1);
    ASSERT_EQ(rom_list_len, 1);
    ASSERT_STREQ(rom_list[0].name, "Super Mario");
    ASSERT_EQ(rom_list[0].playTime, 3600);

    unlink(db_path);
}

TEST(read_db_multiple_entries) {
    const char *db_path = "/tmp/test_legacy_db_multi.db";

    rom_list_s entries[3];
    memset(entries, 0, sizeof(entries));
    strncpy(entries[0].name, "Game A", sizeof(entries[0].name) - 1);
    entries[0].playTime = 100;
    strncpy(entries[1].name, "Game B", sizeof(entries[1].name) - 1);
    entries[1].playTime = 200;
    strncpy(entries[2].name, "Game C", sizeof(entries[2].name) - 1);
    entries[2].playTime = 300;

    write_test_db(db_path, entries, 3);

    memset(rom_list, 0, sizeof(rom_list));
    rom_list_len = 0;
    int result = readLegacyDB_path(db_path);

    ASSERT_EQ(result, 1);
    ASSERT_EQ(rom_list_len, 3);
    ASSERT_STREQ(rom_list[0].name, "Game A");
    ASSERT_STREQ(rom_list[1].name, "Game B");
    ASSERT_STREQ(rom_list[2].name, "Game C");
    ASSERT_EQ(rom_list[0].playTime, 100);
    ASSERT_EQ(rom_list[1].playTime, 200);
    ASSERT_EQ(rom_list[2].playTime, 300);

    unlink(db_path);
}

TEST(read_db_skips_zero_playtime) {
    const char *db_path = "/tmp/test_legacy_db_skipzero.db";

    rom_list_s entries[3];
    memset(entries, 0, sizeof(entries));
    strncpy(entries[0].name, "Played", sizeof(entries[0].name) - 1);
    entries[0].playTime = 500;
    strncpy(entries[1].name, "NotPlayed", sizeof(entries[1].name) - 1);
    entries[1].playTime = 0; /* zero playtime → not counted */
    strncpy(entries[2].name, "AlsoPlayed", sizeof(entries[2].name) - 1);
    entries[2].playTime = 1000;

    write_test_db(db_path, entries, 3);

    memset(rom_list, 0, sizeof(rom_list));
    rom_list_len = 0;
    int result = readLegacyDB_path(db_path);

    ASSERT_EQ(result, 1);
    /* Only entries with non-zero name AND non-zero playTime are counted */
    ASSERT_EQ(rom_list_len, 2);

    unlink(db_path);
}

TEST(read_db_skips_empty_name) {
    const char *db_path = "/tmp/test_legacy_db_skipempty.db";

    rom_list_s entries[2];
    memset(entries, 0, sizeof(entries));
    /* Entry 0: empty name but has playtime → not counted */
    entries[0].name[0] = '\0';
    entries[0].playTime = 100;
    /* Entry 1: has name and playtime → counted */
    strncpy(entries[1].name, "ValidGame", sizeof(entries[1].name) - 1);
    entries[1].playTime = 200;

    write_test_db(db_path, entries, 2);

    memset(rom_list, 0, sizeof(rom_list));
    rom_list_len = 0;
    int result = readLegacyDB_path(db_path);

    ASSERT_EQ(result, 1);
    ASSERT_EQ(rom_list_len, 1);

    unlink(db_path);
}

TEST(read_db_truncated_file) {
    const char *db_path = "/tmp/test_legacy_db_trunc.db";

    /* Write a file that's too small (only a few bytes) */
    FILE *fp = fopen(db_path, "wb");
    if (fp) {
        fprintf(fp, "short");
        fclose(fp);
    }

    memset(rom_list, 0, sizeof(rom_list));
    rom_list_len = 0;
    int result = readLegacyDB_path(db_path);

    /* fread returns != 1, so memset zeros the list, rom_list_len = 0 */
    ASSERT_EQ(result, 1);
    ASSERT_EQ(rom_list_len, 0);

    unlink(db_path);
}

TEST(read_db_empty_database) {
    const char *db_path = "/tmp/test_legacy_db_empty.db";

    /* All-zero database */
    rom_list_s entries[1];
    memset(entries, 0, sizeof(entries));
    write_test_db(db_path, entries, 0);

    memset(rom_list, 0, sizeof(rom_list));
    rom_list_len = 42; /* set to non-zero to verify it gets reset */
    int result = readLegacyDB_path(db_path);

    ASSERT_EQ(result, 1);
    ASSERT_EQ(rom_list_len, 0);

    unlink(db_path);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== legacyDB.h Unit Tests ===\n\n");

    /* Struct layout */
    RUN_TEST(struct_size_name_field);
    RUN_TEST(struct_size_total);
    RUN_TEST(struct_initialization);

    /* readLegacyDB */
    RUN_TEST(read_db_nonexistent_file);
    RUN_TEST(read_db_single_entry);
    RUN_TEST(read_db_multiple_entries);
    RUN_TEST(read_db_skips_zero_playtime);
    RUN_TEST(read_db_skips_empty_name);
    RUN_TEST(read_db_truncated_file);
    RUN_TEST(read_db_empty_database);

    TEST_REPORT();
    return test_failures;
}
