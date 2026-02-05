/**
 * @file texture_atlas.h
 * @brief Cache-optimized texture atlas for ARM NEON
 *
 * This header provides a cache-optimized texture atlas implementation designed
 * for the Cortex-A7 processor with NEON-VFPv4 support. The atlas packs multiple
 * textures into a single contiguous memory block for improved cache locality.
 *
 * Key features:
 * - Cache-line aligned memory layout (64-byte alignment for Cortex-A7)
 * - NEON-accelerated texture blitting from atlas
 * - Prefetch hints for predictable access patterns
 * - Single allocation reduces memory fragmentation
 * - Optimized for frequently-used texture groups (icons, UI elements)
 *
 * Usage:
 *   TextureAtlas *atlas = atlas_create(1024, 1024);
 *   uint32_t id = atlas_add_texture(atlas, pixels, w, h);
 *   atlas_blit(atlas, id, dst, dst_x, dst_y, dst_stride);
 *   atlas_destroy(atlas);
 *
 * Build requirements:
 *   CFLAGS: -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
 */

#ifndef UTILS_TEXTURE_ATLAS_H__
#define UTILS_TEXTURE_ATLAS_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "neon_simd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Cache line size for Cortex-A7 (typically 64 bytes) */
#define ATLAS_CACHE_LINE_SIZE 64

/** Maximum number of textures in a single atlas */
#define ATLAS_MAX_TEXTURES 128

/** Invalid texture ID marker */
#define ATLAS_INVALID_ID ((uint32_t)-1)

/**
 * @brief Texture region descriptor within an atlas
 *
 * Stores the location and dimensions of a single texture within the atlas.
 * These are packed together for cache efficiency during lookups.
 */
typedef struct TextureRegion {
    uint16_t x;         /**< X offset in atlas (pixels) */
    uint16_t y;         /**< Y offset in atlas (pixels) */
    uint16_t width;     /**< Texture width (pixels) */
    uint16_t height;    /**< Texture height (pixels) */
} TextureRegion;

/**
 * @brief Cache-optimized texture atlas structure
 *
 * The atlas uses a contiguous memory block with cache-line alignment
 * for optimal performance on ARM Cortex-A7 with NEON.
 */
typedef struct TextureAtlas {
    uint32_t *pixels;           /**< Pixel data (ARGB8888 format) */
    void *raw_alloc;            /**< Original allocation pointer (for manual alignment fallback) */
    uint32_t width;             /**< Atlas width in pixels */
    uint32_t height;            /**< Atlas height in pixels */
    uint32_t stride;            /**< Stride in pixels (may be padded for alignment) */

    TextureRegion regions[ATLAS_MAX_TEXTURES];  /**< Texture region descriptors */
    uint32_t region_count;      /**< Number of textures in atlas */

    /* Simple bin-packing state using shelf algorithm */
    uint16_t shelf_y;           /**< Current shelf Y position */
    uint16_t shelf_height;      /**< Current shelf height */
    uint16_t shelf_x;           /**< Current X position on shelf */

    bool dirty;                 /**< Whether atlas needs cache flush before GPU use */
} TextureAtlas;

/**
 * @brief Align a value up to the specified alignment
 * @param val Value to align
 * @param align Alignment (must be power of 2)
 * @return Aligned value
 */
static inline uint32_t atlas_align_up(uint32_t val, uint32_t align)
{
    return (val + align - 1) & ~(align - 1);
}

/**
 * @brief Create a new texture atlas
 *
 * Allocates a cache-line aligned pixel buffer for the atlas and initializes
 * the packing state for adding textures.
 *
 * @param width Atlas width in pixels
 * @param height Atlas height in pixels
 * @return Pointer to new atlas, or NULL on allocation failure
 */
