#ifndef GAME_SWITCHER_ROMSCREEN_FIND_H
#define GAME_SWITCHER_ROMSCREEN_FIND_H

// Which picture the GameSwitcher shows for a recent game: its own capture
// (ROM_SCREENS_DIR/<hash of the ROM path>.png) first, then the ROM's
// artwork. SDL-free so host tests include it; gs_romscreen.h wraps it.

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils/file.h"
#include "utils/hash.h"
#include "utils/log.h"

#ifndef ROM_SCREENS_DIR // host tests point it at a temporary folder
#define ROM_SCREENS_DIR "/mnt/SDCARD/Saves/CurrentProfile/romScreens"
#endif

typedef enum {
    ROM_SCREEN_NONE = 0,
    ROM_SCREEN_STATE,
    ROM_SCREEN_HASH,
    ROM_SCREEN_ARTWORK
} RomScreenType_e;

static RomScreenType_e findRomScreenPaths(const char *rompath, const char *imgpath,
                                          char *currPicture, size_t currPicture_size)
{
    if (currPicture == NULL || currPicture_size == 0)
        return ROM_SCREEN_NONE;

    // Check if hashed rom screen exists
    uint32_t hash = FNV1A_Pippip_Yurii(rompath, strlen(rompath));
    snprintf(currPicture, currPicture_size, ROM_SCREENS_DIR "/%" PRIu32 ".png", hash);
    printf_debug("Checking for hashed rom screen: %s\n", currPicture);
    if (exists(currPicture)) {
        return ROM_SCREEN_HASH;
    }

    // Check if artwork exists
    snprintf(currPicture, currPicture_size, "%s", imgpath);
    printf_debug("Checking for artwork: %s\n", currPicture);
    if (exists(currPicture)) {
        return ROM_SCREEN_ARTWORK;
    }

    return ROM_SCREEN_NONE;
}

#endif // GAME_SWITCHER_ROMSCREEN_FIND_H
