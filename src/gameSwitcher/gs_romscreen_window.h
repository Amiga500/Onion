#ifndef GAME_SWITCHER_ROMSCREEN_WINDOW_H
#define GAME_SWITCHER_ROMSCREEN_WINDOW_H

#include <stdbool.h>

// Romscreen memory window: which entries the UI thread may free. Kept free
// of SDL so the host tests can check it.
//
// An entry is evicted when it is further than `window` from `center`,
// unless it is `pinned` (pinned < 0: none). The GameSwitcher passes -1:
// the overlay now saves the running game's picture from its own copy of
// the pixels, so no other thread reads a romScreen any more.
static inline bool romscreen_shouldEvict(int index, int center, int window, int pinned)
{
    if (index == pinned)
        return false;
    return index < center - window || index > center + window;
}

#endif // GAME_SWITCHER_ROMSCREEN_WINDOW_H
