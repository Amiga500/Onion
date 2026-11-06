#ifndef GAME_SWITCHER_GAME_NAME_H
#define GAME_SWITCHER_GAME_NAME_H

// The name under the GameSwitcher picture. It is the game's name without
// the tags in brackets ("Super Mario (USA)" -> "Super Mario"), but a name
// made only of tags ("(Hack) [T+Ita]", "[BIOS] (World)") cleans to nothing,
// and SDL_ttf renders an empty text as no surface at all: the name bar then
// dereferenced NULL and the GameSwitcher crashed. SDL-free so host tests
// include it.

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>

static bool gs_nameHasText(const char *s)
{
    if (s == NULL)
        return false;
    for (; *s != '\0'; s++) {
        if (!isspace((unsigned char)*s))
            return true;
    }
    return false;
}

// If `shortname` has no visible text, put in the first of the full name,
// the ROM file name and MainUI's label that has some, else "?".
static void gs_fillShortName(char *shortname, size_t size, const char *name,
                             const char *rom_name, const char *label)
{
    if (shortname == NULL || size == 0 || gs_nameHasText(shortname))
        return;
    const char *fallback = gs_nameHasText(name)       ? name
                           : gs_nameHasText(rom_name) ? rom_name
                           : gs_nameHasText(label)    ? label
                                                      : "?";
    snprintf(shortname, size, "%s", fallback);
}

#endif // GAME_SWITCHER_GAME_NAME_H
