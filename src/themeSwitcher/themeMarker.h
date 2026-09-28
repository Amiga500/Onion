#ifndef THEME_MARKER_H__
#define THEME_MARKER_H__

// runtime.sh re-applies the theme, icons included (--reapply_icons), at
// boot when config/theme-applied-<serial> does not name the current theme.
// The marker must follow every theme change: when only runtime.sh wrote it,
// a theme picked in the Themes app (with "Apply icons" off, or followed by
// another icon pack) got its icons forced on at the next boot.

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define THEME_MARKER_SN_FILE "/tmp/deviceSN"
#define THEME_MARKER_DIR "/mnt/SDCARD/.tmp_update/config"

// Path of the marker for this device: <dir>/theme-applied-<serial>, with
// the serial read from sn_file (written by runtime.sh at boot). False when
// the serial is unknown, so nothing is written outside a normal boot.
static bool theme_markerPath(char *out, size_t size, const char *sn_file,
                             const char *dir)
{
    char serial[64] = "";
    FILE *fp = fopen(sn_file, "r");
    if (fp == NULL)
        return false;
    bool read_ok = fgets(serial, sizeof(serial), fp) != NULL;
    fclose(fp);
    if (!read_ok)
        return false;

    serial[strcspn(serial, "\r\n")] = '\0';
    if (serial[0] == '\0' || strchr(serial, '/') != NULL)
        return false;

    int n = snprintf(out, size, "%s/theme-applied-%s", dir, serial);
    return n > 0 && (size_t)n < size;
}

// Record theme_path as applied on this device, in the format runtime.sh
// compares against (`echo -n "$system_theme"`: the path, no newline).
// Written to a temporary file and renamed, so a power cut leaves either
// the old marker or the new one.
static bool theme_markApplied(const char *theme_path, const char *sn_file,
                              const char *dir)
{
    char path[512], tmp_path[520];
    if (theme_path == NULL ||
        !theme_markerPath(path, sizeof(path), sn_file, dir))
        return false;
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *fp = fopen(tmp_path, "w");
    if (fp == NULL)
        return false;
    bool ok = fputs(theme_path, fp) >= 0;
    ok = (fclose(fp) == 0) && ok;
    if (!ok || rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return false;
    }
    return true;
}

#endif // THEME_MARKER_H__
