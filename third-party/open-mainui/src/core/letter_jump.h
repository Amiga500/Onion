/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_LETTER_JUMP_H
#define MAINUI_LETTER_JUMP_H
#include <stdbool.h>
#include <stdint.h>

/* The label callback returns a borrowed UTF-8 label, or NULL on a read failure.
 * It may replace a catalog window; no returned pointer survives a callback. */
typedef const char *(*MainUILabelAt)(void *context, int index);

typedef struct {
    bool active;
    int selected, total, cursor, direction;
    int previous, next, last_group;
    uint32_t selected_initial, last_initial;
} MainUILetterJump;

/* Begin a request without scanning the list. Invalid/empty inputs leave it idle. */
void mainui_letter_jump_begin(MainUILetterJump *jump, int selected, int total, int direction,
                              MainUILabelAt label_at, void *context);
/* Read at most 64 labels per call. Returns true only when destination is ready.
 * A failed read cancels the request without changing the caller's selection. */
bool mainui_letter_jump_step(MainUILetterJump *jump, MainUILabelAt label_at, void *context,
                             int *destination);
#endif
