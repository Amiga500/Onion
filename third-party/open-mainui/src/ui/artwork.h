/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_ARTWORK_H
#define MAINUI_ARTWORK_H
#include <SDL.h>
/* PNG/JPEG/BMP by content; max 2000 per axis and 16 MiB encoded.
 * Returns an owned surface, or NULL for missing, invalid or unsupported art. */
SDL_Surface *mainui_artwork_load(const char *path);
/* New owned software RGBA copy, preserving alpha/color-key transparency.
 * Does not free the source; independent of the current video surface. */
SDL_Surface *mainui_artwork_software(SDL_Surface *image);
#endif
