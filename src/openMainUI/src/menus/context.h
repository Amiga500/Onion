/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_CONTEXT_H
#define MAINUI_CONTEXT_H
#include "menus/menu.h"

typedef enum {
    CONTEXT_REFRESH,
    CONTEXT_SEARCH,
    CONTEXT_RECENTS,
    CONTEXT_FAVORITES,
    CONTEXT_GAMES,
    CONTEXT_APPS,
    CONTEXT_SETTINGS,
    CONTEXT_EXPERT,
    CONTEXT_THEMES,
    CONTEXT_TWEAKS,
    CONTEXT_SHUTDOWN,
    CONTEXT_CUSTOM1,
    CONTEXT_CUSTOM2,
    CONTEXT_CUSTOM3,
    CONTEXT_COUNT,
    CONTEXT_START,
    CONTEXT_ADD_FAVORITE,
    CONTEXT_REMOVE_RECENT,
    CONTEXT_CLEAR_RECENT,
    CONTEXT_REMOVE_FAVORITE,
    CONTEXT_DELETE_ROM,
    CONTEXT_REFRESH_SYSTEM,
    CONTEXT_FAVORITE_MOVE,
    CONTEXT_FAVORITE_PASTE,
    CONTEXT_FAVORITE_CREATE,
    CONTEXT_FAVORITE_RENAME,
    CONTEXT_FAVORITE_DELETE,
    CONTEXT_FAVORITE_SORT
} MainUIContextAction;

typedef struct {
    MainUIContextAction action;
    bool enabled;
    char label[128], launch[512];
    int type;
} MainUIContextEntry;

typedef struct {
    MainUIContextEntry entries[CONTEXT_COUNT];
    int count, visible[CONTEXT_COUNT], visible_count, selected;
    bool hotkey;
} MainUIContext;

/* Ordered SELECT menu, including hidden registered entries and bounded custom
 * actions. Parsing never executes a launcher or reads a language file. */
void mainui_context_load(MainUIContext *context, const char *directory, const char *sd);
void mainui_context_parse(MainUIContext *context, const char *text, bool search, bool tweaks);
/* Construct a stock list popup; caller supplies the section-specific inventory. */
void mainui_context_rows(MainUIContext *context, const MainUIContextAction *actions, int count);
void mainui_context_reveal(MainUIContext *context);
const char *mainui_context_label(const MainUIContextEntry *entry);
int mainui_context_section(MainUIContextAction action);
#endif
