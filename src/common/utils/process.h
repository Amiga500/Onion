#ifndef PROCESS_H__
#define PROCESS_H__

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* exists() is provided by file.h / utils. Forward declare to keep header light. */
#ifndef exists
bool exists(const char *file_path);
#endif

#ifndef DT_DIR
#define DT_DIR 4
#endif

#ifndef STR_MAX
#define STR_MAX 256
#endif

//
//    Search pid of running executable (forward match)
//
pid_t process_searchpid(const char *commname)
{
    DIR *procdp;
    struct dirent *dir;
    char fname[24];
    char comm[128];
    pid_t pid;
    pid_t ret = 0;
    size_t commlen;

    if (commname == NULL || *commname == '\0')
        return 0;

    commlen = strlen(commname);
    procdp = opendir("/proc");
    if (procdp == NULL) {
        return 0;
    }
    while ((dir = readdir(procdp))) {
        if (dir->d_type == DT_DIR) {
            pid = (int)strtol(dir->d_name, NULL, 10);
            if (pid > 2) {
                snprintf(fname, sizeof(fname), "/proc/%d/comm", pid);
                FILE *fp = fopen(fname, "r");
                if (fp) {
                    if (fscanf(fp, "%127s", comm) != 1)
                        comm[0] = '\0';
                    fclose(fp);
                    if (!strncmp(comm, commname, commlen)) {
                        ret = pid;
                        break;
                    }
                }
            }
        }
    }
    closedir(procdp);
    return ret;
}

bool process_isRunning(const char *commname)
{
    return process_searchpid(commname) != 0;
}

void process_kill(const char *commname)
{
    pid_t pid;
    if ((pid = process_searchpid(commname)))
        kill(pid, SIGKILL);
}

void process_kill_signal(const char *commname, int sig)
{
    pid_t pid;
    if ((pid = process_searchpid(commname)))
        kill(pid, sig);
}

void process_killall(const char *commname)
{
    pid_t pid;
    int max = 999;
    while ((pid = process_searchpid(commname)) && max-- > 0)
        kill(pid, SIGKILL);
}

/**
 * Resolve the full path of an executable using the same search order
 * as the original process_start.
 */
static bool process_resolve_path(const char *pname, const char *home,
                                 char *out_path, size_t out_sz)
{
    if (pname == NULL || out_path == NULL || out_sz < 16)
        return false;

    const char *base = (home != NULL && home[0] != '\0') ? home : ".";

    snprintf(out_path, out_sz, "%s/bin/%s", base, pname);
    if (exists(out_path))
        return true;

    snprintf(out_path, out_sz, "%s/%s", base, pname);
    if (exists(out_path))
        return true;

    snprintf(out_path, out_sz, "/mnt/SDCARD/.tmp_update/bin/%s", pname);
    if (exists(out_path))
        return true;

    snprintf(out_path, out_sz, "/mnt/SDCARD/.tmp_update/%s", pname);
    if (exists(out_path))
        return true;

    snprintf(out_path, out_sz, "/mnt/SDCARD/miyoo/app/%s", pname);
    if (exists(out_path))
        return true;

    return false;
}

/**
 * Launch a process without going through /bin/sh -c (no system()).
 * Uses fork() + execv() instead.
 */
bool process_start(const char *pname, const char *args, const char *home,
                   bool await)
{
    char filename[PATH_MAX];
    if (!process_resolve_path(pname, home, filename, sizeof(filename)))
        return false;

    pid_t pid = fork();
    if (pid < 0) {
        return false; /* fork failed */
    }

    if (pid == 0) {
        /* ---- child ---- */
        if (home != NULL && home[0] != '\0') {
            if (chdir(home) != 0) {
                /* non-fatal */
            }
        }

        char *argv[4];
        argv[0] = filename;
        int argc = 1;

        if (args != NULL && args[0] != '\0') {
            argv[argc++] = (char *)args;
        }
        argv[argc] = NULL;

        execv(filename, argv);
        _exit(127);
    }

    /* ---- parent ---- */
    if (await) {
        int status = 0;
        while (waitpid(pid, &status, 0) < 0) {
            if (errno != EINTR)
                break;
        }
    }

    return true;
}

int process_start_read_return(const char *cmdline, char *out_str)
{
    char buffer[255] = "";
    char *result = NULL;

    if (cmdline == NULL || out_str == NULL)
        return -1;

    FILE *pipe = popen(cmdline, "r");
    if (pipe == NULL) {
        fprintf(stderr, "Error executing command: %s\n", cmdline);
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        free(result);
        result = strdup(buffer);
    }

    pclose(pipe);
    if (result != NULL) {
        size_t len = strlen(result);
        if (len > 0 && result[len - 1] == '\n')
            result[len - 1] = '\0';
        strncpy(out_str, result, STR_MAX - 1);
        out_str[STR_MAX - 1] = '\0';
        free(result);
    } else {
        out_str[0] = '\0';
    }
    return 0;
}

#endif // PROCESS_H__
