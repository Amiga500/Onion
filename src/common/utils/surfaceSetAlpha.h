#ifndef UTILS_SURFACE_SET_ALPHA_H__
#define UTILS_SURFACE_SET_ALPHA_H__

#include <SDL/SDL.h>
#include <stdint.h>

#define FIXED_POINT_ROUNDING_BIAS 128
#define FIXED_POINT_SCALE_257 257

// Changes a surface's alpha value, by altering per-pixel alpha if necessary.
void surfaceSetAlpha(SDL_Surface *surface, Uint8 alpha)
{
    SDL_PixelFormat *fmt = surface->format;

    // If surface has no alpha channel, just set the surface alpha.
    if (__builtin_expect(fmt->Amask == 0, 1)) {
        SDL_SetAlpha(surface, SDL_SRCALPHA, alpha);
    }
    // Else change the alpha of each pixel.
    else {
        unsigned bpp = fmt->BytesPerPixel;

        SDL_LockSurface(surface);

        for (int y = 0; y < surface->h; ++y)
            for (int x = 0; x < surface->w; ++x) {
                // Get a pointer to the current pixel.
                Uint32 *pixel_ptr = (Uint32 *)((Uint8 *)surface->pixels +
                                               y * surface->pitch + x * bpp);

                // Get the old pixel components.
                Uint8 r, g, b, a;
                SDL_GetRGBA(*pixel_ptr, fmt, &r, &g, &b, &a);

                // Set the pixel with the new alpha.
                uint32_t prod = (uint32_t)a * alpha;
                // (prod + bias) * scale >> 16 approximates (prod / 255) with rounding.
                Uint8 out_a = (Uint8)(((prod + FIXED_POINT_ROUNDING_BIAS) *
                                       FIXED_POINT_SCALE_257) >>
                                      16);
                *pixel_ptr = SDL_MapRGBA(fmt, r, g, b, out_a);
            }

        SDL_UnlockSurface(surface);
    }
}

#endif // UTILS_SURFACE_SET_ALPHA_H__
