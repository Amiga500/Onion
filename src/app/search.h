/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_SEARCH_H
#define MAINUI_SEARCH_H
#include "catalog/catalog.h"
#include "catalog/library.h"

typedef struct {
    MainUILibrary *results;
    MainUIViewport source_view, view;
    char query[128], title[512], error[256];
    bool postgame, release_pending;
} MainUISearch;

/* Search the entire active console cache, including nested directories. Results
 * own canonical records; source catalog and its viewport remain unchanged. */
bool mainui_search_open(MainUISearch *search, MainUICatalog *catalog, const MainUIViewport *source,
                        const char *query, int rows);
bool mainui_search_restore_view(MainUISearch *search, const cJSON *saved, const cJSON *record,
                                int rows);
void mainui_search_close(MainUISearch *search);
#endif
