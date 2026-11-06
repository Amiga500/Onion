/* SPDX-License-Identifier: GPL-3.0-only */
#include "menus/game_menu.h"
#include "platform/files.h"
#include "platform/save_states.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *id;
    MainUIContextAction action, alternate;
} GameMenuItem;

/* Each section's IDs in their default order. The alternate is the same row
 * in another state: Move here during a move, Remove for a Favorite. */
static const GameMenuItem roms[] = {{"launch", CONTEXT_START, CONTEXT_START},
                                    {"favorite", CONTEXT_ADD_FAVORITE, CONTEXT_REMOVE_FAVORITE},
                                    {"delete", CONTEXT_DELETE_ROM, CONTEXT_DELETE_ROM},
                                    {"search", CONTEXT_SEARCH, CONTEXT_SEARCH},
                                    {"refresh", CONTEXT_REFRESH_SYSTEM, CONTEXT_REFRESH_SYSTEM}};
static const GameMenuItem favorites[] = {
    {"move", CONTEXT_FAVORITE_MOVE, CONTEXT_FAVORITE_PASTE},
    {"create_folder", CONTEXT_FAVORITE_CREATE, CONTEXT_FAVORITE_CREATE},
    {"sort", CONTEXT_FAVORITE_SORT, CONTEXT_FAVORITE_SORT},
    {"remove_favorite", CONTEXT_REMOVE_FAVORITE, CONTEXT_REMOVE_FAVORITE},
    {"rename_folder", CONTEXT_FAVORITE_RENAME, CONTEXT_FAVORITE_RENAME},
    {"delete_folder", CONTEXT_FAVORITE_DELETE, CONTEXT_FAVORITE_DELETE}};
static const GameMenuItem recents[] = {
    {"launch", CONTEXT_START, CONTEXT_START},
    {"remove_recent", CONTEXT_REMOVE_RECENT, CONTEXT_REMOVE_RECENT},
    {"clear_recents", CONTEXT_CLEAR_RECENT, CONTEXT_CLEAR_RECENT}};
static const GameMenuItem search_results[] = {
    {"launch", CONTEXT_START, CONTEXT_START},
    {"favorite", CONTEXT_ADD_FAVORITE, CONTEXT_REMOVE_FAVORITE}};
static const GameMenuItem consoles[] = {
    {"refresh_all", CONTEXT_REFRESH, CONTEXT_REFRESH},
    {"refresh", CONTEXT_REFRESH_SYSTEM, CONTEXT_REFRESH_SYSTEM}};

static const struct {
    const char *key;
    const GameMenuItem *items;
    int count;
} sections[MAINUI_GAME_MENU_ALL] = {
    {"roms", roms, (int)(sizeof roms / sizeof *roms)},
    {"favorites", favorites, (int)(sizeof favorites / sizeof *favorites)},
    {"recents", recents, (int)(sizeof recents / sizeof *recents)},
    {"search_results", search_results, (int)(sizeof search_results / sizeof *search_results)},
    {"menu", consoles, (int)(sizeof consoles / sizeof *consoles)}};

/* The media menu's IDs, by MAINUI_DETAILS_*. */
static const char *const details_ids[MAINUI_DETAILS_ROWS] = {
    "game_info", "gallery", "saved_games", "maps", "walkthrough", "manual"};

void mainui_game_menu_defaults(MainUIGameMenu *menu)
{
    *menu = (MainUIGameMenu){.resume_mode = MAINUI_RESUME_BACKUP,
                             .saves_upscale = true,
                             .saves_correct_aspect = true,
                             .saves_framed = true,
                             .saves_disabled_count = 1,
                             .saves_disabled = {"ARCADE"},
                             .gallery_right = true,
                             .gallery_upscale = true};
    for (int i = 0; i < MAINUI_DETAILS_ROWS; i++) {
        menu->details[menu->details_count++] = i;
    }
}

