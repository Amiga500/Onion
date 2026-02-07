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
    size_t dest_size = STR_MAX * 6;
    size_t len = 0;
    int n;
    dest[0] = '{';
    dest[1] = '\0';
    len = 1;
    n = snprintf(dest + len, dest_size - len, "\"label\":\"%s\",", entry->label);
    if (n > 0) len += (size_t)n < dest_size - len ? (size_t)n : dest_size - len - 1;
    n = snprintf(dest + len, dest_size - len, "\"launch\":\"%s\",", entry->launch);
    if (n > 0) len += (size_t)n < dest_size - len ? (size_t)n : dest_size - len - 1;
    n = snprintf(dest + len, dest_size - len, "\"type\":%d,", entry->type);
    if (n > 0) len += (size_t)n < dest_size - len ? (size_t)n : dest_size - len - 1;
    if (strlen(entry->imgpath) > 0) {
        n = snprintf(dest + len, dest_size - len, "\"imgpath\":\"%s\",", entry->imgpath);
        if (n > 0) len += (size_t)n < dest_size - len ? (size_t)n : dest_size - len - 1;
    }
    n = snprintf(dest + len, dest_size - len, "\"rompath\":\"%s\"}", entry->rompath);
}

#endif // JSON_GAME_ENTRY_H__
