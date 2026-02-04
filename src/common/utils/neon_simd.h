/**
 * @file neon_simd.h
 * @brief ARM NEON SIMD optimizations for Cortex-A7 (Miyoo Mini)
 *
 * This header provides NEON-accelerated implementations for performance-critical
 * pixel operations commonly used in OnionOS. These optimizations target the
 * Cortex-A7 processor with NEON-VFPv4 support.
 *
 * Key optimizations:
 * - Pixel format conversions (RGB888->ARGB8888, RGBA->ARGB, etc.)
 * - Memory copy with prefetch hints
 * - Alpha blending operations
 * - Bilinear interpolation for image scaling
 *
 * Usage:
 *   #include "utils/neon_simd.h"
 *   // Use neon_* functions when processing pixel data in bulk
 *
 * Build requirements:
 *   CFLAGS: -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
 */

#ifndef UTILS_NEON_SIMD_H__
#define UTILS_NEON_SIMD_H__

#include <stdint.h>
#include <string.h>

#ifdef __ARM_NEON
#include <arm_neon.h>
#define NEON_AVAILABLE 1
#else
#define NEON_AVAILABLE 0
#endif

/**
 * @brief Memory prefetch hint for improved cache utilization
 * @param addr Memory address to prefetch
 *
 * Issues a prefetch hint to the CPU to load the memory at 'addr' into cache.
 * This is particularly useful before processing large pixel arrays.
 */
#ifdef __ARM_NEON
#define neon_prefetch(addr) __builtin_prefetch(addr, 0, 3)
#define neon_prefetch_write(addr) __builtin_prefetch(addr, 1, 3)
#else
#define neon_prefetch(addr) ((void)0)
#define neon_prefetch_write(addr) ((void)0)
#endif

/**
 * @brief Convert RGB888 pixels to ARGB8888 using NEON SIMD
 *
 * Converts an array of RGB888 (3 bytes per pixel) to ARGB8888 (4 bytes per pixel).
 * The alpha channel is set to 0xFF (fully opaque).
 *
 * @param dst Destination buffer (must be at least width * 4 bytes)
 * @param src Source buffer (RGB888, 3 bytes per pixel)
 * @param width Number of pixels to convert
 *
 * Performance: ~4x faster than scalar implementation for large buffers
 */
static inline void neon_rgb888_to_argb8888(uint32_t *dst, const uint8_t *src, uint32_t width)
{
#ifdef __ARM_NEON
    uint32_t i = 0;
    uint32_t simd_width = width & ~7u; // Process 8 pixels at a time

    // Prefetch source data
    neon_prefetch(src);
    neon_prefetch(src + 64);

    // Process 8 pixels per iteration using NEON
    for (; i < simd_width; i += 8) {
        neon_prefetch(src + i * 3 + 128);
        neon_prefetch_write(dst + i + 32);

        // Load 24 bytes (8 RGB pixels) using structure load
        uint8x8x3_t rgb = vld3_u8(src + i * 3);

        // Create alpha channel (fully opaque)
        uint8x8_t alpha = vdup_n_u8(0xFF);

        // Interleave to ARGB8888: [A, R, G, B] in memory
        // For little-endian ARM, store as [B, G, R, A] to get 0xAARRGGBB
        uint8x8x4_t argb;
        argb.val[0] = rgb.val[2]; // B
        argb.val[1] = rgb.val[1]; // G
        argb.val[2] = rgb.val[0]; // R
        argb.val[3] = alpha;      // A

        vst4_u8((uint8_t *)(dst + i), argb);
    }

    // Handle remaining pixels with scalar code
    for (; i < width; i++) {
        const uint8_t *s = src + i * 3;
        dst[i] = 0xFF000000u | ((uint32_t)s[0] << 16) | ((uint32_t)s[1] << 8) | s[2];
    }
#else
    // Scalar fallback
    for (uint32_t i = 0; i < width; i++) {
        const uint8_t *s = src + i * 3;
        dst[i] = 0xFF000000u | ((uint32_t)s[0] << 16) | ((uint32_t)s[1] << 8) | s[2];
    }
#endif
}

/**
 * @brief Convert grayscale pixels to ARGB8888 using NEON SIMD
 *
 * @param dst Destination buffer (ARGB8888)
 * @param src Source buffer (grayscale, 1 byte per pixel)
 * @param width Number of pixels to convert
 */
