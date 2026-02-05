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

#include <stdint.h>
#include <stddef.h>

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

#define NEON_SWAP_RB(dst, src, count) \
    neon_asm_swap_rb((dst), (src), (count))

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

#endif /* USE_NEON_ASM && NEON_ASM_AVAILABLE */

#ifdef __cplusplus
}
#endif

#endif /* UTILS_NEON_ASM_H__ */
