#ifndef NEON_PIXEL_H__
#define NEON_PIXEL_H__

/**
 * ARM NEON-accelerated pixel format conversion utilities.
 *
 * Uses portable NEON intrinsics (arm_neon.h) that compile on both
 * ARMv7 (Cortex-A7, Miyoo Mini) and AArch64 (Cortex-A55, Miyoo Flip).
 * Processes 16 pixels per iteration with scalar fallback for tails
 * and non-NEON builds.
 */

#include <stdint.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

/**
 * Swap Red and Blue channels in-place for an array of ARGB8888 pixels.
 * Memory layout: [B,G,R,A] → [R,G,B,A] (or reverse)
 *
 * @param pixels  Pointer to pixel buffer (modified in-place)
 * @param count   Number of pixels to process
 */
static inline void neon_swap_rb_inplace(uint32_t *pixels, int count)
{
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    int i = 0;
    uint8_t *p8 = (uint8_t *)pixels;
    for (; i + 16 <= count; i += 16, p8 += 64) {
        uint8x16x4_t px = vld4q_u8(p8);
        uint8x16_t tmp = px.val[0]; /* B */
        px.val[0] = px.val[2];      /* B ← R */
        px.val[2] = tmp;             /* R ← B */
        vst4q_u8(p8, px);
    }
    /* scalar tail */
    for (; i < count; i++) {
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
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    int i = 0;
    const uint8_t *s8 = (const uint8_t *)src;
    uint8_t *d8 = (uint8_t *)dst;
    for (; i + 16 <= count; i += 16, s8 += 64, d8 += 64) {
        uint8x16x4_t px = vld4q_u8(s8);
        uint8x16_t tmp = px.val[0]; /* B */
        px.val[0] = px.val[2];      /* B ← R */
        px.val[2] = tmp;             /* R ← B */
        vst4q_u8(d8, px);
    }
    /* scalar tail */
    for (; i < count; i++) {
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
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    int i = 0;
    const uint8_t *s8 = (const uint8_t *)src;
    uint8_t *d8 = (uint8_t *)dst;
    uint8x8_t zero8 = vdup_n_u8(0);
    for (; i + 8 <= count; i += 8, s8 += 32, d8 += 32) {
        uint8x8x4_t px = vld4_u8(s8);       /* d0=B, d1=G, d2=R, d3=A */
        uint8x8_t mask = vceq_u8(px.val[3], zero8); /* 0xFF where A==0 */
        uint8x8_t tmp = px.val[0];
        px.val[0] = px.val[2];               /* B ← R */
        px.val[2] = tmp;                      /* R ← B */
        px.val[0] = vbic_u8(px.val[0], mask); /* zero R where A==0 */
        px.val[1] = vbic_u8(px.val[1], mask); /* zero G where A==0 */
        px.val[2] = vbic_u8(px.val[2], mask); /* zero B where A==0 */
        vst4_u8(d8, px);
    }
    /* scalar tail */
    for (; i < count; i++) {
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
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    int i = 0;
    const uint8_t *s8 = src;
    uint8_t *d8 = (uint8_t *)dst;
    uint8x16_t alpha = vdupq_n_u8(0xFF);
    for (; i + 16 <= count; i += 16, s8 += 48, d8 += 64) {
        uint8x16x3_t rgb = vld3q_u8(s8);    /* val[0]=R, val[1]=G, val[2]=B */
        uint8x16x4_t out;
        out.val[0] = rgb.val[2];             /* B for ARGB little-endian */
        out.val[1] = rgb.val[1];             /* G */
        out.val[2] = rgb.val[0];             /* R */
        out.val[3] = alpha;                  /* A = 0xFF */
        vst4q_u8(d8, out);
    }
    /* scalar tail */
    for (; i < count; i++) {
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
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    int i = 0;
    const uint8_t *s8 = src;
    uint8_t *d8 = (uint8_t *)dst;
    uint8x16_t alpha = vdupq_n_u8(0xFF);
    for (; i + 16 <= count; i += 16, s8 += 16, d8 += 64) {
        uint8x16_t gray = vld1q_u8(s8);
        uint8x16x4_t out;
        out.val[0] = gray;   /* B = gray */
        out.val[1] = gray;   /* G = gray */
        out.val[2] = gray;   /* R = gray */
        out.val[3] = alpha;  /* A = 0xFF */
        vst4q_u8(d8, out);
    }
    /* scalar tail */
    for (; i < count; i++) {
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
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    int i = 0;
    const uint8_t *s8 = src;
    uint8_t *d8 = (uint8_t *)dst;
    for (; i + 16 <= count; i += 16, s8 += 32, d8 += 64) {
        uint8x16x2_t ga = vld2q_u8(s8);  /* val[0]=gray, val[1]=alpha */
        uint8x16x4_t out;
        out.val[0] = ga.val[0];  /* B = gray */
        out.val[1] = ga.val[0];  /* G = gray */
        out.val[2] = ga.val[0];  /* R = gray */
        out.val[3] = ga.val[1];  /* A = alpha */
        vst4q_u8(d8, out);
    }
    /* scalar tail */
    for (; i < count; i++) {
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
 * Processes 4 pixels (16 bytes) per iteration from both ends using
 * portable intrinsics that work on ARMv7 and AArch64.
 *
 * @param pixels  Pointer to ARGB8888 pixel buffer (modified in-place)
 * @param count   Total number of pixels
 */
static inline void neon_rotate180_inplace(uint32_t *pixels, int count)
{
    uint32_t *lo = pixels;
    uint32_t *hi = pixels + count - 1;
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    /* Process 4 pixels from each end per iteration */
    while (hi - lo >= 7) {
        /* Load 4 pixels from lo end */
        uint32x4_t vlo = vld1q_u32(lo);
        /* Load 4 pixels ending at hi (hi-3 .. hi) */
        uint32x4_t vhi = vld1q_u32(hi - 3);
        /* Reverse each: [0,1,2,3] → [3,2,1,0] */
        uint32x4_t vlo_rev = vrev64q_u32(vlo);
        vlo_rev = vcombine_u32(vget_high_u32(vlo_rev), vget_low_u32(vlo_rev));
        uint32x4_t vhi_rev = vrev64q_u32(vhi);
        vhi_rev = vcombine_u32(vget_high_u32(vhi_rev), vget_low_u32(vhi_rev));
        /* Store reversed hi at lo, reversed lo at hi */
        vst1q_u32(lo, vhi_rev);
        vst1q_u32(hi - 3, vlo_rev);
        lo += 4;
        hi -= 4;
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
