/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_LAUNCH_H
#define MAINUI_LAUNCH_H
#include "cJSON.h"
#include "core/core.h"
#define MAINUI_FAVORITE_RETURN_SIZE 1932
/* Encode Onion's double-quote command protocol. Output is allocated and owned
 * by caller. Unsupported shell/parser characters fail with a diagnostic.
 * Normalizes the known launch.sh:/mnt/SDCARD Search encoding before formatting. */
char *mainui_launch_command(const cJSON *record, char error[256]);
/* Exact little-endian bounded patch record. No native struct layout assumptions.
 * Returns false instead of truncating identities. launch bytes stay zero as in
 * the patched producer. */
bool mainui_launch_favorite_return(unsigned char out[MAINUI_FAVORITE_RETURN_SIZE], int type,
                                   const char *folder, const char *rom, const char *label);
/* Publish return envelope + legacy state, then command last. Directory must
 * exist. A pending request is never overwritten. Failure emits no command and
 * removes only files created by this call; legacy state may have been updated.
 * This adapter never executes or supervises a launcher. */
bool mainui_launch_publish(const char *directory, const cJSON *record, const MainUIStack *state,
                           const cJSON *resume, char error[256]);
/* Read/consume one return envelope only after the consumer removed the command.
 * Returns owned JSON or NULL. Invalid envelopes are consumed to avoid loops. */
/* Publish only a committed return envelope (no command) so that the next
 * MainUI start resumes the given screen. Used to restart MainUI in place. */
bool mainui_launch_publish_restart(const char *directory, const cJSON *resume, char error[256]);
cJSON *mainui_launch_take_return(const char *directory);
void mainui_launch_clear_search(const char *directory);
#endif
