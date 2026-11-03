#ifndef SYSTEM_STATE_DECIDE_H__
#define SYSTEM_STATE_DECIDE_H__

// Which program is in front, from what system_state_update() (state.h)
// finds: no /proc and no files here, so the host tests can call it.

#include <stdbool.h>
#include <sys/types.h>

typedef enum system_state_e {
    MODE_UNKNOWN,
    MODE_MAIN_UI,
    MODE_SWITCHER,
    MODE_GAME,
    MODE_APPS,
    MODE_ADVMENU,
    MODE_DRASTIC
} SystemState;

// The processes looked for, in this order.
enum { STATE_P_SWITCHER,
       STATE_P_RETROARCH,
       STATE_P_RA32,
       STATE_P_MAINUI,
       STATE_P_ADVMENU,
       STATE_P_DRASTIC,
       STATE_P_GREEN,
       STATE_P_COUNT };

// switcher_flag: .runGameSwitcher exists; has_cmd: cmd_to_run.sh exists;
// ra_cmd: it starts RetroArch or an Emu/RApp launcher. *pid_out is the
// program's pid, or left alone when there is none to keep.
static inline SystemState system_state_decide(bool switcher_flag, bool has_cmd, bool ra_cmd,
                                              const pid_t pids[STATE_P_COUNT], pid_t *pid_out)
{
    if (switcher_flag && pids[STATE_P_SWITCHER]) {
        *pid_out = pids[STATE_P_SWITCHER];
        return MODE_SWITCHER;
    }
    if (ra_cmd && (pids[STATE_P_RETROARCH] || pids[STATE_P_RA32])) {
        *pid_out = pids[STATE_P_RETROARCH] ? pids[STATE_P_RETROARCH] : pids[STATE_P_RA32];
        return MODE_GAME;
    }
    if (!has_cmd && pids[STATE_P_MAINUI]) {
        // Open MainUI 1.1.0 opens Game Info's Manual in the PDF reader
        // (Green) as its own child, with MainUI still running. MENU, X and
        // Y belong to the reader then, as when it is started from Apps:
        // MENU closes the PDF. Taken as MainUI, MENU was grabbed for its
        // MainUI action and never reached the reader.
        if (pids[STATE_P_GREEN])
            return MODE_APPS;
        *pid_out = pids[STATE_P_MAINUI];
        return MODE_MAIN_UI;
    }
    if (!has_cmd && pids[STATE_P_ADVMENU]) {
        *pid_out = pids[STATE_P_ADVMENU];
        return MODE_ADVMENU;
    }
    if (has_cmd && pids[STATE_P_DRASTIC]) {
        *pid_out = pids[STATE_P_DRASTIC];
        return MODE_DRASTIC;
    }
    return MODE_APPS;
}

#endif // SYSTEM_STATE_DECIDE_H__
