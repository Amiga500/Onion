#ifndef UTILS_SURFACE_SET_ALPHA_H__
#define UTILS_SURFACE_SET_ALPHA_H__

#include <SDL/SDL.h>
#include <stdint.h>

#ifdef __ARM_NEON
#include "surfaceSetAlpha_neon.h"
#define USE_NEON_ALPHA_OPT 1
#else
#define USE_NEON_ALPHA_OPT 0
#endif

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
        unsigned bpp = fmt->BytesPerPixel;
        
        SDL_LockSurface(surface);
        
        // Fast path for ARGB8888/RGBA8888 32-bit formats
        // Check if this is a 32-bit format with an alpha channel
        if (bpp == 4 && fmt->Amask != 0) {
            uint32_t total_pixels = surface->w * surface->h;
            uint32_t *pixel_ptr = (uint32_t *)surface->pixels;
            
            // Determine alpha channel position from mask
            uint32_t alpha_shift = 0;
            uint32_t mask = fmt->Amask;
            while (mask && !(mask & 1)) {
                alpha_shift++;
                mask >>= 1;
            }
            uint32_t rgb_mask = ~fmt->Amask;
            
#if USE_NEON_ALPHA_OPT
            // Use optimized implementation for standard ARGB8888 (alpha at bit 24)
            if (alpha_shift == 24) {
                scale_alpha_fast(pixel_ptr, total_pixels, alpha);
            }
            else {
                // Generic path for other alpha positions
                for (uint32_t i = 0; i < total_pixels; i++) {
                    uint32_t pixel = pixel_ptr[i];
                    uint32_t old_alpha = (pixel >> alpha_shift) & 0xFF;
                    uint32_t new_alpha = (old_alpha * alpha) >> 8;
                    pixel_ptr[i] = (pixel & rgb_mask) | (new_alpha << alpha_shift);
                }
            }
#else
            // Fallback fast integer implementation
            for (uint32_t i = 0; i < total_pixels; i++) {
                uint32_t pixel = pixel_ptr[i];
                uint32_t old_alpha = (pixel >> alpha_shift) & 0xFF;
                uint32_t new_alpha = (old_alpha * alpha) >> 8;
                pixel_ptr[i] = (pixel & rgb_mask) | (new_alpha << alpha_shift);
            }
#endif
        }
        // Original slow path for other formats
        else {
            // Scaling factor to clamp alpha to [0, alpha].
            float scale = alpha / 255.0f;

            for (int y = 0; y < surface->h; ++y)
                for (int x = 0; x < surface->w; ++x) {
                    // Get a pointer to the current pixel.
                    Uint32 *pixel_ptr = (Uint32 *)((Uint8 *)surface->pixels +
                                                   y * surface->pitch + x * bpp);

                    // Get the old pixel components.
                    Uint8 r, g, b, a;
                    SDL_GetRGBA(*pixel_ptr, fmt, &r, &g, &b, &a);

                    // Set the pixel with the new alpha.
                    *pixel_ptr = SDL_MapRGBA(fmt, r, g, b, scale * a);
                }
        }

        SDL_UnlockSurface(surface);
    }
}

#endif // UTILS_SURFACE_SET_ALPHA_H__
