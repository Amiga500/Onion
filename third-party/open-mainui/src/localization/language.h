/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_LANGUAGE_H
#define MAINUI_LANGUAGE_H
#include <stdbool.h>
#define MAINUI_LANG_LIMIT 256

typedef struct {
    char *path, *name, *filename;
} MainUILanguageEntry;

typedef struct {
    MainUILanguageEntry entries[MAINUI_LANG_LIMIT];
    int count, selected, start;
} MainUILanguages;

/* Discovery of all languages is explicit and on demand (Settings). */
bool mainui_languages_open(MainUILanguages *list, const char *sd, const char *fallback);
void mainui_languages_close(MainUILanguages *list);
bool mainui_language_select(const MainUILanguages *list, const char *sd);
/* Load the language saved in system.json at startup: one file read, from
 * miyoo/app/lang, then lang_backup, then <fallback>/lang. Without it, the
 * built-in English strings are used. */
bool mainui_language_load(const char *sd, const char *fallback);
const char *mainui_translate(int id, const char *fallback);
void mainui_language_close(void);
#endif
