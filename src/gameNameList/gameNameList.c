#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <sqlite3/sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gamename.h"
#include "utils/file.h"

#define MAX_MATCHING_FOLDERS 1000
#define LIST_LINE_LEN 1024
#define FULL_ROM_LIST_NAME "full-arcade-rom-name-list.txt"
#define ARCADE_ROM_NAMES_NAME "arcade-rom-names.txt"
#define MISSING_ROM_NAMES_NAME "missing_roms_name.txt"
#define ROM_NAMES_FILE_NAME "all_roms_found.txt"

#ifndef STR_MAX
#define STR_MAX 256
#endif

char matching_folders[MAX_MATCHING_FOLDERS][256];
int systems_count = 0;

// loaded shared lib function
char *(*GetGameName_func)(const char *, const char *);

// Remove the extension from a file name
void removeExtension(char *file_name)
{
    char *last_dot = strrchr(file_name, '.');
    if (last_dot != NULL)
        *last_dot = '\0';
}

int endsWith(const char *str, const char *suffix)
{
    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);
    if (len_suffix > len_str)
        return 0;
    return strncmp(str + len_str - len_suffix, suffix, len_suffix) == 0;
}

/* Systems flagged "shortname": 1 (was find + grep + sed through a shell) */

// Blanks on the same line: grep's [[:space:]] never crossed a line.
static const char *skipLineBlanks(const char *p)
{
    while (*p != '\n' && isspace((unsigned char)*p))
        p++;
    return p;
}

static bool configHasShortname(const char *json)
{
    static const char key[] = "\"shortname\":";
    for (const char *p = strstr(json, key); p != NULL; p = strstr(p + 1, key)) {
        const char *v = skipLineBlanks(p + sizeof(key) - 1);
        if (v[0] == '1' && !isdigit((unsigned char)v[1]))
            return true;
    }
    return false;
}

// First "rompath": "..." value of a config.json.
static bool configRomPath(const char *json, char *out, size_t out_size)
{
    static const char key[] = "\"rompath\":";
    for (const char *p = strstr(json, key); p != NULL; p = strstr(p + 1, key)) {
        const char *v = skipLineBlanks(p + sizeof(key) - 1);
        if (*v != '"')
            continue;
        v++;
        const char *end = strpbrk(v, "\"\n");
        if (end == NULL || *end != '"')
            continue;
        snprintf(out, out_size, "%.*s", (int)(end - v), v);
        return true;
    }
    return false;
}

// Add the ROM folder of a config.json flagged "shortname": 1, once.
static int addShortnameSystem(const char *config_path, int count)
{
    char *json = file_read(config_path);
    if (json == NULL)
        return count;

    char rompath[STR_MAX];
    bool wanted = configHasShortname(json) &&
                  configRomPath(json, rompath, sizeof(rompath));
    free(json);
    if (!wanted)
        return count;

    // Without a rompath the folder name came from an uninitialised buffer;
    // an empty one gave ".", and the ROM scan then walked all of Roms/.
    const char *system_name = basename(rompath);
    if (system_name[0] == '\0' || strcmp(system_name, ".") == 0 ||
        strcmp(system_name, "..") == 0 || strcmp(system_name, "/") == 0)
        return count;

    for (int i = 0; i < count; i++) {
        if (strcmp(matching_folders[i], system_name) == 0)
            return count;
    }
    snprintf(matching_folders[count], sizeof(matching_folders[count]), "%s", system_name);
    return count + 1;
}

// Every config.json below disk_path, like `find disk_path -name config.json
// -type f` (symlinks not followed), without two processes per file.
int findFoldersWithShortname(const char *disk_path, int count)
{
    DIR *dir = opendir(disk_path);
    if (dir == NULL)
        return count;

    struct dirent *entry;
    while (count < MAX_MATCHING_FOLDERS && (entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char path[PATH_MAX];
        if (snprintf(path, sizeof(path), "%s/%s", disk_path, entry->d_name) >= (int)sizeof(path))
            continue;
        if (entry->d_type == DT_DIR)
            count = findFoldersWithShortname(path, count);
        else if (entry->d_type == DT_REG && strcmp(entry->d_name, "config.json") == 0)
            count = addShortnameSystem(path, count);
    }
    closedir(dir);
    return count;
}

void getRomNamesDir(const char *dir_path, const char *rom_ext, FILE *rom_names_file)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        perror("Error opening directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            char sub_dir_path[PATH_MAX];
            snprintf(sub_dir_path, sizeof(sub_dir_path), "%s/%s", dir_path, entry->d_name);
            getRomNamesDir(sub_dir_path, rom_ext, rom_names_file);
        }
        else if (entry->d_type == DT_REG && endsWith(entry->d_name, rom_ext)) {
            char shortname[256];
            snprintf(shortname, sizeof(shortname), "%s", entry->d_name);
            removeExtension(shortname);
            fprintf(rom_names_file, "%s\n", shortname);
        }
    }
    closedir(dir);
}

