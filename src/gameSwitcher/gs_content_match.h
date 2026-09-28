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
    if (content_info == NULL || content_name == NULL || content_name[0] == '\0')
        return false;

    // Check every occurrence: the first one may be part of a longer field
    // (e.g. "Super Mario" inside "Super Mario World" before "Super Mario").
    size_t name_len = strlen(content_name);
    for (const char *found = strstr(content_info, content_name); found != NULL;
         found = strstr(found + 1, content_name)) {
        bool left_ok = found == content_info || *(found - 1) == ',';
        bool right_ok = found[name_len] == ',';
        if (left_ok && right_ok)
            return true;
    }
    return false;
}

#endif // GS_CONTENT_MATCH_H__
