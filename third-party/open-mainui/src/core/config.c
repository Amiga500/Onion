/* SPDX-License-Identifier: GPL-3.0-only
 * Contracts: patcher 1.2 rom_rows/font/key_repeat/title_scroll_config_model.
 */
#include "core/core.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Keep the C/ASCII whitespace contract independent of the host locale. */
static bool whitespace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' || ch == '\f';
}

/* Saturation defines overflow instead of depending on undefined atoi overflow. */
static int integer(const char *text, size_t limit, size_t *consumed, bool *valid)
{
    size_t i = 0;
    unsigned value = 0;
    bool negative = false;
    while (i < limit && whitespace(text[i])) {
        i++;
    }
    if (i < limit && (text[i] == '+' || text[i] == '-')) {
        negative = text[i++] == '-';
    }
    size_t first = i;
    while (i < limit && text[i] >= '0' && text[i] <= '9') {
        unsigned digit = (unsigned)(text[i++] - '0');
        value = value > ((unsigned)INT_MAX - digit) / 10 ? (unsigned)INT_MAX : value * 10 + digit;
    }
    if (consumed) {
        *consumed = i;
    }
    if (valid) {
        *valid = i > first;
    }
    return negative ? -(int)value : (int)value;
}

static int clamp(int n, int low, int high)
{
    return n < low ? low : n > high ? high : n;
}

static size_t bounded_length(const char *s, size_t max)
{
    size_t n = 0;
    if (s) {
        while (n < max && s[n]) {
            n++;
        }
    }
    return n;
}

void mainui_config_parse(MainUIConfig *out, const char *rows, const char *font, const char *repeat,
                         const char *scroll)
{
    *out = (MainUIConfig){.rows = 6,
                          .row_height = 60,
                          .repeat_delay = 500,
                          .repeat_interval = 100,
                          .scroll_status = 1};
    if (rows) {
        out->rows = clamp(integer(rows, bounded_length(rows, 12), NULL, NULL), 6, 20);
    }
    out->row_height = 360 / out->rows;
    if (font) {
        int size = integer(font, bounded_length(font, 12), NULL, NULL);
        if (size > 0) {
            out->font_size = clamp(size, 8, 60);
        }
    }
    /* The reference reads only 31 bytes and uses the FIRST comma or space.
     * Do not replace this with a forgiving tokenizer: doubled separators and
     * leading spaces have observable behavior in existing configuration files.
     */
    if (repeat) {
        size_t length = bounded_length(repeat, 31), delimiter = 0;
        while (delimiter < length && repeat[delimiter] != ',' && repeat[delimiter] != ' ') {
            delimiter++;
        }
        if (delimiter < length) {
            int delay = integer(repeat, length, NULL, NULL);
            int interval = integer(repeat + delimiter + 1, length - delimiter - 1, NULL, NULL);
            if (delay > 0 && interval > 0) {
                out->repeat_delay = clamp(delay, 100, 2000);
                out->repeat_interval = clamp(interval, 30, 500);
            }
        }
    }
    /* Scrolling uses a different grammar from key repeat: multiple comma/space
     * separators are accepted. Preserve disabled negative/zero parsed values
     * so differential tests can compare the full reference-model result.
     */
    if (scroll) {
        size_t length = bounded_length(scroll, 4096), pos;
        bool first, second;
        int delay = integer(scroll, length, &pos, &first);
        size_t separator = pos;
        while (pos < length && (scroll[pos] == ',' || scroll[pos] == ' ')) {
            pos++;
        }
        /* Match the patcher's model: the second sign/digit immediately follows separators. */
        if (first && pos > separator && pos < length && !whitespace(scroll[pos])) {
            int speed = integer(scroll + pos, length - pos, NULL, &second);
            if (second) {
                out->scroll_delay = delay;
                out->scroll_speed = speed;
                if (delay >= 0 && speed > 0) {
                    out->scroll_status = 2;
                    out->scroll_delay = clamp(delay, 10, 30000);
                    out->scroll_speed = clamp(speed, 5, 400);
                }
            }
        }
    }
}

/* Match bounded prefix reads, including successful reads of empty markers.
 * Failure selects the corresponding baseline default; this loader never writes
 * settings or attempts to repair the user's files.
 */
static bool config_read(const char *dir, const char *name, char *out, size_t size)
{
    char path[4096];
    int n = snprintf(path, sizeof path, "%s/%s", dir, name);
    if (n < 0 || (size_t)n >= sizeof path) {
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    size_t bytes = fread(out, 1, size - 1, file);
    bool ok = !ferror(file);
    fclose(file);
    out[bytes] = 0;
    return ok;
}

void mainui_config_load(MainUIConfig *out, const char *directory)
{
    char rows[13], font[13], repeat[32], scroll[4097], flag[2];
    bool r = config_read(directory, ".romListRows", rows, sizeof rows);
    bool f = config_read(directory, ".romListFontSize", font, sizeof font);
    bool k = config_read(directory, ".mainUIKeyRepeat", repeat, sizeof repeat);
    bool s = config_read(directory, ".romListTitleScroll", scroll, sizeof scroll);
    mainui_config_parse(out, r ? rows : NULL, f ? font : NULL, k ? repeat : NULL,
                        s ? scroll : NULL);
    out->case_sensitive = config_read(directory, ".romListCaseSensitiveSort", flag, sizeof flag);
    out->dynamic_favorite_position =
        config_read(directory, ".romListDynamicFavPos", flag, sizeof flag);
    out->show_recents = config_read(directory, ".showRecents", flag, sizeof flag);
    out->show_expert = config_read(directory, ".showExpert", flag, sizeof flag);
}
