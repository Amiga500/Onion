#ifndef BATMON_WARN_RELOAD_H__
#define BATMON_WARN_RELOAD_H__

#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>

// True when `path` has a different modification time than at the last call
// (and records it). batmon re-reads battery/warnAt then, so a threshold set
// in Tweaks applies within a second, as in Onion (which read it every
// second), with one stat() instead of a file read. The 15 s re-read stays
// for changes within the same FAT timestamp (2 s).
static bool warnReload_changed(const char *path, time_t *last_mtime)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    if (st.st_mtime == *last_mtime)
        return false;
    *last_mtime = st.st_mtime;
    return true;
}

#endif // BATMON_WARN_RELOAD_H__
