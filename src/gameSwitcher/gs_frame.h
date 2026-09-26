#ifndef GAME_SWITCHER_FRAME_H__
#define GAME_SWITCHER_FRAME_H__

#include <stdbool.h>

// What the GameSwitcher main loop has to draw in a frame.
typedef enum {
    GS_FRAME_SKIP,      // nothing changed on screen
    GS_FRAME_NAME_ONLY, // only the scrolling game name moves
    GS_FRAME_FULL       // state changed: redraw and present everything
} GsFrame_e;

// changed:      something set appState.changed (input, timeouts, loads).
// name_shown:   the name bar is drawn (not full screen, no pop menu,
//               at least one game).
// name_scrolls: the name is wider than its bar, so it scrolls.
//
// A scrolling name that is not shown (full-screen view, pop menu open)
// used to redraw and present the whole screen every frame, and so did
// the 2 s after every brightness change: the slider is drawn by the
// frame that changes it, and the frame after the timeout removes it.
static GsFrame_e gs_frameKind(bool changed, bool name_shown, bool name_scrolls)
{
    if (changed)
        return GS_FRAME_FULL;
    if (name_shown && name_scrolls)
        return GS_FRAME_NAME_ONLY;
    return GS_FRAME_SKIP;
}

#endif // GAME_SWITCHER_FRAME_H__
