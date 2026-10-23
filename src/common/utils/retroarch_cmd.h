#ifndef UTILS_RETROARCH_CMD_H__
#define UTILS_RETROARCH_CMD_H__

#include <stdbool.h>
#include <stddef.h>

typedef enum RetroArchState {
    RETROARCH_STATE_UNKNOWN,
    RETROARCH_STATE_PLAYING,
    RETROARCH_STATE_PAUSED,
    RETROARCH_STATE_CONTENTLESS
} RetroArchState_e;

typedef struct RetroArchStatus {
    char content_info[1024];
    RetroArchState_e state;
} RetroArchStatus_s;

typedef struct RetroArchInfo {
    unsigned int max_disk_slots;
    unsigned int disk_slot;
    int state_slot;
    bool has_state_slot;
} RetroArchInfo_s;

int retroarch_cmd(const char *cmd);                                       // Send RetroArch UDP command
int retroarch_get(const char *cmd, char *response, size_t response_size); // Get RetroArch UDP command response

int retroarch_quit(void);                           // RetroArch QUIT
int retroarch_toggleMenu(void);                     // RetroArch MENU_TOGGLE
int retroarch_pause(void);                          // RetroArch PAUSE
int retroarch_unpause(void);                        // RetroArch UNPAUSE
int retroarch_pauseNoProbe(void);                   // PAUSE, command listener checked
int retroarch_getStateSlot(int *slot);              // RetroArch GET_STATE_SLOT
int retroarch_setStateSlot(int slot);               // RetroArch SET_STATE_SLOT <slot>
int retroarch_autosave(void);                       // RetroArch SAVE_STATE_SLOT -1
int retroarch_save(int slot);                       // RetroArch SAVE_STATE_SLOT <slot>
int retroarch_load(int slot);                       // RetroArch LOAD_STATE_SLOT <slot>
int retroarch_getStatus(RetroArchStatus_s *status); // Get RetroArch status
int retroarch_getStatusNoProbe(RetroArchStatus_s *status);
int retroarch_getInfo(RetroArchInfo_s *info);       // Get RetroArch info

// GET_STATUS without a probe, waiting up to timeout_ms for the reply while
// keep_waiting (optional) returns true. RetroArch answers the first
// GET_STATUS for an arcade zip only after reading the whole file for its
// CRC32, which can take seconds.
int retroarch_getStatusWait(RetroArchStatus_s *status, int timeout_ms,
                            bool (*keep_waiting)(void *ctx), void *ctx);

#endif // UTILS_RETROARCH_CMD_H__