/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_FAVORITE_STORE_H
#define MAINUI_FAVORITE_STORE_H
#include "catalog/library.h"
#include "platform/files.h"

/* Private editing transaction. Owns the original bytes and parsed records until
 * close; the model editor may adjust record metadata before commit. */
typedef struct {
    char path[4096];
    char *original;
    cJSON *records;
    MainUIFileLock *lock;
} MainUIFavoriteStore;

bool mainui_favorite_store_open(MainUIFavoriteStore *store, const char *sd);
/* Refuse an externally changed source or a competing .writing file. Writes a
 * valid backup before atomic sidecar publication; never writes stock Favorites. */
bool mainui_favorite_store_commit(MainUIFavoriteStore *store, MainUILibrary *library);
void mainui_favorite_store_close(MainUIFavoriteStore *store);
#endif