static inline TextureAtlas *atlas_create(uint32_t width, uint32_t height)
{
    TextureAtlas *atlas = (TextureAtlas *)calloc(1, sizeof(TextureAtlas));
    if (!atlas) {
        return NULL;
    }

    /* Align stride to cache line boundary for optimal memory access */
    atlas->stride = atlas_align_up(width, ATLAS_CACHE_LINE_SIZE / sizeof(uint32_t));
    atlas->width = width;
    atlas->height = height;
    atlas->raw_alloc = NULL;  /* Initialize to NULL for non-fallback cases */

    /* Allocate cache-aligned pixel buffer */
    size_t pixel_bytes = atlas->stride * height * sizeof(uint32_t);

    /* Use aligned allocation for cache-line alignment */
#if defined(_WIN32)
    atlas->pixels = (uint32_t *)_aligned_malloc(pixel_bytes, ATLAS_CACHE_LINE_SIZE);
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
    /* POSIX-compliant aligned allocation */
    void *ptr = NULL;
    if (posix_memalign(&ptr, ATLAS_CACHE_LINE_SIZE, pixel_bytes) == 0) {
        atlas->pixels = (uint32_t *)ptr;
    } else {
        atlas->pixels = NULL;
    }
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    atlas->pixels = (uint32_t *)aligned_alloc(ATLAS_CACHE_LINE_SIZE, pixel_bytes);
#else
    /* Fallback: allocate extra space and manually align */
    void *raw = malloc(pixel_bytes + ATLAS_CACHE_LINE_SIZE);
    if (raw) {
        atlas->raw_alloc = raw;  /* Store original pointer for free() */
        uintptr_t addr = (uintptr_t)raw + ATLAS_CACHE_LINE_SIZE;
        atlas->pixels = (uint32_t *)(addr & ~((uintptr_t)ATLAS_CACHE_LINE_SIZE - 1));
    } else {
        atlas->pixels = NULL;
    }
#endif

    if (!atlas->pixels) {
        free(atlas);
        return NULL;
    }

    /* Clear to transparent black */
    memset(atlas->pixels, 0, pixel_bytes);

    /* Initialize packing state */
    atlas->region_count = 0;
    atlas->shelf_y = 0;
    atlas->shelf_height = 0;
    atlas->shelf_x = 0;
    atlas->dirty = false;

    return atlas;
}

/**
 * @brief Destroy a texture atlas and free its memory
 *
 * @param atlas Atlas to destroy
 */
static inline void atlas_destroy(TextureAtlas *atlas)
{
    if (atlas) {
#if defined(_WIN32)
        _aligned_free(atlas->pixels);
#elif defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L
        free(atlas->pixels);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        free(atlas->pixels);
#else
        /* Fallback: free the original raw allocation if used */
        if (atlas->raw_alloc) {
            free(atlas->raw_alloc);
        } else {
            free(atlas->pixels);
        }
#endif
        free(atlas);
    }
}

/**
 * @brief Add a texture to the atlas using shelf bin-packing
 *
 * Uses a simple shelf algorithm for packing: textures are placed on horizontal
 * shelves. When a texture doesn't fit on the current shelf, a new shelf is started.
 *
 * @param atlas Target atlas
 * @param pixels Source pixel data (ARGB8888)
 * @param width Texture width
 * @param height Texture height
 * @return Texture ID for future reference, or ATLAS_INVALID_ID if atlas is full
 */
static inline uint32_t atlas_add_texture(TextureAtlas *atlas,
                                          const uint32_t *pixels,
                                          uint16_t width, uint16_t height)
{
    if (!atlas || !pixels || atlas->region_count >= ATLAS_MAX_TEXTURES) {
        return ATLAS_INVALID_ID;
    }

    /* Pad dimensions to improve cache alignment */
    uint16_t padded_width = (uint16_t)atlas_align_up(width, 4);

    /* Check if texture fits on current shelf */
    if (atlas->shelf_x + padded_width > atlas->width) {
        /* Start new shelf */
        atlas->shelf_y += atlas->shelf_height;
        atlas->shelf_height = 0;
        atlas->shelf_x = 0;
    }

    /* Check if texture fits in atlas */
    if (atlas->shelf_y + height > atlas->height) {
        return ATLAS_INVALID_ID;
    }

    /* Calculate position */
    uint16_t x = atlas->shelf_x;
    uint16_t y = atlas->shelf_y;

    /* Copy texture data to atlas using NEON-accelerated memcpy */
    uint32_t *dst_row = atlas->pixels + y * atlas->stride + x;
    const uint32_t *src_row = pixels;

    neon_prefetch(src_row);

    for (uint16_t row = 0; row < height; row++) {
        neon_prefetch(src_row + width);
        neon_memcpy(dst_row, src_row, width * sizeof(uint32_t));
        dst_row += atlas->stride;
        src_row += width;
    }

    /* Update shelf state */
    atlas->shelf_x += padded_width;
    if (height > atlas->shelf_height) {
        atlas->shelf_height = height;
    }

    /* Store region info */
    uint32_t id = atlas->region_count;
    atlas->regions[id].x = x;
    atlas->regions[id].y = y;
    atlas->regions[id].width = width;
    atlas->regions[id].height = height;
    atlas->region_count++;

    atlas->dirty = true;

    return id;
}

/**
 * @brief Get a texture region from the atlas
 *
 * @param atlas Source atlas
 * @param id Texture ID (returned from atlas_add_texture)
 * @return Pointer to region info, or NULL if invalid ID
 */
static inline const TextureRegion *atlas_get_region(const TextureAtlas *atlas, uint32_t id)
{
    if (!atlas || id >= atlas->region_count) {
        return NULL;
    }
    return &atlas->regions[id];
}

