#ifndef UTILS_SURFACE_SET_ALPHA_H__
#define UTILS_SURFACE_SET_ALPHA_H__

#include <SDL/SDL.h>
#include <stdint.h>

// Changes a surface's alpha value, by altering per-pixel alpha if necessary.
void surfaceSetAlpha(SDL_Surface *surface, Uint8 alpha)
{
    SDL_PixelFormat *fmt = surface->format;

    // If surface has no alpha channel, just set the surface alpha.
    if (fmt->Amask == 0) {
        SDL_SetAlpha(surface, SDL_SRCALPHA, alpha);
    }
    // Else change the alpha of each pixel.
    else {
        // Precompute alpha lookup table (0-255 scaled by alpha/255)
        // Using integer arithmetic: scaled_alpha = (a * alpha) / 255
        Uint8 alpha_lut[256];
        for (int i = 0; i < 256; i++) {
            alpha_lut[i] = (Uint8)((i * alpha) / 255);
        }

        SDL_LockSurface(surface);

        const int pitch = surface->pitch;
        const int width = surface->w;
        const int height = surface->h;
        Uint8 *pixels = (Uint8 *)surface->pixels;

        for (int y = 0; y < height; ++y) {
            // Calculate row pointer once per row instead of per pixel
            Uint32 *row_ptr = (Uint32 *)(pixels + y * pitch);
            Uint32 *row_end = row_ptr + width;
            
            // Use pointer arithmetic for inner loop (more efficient than index-based access)
            for (Uint32 *pixel_ptr = row_ptr; pixel_ptr < row_end; ++pixel_ptr) {
                // Get the old pixel components.
                Uint8 r, g, b, a;
                SDL_GetRGBA(*pixel_ptr, fmt, &r, &g, &b, &a);

                // Set the pixel with the new alpha using precomputed LUT
                *pixel_ptr = SDL_MapRGBA(fmt, r, g, b, alpha_lut[a]);
            }
        }

        SDL_UnlockSurface(surface);
    }
}

#endif // UTILS_SURFACE_SET_ALPHA_H__
