#ifndef KEYMON_STATE_SCAN_H__
#define KEYMON_STATE_SCAN_H__

#include <stdbool.h>
#include <stdint.h>

// runtime.sh touches /tmp/state_changed before every launch, and keymon
// rescans the system state (every /proc entry) on key events while the
// flag exists, until a 15 s tick removes it: every key press, release and
// repeat for up to 15-30 s after each launch or exit.
//
// Rescan when the flag was touched again since the last scan, when it is
// about to be removed (so the final state is always read), or at most
// every STATE_SCAN_INTERVAL_MS otherwise: the new program may not have
// started yet when the first key after a touch arrives.

#define STATE_SCAN_INTERVAL_MS 500

static bool stateScan_due(bool flag_touched, bool removing_flag, uint32_t now_ms,
                          uint32_t last_scan_ms)
{
    return flag_touched || removing_flag ||
           (uint32_t)(now_ms - last_scan_ms) >= STATE_SCAN_INTERVAL_MS;
}

#endif // KEYMON_STATE_SCAN_H__
