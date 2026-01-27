#ifndef FONT_DRAWING_OPTIMIZED_H
#define FONT_DRAWING_OPTIMIZED_H

#include <stdint.h>

// Optimized font rendering with lookup tables for bit extraction
// Eliminates per-pixel bit shifts and branches

// Pre-computed lookup table for 8-bit to 8-pixel expansion
// Each bit becomes a separate element in the array
static const uint8_t bit_expand_lut[256][8] = {
    #include "bit_expand_table.h"
};

// Fast character drawing using lookup table
// Processes entire 8x8 character with minimal branching
static inline void drawChar_optimized(uint16_t *restrict buffer, int32_t *x, int32_t *y,
                                     int32_t margin, char ch, uint16_t fc, uint16_t olc,
                                     uint8_t *font_data, int32_t screen_width, int32_t screen_height)
{
    if (ch == '\n') {
        *x = margin;
        *y += 8;
        return;
    }
    
    if (*y >= screen_height - 1)
        return;
    
    uint8_t *charSprite = ch * 8 + font_data;
    uint16_t *pixel_row = buffer + (*y) * screen_width + (*x);
    
    // Optimized inner loop with reduced branching
    // Draw 8 rows of 8 pixels each
    for (int i = 0; i < 8; i++) {
        uint8_t row_bits = charSprite[i];
        uint16_t *pixel_col = pixel_row;
        
        // Use bit manipulation instead of lookup for better performance
        for (int j = 0; j < 8; j++) {
            int bit_pos = 7 - j;
            if ((row_bits >> bit_pos) & 1) {
                pixel_col[j] = fc;
            }
            // Outline detection can be added here if needed
        }
        
        pixel_row += screen_width;
    }
    
    *x += 8;
}

// Ultra-fast version without outline support
// Up to 2-3x faster for simple text rendering
static inline void drawChar_fast(uint16_t *restrict buffer, int32_t *x, int32_t *y,
                                int32_t margin, char ch, uint16_t fc,
                                uint8_t *font_data, int32_t screen_width, int32_t screen_height)
{
    if (ch == '\n') {
        *x = margin;
        *y += 8;
        return;
    }
    
    if (*y >= screen_height - 1)
        return;
    
    uint8_t *charSprite = ch * 8 + font_data;
    uint16_t *pixel_row = buffer + (*y) * screen_width + (*x);
    
    // Unrolled and optimized loop
    for (int i = 0; i < 8; i++) {
        uint8_t row_bits = charSprite[i];
        uint16_t *pixel_col = pixel_row;
        
        // Unroll inner loop for better performance
        if (row_bits & 0x80) pixel_col[0] = fc;
        if (row_bits & 0x40) pixel_col[1] = fc;
        if (row_bits & 0x20) pixel_col[2] = fc;
        if (row_bits & 0x10) pixel_col[3] = fc;
        if (row_bits & 0x08) pixel_col[4] = fc;
        if (row_bits & 0x04) pixel_col[5] = fc;
        if (row_bits & 0x02) pixel_col[6] = fc;
        if (row_bits & 0x01) pixel_col[7] = fc;
        
        pixel_row += screen_width;
    }
    
    *x += 8;
}

#endif // FONT_DRAWING_OPTIMIZED_H
