#ifndef CONFIG_H__
#define CONFIG_H__

#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
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
    concat(hidden_flag, key, "_");
    flag_set(CONFIG_PATH, key, value);
    flag_set(CONFIG_PATH, hidden_flag, !value);
}

bool config_get(const char *key, const char *format, void *dest)
{
    FILE *fp;

    char filename[STR_MAX];
    concat(filename, CONFIG_PATH, key);

    if (exists(filename)) {
        file_get(fp, filename, format, dest);
        return true;
    }

    return false;
}

void _config_prepare(const char *key, char *filename)
{
    concat(filename, CONFIG_PATH, key);

    char dir_path[STR_MAX];
    strncpy(dir_path, filename, STR_MAX - 1);
    dir_path[STR_MAX - 1] = '\0';
    dirname(dir_path);

    mkdirs(dir_path);
}

void config_setNumber(const char *key, int value)
{
    char filename[STR_MAX];
    char buf[32];
    _config_prepare(key, filename);
    int len = snprintf(buf, sizeof(buf), "%d", value);
    if (len > 0 && (size_t)len < sizeof(buf))
        file_atomic_write(filename, buf, (size_t)len);
}

void config_setString(const char *key, const char *value)
{
    char filename[STR_MAX];
    _config_prepare(key, filename);
    if (value == NULL)
        value = "";
    file_atomic_write(filename, value, strlen(value));
}

#endif // CONFIG_H__
