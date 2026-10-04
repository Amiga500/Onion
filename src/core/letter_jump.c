/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/letter_jump.h"

/* Patcher grouping is independent of locale: skip ASCII spaces and fold only
 * ASCII/Latin-1 uppercase. Invalid UTF-8 starts form their own byte group. */
static uint32_t initial(const char *label)
{
    const unsigned char *p = (const unsigned char *)label;
    while (*p == ' ') {
        p++;
    }
    uint32_t code = *p;
    int trailing = 0;
    if (*p >= 0xc2 && *p <= 0xdf) {
        code = *p & 0x1f;
        trailing = 1;
    }
    else if (*p >= 0xe0 && *p <= 0xef) {
        code = *p & 0x0f;
        trailing = 2;
    }
    else if (*p >= 0xf0 && *p <= 0xf4) {
        code = *p & 7;
        trailing = 3;
    }
    for (int i = 1; i <= trailing; i++) {
        if ((p[i] & 0xc0) != 0x80) {
            return *p;
        }
        code = (code << 6) | (p[i] & 0x3f);
    }
    if ((code >= 'A' && code <= 'Z') || (code >= 0xc0 && code <= 0xde && code != 0xd7)) {
        code += 32;
    }
    return code;
}

void mainui_letter_jump_begin(MainUILetterJump *jump, int selected, int total, int direction,
                              MainUILabelAt label_at, void *context)
{
    *jump = (MainUILetterJump){0};
    if (selected < 0 || selected >= total || !direction) {
        return;
    }
    const char *label = label_at(context, selected);
    if (!label) {
        return;
    }
    *jump = (MainUILetterJump){.active = true,
                               .selected = selected,
                               .total = total,
                               .direction = direction,
                               .previous = -1,
                               .next = -1,
                               .selected_initial = initial(label)};
}

bool mainui_letter_jump_step(MainUILetterJump *jump, MainUILabelAt label_at, void *context,
                             int *destination)
{
    if (!jump->active) {
        return false;
    }
    /* Search from the current group instead of rescanning the complete catalog.
     * Yield after each bounded slice; stop once the adjacent group is known. */
    for (int budget = 64; budget > 0 && jump->cursor < jump->total; --budget) {
        int step = ++jump->cursor;
        int index = jump->direction > 0 ? (jump->selected + step) % jump->total
                                        : (jump->selected - step + jump->total) % jump->total;
        const char *label = label_at(context, index);
        if (!label) {
            jump->active = false;
            return false;
        }
        uint32_t code = initial(label);
        if (jump->direction > 0) {
            if (index == 0 || code != jump->selected_initial) {
                *destination = index;
                jump->active = false;
                return true;
            }
        }
        else {
            if (jump->previous < 0) {
                if (code != jump->selected_initial || index == jump->total - 1) {
                    jump->previous = index;
                    jump->last_initial = code;
                }
            }
            else if (code == jump->last_initial && index < jump->previous) {
                jump->previous = index;
            }
            else {
                *destination = jump->previous;
                jump->active = false;
                return true;
            }
            if (index == 0 && jump->previous >= 0) {
                *destination = jump->previous;
                jump->active = false;
                return true;
            }
        }
    }
    if (jump->cursor < jump->total) {
        return false;
    }
    *destination =
        jump->previous >= 0 && jump->last_initial != jump->selected_initial ? jump->previous : 0;
    jump->active = false;
    return true;
}
