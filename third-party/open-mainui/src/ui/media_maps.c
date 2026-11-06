/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/media_maps.h"
#include <ctype.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char *filename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int compare_map(const void *left, const void *right)
{
    const MainUIMap *a = left, *b = right;
    if (a->number != b->number) {
        return a->number < b->number ? -1 : 1;
    }
    return strcmp(filename(a->path), filename(b->path));
}

/* The base map is <ROM name>.ext; subsequent maps are <ROM name>-N.ext.
 * Reject arbitrary suffixes and extremely long numbers without overflow. */
static bool matches(const char *file, const char *stem, int *number)
{
    size_t length = strlen(stem);
    /* The name in any case, as the gallery's are found on the SD card. */
    if (strncasecmp(file, stem, length)) {
        return false;
    }
    const char *suffix = file + length;
    int value = 0;
    if (*suffix == '-') {
        suffix++;
        if (!isdigit((unsigned char)*suffix)) {
            return false;
        }
        while (isdigit((unsigned char)*suffix)) {
            int digit = *suffix++ - '0';
            if (value > (2147483647 - digit) / 10) {
                return false;
            }
            value = value * 10 + digit;
        }
        if (!value) {
            return false;
        }
    }
    /* PNG only, as the gallery's pictures. */
    if (strcasecmp(suffix, ".png")) {
        return false;
    }
    *number = value;
    return true;
}

int mainui_maps_find(const char *rom_root, const char *rom, MainUIMap *maps, int capacity)
{
    if (!rom_root || !*rom_root || !rom || !maps || capacity <= 0) {
        return 0;
    }
    if (capacity > MAINUI_MEDIA_MAPS_MAX) {
        capacity = MAINUI_MEDIA_MAPS_MAX;
    }
    const char *file = filename(rom);
    size_t length = strlen(file);
    if (!length || length >= 1024) {
        return 0;
    }
    char stem[1024];
    memcpy(stem, file, length + 1);
    char *dot = strrchr(stem, '.');
    if (dot && dot != stem) {
        *dot = 0;
    }
    if (!*stem) {
        return 0;
    }
    char dirpath[4096];
    int n = snprintf(dirpath, sizeof dirpath, "%s/media/maps", rom_root);
    if (n < 0 || (size_t)n >= sizeof dirpath) {
        return 0;
    }
    DIR *dir = opendir(dirpath);
    if (!dir) {
        return 0;
    }
    MainUIMap *candidate = malloc(sizeof *candidate);
    if (!candidate) {
        closedir(dir);
        return 0;
    }
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        int number;
        if (!matches(name, stem, &number)) {
            continue;
        }
        memset(candidate, 0, sizeof *candidate);
        candidate->number = number;
        n = snprintf(candidate->path, sizeof candidate->path, "%s/%s", dirpath, name);
        if (n < 0 || (size_t)n >= sizeof candidate->path) {
            continue;
        }
        struct stat info;
        if (stat(candidate->path, &info) || !S_ISREG(info.st_mode)) {
            continue;
        }
        /* Deterministic bounded selection, independent of readdir order. */
        int duplicate = -1;
        for (int i = 0; i < count; i++) {
            if (maps[i].number == number) {
                duplicate = i;
                break;
            }
        }
        if (duplicate >= 0) {
            /* One map per numbered slot even with multiple extensions/case variants. */
            if (strcmp(name, filename(maps[duplicate].path)) < 0) {
                maps[duplicate] = *candidate;
                qsort(maps, (size_t)count, sizeof *maps, compare_map);
            }
            continue;
        }
        int where = count;
        while (where > 0 && compare_map(candidate, &maps[where - 1]) < 0) {
            where--;
        }
        if (where >= capacity) {
            continue;
        }
        if (count < capacity) {
            count++;
        }
        for (int i = count - 1; i > where; i--) {
            maps[i] = maps[i - 1];
        }
        maps[where] = *candidate;
    }
    free(candidate);
    closedir(dir);
    return count;
}
