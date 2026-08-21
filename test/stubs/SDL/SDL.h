#ifndef ONION_TEST_STUB_SDL_H
#define ONION_TEST_STUB_SDL_H

/* Minimal SDL 1.2 stub so production headers (list.h, surfaceSetAlpha.h)
 * can be included from host unit tests without libSDL. */
#include <stdint.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;

#define SDL_SRCALPHA 0x00010000

typedef struct SDL_PixelFormat {
    Uint32 Amask;
    Uint8 Ashift;
    Uint8 BytesPerPixel;
} SDL_PixelFormat;

typedef struct SDL_Surface {
    int w;
    int h;
    int pitch;
    void *pixels;
    SDL_PixelFormat *format;
} SDL_Surface;

void SDL_FreeSurface(SDL_Surface *surface);
int SDL_LockSurface(SDL_Surface *surface);
void SDL_UnlockSurface(SDL_Surface *surface);
int SDL_SetAlpha(SDL_Surface *surface, Uint32 flag, Uint8 alpha);

#endif /* ONION_TEST_STUB_SDL_H */
