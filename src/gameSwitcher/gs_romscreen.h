#ifndef GAME_SWITCHER_ROMSCREEN_H
#define GAME_SWITCHER_ROMSCREEN_H

#include <SDL/SDL_image.h>

#include "system/screenshot.h"

#include "gs_model.h"
#include "gs_retroarch.h"

static bool __initial_romscreens_loaded = false;
static pthread_t romscreen_thread_pt;
static pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

void unloadRomScreen(int index)
{
    if (index < 0 || index >= game_list_len)
        return;
    Game_s *game = &game_list[index];

    if (game->romScreen != NULL) {
        SDL_FreeSurface(game->romScreen);
        game->romScreen = NULL;
    }
}

typedef enum {
    ROM_SCREEN_NONE = 0,
    ROM_SCREEN_STATE,
    ROM_SCREEN_HASH,
    ROM_SCREEN_ARTWORK
} RomScreenType_e;

/**
 * @brief Generate the standard Imgs folder artwork path from a ROM path
 * 
 * Given a rompath like "/mnt/SDCARD/Roms/FC/game.nes", generates
 * the artwork path "/mnt/SDCARD/Roms/FC/Imgs/game.png"
 * 
 * @param rompath The full path to the ROM file
 * @param out_imgpath Output buffer for the generated image path
 * @param out_size Size of the output buffer
 * @return true if path was generated successfully, false otherwise
 */
static bool generateArtworkPath(const char *rompath, char *out_imgpath, size_t out_size)
{
    // Validate input
    if (rompath == NULL || out_imgpath == NULL || out_size == 0) {
        return false;
    }

    // Check if rompath starts with expected prefix
    const char *roms_prefix = "/mnt/SDCARD/Roms/";
    if (strncmp(rompath, roms_prefix, strlen(roms_prefix)) != 0) {
        return false;
    }

    // Extract the relative path after /mnt/SDCARD/Roms/
    const char *rel_path = rompath + strlen(roms_prefix);
    
    // Find the first slash to get the system folder
    const char *slash = strchr(rel_path, '/');
    if (slash == NULL) {
        return false;
    }
    
    // Extract system folder name
    size_t sys_len = slash - rel_path;
    char sys_folder[STR_MAX];
    if (sys_len >= sizeof(sys_folder)) {
        return false;
    }
    strncpy(sys_folder, rel_path, sys_len);
    sys_folder[sys_len] = '\0';
    
    // Get the ROM filename without extension
    char *rom_name = file_removeExtension(file_basename(rompath));
    if (rom_name == NULL) {
        return false;
    }
    
    // Generate the Imgs path: /mnt/SDCARD/Roms/{SYSTEM}/Imgs/{ROM_NAME}.png
    int written = snprintf(out_imgpath, out_size, "/mnt/SDCARD/Roms/%s/Imgs/%s.png", 
                           sys_folder, rom_name);
    free(rom_name);
    
    return written > 0 && (size_t)written < out_size;
}

RomScreenType_e findRomScreen(const Game_s *game, char *currPicture)
{
    // Check if save state image exists
    // if (strlen(game->core_name) != 0) {
    //     sprintf(currPicture, STATES_DIR "/%s/%s.state.auto.png", game->core_name, game->rom_name);
    //     printf_debug("Checking for save state image: %s\n", currPicture);
    //     if (exists(currPicture)) {
    //         return ROM_SCREEN_STATE;
    //     }
    // }

    // Check if hashed rom screen exists
    uint32_t hash = FNV1A_Pippip_Yurii(game->recentItem.rompath, strlen(game->recentItem.rompath));
    sprintf(currPicture, ROM_SCREENS_DIR "/%" PRIu32 ".png", hash);
    printf_debug("Checking for hashed rom screen: %s\n", currPicture);
    if (exists(currPicture)) {
        return ROM_SCREEN_HASH;
    }

    // Check if artwork exists from imgpath stored in recentlist.json
    if (strlen(game->recentItem.imgpath) > 0) {
        strcpy(currPicture, game->recentItem.imgpath);
        printf_debug("Checking for artwork: %s\n", currPicture);
        if (exists(currPicture)) {
            return ROM_SCREEN_ARTWORK;
        }
    }

    // Fallback: Try to generate standard Imgs folder path from rompath
    // This handles cases where:
    // 1. imgpath in recentlist.json was empty
    // 2. imgpath pointed to a deleted file (e.g., after OS update)
    // 3. romScreens folder was deleted during update
    char generated_imgpath[STR_MAX * 2];
    if (generateArtworkPath(game->recentItem.rompath, generated_imgpath, sizeof(generated_imgpath))) {
        printf_debug("Checking for generated artwork path: %s\n", generated_imgpath);
        if (exists(generated_imgpath)) {
            strcpy(currPicture, generated_imgpath);
            return ROM_SCREEN_ARTWORK;
        }
    }

    return ROM_SCREEN_NONE;
}

