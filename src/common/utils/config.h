#ifndef CONFIG_H__
#define CONFIG_H__

#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

#include "file.h"
#include "flags.h"
#include "log.h"
#include "str.h"

#define CONFIG_PATH "/mnt/SDCARD/.tmp_update/config/"
#define CONFIG_INT "%d"
#define CONFIG_STR "%[^\n]"

bool config_flag_get(const char *key) { return flag_get(CONFIG_PATH, key); }

void config_flag_set(const char *key, bool value)
{
    char hidden_flag[STR_MAX];
    concat(hidden_flag, sizeof(hidden_flag), key, "_");
    flag_set(CONFIG_PATH, key, value);
    flag_set(CONFIG_PATH, hidden_flag, !value);
}

bool config_get(const char *key, const char *format, void *dest)
{
    FILE *fp;

    char filename[STR_MAX];
    concat(filename, sizeof(filename), CONFIG_PATH, key);

    if (exists(filename)) {
        file_get(fp, filename, format, dest);
        return true;
    }

    return false;
}

bool config_getString(const char *key, char *dest, size_t dest_size)
{
    FILE *fp;

    char filename[STR_MAX];
    concat(filename, sizeof(filename), CONFIG_PATH, key);

    if (exists(filename)) {
        char format[16];
        snprintf(format, sizeof(format), "%%%zu[^\n]", dest_size - 1);
        file_get(fp, filename, format, dest);
        return true;
    }

    return false;
}

void _config_prepare(const char *key, char *filename, size_t filename_size)
{
    concat(filename, filename_size, CONFIG_PATH, key);

    char dir_path[STR_MAX];
    strncpy(dir_path, filename, STR_MAX - 1);
    dir_path[STR_MAX - 1] = '\0';
    dirname(dir_path);

    if (!exists(dir_path)) {
        /* dir_path is bounded to STR_MAX-1 chars by the strncpy above */
        char dir_cmd[STR_MAX + 16];
        snprintf(dir_cmd, sizeof(dir_cmd), "mkdir -p \"%s\"", dir_path);
        system(dir_cmd);
    }
}

void config_setNumber(const char *key, int value)
{
    FILE *fp;
    char filename[STR_MAX];
    _config_prepare(key, filename, sizeof(filename));
    file_put_sync(fp, filename, "%d", value);
}

void config_setString(const char *key, char *value)
{
    FILE *fp;
    char filename[STR_MAX];
    _config_prepare(key, filename, sizeof(filename));
    file_put_sync(fp, filename, "%s", value);
}

#endif // CONFIG_H__