static int compareLines(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

// Sort the lines of a file, without blank lines or duplicates (was
// `awk '$1' | sort -u` through a shell, with the path unquoted).
int sortFileLines(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
        return -1;

    char **lines = NULL;
    size_t count = 0, capacity = 0;
    char line[LIST_LINE_LEN];
    int rc = 0;

    while (rc == 0 && fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0')
            continue;
        if (count == capacity) {
            size_t grown_capacity = capacity ? capacity * 2 : 256;
            char **grown = realloc(lines, grown_capacity * sizeof(*lines));
            if (grown == NULL) {
                rc = -1;
                break;
            }
            lines = grown;
            capacity = grown_capacity;
        }
        if ((lines[count] = strdup(line)) == NULL)
            rc = -1;
        else
            count++;
    }
    fclose(fp);

    if (rc == 0) {
        if (count > 0)
            qsort(lines, count, sizeof(*lines), compareLines);
        fp = fopen(filename, "w");
        if (fp == NULL)
            rc = -1;
        else {
            for (size_t i = 0; i < count; i++) {
                if (i == 0 || strcmp(lines[i], lines[i - 1]) != 0)
                    fprintf(fp, "%s\n", lines[i]);
            }
            if (fclose(fp) != 0)
                rc = -1;
        }
    }

    for (size_t i = 0; i < count; i++)
        free(lines[i]);
    free(lines);
    return rc;
}

int getRomNames(const char *rom_dir_path, const char *rom_names_file_path)
{
    char foldername[PATH_MAX];

    snprintf(foldername, sizeof(foldername), "%s/Emu", rom_dir_path);
    systems_count = findFoldersWithShortname(foldername, 0);
    snprintf(foldername, sizeof(foldername), "%s/RApp", rom_dir_path);
    systems_count = findFoldersWithShortname(foldername, systems_count);

    FILE *rom_names_file = fopen(rom_names_file_path, "w");
    if (rom_names_file == NULL) {
        printf("Error: Failed to open file %s\n", rom_names_file_path);
        return -1;
    }

    for (int i = 0; i < systems_count; i++) {
        snprintf(foldername, sizeof(foldername), "%s/Roms/%s", rom_dir_path, matching_folders[i]);
        getRomNamesDir(foldername, ".zip", rom_names_file);
    }
    fclose(rom_names_file);

    // matchRomNames() merges sorted lists: an unsorted one would match nothing.
    return sortFileLines(rom_names_file_path) == 0 ? 0 : -1;
}

// Next non-empty line of fp, without its line ending. A line longer than buf
// is cut and the rest skipped (fgets returned it as the next line: a ROM
// name over 49 characters became two names).
static bool readListLine(FILE *fp, char *buf, size_t size)
{
    while (fgets(buf, (int)size, fp) != NULL) {
        size_t len = strcspn(buf, "\r\n");
        if (buf[len] == '\0' && !feof(fp)) {
            int c;
            while ((c = fgetc(fp)) != EOF && c != '\n')
                ;
        }
        buf[len] = '\0';
        if (len > 0)
            return true;
    }
    return false;
}

// strcmp() of the first word of a full-list line (up to a space or a tab)
// with a ROM name. A blank line gave strtok() NULL, and strcmp() crashed.
static int compareFirstWord(const char *line, const char *rom)
{
    line += strspn(line, " \t");
    size_t word_len = strcspn(line, " \t");
    int c = strncmp(line, rom, word_len);
    if (c != 0)
        return c;
    return rom[word_len] == '\0' ? 0 : -1;
}

static void mergeRomLists(FILE *roms, FILE *full, FILE *arcade, FILE *missing)
{
    char rom[LIST_LINE_LEN], line[LIST_LINE_LEN];
    bool has_rom = readListLine(roms, rom, sizeof(rom));
    bool has_line = readListLine(full, line, sizeof(line));

    while (has_rom && has_line) {
        int c = compareFirstWord(line, rom);
        if (c == 0) {
            fprintf(arcade, "%s\n", line);
            has_rom = readListLine(roms, rom, sizeof(rom));
            has_line = readListLine(full, line, sizeof(line));
        }
        else if (c > 0) { // not in the full list
            fprintf(missing, "%s\n", rom);
            has_rom = readListLine(roms, rom, sizeof(rom));
        }
        else
            has_line = readListLine(full, line, sizeof(line));
    }

    // ROMs sorted after the last entry of the full list were never written.
    while (has_rom) {
        fprintf(missing, "%s\n", rom);
        has_rom = readListLine(roms, rom, sizeof(rom));
    }
}

int matchRomNames(const char *rom_names_file, const char *full_rom_list_file,
                  const char *arcade_rom_names_file, const char *missing_rom_names_file)
{
    FILE *roms = fopen(rom_names_file, "r");
    FILE *full = fopen(full_rom_list_file, "r");
    FILE *arcade = NULL, *missing = NULL;
    int rc = 1;

    // The outputs are opened (and truncated) only once both inputs opened.
    if (roms != NULL && full != NULL) {
        arcade = fopen(arcade_rom_names_file, "w");
        missing = fopen(missing_rom_names_file, "w");
    }

    if (arcade != NULL && missing != NULL) {
        mergeRomLists(roms, full, arcade, missing);
        rc = 0;
    }
    else
        printf("Error opening files\n");

    if (roms != NULL)
        fclose(roms);
    if (full != NULL)
        fclose(full);
    if (arcade != NULL && fclose(arcade) != 0)
        rc = 1;
    if (missing != NULL && fclose(missing) != 0)
        rc = 1;
    return rc;
}

// Copy through a temporary file: the full list is copied only once, and a
// `cp` cut short by a power loss left a truncated list for good.
static int copyListFile(const char *src, const char *dst)
{
    char tmp[PATH_MAX];
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", dst) >= (int)sizeof(tmp))
        return -1;

    FILE *in = fopen(src, "rb");
    if (in == NULL)
        return -1;
    FILE *out = fopen(tmp, "wb");
    if (out == NULL) {
        fclose(in);
        return -1;
    }

    char buf[4096];
    size_t n;
    int rc = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            rc = -1;
            break;
        }
    }
    if (ferror(in))
        rc = -1;
    fclose(in);
    if (fflush(out) != 0 || fsync(fileno(out)) != 0)
        rc = -1;
    if (fclose(out) != 0)
        rc = -1;
    if (rc == 0 && rename(tmp, dst) != 0)
        rc = -1;
    if (rc != 0)
        remove(tmp);
    return rc;
}

