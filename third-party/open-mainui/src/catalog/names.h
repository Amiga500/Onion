/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_NAMES_H
#define MAINUI_NAMES_H
#include <stdbool.h>
#include <stddef.h>

/* A caller-owned result copied from the process's current SD map snapshot. */
typedef struct {
    char *text;
} MainUINameLookup;

/* Borrowed until the next lookup/close on this handle. Missing maps/keys return
 * fallback. Keys are case-sensitive; a present empty value stays empty.
 * Maps over 8 MiB or 131072 lines use fallback to bound text and index memory.
 * Reopening catalogs does not reload the map, including a missing map. */
const char *mainui_name_lookup(MainUINameLookup *lookup, const char *sd, const char *key,
                               const char *fallback);
void mainui_names_close(MainUINameLookup *lookup);
#endif
