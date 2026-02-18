#ifndef TWEAKS_TOOLS_DEFS_H__
#define TWEAKS_TOOLS_DEFS_H__

#include "utils/str.h"

#define NUM_TOOLS 7

/* Longest entry is "build_short_rom_game_list" = 25 chars; [32] provides headroom. */
static char tools_short_names[NUM_TOOLS][32] = {
    "cue_gen",
    "m3u_gen_sd",
    "m3u_gen_md",
    "build_short_rom_game_list",
    "miyoogamelist_gen",
    "sort_apps_az",
    "sort_apps_za"};

#endif // TWEAKS_TOOLS_DEFS_H__
