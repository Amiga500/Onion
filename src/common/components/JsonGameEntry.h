#ifndef JSON_GAME_ENTRY_H__
#define JSON_GAME_ENTRY_H__

#include "utils/file.h"
#include "utils/json.h"
#include "utils/str.h"

#define FAVORITES_PATH "/mnt/SDCARD/Roms/favourite.json"

typedef struct json_game_entry_s {
    char label[STR_MAX];
    char launch[STR_MAX];
    int type;
    char rompath[STR_MAX];
    char imgpath[STR_MAX];
    char emupath[STR_MAX];
} JsonGameEntry;

JsonGameEntry JsonGameEntry_fromJson(const char *json_str)
{
    JsonGameEntry entry = {.label = "",
                           .launch = "",
                           .type = 5,
                           .rompath = "",
                           .imgpath = "",
                           .emupath = ""};

    cJSON *root = cJSON_Parse(json_str);
    json_getString(root, "label", entry.label);
    json_getString(root, "launch", entry.launch);
    json_getInt(root, "type", &entry.type);
    json_getString(root, "rompath", entry.rompath);
    json_getString(root, "imgpath", entry.imgpath);
    cJSON_Delete(root);

    strcpy(entry.emupath, entry.rompath);
    str_split(entry.emupath, "/../../");

    return entry;
}

void JsonGameEntry_toJson(char dest[STR_MAX * 6], JsonGameEntry *entry)
{
    int offset = 0;
    const int max_len = STR_MAX * 6;
    
    // Build JSON string with offset tracking (much faster than strlen in loop)
    offset += snprintf(dest + offset, max_len - offset, "{");
    offset += snprintf(dest + offset, max_len - offset, "\"label\":\"%s\",", entry->label);
    offset += snprintf(dest + offset, max_len - offset, "\"launch\":\"%s\",", entry->launch);
    offset += snprintf(dest + offset, max_len - offset, "\"type\":%d,", entry->type);
    if (strlen(entry->imgpath) > 0)
        offset += snprintf(dest + offset, max_len - offset, "\"imgpath\":\"%s\",", entry->imgpath);
    offset += snprintf(dest + offset, max_len - offset, "\"rompath\":\"%s\"", entry->rompath);
    snprintf(dest + offset, max_len - offset, "}");
}

#endif // JSON_GAME_ENTRY_H__
