#ifndef NEON_PIXEL_H__
#define NEON_PIXEL_H__

/**
 * ARM NEON-accelerated pixel format conversion utilities
 * for Miyoo Mini/Mini+ (Cortex-A7, NEON VFPv4)
 *
 * Ported from Amiga500/Onion OniOpus46 with asm operand fixes:
 * avoid hard-coded r4/r5/r6 that can alias "+r" operands.
 *
 * All functions process 8–16 pixels per iteration using
 * VLD4/VST4 deinterleave with PLD prefetch.
 * Scalar fallback for remaining pixels and non-NEON builds.
 */

#include <stdint.h>

/**
 * Swap Red and Blue channels in-place for an array of ARGB8888 pixels.
 * Memory layout: [B,G,R,A] ↔ [R,G,B,A]
 */
static inline void neon_swap_rb_inplace(uint32_t *pixels, int count)
{
#ifdef __ARM_NEON
    int neon_count = count & ~15;
    uint32_t *p = pixels;
    if (neon_count > 0) {
        __asm__ volatile(
            "1:\n\t"
            "pld [%[p], #64]\n\t"
            "vld4.8 {d0-d3}, [%[p]]!\n\t"
            "vld4.8 {d4-d7}, [%[p]]\n\t"
            "sub %[p], %[p], #32\n\t"
            "vswp d0, d2\n\t"
            "vswp d4, d6\n\t"
            "vst4.8 {d0-d3}, [%[p]]!\n\t"
            "vst4.8 {d4-d7}, [%[p]]!\n\t"
            "subs %[n], %[n], #16\n\t"
            "bgt 1b\n\t"
            : [p] "+r"(p), [n] "+r"(neon_count)
            :
            : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "memory", "cc");
    }
    for (int i = (count & ~15); i < count; i++) {
        uint32_t px = pixels[i];
        pixels[i] = (px & 0xFF00FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16);
    }
#else
    for (int i = 0; i < count; i++) {
        uint32_t px = pixels[i];
        pixels[i] = (px & 0xFF00FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16);
    }
#endif
}

/**
 * Convert ARGB8888 → RGBA8888 for PNG output (swap R↔B channels).
 * Same channel op as neon_swap_rb_inplace but copies src→dst.
 */
static inline void neon_argb_to_rgba(uint32_t *dst, const uint32_t *src, int count)
{
#ifdef __ARM_NEON
    int neon_count = count & ~15;
    const uint32_t *s = src;
    uint32_t *d = dst;
    if (neon_count > 0) {
        __asm__ volatile(
            "1:\n\t"
            "pld [%[s], #64]\n\t"
            "vld4.8 {d0-d3}, [%[s]]!\n\t"
            "vld4.8 {d4-d7}, [%[s]]!\n\t"
            "vswp d0, d2\n\t"
            "vswp d4, d6\n\t"
            "vst4.8 {d0-d3}, [%[d]]!\n\t"
            "vst4.8 {d4-d7}, [%[d]]!\n\t"
            "subs %[n], %[n], #16\n\t"
            "bgt 1b\n\t"
            : [s] "+r"(s), [d] "+r"(d), [n] "+r"(neon_count)
            :
            : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "memory", "cc");
    }
    for (int i = (count & ~15); i < count; i++) {
        uint32_t px = src[i];
        dst[i] = (px & 0xFF00FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16);
    }
#else
    for (int i = 0; i < count; i++) {
        uint32_t px = src[i];
        dst[i] = (px & 0xFF00FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16);
    }
#endif
}

/**
 * Convert ARGB8888 → RGBA8888 with alpha-conditional zeroing for PNG.
 * Pixels with alpha=0 are output as 0x00000000.
 */
