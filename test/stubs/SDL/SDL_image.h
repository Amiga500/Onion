#ifndef ONION_TEST_STUB_SDL_IMAGE_H
#define ONION_TEST_STUB_SDL_IMAGE_H

/* SDL_image stub, with the few SDL video calls of theme/load.h, so the
 * production theme headers (theme/config.h) can be included from host unit
 * tests. A test that includes them defines these functions (the theme
 * config tests never call them). */
#include "SDL.h"

#define SDL_SWSURFACE 0x00000000

SDL_Surface *IMG_Load(const char *file);
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int width, int height, int depth,
                                  Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
int SDL_BlitSurface(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);

#endif /* ONION_TEST_STUB_SDL_IMAGE_H */
