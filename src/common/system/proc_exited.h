#ifndef SYSTEM_PROC_EXITED_H__
#define SYSTEM_PROC_EXITED_H__

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

// True when a /proc/<pid>/stat line shows a process that has ended: a
// zombie (Z) or dead (X, x) one, still listed until its parent (runtime.sh)
// collects it, so kill(pid, 0) alone would still find it. The state comes
// after the name in brackets, which may hold spaces or brackets itself, so
// it is read after the last ')'. A line that cannot be read counts as not
// ended.
static inline bool proc_statLineExited(const char *line)
{
    const char *end = line ? strrchr(line, ')') : NULL;
    if (end == NULL || end[1] != ' ' || end[2] == '\0')
        return false;
    char state = end[2];
    return state == 'Z' || state == 'X' || state == 'x';
}

// True when the process is gone or has ended (see above).
static inline bool proc_exited(pid_t pid)
{
    char path[32], line[512];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return true;
    bool read = fgets(line, sizeof(line), f) != NULL;
    fclose(f);
    return read && proc_statLineExited(line);
}

#endif // SYSTEM_PROC_EXITED_H__
