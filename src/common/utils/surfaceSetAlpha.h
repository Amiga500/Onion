#ifndef UTILS_SURFACE_SET_ALPHA_H__
#define UTILS_SURFACE_SET_ALPHA_H__

#include <SDL/SDL.h>
#include <stdint.h>
#include "arm_optimization.h"

#ifdef __ARM_NEON__
#include <arm_neon.h>

/**
 * NEON-optimized alpha scaling for ARGB8888 format
 * Processes 4 pixels in parallel using SIMD instructions
 * Computes: new_alpha = (old_alpha * alpha) / 255
 */
static FORCE_INLINE void surfaceSetAlpha_NEON_ARGB8888(
    uint32_t * restrict pixels, int width, int height, int pitch_pixels, uint8_t alpha)
{
    // For integer division by 255, we use: (x * 257) >> 16
    // This works because 257/65536 = 0.00391388... ≈ 1/255 = 0.00392157...
    const uint16_t scale = alpha;
    const uint16x4_t scale_vec = vdup_n_u16(scale);
    const uint16x4_t multiplier = vdup_n_u16(257);
    const uint16x4_t mask_255 = vdup_n_u16(0xFF);
    
    for (int y = 0; y < height; ++y) {
        uint32_t * restrict row = pixels + y * pitch_pixels;
        int x = 0;
        
        // Process 4 pixels at a time with NEON
        for (; x <= width - 4; x += 4) {
            // Load 4 ARGB pixels (128 bits)
            uint32x4_t pixels_vec = vld1q_u32(row + x);
            
            // Extract alpha channel (bits 24-31) from each pixel
            uint16x4_t alpha_old = vmovn_u32(vshrq_n_u32(pixels_vec, 24));
            
            // Compute: new_alpha = (old_alpha * scale * 257) >> 16
            // Step 1: old_alpha * scale (16-bit x 16-bit = 32-bit)
            uint32x4_t alpha_mult = vmull_u16(alpha_old, scale_vec);
            // Step 2: result * 257 (32-bit x 16-bit = 32-bit)
            alpha_mult = vmulq_u32(alpha_mult, vmovl_u16(multiplier));
            // Step 3: shift right by 16 to divide by 65536
            uint16x4_t alpha_scaled = vmovn_u32(vshrq_n_u32(alpha_mult, 16));
            
            // Clamp to 0-255 range (should already be in range, but safety check)
            alpha_scaled = vmin_u16(alpha_scaled, mask_255);
            
            // Reconstruct pixels: clear old alpha, insert new alpha
            uint32x4_t rgb_mask = vdupq_n_u32(0x00FFFFFF);
            uint32x4_t pixels_rgb = vandq_u32(pixels_vec, rgb_mask);
            uint32x4_t alpha_shifted = vshlq_n_u32(vmovl_u16(alpha_scaled), 24);
            uint32x4_t pixels_new = vorrq_u32(pixels_rgb, alpha_shifted);
            
            // Store result
            vst1q_u32(row + x, pixels_new);
        }
        
        // Handle remaining pixels (scalar fallback)
        for (; x < width; ++x) {
            uint32_t pixel = row[x];
            uint8_t old_alpha = (pixel >> 24) & 0xFF;
            // Integer division by 255: (x * 257) >> 16
            uint8_t new_alpha = (old_alpha * alpha * 257) >> 16;
            row[x] = (pixel & 0x00FFFFFF) | (new_alpha << 24);
        }
    }
}
#endif // __ARM_NEON__

// Changes a surface's alpha value, by altering per-pixel alpha if necessary.
void surfaceSetAlpha(SDL_Surface *surface, Uint8 alpha)
{
    SDL_PixelFormat *fmt = surface->format;

    // If surface has no alpha channel, just set the surface alpha.
    if (unlikely(fmt->Amask == 0)) {
        SDL_SetAlpha(surface, SDL_SRCALPHA, alpha);
        return;
    }
    
    // Else change the alpha of each pixel.
    unsigned bpp = fmt->BytesPerPixel;
    
    SDL_LockSurface(surface);
    
#ifdef __ARM_NEON__
    // Fast path: NEON optimization for standard ARGB8888 format
    // Check for ARGB8888 with 4 bytes per pixel and alpha in high byte
    if (likely(bpp == 4 && fmt->Amask == 0xFF000000 && 
               fmt->Rmask == 0x00FF0000 && fmt->Gmask == 0x0000FF00 && 
               fmt->Bmask == 0x000000FF)) {
        int pitch_pixels = surface->pitch / 4;
        surfaceSetAlpha_NEON_ARGB8888((uint32_t*)surface->pixels, 
                                      surface->w, surface->h, 
                                      pitch_pixels, alpha);
        SDL_UnlockSurface(surface);
        return;
    }
#endif
    
    // Fallback: Generic path with SDL functions (for non-standard formats)
    // Integer approximation of division by 255
    // Using: (x * alpha * 257) >> 16 ≈ (x * alpha) / 255 (error < 0.4%)
    // This avoids expensive division and float operations
    
    for (int y = 0; y < surface->h; ++y) {
        for (int x = 0; x < surface->w; ++x) {
            // Get a pointer to the current pixel.
            Uint32 *pixel_ptr = (Uint32 *)((Uint8 *)surface->pixels +
                                           y * surface->pitch + x * bpp);

            // Get the old pixel components.
            Uint8 r, g, b, a;
            SDL_GetRGBA(*pixel_ptr, fmt, &r, &g, &b, &a);

            // Apply integer alpha scaling: new_a = (old_a * alpha * 257) >> 16
            Uint8 new_a = (a * alpha * 257) >> 16;

            // Set the pixel with the new alpha.
            *pixel_ptr = SDL_MapRGBA(fmt, r, g, b, new_a);
        }
    }

    SDL_UnlockSurface(surface);
}

#endif // UTILS_SURFACE_SET_ALPHA_H__
