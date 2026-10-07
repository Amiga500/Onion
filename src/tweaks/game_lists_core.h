#ifndef TWEAKS_GAME_LISTS_CORE_H__
#define TWEAKS_GAME_LISTS_CORE_H__

// SPDX-License-Identifier: GPL-3.0-only
//
// File handling behind Tweaks > Appearance > Game lists: the runtime
// configuration files Open MainUI reads for its game lists. Adapted from
// robcodedev/onionos-mainui-patcher (tools/src/tweaks/game_lists.h), split
// out of the menu so it can be tested without SDL. Changes for OnionPlus:
//  - Title scrolling Off keeps .romListTitleScroll (as "-1,<speed>") instead
//    of removing it: updates add a missing file back with its default value,
//    which would turn scrolling on again.
//  - Scroll speeds are the ones Open MainUI renders exactly (see its
//    docs/TIMING.md): 5 px/s, then multiples of 25 px/s.
//  - Each write uses its own temporary file, so two writers never share one.

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef GAMELISTS_CONFIG_DIR
#define GAMELISTS_CONFIG_DIR "/mnt/SDCARD/.tmp_update/config"
#endif
#define GAMELISTS_ROWS_PATH GAMELISTS_CONFIG_DIR "/.romListRows"
#define GAMELISTS_FONT_PATH GAMELISTS_CONFIG_DIR "/.romListFontSize"
#define GAMELISTS_SCROLL_PATH GAMELISTS_CONFIG_DIR "/.romListTitleScroll"
#define GAMELISTS_REPEAT_PATH GAMELISTS_CONFIG_DIR "/.mainUIKeyRepeat"
#define GAMELISTS_SORT_PATH GAMELISTS_CONFIG_DIR "/.romListCaseSensitiveSort"
#define GAMELISTS_DYNAMIC_FAV_PATH GAMELISTS_CONFIG_DIR "/.romListDynamicFavPos"

#define GAMELISTS_ROWS_MIN 6
#define GAMELISTS_ROWS_MAX 20
#define GAMELISTS_FONT_MIN 8
#define GAMELISTS_FONT_MAX 60
#define GAMELISTS_SCROLL_OFF (-1)
#define GAMELISTS_SCROLL_DEFAULT_SPEED 50
#define GAMELISTS_REPEAT_STOCK_DELAY 500
#define GAMELISTS_REPEAT_STOCK_INTERVAL 100

static const int gamelists_scroll_delays[] = {
    GAMELISTS_SCROLL_OFF, 0, 100, 200, 300, 400, 500, 600, 700, 800, 900,
    1000, 1250, 1500, 1750, 2000, 2500, 3000, 4000, 5000, 7500, 10000,
    15000, 20000, 30000};

// Open MainUI moves titles a whole number of pixels every 40 ms frame, so
// only these speeds are shown as set (below 25 px/s it can also do 12.5,
// 8.33 and 6.25, which are not whole numbers).
static const int gamelists_scroll_speeds[] = {
    5, 25, 50, 75, 100, 125, 150, 175, 200, 225, 250, 275, 300, 325, 350,
    375, 400};

static const int gamelists_repeat_intervals[] = {
    30, 40, 50, 60, 70, 80, 90, 100, 120, 150, 175, 200, 250, 300, 400, 500};

#define GAMELISTS_ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

static int gamelists_clamp(int value, int minimum, int maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static int gamelists_nearest_index(const int *values, int count, int target)
{
    int best_index = 0;
    long best_distance = labs((long)target - values[0]);
    for (int i = 1; i < count; i++) {
        long distance = labs((long)target - values[i]);
        if (distance < best_distance) {
            best_index = i;
            best_distance = distance;
        }
    }
    return best_index;
}

static bool gamelists_parse_int(const char *text, int *value_out)
{
    if (text == NULL || value_out == NULL)
        return false;
    while (isspace((unsigned char)*text))
        text++;
    if (*text == '\0')
        return false;

    errno = 0;
    char *end = NULL;
    long parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || parsed < INT_MIN || parsed > INT_MAX)
        return false;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;

    *value_out = (int)parsed;
    return true;
}

// "first,second" or "first second", as Open MainUI accepts them.
static bool gamelists_parse_pair(const char *text, int *first_out, int *second_out)
{
    if (text == NULL || first_out == NULL || second_out == NULL)
        return false;

    const char *cursor = text;
    while (isspace((unsigned char)*cursor))
        cursor++;

    errno = 0;
    char *end = NULL;
    long first = strtol(cursor, &end, 10);
    if (errno != 0 || end == cursor || first < INT_MIN || first > INT_MAX)
        return false;

    cursor = end;
    while (isspace((unsigned char)*cursor))
        cursor++;
    if (*cursor == ',') {
        cursor++;
        while (isspace((unsigned char)*cursor))
            cursor++;
    }
    else if (cursor == end) {
        return false;
    }

    errno = 0;
    long second = strtol(cursor, &end, 10);
    if (errno != 0 || end == cursor || second < INT_MIN || second > INT_MAX)
        return false;
    while (isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;

    *first_out = (int)first;
    *second_out = (int)second;
    return true;
}

static bool gamelists_read_line(const char *path, char *buffer, size_t size)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return false;
    bool ok = fgets(buffer, (int)size, fp) != NULL;
    if (fclose(fp) != 0)
        ok = false;
    return ok;
}

