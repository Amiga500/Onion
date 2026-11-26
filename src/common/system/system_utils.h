#ifndef SETTINGS_UTILS_H__
#define SETTINGS_UTILS_H__

#include "system/device_model.h"
#include "system/proc_exited.h"
#include "system/settings.h"
#include "system/settings_sync.h"
#include "system/state.h"
#include "utils/file.h"
#include "utils/msleep.h"

//
//    [onion] Check retroarch running & savestate_auto_save in retroarch.cfg is
//    true
//
bool check_autosave(void)
{
    char value[STR_MAX];
    file_parseKeyValue(RETROARCH_CONFIG, "savestate_auto_save", value, '=', 0);
    return strcmp(value, "true") == 0;
}

void kill_mainUI(void)
{
    if (system_state == MODE_MAIN_UI) {
        settings_shm_read();
        kill(system_state_pid, SIGKILL);

        // Not on the Mini Flip: display_reset() right after MainUI is
        // killed could freeze it (robcodedev). Kept on the Mini and Plus.
        // See the alternative display_reset() in display.h.
        if (DEVICE_ID != MIYOO285)
            display_reset();
    }
}

// MainUI stopped to open the GameSwitcher or resume the last game, and
// started again after it: with SIGTERM, Open MainUI (rc10 and later) saves
// the screen it is on, with the list position (mainui-screen.json in /tmp),
// and opens it again at its next start (robcodedev). SIGKILL lost it: back
// from the GameSwitcher, the list was not where it was left. It gets up to
// a second to finish, then SIGKILL as before; the display is reset as in
// kill_mainUI(). Not for the paths that write state.json for MainUI's next
// start (an app or tool shortcut): those keep kill_mainUI().
#define KILL_MAINUI_TERM_WAIT_MS 1000

void kill_mainUI_keepScreen(void)
{
    if (system_state != MODE_MAIN_UI)
        return;
    settings_shm_read();
    pid_t pid = system_state_pid;
    if (kill(pid, SIGTERM) == 0) {
        for (int waited = 0; waited < KILL_MAINUI_TERM_WAIT_MS && !proc_exited(pid); waited += 20)
            msleep(20);
    }
    if (!proc_exited(pid))
        kill(pid, SIGKILL);

    if (DEVICE_ID != MIYOO285)
        display_reset();
}

void set_gameSwitcher(void)
{
    flag_set("/mnt/SDCARD/.tmp_update/", ".runGameSwitcher", true);
}

#endif // SETTINGS_UTILS_H__
