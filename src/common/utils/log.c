#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include "./file.h"
#include "./str.h"

static char _log_path[64] = "";
static bool _log_dirs_created = false;

void log_setName(const char *log_name)
{
    snprintf(_log_path, 63, "/mnt/SDCARD/.tmp_update/logs/%s.log", log_name);
    // Defer directory creation until first log write to avoid unnecessary I/O
    _log_dirs_created = false;
}

void log_debug(const char *file_path, int line, const char *format_str, ...)
{
    char log_message[1024];
    int msg_len;

    va_list valist;
    va_start(valist, format_str);
    // Use snprintf/vsnprintf for buffer overflow protection
    int prefix_len = snprintf(log_message, sizeof(log_message), "%s:%d>\t", file_path, line);
    if (prefix_len < 0)
        prefix_len = 0;
    if (prefix_len >= (int)sizeof(log_message))
        prefix_len = sizeof(log_message) - 1;
    int remaining = (int)sizeof(log_message) - prefix_len;
    int body_len = vsnprintf(log_message + prefix_len, remaining, format_str, valist);
    if (body_len < 0)
        body_len = 0;
    if (body_len >= remaining)
        body_len = remaining - 1;
    msg_len = prefix_len + body_len;
    va_end(valist);

    fprintf(stderr, "%s", log_message);

    // Early exit if no log path configured - avoid strlen() call
    if (_log_path[0] == '\0')
        return;

    // Create log directory only on first write (lazy initialization)
    if (!_log_dirs_created) {
        mkdirs("/mnt/SDCARD/.tmp_update/logs");
        _log_dirs_created = true;
    }

    FILE *fp;
    if ((fp = fopen(_log_path, "a+")) != NULL) {
        // Use pre-calculated message length instead of calling strlen()
        fwrite(log_message, sizeof(char), msg_len, fp);
        fflush(fp);
        fsync(fileno(fp));
        fclose(fp);
    }
}
