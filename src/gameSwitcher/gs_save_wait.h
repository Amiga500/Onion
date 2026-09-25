#ifndef GAME_SWITCHER_SAVE_WAIT_H__
#define GAME_SWITCHER_SAVE_WAIT_H__

#include <stdbool.h>
#include <stdint.h>

#include "utils/file.h"
#include "utils/msleep.h"

/**
 * @brief Wait until the save state file is released by the writer.
 *
 * Two guards precede the poll, both covering regressions from the inline
 * loop this replaces (gs_popMenu.h):
 *
 *  - A save that was never confirmed never touches the path at all.
 *    file_isLocked() opens with O_RDONLY | O_CREAT, so polling it after a
 *    failed save used to leave a 0-byte state file behind, which
 *    _scanSaveStates() then listed as an existing slot.
 *  - The deadline is checked before the file, not after, so an expired
 *    deadline cannot create the file either.
 *
 * The elapsed comparison is done in uint32 wraparound-safe arithmetic, so
 * the deadline holds across an SDL_GetTicks() wrap.
 *
 * The contract of file_isLocked() itself is unchanged.
 *
 * now_fn/sleep_fn are injectable for testing; pass SDL_GetTicks/msleep in
 * production.
 */
static void gs_waitStateFileRelease(const char *path, bool save_confirmed,
                                    uint32_t start, uint32_t timeout_ms,
                                    uint32_t (*now_fn)(void),
                                    int (*sleep_fn)(long msec))
{
    if (path == NULL || !save_confirmed)
        return;

    // Check if any process is using the save state file. The deadline is
    // evaluated before touching the file: file_isLocked() creates the file
    // if it is missing, and that side effect must not happen once the wait
    // has already run out.
    while ((uint32_t)(now_fn() - start) < timeout_ms && file_isLocked(path)) {
        sleep_fn(100);
    }
}

#endif // GAME_SWITCHER_SAVE_WAIT_H__
