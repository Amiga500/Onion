/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_MEDIA_MAPS_H
#define MAINUI_MEDIA_MAPS_H

#define MAINUI_MEDIA_MAPS_MAX 32

typedef struct {
    char path[4096];
    int number; /* 0 = <name>.<ext>, 1 = <name>-1.<ext>, ... */
} MainUIMap;

/* Only <console ROM root>/media/maps is used, with filenames matching
 * <ROM basename>.png and <ROM basename>-N.png, the extension in any case. The
 * ROM may be in a subdirectory; media is always relative to the console root.
 * In number order, at most capacity, one per number, found only when the
 * gallery is opened. */
int mainui_maps_find(const char *rom_root, const char *rom, MainUIMap *maps, int capacity);

#endif
