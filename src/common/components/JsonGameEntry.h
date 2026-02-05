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
    size_t offset = 0;
    int written;
    
    // Use snprintf for buffer overflow protection with proper bounds checking
    written = snprintf(dest, dest_size, "{");
    if (written > 0 && (size_t)written < dest_size) offset = written;
    
    written = snprintf(dest + offset, dest_size - offset, "\"label\":\"%s\",", entry->label);
    if (written > 0 && offset + written < dest_size) offset += written;
    
    written = snprintf(dest + offset, dest_size - offset, "\"launch\":\"%s\",", entry->launch);
    if (written > 0 && offset + written < dest_size) offset += written;
    
    written = snprintf(dest + offset, dest_size - offset, "\"type\":%d,", entry->type);
    if (written > 0 && offset + written < dest_size) offset += written;
    
    if (strlen(entry->imgpath) > 0) {
        written = snprintf(dest + offset, dest_size - offset, "\"imgpath\":\"%s\",", entry->imgpath);
        if (written > 0 && offset + written < dest_size) offset += written;
    }
    snprintf(dest + offset, dest_size - offset, "\"rompath\":\"%s\"}", entry->rompath);
}

#endif // JSON_GAME_ENTRY_H__
