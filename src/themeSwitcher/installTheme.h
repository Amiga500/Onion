#ifndef THEME_SWITCHER_INSTALL_THEME_H__
#define THEME_SWITCHER_INSTALL_THEME_H__

#include <dirent.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "theme/config.h"
#include "utils/apply_icons.h"
#include "utils/file.h"
#include "utils/json.h"
#include "utils/log.h"
#include "utils/str.h"

#include "themeListRescale.h"
#include "themeMarker.h"
#include "themePreview.h"
#include "themeSkin.h"

#ifdef PLATFORM_MIYOOMINI
#define SCRIPT_DIR "/mnt/SDCARD/.tmp_update/script"
#else
#define SCRIPT_DIR "./script"
#endif

#define SYSTEM_SKIN_DIR "/mnt/SDCARD/miyoo/app/skin"
#define THEMES_DIR "/mnt/SDCARD/Themes"
#define ACTIVE_THEME "/mnt/SDCARD/.tmp_update/config/active_theme"

// Max number of records in the DB
#define NUMBER_OF_THEMES 500

int _comp_themes(const void *a, const void *b)
{
    return strcasecmp((const char *)a, (const char *)b);
}

void loadThemeDirectory(const char *theme_dir,
                        char themes_out[NUMBER_OF_THEMES][STR_MAX], int *count,
                        bool check_preview)
{
    DIR *dp;
    struct dirent *ep;
    char config_path[STR_MAX * 2];
    char preview_dir[STR_MAX * 2];

    if ((dp = opendir(theme_dir)) != NULL) {
        while ((ep = readdir(dp))) {
            if (*count >= NUMBER_OF_THEMES)
                break;
            if (ep->d_type != DT_DIR)
                continue;
            if (ep->d_name[0] == '.')
                continue;

            snprintf(config_path, STR_MAX * 2 - 1, "%s/%s/config.json",
                     theme_dir, ep->d_name);

            if (check_preview) {
                // Scanning the extracted Themes/: defer to the previews pass
                // only when a usable compact preview exists. An orphaned
                // preview (its archive was deleted) must not hide the
                // extracted theme, so it is not treated as superseding.
                snprintf(preview_dir, STR_MAX * 2 - 1,
                         THEMES_DIR "/.previews/%s/", ep->d_name);
                if (themePreview_hasArchive(preview_dir))
                    continue;
            }
            else {
                // Scanning Themes/.previews/ directly: skip an entry whose
                // source archive is gone, otherwise a deleted compact theme
                // stays listed but can no longer be loaded.
                snprintf(preview_dir, STR_MAX * 2 - 1, "%s/%s/",
                         theme_dir, ep->d_name);
                if (!themePreview_hasArchive(preview_dir))
                    continue;
            }

            if (is_file(config_path)) {
                if (*count >= NUMBER_OF_THEMES) {
                    printf_debug("Theme limit reached (%d); ignoring remaining entries in %s\n",
                                 NUMBER_OF_THEMES, theme_dir);
                    break;
                }

                snprintf(themes_out[*count], STR_MAX, "%s", ep->d_name);
                (*count)++;
            }
        }
        closedir(dp);
    }
    else {
        perror("Couldn't open the Themes directory");
    }
}

void updatePreviews()
{
    int result = system(SCRIPT_DIR "/themes_extract_previews.sh");
    if (result != 0)
        printf_debug("Theme preview update script failed: %d\n", result);
}

int listAllThemes(char themes_out[NUMBER_OF_THEMES][STR_MAX], const char *installed_theme, int *installed_page)
{
    int count = 0;

    updatePreviews();

    loadThemeDirectory(THEMES_DIR, themes_out, &count, true);
    loadThemeDirectory(THEMES_DIR "/.previews", themes_out, &count, false);

    qsort(themes_out, count, sizeof(char) * STR_MAX, _comp_themes);

    for (int i = 0; i < count; i++) {
        if (strcmp(themes_out[i], installed_theme) == 0) {
            *installed_page = i;
            break;
        }
    }

    return count;
}

bool checkPreview(const char *preview_path)
{
    // A preview is usable only while its source archive still exists.
    return themePreview_hasArchive(preview_path);
}

bool getThemePath(const char *theme_name, char *theme_path_out)
{
    snprintf(theme_path_out, STR_MAX * 2 - 1, THEMES_DIR "/.previews/%s/",
             theme_name);

    if (!checkPreview(theme_path_out))
        snprintf(theme_path_out, STR_MAX * 2 - 1, THEMES_DIR "/%s/",
                 theme_name);

    return is_dir(theme_path_out);
}

