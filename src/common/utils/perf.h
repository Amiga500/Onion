#ifndef PERF_H__
#define PERF_H__

/**
 * @file perf.h
 * @brief Lightweight performance timing instrumentation for Onion OS.
 *
 * Provides millisecond-precision timing macros for measuring critical code
 * paths on the Miyoo Mini hardware (ARM Cortex-A7, ~1.2 GHz).
 *
 * Compile with -DPERF_ENABLED to activate timing. When disabled, all macros
 * expand to nothing (zero overhead in production builds).
 *
 * Output goes to stderr and optionally to /mnt/SDCARD/.tmp_update/logs/perf.log
 * in CSV format: timestamp_ms,label,elapsed_ms
 *
 * Usage:
 *   #include "utils/perf.h"
 *
 *   void myFunction(void) {
 *       PERF_START("game_launch");
 *       // ... code to measure ...
 *       PERF_END("game_launch");
 *   }
 *
 * Build:
 *   make core DEBUG=1 PERF=1
 */

#ifdef PERF_ENABLED

#include <stdio.h>
#include <time.h>

#define PERF_LOG_PATH "/mnt/SDCARD/.tmp_update/logs/perf.log"

/**
 * Get current monotonic time in milliseconds (no syscall overhead on ARM).
 * Uses CLOCK_MONOTONIC_RAW to avoid NTP/adjtime interference.
 */
static inline long perf_get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (long)(ts.tv_sec * 1000L + ts.tv_nsec / 1000000L);
}

/**
 * Write a performance measurement to stderr and the perf log file.
 * Format: CSV — timestamp_ms,label,elapsed_ms
 */
static inline void perf_log(const char *label, long start_ms, long end_ms)
{
    long elapsed = end_ms - start_ms;
    fprintf(stderr, "[PERF] %s: %ld ms\n", label, elapsed);

    FILE *fp = fopen(PERF_LOG_PATH, "a");
    if (fp != NULL) {
        fprintf(fp, "%ld,%s,%ld\n", end_ms, label, elapsed);
        fclose(fp);
    }
}

/**
 * PERF_START(label) — Begin a named timing measurement.
 * Declares a local variable _perf_start_<line> to hold the start time.
 * The label must be a string literal.
 */
#define _PERF_VAR(prefix, line) prefix##line
#define _PERF_VAR2(prefix, line) _PERF_VAR(prefix, line)
#define PERF_START(label) \
    long _PERF_VAR2(_perf_start_, __LINE__) = perf_get_ms(); \
    const long *_perf_start_ptr_ = &_PERF_VAR2(_perf_start_, __LINE__);

/**
 * PERF_END(label) — End a named timing measurement and log the result.
 * Must be called in the same scope as the matching PERF_START.
 */
#define PERF_END(label) \
    do { \
        long _perf_end_ = perf_get_ms(); \
        perf_log(label, *_perf_start_ptr_, _perf_end_); \
    } while (0)

#else /* PERF_ENABLED not defined */

#define PERF_START(label) do {} while (0)
#define PERF_END(label)   do {} while (0)

#endif /* PERF_ENABLED */

#endif /* PERF_H__ */
