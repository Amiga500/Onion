#ifndef GAME_SWITCHER_LONG_PRESS_H__
#define GAME_SWITCHER_LONG_PRESS_H__

#include <stdbool.h>
#include <stdint.h>

// Long press measured in time, not in main-loop iterations. The Y long
// press used to count 75 iterations: ~0.3 s while the loop spun every 4 ms,
// ~2.5 s once the loop sleeps until the next frame (33 ms).

#define GS_LONG_PRESS_MS 300

typedef struct {
    bool held;
    bool fired;
    uint32_t start;
} LongPress_s;

// Call on every loop iteration while the key is held. Returns true once,
// when the key has been held for threshold_ms.
static bool longPress_held(LongPress_s *lp, uint32_t now, uint32_t threshold_ms)
{
    if (!lp->held) {
        lp->held = true;
        lp->fired = false;
        lp->start = now;
    }
    if (!lp->fired && (uint32_t)(now - lp->start) >= threshold_ms) {
        lp->fired = true;
        return true;
    }
    return false;
}

// Call when the key is released. Returns true for a short press (the long
// press did not fire), and resets the state for the next press.
static bool longPress_release(LongPress_s *lp)
{
    bool short_press = !lp->fired;
    lp->held = false;
    lp->fired = false;
    return short_press;
}

#endif // GAME_SWITCHER_LONG_PRESS_H__
