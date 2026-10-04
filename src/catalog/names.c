/* SPDX-License-Identifier: GPL-3.0-only */
#include "catalog/names.h"
#include "platform/files.h"
#include "platform/mutex.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *key, *value;
} MainUINameSlot;

typedef struct {
    char *text;
    MainUINameSlot *slots;
    size_t capacity;
} NameMap;

static NameMap map;
static char source[4096];
static MainUIMutex lock = MAINUI_MUTEX_INITIALIZER;

static size_t hash(const char *key)
{
    uint32_t value = 2166136261u;
    while (*key) {
        value = (value ^ (unsigned char)*key++) * 16777619u;
    }
    return value;
}

void mainui_names_close(MainUINameLookup *lookup)
{
    free(lookup->text);
    *lookup = (MainUINameLookup){0};
}

static void load(NameMap *lookup, const char *sd)
{
    char path[4096];
    int size = snprintf(path, sizeof path, "%s/BIOS/arcade_lists/arcade-rom-names.txt", sd);
    if (size < 0 || size >= (int)sizeof path) {
        return;
    }
    char *text = mainui_read_text(path, 8 * 1024 * 1024);
    if (!text) {
        return;
    }
    size_t lines = 1, capacity = 16;
    for (const char *p = text; *p; p++) {
        lines += *p == '\n';
    }
    /* Bound the index too: a small file can contain millions of empty lines. */
    if (lines > 131072) {
        free(text);
        return;
    }
    while (capacity < lines * 2) {
        capacity *= 2;
    }
    MainUINameSlot *slots = calloc(capacity, sizeof *slots);
    if (!slots) {
        free(text);
        return;
    }
    char *line = text;
    while (line) {
        char *next = strchr(line, '\n');
        if (next) {
            *next++ = 0;
        }
        while (*line == ' ' || *line == '\t' || *line == '\r') {
            line++;
        }
        char *end = line + strlen(line);
        if (end > line && end[-1] == '\r') {
            *--end = 0;
        }
        char *separator = line;
        while (*separator && *separator != ' ' && *separator != '\t' && *separator != '\r') {
            separator++;
        }
        if (*separator) {
            *separator++ = 0;
            while (*separator == ' ' || *separator == '\t') {
                separator++;
            }
            if (end - separator >= 2) {
                /* Frozen libgamename value.substr(1,size-2), including its
                 * unusual trailing-space behavior. Last duplicate wins. */
                char *value = separator + 1;
                end[-1] = 0;
                size_t slot = hash(line) & (capacity - 1);
                while (slots[slot].key && strcmp(slots[slot].key, line)) {
                    slot = (slot + 1) & (capacity - 1);
                }
                slots[slot] = (MainUINameSlot){line, value};
            }
        }
        if (!next) {
            break;
        }
        line = next;
    }
    lookup->text = text;
    lookup->slots = slots;
    lookup->capacity = capacity;
}

const char *mainui_name_lookup(MainUINameLookup *lookup, const char *sd, const char *key,
                               const char *fallback)
{
    if (!key || strlen(sd) >= sizeof source) {
        return fallback;
    }
    mainui_mutex_lock(&lock);
    if (strcmp(source, sd)) {
        free(map.text);
        free(map.slots);
        map = (NameMap){0};
        strcpy(source, sd);
        /* Cache a bounded snapshot, including misses and rejected oversized maps. */
        load(&map, sd);
    }
    const char *value = NULL;
    if (map.slots) {
        size_t slot = hash(key) & (map.capacity - 1);
        while (map.slots[slot].key) {
            if (!strcmp(map.slots[slot].key, key)) {
                value = map.slots[slot].value;
                break;
            }
            slot = (slot + 1) & (map.capacity - 1);
        }
    }
    char *copy = value ? malloc(strlen(value) + 1) : NULL;
    if (copy) {
        strcpy(copy, value);
    }
    mainui_mutex_unlock(&lock);
    free(lookup->text);
    lookup->text = copy;
    return copy ? copy : fallback;
}
