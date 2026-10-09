/* SPDX-License-Identifier: GPL-3.0-only */
#include "platform/input.h"

SDLKey mainui_input_key(SDLKey key)
{
    switch (key) {
    case SDLK_SPACE:
        return SDLK_RETURN;
    case SDLK_LCTRL:
        return SDLK_ESCAPE;
    case SDLK_s:
        return SDLK_RCTRL;
    case SDLK_x:
        return SDLK_LSHIFT;
    case SDLK_y:
        return SDLK_LALT;
    case SDLK_m:
        return SDLK_F1;
    default:
        return key;
    }
}

void mainui_input_device_event(SDL_Event *event)
{
    if (event->type != SDL_KEYDOWN && event->type != SDL_KEYUP) {
        return;
    }
    /* Frozen Onion src/common/system/keymap_sw.h. Canonical controller keys
     * are shared with the host preview, but the raw device mapping is not. */
    switch (event->key.keysym.sym) {
    case SDLK_SPACE:
        event->key.keysym.sym = SDLK_RETURN;
        break;
    case SDLK_LCTRL:
        event->key.keysym.sym = SDLK_ESCAPE;
        break;
    case SDLK_RETURN:
        event->key.keysym.sym = SDLK_F2;
        break;
    case SDLK_ESCAPE:
        event->key.keysym.sym = SDLK_F1;
        break;
    case SDLK_e:
        event->key.keysym.sym = SDLK_PAGEUP;
        break;
    case SDLK_t:
        event->key.keysym.sym = SDLK_PAGEDOWN;
        break;
    case SDLK_TAB:
        event->key.keysym.sym = SDLK_HOME;
        break;
    case SDLK_BACKSPACE:
        event->key.keysym.sym = SDLK_END;
        break;
    default:
        break;
    }
    /* Physical keys must not become e/t/space text in the on-screen keyboard. */
    event->key.keysym.unicode = 0;
}

SDLKey mainui_input_script(char action)
{
    switch (action) {
    case 'U':
        return SDLK_UP;
    case 'D':
        return SDLK_DOWN;
    case 'L':
        return SDLK_LEFT;
    case 'R':
        return SDLK_RIGHT;
    case 'A':
    case 'E':
        return SDLK_RETURN;
    case 'B':
        return SDLK_ESCAPE;
    case 'S':
        return SDLK_RCTRL;
    case 'X':
        return SDLK_LSHIFT;
    case 'Y':
        return SDLK_LALT;
    case 'T':
        return SDLK_F2;
    case 'M':
        return SDLK_F1;
    case '1':
        return SDLK_PAGEUP;
    case '2':
        return SDLK_PAGEDOWN;
    case '3':
        return SDLK_HOME;
    case '4':
        return SDLK_END;
    default:
        return SDLK_UNKNOWN;
    }
}

const char *mainui_input_button(SDLKey key)
{
    switch (mainui_input_key(key)) {
    case SDLK_PAGEUP:
        return "L1";
    case SDLK_PAGEDOWN:
        return "R1";
    case SDLK_HOME:
        return "L2";
    case SDLK_END:
        return "R2";
    case SDLK_RCTRL:
        return "SELECT";
    case SDLK_LSHIFT:
        return "X";
    case SDLK_LALT:
        return "Y";
    case SDLK_F2:
        return "START";
    case SDLK_F1:
        return "MENU";
    default:
        return NULL;
    }
}

int mainui_input_list_delta(SDLKey key, int rows)
{
    switch (key) {
    case SDLK_UP:
        return -1;
    case SDLK_DOWN:
        return 1;
    case SDLK_TAB:
        return -rows;
    case SDLK_BACKSPACE:
        return rows;
    default:
        return 0;
    }
}
