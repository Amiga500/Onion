#ifndef GAME_SWITCHER_ROMSCREEN_WINDOW_H
#define GAME_SWITCHER_ROMSCREEN_WINDOW_H

#include <stdbool.h>

// Romscreen memory window: which entries the UI thread may free. Kept free
// of SDL so the host tests can check it.
//
// An entry is evicted when it is further than `window` from `center`,
// unless it is `pinned`: another thread (the overlay's autosave thread,
// which encodes game_list[0].romScreen to PNG) is still reading its pixels.
// pinned < 0 means nothing is pinned.
static inline bool romscreen_shouldEvict(int index, int center, int window, int pinned)
{
    if (index == pinned)
        return false;
    return index < center - window || index > center + window;
}

#endif // GAME_SWITCHER_ROMSCREEN_WINDOW_H
