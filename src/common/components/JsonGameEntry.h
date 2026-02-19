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

    strncpy(entry.emupath, entry.rompath, STR_MAX - 1);
    entry.emupath[STR_MAX - 1] = '\0';
    (void)str_split(entry.emupath, "/../../");

    return entry;
}

/*
 * dest must hold the full JSON serialisation of entry.
 * Max size: 63 chars fixed boilerplate + 4 fields × (STR_MAX-1) chars + 2 = 1085 chars + NUL.
 * STR_MAX * 4 + 128 = 1152 provides adequate headroom.
 */
void JsonGameEntry_toJson(char dest[STR_MAX * 4 + 128], JsonGameEntry *entry)
{
    if (strlen(entry->imgpath) > 0) {
        snprintf(dest, STR_MAX * 4 + 128,
                 "{\"label\":\"%s\",\"launch\":\"%s\",\"type\":%d,"
                 "\"imgpath\":\"%s\",\"rompath\":\"%s\"}",
                 entry->label, entry->launch, entry->type,
                 entry->imgpath, entry->rompath);
    }
    else {
        snprintf(dest, STR_MAX * 4 + 128,
                 "{\"label\":\"%s\",\"launch\":\"%s\",\"type\":%d,"
                 "\"rompath\":\"%s\"}",
                 entry->label, entry->launch, entry->type,
                 entry->rompath);
    }
}

#endif // JSON_GAME_ENTRY_H__
