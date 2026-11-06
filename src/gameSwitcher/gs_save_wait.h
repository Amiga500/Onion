#ifndef GS_SAVE_WAIT_H__
#define GS_SAVE_WAIT_H__

#include <stdbool.h>
#include <stdint.h>

#include "utils/file.h"

typedef uint32_t (*gs_ticks_fn)(void);
typedef int (*gs_sleep_fn)(long msec);

/**
 * @brief Wait until the save-state writer releases the state file.
 *
 * Only runs after a confirmed save. file_isLocked() opens the path with
 * O_CREAT, so polling it after a failed save would leave an empty state
 * file behind and the slot would look occupied. The timeout is checked
 * before file_isLocked() for the same reason.
 *
 * @param path State file path.
 * @param saved True if the save was confirmed by a newer mtime.
 * @param start Tick count when the save was requested.
 * @param timeout_ms Deadline relative to start.
 * @param ticks Tick source (SDL_GetTicks in production).
 * @param sleep_fn Sleep function (msleep in production).
 */
static inline void gs_waitStateFileRelease(const char *path, bool saved,
                                           uint32_t start, uint32_t timeout_ms,
                                           gs_ticks_fn ticks, gs_sleep_fn sleep_fn)
{
    if (!saved || path == NULL)
        return;

    while (ticks() - start < timeout_ms && file_isLocked(path)) {
        sleep_fn(100);
    }
}

#endif // GS_SAVE_WAIT_H__
