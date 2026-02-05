/**
 * @file neon_asm.h
 * @brief Declarations for ARM NEON assembly-optimized functions
 *
 * This header provides declarations for the pure assembly implementations
 * of NEON-optimized pixel operations. These functions are implemented in
 * neon_asm.S and provide maximum performance on ARM Cortex-A7.
 *
 * When USE_NEON_ASM is defined and the platform is ARM, these functions
 * will be used instead of the C intrinsic versions.
 *
 * Performance comparison (Cortex-A7):
 *
 * | Function                  | Intrinsics | Assembly | Improvement |
 * |---------------------------|------------|----------|-------------|
 * | rgb888_to_argb8888        | 7-8 cy/px  | 5-6 cy/px| ~20-25%     |
 * | gray_to_argb8888          | 2.5 cy/px  | 1.5 cy/px| ~40%        |
 * | swap_rb                   | 2.0 cy/px  | 1.5 cy/px| ~25%        |
 * | alpha_blend               | 5-6 cy/px  | 3-4 cy/px| ~35%        |
 * | memcpy (large)            | 0.8 cy/B   | 0.5 cy/B | ~35%        |
 * | fill32                    | 0.4 cy/W   | 0.25 cy/W| ~35%        |
 *
 * The improvements come from:
 * - Optimal register allocation
 * - Precise prefetch placement
 * - Better instruction scheduling for dual-issue
 * - Elimination of compiler overhead
 *
 * Build requirements:
 *   Define USE_NEON_ASM=1 in CFLAGS/CXXFLAGS
 *   Link with neon_asm.o (assembled from neon_asm.S)
 */

#ifndef UTILS_NEON_ASM_H__
#define UTILS_NEON_ASM_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Check if assembly implementations are available
 * Only available on ARM platforms with NEON
 */
#if defined(__ARM_NEON) || defined(__arm__)
#define NEON_ASM_AVAILABLE 1
#else
#define NEON_ASM_AVAILABLE 0
#endif

#if NEON_ASM_AVAILABLE

/**
 * @brief Convert RGB888 pixels to ARGB8888 (Assembly version)
 *
 * @param dst Destination buffer (4 bytes per pixel)
 * @param src Source buffer (3 bytes per pixel, RGB order)
 * @param width Number of pixels to convert
 *
 * Performance: ~5-6 cycles/pixel on Cortex-A7
 */
extern void neon_asm_rgb888_to_argb8888(uint32_t *dst, const uint8_t *src,
                                        uint32_t width);

/**
 * @brief Convert grayscale pixels to ARGB8888 (Assembly version)
 *
 * @param dst Destination buffer (4 bytes per pixel)
 * @param src Source buffer (1 byte per pixel, grayscale)
 * @param width Number of pixels to convert
 *
 * Performance: ~1.5 cycles/pixel on Cortex-A7
 */
extern void neon_asm_gray_to_argb8888(uint32_t *dst, const uint8_t *src,
                                      uint32_t width);

/**
 * @brief Swap red and blue channels (Assembly version)
 *
 * Converts RGBA↔BGRA or ARGB↔ABGR by swapping R and B.
 *
 * @param dst Destination buffer
 * @param src Source buffer
 * @param count Number of pixels
 *
 * Performance: ~1.5 cycles/pixel on Cortex-A7
 */
extern void neon_asm_swap_rb(uint32_t *dst, const uint32_t *src,
                             uint32_t count);

/**
 * @brief Alpha blend source over destination (Assembly version)
 *
 * Performs src-over alpha compositing: result = src * alpha + dst * (1-alpha)
 *
 * @param dst Destination buffer (also receives result)
 * @param src Source buffer (foreground)
 * @param count Number of pixels
 *
 * Performance: ~3-4 cycles/pixel on Cortex-A7
 */
extern void neon_asm_alpha_blend(uint32_t *dst, const uint32_t *src,
                                 uint32_t count);

/**
 * @brief Optimized memory copy (Assembly version)
 *
 * Uses NEON with aggressive prefetching for maximum throughput.
 *
 * @param dst Destination buffer
 * @param src Source buffer
 * @param bytes Number of bytes to copy
 *
 * Performance: ~0.5 cycles/byte on Cortex-A7 for large aligned buffers
 */
extern void neon_asm_memcpy(void *dst, const void *src, size_t bytes);

