#ifndef UTILS_SURFACE_SET_ALPHA_H__
#define UTILS_SURFACE_SET_ALPHA_H__

#include <SDL/SDL.h>
#include <stdint.h>
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>
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
        // Pre-compute alpha scale as fixed-point (8.8) to avoid per-pixel float
        const uint32_t alpha_scale = ((uint32_t)alpha * 257 + 1) >> 8;
        const uint32_t a_shift = fmt->Ashift;
        const uint32_t a_mask = fmt->Amask;
        const uint32_t rgb_mask = ~a_mask;

        SDL_LockSurface(surface);

        const int total_pixels = surface->w * surface->h;
        Uint32 *pixels = (Uint32 *)surface->pixels;

        if (surface->pitch == surface->w * (int)fmt->BytesPerPixel) {
#if defined(__ARM_NEON__) || defined(__ARM_NEON)
            // NEON-friendly: process alpha bytes directly, 8 pixels at a time
            const int a_byte = a_shift / 8;
            int i = 0;
            const int neon_limit = total_pixels - 7;
            const uint8x8_t vscale = vdup_n_u8((uint8_t)(alpha > 0 ? alpha : 1));
            for (; i < neon_limit; i += 8) {
                uint8_t *p = (uint8_t *)&pixels[i];
                // Gather 8 alpha bytes from stride-4 positions
                uint8x8_t alphas = (uint8x8_t){
                    p[a_byte], p[a_byte + 4], p[a_byte + 8], p[a_byte + 12],
                    p[a_byte + 16], p[a_byte + 20], p[a_byte + 24], p[a_byte + 28]
                };
                // Widen multiply: uint8 * uint8 → uint16, then take high byte ≈ (a * scale) / 256
                uint16x8_t wide = vmull_u8(alphas, vscale);
                uint8x8_t result = vshrn_n_u16(wide, 8);
                // Scatter back to alpha byte positions
                p[a_byte]      = vget_lane_u8(result, 0);
                p[a_byte + 4]  = vget_lane_u8(result, 1);
                p[a_byte + 8]  = vget_lane_u8(result, 2);
                p[a_byte + 12] = vget_lane_u8(result, 3);
                p[a_byte + 16] = vget_lane_u8(result, 4);
                p[a_byte + 20] = vget_lane_u8(result, 5);
                p[a_byte + 24] = vget_lane_u8(result, 6);
                p[a_byte + 28] = vget_lane_u8(result, 7);
            }
            for (; i < total_pixels; i++) {
                uint32_t px = pixels[i];
                uint32_t a = (px & a_mask) >> a_shift;
                a = (a * alpha_scale) >> 8;
                pixels[i] = (px & rgb_mask) | (a << a_shift);
            }
#else
            for (int i = 0; i < total_pixels; i++) {
                uint32_t px = pixels[i];
                uint32_t a = (px & a_mask) >> a_shift;
                a = (a * alpha_scale) >> 8;
                pixels[i] = (px & rgb_mask) | (a << a_shift);
            }
#endif
        }
        else {
            for (int y = 0; y < surface->h; ++y) {
                Uint32 *row = (Uint32 *)((Uint8 *)surface->pixels + y * surface->pitch);
                for (int x = 0; x < surface->w; ++x) {
                    uint32_t px = row[x];
                    uint32_t a = (px & a_mask) >> a_shift;
                    a = (a * alpha_scale) >> 8;
                    row[x] = (px & rgb_mask) | (a << a_shift);
                }
            }
        }

        SDL_UnlockSurface(surface);
    }
}

#endif // UTILS_SURFACE_SET_ALPHA_H__