static bool gamelists_read_int(const char *path, int *value_out)
{
    char buffer[64];
    return gamelists_read_line(path, buffer, sizeof(buffer)) &&
           gamelists_parse_int(buffer, value_out);
}

static bool gamelists_read_pair(const char *path, int *first_out, int *second_out)
{
    char buffer[96];
    return gamelists_read_line(path, buffer, sizeof(buffer)) &&
           gamelists_parse_pair(buffer, first_out, second_out);
}

static bool gamelists_ensure_config_dir(void)
{
    if (mkdir(GAMELISTS_CONFIG_DIR, 0755) == 0)
        return true;
    if (errno != EEXIST)
        return false;
    struct stat st;
    return stat(GAMELISTS_CONFIG_DIR, &st) == 0 && S_ISDIR(st.st_mode);
}

// Write through a temporary file of this process, then rename it in place.
static bool gamelists_write_text(const char *path, const char *text)
{
    if (!gamelists_ensure_config_dir())
        return false;

    char temporary[512];
    int n = snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", path, (long)getpid());
    if (n < 0 || n >= (int)sizeof(temporary))
        return false;

    FILE *fp = fopen(temporary, "w");
    if (fp == NULL)
        return false;

    size_t length = strlen(text);
    bool ok = fwrite(text, 1, length, fp) == length;
    if (ok)
        ok = fflush(fp) == 0;
    if (ok)
        ok = fsync(fileno(fp)) == 0;
    if (fclose(fp) != 0)
        ok = false;

    if (!ok || rename(temporary, path) != 0) {
        remove(temporary);
        return false;
    }
    return true;
}

static bool gamelists_write_int(const char *path, int value)
{
    char buffer[48];
    snprintf(buffer, sizeof(buffer), "%d\n", value);
    return gamelists_write_text(path, buffer);
}

static bool gamelists_write_pair(const char *path, int first, int second)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d,%d\n", first, second);
    return gamelists_write_text(path, buffer);
}

static bool gamelists_remove_file(const char *path)
{
    return remove(path) == 0 || errno == ENOENT;
}

/* ---- Title scrolling: .romListTitleScroll = "<delay ms>,<speed px/s>" ---- */

// A missing file, a negative delay or a speed of 0 is Off for Open MainUI.
static void gamelists_load_scroll(int *delay_out, int *speed_out)
{
    int delay = GAMELISTS_SCROLL_OFF;
    int speed = GAMELISTS_SCROLL_DEFAULT_SPEED;
    if (gamelists_read_pair(GAMELISTS_SCROLL_PATH, &delay, &speed)) {
        if (delay < 0 || speed <= 0) {
            delay = GAMELISTS_SCROLL_OFF;
            speed = speed > 0 ? gamelists_clamp(speed, 5, 400) : GAMELISTS_SCROLL_DEFAULT_SPEED;
        }
        else {
            delay = gamelists_clamp(delay, 0, 30000);
            speed = gamelists_clamp(speed, 5, 400);
        }
    }
    else {
        delay = GAMELISTS_SCROLL_OFF;
        speed = GAMELISTS_SCROLL_DEFAULT_SPEED;
    }
    *delay_out = delay;
    *speed_out = speed;
}

// Always keeps the file, also for Off ("-1,<speed>"), so an update can't
// bring the default back and the chosen speed survives Off and On again.
static bool gamelists_store_scroll(int delay, int speed)
{
    speed = gamelists_clamp(speed, 5, 400);
    delay = delay < 0 ? GAMELISTS_SCROLL_OFF : gamelists_clamp(delay, 0, 30000);
    return gamelists_write_pair(GAMELISTS_SCROLL_PATH, delay, speed);
}

/* ---- Key repeat: .mainUIKeyRepeat = "<delay ms>,<interval ms>" ---- */

static void gamelists_load_repeat(int *delay_out, int *interval_out)
{
    int delay = GAMELISTS_REPEAT_STOCK_DELAY;
    int interval = GAMELISTS_REPEAT_STOCK_INTERVAL;
    if (!gamelists_read_pair(GAMELISTS_REPEAT_PATH, &delay, &interval) ||
        delay <= 0 || interval <= 0) {
        delay = GAMELISTS_REPEAT_STOCK_DELAY;
        interval = GAMELISTS_REPEAT_STOCK_INTERVAL;
    }
    *delay_out = gamelists_clamp(delay, 100, 2000);
    *interval_out = gamelists_clamp(interval, 30, 500);
}

