/* Part of OnionPlus's SDL_ttf (zlib terms, see ../COPYING-SDL_ttf). */
/* libSDL_ttf-script.so: shapes Arabic and Bengali runs with HarfBuzz and
 * draws them with FreeType. SDL_ttf opens it on demand (ttf_script.c) and
 * talks to it only through TTFScriptApi (script_api.h).
 *
 * Fonts are read into memory once and shared by HarfBuzz and FreeType, so the
 * glyph indices HarfBuzz returns always name glyphs of the face FreeType draws.
 * SDL_ttf calls it under one lock, never from two threads at once. */
#include "script_api.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include <hb-ot.h>
#include <hb.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPORT __attribute__((visibility("default")))
/* Fonts larger than this are refused. */
#define FONT_LIMIT (16u * 1024u * 1024u)
/* SDL_ttf 2.0's italic shear (GLYPH_ITALICS 0.207), in 16.16. */
#define ITALIC_SHEAR 13566
/* Bound the work for one run: labels are short, and a stray huge string must
 * not stall the program drawing it. */
#define RUN_LIMIT 4096

typedef struct {
    void *data;
    FT_Face ft;
    hb_blob_t *blob;
    hb_face_t *hb_face;
    hb_font_t *hb_font;
    int size; /* pixel size FT and HarfBuzz are set to, 0 when unset */
} Face;

struct TTFScriptFont {
    Face regular, bold;
    bool has_bold;
};

static FT_Library library;
static hb_buffer_t *buffer;

static void *read_file(const char *path, size_t *length)
{
    FILE *file = path ? fopen(path, "rb") : NULL;
    if (!file) {
        return NULL;
    }
    void *data = NULL;
    long size = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    if (size > 0 && (unsigned long)size <= FONT_LIMIT && fseek(file, 0, SEEK_SET) == 0) {
        data = malloc((size_t)size);
        if (data && fread(data, 1, (size_t)size, file) != (size_t)size) {
            free(data);
            data = NULL;
        }
    }
    fclose(file);
    *length = data ? (size_t)size : 0;
    return data;
}

static void face_close(Face *face)
{
    if (face->hb_font) {
        hb_font_destroy(face->hb_font);
    }
    if (face->hb_face) {
        hb_face_destroy(face->hb_face);
    }
    if (face->blob) {
        hb_blob_destroy(face->blob);
    }
    if (face->ft) {
        FT_Done_Face(face->ft);
    }
    free(face->data);
    memset(face, 0, sizeof *face);
}

static bool face_open(Face *face, const char *path)
{
    memset(face, 0, sizeof *face);
    size_t length = 0;
    face->data = read_file(path, &length);
    if (!face->data || length > INT_MAX ||
        FT_New_Memory_Face(library, face->data, (FT_Long)length, 0, &face->ft) ||
        !FT_IS_SCALABLE(face->ft)) {
        face_close(face);
        return false;
    }
    face->blob = hb_blob_create(face->data, (unsigned)length, HB_MEMORY_MODE_READONLY, NULL, NULL);
    face->hb_face = hb_face_create(face->blob, 0);
    face->hb_font = hb_font_create(face->hb_face);
    if (!hb_face_get_glyph_count(face->hb_face) || hb_font_is_immutable(face->hb_font)) {
        face_close(face);
        return false;
    }
    return true;
}

static bool face_size(Face *face, int size)
{
    if (face->size == size) {
        return true;
    }
    /* As SDL_ttf 2.0 sets theme fonts: points at 72 dpi, so pixels. */
    if (FT_Set_Char_Size(face->ft, 0, (FT_F26Dot6)size * 64, 0, 0)) {
        face->size = 0;
        return false;
    }
    hb_font_set_scale(face->hb_font, size * 64, size * 64);
    hb_font_set_ppem(face->hb_font, (unsigned)size, (unsigned)size);
    face->size = size;
    return true;
}

static TTFScriptFont *script_open(const char *regular, const char *bold)
{
    if (!library && FT_Init_FreeType(&library)) {
        library = NULL;
        return NULL;
    }
    if (!buffer) {
        buffer = hb_buffer_create();
        if (!hb_buffer_allocation_successful(buffer)) {
            hb_buffer_destroy(buffer);
            buffer = NULL;
            return NULL;
        }
    }
    TTFScriptFont *font = calloc(1, sizeof *font);
    if (!font || !face_open(&font->regular, regular)) {
        free(font);
        return NULL;
    }
    font->has_bold = bold && face_open(&font->bold, bold);
    return font;
}

static void script_close(TTFScriptFont *font)
{
    if (font) {
        face_close(&font->regular);
        if (font->has_bold) {
            face_close(&font->bold);
        }
        free(font);
    }
}

static int script_covers(TTFScriptFont *font, uint32_t codepoint)
{
    return font && FT_Get_Char_Index(font->regular.ft, codepoint) != 0;
}

static void script_metrics(TTFScriptFont *font, int size, int *ascent, int *descent)
{
    *ascent = *descent = 0;
    if (font && size > 0 && face_size(&font->regular, size)) {
        const FT_Size_Metrics *m = &font->regular.ft->size->metrics;
        *ascent = (int)((m->ascender + 63) >> 6);
        *descent = (int)((-m->descender + 63) >> 6);
    }
}

/* Round a 26.6 value down to whole pixels, also for negative values. */
static int floor26(long value)
{
    return (int)(value >= 0 ? value >> 6 : -((-value + 63) >> 6));
}