int createCopyFile(const char *src_path, const char *dst_path)
{
    if (access(src_path, F_OK) != 0) {
        printf("The input file to copy does not exist\n");
        return -1;
    }
    if (access(dst_path, F_OK) == 0)
        return 0; // made on the first run, kept since
    return copyListFile(src_path, dst_path) == 0 ? 1 : -1;
}

// Write the displayed title of every ROM of one system into its MainUI
// cache. The UPDATE is prepared once per database (was once per row), and a
// failure rolls the system back instead of leaving the transaction open.
static void updateCacheDb(const char *cache_path, const char *system_name)
{
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(cache_path, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s (%s)\n", sqlite3_errmsg(db), cache_path);
        sqlite3_close(db);
        return;
    }

    // %w quotes the table name (a folder name with a quote broke the SQL).
    char *select_sql = sqlite3_mprintf("SELECT id, path FROM \"%w_roms\" WHERE type = 0", system_name);
    char *update_sql = sqlite3_mprintf("UPDATE \"%w_roms\" SET disp = ? WHERE id = ?", system_name);
    sqlite3_stmt *select = NULL, *update = NULL;
    bool ok = select_sql != NULL && update_sql != NULL &&
              sqlite3_prepare_v2(db, select_sql, -1, &select, NULL) == SQLITE_OK &&
              sqlite3_prepare_v2(db, update_sql, -1, &update, NULL) == SQLITE_OK &&
              sqlite3_exec(db, "BEGIN", NULL, NULL, NULL) == SQLITE_OK;
    if (!ok)
        fprintf(stderr, "Cannot prepare the update: %s (%s)\n", sqlite3_errmsg(db), cache_path);
    bool in_transaction = ok;

    int rc = SQLITE_DONE;
    while (ok && (rc = sqlite3_step(select)) == SQLITE_ROW) {
        const char *rom_path = (const char *)sqlite3_column_text(select, 1);
        if (rom_path == NULL)
            continue;

        // basename() and removeExtension() write into their argument: work
        // on a copy, not on the row SQLite handed over.
        char name[PATH_MAX];
        snprintf(name, sizeof(name), "%s", rom_path);
        char *romname = basename(name);
        removeExtension(romname);

        char *title = GetGameName_func("wathever", romname);
        if (title == NULL)
            continue;

        sqlite3_bind_text(update, 1, title, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(update, 2, sqlite3_column_int(select, 0));
        if (sqlite3_step(update) != SQLITE_DONE) {
            fprintf(stderr, "Update failed: %s (%s)\n", sqlite3_errmsg(db), cache_path);
            ok = false;
        }
        sqlite3_reset(update);
    }
    if (ok && rc != SQLITE_DONE) {
        fprintf(stderr, "Error selecting rows: %s\n", sqlite3_errmsg(db));
        ok = false;
    }

    sqlite3_reset(select);
    if (in_transaction)
        sqlite3_exec(db, ok ? "COMMIT" : "ROLLBACK", NULL, NULL, NULL);

    sqlite3_finalize(select);
    sqlite3_finalize(update);
    sqlite3_free(select_sql);
    sqlite3_free(update_sql);
    sqlite3_close(db);
}

void updateSqlliteCache(const char *base_dir_path)
{
    for (int i = 0; i < systems_count; i++) {
        char cache_path[PATH_MAX];
        // a bit of assumption here on the path, to be perfected
        snprintf(cache_path, sizeof(cache_path), "%s/Roms/%s/%s_cache6.db",
                 base_dir_path, matching_folders[i], matching_folders[i]);
        if (is_file(cache_path))
            updateCacheDb(cache_path, matching_folders[i]);
    }
}

int main(int argc, char *argv[])
{
    char full_rom_list_path[PATH_MAX];
    char arcade_rom_names_path[PATH_MAX];
    char missing_rom_names_path[PATH_MAX];
    char rom_names_file_path[PATH_MAX];
    char libpath[PATH_MAX];

    // Both paths are needed: with one argument, argv[2] was NULL.
    if (argc < 3) {
        printf("Usage: gameNameList base_dir_path list_dir_path\n");
        return 1;
    }

    const char *base_dir_path = argv[1];
    const char *list_dir_path = argv[2];
    snprintf(full_rom_list_path, sizeof(full_rom_list_path), "%s/%s", list_dir_path, FULL_ROM_LIST_NAME);
    snprintf(arcade_rom_names_path, sizeof(arcade_rom_names_path), "%s/%s", list_dir_path, ARCADE_ROM_NAMES_NAME);
    snprintf(missing_rom_names_path, sizeof(missing_rom_names_path), "%s/%s", list_dir_path, MISSING_ROM_NAMES_NAME);
    snprintf(rom_names_file_path, sizeof(rom_names_file_path), "%s/%s", list_dir_path, ROM_NAMES_FILE_NAME);

    if (createCopyFile(arcade_rom_names_path, full_rom_list_path) < 0)
        return -1;

    // Get the list of rom names and write them to a sorted file
    if (getRomNames(base_dir_path, rom_names_file_path) != 0) {
        printf("Error: Failed to get rom names\n");
        return -1;
    }

    // Match the rom names and write the results to the output files
    if (matchRomNames(rom_names_file_path, full_rom_list_path,
                      arcade_rom_names_path, missing_rom_names_path) != 0) {
        printf("Error: Failed to match rom names\n");
        return -1;
    }

    // open the shared library to get the rom title from the rom short name
    snprintf(libpath, sizeof(libpath), "%s/miyoo/lib/libgamename.so", base_dir_path);
    void *handle = dlopen(libpath, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "Error loading library: %s\n", dlerror());
        return 1;
    }

    GetGameName_func = dlsym(handle, "GetGameNameForC");
    if (GetGameName_func == NULL) {
        fprintf(stderr, "Error retrieving symbol: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }

    // Titles into the MainUI caches, so lists sort by the displayed name
    updateSqlliteCache(base_dir_path);

    remove(rom_names_file_path);
    dlclose(handle);
    return 0;
}
