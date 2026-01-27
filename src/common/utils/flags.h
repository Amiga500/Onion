#ifndef FLAGS_H__
#define FLAGS_H__

#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file.h"
#include "str.h"

#define temp_flag_get(key) flag_get("/tmp/", key)
#define temp_flag_set(key, value) flag_set("/tmp/", key, value)

/**
 * @brief Check if a flag file exists
 *        Inline hint for this frequently called function
 */
static inline bool flag_get(const char *path, const char *key)
{
    char filename[STR_MAX];
    concat(filename, path, key);
    return exists(filename);
}

/**
 * @brief Set or clear a flag file
 *        Note: Fixed mode from 777 (octal would be 0777) to proper octal mode
 */
static inline void flag_set(const char *path, const char *key, bool value)
{
    char filename[STR_MAX];
    concat(filename, path, key);

    if (value) {
        int fd = creat(filename, 0644);
        if (fd >= 0)
            close(fd);
    }
    else {
        remove(filename);
    }
}

#endif // FLAGS_H__
