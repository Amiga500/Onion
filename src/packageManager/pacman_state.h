#ifndef PACMAN_STATE_H__
#define PACMAN_STATE_H__

// Package lists and pending changes of the Package Manager, without SDL,
// so the change bookkeeping (changes.h, listActions.h) can be tested on a
// host. globals.h includes it.

#include <stdbool.h>

#include "utils/str.h"

#define PACKAGE_DIR "/mnt/SDCARD/App/PackageManager/data/"

// Max number of records in the DB
#define LAYER_ITEM_COUNT 200
#define MAX_LAYER_NAME_SIZE 256
#define MAY_LAYER_DISPLAY 35

typedef struct package_s {
    char name[STR_MAX];
    bool installed;
    bool changed;
    bool complete;
    bool has_roms;
} Package;

static const int tab_count = 4;
static const int summary_tab = tab_count - 1;

static Package packages[4][LAYER_ITEM_COUNT];
static int package_count[] = {0, 0, 0, 0};
static int package_installed_count[] = {0, 0, 0, 0};
static int changes_installs[] = {0, 0, 0, 0};
static int changes_removals[] = {0, 0, 0, 0};

#endif // PACMAN_STATE_H__