bool mainui_game_menu_details(const MainUIGameMenu *menu, int row)
{
    for (int i = 0; i < menu->details_count; i++) {
        if (menu->details[i] == row) {
            return true;
        }
    }
    return false;
}

/* "details": known IDs once each, in order; anything but an array keeps the
 * default. "saved_games": {"resume_mode": "overwrite", "backup" or "swap",
 * "left_shortcut", "upscale_preview", "show_auto" and
 * "correct_aspect_ratio": true or false, "layout": "framed" or "overlay"}. */
static void parse_details(MainUIGameMenu *menu, const cJSON *root)
{
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "details");
    if (cJSON_IsArray(list)) {
        menu->details_count = 0;
        const cJSON *id;
        cJSON_ArrayForEach(id, list)
        {
            for (int i = 0; cJSON_IsString(id) && i < MAINUI_DETAILS_ROWS; i++) {
                if (!strcmp(id->valuestring, details_ids[i]) &&
                    !mainui_game_menu_details(menu, i)) {
                    menu->details[menu->details_count++] = i;
                }
            }
        }
    }
    const cJSON *gallery = cJSON_GetObjectItemCaseSensitive(root, "gallery");
    const cJSON *right = cJSON_GetObjectItemCaseSensitive(gallery, "right_shortcut");
    if (cJSON_IsBool(right)) {
        menu->gallery_right = cJSON_IsTrue(right);
    }
    const cJSON *enlarge = cJSON_GetObjectItemCaseSensitive(gallery, "upscale_images");
    if (cJSON_IsBool(enlarge)) {
        menu->gallery_upscale = cJSON_IsTrue(enlarge);
    }
    const cJSON *saves = cJSON_GetObjectItemCaseSensitive(root, "saved_games");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(saves, "resume_mode");
    if (cJSON_IsString(mode)) {
        menu->resume_mode = !strcmp(mode->valuestring, "overwrite") ? MAINUI_RESUME_OVERWRITE
                            : !strcmp(mode->valuestring, "swap")    ? MAINUI_RESUME_SWAP
                                                                    : MAINUI_RESUME_BACKUP;
    }
    const cJSON *left = cJSON_GetObjectItemCaseSensitive(saves, "left_shortcut");
    if (cJSON_IsBool(left)) {
        menu->saves_left = cJSON_IsTrue(left);
    }
    const cJSON *upscale = cJSON_GetObjectItemCaseSensitive(saves, "upscale_preview");
    if (cJSON_IsBool(upscale)) {
        menu->saves_upscale = cJSON_IsTrue(upscale);
    }
    const cJSON *aspect = cJSON_GetObjectItemCaseSensitive(saves, "correct_aspect_ratio");
    if (cJSON_IsBool(aspect)) {
        menu->saves_correct_aspect = cJSON_IsTrue(aspect);
    }
    /* An array replaces the default list; entries that are not names of a
     * folder, or repeat one, are skipped. */
    const cJSON *disabled = cJSON_GetObjectItemCaseSensitive(saves, "disabled_rom_dirs");
    if (cJSON_IsArray(disabled)) {
        menu->saves_disabled_count = 0;
        const cJSON *item;
        cJSON_ArrayForEach(item, disabled)
        {
            const char *name = cJSON_IsString(item) ? item->valuestring : "";
            size_t length = strlen(name);
            bool usable = length && length < sizeof menu->saves_disabled[0] && !strchr(name, '/') &&
                          strcmp(name, ".") && strcmp(name, "..") &&
                          menu->saves_disabled_count < MAINUI_SAVES_DISABLED_MAX;
            for (int i = 0; usable && i < menu->saves_disabled_count; i++) {
                usable = strcasecmp(menu->saves_disabled[i], name) != 0;
            }
            if (usable) {
                strcpy(menu->saves_disabled[menu->saves_disabled_count++], name);
            }
        }
    }
    const cJSON *layout = cJSON_GetObjectItemCaseSensitive(saves, "layout");
    if (cJSON_IsString(layout)) {
        menu->saves_framed = strcmp(layout->valuestring, "overlay") != 0;
    }
    const cJSON *autosave = cJSON_GetObjectItemCaseSensitive(saves, "show_auto");
    if (cJSON_IsBool(autosave)) {
        menu->saves_auto = cJSON_IsTrue(autosave);
    }
}

