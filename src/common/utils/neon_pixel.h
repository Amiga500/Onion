#ifndef NEON_PIXEL_H__
#define NEON_PIXEL_H__

/**
 * ARM NEON-accelerated pixel format conversion utilities
 * for Miyoo Mini/Mini+ (Cortex-A7, NEON VFPv4)
 *
 * All functions process 16 pixels per iteration using
 * VLD4/VST4 deinterleave instructions with PLD prefetch.
 * Scalar fallback for remaining 0-15 pixels and non-NEON builds.
 */

#include <stdint.h>

/**
 * Swap Red and Blue channels in-place for an array of ARGB8888 pixels.
 * Memory layout: [B,G,R,A] → [R,G,B,A] (or reverse)
 *
 * @param pixels  Pointer to pixel buffer (modified in-place)
 * @param count   Number of pixels to process
 */
static inline void neon_swap_rb_inplace(uint32_t *pixels, int count)
{
#ifdef __ARM_NEON
    int i = 0;
    int neon_count = count & ~15; /* round down to multiple of 16 */
    if (neon_count > 0) {
        __asm__ volatile(
            "mov r4, %[pixels]\n\t"
            "mov r5, %[neon_count]\n\t"
            "1:\n\t"
            "pld [r4, #64]\n\t"
            "vld4.8 {d0-d3}, [r4]!\n\t"
            "vld4.8 {d4-d7}, [r4]\n\t"
            "sub r4, r4, #32\n\t"
            "vswp d0, d2\n\t"
            "vswp d4, d6\n\t"
            "vst4.8 {d0-d3}, [r4]!\n\t"
            "vst4.8 {d4-d7}, [r4]!\n\t"
            "subs r5, r5, #16\n\t"
            "bgt 1b\n\t"
            : [pixels] "+r"(pixels), [neon_count] "+r"(neon_count)
            :
            : "r4", "r5", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
              "memory", "cc");
    }
    /* scalar tail */
    for (i = (count & ~15); i < count; i++) {
        uint32_t p = pixels[i];
        pixels[i] = (p & 0xFF00FF00) | ((p & 0x00FF0000) >> 16) | ((p & 0x000000FF) << 16);
    }
#else
    for (int i = 0; i < count; i++) {
        uint32_t p = pixels[i];
        pixels[i] = (p & 0xFF00FF00) | ((p & 0x00FF0000) >> 16) | ((p & 0x000000FF) << 16);
    }
#endif
}

/**
 * Convert ARGB8888 → RGBA8888 for PNG output (swap R↔B channels).
 * Same operation as neon_swap_rb_inplace but copies src→dst.
 *
 * @param dst    Destination RGBA8888 buffer
 * @param src    Source ARGB8888 buffer (not modified)
 * @param count  Number of pixels to process
 */
static inline void neon_argb_to_rgba(uint32_t *dst, const uint32_t *src, int count)
{
#ifdef __ARM_NEON
    int i = 0;
    int neon_count = count & ~15;
    const uint32_t *s = src;
    uint32_t *d = dst;
    if (neon_count > 0) {
        __asm__ volatile(
            "mov r4, %[s]\n\t"
            "mov r5, %[d]\n\t"
            "mov r6, %[neon_count]\n\t"
            "1:\n\t"
            "pld [r4, #64]\n\t"
            "vld4.8 {d0-d3}, [r4]!\n\t"
            "vld4.8 {d4-d7}, [r4]!\n\t"
            "vswp d0, d2\n\t"
            "vswp d4, d6\n\t"
            "vst4.8 {d0-d3}, [r5]!\n\t"
            "vst4.8 {d4-d7}, [r5]!\n\t"
            "subs r6, r6, #16\n\t"
            "bgt 1b\n\t"
            : [s] "+r"(s), [d] "+r"(d), [neon_count] "+r"(neon_count)
            :
            : "r4", "r5", "r6", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
              "memory", "cc");
    }
    /* scalar tail */
    for (i = (count & ~15); i < count; i++) {
        uint32_t p = src[i];
        dst[i] = (p & 0xFF00FF00) | ((p & 0x00FF0000) >> 16) | ((p & 0x000000FF) << 16);
    }
#else
    for (int i = 0; i < count; i++) {
        uint32_t p = src[i];
        dst[i] = (p & 0xFF00FF00) | ((p & 0x00FF0000) >> 16) | ((p & 0x000000FF) << 16);
    }
#endif
}