/**
 * @brief Fill memory with 32-bit value (Assembly version)
 *
 * @param dst Destination buffer (should be 4-byte aligned)
 * @param value 32-bit value to fill
 * @param count Number of 32-bit words to fill
 *
 * Performance: ~0.25 cycles/word on Cortex-A7
 */
extern void neon_asm_fill32(uint32_t *dst, uint32_t value, uint32_t count);

/**
 * @brief Blit a single row of pixels (Assembly version)
 *
 * Optimized for texture atlas row-by-row blitting.
 *
 * @param dst Destination buffer
 * @param src Source buffer
 * @param pixels Number of pixels to copy
 *
 * Performance: ~0.3 cycles/pixel on Cortex-A7 for 32+ pixels
 */
extern void neon_asm_blit_row(uint32_t *dst, const uint32_t *src, uint32_t pixels);

/**
 * @brief Blit a rectangular region with strides (Assembly version)
 *
 * Core operation for texture atlas blitting with different strides.
 *
 * @param dst Destination buffer
 * @param dst_stride Destination stride in pixels
 * @param src Source buffer
 * @param src_stride Source stride in pixels
 * @param width Region width in pixels
 * @param height Region height in pixels
 *
 * Performance: ~0.35 cycles/pixel on Cortex-A7 for 32x32+ regions
 */
extern void neon_asm_blit_rect(uint32_t *dst, uint32_t dst_stride,
                               const uint32_t *src, uint32_t src_stride,
                               uint32_t width, uint32_t height);

/**
 * @brief Render a glyph row with outline detection (Assembly version)
 *
 * Renders a single 8-pixel row of an 8x8 monochrome font with outline.
 *
 * @param dst Destination buffer (16-bit RGB565)
 * @param glyph_row 8-bit glyph row (MSB = leftmost pixel)
 * @param glyph_row_above Glyph row above (for outline detection)
 * @param glyph_row_below Glyph row below (for outline detection)
 * @param fg_color Foreground color (16-bit RGB565)
 * @param outline_color Outline color (16-bit RGB565)
 *
 * Performance: ~15 cycles/row on Cortex-A7 (vs ~25 for intrinsics)
 */
extern void neon_asm_render_glyph_row(uint16_t *dst, uint8_t glyph_row,
                                      uint8_t glyph_row_above, uint8_t glyph_row_below,
                                      uint16_t fg_color, uint16_t outline_color);

/**
 * @brief Premultiply alpha for ARGB8888 pixels (Assembly version)
 *
 * Converts ARGB8888 to premultiplied alpha: R' = R*A/255, etc.
 *
 * @param dst Destination buffer
 * @param src Source buffer
 * @param count Number of pixels
 *
 * Performance: ~2 cycles/pixel on Cortex-A7
 */
extern void neon_asm_premultiply_alpha(uint32_t *dst, const uint32_t *src,
                                       uint32_t count);

/**
 * @brief Bilinear interpolation for 4 pixels (Assembly version)
 *
 * Performs bilinear interpolation for 4 destination pixels.
 *
 * The bilinear interpolation formula for each channel:
 *   t1 = c00 * (1-ex) + c01 * ex   (top row interpolation)
 *   t2 = c10 * (1-ex) + c11 * ex   (bottom row interpolation)
 *   result = t1 * (1-ey) + t2 * ey (vertical interpolation)
 *
 * @param dst Destination buffer (4 x 32-bit RGBA pixels)
 * @param c00_base Top row source pointer (already offset to batch start)
 * @param c10_base Bottom row source pointer (c00_base + src_pitch)
 * @param csax Array of 4 x int, where each csax[i] = (step << 16) | ex:
 *             - Bits 0-15: ex weight (0-65535, assembly converts to 8-bit by >> 8)
 *             - Bits 16-31: incremental step to reach this pixel from previous
 *             - csax[0]: ex for pixel 0 (step in upper bits is not used)
 *             - csax[1]: ex for pixel 1, (csax[1]>>16) = step from pixel 0 to 1
 *             - csax[2]: ex for pixel 2, (csax[2]>>16) = step from pixel 1 to 2
 *             - csax[3]: ex for pixel 3, (csax[3]>>16) = step from pixel 2 to 3
 * @param ey Y interpolation weight (0-65535, assembly converts to 8-bit by >> 8)
 *
 * Performance: ~10-12 cycles/pixel on Cortex-A7 (vs ~15 for intrinsics)
 */
