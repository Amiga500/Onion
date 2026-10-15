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

typedef enum {
    FLIP_SUSPEND_STAY = 0,     // keep sleeping
    FLIP_SUSPEND_WAKE = 1,     // the lid was opened: resume
    FLIP_SUSPEND_POWER_OFF = 2 // suspend timeout elapsed off the charger
} FlipSuspendStep;

// One 500 ms iteration of the Mini Flip suspend loop with no key event.
// Opening the lid wakes the device whether or not it is charging (the
// charger used to be checked first, so a charging Flip ignored the lid and
// only the power button woke it). The charger only holds off the timeout
// power-off, so is_charging is called only when the timeout has elapsed.
// *saved_lid follows every lid change, charging or not.
static FlipSuspendStep flipSuspend_step(int current_lid, int *saved_lid,
                                        bool timed_out,
                                        bool (*is_charging)(void))
{
    if (flipSuspend_nothingToDo(current_lid, *saved_lid, timed_out))
        return FLIP_SUSPEND_STAY;
    if (current_lid == 1 && *saved_lid == 0)
        return FLIP_SUSPEND_WAKE;
    *saved_lid = current_lid;
    if (timed_out && !is_charging())
        return FLIP_SUSPEND_POWER_OFF;
    return FLIP_SUSPEND_STAY;
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
