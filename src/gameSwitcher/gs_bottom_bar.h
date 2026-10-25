#ifndef GAME_SWITCHER_BOTTOM_BAR_H
#define GAME_SWITCHER_BOTTOM_BAR_H

#include <stdbool.h>

// The GameSwitcher's bottom bar is theme art (extra/gs-bottom-bar). With
// Footer layout set to "B A, page left" a theme can provide the same bar
// with its hints rearranged, extra/gs-bottom-bar-alt. The alternative is
// used only when it comes from the same place as the normal bar or a closer
// one (theme_getImagePath load mode: 2 overrides, 1 theme, 0 system): a bar
// the user put in the theme overrides is never replaced by the theme's
// alternative, and a theme without one keeps its own bar.
static inline const char *gs_bottomBarName(bool alt_layout, int alt_mode, int bar_mode)
{
    if (alt_layout && alt_mode >= 1 && alt_mode >= bar_mode)
        return "extra/gs-bottom-bar-alt";
    return "extra/gs-bottom-bar";
}

#endif // GAME_SWITCHER_BOTTOM_BAR_H
