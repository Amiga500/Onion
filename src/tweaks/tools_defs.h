#ifndef TWEAKS_TOOLS_DEFS_H__
#define TWEAKS_TOOLS_DEFS_H__

#include "utils/str.h"

#define NUM_TOOLS 7

static char tools_short_names[NUM_TOOLS][STR_MAX] = {
    "cue_gen",
    "m3u_gen_sd",
    "m3u_gen_md",
    "build_short_rom_game_list",
    "miyoogamelist_gen",
    "sort_apps_az",
    "sort_apps_za"};

// The names shown for them (Tweaks > Buttons, the tool's own window): the
// short names above are the saved values and the script names, and showed
// as "Tool: build_short_rom_game_list" (UI review, 4.5.1).
static const char *tools_names[NUM_TOOLS] = {
    "Make CUE files",
    "Make M3U files (one folder)",
    "Make M3U files (per game)",
    "Game list for short names",
    "Make miyoogamelist",
    "Sort apps A to Z",
    "Sort apps Z to A"};

#endif // TWEAKS_TOOLS_DEFS_H__
