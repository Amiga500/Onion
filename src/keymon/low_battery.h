#ifndef KEYMON_LOW_BATTERY_H__
#define KEYMON_LOW_BATTERY_H__

#include <stdbool.h>

#include "system/state_decide.h"

// Tweaks > System > "Low battery: save and quit" (battery/exitAt, a
// percentage; 0 is Off). keymon checks it every 15 s.

// The level is battery_chargingLevel(): -1 when /tmp/percBat is missing or
// unreadable. battery_getPercentage() shows 0 there, which read as "at the
// level" and closed the game with no reading at all.
static inline bool lowBattery_levelReached(int level, int exit_at)
{
    return exit_at > 0 && level >= 0 && level <= exit_at;
}

// Only a game is saved and closed: RetroArch (auto save state) or DraStic.
// In MainUI or an app nothing is closed, and /tmp/.lowBat used to be set
// anyway, so the next game exited normally shut the console down.
static inline bool lowBattery_inGame(SystemState state)
{
    return state == MODE_GAME || state == MODE_DRASTIC;
}

#endif // KEYMON_LOW_BATTERY_H__
