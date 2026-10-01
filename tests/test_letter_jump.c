/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/letter_jump.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static const char *label_at(void *context, int index)
{
    const char **labels = context;
    return labels[index];
}

static int jump(const char **labels, int total, int selected, int direction)
{
    MainUILetterJump state;
    mainui_letter_jump_begin(&state, selected, total, direction, label_at, labels);
    int destination = -1;
    while (state.active) {
        mainui_letter_jump_step(&state, label_at, labels, &destination);
    }
    return destination;
}

int mainui_suite_letter_jump(void)
{
    const char *labels[] = {"  Alpha", "aardvark", "Beta",    "beta two",
                            "Charlie", "\xc3\x80", "\xc3\xa0"};
    assert(jump(labels, 7, 0, 1) == 2);
    assert(jump(labels, 7, 3, -1) == 0);
    assert(jump(labels, 7, 0, -1) == 5);
    assert(jump(labels, 7, 6, 1) == 0);
    assert(jump(labels, 7, 6, -1) == 4);
    const char *large[130];
    for (int i = 0; i < 130; i++) {
        large[i] = i < 129 ? "Alpha" : "Beta";
    }
    MainUILetterJump state;
    mainui_letter_jump_begin(&state, 0, 130, 1, label_at, large);
    int destination = -1;
    assert(!mainui_letter_jump_step(&state, label_at, large, &destination));
    assert(state.active && state.cursor == 64 && destination == -1);
    assert(!mainui_letter_jump_step(&state, label_at, large, &destination));
    assert(mainui_letter_jump_step(&state, label_at, large, &destination));
    assert(destination == 129);
    large[0] = NULL;
    assert(jump(large, 130, 0, 1) == -1);
    puts("Letter groups, Latin-1 folding, wrap and bounded slices passed");
    return 0;
}
