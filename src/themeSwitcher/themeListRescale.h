#ifndef THEME_LIST_RESCALE_H__
#define THEME_LIST_RESCALE_H__

// Tweaks > Appearance > Game lists > "Rescale theme list icons to fit"
// resizes the active theme's list icons for the row count, but only Tweaks
// ran it: a theme picked in the Themes app kept its full-size icons until the
// option was turned off and on again. Installing a theme now runs the same
// script, with the same settings and row height as Tweaks
// (src/tweaks/game_lists.h). Adapted from robcodedev/onionos-mainui-patcher
// (tools/src/themeSwitcher).

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define THEME_LIST_CONFIG_DIR "/mnt/SDCARD/.tmp_update/config"
#define THEME_LIST_RESCALE_SCRIPT "/mnt/SDCARD/.tmp_update/script/rescale_theme_list_icons.sh"
#define THEME_LIST_ROWS_MIN 6
#define THEME_LIST_ROWS_MAX 20

static bool theme_listReadInt(const char *config_dir, const char *name, int *out)
{
    char path[512];
    if (snprintf(path, sizeof(path), "%s/%s", config_dir, name) >= (int)sizeof(path))
        return false;
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return false;
    bool ok = fscanf(fp, "%d", out) == 1;
    fclose(fp);
    return ok;
}

// Rows in Open MainUI's game lists (.romListRows, 6 when missing).
static int theme_listRows(const char *config_dir)
{
    int rows = THEME_LIST_ROWS_MIN;
    if (!theme_listReadInt(config_dir, ".romListRows", &rows))
        return THEME_LIST_ROWS_MIN;
    if (rows < THEME_LIST_ROWS_MIN)
        return THEME_LIST_ROWS_MIN;
    if (rows > THEME_LIST_ROWS_MAX)
        return THEME_LIST_ROWS_MAX;
    return rows;
}

// The Tweaks option, on unless set to 0.
static bool theme_listRescaleEnabled(const char *config_dir)
{
    int enabled = 1;
    if (!theme_listReadInt(config_dir, "gameLists/rescaleThemeListIcons", &enabled))
        return true;
    return enabled != 0;
}

// Resize theme_path's list icons for the current row count, as Tweaks does.
// Also runs for 6 rows: the script then puts back originals that an earlier
// row count resized. False when the option is off, the script is missing or
// it fails; the theme is installed either way.
static bool theme_listRescale(const char *theme_path, const char *config_dir,
                              const char *script)
{
    if (theme_path == NULL || theme_path[0] == '\0')
        return false;
    if (!theme_listRescaleEnabled(config_dir) || access(script, R_OK) != 0)
        return false;

    int rows = theme_listRows(config_dir);
    char height[16], row_count[16];
    snprintf(height, sizeof(height), "%d", 360 / rows);
    snprintf(row_count, sizeof(row_count), "%d", rows);

    pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0) {
        execl("/bin/sh", "sh", script, theme_path, height, row_count, (char *)NULL);
        _exit(127);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

#endif // THEME_LIST_RESCALE_H__
