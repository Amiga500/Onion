#ifndef GAME_SWITCHER_STATE_RATIO_H
#define GAME_SWITCHER_STATE_RATIO_H

// Shape of a save state picture in the Load preview, without SDL, so host
// tests include it. RetroArch saves the core's raw frame (NES 256x240,
// nearly square), which looks squashed next to the game as it is played
// (thanks zazzagone). The preview takes the shape the game has on screen:
//  - RetroArch's "keep aspect" off: the game fills the 4:3 screen;
//  - otherwise the console's screen, by ROM folder, the same table as
//    Open MainUI 1.1.0 rc14's Saved Games "console ratio": 4:3, GB/GBC/GG
//    10:9, GBA 3:2;
//  - any other folder: 0, the picture keeps its own shape.

#include <stdbool.h>
#include <string.h>
#include <strings.h>

typedef struct {
    const char *folder;
    double ratio;
} GsStateRatio_s;

static const GsStateRatio_s gs_state_ratios[] = {
    {"FC", 4.0 / 3.0},
    {"NES", 4.0 / 3.0},
    {"FDS", 4.0 / 3.0},
    {"SFC", 4.0 / 3.0},
    {"SNES", 4.0 / 3.0},
    {"MD", 4.0 / 3.0},
    {"GEN", 4.0 / 3.0},
    {"GENESIS", 4.0 / 3.0},
    {"SMS", 4.0 / 3.0},
    {"MS", 4.0 / 3.0},
    {"PCE", 4.0 / 3.0},
    {"PCECD", 4.0 / 3.0},
    {"TG16", 4.0 / 3.0},
    {"PS", 4.0 / 3.0},
    {"PS1", 4.0 / 3.0},
    {"PSX", 4.0 / 3.0},
    {"GB", 10.0 / 9.0},
    {"GBC", 10.0 / 9.0},
    {"GG", 10.0 / 9.0},
    {"GBA", 3.0 / 2.0},
};

// The console folder of a ROM path: the part after "/Roms/", up to the next
// "/". False when the path has none.
static bool gs_romFolder(const char *rom_path, char *out, size_t out_size)
{
    if (rom_path == NULL || out == NULL || out_size == 0)
        return false;
    const char *start = strstr(rom_path, "/Roms/");
    if (start == NULL)
        return false;
    start += 6;
    const char *end = strchr(start, '/');
    if (end == NULL || end == start)
        return false;
    size_t len = (size_t)(end - start);
    if (len >= out_size)
        return false;
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

// Width / height for the preview of a state of this ROM, or 0 to keep the
// picture's own shape.
static double gs_stateRatio(const char *rom_path, bool keep_aspect)
{
    if (!keep_aspect)
        return 4.0 / 3.0;
    char folder[64];
    if (!gs_romFolder(rom_path, folder, sizeof(folder)))
        return 0;
    for (size_t i = 0; i < sizeof(gs_state_ratios) / sizeof(gs_state_ratios[0]); i++) {
        if (strcasecmp(folder, gs_state_ratios[i].folder) == 0)
            return gs_state_ratios[i].ratio;
    }
    return 0;
}

#endif // GAME_SWITCHER_STATE_RATIO_H
