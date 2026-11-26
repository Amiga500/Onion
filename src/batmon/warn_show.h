#ifndef BATMON_WARN_SHOW_H__
#define BATMON_WARN_SHOW_H__

#include <stdbool.h>

// Whether the low-battery icon (top right, Tweaks > System > Low battery
// warning) is shown. Not in standby, and not while charging: on the Mini
// Plus and Mini Flip the level stays low for a while after the charger is
// plugged in (3, 4, 5%...), and the icon stayed until it passed the
// threshold (robcodedev). On the Mini, charging reads as 500 and never
// showed it. Otherwise below the threshold. batmon then checks that the
// warning is not turned off (warningDisabled(), which looks for MainUI
// among the processes: only when the level is low, not every second).
static inline bool batteryWarning_wanted(bool suspended, bool charging, int percentage,
                                         int warn_at)
{
    if (suspended || charging || percentage == 500)
        return false;
    return percentage >= 0 && percentage < warn_at;
}

#endif // BATMON_WARN_SHOW_H__
