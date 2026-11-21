#ifndef TWEAKS_RESET_H__
#define TWEAKS_RESET_H__

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "system/device_model.h"
#include "system/keymap_sw.h"
#include "theme/render/dialog.h"
#include "theme/sound.h"
#include "utils/file.h"

#include "./appstate.h"

#define RESET_CONFIGS_PAK "/mnt/SDCARD/.tmp_update/config/configs.pak"

bool _confirmReset(const char *title_str, const char *message_str)
{
    bool retval = false;
    bool confirm_quit = false;
    SDLKey changed_key = SDLK_UNKNOWN;

    keys_enabled = false;

    background_cache = SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);
    SDL_BlitSurface(screen, NULL, background_cache, NULL);

    theme_renderDialog(screen, title_str, message_str, true);
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);

    while (!confirm_quit) {
        if (updateKeystate(keystate, &confirm_quit, true, &changed_key)) {
            if (changed_key == SW_BTN_B && keystate[SW_BTN_B] == PRESSED)
                confirm_quit = true;
            else if (changed_key == SW_BTN_A && keystate[SW_BTN_A] == PRESSED) {
                retval = true;
                confirm_quit = true;
            }
        }
        SDL_Delay(15); // wait for input without spinning
    }

    if (changed_key != SDLK_UNKNOWN)
        sound_change();

    if (retval) {
        SDL_BlitSurface(screen, NULL, background_cache, NULL);
        theme_renderDialog(screen, title_str, "Resetting...", false);
        SDL_BlitSurface(screen, NULL, video, NULL);
        SDL_Flip(video);
    }
    else {
        // Cancelled: _notifyResetDone() (which frees it) will not run.
        SDL_FreeSurface(background_cache);
        background_cache = NULL;
        keys_enabled = true;
        all_changed = true;
    }

    return retval;
}

void _notifyResetDone(const char *title_str)
{
    SDL_BlitSurface(background_cache, NULL, screen, NULL);
    theme_renderDialog(screen, title_str, "Done", false);
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
    msleep(300);

    SDL_FreeSurface(background_cache);
    background_cache = NULL;
    keys_enabled = true;
    all_changed = true;

    sync();
}

void action_resetTweaks(void *pt)
{
    const char title_str[] = "Reset system tweaks";
    if (!_disable_confirm && !_confirmReset(title_str, "Are you sure you want to\nreset system tweaks?"))
        return;
    // The settings go, not the folder: configs.pak only holds the default
    // settings, so the installer's state (without it every migration ran
    // again at the next update), Syncthing's identity and the two netplay
    // files of the system zip were gone for good.
    static const char *const keep[] = {"configs.pak", "migration_state", "syncthing",
                                       "netplay_cores.conf", "hotspot_wpa_supplicant.conf"};
    DIR *dp = opendir("/mnt/SDCARD/.tmp_update/config");
    struct dirent *ep;
    while (dp != NULL && (ep = readdir(dp)) != NULL) {
        bool kept = strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0;
        for (size_t i = 0; !kept && i < sizeof(keep) / sizeof(keep[0]); i++)
            kept = strcmp(ep->d_name, keep[i]) == 0;
        if (!kept) {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "/mnt/SDCARD/.tmp_update/config/%s", ep->d_name);
            file_remove_recursive(path);
        }
    }
    if (dp != NULL)
        closedir(dp);
    system("7z x -aoa " RESET_CONFIGS_PAK " -o/mnt/SDCARD/ -ir!.tmp_update/config/*");
    // RedOnion's defaults that only a migration creates (Apps sorted by
    // name, console pictures, Saved Games: Overwrite); each one leaves a
    // file that is there alone. Keeping migration_state without these would
    // leave them off for good.
    system("cd /mnt/SDCARD/.tmp_update/script/migration && "
           "sh ./00021_sort_apps_by_name.sh; sh ./00026_console_pictures_default.sh; "
           "sh ./00028_saved_games_overwrite_default.sh");
    reset_menus = true;
    settings_load();
    if (!_disable_confirm)
        _notifyResetDone(title_str);
}

void action_resetThemeOverrides(void *pt)
{
    const char title_str[] = "Reset theme overrides";
    if (!_disable_confirm && !_confirmReset(title_str, "Are you sure you want to\nreset theme overrides?"))
        return;
    file_remove_recursive("/mnt/SDCARD/Saves/CurrentProfile/theme");
    mkdirs("/mnt/SDCARD/Saves/CurrentProfile/theme");
    if (!_disable_confirm)
        _notifyResetDone(title_str);
}

void action_resetMainUI(void *pt)
{
    const char title_str[] = "Reset MainUI settings";

    if (!_disable_confirm && !_confirmReset(title_str, "Are you sure you want to\nreset MainUI settings?"))
        return;

    remove("/mnt/SDCARD/system.json");

    char src_path[80];
    snprintf(src_path, sizeof(src_path), "/mnt/SDCARD/.tmp_update/res/miyoo%d_system.json", DEVICE_ID);
    file_copy(src_path, "/mnt/SDCARD/system.json");

    if (IS_MIYOO_PLUS_OR_FLIP()) {
        remove("/appconfigs/wpa_supplicant.conf");
        file_copy("/mnt/SDCARD/.tmp_update/res/wpa_supplicant.reset", "/appconfigs/wpa_supplicant.conf");
    }

    reset_menus = true;
    settings_load();
    if (!_disable_confirm)
        _notifyResetDone(title_str);
}

void action_resetRAMain(void *pt)
{
    const char title_str[] = "Reset RetroArch configuration";
    if (!_disable_confirm && !_confirmReset(title_str, "Are you sure you want to reset\nRetroArch main configuration?"))
        return;
    system("7z x -aoa " RESET_CONFIGS_PAK " -o/mnt/SDCARD/ -ir!RetroArch/*");
    reset_menus = true;
    if (!_disable_confirm)
        _notifyResetDone(title_str);
}

void action_resetRACores(void *pt)
{
    const char title_str[] = "Reset all RA core overrides";
    if (!_disable_confirm && !_confirmReset(title_str, "Are you sure you want to reset\nall RetroArch core overrides?"))
        return;
    file_remove_recursive("/mnt/SDCARD/Saves/CurrentProfile/config");
    mkdirs("/mnt/SDCARD/Saves/CurrentProfile/config");
    system("7z x " RESET_CONFIGS_PAK " -o/mnt/SDCARD/ -ir!Saves/CurrentProfile/config/*");
    reset_menus = true;
    if (!_disable_confirm)
        _notifyResetDone(title_str);
}

void action_resetAdvanceMENU(void *pt)
{
    const char title_str[] = "Reset AdvanceMENU/MAME/MESS";
    if (!_disable_confirm && !_confirmReset(title_str, "Are you sure you want to\nreset AdvanceMENU/MAME/MESS?"))
        return;
    system("7z x -aoa " RESET_CONFIGS_PAK " -o/mnt/SDCARD/ -ir!BIOS/.advance/*");
    reset_menus = true;
    if (!_disable_confirm)
        _notifyResetDone(title_str);
}

void action_resetAll(void *pt)
{
    const char title_str[] = "Reset everything";
    if (!_confirmReset(title_str, "Are you sure you want to\nreset everything?"))
        return;
    _disable_confirm = true;
    action_resetTweaks(pt);
    action_resetThemeOverrides(pt);
    action_resetMainUI(pt);
    action_resetRAMain(pt);
    action_resetRACores(pt);
    action_resetAdvanceMENU(pt);
    _disable_confirm = false;
    reset_menus = true;
    settings_load();
    _notifyResetDone(title_str);
}

#endif // TWEAKS_RESET_H__
