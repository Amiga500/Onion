#ifndef GS_IDLE_H__
#define GS_IDLE_H__

#include <stdint.h>

/**
 * @brief Milliseconds the main loop may sleep before the next render step.
 *
 * Returns 0 when a step is already due (acc_ticks >= time_step), so the
 * loop never delays a pending render.
 */
static inline int gs_idleWaitMs(uint32_t acc_ticks, uint32_t time_step)
{
    if (acc_ticks >= time_step)
        return 0;
    return (int)(time_step - acc_ticks);
}

#endif // GS_IDLE_H__
