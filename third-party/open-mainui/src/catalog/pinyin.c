/* SPDX-License-Identifier: GPL-3.0-only */
#include "catalog/pinyin.h"
#include "platform/files.h"
#include "platform/mutex.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t code, order;
    unsigned char initial;
} Initial;

static Initial *table;
static size_t count;
static char source[4096];
static MainUIMutex lock = MAINUI_MUTEX_INITIALIZER;

/* Invalid input consumes one byte, never reads beyond the terminating NUL. */
static uint32_t character(const unsigned char **cursor)
{
    const unsigned char *p = *cursor;
    uint32_t code = *p++;
    unsigned n = 0, minimum = 0;
    if (code >= 0xc2 && code <= 0xdf) {
        n = 1;
        minimum = 0x80;
        code &= 31;
    }
    else if (code >= 0xe0 && code <= 0xef) {
        n = 2;
        minimum = 0x800;
        code &= 15;
    }
    else if (code >= 0xf0 && code <= 0xf4) {
        n = 3;
        minimum = 0x10000;
        code &= 7;
    }
    else if (code >= 0x80) {
        (*cursor)++;
        return 0xfffd;
    }
    for (unsigned i = 0; i < n; i++) {
        if ((p[i] & 0xc0) != 0x80) {
            (*cursor)++;
            return 0xfffd;
        }
        code = (code << 6) | (p[i] & 63);
    }
    *cursor = p + n;
    return code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) ? 0xfffd : code;
}

static int compare(const void *a, const void *b)
{
    const Initial *x = a, *y = b;
    if (x->code != y->code) {
        return x->code < y->code ? -1 : 1;
    }
    return (x->order > y->order) - (x->order < y->order);
}

static void load(const char *path)
{
    free(table);
    table = NULL;
    count = 0;
    strcpy(source, path);
    char *text = mainui_read_text(path, 1024 * 1024);
    if (!text) {
        return;
    }
    size_t lines = 1;
    for (const char *p = text; *p; p++) {
        lines += *p == '\n';
    }
    if (lines > 65536 || !(table = calloc(lines, sizeof *table))) {
        free(text);
        return;
    }
    char *line = text;
    while (line) {
        char *next = strchr(line, '\n');
        if (next) {
            *next++ = 0;
        }
        const unsigned char *p = (const unsigned char *)line;
        uint32_t code = *p ? character(&p) : 0;
        /* The reference splits on literal spaces and uses the first byte of
         * the first pronunciation. Do not fold case or choose a later reading. */
        if (code > 127 && *p == ' ' && p[1]) {
            table[count] = (Initial){code, (uint32_t)count, p[1]};
            count++;
        }
        line = next;
    }
    qsort(table, count, sizeof *table, compare);
    free(text);
}

void mainui_pinyin(const char *sd, const char *name, char *out, size_t capacity)
{
    if (!capacity) {
        return;
    }
    out[0] = 0;
    if (!name) {
        return;
    }
    char path[4096];
    int n = snprintf(path, sizeof path, "%s/miyoo/app/py.dat", sd);
    bool valid = n > 0 && n < (int)sizeof path;
    mainui_mutex_lock(&lock);
    if (valid && strcmp(source, path)) {
        load(path);
    }
    const unsigned char *p = (const unsigned char *)name;
    size_t length = 0;
    while (*p && length + 1 < capacity) {
        uint32_t code = character(&p);
        unsigned char initial = code < 128 ? (unsigned char)code : ' ';
        if (code >= 128 && valid) {
            size_t low = 0, high = count;
            while (low < high) {
                size_t mid = low + (high - low) / 2;
                if (table[mid].code <= code) {
                    low = mid + 1;
                }
                else {
                    high = mid;
                }
            }
            if (low && table[low - 1].code == code) {
                initial = table[low - 1].initial;
            }
        }
        out[length++] = (char)initial;
    }
    out[length] = 0;
    mainui_mutex_unlock(&lock);
}