static inline void neon_gray_to_argb8888(uint32_t *dst, const uint8_t *src, uint32_t width)
{
#ifdef __ARM_NEON
    uint32_t i = 0;
    uint32_t simd_width = width & ~7u;

    neon_prefetch(src);

    for (; i < simd_width; i += 8) {
        neon_prefetch(src + i + 64);
        neon_prefetch_write(dst + i + 32);

        // Load 8 grayscale pixels
        uint8x8_t gray = vld1_u8(src + i);
        uint8x8_t alpha = vdup_n_u8(0xFF);

        // Replicate gray to RGB channels
        uint8x8x4_t argb;
        argb.val[0] = gray;  // B
        argb.val[1] = gray;  // G
        argb.val[2] = gray;  // R
        argb.val[3] = alpha; // A

        vst4_u8((uint8_t *)(dst + i), argb);
    }

    // Scalar fallback for remainder
    for (; i < width; i++) {
        uint8_t g = src[i];
        dst[i] = 0xFF000000u | ((uint32_t)g << 16) | ((uint32_t)g << 8) | g;
    }
#else
    for (uint32_t i = 0; i < width; i++) {
        uint8_t g = src[i];
        dst[i] = 0xFF000000u | ((uint32_t)g << 16) | ((uint32_t)g << 8) | g;
    }
#endif
}

/**
 * @brief Convert grayscale+alpha pixels to ARGB8888 using NEON SIMD
 *
 * @param dst Destination buffer (ARGB8888)
 * @param src Source buffer (grayscale+alpha, 2 bytes per pixel)
 * @param width Number of pixels to convert
 */
static inline void neon_graya_to_argb8888(uint32_t *dst, const uint8_t *src, uint32_t width)
{
#ifdef __ARM_NEON
    uint32_t i = 0;
    uint32_t simd_width = width & ~7u;

    neon_prefetch(src);

    for (; i < simd_width; i += 8) {
        neon_prefetch(src + i * 2 + 64);
        neon_prefetch_write(dst + i + 32);

        // Load 8 GA pixels (16 bytes)
        uint8x8x2_t ga = vld2_u8(src + i * 2);

        // Replicate gray to RGB channels
        uint8x8x4_t argb;
        argb.val[0] = ga.val[0]; // B = gray
        argb.val[1] = ga.val[0]; // G = gray
        argb.val[2] = ga.val[0]; // R = gray
        argb.val[3] = ga.val[1]; // A = alpha

        vst4_u8((uint8_t *)(dst + i), argb);
    }

    // Scalar fallback for remainder
    for (; i < width; i++) {
        const uint8_t *s = src + i * 2;
        dst[i] = ((uint32_t)s[1] << 24) | ((uint32_t)s[0] << 16) | ((uint32_t)s[0] << 8) | s[0];
    }
#else
    for (uint32_t i = 0; i < width; i++) {
        const uint8_t *s = src + i * 2;
        dst[i] = ((uint32_t)s[1] << 24) | ((uint32_t)s[0] << 16) | ((uint32_t)s[0] << 8) | s[0];
    }
#endif
}

/**
 * @brief Swap red and blue channels in RGBA/ARGB pixels using NEON SIMD
 *
 * Converts RGBA8888 to BGRA8888 (or vice versa) by swapping R and B channels.
 * This is commonly needed when converting between different pixel orderings.
 *
 * @param dst Destination buffer
 * @param src Source buffer
 * @param count Number of pixels to convert
 */
static inline void neon_swap_rb(uint32_t *dst, const uint32_t *src, uint32_t count)
{
#ifdef __ARM_NEON
    uint32_t i = 0;
    uint32_t simd_count = count & ~7u;

    neon_prefetch(src);

    for (; i < simd_count; i += 8) {
        neon_prefetch(src + i + 32);
        neon_prefetch_write(dst + i + 32);

        // Load 8 RGBA pixels (32 bytes)
        uint8x8x4_t rgba = vld4_u8((const uint8_t *)(src + i));

        // Swap R and B channels
        uint8x8_t temp = rgba.val[0];
        rgba.val[0] = rgba.val[2];
        rgba.val[2] = temp;

        vst4_u8((uint8_t *)(dst + i), rgba);
    }

    // Scalar fallback for remainder
    for (; i < count; i++) {
        uint32_t pix = src[i];
        dst[i] = (pix & 0xFF00FF00u) | ((pix & 0x00FF0000u) >> 16) | ((pix & 0x000000FFu) << 16);
    }
#else
    for (uint32_t i = 0; i < count; i++) {
        uint32_t pix = src[i];
        dst[i] = (pix & 0xFF00FF00u) | ((pix & 0x00FF0000u) >> 16) | ((pix & 0x000000FFu) << 16);
    }
#endif
}

