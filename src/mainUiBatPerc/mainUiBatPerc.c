#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "system/battery.h"
#include "system/settings.h"
#include "theme/background.h"
#include "theme/theme.h"
#include "utils/IMG_Save.h"
#include "utils/file.h"
#include "utils/log.h"
#include "utils/str.h"

void restoreRegularDisplay(void)
{
    print_debug(":: restoreRegularDisplay");

    char icon_path[STR_MAX + 20],
        icon_backup[STR_MAX],
        theme_path[STR_MAX];
    theme_getPath(theme_path);

    printf_debug("theme_path: %s\n", theme_path);

    bool icon_exists = theme_getImagePath(theme_path, "power-full-icon", icon_path) == 1;
    bool backup_exists = theme_getImagePath(theme_path, "power-full-icon_back", icon_backup) == 1;

    // Restore regular battery display
    if (icon_exists && backup_exists) {
        remove(icon_path);
        file_copy(icon_backup, icon_path);
        remove(icon_backup);
    }

    printf_debug("icon_path: %s (exists: %d)\n", icon_path, icon_exists);
    printf_debug("icon_backup: %s (exists: %d)\n", icon_backup, backup_exists);
}

#define BATT_PERC_KEY_PATH "/tmp/.batt-perc.key"

static long _mtime(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (long)st.st_mtime : -1L;
}

// Everything the rendered icon depends on: theme, percentage, the theme's
// config and files, and the user's theme overrides. /tmp is cleared on every
// boot, so the icon is always regenerated once per boot.
static void _buildIconKey(char *key, size_t key_size, const char *theme_path, int percentage)
{
    char path[STR_MAX + 32];
    long theme_config, theme_skin;

    snprintf(path, sizeof(path), "%sconfig.json", theme_path);
    theme_config = _mtime(path);
    snprintf(path, sizeof(path), "%sskin", theme_path);
    theme_skin = _mtime(path);

    snprintf(key, key_size, "%s|%d|%ld|%ld|%ld|%ld|%ld",
             theme_path, percentage, theme_config, _mtime(theme_path), theme_skin,
             _mtime(THEME_OVERRIDES "/config.json"), _mtime(THEME_OVERRIDES));
}

void drawBatteryPercentage(void)
{
    print_debug(":: drawBatteryPercentage");

    char theme_path[STR_MAX];
    theme_getPath(theme_path);

    printf_debug("theme_path: %s\n", theme_path);

    char icon_path[STR_MAX + 20];
    snprintf(icon_path, STR_MAX + 19, "%sskin/.batt-perc.png", theme_path);

    printf_debug("icon_path: %s\n", icon_path);

    int percentage = battery_getPercentage();

    // The charging sentinel is never saved: skip loading and rendering.
    if (percentage == 500)
        return;

    // This runs every time MainUI starts (after every game). Decoding the
    // full-screen theme background, rendering and encoding a PNG onto the SD
    // card is skipped when nothing the icon depends on has changed.
    char key[STR_MAX * 2];
    _buildIconKey(key, sizeof(key), theme_path, percentage);

    if (exists(icon_path)) {
        char *last_key = file_read(BATT_PERC_KEY_PATH);
        bool unchanged = last_key != NULL && strcmp(last_key, key) == 0;
        free(last_key);
        if (unchanged) {
            print_debug("Battery icon up to date, skipped");
            return;
        }
    }

    TTF_Init();

    SDL_Surface *image = theme_batterySurfaceWithBg(percentage, theme_background());

    // Save custom battery icon
    if (image != NULL) {
        // Drop the key first: if saving fails, the next run renders again.
        remove(BATT_PERC_KEY_PATH);
        IMG_Save(image, icon_path);
        struct stat st;
        if (stat(icon_path, &st) == 0 && st.st_size > 0) {
            // Recompute after saving: creating the icon changes the mtime of
            // the skin folder, which is part of the key.
            _buildIconKey(key, sizeof(key), theme_path, percentage);
            file_atomic_write(BATT_PERC_KEY_PATH, key, strlen(key));
        }
    }

    SDL_FreeSurface(image);
    resources_free();
    TTF_Quit();
}

int main(int argc, char *argv[])
{
    // Repair themes modified with the previous logic
    // and make sure the percentage resource exists
    restoreRegularDisplay();
    drawBatteryPercentage();
    return 0;
}
