#ifndef TWEAKS_GAME_LISTS_H__
#define TWEAKS_GAME_LISTS_H__

// SPDX-License-Identifier: GPL-3.0-only
//
// Tweaks > Appearance > Game lists: the game-list settings Open MainUI reads.
// Adapted from robcodedev/onionos-mainui-patcher (tools/src/tweaks); the file
// handling is in game_lists_core.h. Not taken over: "Rebuild caches after case
// change", since Open MainUI applies the sort order when it reads a cache, so
// a cache never needs rebuilding for it.

#include <sys/types.h>
#include <sys/wait.h>

#include "components/list.h"
#include "system/settings.h"
#include "utils/config.h"

#include "./appstate.h"
#include "./game_lists_core.h"

#define GAMELISTS_ROMWINIDX_PATH "/appconfigs/romwinidx.json"
#define GAMELISTS_THEME_RESCALE_SCRIPT "/mnt/SDCARD/.tmp_update/script/rescale_theme_list_icons.sh"
#define GAMELISTS_PREF_RESCALE_ICONS "gameLists/rescaleThemeListIcons"

static bool gamelists_rescale_pending = false;

static int gamelists_value_rescale_icons(void)
{
    int value = 1;
    if (!config_get(GAMELISTS_PREF_RESCALE_ICONS, CONFIG_INT, &value))
        return 1;
    return value != 0;
}

// Row height Open MainUI uses for a row count (docs/THEMES.md).
static int gamelists_row_height_for_rows(int rows)
{
    return 360 / gamelists_clamp(rows, GAMELISTS_ROWS_MIN, GAMELISTS_ROWS_MAX);
}

