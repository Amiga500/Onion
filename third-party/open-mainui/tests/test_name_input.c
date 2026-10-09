/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/details.h"
#include "platform/input.h"
#include "ui/name_input.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>

static MainUINameResult press(MainUINameInput *input, SDLKey key, Uint16 code)
{
    SDL_keysym event = {0};
    event.sym = key;
    event.unicode = code;
    return mainui_name_input_key(input, &event);
}

int mainui_suite_name_input(void)
{
    /* Patcher meta_input: a short final advance clamps the line offset, and
     * paging back subtracts a full visible page from that clamped position. */
    MainUIDetails details = {.line_count = 22, .lines_per_page = 10};
    mainui_details_key(&details, NULL, NULL, NULL, NULL, 6, SDLK_END);
    assert(details.scroll_line == 10);
    mainui_details_key(&details, NULL, NULL, NULL, NULL, 6, SDLK_END);
    assert(details.scroll_line == 12);
    mainui_details_key(&details, NULL, NULL, NULL, NULL, 6, SDLK_HOME);
    assert(details.scroll_line == 2);
    mainui_details_key(&details, NULL, NULL, NULL, NULL, 6, SDLK_HOME);
    assert(details.scroll_line == 0);

    MainUINameInput input = {0};
    /* Printable aliases must remain text while naming, including shifted text. */
    press(&input, SDLK_s, 's');
    press(&input, SDLK_x, 'X');
    press(&input, SDLK_y, 'y');
    press(&input, SDLK_m, 'm');
    press(&input, SDLK_UNKNOWN, 0x20ac);
    assert(!strcmp(input.text, "sXym\xe2\x82\xac"));
    press(&input, SDLK_BACKSPACE, 0);
    assert(!strcmp(input.text, "sXym"));
    press(&input, SDLK_UNKNOWN, 0xd800);
    assert(!strcmp(input.text, "sXym"));
    input.text[0] = 0;
    input.selected = 10;
    press(&input, SDLK_RETURN, 0);
    press(&input, SDLK_HOME, 0);
    press(&input, SDLK_RETURN, 0);
    press(&input, SDLK_END, 0);
    press(&input, SDLK_RETURN, 0);
    assert(!strcmp(input.text, "qQ!"));
    input.selected = 39;
    press(&input, SDLK_DOWN, 0);
    assert(input.selected == 44);
    assert(press(&input, SDLK_RETURN, 0) == NAME_SUBMIT);
    press(&input, SDLK_RIGHT, 0);
    assert(input.selected == 40);
    press(&input, SDLK_LEFT, 0);
    assert(input.selected == 44);
    press(&input, SDLK_UP, 0);
    assert(input.selected == 38);
    input.selected = 42;
    press(&input, SDLK_RETURN, 0);
    assert(!strcmp(input.text, "qQ! "));
    press(&input, SDLK_LSHIFT, 0);
    assert(!strcmp(input.text, "qQ!  "));
    press(&input, SDLK_LALT, 0);
    input.selected = 43;
    press(&input, SDLK_RETURN, 0);
    assert(!strcmp(input.text, "qQ!"));
    memset(input.text, 'a', 126);
    input.text[126] = 0;
    press(&input, SDLK_UNKNOWN, 0x20ac);
    assert(strlen(input.text) == 126);
    press(&input, SDLK_z, 'z');
    press(&input, SDLK_z, 'z');
    assert(strlen(input.text) == 127);
    assert(press(&input, SDLK_F2, 0) == NAME_SUBMIT);
    assert(press(&input, SDLK_ESCAPE, 0) == NAME_CANCEL);
    input.text[0] = 0;
    input.selected = 10;
    input.shift = input.symbols = false;
    SDL_Event physical = {0};
    physical.type = SDL_KEYDOWN;
    physical.key.keysym.sym = SDLK_SPACE;
    physical.key.keysym.unicode = ' ';
    mainui_input_device_event(&physical);
    assert(mainui_name_input_key(&input, &physical.key.keysym) == NAME_EDITING);
    assert(!strcmp(input.text, "q"));
    physical.key.keysym.sym = SDLK_RETURN;
    mainui_input_device_event(&physical);
    assert(mainui_name_input_key(&input, &physical.key.keysym) == NAME_SUBMIT);
    physical.key.keysym.sym = SDLK_ESCAPE;
    mainui_input_device_event(&physical);
    assert(mainui_name_input_key(&input, &physical.key.keysym) == NAME_EDITING);
    physical.key.keysym.sym = SDLK_LCTRL;
    mainui_input_device_event(&physical);
    assert(mainui_name_input_key(&input, &physical.key.keysym) == NAME_CANCEL);
    mainui_name_input_close(&input);
    return 0;
}