extern void neon_asm_bilinear_interp_4px(uint32_t *dst, const uint32_t *c00_base,
                                         const uint32_t *c10_base, const int *csax,
                                         int ey);

/**
 * @brief Convert Grayscale+Alpha to ARGB8888 (Assembly version)
 *
 * Converts grayscale+alpha (2 bytes per pixel) to ARGB8888 (4 bytes per pixel).
 * The grayscale value is replicated to R, G, B channels.
 *
 * @param dst Destination buffer (4 bytes per pixel)
 * @param src Source buffer (2 bytes per pixel: gray, alpha)
 * @param width Number of pixels to convert
 *
 * Performance: ~2 cycles/pixel on Cortex-A7 (vs ~3 for intrinsics)
 */
extern void neon_asm_graya_to_argb8888(uint32_t *dst, const uint8_t *src,
                                        uint32_t width);

/**
 * @brief Convert RGBA8888 to ARGB8888 (Assembly version)
 *
 * Reorders RGBA pixel data to ARGB format by swapping R and B channels.
 *
 * @param dst Destination buffer (ARGB8888)
 * @param src Source buffer (RGBA8888)
 * @param count Number of pixels to convert
 *
 * Performance: ~1.5 cycles/pixel on Cortex-A7 (vs ~2 for intrinsics)
 */
extern void neon_asm_rgba_to_argb(uint32_t *dst, const uint32_t *src,
                                   uint32_t count);

/**
 * @brief Render 8x8 glyph with outline (Assembly version)
 *
 * Renders a complete 8x8 monochrome glyph with outline effect.
 * All 8 rows are processed efficiently with loop unrolling.
 *
 * @param dst Destination buffer (16-bit pixels)
 * @param glyph Pointer to 8 bytes of glyph data (8 rows, MSB = leftmost pixel)
 * @param stride Destination buffer stride in pixels
 * @param fg_color Foreground color (16-bit RGB565)
 * @param outline_color Outline color (16-bit RGB565)
 *
 * Performance: ~12 cycles/row (~96 total vs ~200 for intrinsics)
 */
extern void neon_asm_render_glyph_8x8(uint16_t *dst, const uint8_t *glyph,
                                       uint32_t stride, uint16_t fg_color,
                                       uint16_t outline_color);

#endif /* NEON_ASM_AVAILABLE */

/**
 * ============================================================================
 * Convenience macros for switching between implementations
 * ============================================================================
 *
 * When USE_NEON_ASM is defined and NEON_ASM_AVAILABLE is true, these macros
 * will use the assembly implementations. Otherwise, they use the intrinsics
 * from neon_simd.h.
 *
 * Usage:
 *   #define USE_NEON_ASM 1
 *   #include "utils/neon_asm.h"
 *   // Now NEON_RGB888_TO_ARGB8888() uses assembly on ARM
 */

#if defined(USE_NEON_ASM) && NEON_ASM_AVAILABLE

#define NEON_RGB888_TO_ARGB8888(dst, src, width) \
    neon_asm_rgb888_to_argb8888((dst), (src), (width))

#define NEON_GRAY_TO_ARGB8888(dst, src, width) \
    neon_asm_gray_to_argb8888((dst), (src), (width))

#define NEON_GRAYA_TO_ARGB8888(dst, src, width) \
    neon_asm_graya_to_argb8888((dst), (src), (width))

#define NEON_SWAP_RB(dst, src, count) \
    neon_asm_swap_rb((dst), (src), (count))

#define NEON_RGBA_TO_ARGB(dst, src, count) \
    neon_asm_rgba_to_argb((dst), (src), (count))

#define NEON_ALPHA_BLEND(dst, src, count) \
    neon_asm_alpha_blend((dst), (src), (count))

#define NEON_MEMCPY(dst, src, bytes) \
    neon_asm_memcpy((dst), (src), (bytes))

#define NEON_FILL32(dst, value, count) \
    neon_asm_fill32((dst), (value), (count))

#define NEON_BLIT_ROW(dst, src, pixels) \
    neon_asm_blit_row((dst), (src), (pixels))

#define NEON_BLIT_RECT(dst, dst_stride, src, src_stride, width, height) \
    neon_asm_blit_rect((dst), (dst_stride), (src), (src_stride), (width), (height))

#define NEON_RENDER_GLYPH_ROW(dst, row, above, below, fg, outline) \
    neon_asm_render_glyph_row((dst), (row), (above), (below), (fg), (outline))

