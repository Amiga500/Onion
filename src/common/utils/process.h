#ifndef PROCESS_H__
#define PROCESS_H__

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "file.h"

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
    if (procdp == NULL)
        return 0;
    while ((dir = readdir(procdp))) {
        if (dir->d_type == DT_DIR) {
            pid = atoi(dir->d_name);
            if (pid > 2) {
                sprintf(fname, "/proc/%d/comm", pid);
                FILE *fp = fopen(fname, "r");
                if (fp) {
                    fscanf(fp, "%127s", comm);
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

void process_killall(const char *commname)
{
    pid_t pid;
    int max = 999;
    while ((pid = process_searchpid(commname)) && max-- > 0)
        kill(pid, SIGKILL);
}

bool process_start(const char *pname, const char *args, const char *home,
                   bool await)
{
    char filename[256];
    sprintf(filename, "%s/bin/%s", home != NULL ? home : ".", pname);
    if (!exists(filename))
        sprintf(filename, "%s/%s", home != NULL ? home : ".", pname);
    if (!exists(filename))
        sprintf(filename, "/mnt/SDCARD/.tmp_update/bin/%s", pname);
    if (!exists(filename))
        sprintf(filename, "/mnt/SDCARD/.tmp_update/%s", pname);
    if (!exists(filename))
        sprintf(filename, "/mnt/SDCARD/miyoo/app/%s", pname);
    if (!exists(filename))
        return false;

    pid_t pid = fork();
    if (pid < 0)
        return false;
    if (pid == 0) {
        if (chdir(home != NULL ? home : ".") != 0)
            _exit(127);
        if (args != NULL && args[0] != '\0')
            execl(filename, filename, args, (char *)NULL);
        else
            execl(filename, filename, (char *)NULL);
        _exit(127);
    }
    if (await) {
        int status;
        waitpid(pid, &status, 0);
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
        out_str[0] = '\0';
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        free(result);
        result = strdup(buffer);
    }

    pclose(pipe);
    if (result != NULL) {
        size_t n = strlen(result);
        if (n > 0 && result[n - 1] == '\n')
            result[n - 1] = '\0';
        strcpy(out_str, result);
        free(result);
    }
    else {
        out_str[0] = '\0';
    }
    return 0;
}

#endif // PROCESS_H__
