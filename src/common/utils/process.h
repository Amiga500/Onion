#ifndef PROCESS_H__
#define PROCESS_H__

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DT_DIR
#define DT_DIR 4
#endif

//
//    Search pid of running executable (forward match)
//
static inline pid_t process_searchpid(const char *commname)
{
    DIR *procdp;
    struct dirent *dir;
    char fname[24];
    char comm[128];
    pid_t pid;
    pid_t ret = 0;
    size_t commlen = strlen(commname);

    procdp = opendir("/proc");
    if (!procdp)
        return 0;

    while ((dir = readdir(procdp))) {
        if (dir->d_type == DT_DIR) {
            pid = atoi(dir->d_name);
            if (pid > 2) {
                sprintf(fname, "/proc/%d/comm", pid);
                FILE *fp = fopen(fname, "r");
                if (fp) {
                    if (fscanf(fp, "%127s", comm) == 1) {
                        if (!strncmp(comm, commname, commlen)) {
                            ret = pid;
                            fclose(fp);
                            break;
                        }
                    }
                    fclose(fp);
                }
            }
        }
    }
    closedir(procdp);
    return ret;
}

static inline bool process_isRunning(const char *commname)
{
    return process_searchpid(commname) != 0;
}

static inline void process_kill(const char *commname)
{
    pid_t pid;
    if ((pid = process_searchpid(commname)))
        kill(pid, SIGKILL);
}

static inline void process_killall(const char *commname)
{
    pid_t pid;
    int max = 999;
    while ((pid = process_searchpid(commname)) && max-- > 0)
        kill(pid, SIGKILL);
}

static inline bool process_start(const char *pname, const char *args, const char *home,
                   bool await)
{
    char filename[256];

    // Check possible locations in order of likelihood
    // Using a single path buffer and testing each location
    const char *paths[] = {
        "%s/bin/%s",
        "%s/%s",
        "/mnt/SDCARD/.tmp_update/bin/%s",
        "/mnt/SDCARD/.tmp_update/%s",
        "/mnt/SDCARD/miyoo/app/%s"
    };
    const char *homedir = home != NULL ? home : ".";
    bool found = false;

    for (int i = 0; i < 5 && !found; i++) {
        if (i < 2) {
            sprintf(filename, paths[i], homedir, pname);
        } else {
            sprintf(filename, paths[i], pname);
        }
        if (exists(filename)) {
            found = true;
        }
    }

    if (!found)
        return false;

    char cmd[512];
    sprintf(cmd, "cd \"%s\"; %s %s %s", homedir, filename,
            args != NULL ? args : "", await ? "" : "&");
    system(cmd);

    return true;
}

/**
 * @brief Execute a command and read its output.
 *        Fixed memory leak from repeated strdup() in loop.
 *
 * @param cmdline Command to execute
 * @param out_str Output buffer for result
 * @return 0 on success, -1 on error
 */
static inline int process_start_read_return(const char *cmdline, char *out_str)
{
    char buffer[255] = "";

    FILE *pipe = popen(cmdline, "r");
    if (pipe == NULL) {
        fprintf(stderr, "Error executing command: %s\n", cmdline);
        out_str[0] = '\0';
        return -1;
    }

    // Read lines, keeping only the last one
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        // buffer now contains the last line read
    }

    pclose(pipe);

    // Copy the last line read (or empty if none)
    if (buffer[0] != '\0') {
        size_t len = strlen(buffer);
        // Remove trailing newline if present
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        strcpy(out_str, buffer);
    }
    else {
        out_str[0] = '\0';
    }

    return 0;
}

#endif // PROCESS_H__