/**
 * @brief Convert RGBA8888 to ARGB8888 using NEON SIMD
 *
 * Rearranges RGBA pixel data to ARGB format by swapping R<->B and keeping G,A.
 *
 * @param dst Destination buffer (ARGB8888)
 * @param src Source buffer (RGBA8888)
 * @param count Number of pixels to convert
 */
static inline void neon_rgba_to_argb(uint32_t *dst, const uint32_t *src, uint32_t count)
{
#ifdef __ARM_NEON
    uint32_t i = 0;
    uint32_t simd_count = count & ~7u;

    neon_prefetch(src);

    for (; i < simd_count; i += 8) {
        neon_prefetch(src + i + 32);
        neon_prefetch_write(dst + i + 32);

        // Load as RGBA (R, G, B, A)
        uint8x8x4_t rgba = vld4_u8((const uint8_t *)(src + i));

        // Reorder to ARGB in memory layout [B, G, R, A] for little-endian
        uint8x8x4_t argb;
        argb.val[0] = rgba.val[2]; // B
        argb.val[1] = rgba.val[1]; // G
        argb.val[2] = rgba.val[0]; // R
        argb.val[3] = rgba.val[3]; // A

        vst4_u8((uint8_t *)(dst + i), argb);
    }

    // Scalar fallback
    for (; i < count; i++) {
        uint32_t pix = src[i];
        dst[i] = (pix & 0xFF00FF00u) | ((pix & 0x00FF0000u) >> 16) | ((pix & 0x000000FFu) << 16);
    }
#else
    for (uint32_t i = 0; i < count; i++) {
        uint32_t pix = src[i];
        dst[i] = (pix & 0xFF00FF00u) | ((pix & 0x00FF0000u) >> 16) | ((pix & 0x000000FFu) << 16);
    }
#endif
}

/**
 * @brief Fast memory copy with prefetch hints
 *
 * Optimized memcpy for large pixel buffers with cache prefetch hints.
 *
 * @param dst Destination buffer
 * @param src Source buffer
 * @param bytes Number of bytes to copy
 */
static inline void neon_memcpy(void *dst, const void *src, size_t bytes)
{
#ifdef __ARM_NEON
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i = 0;

    // Prefetch first cache lines
    neon_prefetch(s);
    neon_prefetch(s + 64);

    // Process 64 bytes at a time (4 x 16-byte vectors)
    size_t simd_bytes = bytes & ~63u;
    for (; i < simd_bytes; i += 64) {
        neon_prefetch(s + i + 128);
        neon_prefetch_write(d + i + 128);

        uint8x16_t v0 = vld1q_u8(s + i);
        uint8x16_t v1 = vld1q_u8(s + i + 16);
        uint8x16_t v2 = vld1q_u8(s + i + 32);
        uint8x16_t v3 = vld1q_u8(s + i + 48);

        vst1q_u8(d + i, v0);
        vst1q_u8(d + i + 16, v1);
        vst1q_u8(d + i + 32, v2);
        vst1q_u8(d + i + 48, v3);
    }

    // Handle remaining bytes
    if (i < bytes) {
        memcpy(d + i, s + i, bytes - i);
    }
#else
    memcpy(dst, src, bytes);
#endif
}

/**
 * @brief Bilinear interpolation weights calculation using NEON
 *
 * Calculates bilinear interpolation weights for 8 pixels at once.
 * This is a helper for optimized image scaling.
 *
 * @param ex Array of 8 x-interpolation values (0x0000-0xFFFF)
 * @param ey Y-interpolation value (0x0000-0xFFFF)
 * @param c00 Top-left corner pixels (8 pixels)
 * @param c01 Top-right corner pixels (8 pixels)
 * @param c10 Bottom-left corner pixels (8 pixels)
 * @param c11 Bottom-right corner pixels (8 pixels)
 * @param dst Destination pixels (8 pixels)
 */
