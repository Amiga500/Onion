/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_CONSOLE_POSITION_H
#define MAINUI_CONSOLE_POSITION_H
#include "catalog/catalog.h"
#include <stdbool.h>

/* The console last selected in Games and in Expert, kept in
 * <runtime>/console-position.json while the device is on, so a MainUI started
 * again (after the Game Switcher, say) opens each section on it. Consoles are
 * named by their config file, or their folder without one, never by row. */
enum {
    MAINUI_POSITION_GAMES,
    MAINUI_POSITION_EXPERT,
    MAINUI_POSITION_SECTIONS
};

typedef struct {
    char console[MAINUI_POSITION_SECTIONS][4096];
} MainUIConsolePosition;

/* The name a console is remembered by: its config file, else its folder. */
const char *mainui_console_position_identity(const MainUIEntry *entry);
/* Missing, unreadable or damaged files read as nothing remembered. */
void mainui_console_position_load(MainUIConsolePosition *position, const char *directory);
/* Replaces the file whole (write aside, then rename); false leaves the old one. */
bool mainui_console_position_save(const MainUIConsolePosition *position, const char *directory);
#endif
