/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_DELETE_H
#define MAINUI_DELETE_H
#include <stdbool.h>
/* Caller holds the cache writer lock. Conflicting live/staged ROMs are preserved. */
bool mainui_delete_recover(const char *cache, const char *table, const char *root, const char *sd);
/* Caller holds the cache writer lock. Used by Refresh roms after a refused
 * recovery: removes only the journal, never the original or staged ROM, so the
 * worst case is an extra ROM copy left on the card. True if no journal remains. */
bool mainui_delete_abandon(const char *cache, const char *sd);
/* True if any directory entry exists at the journal path (lstat semantics, so a
 * dangling symlink counts). False only when it is absent. */
bool mainui_delete_journal_present(const char *cache);
/* Read-only diagnostic; never scans the console or attempts recovery. */
void mainui_delete_pending_error(const char *cache, const char *sd, char error[256]);
bool mainui_delete_prepare(const char *cache, const char *original, const char *key,
                           char staged[4096]);
#endif
