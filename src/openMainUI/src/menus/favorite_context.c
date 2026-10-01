/* SPDX-License-Identifier: GPL-3.0-only */
#include "menus/favorite_context.h"
#include <limits.h>

void mainui_favorite_context(MainUIContext *context, const MainUILibrary *library, int selected,
                             bool pending_move)
{
    MainUIContextAction actions[4] = {0};
    int count = 0;
    if (!library || library->recent) {
        mainui_context_rows(context, actions, 0);
        return;
    }
    int row = INT_MIN;
    if (selected >= 0 && selected < library->visible_count) {
        row = library->visible[selected];
    }
    bool item = row != INT_MIN;
    bool folder = item && row < 0;
    int depth = 0;
    for (int parent = library->current; parent >= 0 && depth < 3;
         parent = library->folders[parent].parent) {
        depth++;
    }
    /* The folder menu deliberately puts Move first, matching stock ordering.
     * Empty/parent selections still permit Create, but never Rename/Remove. */
    if (pending_move) {
        actions[count++] = CONTEXT_FAVORITE_PASTE;
    }
    else if (item) {
        actions[count++] = CONTEXT_FAVORITE_MOVE;
    }
    if (depth < 3) {
        actions[count++] = CONTEXT_FAVORITE_CREATE;
    }
    if (folder) {
        actions[count++] = CONTEXT_FAVORITE_RENAME;
        actions[count++] = CONTEXT_FAVORITE_DELETE;
    }
    else if (item) {
        actions[count++] = CONTEXT_FAVORITE_SORT;
        actions[count++] = CONTEXT_REMOVE_FAVORITE;
    }
    mainui_context_rows(context, actions, count);
}