typedef struct {
    bool keepAspect;
    bool integerScaling;
} ScalingMode_s;

void scaleRomScreen(Game_s *game, ScalingMode_s mode)
{
    // Zoom the image to fit the screen
    double zx = (double)(DISPLAY_WIDTH) / game->romScreen->w;
    double zy = (double)(DISPLAY_HEIGHT) / game->romScreen->h;

    if (mode.integerScaling) {
        zx = (int)zx;
        zy = (int)zy;
    }

    // Scale the image to fit application window
    zx *= (double)g_display.width / (double)(DISPLAY_WIDTH);
    zy *= (double)g_display.height / (double)(DISPLAY_HEIGHT);

    if (mode.keepAspect) {
        if (zx < zy)
            zy = zx;
        else
            zx = zy;
    }

    SDL_Surface *zoomed = zoomSurface(game->romScreen, zx, zy, SMOOTHING_OFF);
    SDL_FreeSurface(game->romScreen);
    game->romScreen = zoomed;
}

ScalingMode_s getDynamicScalingMode(const Game_s *game)
{
    return (ScalingMode_s){
        ra_getConfigOverrideOption(game, ASPECT_RATIO_OPTION, true),
        ra_getConfigOverrideOption(game, INTEGER_SCALING_OPTION, false),
    };
}

SDL_Surface *loadRomScreen(int index)
{
    if (index < 0 || index >= game_list_len)
        return NULL;

    Game_s *game = &game_list[index];

    pthread_mutex_lock(&thread_mutex);

    if (game->romScreen == NULL && game->processed) {
        char currPicture[STR_MAX * 2];
        RomScreenType_e romScreenType = findRomScreen(game, currPicture);
        if (romScreenType != ROM_SCREEN_NONE) {
            game->romScreen = IMG_Load(currPicture);

            if (game->romScreen == NULL) {
                printf_debug("Error loading image: %s\n", currPicture);
            }
            else if (romScreenType == ROM_SCREEN_STATE) {
                scaleRomScreen(game, getDynamicScalingMode(game));
            }
            else {
                scaleRomScreen(game, (ScalingMode_s){true, false});
            }
        }
    }

    if (__initial_romscreens_loaded) {
        unloadRomScreen(index + 5);
        if (index > 5) {
            unloadRomScreen(index - 5);
        }
    }

    pthread_mutex_unlock(&thread_mutex);

    return game->romScreen;
}

void freeRomScreens()
{
    for (int i = 0; i < game_list_len; i++) {
        Game_s *game = &game_list[i];

        if (game->romScreen != NULL) {
            SDL_FreeSurface(game->romScreen);
            game->romScreen = NULL;
        }
    }
}

static void *_loadRomScreensThread(void *_)
{
    for (int i = 0; i < 10 && i < game_list_len; i++) {
        Game_s *game = &game_list[i];

        if (game->romScreen == NULL)
            loadRomScreen(i);
    }

    __initial_romscreens_loaded = true;

    return NULL;
}

void loadRomScreens()
{
    pthread_create(&romscreen_thread_pt, NULL, _loadRomScreensThread, NULL);
}

#endif // GAME_SWITCHER_ROMSCREEN_H
