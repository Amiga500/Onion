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
pid_t process_searchpid(const char *commname)
{
    DIR *procdp;
    struct dirent *dir;
    char fname[32];  // Increased slightly for safety
    char comm[128];
    pid_t pid;
    pid_t ret = 0;
    size_t commlen = strlen(commname);

    procdp = opendir("/proc");
    if (!procdp) {
        return 0;
    }
    
    while ((dir = readdir(procdp))) {
        if (dir->d_type == DT_DIR) {
            pid = atoi(dir->d_name);
            if (pid > 2) {
                // Build path more efficiently
                int len = snprintf(fname, sizeof(fname), "/proc/%d/comm", pid);
                if (len < 0 || len >= (int)sizeof(fname)) {
                    continue;
                }
                
                FILE *fp = fopen(fname, "r");
                if (fp) {
                    if (fgets(comm, sizeof(comm), fp)) {
                        // Remove newline if present
                        size_t len = strlen(comm);
                        if (len > 0 && comm[len-1] == '\n') {
                            comm[len-1] = '\0';
                        }
                        
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

bool process_isRunning(const char *commname)
{
    // Simple cache for frequent process checks (e.g., MainUI, batmon)
    static struct {
        char name[32];
        bool running;
        time_t check_time;
    } cache = {0};
    
    time_t now = time(NULL);
    
    // Check cache (500ms TTL for process state)
    if (cache.name[0] != '\0' && 
        strcmp(cache.name, commname) == 0 &&
        (now - cache.check_time) < 1) {
        return cache.running;
    }
    
    // Update cache
    bool result = (process_searchpid(commname) != 0);
    strncpy(cache.name, commname, sizeof(cache.name) - 1);
    cache.name[sizeof(cache.name) - 1] = '\0';
    cache.running = result;
    cache.check_time = now;
    
    return result;
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
    const char *home_dir = home != NULL ? home : ".";
    
    // Try paths in order, using snprintf for safety and speed
    snprintf(filename, sizeof(filename), "%s/bin/%s", home_dir, pname);
    if (!exists(filename)) {
        snprintf(filename, sizeof(filename), "%s/%s", home_dir, pname);
        if (!exists(filename)) {
            snprintf(filename, sizeof(filename), "/mnt/SDCARD/.tmp_update/bin/%s", pname);
            if (!exists(filename)) {
                snprintf(filename, sizeof(filename), "/mnt/SDCARD/.tmp_update/%s", pname);
                if (!exists(filename)) {
                    snprintf(filename, sizeof(filename), "/mnt/SDCARD/miyoo/app/%s", pname);
                    if (!exists(filename)) {
                        return false;
                    }
                }
            }
        }
    }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cd \"%s\"; %s %s %s", home_dir, filename,
            args != NULL ? args : "", await ? "" : "&");
    system(cmd);

    return true;
}

bool process_start_read_return(const char *cmdline, char *out_str)
{
    char buffer[255] = "";
    char *result = NULL;

    FILE *pipe = popen(cmdline, "r");
    if (pipe == NULL) {
        fprintf(stderr, "Error executing command: %s\n", cmdline);
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result = strdup(buffer);
    }

    pclose(pipe);
    if (result != NULL) {
        result[strlen(buffer) - 1] = '\0';
        strcpy(out_str, result);
        free(result);
    }
    else {
        strcpy(out_str, "");
    }
    return 0;
}

#endif // PROCESS_H__
