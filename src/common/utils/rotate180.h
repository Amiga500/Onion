#ifndef ROTATE_180_H__
#define ROTATE_180_H__

#include <SDL/SDL.h>
#include "neon_pixel.h"

/**
 * Rotate an SDL surface 180 degrees in-place using NEON SIMD.
 * ~50x faster than rotozoomSurface(180°) — no bilinear interpolation needed.
 */
SDL_Surface *rotate180(SDL_Surface *original)
{
    if (!original || original->format->BytesPerPixel != 4) {
        /* Fallback for non-32bpp: use old rotozoom path */
        #include "SDL/SDL_rotozoom.h"
        SDL_Surface *rotated = rotozoomSurface(original, 180.0, 1.0, 0);
        SDL_FillRect(original, NULL, SDL_MapRGB(original->format, 255, 0, 0));
        SDL_Rect rect = {-2, -2};
        SDL_BlitSurface(rotated, NULL, original, &rect);
        SDL_FreeSurface(rotated);
        return original;
    }

    /* For 32bpp surfaces: simple in-place pixel array reversal */
    SDL_LockSurface(original);
    neon_rotate180_inplace((uint32_t *)original->pixels, original->w * original->h);
    SDL_UnlockSurface(original);
    return original;
}

#endif // ROTATE_180_H__