/**
 * Convert ARGB8888 → RGBA8888 with alpha-conditional zeroing for PNG.
 * Pixels with alpha=0 are output as 0x00000000 (transparent).
 *
 * @param dst    Destination RGBA8888 buffer
 * @param src    Source ARGB8888 buffer (not modified)
 * @param count  Number of pixels to process
 */
static inline void neon_argb_to_rgba_alpha(uint32_t *dst, const uint32_t *src, int count)
{
#ifdef __ARM_NEON
    int i = 0;
    int neon_count = count & ~7; /* 8 pixels at a time */
    const uint32_t *s = src;
    uint32_t *d = dst;
    if (neon_count > 0) {
        __asm__ volatile(
            "mov r4, %[s]\n\t"
            "mov r5, %[d]\n\t"
            "mov r6, %[neon_count]\n\t"
            "1:\n\t"
            "pld [r4, #64]\n\t"
            "vld4.8 {d0-d3}, [r4]!\n\t"  /* d0=B, d1=G, d2=R, d3=A (8 px) */
            "vceq.i8 d4, d3, #0\n\t"     /* d4 = mask: 0xFF where A==0 */
            "vswp d0, d2\n\t"            /* swap R↔B */
            "vbic d0, d0, d4\n\t"        /* zero R where A==0 */
            "vbic d1, d1, d4\n\t"        /* zero G where A==0 */
            "vbic d2, d2, d4\n\t"        /* zero B where A==0 */
            "vst4.8 {d0-d3}, [r5]!\n\t"
            "subs r6, r6, #8\n\t"
            "bgt 1b\n\t"
            : [s] "+r"(s), [d] "+r"(d), [neon_count] "+r"(neon_count)
            :
            : "r4", "r5", "r6", "d0", "d1", "d2", "d3", "d4",
              "memory", "cc");
    }
    /* scalar tail */
    for (i = (count & ~7); i < count; i++) {
        uint32_t p = src[i];
        dst[i] = (p & 0xFF000000)
                     ? ((p & 0xFF00FF00) | ((p & 0x00FF0000) >> 16) | ((p & 0x000000FF) << 16))
                     : 0;
    }
#else
    for (int i = 0; i < count; i++) {
        uint32_t p = src[i];
        dst[i] = (p & 0xFF000000)
                     ? ((p & 0xFF00FF00) | ((p & 0x00FF0000) >> 16) | ((p & 0x000000FF) << 16))
                     : 0;
    }
#endif
}

/**
 * Convert packed RGB888 (3 bytes/pixel) → ARGB8888 (4 bytes/pixel).
 * Sets alpha to 0xFF for all output pixels.
 *
 * @param dst    Destination ARGB8888 buffer
 * @param src    Source RGB888 buffer (3 bytes per pixel)
 * @param count  Number of pixels to process
 */
