#ifndef SURFACE_SET_ALPHA_NEON_H
#define SURFACE_SET_ALPHA_NEON_H

#include <stdint.h>
#include <arm_neon.h>

// ARM NEON optimized alpha blending for ARGB8888 surfaces
// Processes 4 pixels at once using NEON SIMD

// Fast alpha scaling using NEON - processes 4 ARGB8888 pixels at once
static inline void scale_alpha_neon_argb8888(uint32_t *pixels, uint32_t count, uint8_t alpha)
{
    uint32_t i;
    const uint32_t simd_count = count & ~3;  // Process 4 pixels at a time
    
    // Create scale factor in 16-bit for precision
    uint16_t scale = (alpha * 257) >> 8;  // Convert to 16-bit scale
    uint16x4_t scale_vec = vdup_n_u16(scale);
    
    // NEON SIMD loop - processes 4 pixels per iteration
    for (i = 0; i < simd_count; i += 4) {
        // Load 4 ARGB8888 pixels
        uint8x16_t pixels_vec = vld1q_u8((uint8_t *)(pixels + i));
        
        // Extract alpha channel (every 4th byte starting at offset 3)
        uint8x8_t alpha_low = vget_low_u8(pixels_vec);
        uint8x8_t alpha_high = vget_high_u8(pixels_vec);
        
        // Extract alpha values from ARGB format (byte 3, 7, 11, 15)
        // Shift to get alpha bytes and extract
        uint16x4_t alpha_16_0 = vmovl_u8(alpha_low);
        uint16x4_t alpha_16_1 = vmovl_u8(alpha_high);
        
        // Extract every other 16-bit value to get alpha channel
        uint16x4_t alpha_0 = vshr_n_u16(alpha_16_0, 8);  // Gets bytes 3, 7
        uint16x4_t alpha_1 = vshr_n_u16(alpha_16_1, 8);  // Gets bytes 11, 15
        
        // Scale alpha values
        alpha_0 = vmul_u16(alpha_0, scale_vec);
        alpha_1 = vmul_u16(alpha_1, scale_vec);
        alpha_0 = vshr_n_u16(alpha_0, 8);
        alpha_1 = vshr_n_u16(alpha_1, 8);
        
        // Convert back to 8-bit
        uint8x8_t alpha_new_low = vmovn_u16(vcombine_u16(alpha_0, alpha_0));
        uint8x8_t alpha_new_high = vmovn_u16(vcombine_u16(alpha_1, alpha_1));
        
        // Merge back into pixels (only update alpha channel)
        // Mask to keep RGB, update only A
        uint32_t mask_rgb = 0x00FFFFFF;
        
        // Process each pixel individually for correct alpha placement
        uint32_t *p = pixels + i;
        uint8_t new_alpha[4];
        vst1_u8(new_alpha, alpha_new_low);
        
        p[0] = (p[0] & mask_rgb) | ((uint32_t)new_alpha[0] << 24);
        p[1] = (p[1] & mask_rgb) | ((uint32_t)new_alpha[2] << 24);
        
        vst1_u8(new_alpha, alpha_new_high);
        p[2] = (p[2] & mask_rgb) | ((uint32_t)new_alpha[0] << 24);
        p[3] = (p[3] & mask_rgb) | ((uint32_t)new_alpha[2] << 24);
    }
    
    // Scalar cleanup for remaining pixels
    for (i = simd_count; i < count; i++) {
        uint32_t pixel = pixels[i];
        uint8_t old_alpha = (pixel >> 24) & 0xFF;
        uint8_t new_alpha = (old_alpha * alpha) / 255;
        pixels[i] = (pixel & 0x00FFFFFF) | ((uint32_t)new_alpha << 24);
    }
}

// Optimized version using direct bit manipulation (no NEON, but fast)
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
