#ifndef ONION_TEST_STUB_SDL_H
#define ONION_TEST_STUB_SDL_H

/* Minimal SDL stub so production headers that call SDL_FreeSurface can be
 * included from host unit tests without libSDL. */
#include <stdint.h>

typedef struct SDL_Surface {
    int w;
    int h;
} SDL_Surface;

void SDL_FreeSurface(SDL_Surface *surface);

#endif /* ONION_TEST_STUB_SDL_H */
