#include "font_drawing.h"
#include "font_menudata.h"
#include <SDL/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Include NEON SIMD utilities for optimized glyph rendering.
 * The Makefile in src/clock includes -I../common via config.mk,
 * and the compilation runs from src/clock directory. */
#include "utils/neon_simd.h"

extern SDL_Surface *sdl_screen;
#define HOST_WIDTH_RESOLUTION sdl_screen->w
#define HOST_HEIGHT_RESOLUTION sdl_screen->h

/**
 * @brief Draw a single character using NEON-optimized glyph rendering
 *
 * This function renders an 8x8 monochrome font glyph with outline effect.
 * Uses NEON SIMD for parallel pixel processing when available.
 *
 * @param buffer Destination pixel buffer (16-bit RGB565)
 * @param x Current x position (updated after drawing)
 * @param y Current y position (updated on newline)
 * @param margin Left margin for newline
 * @param ch Character to draw
 * @param fc Foreground color (16-bit RGB565)
 * @param olc Outline color (16-bit RGB565)
 */
static void drawChar(uint16_t *restrict buffer, int32_t *x, int32_t *y,
                     int32_t margin, char ch, uint16_t fc, uint16_t olc)
{
    if (ch == '\n') {
        *x = margin;
        *y += 8;
    }
    else if (*y < HOST_HEIGHT_RESOLUTION - 1) {
        const uint8_t *charSprite = (const uint8_t *)ch * 8 + n2DLib_font;

        /* Use NEON-optimized glyph rendering for the 8x8 character */
        uint16_t *dst = buffer + *x + (*y * HOST_WIDTH_RESOLUTION);
        neon_render_glyph_8x8(dst, charSprite, (uint32_t)HOST_WIDTH_RESOLUTION, fc, olc);

        *x += 8;
    }
}

static void drawString(uint16_t *restrict buffer, int32_t *x, int32_t *y,
                       int32_t _x, const char *str, uint16_t fc, uint16_t olc)
{
    unsigned long i, max = strlen(str) + 1;
    for (i = 0; i < max; i++)
        drawChar(buffer, x, y, _x, str[i], fc, olc);
}

void print_string(const char *s, const uint16_t fg_color,
                  const uint16_t bg_color, int32_t x, int32_t y,
                  uint16_t *restrict buffer)
{
    drawString(buffer, &x, &y, 0, s, fg_color, bg_color);
}
