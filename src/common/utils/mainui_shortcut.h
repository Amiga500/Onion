#ifndef UTILS_MAINUI_SHORTCUT_H__
#define UTILS_MAINUI_SHORTCUT_H__

#include <stdbool.h>
#include <string.h>

// MainUI X and Y button actions that are not an app or a tool, as saved in
// .tmp_update/config/keymap.json ("mainui_button_x" / "mainui_button_y").
// Open MainUI 1.0.10 reads both buttons:
//  - "glo": the selected game starts with its Game List Options; keymon
//    sets /tmp/launch_alt so runtime.sh opens them instead of the game;
//  - "fav": the selected game is added to or removed from Favorites, by
//    MainUI alone (keymon does nothing);
//  - "disable": the button does nothing in MainUI (Open MainUI 1.1.0 rc9 and
//    later; keymon does nothing either). keymap.json may also hold false for
//    the same thing, which settings load reads as "disable".
// Tweaks lists them after the apps and the tools, in this order, for both
// buttons.
#define MAINUI_SHORTCUT_EXTRA_COUNT 3

static const char *const mainui_shortcut_extra_values[MAINUI_SHORTCUT_EXTRA_COUNT] = {"glo", "fav", "disable"};
static const char *const mainui_shortcut_extra_labels[MAINUI_SHORTCUT_EXTRA_COUNT] = {"GLO", "Toggle favorite", "Disabled"};

// Index of a saved value among the extras, or -1.
static inline int mainui_shortcut_extraIndex(const char *value)
{
    if (value == NULL)
        return -1;
    for (int i = 0; i < MAINUI_SHORTCUT_EXTRA_COUNT; i++)
        if (strcmp(value, mainui_shortcut_extra_values[i]) == 0)
            return i;
    return -1;
}

// keymon flags the next launch for Game List Options on this action.
static inline bool mainui_shortcut_launchesGlo(const char *value)
{
    return value != NULL && strcmp(value, "glo") == 0;
}

#endif // UTILS_MAINUI_SHORTCUT_H__