bool loadTheme(const char *theme_name, Theme_s *theme_out)
{
    char theme_path[STR_MAX * 2];

    if (getThemePath(theme_name, theme_path)) {
        *theme_out = theme_loadFromPath(theme_path, false);
        return true;
    }

    return false;
}

void installNonDynamicElement(const char *theme_path, const char *image_name)
{
    if (!ts_installSkinImage(THEME_OVERRIDES, theme_path, image_name, SYSTEM_SKIN_DIR)) {
        printf_debug("Could not install skin image %s\n", image_name);
    }
}

// Apply the theme at theme_path (a buffer of size bytes; a compact theme's
// path becomes that of its extracted folder). False, with the current
// theme left as it is, when a compact theme could not be extracted.
bool installTheme(char *theme_path, size_t size, bool apply_icons)
{
    if (strstr(theme_path, "/.previews/") != NULL) {
        // Before anything changes: an extraction that failed (SD card
        // full, damaged archive) used to switch to a missing or cut theme.
        char extracted[STR_MAX * 2];
        if (!ts_extractCompactTheme(SCRIPT_DIR "/themes_extract_theme.sh", theme_path,
                                    THEMES_DIR, extracted, sizeof(extracted))) {
            printf_debug("Could not extract theme: %s\n", theme_path);
            return false;
        }
        if (strlen(extracted) >= size)
            return false;
        snprintf(theme_path, size, "%s", extracted);
    }

    if (strlen(theme_path) >= sizeof(settings.theme)) {
        printf_debug("Theme path too long: %s\n", theme_path);
        return false;
    }

    system("/mnt/SDCARD/.tmp_update/bin/mainUiBatPerc --restore");

    // change theme setting
    snprintf(settings.theme, sizeof(settings.theme), "%s", theme_path);
    settings_save();

    FILE *fp;
    file_put_sync(fp, ACTIVE_THEME, "%s", theme_path);
    theme_markApplied(theme_path, THEME_MARKER_SN_FILE, THEME_MARKER_DIR);

    Theme_s with_overrides = theme_loadFromPath(theme_path, true);

    lang_removeIconLabels(with_overrides.hideLabels.icons,
                          with_overrides.hideLabels.hints);

    installNonDynamicElement(settings.theme, "bg-io-testing");
    installNonDynamicElement(settings.theme, "ic-MENU+A");
    installNonDynamicElement(settings.theme, "progress-dot");

    if (apply_icons) {
        char icon_pack_path[STR_MAX + 32];
        snprintf(icon_pack_path, STR_MAX + 32 - 1, "%sicons", theme_path);
        apply_iconPack(
            is_dir(icon_pack_path) ? icon_pack_path : ICON_PACK_DEFAULT, true);
    }

    // Apps that load fixed files instead of theme images (Package Manager,
    // Search, Quick Guide, ...) follow the theme through its "system" folder.
    ts_runScript(SCRIPT_DIR "/system_skin.sh", theme_path);

    // List icons sized for the game-list row count, as Tweaks > Game lists >
    // Layout does when its rows or resize option change.
    theme_listRescale(theme_path, THEME_LIST_CONFIG_DIR, THEME_LIST_RESCALE_SCRIPT);
    return true;
}

bool checkAndSetDir(char *dest, const char *dir_path)
{
    snprintf(dest, STR_MAX * 2, "%s", dir_path);
    return is_dir(dest);
}

bool checkActiveTheme(char *theme_path_out)
{
    if (!is_file(ACTIVE_THEME))
        return false;

    char active_theme_path[STR_MAX];
    if (!file_read_line(ACTIVE_THEME, active_theme_path, sizeof(active_theme_path)))
        return false;

    printf_debug("active theme: %s\n", active_theme_path);

    return getThemePath(basename(active_theme_path), theme_path_out);
}

bool ensureThemePath(const char *theme_name, char *theme_path_out)
{
    if (getThemePath(theme_name, theme_path_out))
        return true;
    if (checkActiveTheme(theme_path_out))
        return true;
    if (checkAndSetDir(theme_path_out, DEFAULT_THEME_PATH))
        return true;
    if (checkAndSetDir(theme_path_out, "/mnt/SDCARD/miyoo/app"))
        return true;
    return checkAndSetDir(theme_path_out, "/customer/app");
}

void reinstallTheme(const char *theme_name, bool apply_icons,
                    bool update_previews)
{
    char theme_path[STR_MAX * 2];

    if (update_previews)
        updatePreviews();

    if (!ensureThemePath(theme_name, theme_path))
        return;

    if (installTheme(theme_path, sizeof(theme_path), apply_icons)) {
        printf_debug("Reapplied: \"%s\"\n", theme_path);
    }
}

#endif // THEME_SWITCHER_INSTALL_THEME_H__
