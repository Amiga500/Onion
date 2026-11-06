/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/media_text.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool regular(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0 && S_ISREG(info.st_mode);
}

bool mainui_manual_find(const char *rom_root, const char *rom, const char *suffix, char path[4096])
{
    if (!path) {
        return false;
    }
    path[0] = 0;
    if (!rom_root || !*rom_root || !rom || !suffix ||
        (strcmp(suffix, ".txt") && strcmp(suffix, ".pdf"))) {
        return false;
    }
    const char *name = strrchr(rom, '/');
    name = name ? name + 1 : rom;
    size_t length = strlen(name);
    if (!length || length >= 1024) {
        return false;
    }
    char stem[1024];
    memcpy(stem, name, length + 1);
    char *dot = strrchr(stem, '.');
    if (dot && dot != stem) {
        *dot = 0;
    }
    if (!*stem || !strcmp(stem, ".") || !strcmp(stem, "..")) {
        return false;
    }
    int n = snprintf(path, 4096, "%s/media/manuals/%s%s", rom_root, stem, suffix);
    if (n < 0 || n >= 4096 || !regular(path)) {
        path[0] = 0;
        return false;
    }
    return true;
}

bool mainui_text_find(const char *rom_root, const char *rom, char path[4096])
{
    return mainui_manual_find(rom_root, rom, ".txt", path);
}

static size_t utf8_size(const unsigned char *data, size_t length)
{
    unsigned char c = data[0];
    if (c < 0x80) {
        return 1;
    }
    size_t size = c >= 0xc2 && c <= 0xdf   ? 2
                  : c >= 0xe0 && c <= 0xef ? 3
                  : c >= 0xf0 && c <= 0xf4 ? 4
                                           : 0;
    if (!size || size > length) {
        return 0;
    }
    for (size_t i = 1; i < size; i++) {
        if ((data[i] & 0xc0) != 0x80) {
            return 0;
        }
    }
    if ((c == 0xe0 && data[1] < 0xa0) || (c == 0xed && data[1] >= 0xa0) ||
        (c == 0xf0 && data[1] < 0x90) || (c == 0xf4 && data[1] >= 0x90)) {
        return 0;
    }
    return size;
}

