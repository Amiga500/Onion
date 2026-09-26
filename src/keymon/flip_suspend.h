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

#endif // KEYMON_FLIP_SUSPEND_H__
