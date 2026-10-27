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
//  - Open MainUI 1.1.0 reads an optional scroll style after the speed:
//    "<delay>,<speed>,bounce" bounces long titles back and forth; without
//    it ("<delay>,<speed>", or ",rolling") they roll round as before.
//  - Open MainUI 1.1.0 plays a game's video in the game list when
//    .romListVideoPreview exists, after the delay it holds (1000 ms when
//    empty). No file is no video. From rc4 a size can follow the delay:
//    "<delay>,fullscreen" plays it over the whole screen; without it (or
//    ",cover") it plays where the cover is.
//  - Open MainUI 1.1.0 scrolls the Game Info description by itself when
//    .gameInfoAutoScroll exists: "<delay ms>,<speed px/s>", 2000,20 when
//    empty (rc8; rc7 used 1500,40). No file is no scrolling.

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
#define GAMELISTS_VIDEO_PREVIEW_PATH GAMELISTS_CONFIG_DIR "/.romListVideoPreview"
#define GAMELISTS_INFO_SCROLL_PATH GAMELISTS_CONFIG_DIR "/.gameInfoAutoScroll"

#define GAMELISTS_ROWS_MIN 6
#define GAMELISTS_ROWS_MAX 20
#define GAMELISTS_FONT_MIN 8
#define GAMELISTS_FONT_MAX 60
#define GAMELISTS_SCROLL_OFF (-1)
#define GAMELISTS_SCROLL_DEFAULT_SPEED 50
#define GAMELISTS_SCROLL_ROLLING 0
#define GAMELISTS_SCROLL_BOUNCE 1
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

// Video preview: Off (no file), then the wait before the video starts.
#define GAMELISTS_VIDEO_OFF (-1)
#define GAMELISTS_VIDEO_DEFAULT_DELAY 1000
static const int gamelists_video_delays[] = {
    GAMELISTS_VIDEO_OFF, 0, 250, 500, 750, 1000, 1500, 2000, 3000, 5000};
#define GAMELISTS_VIDEO_COVER 0
#define GAMELISTS_VIDEO_FULLSCREEN 1
// The size chosen while the preview is Off: there is no file to keep it
// in, so it is used when a delay is chosen next.
static int gamelists_video_size_pending = GAMELISTS_VIDEO_COVER;

// Game Info auto scroll: Off (no file), then the wait before it moves.
// Open MainUI keeps the delay in 0..30000 ms and the speed in 5..150 px/s.
#define GAMELISTS_INFO_SCROLL_OFF (-1)
#define GAMELISTS_INFO_SCROLL_DEFAULT_DELAY 2000
#define GAMELISTS_INFO_SCROLL_DEFAULT_SPEED 20
static const int gamelists_info_scroll_delays[] = {
    GAMELISTS_INFO_SCROLL_OFF, 0, 500, 1000, 1500, 2000, 3000, 5000, 7500, 10000};
static const int gamelists_info_scroll_speeds[] = {
    5, 10, 15, 20, 25, 30, 40, 50, 60, 75, 100, 125, 150};
// The speed chosen while it is Off, used when a delay is chosen next.
static int gamelists_info_scroll_speed_pending = GAMELISTS_INFO_SCROLL_DEFAULT_SPEED;

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

/* ---- Title scrolling: .romListTitleScroll = "<delay ms>,<speed px/s>[,<style>]" ---- */

// Reads the file's delay and speed, and its style: "bounce" after a second
// comma, anything else (nothing, "rolling") is the rolling scroll.
static bool gamelists_read_scroll(int *delay_out, int *speed_out, int *style_out)
{
    char buffer[96];
    *style_out = GAMELISTS_SCROLL_ROLLING;
    if (!gamelists_read_line(GAMELISTS_SCROLL_PATH, buffer, sizeof(buffer)))
        return false;
    char *first_comma = strchr(buffer, ',');
    char *second_comma = first_comma ? strchr(first_comma + 1, ',') : NULL;
    if (second_comma != NULL) {
        *second_comma = '\0';
        char *style = second_comma + 1;
        while (isspace((unsigned char)*style))
            style++;
        size_t length = strcspn(style, " \t\r\n");
        if (length == 6 && strncmp(style, "bounce", 6) == 0)
            *style_out = GAMELISTS_SCROLL_BOUNCE;
    }
    return gamelists_parse_pair(buffer, delay_out, speed_out);
}

