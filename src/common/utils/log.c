#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include "./file.h"
#include "./str.h"

static char _log_path[64] = "";

void log_setName(const char *log_name)
{
    snprintf(_log_path, 63, "/mnt/SDCARD/.tmp_update/logs/%s.log", log_name);
    mkdirs("/mnt/SDCARD/.tmp_update/logs");
}

void log_debug(const char *file_path, int line, const char *format_str, ...)
{
    char log_message[1024];
    int offset;

    va_list valist;
    va_start(valist, format_str);
    
    // Use snprintf to get offset, avoiding strlen
    offset = snprintf(log_message, sizeof(log_message), "%s:%d>\t", file_path, line);
    if (offset > 0 && offset < (int)sizeof(log_message)) {
        vsnprintf(log_message + offset, sizeof(log_message) - offset, format_str, valist);
    }
    va_end(valist);

    fprintf(stderr, "%s", log_message);

    if (_log_path[0] == '\0')  // Faster than strlen for empty check
        return;

    FILE *fp;
    if ((fp = fopen(_log_path, "a+")) != NULL) {
        // fputs is faster than fwrite for strings
        fputs(log_message, fp);
        fflush(fp);
        fsync(fileno(fp));
        fclose(fp);
    }
}
