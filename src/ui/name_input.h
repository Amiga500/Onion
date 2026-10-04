/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_NAME_INPUT_H
#define MAINUI_NAME_INPUT_H
#include "ui/theme.h"

typedef enum {
    NAME_EDITING,
    NAME_CANCEL,
    NAME_SUBMIT
} MainUINameResult;

/* Owns the keyboard's optional themed surfaces until close. Text is UTF-8 and
 * bounded to the patcher's 127-byte folder-name limit. */
typedef struct {
    bool open, shift, symbols, secret;
    int selected, title_id;
    char text[128], error[160];
    SDL_Surface *wide_normal, *wide_focused, *caps_active, *abc, *symbols_icon, *hints[5];
    SDL_Surface *glyphs[128];
    SDL_Surface *normal, *focused, *field, *space, *erase, *caps, *accept;
} MainUINameInput;

void mainui_name_input_open(MainUINameInput *input, const MainUITheme *theme, int title_id,
                            const char *initial);
void mainui_name_input_close(MainUINameInput *input);
/* Pass raw SDL keys before host action aliases so typing S/X/Y/M stays text. */
MainUINameResult mainui_name_input_key(MainUINameInput *input, const SDL_keysym *key);
void mainui_name_input_draw(MainUINameInput *input, SDL_Surface *screen, MainUITheme *theme);
#endif
