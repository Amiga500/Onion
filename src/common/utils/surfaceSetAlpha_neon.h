#ifndef SURFACE_SET_ALPHA_NEON_H
#define SURFACE_SET_ALPHA_NEON_H

#include <stdint.h>

// Optimized alpha scaling using fast integer arithmetic
// This provides significant speedup without complex NEON code

// Fast alpha scaling using bit shifts instead of division
// Processes ARGB8888 pixels with direct bit manipulation
static inline void scale_alpha_fast(uint32_t *pixels, uint32_t count, uint8_t alpha)
{
    // Fast integer scaling: multiply by alpha and divide by 256 (shift)
    uint32_t scale = alpha;
    
    for (uint32_t i = 0; i < count; i++) {
        uint32_t pixel = pixels[i];
        uint32_t old_alpha = (pixel >> 24) & 0xFF;
        uint32_t new_alpha = (old_alpha * scale) >> 8;  // Fast divide by 256
        pixels[i] = (pixel & 0x00FFFFFF) | (new_alpha << 24);
    }
}

#endif // SURFACE_SET_ALPHA_NEON_H