/**
 * @brief Blit a texture from the atlas to a destination buffer
 *
 * Uses NEON-accelerated memory copy with prefetch hints for optimal performance.
 * Prefetches the next row while copying the current row.
 *
 * @param atlas Source atlas
 * @param id Texture ID
 * @param dst Destination buffer (ARGB8888)
 * @param dst_x Destination X position
 * @param dst_y Destination Y position
 * @param dst_stride Destination stride in pixels
 */
static inline void atlas_blit(const TextureAtlas *atlas, uint32_t id,
                               uint32_t *dst, uint32_t dst_x, uint32_t dst_y,
                               uint32_t dst_stride)
{
    const TextureRegion *region = atlas_get_region(atlas, id);
    if (!region || !dst) {
        return;
    }

    const uint32_t *src_row = atlas->pixels + region->y * atlas->stride + region->x;
    uint32_t *dst_row = dst + dst_y * dst_stride + dst_x;
    uint32_t row_bytes = region->width * sizeof(uint32_t);

    /* Prefetch first rows */
    neon_prefetch(src_row);
    neon_prefetch(src_row + atlas->stride);
    neon_prefetch_write(dst_row);

    for (uint16_t row = 0; row < region->height; row++) {
        /* Prefetch next source and destination rows */
        if (row + 2 < region->height) {
            neon_prefetch(src_row + 2 * atlas->stride);
        }
        neon_prefetch_write(dst_row + dst_stride);

        neon_memcpy(dst_row, src_row, row_bytes);

        src_row += atlas->stride;
        dst_row += dst_stride;
    }
}

/**
 * @brief Blit a texture from the atlas with alpha blending
 *
 * Uses NEON-accelerated alpha blending for compositing textures onto
 * an existing background.
 *
 * @param atlas Source atlas
 * @param id Texture ID
 * @param dst Destination buffer (ARGB8888, read-modify-write)
 * @param dst_x Destination X position
 * @param dst_y Destination Y position
 * @param dst_stride Destination stride in pixels
 */
static inline void atlas_blit_blend(const TextureAtlas *atlas, uint32_t id,
                                     uint32_t *dst, uint32_t dst_x, uint32_t dst_y,
                                     uint32_t dst_stride)
{
    const TextureRegion *region = atlas_get_region(atlas, id);
    if (!region || !dst) {
        return;
    }

    const uint32_t *src_row = atlas->pixels + region->y * atlas->stride + region->x;
    uint32_t *dst_row = dst + dst_y * dst_stride + dst_x;

    /* Prefetch first rows */
    neon_prefetch(src_row);
    neon_prefetch(dst_row);

    for (uint16_t row = 0; row < region->height; row++) {
        /* Prefetch next rows */
        if (row + 2 < region->height) {
            neon_prefetch(src_row + 2 * atlas->stride);
            neon_prefetch(dst_row + 2 * dst_stride);
        }

        neon_alpha_blend(dst_row, src_row, region->width);

        src_row += atlas->stride;
        dst_row += dst_stride;
    }
}

/**
 * @brief Prefetch multiple texture regions for upcoming operations
 *
 * Hints the CPU to load texture data into cache before it's needed.
 * Call this when you know which textures will be rendered next frame.
 *
 * @param atlas Source atlas
 * @param ids Array of texture IDs to prefetch
 * @param count Number of IDs in array
 */
static inline void atlas_prefetch_regions(const TextureAtlas *atlas,
                                           const uint32_t *ids, uint32_t count)
{
    if (!atlas || !ids) {
        return;
    }

#if NEON_AVAILABLE
    for (uint32_t i = 0; i < count; i++) {
        if (ids[i] < atlas->region_count) {
            const TextureRegion *region = &atlas->regions[ids[i]];
            const uint32_t *row = atlas->pixels + region->y * atlas->stride + region->x;

            /* Prefetch first few rows of each texture */
            neon_prefetch(row);
            if (region->height > 1) {
                neon_prefetch(row + atlas->stride);
            }
            if (region->height > 2) {
                neon_prefetch(row + 2 * atlas->stride);
            }
        }
    }
#else
    /* No-op on non-NEON platforms */
    (void)count;
#endif
}

/**
 * @brief Get direct pixel pointer to a texture region
 *
 * Returns a pointer to the first pixel of the texture within the atlas.
 * Useful for advanced operations or when passing to other render systems.
 *
 * @param atlas Source atlas
 * @param id Texture ID
 * @param out_stride Output: atlas stride in pixels (for row advancement)
 * @return Pointer to texture pixels, or NULL if invalid ID
 */
static inline const uint32_t *atlas_get_pixels(const TextureAtlas *atlas,
                                                uint32_t id, uint32_t *out_stride)
{
    const TextureRegion *region = atlas_get_region(atlas, id);
    if (!region) {
        if (out_stride) *out_stride = 0;
        return NULL;
    }

    if (out_stride) {
        *out_stride = atlas->stride;
    }

    return atlas->pixels + region->y * atlas->stride + region->x;
}