static inline void neon_argb_to_rgba_alpha(uint32_t *dst, const uint32_t *src, int count)
{
#ifdef __ARM_NEON
    int neon_count = count & ~7;
    const uint32_t *s = src;
    uint32_t *d = dst;
    if (neon_count > 0) {
        __asm__ volatile(
            "1:\n\t"
            "pld [%[s], #64]\n\t"
            "vld4.8 {d0-d3}, [%[s]]!\n\t" /* d0=B, d1=G, d2=R, d3=A */
            "vceq.i8 d4, d3, #0\n\t"
            "vswp d0, d2\n\t"
            "vbic d0, d0, d4\n\t"
            "vbic d1, d1, d4\n\t"
            "vbic d2, d2, d4\n\t"
            "vst4.8 {d0-d3}, [%[d]]!\n\t"
            "subs %[n], %[n], #8\n\t"
            "bgt 1b\n\t"
            : [s] "+r"(s), [d] "+r"(d), [n] "+r"(neon_count)
            :
            : "d0", "d1", "d2", "d3", "d4", "memory", "cc");
    }
    for (int i = (count & ~7); i < count; i++) {
        uint32_t px = src[i];
        dst[i] = (px & 0xFF000000)
                     ? ((px & 0xFF00FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16))
                     : 0;
    }
#else
    for (int i = 0; i < count; i++) {
        uint32_t px = src[i];
        dst[i] = (px & 0xFF000000)
                     ? ((px & 0xFF00FF00) | ((px & 0x00FF0000) >> 16) | ((px & 0x000000FF) << 16))
                     : 0;
    }
#endif
}

/**
 * Convert packed RGB888 (3 bytes/pixel) → ARGB8888 (4 bytes/pixel).
 * Sets alpha to 0xFF for all output pixels.
 */
static inline void neon_rgb888_to_argb(uint32_t *dst, const uint8_t *src, int count)
{
#ifdef __ARM_NEON
    int neon_count = count & ~15;
    const uint8_t *s = src;
    uint8_t *d = (uint8_t *)dst;
    if (neon_count > 0) {
        __asm__ volatile(
            "vmov.i8 d3, #0xFF\n\t"
            "vmov.i8 d7, #0xFF\n\t"
            "1:\n\t"
            "pld [%[s], #64]\n\t"
            "vld3.8 {d0-d2}, [%[s]]!\n\t"
            "vld3.8 {d4-d6}, [%[s]]!\n\t"
            "vswp d0, d2\n\t"
            "vswp d4, d6\n\t"
            "vst4.8 {d0-d3}, [%[d]]!\n\t"
            "vst4.8 {d4-d7}, [%[d]]!\n\t"
            "subs %[n], %[n], #16\n\t"
            "bgt 1b\n\t"
            : [s] "+r"(s), [d] "+r"(d), [n] "+r"(neon_count)
            :
            : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "memory", "cc");
    }
    for (int i = (count & ~15); i < count; i++) {
        const uint8_t *p = src + i * 3;
        dst[i] = 0xFF000000 | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    }
#else
    for (int i = 0; i < count; i++) {
        const uint8_t *p = src + i * 3;
        dst[i] = 0xFF000000 | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    }
#endif
}

/**
 * Convert 8-bit grayscale → ARGB8888. Alpha=0xFF, gray→R/G/B.
 */
static inline void neon_gray8_to_argb(uint32_t *dst, const uint8_t *src, int count)
{
#ifdef __ARM_NEON
    int neon_count = count & ~15;
    const uint8_t *s = src;
    uint8_t *d = (uint8_t *)dst;
    if (neon_count > 0) {
        __asm__ volatile(
            "1:\n\t"
            "pld [%[s], #64]\n\t"
            "vld1.8 {d0}, [%[s]]!\n\t"
            "vmov d1, d0\n\t"
            "vmov d2, d0\n\t"
            "vmov.i8 d3, #0xFF\n\t"
            "vld1.8 {d4}, [%[s]]!\n\t"
            "vmov d5, d4\n\t"
            "vmov d6, d4\n\t"
            "vmov.i8 d7, #0xFF\n\t"
            "vst4.8 {d0-d3}, [%[d]]!\n\t"
            "vst4.8 {d4-d7}, [%[d]]!\n\t"
            "subs %[n], %[n], #16\n\t"
            "bgt 1b\n\t"
            : [s] "+r"(s), [d] "+r"(d), [n] "+r"(neon_count)
            :
            : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "memory", "cc");
    }
    for (int i = (count & ~15); i < count; i++) {
        uint32_t g = src[i];
        dst[i] = 0xFF000000 | (g << 16) | (g << 8) | g;
    }
#else
    for (int i = 0; i < count; i++) {
        uint32_t g = src[i];
        dst[i] = 0xFF000000 | (g << 16) | (g << 8) | g;
    }
#endif
}

