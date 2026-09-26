#ifndef PROCESS_H__
#define PROCESS_H__

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef DT_DIR
#define DT_DIR 4
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
    size_t commlen = strlen(commname);

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

//
//    Search several executables in a single pass over /proc.
//    pids_out[i] receives the first pid whose comm starts with names[i],
//    or 0 when none is found (same forward match as process_searchpid).
//
void process_searchpids(const char *const names[], pid_t pids_out[], int count)
{
    DIR *procdp;
    struct dirent *dir;
    char fname[24];
    char comm[128];
    int remaining = count;

    for (int i = 0; i < count; i++)
        pids_out[i] = 0;

    procdp = opendir("/proc");
    if (procdp == NULL)
        return;

    while (remaining > 0 && (dir = readdir(procdp))) {
        if (dir->d_type != DT_DIR)
            continue;
        pid_t pid = (int)strtol(dir->d_name, NULL, 10);
        if (pid <= 2)
            continue;

        snprintf(fname, sizeof(fname), "/proc/%d/comm", pid);
        FILE *fp = fopen(fname, "r");
        if (fp == NULL)
            continue;
        if (fscanf(fp, "%127s", comm) != 1)
            comm[0] = '\0';
        fclose(fp);

        for (int i = 0; i < count; i++) {
            if (pids_out[i] == 0 && !strncmp(comm, names[i], strlen(names[i]))) {
                pids_out[i] = pid;
                remaining--;
            }
        }
    }
    closedir(procdp);
}

//
//    Start a program without a shell and without waiting for it.
//    Double fork: the worker is reparented to init, so no zombie is left
//    behind and the caller never blocks. argv[0] is looked up in PATH.
//
bool process_spawn_detached(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return false;

    if (pid == 0) {
        pid_t worker = fork();
        if (worker == 0) {
            setsid();
            execvp(argv[0], argv);
            _exit(127);
        }
        _exit(worker < 0 ? 1 : 0);
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        // ECHILD (e.g. SIGCHLD ignored): the intermediate child was reaped
        // already, so status was never filled in. It exits at once after
        // forking the worker, so report the spawn as done.
        if (errno != EINTR)
            return errno == ECHILD;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

//
//    Run a program without a shell and wait for it.
//    Returns the exit code, or -1 if it could not be started.
//
int process_run_wait(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }

    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
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

void process_killall_signal(const char *commname, int sig)
{
    pid_t pid;
    int max = 999;
    while ((pid = process_searchpid(commname)) && max-- > 0)
        kill(pid, sig);
}

void process_killall(const char *commname)
{
    process_killall_signal(commname, SIGKILL);
}

bool process_start(const char *pname, const char *args, const char *home,
                   bool await)
{
    char filename[256];
    snprintf(filename, sizeof(filename), "%s/bin/%s", home != NULL ? home : ".", pname);
    if (!exists(filename))
        snprintf(filename, sizeof(filename), "%s/%s", home != NULL ? home : ".", pname);
    if (!exists(filename))
        snprintf(filename, sizeof(filename), "/mnt/SDCARD/.tmp_update/bin/%s", pname);
    if (!exists(filename))
        snprintf(filename, sizeof(filename), "/mnt/SDCARD/.tmp_update/%s", pname);
    if (!exists(filename))
        snprintf(filename, sizeof(filename), "/mnt/SDCARD/miyoo/app/%s", pname);
    if (!exists(filename))
        return false;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cd \"%s\"; %s %s %s", home != NULL ? home : ".", filename,
             args != NULL ? args : "", await ? "" : "&");
    system(cmd);

    return true;
}

int process_start_read_return(const char *cmdline, char *out_str)
{
    char buffer[255] = "";
    char *result = NULL;

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
    }
    else {
        out_str[0] = '\0';
    }
    return 0;
}

#endif // PROCESS_H__
