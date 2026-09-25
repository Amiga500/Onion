#ifndef GS_HOLD_H__
#define GS_HOLD_H__

#include <stdbool.h>
#include <stdint.h>

// Hold Y this long to show the fullscreen image (same as keymon's MENU
// long press). Measured in ticks, not loop iterations, so it does not
// depend on how often the main loop runs.
#define GS_Y_HOLD_FULLSCREEN_MS 700

/**
 * @brief True once a button pressed at down_ticks has been held for
 * threshold_ms. Wrap-safe for uint32_t tick counters.
 */
static inline bool gs_holdReached(bool held, uint32_t down_ticks, uint32_t now, uint32_t threshold_ms)
{
    return held && now - down_ticks >= threshold_ms;
}

#endif // GS_HOLD_H__
