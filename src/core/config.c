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

/* The optional third value after the speed: "rolling" (the default) or
 * "bounce", after at least one comma or space. Anything else rolls, as every
 * earlier version did with whatever followed the speed. */
static bool scroll_style_bounce(const char *text, size_t length)
{
    static const char word[] = "bounce";
    size_t pos = 0;
    while (pos < length && (text[pos] == ',' || text[pos] == ' ')) {
        pos++;
    }
    if (!pos || length - pos < sizeof word - 1) {
        return false;
    }
    for (size_t i = 0; i < sizeof word - 1; i++) {
        char ch = text[pos + i];
        if ((ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch) != word[i]) {
            return false;
        }
    }
    pos += sizeof word - 1;
    return pos == length || whitespace(text[pos]) || text[pos] == ',';
}

/* The next word of text from *pos, after commas and whitespace: its start
 * and length (0 at the end). */
static size_t next_word(const char *text, size_t length, size_t *pos)
{
    while (*pos < length && (text[*pos] == ',' || whitespace(text[*pos]))) {
        ++*pos;
    }
    size_t end = *pos;
    while (end < length && text[end] != ',' && !whitespace(text[end])) {
        end++;
    }
    return end - *pos;
}

static bool word_is(const char *text, size_t length, const char *word)
{
    if (strlen(word) != length) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        char ch = text[i];
        if ((ch >= 'A' && ch <= 'Z' ? ch - 'A' + 'a' : ch) != word[i]) {
            return false;
        }
    }
    return true;
}

int mainui_config_video_preview(const char *text, bool *fullscreen)
{
    *fullscreen = false;
    if (!text) {
        return -1;
    }
    size_t length = bounded_length(text, 31), pos = 0, used;
    size_t size = next_word(text, length, &pos);
    bool valid;
    int delay = size ? integer(text + pos, size, &used, &valid) : 0;
    /* A delay, then a mode; or the mode alone, after 1000 ms. */
    if (size && valid && used == size) {
        delay = clamp(delay, 0, 30000);
        pos += size;
        size = next_word(text, length, &pos);
    }
    else {
        delay = 1000;
    }
    *fullscreen = word_is(text + pos, size, "fullscreen");
    /* Fullscreen takes the whole screen: at least 100 ms, so moving
     * through the list never flashes one up. */
    return *fullscreen && delay < 100 ? 100 : delay;
}

void mainui_config_parse(MainUIConfig *out, const char *rows, const char *font, const char *repeat,
                         const char *scroll)
{
    *out = (MainUIConfig){.rows = 6,
                          .row_height = 60,
                          .repeat_delay = 500,
                          .repeat_interval = 100,
                          .scroll_status = 1,
                          .context_ellipsis = true,
                          .video_preview_delay = -1};
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
            size_t used;
            int speed = integer(scroll + pos, length - pos, &used, &second);
            if (second) {
                out->scroll_delay = delay;
                out->scroll_speed = speed;
                if (delay >= 0 && speed > 0) {
                    out->scroll_status = 2;
                    out->scroll_delay = clamp(delay, 10, 30000);
                    out->scroll_speed = clamp(speed, 5, 400);
                }
                out->scroll_bounce = scroll_style_bounce(scroll + pos + used, length - pos - used);
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

void mainui_config_details_auto(const char *text, int *delay, int *speed)
{
    *delay = 0;
    *speed = 0;
    if (!text) {
        return;
    }
    *delay = 2000;
    *speed = 20;
    size_t length = bounded_length(text, 63), pos = 0, used;
    size_t size = next_word(text, length, &pos);
    bool valid;
    if (size) {
        int value = integer(text + pos, size, &used, &valid);
        if (valid && used == size) {
            *delay = clamp(value, 0, 30000);
            pos += size;
            size = next_word(text, length, &pos);
            if (size) {
                value = integer(text + pos, size, &used, &valid);
                if (valid && used == size) {
                    *speed = clamp(value, 5, 150);
                }
            }
        }
    }
}

void mainui_config_load(MainUIConfig *out, const char *directory)
{
    char rows[13], font[13], repeat[32], scroll[4097], flag[2], video[32];
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
    out->context_ellipsis = !config_read(directory, ".noContextMenuEllipsis", flag, sizeof flag);
    out->footer_alt = config_read(directory, ".mainuiFooterAltLayout", flag, sizeof flag);
    char details[64];
    mainui_config_details_auto(
        config_read(directory, ".gameInfoAutoScroll", details, sizeof details) ? details : NULL,
        &out->details_auto_delay, &out->details_auto_speed);
    out->video_preview_delay = mainui_config_video_preview(
        config_read(directory, ".romListVideoPreview", video, sizeof video) ? video : NULL,
        &out->video_preview_fullscreen);
}
