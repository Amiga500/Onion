/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_INPUT_H
#define MAINUI_INPUT_H
#include <SDL.h>
/* Host keyboard aliases preserve Enter=A/Escape=B. F2 and F1 avoid collisions with
 * Onion's device START/MENU keycodes; raw device translation is separate. */
/* Normalize host aliases; all other SDL keys are returned unchanged. */
SDLKey mainui_input_key(SDLKey key);
/* Translate raw Onion SDL events once, before controller/keyboard dispatch.
 * Scripted test actions already use canonical keys and bypass this translation. */
void mainui_input_device_event(SDL_Event *event);
/* Unknown script characters return SDLK_UNKNOWN. 1/2=L1/R1, 3/4=L2/R2. */
SDLKey mainui_input_script(char action);
/* Borrow a constant button label, or NULL for ordinary navigation/unmapped keys. */
const char *mainui_input_button(SDLKey key);
/* Standard vertical movement; page aliases clamp when applied to a viewport. */
int mainui_input_list_delta(SDLKey key, int rows);
#endif
