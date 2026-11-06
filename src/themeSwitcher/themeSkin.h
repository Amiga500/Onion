#ifndef THEME_SWITCHER_THEME_SKIN_H__
#define THEME_SWITCHER_THEME_SKIN_H__

// File work of applying a theme, without SDL so host tests run it.
//
// MainUI's fixed images (bg-io-testing, ic-MENU+A, progress-dot) were
// copied over the system ones with file_copy(), which truncates the target
// and writes into it, errors ignored: a crash or a full SD card left a cut
// image, and a failed "restore Onion's own image" deleted the backup it had
// just failed to copy back. Copies are now atomic (temporary file, rename)
// and the backup goes only once it is back in place.
//
// The scripts get their arguments as they are (fork/exec): a theme folder
// name with ", $ or ` broke the shell commands they were put into.

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils/file.h"

// Copy src over dst through a temporary file: dst is either the old file or
// a complete copy. False, with dst as it was, on any error.
static bool ts_copyFileAtomic(const char *src, const char *dst)
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

// One of MainUI's fixed images: from the theme overrides, else from the
// theme, else Onion's own again. Onion's own is kept as <name>_back.png the
// first time a theme replaces it. theme_path and overrides_dir end with
// '/'. False when an image couldn't be put in place (the old one stays).
static bool ts_installSkinImage(const char *overrides_dir, const char *theme_path,
                                const char *image_name, const char *system_dir)
{
    char override_image[PATH_MAX], theme_image[PATH_MAX], system_image[PATH_MAX], backup[PATH_MAX];
    if (snprintf(override_image, sizeof(override_image), "%sskin/%s.png", overrides_dir, image_name) >= (int)sizeof(override_image) ||
        snprintf(theme_image, sizeof(theme_image), "%sskin/%s.png", theme_path, image_name) >= (int)sizeof(theme_image) ||
        snprintf(system_image, sizeof(system_image), "%s/%s.png", system_dir, image_name) >= (int)sizeof(system_image) ||
        snprintf(backup, sizeof(backup), "%s/%s_back.png", system_dir, image_name) >= (int)sizeof(backup))
        return false;

    const char *source = is_file(override_image) ? override_image
                         : is_file(theme_image)  ? theme_image
                                                 : NULL;
    if (source != NULL) {
        // Never replace Onion's own without its backup.
        if (!exists(backup) && is_file(system_image) && !ts_copyFileAtomic(system_image, backup))
            return false;
        return ts_copyFileAtomic(source, system_image);
    }

    // Back to Onion's own image.
    if (!exists(backup))
        return true;
    if (!ts_copyFileAtomic(backup, system_image))
        return false; // the backup stays for the next try
    remove(backup);
    return true;
}

// Run /bin/sh with argv; true when it exits with 0.
static bool ts_runShell(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0) {
        execv("/bin/sh", argv);
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// "sh <script> <arg>"
static bool ts_runScript(const char *script, const char *arg)
{
    char *argv[] = {"sh", (char *)script, (char *)arg, NULL};
    return ts_runShell(argv);
}

// The last folder name of path ("/a/b/Theme/" -> "Theme"), into out.
static bool ts_lastName(const char *path, char *out, size_t size)
{
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/')
        len--;
    size_t start = len;
    while (start > 0 && path[start - 1] != '/')
        start--;
    if (len == start || len - start >= size)
        return false;
    memcpy(out, path + start, len - start);
    out[len - start] = '\0';
    return true;
}

// A compact theme (a preview of an archive): extract it with script into
// themes_dir, and give its path there ("<themes_dir>/<name>/"). False when
// the extraction failed or left no config.json: the theme can't be used.
static bool ts_extractCompactTheme(const char *script, const char *preview_path,
                                   const char *themes_dir, char *theme_path_out, size_t size)
{
    char name[NAME_MAX + 1], config[PATH_MAX];
    if (!ts_lastName(preview_path, name, sizeof(name)))
        return false;
    if (snprintf(config, sizeof(config), "%s/%s/config.json", themes_dir, name) >= (int)sizeof(config))
        return false;

    bool extracted = ts_runScript(script, preview_path);
    sync();
    if (!extracted || !is_file(config))
        return false;

    return snprintf(theme_path_out, size, "%s/%s/", themes_dir, name) < (int)size;
}

#endif // THEME_SWITCHER_THEME_SKIN_H__
