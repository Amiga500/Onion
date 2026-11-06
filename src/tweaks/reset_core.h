#ifndef TWEAKS_RESET_CORE_H__
#define TWEAKS_RESET_CORE_H__

// The file work of Tweaks > Advanced > Reset settings, without SDL so host
// tests run it on real folders.
//
// The defaults come from configs.pak (a 7z archive). The resets used to
// delete the live folder and then extract the pak over it, ignoring 7z's
// result: a failed extraction (SD card full, damaged pak, power cut) left an
// empty or half-written folder, and "Reset system tweaks" lost the pak
// itself on the way (it was moved to .tmp_update/temp for the extraction).
//
// Now the pak is extracted to a staging folder first and checked. Only then
// are the folders swapped with two renames, or each file renamed over its
// old copy: a failure changes nothing, and what a reset keeps (the pak, the
// system's own files) is moved over from the old folder. A swap cut short
// by a crash is finished or undone the next time Tweaks starts.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils/file.h"

#ifndef RESET_SD_ROOT
#define RESET_SD_ROOT "/mnt/SDCARD"
#endif
#define RESET_CONFIGS_PAK RESET_SD_ROOT "/.tmp_update/config/configs.pak"
#define RESET_STAGING_DIR RESET_SD_ROOT "/.tmp_update/.reset_staging"
// Where "Reset system tweaks" used to park the pak during the extraction.
#define RESET_LEGACY_PAK RESET_SD_ROOT "/.tmp_update/temp"
#ifndef RESET_MIGRATION_DIR
#define RESET_MIGRATION_DIR RESET_SD_ROOT "/.tmp_update/script/migration"
#endif
// Suffix of the folder a reset is replacing, while it does.
#define RESET_OLD_SUFFIX ".reset-old"

typedef struct {
    const char *sd_root; // where the pak's paths go
    const char *pak;     // configs.pak
    const char *staging; // scratch folder, on the same file system
    // Extract the files matching filter ("<folder>/*") into out_dir, keeping
    // their paths. True on success.
    bool (*extract)(const char *pak, const char *out_dir, const char *filter);
} ResetSource;

static bool reset_join(char *out, size_t size, const char *a, const char *b)
{
    int n = snprintf(out, size, "%s/%s", a, b);
    return n > 0 && (size_t)n < size;
}

static bool reset_withSuffix(char *out, size_t size, const char *path, const char *suffix)
{
    int n = snprintf(out, size, "%s%s", path, suffix);
    return n > 0 && (size_t)n < size;
}

// Anything at path, a dangling link included.
static bool reset_present(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0;
}

static bool reset_isDir(const char *path)
{
    struct stat st;
    return lstat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// Remove a file or a folder tree. True when nothing is left.
static bool reset_removeAll(const char *path)
{
    if (!reset_present(path))
        return true;
    if (!reset_isDir(path))
        return remove(path) == 0 || errno == ENOENT;
    file_remove_recursive(path);
    return !reset_present(path);
}

// keep: names, or prefixes ending in '*'; NULL-terminated, or NULL.
static bool reset_keepMatches(const char *name, const char *const *keep)
{
    for (; keep != NULL && *keep != NULL; keep++) {
        size_t len = strlen(*keep);
        if (len > 0 && (*keep)[len - 1] == '*') {
            if (strncmp(name, *keep, len - 1) == 0)
                return true;
        }
        else if (strcmp(name, *keep) == 0)
            return true;
    }
    return false;
}

// The entry names of dir (without . and ..), or only those keep names when
// keep is given; *count_out of them. NULL when dir can't be read; free with
// reset_freeNames().
static char **reset_listNames(const char *dir, const char *const *keep, int *count_out)
{
    *count_out = 0;
    DIR *d = opendir(dir);
    if (d == NULL)
        return NULL;

    int count = 0, cap = 0;
    char **names = NULL;
    bool ok = true;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        if (keep != NULL && !reset_keepMatches(e->d_name, keep))
            continue;
        if (count == cap) {
            int new_cap = cap == 0 ? 16 : cap * 2;
            char **grown = realloc(names, (size_t)new_cap * sizeof(char *));
            if (grown == NULL) {
                ok = false;
                break;
            }
            names = grown;
            cap = new_cap;
        }
        if ((names[count] = strdup(e->d_name)) == NULL) {
            ok = false;
            break;
        }
        count++;
    }
    closedir(d);

    if (!ok) {
        for (int i = 0; i < count; i++)
            free(names[i]);
        free(names);
        return NULL;
    }
    if (names == NULL)
        names = calloc(1, sizeof(char *)); // empty folder: not an error
    *count_out = count;
    return names;
}