// The style saved in the file: rolling when there is none.
static int gamelists_scroll_style(void)
{
    int delay, speed, style;
    if (!gamelists_read_scroll(&delay, &speed, &style))
        return GAMELISTS_SCROLL_ROLLING;
    return style;
}

// A missing file, a negative delay or a speed of 0 is Off for Open MainUI.
static void gamelists_load_scroll(int *delay_out, int *speed_out)
{
    int delay = GAMELISTS_SCROLL_OFF;
    int speed = GAMELISTS_SCROLL_DEFAULT_SPEED;
    int style;
    if (gamelists_read_scroll(&delay, &speed, &style)) {
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
// Rolling is written without a style, as before, so launchers older than
// 1.1.0 read the file as they always did.
static bool gamelists_store_scroll_style(int delay, int speed, int style)
{
    speed = gamelists_clamp(speed, 5, 400);
    delay = delay < 0 ? GAMELISTS_SCROLL_OFF : gamelists_clamp(delay, 0, 30000);
    if (style != GAMELISTS_SCROLL_BOUNCE)
        return gamelists_write_pair(GAMELISTS_SCROLL_PATH, delay, speed);
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%d,%d,bounce\n", delay, speed);
    return gamelists_write_text(GAMELISTS_SCROLL_PATH, buffer);
}

// A new delay or speed keeps the style already chosen.
static bool gamelists_store_scroll(int delay, int speed)
{
    return gamelists_store_scroll_style(delay, speed, gamelists_scroll_style());
}

// A new style keeps the delay and speed.
static bool gamelists_set_scroll_style(int style)
{
    int delay, speed;
    gamelists_load_scroll(&delay, &speed);
    return gamelists_store_scroll_style(delay, speed, style);
}

/* ---- Video preview: .romListVideoPreview = "<delay ms>[,fullscreen]" ---- */

// No file is Off; an empty or unreadable delay is Open MainUI's 1000 ms.
static int gamelists_read_video_preview(int *size_out)
{
    *size_out = GAMELISTS_VIDEO_COVER;
    if (access(GAMELISTS_VIDEO_PREVIEW_PATH, F_OK) != 0)
        return GAMELISTS_VIDEO_OFF;
    char buffer[64];
    if (!gamelists_read_line(GAMELISTS_VIDEO_PREVIEW_PATH, buffer, sizeof(buffer)))
        return GAMELISTS_VIDEO_DEFAULT_DELAY;
    char *comma = strchr(buffer, ',');
    if (comma != NULL) {
        *comma = '\0';
        char *size = comma + 1;
        while (isspace((unsigned char)*size))
            size++;
        size_t length = strcspn(size, " \t\r\n");
        if (length == 10 && strncmp(size, "fullscreen", 10) == 0)
            *size_out = GAMELISTS_VIDEO_FULLSCREEN;
    }
    int delay;
    if (!gamelists_parse_int(buffer, &delay) || delay < 0)
        return GAMELISTS_VIDEO_DEFAULT_DELAY;
    return gamelists_clamp(delay, 0, 30000);
}

static int gamelists_load_video_preview(void)
{
    int size;
    return gamelists_read_video_preview(&size);
}

static int gamelists_video_size(void)
{
    int size;
    if (gamelists_read_video_preview(&size) == GAMELISTS_VIDEO_OFF)
        return gamelists_video_size_pending;
    return size;
}

// The cover size is written as the delay alone, which every 1.1.0 rc reads.
static bool gamelists_store_video_preview_size(int delay, int size)
{
    if (delay < 0)
        return gamelists_remove_file(GAMELISTS_VIDEO_PREVIEW_PATH);
    char buffer[32];
    delay = gamelists_clamp(delay, 0, 30000);
    if (size == GAMELISTS_VIDEO_FULLSCREEN)
        snprintf(buffer, sizeof(buffer), "%d,fullscreen\n", delay);
    else
        snprintf(buffer, sizeof(buffer), "%d\n", delay);
    return gamelists_write_text(GAMELISTS_VIDEO_PREVIEW_PATH, buffer);
}

// A new delay keeps the size; Off forgets the file, not the size.
static bool gamelists_store_video_preview(int delay)
{
    int size = gamelists_video_size();
    if (delay < 0)
        gamelists_video_size_pending = size;
    return gamelists_store_video_preview_size(delay, size);
}

// A new size keeps the delay. While the preview is Off it waits for one.
static bool gamelists_set_video_size(int size)
{
    size = size == GAMELISTS_VIDEO_FULLSCREEN ? GAMELISTS_VIDEO_FULLSCREEN : GAMELISTS_VIDEO_COVER;
    gamelists_video_size_pending = size;
    int delay = gamelists_load_video_preview();
    if (delay == GAMELISTS_VIDEO_OFF)
        return true;
    return gamelists_store_video_preview_size(delay, size);
}

/* ---- Game Info auto scroll: .gameInfoAutoScroll = "<delay ms>,<speed px/s>" ---- */

// No file is Off. An empty file is Open MainUI's 2000 ms at 20 px/s, and a
// delay alone keeps the default speed.
static void gamelists_load_info_scroll(int *delay_out, int *speed_out)
{
    *delay_out = GAMELISTS_INFO_SCROLL_OFF;
    *speed_out = gamelists_info_scroll_speed_pending;
    if (access(GAMELISTS_INFO_SCROLL_PATH, F_OK) != 0)
        return;
    int delay = GAMELISTS_INFO_SCROLL_DEFAULT_DELAY;
    int speed = GAMELISTS_INFO_SCROLL_DEFAULT_SPEED;
    char buffer[96];
    if (gamelists_read_line(GAMELISTS_INFO_SCROLL_PATH, buffer, sizeof(buffer)) &&
        !gamelists_parse_pair(buffer, &delay, &speed)) {
        speed = GAMELISTS_INFO_SCROLL_DEFAULT_SPEED;
        if (!gamelists_parse_int(buffer, &delay))
            delay = GAMELISTS_INFO_SCROLL_DEFAULT_DELAY;
    }
    *delay_out = gamelists_clamp(delay, 0, 30000);
    *speed_out = gamelists_clamp(speed, 5, 150);
}

// Off removes the file and remembers the speed for when it is on again.
static bool gamelists_store_info_scroll(int delay, int speed)
{
    speed = gamelists_clamp(speed, 5, 150);
    if (delay < 0) {
        gamelists_info_scroll_speed_pending = speed;
        return gamelists_remove_file(GAMELISTS_INFO_SCROLL_PATH);
    }
    return gamelists_write_pair(GAMELISTS_INFO_SCROLL_PATH, gamelists_clamp(delay, 0, 30000), speed);
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

static int gamelists_value_video_preview(void)
{
    return gamelists_nearest_index(gamelists_video_delays,
                                   GAMELISTS_ARRAY_COUNT(gamelists_video_delays),
                                   gamelists_load_video_preview());
}

static int gamelists_value_info_scroll_delay(void)
{
    int delay, speed;
    gamelists_load_info_scroll(&delay, &speed);
    return gamelists_nearest_index(gamelists_info_scroll_delays,
                                   GAMELISTS_ARRAY_COUNT(gamelists_info_scroll_delays), delay);
}

static int gamelists_value_info_scroll_speed(void)
{
    int delay, speed;
    gamelists_load_info_scroll(&delay, &speed);
    return gamelists_nearest_index(gamelists_info_scroll_speeds,
                                   GAMELISTS_ARRAY_COUNT(gamelists_info_scroll_speeds), speed);
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

static void gamelists_label_video_preview(int value, char *out, size_t size)
{
    int delay = gamelists_video_delays[gamelists_clamp(value, 0, GAMELISTS_ARRAY_COUNT(gamelists_video_delays) - 1)];
    if (delay < 0)
        snprintf(out, size, "Off");
    else if (delay == 0)
        snprintf(out, size, "Immediate");
    else
        snprintf(out, size, "After %d ms", delay);
}

static void gamelists_label_info_scroll_delay(int value, char *out, size_t size)
{
    int delay = gamelists_info_scroll_delays[gamelists_clamp(value, 0, GAMELISTS_ARRAY_COUNT(gamelists_info_scroll_delays) - 1)];
    if (delay < 0)
        snprintf(out, size, "Off");
    else if (delay == 0)
        snprintf(out, size, "Immediate");
    else
        snprintf(out, size, "After %d ms", delay);
}

static void gamelists_label_info_scroll_speed(int value, char *out, size_t size)
{
    snprintf(out, size, "%d px/s",
             gamelists_info_scroll_speeds[gamelists_clamp(value, 0, GAMELISTS_ARRAY_COUNT(gamelists_info_scroll_speeds) - 1)]);
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