#ifdef __ARM_NEON
static inline void neon_bilinear_interp_8px(const uint16_t *ex, uint16_t ey,
                                            const uint8x8x4_t *c00, const uint8x8x4_t *c01,
                                            const uint8x8x4_t *c10, const uint8x8x4_t *c11,
                                            uint8x8x4_t *dst)
{
    // Load interpolation weights (expected range: 0x0000-0xFFFF)
    uint16x8_t vex = vld1q_u16(ex);
    uint16x8_t vey = vdupq_n_u16(ey);
    // For 16-bit fixed point, max value is 0xFFFF (represents 1.0)
    uint16x8_t vmax = vdupq_n_u16(0xFFFF);

    // Calculate complement weights: (1 - weight) in fixed point
    uint16x8_t vex_inv = vsubq_u16(vmax, vex);
    uint16x8_t vey_inv = vsubq_u16(vmax, vey);

    // Process each channel
    for (int ch = 0; ch < 4; ch++) {
        // Widen to 16-bit
        uint16x8_t p00 = vmovl_u8(c00->val[ch]);
        uint16x8_t p01 = vmovl_u8(c01->val[ch]);
        uint16x8_t p10 = vmovl_u8(c10->val[ch]);
        uint16x8_t p11 = vmovl_u8(c11->val[ch]);

        // Interpolate top row: t1 = p00 * (1-ex) + p01 * ex
        uint32x4_t t1_lo = vmull_u16(vget_low_u16(p00), vget_low_u16(vex_inv));
        uint32x4_t t1_hi = vmull_u16(vget_high_u16(p00), vget_high_u16(vex_inv));
        t1_lo = vmlal_u16(t1_lo, vget_low_u16(p01), vget_low_u16(vex));
        t1_hi = vmlal_u16(t1_hi, vget_high_u16(p01), vget_high_u16(vex));

        // Interpolate bottom row: t2 = p10 * (1-ex) + p11 * ex
        uint32x4_t t2_lo = vmull_u16(vget_low_u16(p10), vget_low_u16(vex_inv));
        uint32x4_t t2_hi = vmull_u16(vget_high_u16(p10), vget_high_u16(vex_inv));
        t2_lo = vmlal_u16(t2_lo, vget_low_u16(p11), vget_low_u16(vex));
        t2_hi = vmlal_u16(t2_hi, vget_high_u16(p11), vget_high_u16(vex));

        // Reduce to 16-bit (shift by 16)
        uint16x4_t t1_lo_16 = vshrn_n_u32(t1_lo, 16);
        uint16x4_t t1_hi_16 = vshrn_n_u32(t1_hi, 16);
        uint16x4_t t2_lo_16 = vshrn_n_u32(t2_lo, 16);
        uint16x4_t t2_hi_16 = vshrn_n_u32(t2_hi, 16);

        uint16x8_t t1 = vcombine_u16(t1_lo_16, t1_hi_16);
        uint16x8_t t2 = vcombine_u16(t2_lo_16, t2_hi_16);

        // Final vertical interpolation: result = t1 * (1-ey) + t2 * ey
        uint32x4_t r_lo = vmull_u16(vget_low_u16(t1), vget_low_u16(vey_inv));
        uint32x4_t r_hi = vmull_u16(vget_high_u16(t1), vget_high_u16(vey_inv));
        r_lo = vmlal_u16(r_lo, vget_low_u16(t2), vget_low_u16(vey));
        r_hi = vmlal_u16(r_hi, vget_high_u16(t2), vget_high_u16(vey));

        // Reduce to 8-bit
        uint16x4_t r_lo_16 = vshrn_n_u32(r_lo, 16);
        uint16x4_t r_hi_16 = vshrn_n_u32(r_hi, 16);
        uint16x8_t r16 = vcombine_u16(r_lo_16, r_hi_16);
        dst->val[ch] = vmovn_u16(r16);
    }
}
#endif

/**
 * @brief Fill memory with a constant 32-bit value using NEON
 *
 * Optimized memset for 32-bit values (useful for filling pixel buffers).
 *
 * @param dst Destination buffer (should be 4-byte aligned)
 * @param value 32-bit value to fill
 * @param count Number of 32-bit words to fill
 */
