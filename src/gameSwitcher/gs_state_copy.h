#ifndef GAME_SWITCHER_STATE_COPY_H
#define GAME_SWITCHER_STATE_COPY_H

// Load on a game that is not running: RetroArch will start it from its auto
// save state, so the chosen slot becomes that state. The copy used to
// truncate the auto save and then write into it, ignoring errors: a failed
// write left a broken state that the game was started with anyway.
//
// Now the slot is copied atomically (temporary file, fsync, rename): the
// auto save is either the old one or the whole slot, never a partial copy,
// and an error is reported so the caller does not start the game. The
// slot's picture goes with it. Like Open MainUI's Saved Games with
// RedOnion's default (Overwrite), the replaced auto save is not kept.
// SDL-free so host tests include it.

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#include "utils/file.h"

// Copy src over dst through a temporary file: dst is either the old file or
// a complete copy, never a partial one.
static bool gs_copyFileAtomic(const char *src, const char *dst)
{
    if (src == NULL || dst == NULL)
        return false;
    FILE *in = fopen(src, "rb");
    if (in == NULL)
        return false;

    char tmp_path[PATH_MAX];
    char final_path[PATH_MAX];
    FILE *out = file_atomic_begin(dst, tmp_path, sizeof(tmp_path), final_path, sizeof(final_path));
    if (out == NULL) {
        fclose(in);
        return false;
    }

    char buf[16384];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = false;
            break;
        }
    }
    if (ferror(in))
        ok = false;
    fclose(in);

    if (!ok) {
        fclose(out);
        remove(tmp_path);
        return false;
    }
    return file_atomic_commit(out, tmp_path, final_path);
}

// "<path><suffix>" in out; false if it does not fit.
static bool gs_pathWithSuffix(char *out, size_t size, const char *path, const char *suffix)
{
    int n = snprintf(out, size, "%s%s", path, suffix);
    return n > 0 && (size_t)n < size;
}

// Make the state at slot_path the auto save at auto_path (see above).
static bool gs_loadSlotIntoAuto(const char *slot_path, const char *auto_path)
{
    if (slot_path == NULL || auto_path == NULL || !is_file(slot_path))
        return false;

    char slot_png[PATH_MAX], auto_png[PATH_MAX];
    if (!gs_pathWithSuffix(slot_png, sizeof(slot_png), slot_path, ".png") ||
        !gs_pathWithSuffix(auto_png, sizeof(auto_png), auto_path, ".png"))
        return false;

    if (!gs_copyFileAtomic(slot_path, auto_path))
        return false;

    // The picture of the state that will be loaded, or none rather than
    // the picture of the replaced one.
    if (!is_file(slot_png) || !gs_copyFileAtomic(slot_png, auto_png))
        remove(auto_png);
    return true;
}

#endif // GAME_SWITCHER_STATE_COPY_H
