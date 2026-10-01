/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_TIMING_H
#define MAINUI_TIMING_H
#include <time.h>

typedef enum {
    MAINUI_MARK_ENTRY,
    MAINUI_MARK_SESSION,
    MAINUI_MARK_VIDEO,
    MAINUI_MARK_RESTORE,
    MAINUI_MARK_READY,
    MAINUI_MARK_FIRST_FRAME,
    MAINUI_MARK_EVENT,
    MAINUI_MARK_HANDOFF,
    MAINUI_MARK_EXIT,
    MAINUI_MARK_COUNT
} MainUIMark;

/* UI thread only. ENTRY resets the session and disables collection when stdout
 * is /dev/null. FIRST_FRAME is recorded once; EVENT means latest key-down.
 * Other marks are overwritten, except EVENT freezes after HANDOFF. */
void mainui_mark(MainUIMark mark);
/* Fixed known counter names; safe from catalog workers. Unknown names ignored.
 * count sets a gauge; add accumulates work across the entire session. */
void mainui_count(const char *name, long value);
void mainui_count_add(const char *name, long value);
/* Worker-safe elapsed counters. No clock reads when logging is disabled.
 * Finish every attempted operation, including failure/cancellation. */
struct timespec mainui_timing_start(void);
void mainui_timing_finish(const char *name, struct timespec start);
/* UI thread: optional tmpfs exchange, read against ENTRY, scoped to this boot.
 * Configure only for a real device; no SD writes or additional logging flag. */
void mainui_timing_handoff(const char *path);
/* UI thread, called often. When logging, writes an interim counter line at
 * most every interval_ms and only if counters changed, so a session that ends
 * in SIGKILL (Onion's game switcher) still leaves its figures in the log. */
void mainui_timing_interim(long interval_ms);
/* Once per session, after workers stop; only interim lines come before it.
 * Missing measurements are -1. EXIT is a pre-report mark, not process death. */
void mainui_timing_report(void);
#endif