// The stock values remove the file (no default copy of it is shipped).
static bool gamelists_store_repeat(int delay, int interval)
{
    delay = gamelists_clamp(delay, 100, 2000);
    interval = gamelists_clamp(interval, 30, 500);
    if (delay == GAMELISTS_REPEAT_STOCK_DELAY && interval == GAMELISTS_REPEAT_STOCK_INTERVAL)
        return gamelists_remove_file(GAMELISTS_REPEAT_PATH);
    return gamelists_write_pair(GAMELISTS_REPEAT_PATH, delay, interval);
}

/* ---- Menu values (list indexes) ---- */

static int gamelists_rows(void)
{
    int rows = GAMELISTS_ROWS_MIN;
    if (!gamelists_read_int(GAMELISTS_ROWS_PATH, &rows))
        rows = GAMELISTS_ROWS_MIN;
    return gamelists_clamp(rows, GAMELISTS_ROWS_MIN, GAMELISTS_ROWS_MAX);
}

static int gamelists_value_rows(void) { return gamelists_rows() - GAMELISTS_ROWS_MIN; }

// 0 = the theme's size, otherwise 8..60 px.
static int gamelists_value_font(void)
{
    int size = 0;
    if (!gamelists_read_int(GAMELISTS_FONT_PATH, &size) || size <= 0)
        return 0;
    return gamelists_clamp(size, GAMELISTS_FONT_MIN, GAMELISTS_FONT_MAX) - GAMELISTS_FONT_MIN + 1;
}

static int gamelists_value_scroll_delay(void)
{
    int delay, speed;
    gamelists_load_scroll(&delay, &speed);
    return gamelists_nearest_index(gamelists_scroll_delays,
                                   GAMELISTS_ARRAY_COUNT(gamelists_scroll_delays), delay);
}

static int gamelists_value_scroll_speed(void)
{
    int delay, speed;
    gamelists_load_scroll(&delay, &speed);
    return gamelists_nearest_index(gamelists_scroll_speeds,
                                   GAMELISTS_ARRAY_COUNT(gamelists_scroll_speeds), speed);
}

static int gamelists_value_repeat_delay(void)
{
    int delay, interval;
    gamelists_load_repeat(&delay, &interval);
    return (delay - 100 + 25) / 50;
}

static int gamelists_value_repeat_interval(void)
{
    int delay, interval;
    gamelists_load_repeat(&delay, &interval);
    return gamelists_nearest_index(gamelists_repeat_intervals,
                                   GAMELISTS_ARRAY_COUNT(gamelists_repeat_intervals), interval);
}

static int gamelists_value_sort(void) { return access(GAMELISTS_SORT_PATH, F_OK) == 0; }

static int gamelists_value_fixed_favorite_position(void)
{
    return access(GAMELISTS_DYNAMIC_FAV_PATH, F_OK) != 0;
}

/* ---- Labels ---- */

static void gamelists_label_rows(int value, char *out, size_t size)
{
    int rows = value + GAMELISTS_ROWS_MIN;
    if (rows == GAMELISTS_ROWS_MIN)
        snprintf(out, size, "%d (stock)", rows);
    else
        snprintf(out, size, "%d rows", rows);
}

static void gamelists_label_font(int value, char *out, size_t size)
{
    if (value <= 0)
        snprintf(out, size, "Theme default");
    else
        snprintf(out, size, "%d px", value + GAMELISTS_FONT_MIN - 1);
}

static void gamelists_label_scroll_delay(int value, char *out, size_t size)
{
    int delay = gamelists_scroll_delays[gamelists_clamp(value, 0, GAMELISTS_ARRAY_COUNT(gamelists_scroll_delays) - 1)];
    if (delay < 0)
        snprintf(out, size, "Off");
    else if (delay == 0)
        snprintf(out, size, "Immediate");
    else
        snprintf(out, size, "%d ms", delay);
}

static void gamelists_label_scroll_speed(int value, char *out, size_t size)
{
    snprintf(out, size, "%d px/s",
             gamelists_scroll_speeds[gamelists_clamp(value, 0, GAMELISTS_ARRAY_COUNT(gamelists_scroll_speeds) - 1)]);
}

static void gamelists_label_repeat_delay(int value, char *out, size_t size)
{
    int delay = 100 + value * 50;
    if (delay == GAMELISTS_REPEAT_STOCK_DELAY)
        snprintf(out, size, "%d ms (stock)", delay);
    else
        snprintf(out, size, "%d ms", delay);
}

static void gamelists_label_repeat_interval(int value, char *out, size_t size)
{
    int interval = gamelists_repeat_intervals[gamelists_clamp(value, 0, GAMELISTS_ARRAY_COUNT(gamelists_repeat_intervals) - 1)];
    if (interval == GAMELISTS_REPEAT_STOCK_INTERVAL)
        snprintf(out, size, "%d ms (stock)", interval);
    else
        snprintf(out, size, "%d ms", interval);
}

#endif // TWEAKS_GAME_LISTS_CORE_H__