static inline void neon_fill32(uint32_t *dst, uint32_t value, uint32_t count)
{
#ifdef __ARM_NEON
    uint32_t i = 0;
    uint32_t simd_count = count & ~15u; // Process 16 values at a time

    uint32x4_t v = vdupq_n_u32(value);

    for (; i < simd_count; i += 16) {
        neon_prefetch_write(dst + i + 64);

        vst1q_u32(dst + i, v);
        vst1q_u32(dst + i + 4, v);
        vst1q_u32(dst + i + 8, v);
        vst1q_u32(dst + i + 12, v);
    }

    // Handle remaining values
    for (; i < count; i++) {
        dst[i] = value;
    }
#else
    for (uint32_t i = 0; i < count; i++) {
        dst[i] = value;
    }
#endif
}

/**
 * @brief Alpha blend two pixels using NEON
 *
 * Performs src-over alpha compositing for arrays of ARGB8888 pixels.
 *
 * @param dst Destination buffer (both source and destination)
 * @param src Source buffer (foreground)
 * @param count Number of pixels
 */
static inline void neon_alpha_blend(uint32_t *dst, const uint32_t *src, uint32_t count)
{
#ifdef __ARM_NEON
    uint32_t i = 0;
    uint32_t simd_count = count & ~7u;

    for (; i < simd_count; i += 8) {
        neon_prefetch(src + i + 32);
        neon_prefetch(dst + i + 32);

        // Load source and destination as separate channels
        uint8x8x4_t s = vld4_u8((const uint8_t *)(src + i));
        uint8x8x4_t d = vld4_u8((const uint8_t *)(dst + i));

        // Get source alpha
        uint16x8_t alpha = vmovl_u8(s.val[3]);
        uint16x8_t inv_alpha = vsubq_u16(vdupq_n_u16(255), alpha);

        // Blend each channel: result = src * alpha + dst * (255 - alpha)
        for (int ch = 0; ch < 3; ch++) {
            uint16x8_t s_ch = vmovl_u8(s.val[ch]);
            uint16x8_t d_ch = vmovl_u8(d.val[ch]);

            uint16x8_t result = vaddq_u16(vmulq_u16(s_ch, alpha), vmulq_u16(d_ch, inv_alpha));
            result = vshrq_n_u16(vaddq_u16(result, vdupq_n_u16(128)), 8);

            d.val[ch] = vmovn_u16(result);
        }

        // Keep destination alpha or use source alpha (src-over)
        d.val[3] = s.val[3];

        vst4_u8((uint8_t *)(dst + i), d);
    }

    // Scalar fallback
    for (; i < count; i++) {
        uint32_t sp = src[i];
        uint32_t dp = dst[i];
        uint32_t sa = (sp >> 24) & 0xFF;
        uint32_t ia = 255 - sa;

        uint32_t sr = (sp >> 16) & 0xFF;
        uint32_t sg = (sp >> 8) & 0xFF;
        uint32_t sb = sp & 0xFF;

        uint32_t dr = (dp >> 16) & 0xFF;
        uint32_t dg = (dp >> 8) & 0xFF;
        uint32_t db = dp & 0xFF;

        uint32_t rr = (sr * sa + dr * ia + 128) >> 8;
        uint32_t rg = (sg * sa + dg * ia + 128) >> 8;
        uint32_t rb = (sb * sa + db * ia + 128) >> 8;

        dst[i] = (sa << 24) | (rr << 16) | (rg << 8) | rb;
    }
#else
    for (uint32_t i = 0; i < count; i++) {
        uint32_t sp = src[i];
        uint32_t dp = dst[i];
        uint32_t sa = (sp >> 24) & 0xFF;
        uint32_t ia = 255 - sa;

        uint32_t sr = (sp >> 16) & 0xFF;
        uint32_t sg = (sp >> 8) & 0xFF;
        uint32_t sb = sp & 0xFF;

        uint32_t dr = (dp >> 16) & 0xFF;
        uint32_t dg = (dp >> 8) & 0xFF;
        uint32_t db = dp & 0xFF;

        uint32_t rr = (sr * sa + dr * ia + 128) >> 8;
        uint32_t rg = (sg * sa + dg * ia + 128) >> 8;
        uint32_t rb = (sb * sa + db * ia + 128) >> 8;

        dst[i] = (sa << 24) | (rr << 16) | (rg << 8) | rb;
    }
#endif
}

#endif // UTILS_NEON_SIMD_H__
