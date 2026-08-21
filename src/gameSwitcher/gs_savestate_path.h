#ifndef GS_SAVESTATE_PATH_H__
#define GS_SAVESTATE_PATH_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* Same path as gs_model.h; defined here so this header is testable without SDL. */
#ifndef STATES_DIR
#define STATES_DIR "/mnt/SDCARD/Saves/CurrentProfile/states"
#endif

/**
 * Build a RetroArch save-state path from core/rom names.
 *
 * Returns false without writing out_path when core_name is empty.
 * That is the _save_thread early-return gate: the caller must not inspect
 * or poll out_path, and must not call retroarch_save.
 */
static bool createSaveStatePathFromNames(const char *core_name, const char *rom_name,
                                         int slot, char *out_path, size_t out_path_size)
{
    if (core_name == NULL || core_name[0] == '\0') {
        return false;
    }

    if (rom_name == NULL) {
        rom_name = "";
    }

    if (slot == -1) {
        snprintf(out_path, out_path_size, STATES_DIR "/%s/%s.state.auto", core_name, rom_name);
    }
    else if (slot == 0) {
        snprintf(out_path, out_path_size, STATES_DIR "/%s/%s.state", core_name, rom_name);
    }
    else {
        snprintf(out_path, out_path_size, STATES_DIR "/%s/%s.state%d", core_name, rom_name, slot);
    }

    return true;
}

#endif /* GS_SAVESTATE_PATH_H__ */
