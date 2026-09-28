#ifndef KEYSTATE_H__
#define KEYSTATE_H__

#include <SDL/SDL.h>

#include "./msleep.h"

typedef enum { RELEASED,
               PRESSED,
               REPEATING } KeyState;

static SDL_Event keystate_event;

bool updateKeystate(KeyState keystate[320], bool *quit_flag, bool enabled,
                    SDLKey *changed_key)
{
    bool retval = false;

    while (SDL_PollEvent(&keystate_event)) {
        SDLKey key = keystate_event.key.keysym.sym;

        if (!enabled)
            continue;

        switch (keystate_event.type) {
        case SDL_QUIT:
            *quit_flag = true;
            if (changed_key != NULL)
                *changed_key = SDLK_UNKNOWN;
            break;
        case SDL_KEYDOWN:
            if (keystate[key] != RELEASED)
                keystate[key] = REPEATING;
            else
                keystate[key] = PRESSED;
            if (changed_key != NULL)
                *changed_key = key;
            retval = true;
            break;
        case SDL_KEYUP:
            keystate[key] = RELEASED;
            if (changed_key != NULL)
                *changed_key = key;
            retval = true;
            break;
        default:
            if (changed_key != NULL)
                *changed_key = SDLK_UNKNOWN;
            break;
        }
    }

    msleep(4);

    return retval;
}

// State of `key` for a keystate array after code that may have read the
// input events itself (an action, a dialog): PRESSED while SDL still sees
// the key down, so its key repeats arrive as REPEATING and are not taken
// for a new press; RELEASED once the key is up.
static inline KeyState keystate_resync(SDLKey key)
{
    Uint8 *keys = SDL_GetKeyState(NULL);
    return keys != NULL && keys[key] ? PRESSED : RELEASED;
}

#endif