static void reset_freeNames(char **names, int count)
{
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

// Move the entries of from/ that keep names into to/, replacing what to/
// has under those names. True when all of them were moved.
static bool reset_moveKept(const char *from, const char *to, const char *const *keep)
{
    if (keep == NULL)
        return true;
    int count = 0;
    char **names = reset_listNames(from, keep, &count);
    if (names == NULL)
        return false;

    bool ok = true;
    for (int i = 0; i < count; i++) {
        char src[PATH_MAX], dst[PATH_MAX];
        if (!reset_join(src, sizeof(src), from, names[i]) ||
            !reset_join(dst, sizeof(dst), to, names[i]) ||
            !reset_removeAll(dst) || rename(src, dst) != 0)
            ok = false;
    }
    reset_freeNames(names, count);
    return ok;
}

// A reset of dir cut short by a crash or a power cut: with no dir, it
// stopped between its two renames and the old folder is put back; with
// both, the defaults are in place and the old folder only still holds what
// the reset keeps: that is moved over and the old folder removed. True
// when no old folder is left.
static bool reset_recoverDir(const char *dir, const char *const *keep)
{
    char old[PATH_MAX];
    if (!reset_withSuffix(old, sizeof(old), dir, RESET_OLD_SUFFIX))
        return false;
    if (!reset_present(old))
        return true;
    if (!reset_present(dir))
        return rename(old, dir) == 0;
    if (!reset_moveKept(old, dir, keep))
        return false;
    return reset_removeAll(old);
}

// Extract "<rel>/*" from the pak into a fresh staging folder. True when 7z
// succeeded and the staging folder holds <rel>.
static bool reset_stage(const ResetSource *src, const char *rel)
{
    char filter[PATH_MAX], staged[PATH_MAX];
    if (!reset_withSuffix(filter, sizeof(filter), rel, "/*") ||
        !reset_join(staged, sizeof(staged), src->staging, rel))
        return false;
    if (!is_file(src->pak) || !reset_removeAll(src->staging))
        return false;
    if (!mkdirs(src->staging) && !reset_isDir(src->staging))
        return false;

    if (!src->extract(src->pak, src->staging, filter) || !reset_isDir(staged)) {
        reset_removeAll(src->staging);
        return false;
    }
    // On the card before the old files go: FAT writes are not ordered, and
    // a power cut right after the swap could find the new files empty.
    sync();
    return true;
}

// Replace the folder <sd_root>/<rel> with the pak's, carrying over the
// entries keep names. True when the pak's folder is in place; false, with
// the folder as it was, otherwise.
static bool reset_replaceDir(const ResetSource *src, const char *rel, const char *const *keep)
{
    char dir[PATH_MAX], old[PATH_MAX], staged[PATH_MAX];
    if (!reset_join(dir, sizeof(dir), src->sd_root, rel) ||
        !reset_withSuffix(old, sizeof(old), dir, RESET_OLD_SUFFIX) ||
        !reset_join(staged, sizeof(staged), src->staging, rel))
        return false;

    // What an earlier, interrupted reset left; an old folder that can't be
    // sorted out still holds kept files: not deleted.
    if (!reset_recoverDir(dir, keep))
        return false;

    if (!reset_stage(src, rel))
        return false;

    bool had_dir = reset_present(dir);
    if (had_dir && rename(dir, old) != 0) {
        reset_removeAll(src->staging);
        return false;
    }
    if (rename(staged, dir) != 0) {
        if (had_dir)
            rename(old, dir);
        reset_removeAll(src->staging);
        return false;
    }

    // Kept entries that could not be moved stay in the old folder:
    // reset_recoverDir() moves them at the next start.
    if (had_dir && reset_moveKept(old, dir, keep))
        reset_removeAll(old);
    reset_removeAll(src->staging);
    return true;
}

// Rename every file under from/ over its copy under to/, making folders as
// needed. True when all of them were moved.
static bool reset_moveFilesOver(const char *from, const char *to)
{
    if (!reset_isDir(to) && !mkdirs(to) && !reset_isDir(to))
        return false;

    int count = 0;
    char **names = reset_listNames(from, NULL, &count);
    if (names == NULL)
        return false;

    bool ok = true;
    for (int i = 0; i < count; i++) {
        char src[PATH_MAX], dst[PATH_MAX];
        if (!reset_join(src, sizeof(src), from, names[i]) ||
            !reset_join(dst, sizeof(dst), to, names[i])) {
            ok = false;
            continue;
        }
        if (reset_isDir(src)) {
            if (!reset_moveFilesOver(src, dst))
                ok = false;
        }
        else if (reset_isDir(dst) || rename(src, dst) != 0)
            ok = false;
    }
    reset_freeNames(names, count);
    return ok;
}

// Put the pak's files of <sd_root>/<rel> in place, each renamed over its
// old copy (the other files there stay): every file is either the old one
// or the default, never a part. True when all of them were replaced.
static bool reset_overwriteFiles(const ResetSource *src, const char *rel)
{
    char dir[PATH_MAX], staged[PATH_MAX];
    if (!reset_join(dir, sizeof(dir), src->sd_root, rel) ||
        !reset_join(staged, sizeof(staged), src->staging, rel))
        return false;
    if (!reset_stage(src, rel))
        return false;
    bool ok = reset_moveFilesOver(staged, dir);
    reset_removeAll(src->staging);
    return ok;
}

// Copy src over dst through a temporary file: dst is either the old file or
// a complete copy. False, with dst as it was, when src can't be read.
static bool reset_copyFileAtomic(const char *src, const char *dst)
{
    if (src == NULL || dst == NULL || !is_file(src))
        return false;
    FILE *in = fopen(src, "rb");
    if (in == NULL)
        return false;

    char tmp_path[PATH_MAX], final_path[PATH_MAX];
    FILE *out = file_atomic_begin(dst, tmp_path, sizeof(tmp_path), final_path, sizeof(final_path));
    if (out == NULL) {
        fclose(in);
        return false;
    }

    char buf[8192];
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

// Run a program and wait for it; argv[0] is looked up in PATH. True when
// it exits with 0.
static bool reset_run(char *const argv[], bool quiet)
{
    pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0) {
        if (quiet) {
            int fd = open("/dev/null", O_WRONLY);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// ResetSource.extract on the device: 7z, overwriting without asking.
static bool reset_extract7z(const char *pak, const char *out_dir, const char *filter)
{
    char out_opt[PATH_MAX + 4], filter_opt[PATH_MAX + 4];
    if (!reset_withSuffix(out_opt, sizeof(out_opt), "-o", out_dir) ||
        !reset_withSuffix(filter_opt, sizeof(filter_opt), "-ir!", filter))
        return false;
    char *argv[] = {"7z", "x", "-y", "-aoa", (char *)pak, out_opt, filter_opt, NULL};
    return reset_run(argv, true);
}

// Entries of .tmp_update/config that "Reset system tweaks" keeps: they are
// not settings, or not Tweaks' own.
static const char *const reset_config_keep[] = {
    "configs.pak",                 // the defaults themselves
    "netplay_cores.conf",          // installed with RedOnion, not in the pak:
    "hotspot_wpa_supplicant.conf", // Netplay and joining a hotspot need them
    "migration_state",             // or every migration runs again at the next update
    "system",                      // MainUI settings saved per device (Reset MainUI settings)
    "active_theme",                // what is applied on the card: the reset
    "active_icon_pack",            // doesn't change the theme or the icons
    "theme-applied-*",
    ".systemSkin",
    "ota_channel",  // the update channel, chosen in the updater
    "system_cores", // cores chosen per console in the game list menu
    "syncthing",    // Syncthing's keys, devices and folders (its own web UI)
    NULL};

// Migrations that write RedOnion's defaults into .tmp_update/config (a new
// install gets them from there, not from the pak): run again after "Reset
// system tweaks". Each only writes a file that is missing.
static const char *const reset_default_migrations[] = {
    "00021_sort_apps_by_name.sh",
    "00026_console_pictures_default.sh",
    "00028_saved_games_overwrite_default.sh",
    NULL};

static void reset_runDefaultMigrations(const char *migration_dir)
{
    for (const char *const *m = reset_default_migrations; *m != NULL; m++) {
        char path[PATH_MAX];
        if (!reset_join(path, sizeof(path), migration_dir, *m) || !is_file(path))
            continue;
        char *argv[] = {"sh", path, NULL};
        reset_run(argv, true);
    }
}

// Before any reset, and when Tweaks starts: what an interrupted reset left.
// The pak parked by the old "Reset system tweaks" goes back too.
static void reset_recoverAll(const char *sd_root, const char *staging,
                             const char *legacy_pak, const char *pak)
{
    static const char *const dirs[] = {".tmp_update/config", "Saves/CurrentProfile/config"};
    for (int i = 0; i < (int)(sizeof(dirs) / sizeof(dirs[0])); i++) {
        char dir[PATH_MAX], old[PATH_MAX];
        if (!reset_join(dir, sizeof(dir), sd_root, dirs[i]) ||
            !reset_withSuffix(old, sizeof(old), dir, RESET_OLD_SUFFIX))
            continue;
        // Both there: a "Reset system tweaks" stopped after its swap, before
        // writing RedOnion's defaults; this finishes it.
        bool finishing = i == 0 && reset_present(old) && reset_present(dir);
        if (reset_recoverDir(dir, i == 0 ? reset_config_keep : NULL) && finishing)
            reset_runDefaultMigrations(RESET_MIGRATION_DIR);
    }
    if (!is_file(pak) && is_file(legacy_pak))
        rename(legacy_pak, pak);
    reset_removeAll(staging);
}

#endif // TWEAKS_RESET_CORE_H__
