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
#include "./reset_keep.h"

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
    // settings. What is not one of them (the installer's state, the update
    // channel, Syncthing, the applied theme, the cores chosen per console...)
    // is kept: see reset_keep.h.
    reset_clearConfigDir("/mnt/SDCARD/.tmp_update/config");
    system("7z x -aoa " RESET_CONFIGS_PAK " -o/mnt/SDCARD/ -ir!.tmp_update/config/*");
    // RedOnion's defaults that only a migration creates; keeping
    // migration_state without these would leave them off for good.
    char migrations[512];
    if (reset_migrationCommand(migrations, sizeof(migrations), "/mnt/SDCARD/.tmp_update/script/migration"))
        system(migrations);
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

    // The boot puts this device's copy (config/system/<serial>.json, saved
    // at each shutdown and game launch) back over system.json: it gets the
    // defaults too, or a power-off without a shutdown brought the old
    // settings back. "Reset system tweaks" keeps that folder.
    getDeviceSerial();
    if (DEVICE_SN[0] != '\0') {
        char device_path[96];
        snprintf(device_path, sizeof(device_path), "/mnt/SDCARD/.tmp_update/config/system/%s.json", DEVICE_SN);
        if (is_file(device_path))
            file_copy("/mnt/SDCARD/system.json", device_path);
    }

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
