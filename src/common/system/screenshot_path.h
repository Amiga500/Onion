#ifndef SCREENSHOT_PATH_H__
#define SCREENSHOT_PATH_H__

// Screenshot file naming, without libpng or the system state, so host
// tests include it. system/screenshot.h uses it.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "utils/file.h"

#ifndef SCREENSHOTS_DIR
#define SCREENSHOTS_DIR "/mnt/SDCARD/Screenshots"
#endif

// First free "<dir>/<name>_NNN.png" (NNN from 000 to 999) in path_out;
// "Screenshot" when name is empty or NULL. False when all 1000 are taken
// or the path does not fit in path_out_size.
static bool screenshot_numberedPath(char *path_out, size_t path_out_size,
                                    const char *dir, const char *name)
{
    uint32_t i;

    if (name == NULL || name[0] == '\0')
        name = "Screenshot";

    for (i = 0; i < 1000; i++) {
        int n = snprintf(path_out, path_out_size, "%s/%s_%03d.png", dir, name, i);
        if (n < 0 || (size_t)n >= path_out_size)
            return false;
        if (!exists(path_out))
            break;
    }

    return i <= 999;
}

#endif // SCREENSHOT_PATH_H__
