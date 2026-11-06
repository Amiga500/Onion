/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MAINUI_IMAGE_SCALE_H
#define MAINUI_IMAGE_SCALE_H
#include <SDL.h>

/* Alpha-aware bilinear scaling of a 32-bit software surface, either way, to
 * exactly width by height. Its fixed-point interpolation handles non-integer
 * factors, such as a pixel aspect correction, without nearest-neighbour
 * block-size artifacts, and reduces Carousel artwork quickly. Returns a new surface, or NULL when
 * the format is not supported or allocation fails; the caller keeps source. */
SDL_Surface *mainui_image_bilinear_copy(SDL_Surface *source, int width, int height);

/* For framed Saved Games: area-average enlargement along an axis that grows,
 * crisp pixels with blended edges, and bilinear along one that shrinks. */
SDL_Surface *mainui_image_framed_copy(SDL_Surface *source, int width, int height);

/* Fit within max_width by max_height without enlargement, keeping the
 * rounded aspect ratio, bilinear; NULL when it already fits. */
SDL_Surface *mainui_image_fit(SDL_Surface *source, int max_width, int max_height);
#endif
