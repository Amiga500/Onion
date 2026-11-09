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

// Built with cJSON so quotes and backslashes in a label or path are
// escaped: sprintf wrote them raw, and the line became invalid JSON.
bool JsonGameEntry_toJson(char dest[STR_MAX * 6], JsonGameEntry *entry)
{
    dest[0] = '\0';
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return false;
    cJSON_AddStringToObject(root, "label", entry->label);
    cJSON_AddStringToObject(root, "launch", entry->launch);
    cJSON_AddNumberToObject(root, "type", entry->type);
    if (entry->imgpath[0] != '\0')
        cJSON_AddStringToObject(root, "imgpath", entry->imgpath);
    cJSON_AddStringToObject(root, "rompath", entry->rompath);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL)
        return false;
    int n = snprintf(dest, STR_MAX * 6, "%s", json);
    cJSON_free(json);
    if (n < 0 || n >= STR_MAX * 6) {
        dest[0] = '\0'; // a cut line would be invalid JSON
        return false;
    }
    return true;
}

#endif // JSON_GAME_ENTRY_H__
