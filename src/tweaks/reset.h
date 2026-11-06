#ifndef TWEAKS_RESET_H__
#define TWEAKS_RESET_H__

#include <stdio.h>

#include "system/device_model.h"
#include "system/keymap_sw.h"
#include "system/settings.h"
#include "theme/render/dialog.h"
#include "theme/sound.h"
#include "utils/file.h"
#include "utils/flags.h"
#include "utils/msleep.h"

#include "./appstate.h"
#include "./reset_core.h"

static const ResetSource reset_source = {
    .sd_root = RESET_SD_ROOT,
    .pak = RESET_CONFIGS_PAK,
    .staging = RESET_STAGING_DIR,
    .extract = reset_extract7z,
};

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
        // Cancelled: _notifyResetResult() (which frees it) will not run.
        SDL_FreeSurface(background_cache);
        background_cache = NULL;
        keys_enabled = true;
        all_changed = true;
    }

    return retval;
}

void _notifyResetResult(const char *title_str, bool ok)
{
    SDL_BlitSurface(background_cache, NULL, screen, NULL);
    theme_renderDialog(screen, title_str, ok ? "Done" : "Reset failed", false);
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
    msleep(ok ? 300 : 1500);

    SDL_FreeSurface(background_cache);
    background_cache = NULL;
    keys_enabled = true;
    all_changed = true;

    sync();
}

void _notifyResetDone(const char *title_str)
{
    _notifyResetResult(title_str, true);
}

// Each reset: true when the defaults are in place. A failed one leaves the
// files as they were (see reset_core.h).

static bool _resetTweaks(void)
{
    reset_recoverAll(RESET_SD_ROOT, RESET_STAGING_DIR, RESET_LEGACY_PAK, RESET_CONFIGS_PAK);
    if (!reset_replaceDir(&reset_source, ".tmp_update/config", reset_config_keep))
        return false;
    reset_runDefaultMigrations(RESET_MIGRATION_DIR);

    // The network services follow their flags again when Tweaks closes
    // (runtime.sh checks them), and the Network menu shows the new ones.
    if (HAS_WIFI())
        temp_flag_set("network_changed", true);
    network_state_stale = true;
    return true;
}

static bool _resetThemeOverrides(void)
{
    file_remove_recursive("/mnt/SDCARD/Saves/CurrentProfile/theme");
    mkdirs("/mnt/SDCARD/Saves/CurrentProfile/theme");
    return is_dir("/mnt/SDCARD/Saves/CurrentProfile/theme");
}

static bool _resetMainUI(void)
{
    char src_path[80];
    snprintf(src_path, sizeof(src_path), "/mnt/SDCARD/.tmp_update/res/miyoo%d_system.json", DEVICE_ID);

    // Replaced, not deleted and copied: a failed copy used to leave no
    // system.json, or no Wi-Fi setup. Under the lock other programs take to
    // save system.json (keymon on a volume change).
    int lock = file_lock(MAIN_UI_SETTINGS, 1000);
    bool ok = reset_copyFileAtomic(src_path, MAIN_UI_SETTINGS);
    file_unlock(lock);

    if (IS_MIYOO_PLUS_OR_FLIP() &&
        !reset_copyFileAtomic("/mnt/SDCARD/.tmp_update/res/wpa_supplicant.reset", "/appconfigs/wpa_supplicant.conf"))
        ok = false;

    return ok;
}

static bool _resetRAMain(void)
{
    return reset_overwriteFiles(&reset_source, "RetroArch");
}

static bool _resetRACores(void)
{
    reset_recoverAll(RESET_SD_ROOT, RESET_STAGING_DIR, RESET_LEGACY_PAK, RESET_CONFIGS_PAK);
    return reset_replaceDir(&reset_source, "Saves/CurrentProfile/config", NULL);
}

static bool _resetAdvanceMENU(void)
{
    return reset_overwriteFiles(&reset_source, "BIOS/.advance");
}

void action_resetTweaks(void *pt)
{
    const char title_str[] = "Reset system tweaks";
    if (!_confirmReset(title_str, "Are you sure you want to\nreset system tweaks?"))
        return;
    bool ok = _resetTweaks();
    reset_menus = true;
    settings_load();
    _notifyResetResult(title_str, ok);
}

void action_resetThemeOverrides(void *pt)
{
    const char title_str[] = "Reset theme overrides";
    if (!_confirmReset(title_str, "Are you sure you want to\nreset theme overrides?"))
        return;
    bool ok = _resetThemeOverrides();
    _notifyResetResult(title_str, ok);
}

void action_resetMainUI(void *pt)
{
    const char title_str[] = "Reset MainUI settings";
    if (!_confirmReset(title_str, "Are you sure you want to\nreset MainUI settings?"))
        return;
    bool ok = _resetMainUI();
    reset_menus = true;
    settings_load();
    _notifyResetResult(title_str, ok);
}

void action_resetRAMain(void *pt)
{
    const char title_str[] = "Reset RetroArch configuration";
    if (!_confirmReset(title_str, "Are you sure you want to reset\nRetroArch main configuration?"))
        return;
    bool ok = _resetRAMain();
    reset_menus = true;
    _notifyResetResult(title_str, ok);
}

void action_resetRACores(void *pt)
{
    const char title_str[] = "Reset all RA core overrides";
    if (!_confirmReset(title_str, "Are you sure you want to reset\nall RetroArch core overrides?"))
        return;
    bool ok = _resetRACores();
    reset_menus = true;
    _notifyResetResult(title_str, ok);
}

void action_resetAdvanceMENU(void *pt)
{
    const char title_str[] = "Reset AdvanceMENU/MAME/MESS";
    if (!_confirmReset(title_str, "Are you sure you want to\nreset AdvanceMENU/MAME/MESS?"))
        return;
    bool ok = _resetAdvanceMENU();
    reset_menus = true;
    _notifyResetResult(title_str, ok);
}

void action_resetAll(void *pt)
{
    const char title_str[] = "Reset everything";
    if (!_confirmReset(title_str, "Are you sure you want to\nreset everything?"))
        return;
    // All of them, even after one failed; "Reset failed" if any did.
    bool ok = _resetTweaks();
    ok = _resetThemeOverrides() && ok;
    ok = _resetMainUI() && ok;
    ok = _resetRAMain() && ok;
    ok = _resetRACores() && ok;
    ok = _resetAdvanceMENU() && ok;
    reset_menus = true;
    settings_load();
    _notifyResetResult(title_str, ok);
}

#endif // TWEAKS_RESET_H__
