#ifndef THEME_SWITCHER_THEME_PREVIEW_H__
#define THEME_SWITCHER_THEME_PREVIEW_H__

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "utils/file.h"
#include "utils/str.h"

/**
 * A compact theme is left on the card as an archive (.zip/.7z/.rar); the Themes
 * app extracts only a lightweight preview into Themes/.previews/<name>/ and
 * writes a `source` file naming the archive it came from. The preview is a
 * stand-in for the archive: the theme can only be loaded while that archive
 * still exists.
 *
 * If the archive is deleted (e.g. from the web file manager, which does not
 * touch the hidden .previews cache), the preview is left orphaned. Listing it
 * would show a theme that fails to load, so callers use this to skip it.
 *
 * Returns true only when preview_dir is a directory holding a `source` file
 * whose archive path still exists on disk. The read is bounded.
 */
static inline bool themePreview_hasArchive(const char *preview_dir)
{
    if (preview_dir == NULL || !is_dir(preview_dir))
        return false;

    char source_path[STR_MAX * 2];
    snprintf(source_path, sizeof(source_path), "%s/source", preview_dir);
    if (!is_file(source_path))
        return false;

    FILE *fp = fopen(source_path, "r");
    if (fp == NULL)
        return false;

    char archive_path[STR_MAX * 2];
    if (fgets(archive_path, sizeof(archive_path), fp) == NULL) {
        fclose(fp);
        return false;
    }
    fclose(fp);

    archive_path[strcspn(archive_path, "\r\n")] = '\0';
    if (archive_path[0] == '\0')
        return false;

    return is_file(archive_path);
}

#endif // THEME_SWITCHER_THEME_PREVIEW_H__
