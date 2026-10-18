#ifndef KEYMON_FLIP_SUSPEND_H__
#define KEYMON_FLIP_SUSPEND_H__

#include <stdbool.h>

// Mini Flip suspend loop (keymon suspend_exec): the device wakes every
// 500 ms to watch the lid. When the lid state has not changed and the
// suspend timeout has not elapsed, nothing in that iteration can change,
// whatever the charger state: no wake-up, no power-off, no lid state to
// record. The charger check (battery_isCharging(), a popen of axp_test
// once its 2 s cache expires) is only needed otherwise.
static bool flipSuspend_nothingToDo(int current_lid, int saved_lid,
                                    bool timed_out)
{
    return current_lid == saved_lid && !timed_out;
}

// Lid close action from Tweaks (flip/lidCloseAction).
typedef enum {
    FLIP_LID_SUSPEND = 0,
    FLIP_LID_SHUTDOWN = 1,
    FLIP_LID_NOTHING = 2
} FlipLidAction;

// What keymon does when the Mini Flip lid closes. disable_standby is the
// Power-button "single press: Shutdown" flag and must not affect the lid
// (#228); it is passed only so tests can prove that. Unknown values from a
// hand-edited config do nothing, as the original switch did.
static FlipLidAction flipLid_onClose(int lid_close_action, bool disable_standby)
{
    (void)disable_standby;
    switch (lid_close_action) {
    case FLIP_LID_SUSPEND:
        return FLIP_LID_SUSPEND;
    case FLIP_LID_SHUTDOWN:
        return FLIP_LID_SHUTDOWN;
    default:
        return FLIP_LID_NOTHING;
    }
}

#endif // KEYMON_FLIP_SUSPEND_H__
