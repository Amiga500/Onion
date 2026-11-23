#ifndef TWEAKS_RESET_KEEP_H__
#define TWEAKS_RESET_KEEP_H__

// What "Reset system tweaks" keeps in .tmp_update/config, and the migrations
// it runs again. SDL-free, so test_tweaks_reset_keep runs it on real folders.
//
// The reset empties the folder and extracts the defaults from configs.pak,
// which only holds Tweaks' default settings. Everything below is not one of
// those settings, and nothing gives it back once it is gone.

#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "utils/file.h"

// Names, or prefixes ending in '*'. NULL-terminated.
static const char *const reset_config_keep[] = {
    "configs.pak",                 // the defaults themselves
    "migration_state",             // or every migration runs again at the next update
    "netplay_cores.conf",          // installed with RedOnion, not in the pak:
    "hotspot_wpa_supplicant.conf", // Netplay and joining a hotspot need them
    "syncthing",                   // Syncthing's keys, devices and folders
    "ota_channel",                 // the update channel chosen in the updater,
    ".otaTesters",                 // and the file that offers the testers one
    "system",                      // MainUI settings saved per device ("Reset MainUI settings")
    "active_theme",                // the theme and icons applied on the card:
    "active_icon_pack",            // the reset doesn't change them
    "theme-applied-*",
    ".systemSkin",
    "system_cores",  // cores chosen per console in the game list menu
    "audiotune.ini", // AudioTune's settings (its own app, not Tweaks)
    NULL};

// Migrations that write RedOnion's defaults into .tmp_update/config: a new
// install gets them from there, not from the pak. Each only writes a file
// that is missing, and the reset removed it.
static const char *const reset_default_migrations[] = {
    "00021_sort_apps_by_name.sh",
    "00026_console_pictures_default.sh",
    "00028_saved_games_overwrite_default.sh",
    "00029_game_info_auto_scroll_default.sh",
    NULL};

static bool reset_keepsConfigEntry(const char *name)
{
    if (name == NULL || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return true;
    for (const char *const *k = reset_config_keep; *k != NULL; k++) {
        size_t len = strlen(*k);
        if (len > 0 && (*k)[len - 1] == '*') {
            if (strncmp(name, *k, len - 1) == 0)
                return true;
        }
        else if (strcmp(name, *k) == 0)
            return true;
    }
    return false;
}

// Removes every entry of dir that the reset doesn't keep. False when dir
// can't be read.
static bool reset_clearConfigDir(const char *dir)
{
    DIR *dp = opendir(dir);
    if (dp == NULL)
        return false;
    struct dirent *ep;
    while ((ep = readdir(dp)) != NULL) {
        if (reset_keepsConfigEntry(ep->d_name))
            continue;
        char path[PATH_MAX];
        if (snprintf(path, sizeof(path), "%s/%s", dir, ep->d_name) >= (int)sizeof(path))
            continue;
        file_remove_recursive(path);
    }
    closedir(dp);
    return true;
}

// The shell command that runs reset_default_migrations from migration_dir
// (none when it can't go there), each one even when another fails. False
// when it doesn't fit.
static bool reset_migrationCommand(char *out, size_t size, const char *migration_dir)
{
    int n = snprintf(out, size, "cd \"%s\" && {", migration_dir);
    if (n < 0 || (size_t)n >= size)
        return false;
    size_t used = (size_t)n;
    for (const char *const *m = reset_default_migrations; *m != NULL; m++) {
        n = snprintf(out + used, size - used, " sh ./%s;", *m);
        if (n < 0 || (size_t)n >= size - used)
            return false;
        used += (size_t)n;
    }
    n = snprintf(out + used, size - used, " }");
    return n >= 0 && (size_t)n < size - used;
}

#endif // TWEAKS_RESET_KEEP_H__
