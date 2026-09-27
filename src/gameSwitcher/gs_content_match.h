#ifndef GS_CONTENT_MATCH_H__
#define GS_CONTENT_MATCH_H__

#include <stdbool.h>
#include <string.h>

// RetroArch GET_STATUS gives "core,content name,crc32=...": the GameSwitcher
// overlay checks that the running content is its entry 0. The name must be
// a whole comma-separated field (start of the string or a comma before it,
// a comma after it). SDL-free so host tests include it.
static bool _isContentNameInInfo(const char *content_info, const char *content_name)
{
    const char *found = strstr(content_info, content_name);
    if (found != NULL) {
        bool left_ok = found == content_info || *(found - 1) == ',';
        bool right_ok = *(found + strlen(content_name)) == ',';
        return left_ok && right_ok;
    }
    return false;
}

#endif // GS_CONTENT_MATCH_H__