static void blend(uint8_t *alpha, int width, int height, const FT_Bitmap *bitmap, int x, int y)
{
    for (unsigned row = 0; row < bitmap->rows; row++) {
        int ty = y + (int)row;
        if (ty < 0 || ty >= height) {
            continue;
        }
        const unsigned char *source = bitmap->buffer + (long)row * bitmap->pitch;
        uint8_t *target = alpha + (size_t)ty * (size_t)width;
        for (unsigned column = 0; column < bitmap->width; column++) {
            int tx = x + (int)column;
            if (tx < 0 || tx >= width || !source[column]) {
                continue;
            }
            /* Coverage "over" coverage: overlapping marks and joins stay
             * solid instead of doubling or leaving seams. */
            unsigned a = target[tx], b = source[column];
            target[tx] = (uint8_t)(a + b - a * b / 255);
        }
    }
}

static int script_run(TTFScriptFont *font, int size, int style, int script, int rtl,
                      const uint32_t *text, int length, uint8_t *alpha, int width, int height,
                      int x, int baseline, TTFScriptInk *ink)
{
    if (ink) {
        *ink = (TTFScriptInk){0};
    }
    if (!font || !buffer || size <= 0 || size > 512 || !text || length <= 0 || length > RUN_LIMIT) {
        return length == 0 ? 0 : -1;
    }
    bool bold = style & TTF_SCRIPT_BOLD;
    bool italic = style & TTF_SCRIPT_ITALIC;
    Face *face = bold && font->has_bold ? &font->bold : &font->regular;
    bool embolden = bold && !font->has_bold;
    if (!face_size(face, size)) {
        return -1;
    }
    hb_buffer_clear_contents(buffer);
    hb_buffer_add_utf32(buffer, text, length, 0, length);
    hb_buffer_set_direction(buffer, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer,
                         script == TTF_SCRIPT_ARABIC ? HB_SCRIPT_ARABIC : HB_SCRIPT_BENGALI);
    /* No language: both fonts use their script's default language system, and
     * a lean HarfBuzz never frees the languages it is given. */
    hb_shape(face->hb_font, buffer, NULL, 0);
    if (!hb_buffer_allocation_successful(buffer)) {
        return -1;
    }
    unsigned count = 0;
    const hb_glyph_info_t *info = hb_buffer_get_glyph_infos(buffer, &count);
    const hb_glyph_position_t *position = hb_buffer_get_glyph_positions(buffer, NULL);

    FT_Matrix shear = {0x10000, ITALIC_SHEAR, 0, 0x10000};
    FT_Set_Transform(face->ft, italic ? &shear : NULL, NULL);
    /* Light hinting keeps glyph widths unhinted, so they agree with the
     * HarfBuzz positions used to place them. */
    FT_Int32 flags = FT_LOAD_TARGET_LIGHT | FT_LOAD_NO_BITMAP;
    long pen = 0;
    int left = INT_MAX, right = INT_MIN, top = INT_MAX, bottom = INT_MIN;
    bool measure = ink || alpha;
    for (unsigned i = 0; i < count; i++) {
        if (measure && !FT_Load_Glyph(face->ft, info[i].codepoint, flags)) {
            FT_GlyphSlot slot = face->ft->glyph;
            if (embolden && slot->format == FT_GLYPH_FORMAT_OUTLINE) {
                /* SDL_ttf 2.0 emboldens by a 24th of the size. */
                FT_Pos strength =
                    FT_MulFix(face->ft->units_per_EM, face->ft->size->metrics.y_scale) / 24;
                FT_Outline_EmboldenXY(&slot->outline, strength, 0);
            }
            if (!FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL)) {
                int gx = floor26(pen + position[i].x_offset) + slot->bitmap_left;
                int gy = -floor26(position[i].y_offset) - slot->bitmap_top;
                if (slot->bitmap.width && slot->bitmap.rows) {
                    if (gx < left) {
                        left = gx;
                    }
                    if (gx + (int)slot->bitmap.width > right) {
                        right = gx + (int)slot->bitmap.width;
                    }
                    if (gy < top) {
                        top = gy;
                    }
                    if (gy + (int)slot->bitmap.rows > bottom) {
                        bottom = gy + (int)slot->bitmap.rows;
                    }
                }
                if (alpha) {
                    blend(alpha, width, height, &slot->bitmap, x + gx, baseline + gy);
                }
            }
        }
        pen += position[i].x_advance;
    }
    FT_Set_Transform(face->ft, NULL, NULL);
    int advance = (int)((pen + 63) >> 6);
    if (embolden && advance > 0) {
        /* Emboldening widens each glyph; keep the last one inside the run. */
        advance += size / 24 > 0 ? size / 24 : 1;
    }
    if (ink && left <= right) {
        *ink = (TTFScriptInk){left, right, top, bottom};
    }
    return advance < 0 ? 0 : advance;
}

static void script_shutdown(void)
{
    if (buffer) {
        hb_buffer_destroy(buffer);
        buffer = NULL;
    }
    if (library) {
        FT_Done_FreeType(library);
        library = NULL;
    }
}

static const TTFScriptApi api = {
    .abi = TTF_SCRIPT_ABI,
    .open = script_open,
    .close = script_close,
    .covers = script_covers,
    .metrics = script_metrics,
    .run = script_run,
    .shutdown = script_shutdown,
};

EXPORT const TTFScriptApi *ttf_script_api(void)
{
    return &api;
}
