#ifndef UTILS_RETROARCH_CMD_OPTIMIZED_H__
#define UTILS_RETROARCH_CMD_OPTIMIZED_H__

/*
 * Optimized RetroArch command interface for Miyoo Mini+
 * 
 * Improvements over original:
 * 1. Shorter timeouts for faster failure detection
 * 2. Non-blocking save operations
 * 3. Batch command support
 * 4. Connection pooling (reuse socket)
 * 5. Better error reporting
 */

#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

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

// Optimized command interface with shorter timeouts
typedef struct RetroArchCmdOpts {
    int timeout_ms;           // Command timeout (default: 100ms vs 60000ms)
    int retry_count;          // Number of retries (default: 1 vs 3)
    bool async;               // Asynchronous execution
} RetroArchCmdOpts_s;

// Default options for different command types
static const RetroArchCmdOpts_s RA_CMD_OPTS_FAST = {
    .timeout_ms = 100,
    .retry_count = 1,
    .async = false
};

static const RetroArchCmdOpts_s RA_CMD_OPTS_SAVE = {
    .timeout_ms = 5000,  // Save states can take longer
    .retry_count = 1,
    .async = true       // Don't block UI
};

static const RetroArchCmdOpts_s RA_CMD_OPTS_RELIABLE = {
    .timeout_ms = 1000,
    .retry_count = 3,
    .async = false
};

// Standard interface (backward compatible)
int retroarch_cmd(const char *cmd);
int retroarch_get(const char *cmd, char *response, size_t response_size);

// Optimized interface with custom options
int retroarch_cmd_opts(const char *cmd, const RetroArchCmdOpts_s *opts);
int retroarch_get_opts(const char *cmd, char *response, size_t response_size, const RetroArchCmdOpts_s *opts);

// Command wrappers
int retroarch_quit(void);
int retroarch_toggleMenu(void);
int retroarch_pause(void);
int retroarch_unpause(void);
int retroarch_getStateSlot(int *slot);
int retroarch_setStateSlot(int slot);

// Optimized save/load with async support
int retroarch_autosave_async(void);  // Non-blocking save
int retroarch_save_async(int slot);   // Non-blocking save to slot
int retroarch_load(int slot);
bool retroarch_save_complete(void);   // Check if async save finished

int retroarch_getStatus(RetroArchStatus_s *status);
int retroarch_getInfo(RetroArchInfo_s *info);

// Batch commands (more efficient than multiple individual commands)
typedef struct RetroArchBatchCmd {
    const char *cmd;
    char *response;
    size_t response_size;
    int result;
} RetroArchBatchCmd_s;

int retroarch_batch_execute(RetroArchBatchCmd_s *cmds, int count, const RetroArchCmdOpts_s *opts);

// Connection management
void retroarch_connection_init(void);    // Initialize connection pool
void retroarch_connection_cleanup(void); // Cleanup connection pool

// Statistics for benchmarking
typedef struct RetroArchStats {
    uint64_t total_commands;
    uint64_t failed_commands;
    uint64_t total_time_ms;
    uint64_t avg_time_ms;
} RetroArchStats_s;

void retroarch_get_stats(RetroArchStats_s *stats);
void retroarch_reset_stats(void);

#endif // UTILS_RETROARCH_CMD_OPTIMIZED_H__
