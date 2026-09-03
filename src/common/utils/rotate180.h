#ifndef ROTATE_180_H__
#define ROTATE_180_H__

#include "SDL/SDL_rotozoom.h"
#include "neon_pixel.h"
#include <SDL/SDL.h>

/**
 * Rotate an SDL surface 180 degrees.
 * 32bpp contiguous surfaces use in-place NEON reverse (~50x vs rotozoom).
 * Other formats keep the stock rotozoom blit path.
 */
SDL_Surface *rotate180(SDL_Surface *original)
{
    if (!original) {
        return NULL;
    }

    if (original->format->BytesPerPixel == 4 &&
        original->pitch == original->w * 4) {
        SDL_LockSurface(original);
        neon_rotate180_inplace((uint32_t *)original->pixels, original->w * original->h);
        SDL_UnlockSurface(original);
        return original;
    }

    /* Fallback for non-32bpp / padded pitch: stock rotozoom path */
    SDL_Surface *rotated = rotozoomSurface(original, 180.0, 1.0, 0);
    SDL_FillRect(original, NULL, SDL_MapRGB(original->format, 255, 0, 0));
    SDL_Rect rect = {-2, -2};
    SDL_BlitSurface(rotated, NULL, original, &rect);
    SDL_FreeSurface(rotated);
    return original;
}

#endif // ROTATE_180_H__