/**
 * Convert gray8+alpha (GA) → ARGB8888.
 * Uses vld2.8 {d0, d1} (ascending) — not the invalid stride form.
 */
static inline void neon_gray8a_to_argb(uint32_t *dst, const uint8_t *src, int count)
{
#ifdef __ARM_NEON
    int neon_count = count & ~15;
    const uint8_t *s = src;
    uint8_t *d = (uint8_t *)dst;
    if (neon_count > 0) {
        __asm__ volatile(
            "1:\n\t"
            "pld [%[s], #64]\n\t"
            "vld2.8 {d0, d1}, [%[s]]!\n\t" /* d0=gray, d1=alpha */
            "vmov d3, d1\n\t"
            "vmov d2, d0\n\t"
            "vmov d1, d0\n\t"
            "vld2.8 {d4, d5}, [%[s]]!\n\t"
            "vmov d7, d5\n\t"
            "vmov d6, d4\n\t"
            "vmov d5, d4\n\t"
            "vst4.8 {d0-d3}, [%[d]]!\n\t"
            "vst4.8 {d4-d7}, [%[d]]!\n\t"
            "subs %[n], %[n], #16\n\t"
            "bgt 1b\n\t"
            : [s] "+r"(s), [d] "+r"(d), [n] "+r"(neon_count)
            :
            : "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "memory", "cc");
    }
    for (int i = (count & ~15); i < count; i++) {
        uint32_t g = src[i * 2];
        uint32_t a = src[i * 2 + 1];
        dst[i] = (a << 24) | (g << 16) | (g << 8) | g;
    }
#else
    for (int i = 0; i < count; i++) {
        uint32_t g = src[i * 2];
        uint32_t a = src[i * 2 + 1];
        dst[i] = (a << 24) | (g << 16) | (g << 8) | g;
    }
#endif
}

/**
 * Rotate a 32bpp pixel buffer 180° in-place (reverse the pixel array).
 * Caller must ensure the buffer is contiguous (pitch == width * 4).
 */
static inline void neon_rotate180_inplace(uint32_t *pixels, int count)
{
    uint32_t *lo = pixels;
    uint32_t *hi = pixels + count - 1;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    while (hi - lo >= 15) {
        uint32_t *hip = hi - 7;
        __asm__ volatile(
            "pld        [%[lo], #64]        \n"
            "pld        [%[hip], #-64]      \n"
            "vld1.32    {q0, q1}, [%[lo]]   \n"
            "vld1.32    {q2, q3}, [%[hip]]  \n"
            "vrev64.32  q4, q1              \n"
            "vrev64.32  q5, q0              \n"
            "vswp       d8, d9              \n"
            "vswp       d10, d11            \n"
            "vrev64.32  q6, q3              \n"
            "vrev64.32  q7, q2              \n"
            "vswp       d12, d13            \n"
            "vswp       d14, d15            \n"
            "vst1.32    {q6, q7}, [%[lo]]!  \n"
            "vst1.32    {q4, q5}, [%[hip]]  \n"
            : [lo] "+r"(lo), [hip] "+r"(hip)
            :
            : "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "memory");
        hi -= 8;
    }
#endif
    while (lo < hi) {
        uint32_t tmp = *lo;
        *lo++ = *hi;
        *hi-- = tmp;
    }
}

#endif /* NEON_PIXEL_H__ */