/**
 * @brief Clear the atlas, removing all textures
 *
 * Resets the atlas to an empty state, allowing it to be reused.
 * The pixel buffer is cleared to transparent black.
 *
 * @param atlas Atlas to clear
 */
static inline void atlas_clear(TextureAtlas *atlas)
{
    if (!atlas) {
        return;
    }

    /* Clear pixel data using NEON fill */
    neon_fill32(atlas->pixels, 0, atlas->stride * atlas->height);

    /* Reset packing state */
    atlas->region_count = 0;
    atlas->shelf_y = 0;
    atlas->shelf_height = 0;
    atlas->shelf_x = 0;
    atlas->dirty = true;
}

/**
 * @brief Check if atlas has room for a texture of given dimensions
 *
 * Uses the shelf algorithm to determine if the texture would fit.
 *
 * @param atlas Atlas to check
 * @param width Texture width
 * @param height Texture height
 * @return true if texture would fit, false otherwise
 */
static inline bool atlas_can_fit(const TextureAtlas *atlas,
                                  uint16_t width, uint16_t height)
{
    if (!atlas || atlas->region_count >= ATLAS_MAX_TEXTURES) {
        return false;
    }

    uint16_t padded_width = (uint16_t)atlas_align_up(width, 4);

    /* Check if it fits on current shelf */
    if (atlas->shelf_x + padded_width <= atlas->width) {
        uint16_t max_shelf_height = height > atlas->shelf_height ? height : atlas->shelf_height;
        if (atlas->shelf_y + max_shelf_height <= atlas->height) {
            return true;
        }
    }

    /* Check if it fits on a new shelf */
    uint16_t new_shelf_y = atlas->shelf_y + atlas->shelf_height;
    if (new_shelf_y + height <= atlas->height && padded_width <= atlas->width) {
        return true;
    }

    return false;
}

/**
 * @brief Get atlas utilization statistics
 *
 * @param atlas Atlas to analyze
 * @param out_used_pixels Output: total pixels used by textures
 * @param out_total_pixels Output: total atlas capacity in pixels
 */
static inline void atlas_get_stats(const TextureAtlas *atlas,
                                    uint32_t *out_used_pixels, uint32_t *out_total_pixels)
{
    if (!atlas) {
        if (out_used_pixels) *out_used_pixels = 0;
        if (out_total_pixels) *out_total_pixels = 0;
        return;
    }

    uint32_t used = 0;
    for (uint32_t i = 0; i < atlas->region_count; i++) {
        used += atlas->regions[i].width * atlas->regions[i].height;
    }

    if (out_used_pixels) *out_used_pixels = used;
    if (out_total_pixels) *out_total_pixels = atlas->width * atlas->height;
}

/**
 * @brief Batch blit multiple textures from atlas
 *
 * Optimized for rendering multiple textures in a single call.
 * Uses prefetch hints to load next texture while blitting current one.
 *
 * @param atlas Source atlas
 * @param ids Array of texture IDs
 * @param dst_positions Array of destination positions (x0, y0, x1, y1, ...)
 * @param count Number of textures to blit
 * @param dst Destination buffer
 * @param dst_stride Destination stride in pixels
 */
static inline void atlas_blit_batch(const TextureAtlas *atlas,
                                     const uint32_t *ids,
                                     const uint16_t *dst_positions,
                                     uint32_t count,
                                     uint32_t *dst, uint32_t dst_stride)
{
    if (!atlas || !ids || !dst_positions || !dst || count == 0) {
        return;
    }

#if NEON_AVAILABLE
    /* Prefetch first texture's data */
    if (ids[0] < atlas->region_count) {
        const TextureRegion *r = &atlas->regions[ids[0]];
        const uint32_t *px = atlas->pixels + r->y * atlas->stride + r->x;
        neon_prefetch(px);
        neon_prefetch(px + atlas->stride);
    }
#endif

    for (uint32_t i = 0; i < count; i++) {
#if NEON_AVAILABLE
        /* Prefetch next texture while processing current */
        if (i + 1 < count && ids[i + 1] < atlas->region_count) {
            const TextureRegion *next = &atlas->regions[ids[i + 1]];
            const uint32_t *next_px = atlas->pixels + next->y * atlas->stride + next->x;
            neon_prefetch(next_px);
            neon_prefetch(next_px + atlas->stride);
        }
#endif

        uint16_t dx = dst_positions[i * 2];
        uint16_t dy = dst_positions[i * 2 + 1];
        atlas_blit(atlas, ids[i], dst, dx, dy, dst_stride);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UTILS_TEXTURE_ATLAS_H__ */