static bool gamelists_run_theme_rescale(int rows)
{
    if (settings.theme[0] == '\0')
        return false;

    char height[16], row_count[16];
    snprintf(height, sizeof(height), "%d", gamelists_row_height_for_rows(rows));
    snprintf(row_count, sizeof(row_count), "%d", rows);

    pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0) {
        execl("/bin/sh", "sh", GAMELISTS_THEME_RESCALE_SCRIPT, settings.theme, height,
              row_count, (char *)NULL);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void gamelists_formatter_rows(void *pt, char *out_label)
{
    gamelists_label_rows(((ListItem *)pt)->value, out_label, STR_MAX);
}

static void gamelists_formatter_font(void *pt, char *out_label)
{
    gamelists_label_font(((ListItem *)pt)->value, out_label, STR_MAX);
}

static void gamelists_formatter_scroll_delay(void *pt, char *out_label)
{
    gamelists_label_scroll_delay(((ListItem *)pt)->value, out_label, STR_MAX);
}

static void gamelists_formatter_scroll_speed(void *pt, char *out_label)
{
    gamelists_label_scroll_speed(((ListItem *)pt)->value, out_label, STR_MAX);
}

static void gamelists_formatter_repeat_delay(void *pt, char *out_label)
{
    gamelists_label_repeat_delay(((ListItem *)pt)->value, out_label, STR_MAX);
}

static void gamelists_formatter_repeat_interval(void *pt, char *out_label)
{
    gamelists_label_repeat_interval(((ListItem *)pt)->value, out_label, STR_MAX);
}

static void gamelists_action_rows(void *pt)
{
    int rows = ((ListItem *)pt)->value + GAMELISTS_ROWS_MIN;
    if (rows == gamelists_rows())
        return;

    bool stored = rows == GAMELISTS_ROWS_MIN ? gamelists_remove_file(GAMELISTS_ROWS_PATH)
                                             : gamelists_write_int(GAMELISTS_ROWS_PATH, rows);
    if (stored) {
        // Saved list positions belong to the old row count.
        gamelists_remove_file(GAMELISTS_ROMWINIDX_PATH);
        gamelists_rescale_pending = true;
    }
}

static void gamelists_action_rescale_icons(void *pt)
{
    int enabled = ((ListItem *)pt)->value != 0;
    config_setNumber(GAMELISTS_PREF_RESCALE_ICONS, enabled);
    if (enabled)
        gamelists_rescale_pending = true;
}

static void gamelists_action_font(void *pt)
{
    int value = ((ListItem *)pt)->value;
    if (value <= 0)
        gamelists_remove_file(GAMELISTS_FONT_PATH);
    else
        gamelists_write_int(GAMELISTS_FONT_PATH, value + GAMELISTS_FONT_MIN - 1);
}

static void gamelists_action_scroll_delay(void *pt)
{
    int delay, speed;
    gamelists_load_scroll(&delay, &speed);
    delay = gamelists_scroll_delays[((ListItem *)pt)->value];
    gamelists_store_scroll(delay, speed);
}

static void gamelists_action_scroll_speed(void *pt)
{
    int delay, speed;
    gamelists_load_scroll(&delay, &speed);
    speed = gamelists_scroll_speeds[((ListItem *)pt)->value];
    gamelists_store_scroll(delay, speed);
}

static void gamelists_action_repeat_delay(void *pt)
{
    int delay, interval;
    gamelists_load_repeat(&delay, &interval);
    delay = 100 + ((ListItem *)pt)->value * 50;
    gamelists_store_repeat(delay, interval);
}

static void gamelists_action_repeat_interval(void *pt)
{
    int delay, interval;
    gamelists_load_repeat(&delay, &interval);
    interval = gamelists_repeat_intervals[((ListItem *)pt)->value];
    gamelists_store_repeat(delay, interval);
}

static void gamelists_action_sort(void *pt)
{
    if (((ListItem *)pt)->value != 0)
        gamelists_write_text(GAMELISTS_SORT_PATH, "");
    else
        gamelists_remove_file(GAMELISTS_SORT_PATH);
}

static void gamelists_action_fixed_favorite_position(void *pt)
{
    if (((ListItem *)pt)->value != 0)
        gamelists_remove_file(GAMELISTS_DYNAMIC_FAV_PATH);
    else
        gamelists_write_text(GAMELISTS_DYNAMIC_FAV_PATH, "");
}

// Called when leaving the menu: resize the theme's list icons once, after
// any number of Rows changes.
static void gamelists_on_menu_exit(void)
{
    if (!gamelists_rescale_pending)
        return;
    if (!gamelists_value_rescale_icons() || gamelists_run_theme_rescale(gamelists_rows()))
        gamelists_rescale_pending = false;
}

void menu_gameLists(void *_)
{
    (void)_;
    if (!_menu_game_lists._created) {
        _menu_game_lists = list_createWithTitle(9, LIST_SMALL, "Game lists");

        list_addItemWithInfoNote(
            &_menu_game_lists,
            (ListItem){
                .label = "Rows",
                .item_type = MULTIVALUE,
                .value_max = GAMELISTS_ROWS_MAX - GAMELISTS_ROWS_MIN,
                .value = gamelists_value_rows(),
                .value_formatter = gamelists_formatter_rows,
                .action = gamelists_action_rows},
            "Number of visible rows in ROM, Favorites,\n"
            "Recents and Search game lists.");

        list_addItemWithInfoNote(
            &_menu_game_lists,
            (ListItem){
                .label = "Rescale theme list icons to fit",
                .item_type = TOGGLE,
                .value = gamelists_value_rescale_icons(),
                .action = gamelists_action_rescale_icons},
            "After changing Rows, resize the theme's list\n"
            "icons when leaving this menu. The originals\n"
            "are kept as .bak; 6 rows puts them back.");

        list_addItemWithInfoNote(
            &_menu_game_lists,
            (ListItem){
                .label = "Font override",
                .item_type = MULTIVALUE,
                .value_max = GAMELISTS_FONT_MAX - GAMELISTS_FONT_MIN + 1,
                .value = gamelists_value_font(),
                .value_formatter = gamelists_formatter_font,
                .action = gamelists_action_font},
            "Text size in game lists only.\n"
            "Theme default uses the theme's size.");

        list_addItemWithInfoNote(
            &_menu_game_lists,
            (ListItem){
                .label = "Title scroll delay",
                .item_type = MULTIVALUE,
                .value_max = GAMELISTS_ARRAY_COUNT(gamelists_scroll_delays) - 1,
                .value = gamelists_value_scroll_delay(),
                .value_formatter = gamelists_formatter_scroll_delay,
                .action = gamelists_action_scroll_delay},
            "Wait before a selected long title starts\n"
            "scrolling. Off stops title scrolling.");

        list_addItemWithInfoNote(
            &_menu_game_lists,
            (ListItem){
                .label = "Title scroll speed",
                .item_type = MULTIVALUE,
                .value_max = GAMELISTS_ARRAY_COUNT(gamelists_scroll_speeds) - 1,
                .value = gamelists_value_scroll_speed(),
                .value_formatter = gamelists_formatter_scroll_speed,
                .action = gamelists_action_scroll_speed},
            "How fast long titles scroll, in pixels\n"
            "per second.");

        list_addItemWithInfoNote(
            &_menu_game_lists,
            (ListItem){
                .label = "Repeat delay",
                .item_type = MULTIVALUE,
                .value_max = 38,
                .value = gamelists_value_repeat_delay(),
                .value_formatter = gamelists_formatter_repeat_delay,
                .action = gamelists_action_repeat_delay},
            "Wait before a held button starts to\n"
            "repeat, in all MainUI screens.");

        list_addItemWithInfoNote(
            &_menu_game_lists,
            (ListItem){
                .label = "Repeat interval",
                .item_type = MULTIVALUE,
                .value_max = GAMELISTS_ARRAY_COUNT(gamelists_repeat_intervals) - 1,
                .value = gamelists_value_repeat_interval(),
                .value_formatter = gamelists_formatter_repeat_interval,
                .action = gamelists_action_repeat_interval},
            "Time between repeats while a button\n"
            "is held, in all MainUI screens.");

        list_addItemWithInfoNote(
            &_menu_game_lists,
            (ListItem){
                .label = "Sorting",
                .item_type = MULTIVALUE,
                .value_max = 1,
                .value_labels = {"Case-insensitive", "Case-sensitive"},
                .value = gamelists_value_sort(),
                .action = gamelists_action_sort},
            "Case-sensitive puts names that start\n"
            "with a capital letter first.");

        list_addItemWithInfoNote(
            &_menu_game_lists,
            (ListItem){
                .label = "Fixed favorite position",
                .item_type = TOGGLE,
                .value = gamelists_value_fixed_favorite_position(),
                .action = gamelists_action_fixed_favorite_position},
            "Keep the favorite star in a fixed place.\n"
            "Off: the star follows the title.");
    }

    menu_stack[++menu_level] = &_menu_game_lists;
    header_changed = true;
}

#endif // TWEAKS_GAME_LISTS_H__
