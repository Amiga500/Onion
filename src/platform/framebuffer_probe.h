/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_FRAMEBUFFER_PROBE_H
#define MAINUI_FRAMEBUFFER_PROBE_H
#include <SDL.h>

/* Log the SDL surface and read-only fbdev geometry once on the first Carousel
 * frame. Never changes the framebuffer mode, page offset or synchronization. */
void mainui_framebuffer_probe(const SDL_Surface *display);
#endif
