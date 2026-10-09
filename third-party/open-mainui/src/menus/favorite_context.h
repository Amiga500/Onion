/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_FAVORITE_CONTEXT_H
#define MAINUI_FAVORITE_CONTEXT_H
#include "catalog/library.h"
#include "menus/context.h"

/* Build the patcher's Favorite popup from the current logical view. The synthetic
 * parent is never an editable Favorite. Borrow all inputs; no files are written.
 * pending_move comes from the editing controller, not inferred from selection. */
void mainui_favorite_context(MainUIContext *context, const MainUILibrary *library, int selected,
                             bool pending_move);
#endif
