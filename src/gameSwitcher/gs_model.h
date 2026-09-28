#ifndef GAME_SWITCHER_MODEL_H
#define GAME_SWITCHER_MODEL_H

#include <SDL/SDL.h>
#include <pthread.h>
#include <stdbool.h>

#include "SDL/SDL_rotozoom.h"
#include "system/display.h"
#include "utils/retroarch_cmd.h"
#include "utils/sdl_direct_fb.h"
#include "utils/str.h"

#include "gs_recent_item.h"

#define MAX_HISTORY 100

#ifndef ROM_SCREENS_DIR
#define ROM_SCREENS_DIR "/mnt/SDCARD/Saves/CurrentProfile/romScreens"
#endif
#define HISTORY_PATH "/mnt/SDCARD/Saves/CurrentProfile/lists/content_history.lpl"
#define CONFIG_DIR "/mnt/SDCARD/Saves/CurrentProfile/config"
#define STATES_DIR "/mnt/SDCARD/Saves/CurrentProfile/states"
#define RETROARCH_CONFIG_PATH "/mnt/SDCARD/RetroArch/.retroarch/retroarch.cfg"
#define ASPECT_RATIO_OPTION "video_dingux_ipu_keep_aspect"
#define INTEGER_SCALING_OPTION "video_scale_integer"

static Game_s game_list[MAX_HISTORY];
static int game_list_len = 0;

#endif // GAME_SWITCHER_MODEL_H