static inline void neon_rgb888_to_argb(uint32_t *dst, const uint8_t *src, int count)
{
#ifdef __ARM_NEON
    int i = 0;
    int neon_count = count & ~15;
    const uint8_t *s = src;
    uint8_t *d = (uint8_t *)dst;
    if (neon_count > 0) {
        __asm__ volatile(
            "mov r4, %[s]\n\t"
            "mov r5, %[d]\n\t"
            "mov r6, %[neon_count]\n\t"
            "vmov.i8 d3, #0xFF\n\t"      /* alpha = 0xFF */
            "vmov.i8 d7, #0xFF\n\t"
            "1:\n\t"
            "pld [r4, #64]\n\t"
            "vld3.8 {d0-d2}, [r4]!\n\t"  /* d0=R, d1=G, d2=B (8 px) */
            "vld3.8 {d4-d6}, [r4]!\n\t"  /* next 8 px */
            "vswp d0, d2\n\t"            /* B,G,R order for ARGB little-endian */
            "vswp d4, d6\n\t"
            "vst4.8 {d0-d3}, [r5]!\n\t"  /* store B,G,R,A = ARGB8888 */
            "vst4.8 {d4-d7}, [r5]!\n\t"
            "subs r6, r6, #16\n\t"
            "bgt 1b\n\t"
            : [s] "+r"(s), [d] "+r"(d), [neon_count] "+r"(neon_count)
            :
            : "r4", "r5", "r6", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
              "memory", "cc");
    }
    /* scalar tail */
    for (i = (count & ~15); i < count; i++) {
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
 * Convert 8-bit grayscale (1 byte/pixel) → ARGB8888 (4 bytes/pixel).
 * Sets alpha to 0xFF and replicates gray value to R, G, B channels.
 * Memory layout out: [B=gray, G=gray, R=gray, A=0xFF].
 * Processes 16 pixels per iteration on NEON.
 *
 * @param dst    Destination ARGB8888 buffer
 * @param src    Source grayscale buffer (1 byte per pixel)
 * @param count  Number of pixels to process
 */
static inline void neon_gray8_to_argb(uint32_t *dst, const uint8_t *src, int count)
{
#ifdef __ARM_NEON
    int i = 0;
    int neon_count = count & ~15;
    const uint8_t *s = src;
    uint8_t *d = (uint8_t *)dst;
    if (neon_count > 0) {
        __asm__ volatile(
            "mov r4, %[s]\n\t"
            "mov r5, %[d]\n\t"
            "mov r6, %[neon_count]\n\t"
            "1:\n\t"
            "pld [r4, #64]\n\t"
            "vld1.8 {d0}, [r4]!\n\t"    /* load 8 gray values */
            "vmov d1, d0\n\t"           /* d1 = gray (G channel) */
            "vmov d2, d0\n\t"           /* d2 = gray (R channel) */
            "vmov.i8 d3, #0xFF\n\t"     /* d3 = alpha = 0xFF */
            "vld1.8 {d4}, [r4]!\n\t"    /* load next 8 gray values */
            "vmov d5, d4\n\t"
            "vmov d6, d4\n\t"
            "vmov.i8 d7, #0xFF\n\t"
            "vst4.8 {d0-d3}, [r5]!\n\t" /* store [B,G,R,A] = [gray,gray,gray,0xFF] */
            "vst4.8 {d4-d7}, [r5]!\n\t"
            "subs r6, r6, #16\n\t"
            "bgt 1b\n\t"
            : [s] "+r"(s), [d] "+r"(d), [neon_count] "+r"(neon_count)
            :
            : "r4", "r5", "r6", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
              "memory", "cc");
    }
    /* scalar tail */
    for (i = (count & ~15); i < count; i++) {
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
 * Convert 8-bit grayscale+alpha (2 bytes/pixel) → ARGB8888 (4 bytes/pixel).
 * Each input pixel is [gray, alpha]; replicates gray to R, G, B channels.
 * Memory layout out: [B=gray, G=gray, R=gray, A=alpha].
 * Processes 16 pixels per iteration on NEON.
 *
 * @param dst    Destination ARGB8888 buffer
 * @param src    Source grayscale+alpha buffer (2 bytes per pixel: gray, alpha)
 * @param count  Number of pixels to process
 */
static inline void neon_gray8a_to_argb(uint32_t *dst, const uint8_t *src, int count)
{
#ifdef __ARM_NEON
    int i = 0;
    int neon_count = count & ~15;
    const uint8_t *s = src;
    uint8_t *d = (uint8_t *)dst;
    if (neon_count > 0) {
        __asm__ volatile(
            "mov r4, %[s]\n\t"
            "mov r5, %[d]\n\t"
            "mov r6, %[neon_count]\n\t"
            "1:\n\t"
            "pld [r4, #64]\n\t"
            "vld2.8 {d0, d1}, [r4]!\n\t"  /* d0=gray[8], d1=alpha[8] (16 bytes) */
            "vmov d3, d1\n\t"             /* d3 = alpha */
            "vmov d2, d0\n\t"             /* d2 = gray (R channel) */
            "vmov d1, d0\n\t"             /* d1 = gray (G channel) */
            "vld2.8 {d4, d5}, [r4]!\n\t"  /* next 8 pixels: d4=gray, d5=alpha */
            "vmov d7, d5\n\t"             /* d7 = alpha */
            "vmov d6, d4\n\t"             /* d6 = gray (R channel) */
            "vmov d5, d4\n\t"             /* d5 = gray (G channel) */
            "vst4.8 {d0-d3}, [r5]!\n\t"   /* store [B,G,R,A] = [gray,gray,gray,alpha] */
            "vst4.8 {d4-d7}, [r5]!\n\t"
            "subs r6, r6, #16\n\t"
            "bgt 1b\n\t"
            : [s] "+r"(s), [d] "+r"(d), [neon_count] "+r"(neon_count)
            :
            : "r4", "r5", "r6", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
              "memory", "cc");
    }
    /* scalar tail */
    for (i = (count & ~15); i < count; i++) {
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
 * Rotate a 32bpp pixel buffer 180 degrees in-place using NEON.
 * Reverses the pixel array so first pixel becomes last and vice versa.
 * Processes 8 pixels (32 bytes) per iteration from both ends.
 *
 * @param pixels  Pointer to ARGB8888 pixel buffer (modified in-place)
 * @param count   Total number of pixels
 */
static inline void neon_rotate180_inplace(uint32_t *pixels, int count)
{
    uint32_t *lo = pixels;
    uint32_t *hi = pixels + count - 1;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    /* Process 8 pixels (32 bytes) from each end per iteration */
    while (hi - lo >= 15) {
        asm volatile(
            "pld        [%[lo], #64]        \n"
            "pld        [%[hi], #-64]       \n"
            /* Load 8 pixels from lo end */
            "vld1.32    {q0, q1}, [%[lo]]   \n"
            /* Load 8 pixels from hi end (go back 7 pixels = 28 bytes from hi) */
            "sub        r4, %[hi], #28      \n"
            "vld1.32    {q2, q3}, [r4]      \n"
            /* Reverse lo pixels: q0=[0,1,2,3] q1=[4,5,6,7] → q4=[7,6,5,4] q5=[3,2,1,0] */
            "vrev64.32  q4, q1              \n"
            "vrev64.32  q5, q0              \n"
            "vswp       d8, d9              \n"
            "vswp       d10, d11            \n"
            /* Reverse hi pixels: q2 q3 → q6=[h,g,f,e] q7=[d,c,b,a] */
            "vrev64.32  q6, q3              \n"
            "vrev64.32  q7, q2              \n"
            "vswp       d12, d13            \n"
            "vswp       d14, d15            \n"
            /* Store reversed hi pixels at lo, reversed lo pixels at hi (ascending reg order) */
            "vst1.32    {q6, q7}, [%[lo]]!  \n"
            "vst1.32    {q4, q5}, [r4]      \n"
            "sub        %[hi], %[hi], #32   \n"
            : [lo] "+r"(lo), [hi] "+r"(hi)
            :
            : "r4", "q0", "q1", "q2", "q3", "q4", "q5", "q6", "q7", "memory", "cc");
    }
#endif
    /* Scalar tail: swap remaining pixels from both ends */
    while (lo < hi) {
        uint32_t tmp = *lo;
        *lo++ = *hi;
        *hi-- = tmp;
    }
}

#endif /* NEON_PIXEL_H__ */
