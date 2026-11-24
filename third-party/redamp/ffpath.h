/* Shared FFmpeg input-path validation used by the private audio bridge.
 * Keep this dependency-free so host tests can exercise path rules without
 * needing the ARM FFmpeg headers/libraries. */
#ifndef REDAMP_FFPATH_H
#define REDAMP_FFPATH_H
#include <stddef.h>
#include <string.h>
#include <strings.h>

static inline int ffpath_allowed(const char *path, int live)
{
    if (!path || !*path || strlen(path) > 1800) return 0;
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        /* Filenames may contain spaces, e.g. "AAC - 5MINS - LARGE.aac".
         * Never admit controls or spaces in remote URLs. */
        if (*p < 32 || *p == 127 || (live && *p == 32)) return 0;
    }
    if (live)
        return !strncasecmp(path, "http://", 7) || !strncasecmp(path, "https://", 8);
    return path[0] == '/' && strstr(path, "://") == NULL;
}
#endif
