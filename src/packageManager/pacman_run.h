#ifndef PACMAN_RUN_H__
#define PACMAN_RUN_H__

// Running the install scripts of the Package Manager, without SDL so host
// tests run it.
//
// The commands were built with sprintf() into fixed buffers and run by
// system(), with the package name between double quotes: a name with ", $
// or ` broke the command (or ran part of the name), a long path overflowed
// the buffer, and the result was ignored, so a failed copy (SD card full)
// left a half-installed package without a word. The scripts now get their
// arguments as they are (fork/exec) and their exit status is returned.

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PACMAN_INSTALL_SCRIPT
#define PACMAN_INSTALL_SCRIPT "/mnt/SDCARD/.tmp_update/script/pacman_install.sh"
#endif

// Run /bin/sh with argv (from the second element), in dir when given.
// True when it exits with 0.
static bool pacman_runShell(const char *dir, char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0) {
        if (dir != NULL && chdir(dir) != 0)
            _exit(126);
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

// pacman_install.sh <data_path> <package>: copy the package's files.
static bool pacman_runInstall(const char *script, const char *data_path, const char *package)
{
    char *argv[] = {"sh", (char *)script, (char *)data_path, (char *)package, NULL};
    return pacman_runShell(NULL, argv);
}

// The install.sh or uninstall.sh of an installed package (main_path), run
// from its folder. True when there is none, or it succeeded.
static bool pacman_runPackageScript(const char *main_path, bool install)
{
    const char *name = install ? "install.sh" : "uninstall.sh";
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/%s", main_path, name);
    if (n < 0 || (size_t)n >= sizeof(path))
        return false;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        return true;
    // "sh ./install.sh": no need to make it executable first.
    char script[16];
    snprintf(script, sizeof(script), "./%s", name);
    char *argv[] = {"sh", script, NULL};
    return pacman_runShell(main_path, argv);
}

#endif // PACMAN_RUN_H__