bool mainui_text_open(MainUITextReader *reader, const char *path)
{
    if (!reader) {
        return false;
    }
    mainui_text_close(reader);
    if (!path || !regular(path)) {
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    struct stat statbuf;
    if (fstat(fileno(file), &statbuf) || !S_ISREG(statbuf.st_mode) || statbuf.st_size < 0) {
        fclose(file);
        return false;
    }
    size_t size = (unsigned long long)statbuf.st_size > MAINUI_TEXT_FILE_LIMIT
                      ? MAINUI_TEXT_FILE_LIMIT
                      : (size_t)statbuf.st_size;
    reader->data = malloc(size + 1);
    if (!reader->data) {
        fclose(file);
        return false;
    }
    size_t got = fread(reader->data, 1, size, file);
    bool error = ferror(file) != 0;
    fclose(file);
    if (error) {
        mainui_text_close(reader);
        return false;
    }
    reader->bytes = got;
    reader->truncated = (unsigned long long)statbuf.st_size > got;
    /* Preserve valid UTF-8; normalize controls and invalid input in place.
     * A torn multibyte sequence at the read limit becomes '?'. */
    for (size_t i = 0; i < got;) {
        unsigned char c = (unsigned char)reader->data[i];
        if (c == '\t' || c == 0 || (c < 32 && c != '\r' && c != '\n')) {
            reader->data[i++] = ' ';
        }
        else if (c < 0x80) {
            i++;
        }
        else {
            size_t sequence = utf8_size((const unsigned char *)reader->data + i, got - i);
            if (!sequence) {
                reader->data[i++] = '?';
            }
            else {
                i += sequence;
            }
        }
    }
    reader->data[got] = 0;
    /* Skip an optional UTF-8 BOM, never expose it as a visible character. */
    if (got >= 3 && !memcmp(reader->data, "\xef\xbb\xbf", 3)) {
        reader->cursor = 3;
    }
    return true;
}

void mainui_text_close(MainUITextReader *reader)
{
    if (!reader) {
        return;
    }
    free(reader->data);
    free(reader->lines);
    *reader = (MainUITextReader){0};
}

/* Generate a single visual row. Its source span is bounded by 1023 bytes,
 * and the last row of a physical line consumes CR/LF, including CRLF. */
static bool next_line(MainUITextReader *reader, int width, int (*measure)(const char *, void *),
                      void *context)
{
    if (reader->cursor >= reader->bytes || reader->count >= MAINUI_TEXT_MAX_LINES) {
        if (reader->count >= MAINUI_TEXT_MAX_LINES) {
            reader->truncated = true;
        }
        reader->complete = true;
        return false;
    }
    if (reader->count == reader->capacity) {
        int capacity = reader->capacity ? reader->capacity * 2 : 128;
        if (capacity > MAINUI_TEXT_MAX_LINES) {
            capacity = MAINUI_TEXT_MAX_LINES;
        }
        MainUITextLine *lines = realloc(reader->lines, (size_t)capacity * sizeof *lines);
        if (!lines) {
            reader->complete = true;
            reader->truncated = true;
            return false;
        }
        reader->lines = lines;
        reader->capacity = capacity;
    }
    size_t start = reader->cursor;
    size_t span = 0;
    /* Offsets within one line, below MAINUI_TEXT_LINE_BYTES: 16 bits suffice. */
    unsigned short boundaries[MAINUI_TEXT_LINE_BYTES];
    int points = 0;
    boundaries[points++] = 0;
    while (start + span < reader->bytes && span < MAINUI_TEXT_LINE_BYTES - 4 &&
           reader->data[start + span] != '\n' && reader->data[start + span] != '\r') {
        size_t advance = utf8_size((const unsigned char *)reader->data + start + span,
                                   reader->bytes - start - span);
        if (!advance) {
            advance = 1; /* already sanitized by open() */
        }
        span += advance;
        boundaries[points++] = (unsigned short)span;
    }
    char text[MAINUI_TEXT_LINE_BYTES];
    memcpy(text, reader->data + start, span);
    text[span] = 0;
    int chosen = points - 1;
    if (chosen && measure(text, context) > width) {
        int low = 1, high = chosen;
        chosen = 1; /* progress even if one glyph is wider than the viewport */
        while (low <= high) {
            int mid = low + (high - low) / 2;
            char previous = text[boundaries[mid]];
            text[boundaries[mid]] = 0;
            int pixels = measure(text, context);
            text[boundaries[mid]] = previous;
            if (pixels <= width) {
                chosen = mid;
                low = mid + 1;
            }
            else {
                high = mid - 1;
            }
        }
    }
    size_t end = start + boundaries[chosen], next = end;
    if (chosen < points - 1 ||
        (start + span < reader->bytes && reader->data[start + span] != '\n' &&
         reader->data[start + span] != '\r')) {
        /* Prefer word breaks, but cut a very long word on a UTF-8 boundary. */
        size_t word = end;
        while (word > start && reader->data[word - 1] != ' ') {
            word--;
        }
        if (word > start) {
            end = word;
            while (end > start && reader->data[end - 1] == ' ') {
                end--;
            }
            next = word;
            while (next < reader->bytes && reader->data[next] == ' ') {
                next++;
            }
        }
    }
    if (next == start + span && next < reader->bytes) {
        if (reader->data[next] == '\r') {
            next++;
        }
        if (next < reader->bytes && reader->data[next] == '\n') {
            next++;
        }
    }
    reader->lines[reader->count++] = (MainUITextLine){start, end};
    reader->cursor = next;
    if (reader->count == MAINUI_TEXT_MAX_LINES) {
        reader->complete = true;
        if (reader->cursor < reader->bytes) {
            reader->truncated = true;
        }
    }
    return true;
}

void mainui_text_layout(MainUITextReader *reader, int width, int (*measure)(const char *, void *),
                        void *context, int wanted)
{
    if (!reader || !reader->data || !measure || width <= 0) {
        return;
    }
    if (reader->width != width) {
        reader->width = width;
        reader->count = reader->top = 0;
        reader->cursor = reader->bytes >= 3 && !memcmp(reader->data, "\xef\xbb\xbf", 3) ? 3 : 0;
        reader->complete = false;
    }
    if (wanted > MAINUI_TEXT_MAX_LINES) {
        wanted = MAINUI_TEXT_MAX_LINES;
    }
    while (reader->count < wanted && !reader->complete) {
        if (!next_line(reader, width, measure, context)) {
            break;
        }
    }
}

void mainui_text_line(const MainUITextReader *reader, int index,
                      char output[MAINUI_TEXT_LINE_BYTES])
{
    output[0] = 0;
    if (!reader || index < 0 || index >= reader->count) {
        return;
    }
    MainUITextLine line = reader->lines[index];
    size_t size = line.end - line.start;
    if (size >= MAINUI_TEXT_LINE_BYTES) {
        size = MAINUI_TEXT_LINE_BYTES - 1;
    }
    memcpy(output, reader->data + line.start, size);
    output[size] = 0;
}

int mainui_text_match_segments(const MainUITextReader *reader, int index, const char *term,
                               MainUITextLine *spans, int max)
{
    if (!reader || !reader->data || index < 0 || index >= reader->count || !term || !*term ||
        !spans || max <= 0) {
        return 0;
    }
    MainUITextLine visual = reader->lines[index];
    size_t length = strlen(term);
    if (length > reader->bytes || visual.end <= visual.start) {
        return 0;
    }
    size_t first = visual.start >= length - 1 ? visual.start - length + 1 : 0;
    int matches = 0;
    for (size_t i = first; i < visual.end && i <= reader->bytes - length; i++) {
        size_t j = 0;
        while (j < length &&
               tolower((unsigned char)reader->data[i + j]) == tolower((unsigned char)term[j])) {
            j++;
        }
        if (j != length) {
            continue;
        }
        size_t begin = i > visual.start ? i : visual.start;
        size_t end = i + length < visual.end ? i + length : visual.end;
        if (end > begin) {
            spans[matches++] = (MainUITextLine){begin, end};
            if (matches == max) {
                break;
            }
        }
        i += length - 1; /* matching the same phrase repeatedly isn't needed */
    }
    return matches;
}