#define NEON_RENDER_GLYPH_8X8(dst, glyph, stride, fg, outline) \
    neon_asm_render_glyph_8x8((dst), (glyph), (stride), (fg), (outline))

#define NEON_PREMULTIPLY_ALPHA(dst, src, count) \
    neon_asm_premultiply_alpha((dst), (src), (count))

#define NEON_BILINEAR_INTERP_4PX(dst, c00_base, c10_base, csax, ey) \
    neon_asm_bilinear_interp_4px((dst), (c00_base), (c10_base), (csax), (ey))

#define NEON_BILINEAR_INTERP_4PX_AVAILABLE 1

#else /* Use intrinsics fallback */

#include "neon_simd.h"

#define NEON_RGB888_TO_ARGB8888(dst, src, width) \
    neon_rgb888_to_argb8888((dst), (src), (width))

#define NEON_GRAY_TO_ARGB8888(dst, src, width) \
    neon_gray_to_argb8888((dst), (src), (width))

#define NEON_SWAP_RB(dst, src, count) \
    neon_swap_rb((dst), (src), (count))

#define NEON_ALPHA_BLEND(dst, src, count) \
    neon_alpha_blend((dst), (src), (count))

#define NEON_MEMCPY(dst, src, bytes) \
    neon_memcpy((dst), (src), (bytes))

#define NEON_FILL32(dst, value, count) \
    neon_fill32((dst), (value), (count))

/* Fallback for blit functions using neon_memcpy */
#define NEON_BLIT_ROW(dst, src, pixels) \
    neon_memcpy((dst), (src), (pixels) * sizeof(uint32_t))

/* Fallback: row-by-row blit using neon_memcpy */
static inline void neon_blit_rect_fallback(uint32_t *dst, uint32_t dst_stride,
                                           const uint32_t *src, uint32_t src_stride,
                                           uint32_t width, uint32_t height)
{
    for (uint32_t row = 0; row < height; row++) {
        neon_memcpy(dst, src, width * sizeof(uint32_t));
        dst += dst_stride;
        src += src_stride;
    }
}

#define NEON_BLIT_RECT(dst, dst_stride, src, src_stride, width, height) \
    neon_blit_rect_fallback((dst), (dst_stride), (src), (src_stride), (width), (height))

/* Fallback: use neon_simd.h intrinsics version for glyph rendering */
#define NEON_RENDER_GLYPH_ROW(dst, row, above, below, fg, outline) \
    neon_render_glyph_row((dst), (row), (above), (below), (fg), (outline))

/* Fallback: scalar premultiply alpha */
static inline void neon_premultiply_alpha_fallback(uint32_t *dst, const uint32_t *src,
                                                   uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        uint32_t px = src[i];
        uint32_t a = (px >> 24) & 0xFF;
        uint32_t r = ((px >> 16) & 0xFF) * a / 255;
        uint32_t g = ((px >> 8) & 0xFF) * a / 255;
        uint32_t b = (px & 0xFF) * a / 255;
        dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

#define NEON_PREMULTIPLY_ALPHA(dst, src, count) \
    neon_premultiply_alpha_fallback((dst), (src), (count))

/* Fallback: use neon_simd.h intrinsics for graya conversion */
#define NEON_GRAYA_TO_ARGB8888(dst, src, width) \
    neon_graya_to_argb8888((dst), (src), (width))

/* Fallback: use neon_simd.h intrinsics for rgba to argb */
#define NEON_RGBA_TO_ARGB(dst, src, count) \
    neon_rgba_to_argb((dst), (src), (count))

/* Fallback: use neon_simd.h intrinsics for 8x8 glyph rendering */
#define NEON_RENDER_GLYPH_8X8(dst, glyph, stride, fg, outline) \
    neon_render_glyph_8x8((dst), (glyph), (stride), (fg), (outline))

/* Bilinear interpolation fallback - uses the C intrinsics in SDL_rotozoom.c
 * This macro is defined but not used in fallback mode; the intrinsics version
 * neon_bilinear_interp_4px() in SDL_rotozoom.c is called directly instead. */
#define NEON_BILINEAR_INTERP_4PX_AVAILABLE 0

#endif /* USE_NEON_ASM && NEON_ASM_AVAILABLE */

#ifdef __cplusplus
}
#endif

#endif /* UTILS_NEON_ASM_H__ */