bool mainui_game_menu_parse(MainUIGameMenu *menu, const char *text)
{
    mainui_game_menu_defaults(menu);
    if (!text) {
        return true;
    }
    cJSON *root = mainui_menu_json(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }
    for (int s = 0; s < MAINUI_GAME_MENU_SECTIONS; s++) {
        mainui_game_menu_parse_list(menu, s,
                                    cJSON_GetObjectItemCaseSensitive(root, sections[s].key));
    }
    parse_details(menu, root);
    cJSON_Delete(root);
    return true;
}

void mainui_game_menu_parse_list(MainUIGameMenu *menu, MainUIGameMenuSection s, const cJSON *list)
{
    if (!cJSON_IsArray(list) || (int)s < 0 || s >= MAINUI_GAME_MENU_ALL) {
        return;
    }
    menu->configured[s] = true;
    menu->count[s] = 0;
    const cJSON *id;
    cJSON_ArrayForEach(id, list)
    {
        if (!cJSON_IsString(id)) {
            continue;
        }
        for (int i = 0; i < sections[s].count; i++) {
            if (strcmp(id->valuestring, sections[s].items[i].id)) {
                continue;
            }
            bool seen = false;
            for (int k = 0; k < menu->count[s]; k++) {
                seen |= menu->order[s][k] == i;
            }
            if (!seen && menu->count[s] < MAINUI_GAME_MENU_ITEMS) {
                menu->order[s][menu->count[s]++] = i;
            }
            break;
        }
    }
}

void mainui_game_menu_load(MainUIGameMenu *menu, const char *directory)
{
    char path[4096];
    int n = directory ? snprintf(path, sizeof path, "%s/game-menu.json", directory) : -1;
    char *text = n > 0 && n < (int)sizeof path ? mainui_read_text(path, 128 * 1024) : NULL;
    if (!mainui_game_menu_parse(menu, text)) {
        fprintf(stderr, "[menu] %s is not a JSON object; the game menus are the default ones\n",
                path);
    }
    free(text);
}

void mainui_game_menu_apply(MainUIContext *context, const MainUIGameMenu *menu,
                            MainUIGameMenuSection section)
{
    if (!menu || (int)section < 0 || section >= MAINUI_GAME_MENU_ALL ||
        !menu->configured[section]) {
        return;
    }
    MainUIContextAction actions[CONTEXT_COUNT];
    int count = 0;
    for (int k = 0; k < menu->count[section]; k++) {
        const GameMenuItem *item = &sections[section].items[menu->order[section][k]];
        for (int v = 0; v < context->visible_count && count < CONTEXT_COUNT; v++) {
            MainUIContextAction action = context->entries[context->visible[v]].action;
            if (action == item->action || action == item->alternate) {
                actions[count++] = action;
                break;
            }
        }
    }
    mainui_context_rows(context, actions, count);
}

bool mainui_game_menu_saves_disabled(const MainUIGameMenu *menu, const char *rom_root)
{
    if (!menu || !rom_root) {
        return false;
    }
    const char *end = rom_root + strlen(rom_root);
    while (end > rom_root && end[-1] == '/') {
        end--;
    }
    const char *name = end;
    while (name > rom_root && name[-1] != '/') {
        name--;
    }
    size_t length = (size_t)(end - name);
    for (int i = 0; length && i < menu->saves_disabled_count; i++) {
        if (strlen(menu->saves_disabled[i]) == length &&
            !strncasecmp(menu->saves_disabled[i], name, length)) {
            return true;
        }
    }
    return false;
}
