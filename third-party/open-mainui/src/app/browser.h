/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_BROWSER_H
#define MAINUI_BROWSER_H
#include "catalog/catalog.h"
/* Logical screen rows add a parent above the unchanged catalog/cache window.
 * A negative data index denotes the synthetic parent, never a database row. */
void mainui_browser_grid_restore(const MainUICatalog *, MainUIViewport *, int selected);
int mainui_browser_count(const MainUICatalog *catalog);
int mainui_browser_index(const MainUICatalog *catalog, int row);
const char *mainui_browser_label(MainUICatalog *catalog, int row);
bool mainui_browser_folder(MainUICatalog *catalog, int row);
/* Enter/Back owns viewport save/restore. Failure preserves the active view.
 * Borrowed catalog strings must not be retained across either operation. */
bool mainui_browser_enter(MainUICatalog *catalog, MainUIViewport *view, int rows);
bool mainui_browser_back(MainUICatalog *catalog, MainUIViewport *view);
/* Invalidate derived Emu/RApp caches without rebuilding or scanning ROM trees.
 * Callers close active readers first. Rebuild is deferred until console entry. */
bool mainui_browser_refresh_all(const char *sd, bool case_sensitive, char error[256]);
bool mainui_browser_refresh_control(const char *sd, bool case_sensitive, char error[256],
                                    MainUICancel cancel);
/* Confirmed deletion only; rejects parent/folder/out-of-root and linked paths. */
bool mainui_browser_delete(MainUICatalog *catalog, MainUIViewport *view, int rows);
#endif
