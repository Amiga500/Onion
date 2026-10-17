/* Part of OnionPlus's SDL_ttf (zlib terms, see ../COPYING-SDL_ttf). */
#ifndef TTF_SCRIPT_API_H
#define TTF_SCRIPT_API_H
#include <stdint.h>

/* Interface between SDL_ttf (ttf_script.c) and libSDL_ttf-script.so, which
 * shapes and draws Arabic and Bengali with HarfBuzz and FreeType. SDL_ttf
 * opens the library only when such text is first drawn: programs drawing
 * other languages never load it.
 *
 * Raise TTF_SCRIPT_ABI whenever this structure changes; SDL_ttf then refuses
 * an older library and draws the text as before. */
#define TTF_SCRIPT_ABI 1
#define TTF_SCRIPT_LIBRARY "libSDL_ttf-script.so"
#define TTF_SCRIPT_ENTRY "ttf_script_api"

enum {
    TTF_SCRIPT_ARABIC = 1,
    TTF_SCRIPT_BENGALI = 2
};

/* Same values as TTF_STYLE_BOLD and TTF_STYLE_ITALIC. */
enum {
    TTF_SCRIPT_BOLD = 1,
    TTF_SCRIPT_ITALIC = 2
};

typedef struct TTFScriptFont TTFScriptFont;

typedef struct {
    /* Ink extent of a run in pixels: left and right from the run's start
     * (left may be negative, right may exceed the advance for italic or wide
     * marks), top and bottom from the baseline (top negative, above it). All
     * zero when the run has no ink. */
    int left, right, top, bottom;
} TTFScriptInk;

typedef struct {
    int abi;
    /* Open a font from files; bold may be NULL or missing, then bold is
     * emboldened from the regular face. NULL when the regular face fails. */
    TTFScriptFont *(*open)(const char *regular, const char *bold);
    void (*close)(TTFScriptFont *font);
    /* Whether the regular face maps this code point to a glyph. */
    int (*covers)(TTFScriptFont *font, uint32_t codepoint);
    /* Ascent and descent (positive, below the baseline) at a pixel size. */
    void (*metrics)(TTFScriptFont *font, int size, int *ascent, int *descent);
    /* Shape a run in logical order and return its advance in pixels, or -1.
     * With alpha set, also draw it as coverage into the width x height
     * buffer, starting at x on the baseline; pixels outside are dropped.
     * ink, when set, receives the run's ink extent. */
    int (*run)(TTFScriptFont *font, int size, int style, int script, int rtl,
               const uint32_t *text, int length, uint8_t *alpha, int width, int height, int x,
               int baseline, TTFScriptInk *ink);
    /* Release what the library keeps between calls, after every font is
     * closed and before the library is unloaded. */
    void (*shutdown)(void);
} TTFScriptApi;

typedef const TTFScriptApi *(*TTFScriptEntry)(void);

#endif
