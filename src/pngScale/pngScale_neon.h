#ifndef PNGSCALE_NEON_H
#define PNGSCALE_NEON_H

#include <stdint.h>
#include <arm_neon.h>

// ARM NEON optimized PNG pixel format conversions
// Target: ARM Cortex-A7 with NEON-VFPv4

// Optimized conversion for 1-channel (grayscale) PNG to ARGB8888
// Processes 8 pixels at once using NEON SIMD
static inline void convert_grayscale_neon(const uint8_t *src, uint32_t *dst, uint32_t count)
{
    uint32_t i;
    const uint32_t simd_count = count & ~7;  // Process 8 pixels at a time
    
    // NEON SIMD loop - processes 8 pixels per iteration
    for (i = 0; i < simd_count; i += 8) {
        // Load 8 grayscale pixels
        uint8x8_t gray = vld1_u8(src + i);
        
        // Create ARGB components
        // Alpha = 0xFF (fully opaque)
        uint8x8_t alpha = vdup_n_u8(0xFF);
        
        // R = G = B = gray value
        uint8x8_t r = gray;
        uint8x8_t g = gray;
        uint8x8_t b = gray;
        
        // Interleave to create RGBA pixels
        uint8x8x4_t rgba;
        rgba.val[0] = b;  // Blue (byte 0 in little-endian ARGB8888)
        rgba.val[1] = g;  // Green (byte 1)
        rgba.val[2] = r;  // Red (byte 2)
        rgba.val[3] = alpha;  // Alpha (byte 3, MSB in little-endian)
        
        // Store as ARGB8888 format (0xAARRGGBB in memory appears as BGRA bytes in little-endian)
        vst4_u8((uint8_t *)(dst + i), rgba);
    }
    
    // Scalar cleanup for remaining pixels
    for (i = simd_count; i < count; i++) {
        uint8_t val = src[i];
        dst[i] = 0xFF000000 | (val << 16) | (val << 8) | val;
    }
}

// Optimized conversion for 3-channel (RGB) PNG to ARGB8888
// Processes 8 pixels at once using NEON SIMD
static inline void convert_rgb_neon(const uint8_t *src, uint32_t *dst, uint32_t count)
{
    uint32_t i;
    const uint32_t simd_count = count & ~7;  // Process 8 pixels at a time
    
    // NEON SIMD loop - processes 8 pixels per iteration
    for (i = 0; i < simd_count; i += 8) {
        // Load 24 bytes (8 RGB pixels)
        uint8x8x3_t rgb = vld3_u8(src + i * 3);
        
        // Alpha = 0xFF (fully opaque)
        uint8x8_t alpha = vdup_n_u8(0xFF);
        
        // Create RGBA structure (BGRA for little-endian ARGB8888)
        uint8x8x4_t rgba;
        rgba.val[0] = rgb.val[2];  // Blue (from RGB[2])
        rgba.val[1] = rgb.val[1];  // Green
        rgba.val[2] = rgb.val[0];  // Red (from RGB[0])
        rgba.val[3] = alpha;       // Alpha
        
        // Store 8 ARGB8888 pixels
        vst4_u8((uint8_t *)(dst + i), rgba);
    }
    
    // Scalar cleanup for remaining pixels
    for (i = simd_count; i < count; i++) {
        const uint8_t *p = src + i * 3;
        dst[i] = 0xFF000000 | (p[0] << 16) | (p[1] << 8) | p[2];
    }
}

// Optimized conversion for 2-channel (grayscale+alpha) PNG to ARGB8888
// Processes 8 pixels at once using NEON SIMD
static inline void convert_gray_alpha_neon(const uint8_t *src, uint32_t *dst, uint32_t count)
{
    uint32_t i;
    const uint32_t simd_count = count & ~7;  // Process 8 pixels at a time
    
    // NEON SIMD loop
    for (i = 0; i < simd_count; i += 8) {
        // Load 16 bytes (8 gray+alpha pairs)
        uint8x8x2_t ga = vld2_u8(src + i * 2);
        
        uint8x8_t gray = ga.val[0];
        uint8x8_t alpha = ga.val[1];
        
        // Create RGBA structure
        uint8x8x4_t rgba;
        rgba.val[0] = gray;   // Blue
        rgba.val[1] = gray;   // Green
        rgba.val[2] = gray;   // Red
        rgba.val[3] = alpha;  // Alpha
        
        // Store 8 ARGB8888 pixels
        vst4_u8((uint8_t *)(dst + i), rgba);
    }
    
    // Scalar cleanup
    for (i = simd_count; i < count; i++) {
        const uint8_t *p = src + i * 2;
        dst[i] = (p[1] << 24) | (p[0] << 16) | (p[0] << 8) | p[0];
    }
}

// Optimized conversion for 4-channel (RGBA) PNG to ARGB8888
// Mainly performs R<->B swap, processes 8 pixels at once
static inline void convert_rgba_neon(const uint32_t *src, uint32_t *dst, uint32_t count)
{
    uint32_t i;
    const uint32_t simd_count = count & ~7;  // Process 8 pixels at a time
    
    // NEON SIMD loop for R<->B swap
    for (i = 0; i < simd_count; i += 8) {
        // Load 8 RGBA pixels
        uint8x8x4_t rgba = vld4_u8((const uint8_t *)(src + i));
        
        // Swap R and B channels
        uint8x8x4_t bgra;
        bgra.val[0] = rgba.val[2];  // B <- R
        bgra.val[1] = rgba.val[1];  // G stays
        bgra.val[2] = rgba.val[0];  // R <- B
        bgra.val[3] = rgba.val[3];  // A stays
        
        // Store 8 ARGB8888 pixels
        vst4_u8((uint8_t *)(dst + i), bgra);
    }
    
    // Scalar cleanup - optimized with single operation
    for (i = simd_count; i < count; i++) {
        uint32_t pix = src[i];
        // Swap R and B: keep G and A, swap R (bits 16-23) with B (bits 0-7)
        dst[i] = (pix & 0xFF00FF00) | ((pix << 16) & 0x00FF0000) | ((pix >> 16) & 0x000000FF);
    }
}

#endif // PNGSCALE_NEON_H
