/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_FILES_H
#define MAINUI_FILES_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t size, modified, identity;
    bool exists;
} MainUIFileStamp;

MainUIFileStamp mainui_file_stamp(const char *path);
bool mainui_file_stamp_equal(MainUIFileStamp a, MainUIFileStamp b);
bool mainui_sync_parent(const char *path);
/* removed remains true when unlink succeeds but the directory sync fails. */
int mainui_remove_file_status(const char *path, bool *removed);
int mainui_remove_file(const char *path); /* remove semantics, UTF-8 paths */
bool mainui_temporary_path(char out[4096], const char *target, const char *tag);
#ifdef MAINUI_TEST_FAULTS
void mainui_test_fault(const char *point);
#endif
typedef struct MainUIFileLock MainUIFileLock;
/* Nonblocking cooperative lock, keyed by the target path in /tmp/mainui-locks.
 * Callers must use a consistent normalized absolute target path.
 * The lock inode remains; process termination releases the OS lock.
 * Never infer ownership from file existence alone. */
MainUIFileLock *mainui_file_lock(const char *target);
void mainui_file_unlock(MainUIFileLock *lock);
/* Read bounded text into an owned NUL-terminated buffer; free with free().
 * Return NULL on I/O, size or allocation failure. No file writes.
 */
char *mainui_read_text(const char *path, size_t max_bytes);

/* Outcome of a publish-then-flush write. A plain boolean cannot tell "nothing
 * changed" from "the new file is visible but its folder flush failed". */
typedef enum {
    MAINUI_WRITE_UNCHANGED,   /* Destination untouched. */
    MAINUI_WRITE_NOT_DURABLE, /* New contents visible; parent-directory flush failed. */
    MAINUI_WRITE_DURABLE      /* New contents visible and flushed. */
} MainUIWriteResult;

/* Publish through an exclusive .writing.pid.serial sibling. */
MainUIWriteResult mainui_write_text_atomic_result(const char *path, const char *text);
/* True when the new contents are visible (a failed folder flush is logged):
 * false means the old file is intact. For callers that need durability, such
 * as transactions with rollback, use mainui_write_text_atomic_result(). */
bool mainui_write_text_atomic(const char *path, const char *text);
/* Publish complete bytes without replacing an existing destination. The
 * exclusive sibling staging reservation and destination are never truncated.
 * True when published (a failed folder flush is logged), like the above. */
bool mainui_write_bytes_new(const char *path, const void *data, size_t size);
/* Strict: returns true only when published and flushed. published remains true
 * if publication succeeded but directory sync failed; use it, rather than the
 * return value, to track cleanup ownership. */
bool mainui_write_bytes_new_status(const char *path, const void *data, size_t size,
                                   bool *published);
bool mainui_move_file_new(const char *source, const char *destination);
/* FAT-safe publication/move: caller must hold an exclusive cooperative lock.
 * The absence check followed by rename is NOT race-free without that lock.
 * Strict: a sync failure reports false after the rename has completed; the
 * ROM-deletion journal relies on this. */
bool mainui_write_bytes_new_locked(const char *path, const void *data, size_t size);
bool mainui_move_file_new_locked(const char *source, const char *destination);
/* Lexical containment for normalized paths; does not resolve symlinks. */
bool mainui_path_within(const char *path, const char *root);
bool mainui_regular_file_within(const char *path, const char *root);
bool mainui_directory_within(const char *path, const char *root);
#endif
