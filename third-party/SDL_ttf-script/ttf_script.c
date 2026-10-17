/* ttf_script.c: Arabic and Bengali for SDL_ttf. Included at the end of
 * SDL_ttf.c, so it sees TTF_Font's fields and the original text functions,
 * renamed plain_*.
 *
 * SDL_ttf 2.0 draws one glyph per character, left to right: Arabic letters
 * come out unjoined and in the wrong order, Bengali vowel signs and conjuncts
 * come out as separate pieces, and no font Onion ships has their letters. Here
 * the four public functions every text call ends in (TTF_SizeUNICODE and the
 * three TTF_RenderUNICODE_*, which the Text and UTF8 variants call) look for
 * those letters first. Text without them goes to the original code untouched.
 * Text with them is split into runs in display order (ttf_bidi.c): Arabic and
 * Bengali runs are shaped and drawn by libSDL_ttf-script.so with Noto Sans
 * Arabic UI and Noto Sans Bengali UI, the rest by the original code in the
 * caller's font, and the runs are put together in the mode asked for.
 *
 * The library is opened the first time such text is drawn, from the folder
 * this library is in, so programs that never draw Arabic or Bengali never load
 * it. Without the library or a font, the text is drawn as before.
 *
 * Copyright (C) 2026 OnionPlus contributors. Same terms as SDL_ttf (zlib),
 * see COPYING-SDL_ttf. */
#include "script/script_api.h"
#include "ttf_bidi.c"
#include <dlfcn.h>
#include <pthread.h>

/* Where the fonts are: Onion's built-in asset folder, or TTF_SCRIPT_FONTS. */
#define TTF_SCRIPT_FONT_DIR "/mnt/SDCARD/miyoo/app"
/* Longer text goes to the original code as it is: labels are far shorter. */
#define TTF_SCRIPT_TEXT_LIMIT 4096

/* One lock for the library, its fonts and its shaping buffer. SDL_ttf is not
 * thread-safe for one font, but programs do draw with different fonts from
 * different threads, and the script fonts are shared by all of them. */
static pthread_mutex_t script_lock = PTHREAD_MUTEX_INITIALIZER;
static void *script_library;
static const TTFScriptApi *script_api;
static int script_library_failed;
static TTFScriptFont *script_fonts[3];
static int script_font_failed[3];

static int script_load_library(void)
{
    if (script_api) {
        return 1;
    }
    if (script_library_failed) {
        return 0;
    }
    script_library_failed = 1;
    /* Next to this library: miyoo/lib on the device. Then the usual search
     * path, for a build installed elsewhere. */
    char path[4096];
    Dl_info info;
    path[0] = 0;
    if (dladdr((void *)TTF_Init, &info) && info.dli_fname) {
        const char *slash = strrchr(info.dli_fname, '/');
        size_t length = slash ? (size_t)(slash - info.dli_fname) : 0;
        if (slash && length + 1 + sizeof TTF_SCRIPT_LIBRARY < sizeof path) {
            memcpy(path, info.dli_fname, length);
            snprintf(path + length, sizeof path - length, "/%s", TTF_SCRIPT_LIBRARY);
        }
    }
    script_library = path[0] ? dlopen(path, RTLD_NOW | RTLD_LOCAL) : NULL;
    if (!script_library) {
        script_library = dlopen(TTF_SCRIPT_LIBRARY, RTLD_NOW | RTLD_LOCAL);
    }
    if (!script_library) {
        fprintf(stderr, "SDL_ttf: Arabic and Bengali text unavailable: %s\n", dlerror());
        return 0;
    }
    TTFScriptEntry entry;
    *(void **)&entry = dlsym(script_library, TTF_SCRIPT_ENTRY);
    const TTFScriptApi *table = entry ? entry() : NULL;
    if (!table || table->abi != TTF_SCRIPT_ABI || !table->open || !table->close ||
        !table->covers || !table->metrics || !table->run || !table->shutdown) {
        fprintf(stderr, "SDL_ttf: Arabic and Bengali text unavailable: "
                        "%s has another interface\n", TTF_SCRIPT_LIBRARY);
        dlclose(script_library);
        script_library = NULL;
        return 0;
    }
    script_api = table;
    script_library_failed = 0;
    return 1;
}

static TTFScriptFont *script_font(int script)
{
    if (script_fonts[script] || script_font_failed[script] || !script_load_library()) {
        return script_fonts[script];
    }
    const char *dir = getenv("TTF_SCRIPT_FONTS");
    if (!dir || !*dir) {
        dir = TTF_SCRIPT_FONT_DIR;
    }
    const char *name = script == TTF_BIDI_ARABIC ? "NotoSansArabicUI" : "NotoSansBengaliUI";
    static char regular[4096], bold[4096]; /* under script_lock */
    int a = snprintf(regular, sizeof regular, "%s/%s-Regular.ttf", dir, name);
    int b = snprintf(bold, sizeof bold, "%s/%s-Bold.ttf", dir, name);
    if (a > 0 && a < (int)sizeof regular && b > 0 && b < (int)sizeof bold) {
        script_fonts[script] = script_api->open(regular, bold);
    }
    if (!script_fonts[script]) {
        script_font_failed[script] = 1;
        fprintf(stderr, "SDL_ttf: cannot open script font %s\n", regular);
    }
    return script_fonts[script];
}

static bool script_covers(int script, uint32_t codepoint, void *context)
{
    (void)context;
    return script_fonts[script] && script_api->covers(script_fonts[script], codepoint);
}

/* Called by TTF_Quit: the fonts and the shaping buffer are released; the
 * library stays loaded, as HarfBuzz keeps a few tables it never frees. */
static void ttf_script_quit(void)
{
    pthread_mutex_lock(&script_lock);
    for (int s = 1; s <= 2; s++) {
        if (script_api && script_fonts[s]) {
            script_api->close(script_fonts[s]);
        }
        script_fonts[s] = NULL;
        script_font_failed[s] = 0;
    }
    if (script_api) {
        script_api->shutdown();
    }
    pthread_mutex_unlock(&script_lock);
}

/* ----------------------------------------------------------------- text */

/* The code points of a UNICODE string, with its byte order marks applied as
 * the original functions apply them. Returns how many, or -1 when longer
 * than `capacity`. */
static int script_decode(const Uint16 *text, uint32_t *out, int capacity)
{
    int swapped = TTF_byteswapped;
    int n = 0;
    for (const Uint16 *ch = text; *ch; ++ch) {
        Uint16 c = *ch;
        if (c == UNICODE_BOM_NATIVE) {
            swapped = 0;
            continue;
        }
        if (c == UNICODE_BOM_SWAPPED) {
            swapped = 1;
            continue;
        }
        if (n == capacity) {
            return -1;
        }
        out[n++] = swapped ? SDL_Swap16(c) : c;
    }
    return n;
}

/* The test on every call, without allocating or locking: whether the text has
 * an Arabic or Bengali letter, byte order marks applied as script_decode
 * applies them. Chinese, Japanese or Korean text costs one pass, no more. */
static int script_maybe(const Uint16 *text)
{
    int swapped = TTF_byteswapped;
    for (const Uint16 *ch = text; *ch; ++ch) {
        Uint16 c = *ch;
        if (c == UNICODE_BOM_NATIVE || c == UNICODE_BOM_SWAPPED) {
            swapped = c == UNICODE_BOM_SWAPPED;
            continue;
        }
        if (script_of(swapped ? SDL_Swap16(c) : c)) {
            return 1;
        }
    }
    return 0;
}

typedef struct {
    int x, width;
    TTFBidiRun run;
    Uint16 *plain;        /* a run in the caller's font, in display order */
    SDL_Surface *surface; /* its raster (Blended), when drawing */
} Piece;

typedef struct {
    uint32_t *text;
    TTFBidiRun *runs;
    Piece *pieces;
    int count;
    int size, style;
    int width, height, origin, baseline; /* origin: x of the first piece */
} Layout;

static void layout_free(Layout *layout)
{
    for (int i = 0; i < layout->count; i++) {
        free(layout->pieces[i].plain);
        if (layout->pieces[i].surface) {
            SDL_FreeSurface(layout->pieces[i].surface);
        }
    }
    free(layout->pieces);
    free(layout->runs);
    free(layout->text);
    memset(layout, 0, sizeof *layout);
}

/* Lay the text out; with `draw`, also render the runs in the caller's font.
 * Returns 0 when the text must go to the original code as it is. Called
 * under script_lock. */
static int layout_build(Layout *layout, TTF_Font *font, const Uint16 *text, int draw)
{
    memset(layout, 0, sizeof *layout);
    /* A font with an outline keeps the original code: rare, and its
     * metrics would need the outline added to every run. */
    if (!font || font->outline > 0 || !font->face || !font->face->size) {
        return 0;
    }
    int size = font->face->size->metrics.y_ppem;
    if (size <= 0) {
        return 0;
    }
    layout->text = malloc(TTF_SCRIPT_TEXT_LIMIT * sizeof *layout->text);
    if (!layout->text) {
        return 0;
    }
    int length = script_decode(text, layout->text, TTF_SCRIPT_TEXT_LIMIT);
    if (length <= 0 || !ttf_bidi_complex(layout->text, length)) {
        layout_free(layout);
        return 0;
    }
    /* Only the fonts of the scripts this text has. */
    unsigned present = 0, available = 0;
    for (int i = 0; i < length; i++) {
        present |= 1u << script_of(layout->text[i]);
    }
    for (int script = 1; script <= 2; script++) {
        if (present & (1u << script) && script_font(script)) {
            available |= 1u << script;
        }
    }
    layout->runs = available ? malloc((size_t)length * sizeof *layout->runs) : NULL;
    int count = layout->runs ? ttf_bidi_layout(layout->text, length, available, script_covers,
                                               NULL, layout->runs, length)
                             : -1;
    layout->pieces = count > 0 ? calloc((size_t)count, sizeof *layout->pieces) : NULL;
    if (!layout->pieces) {
        layout_free(layout);
        return 0;
    }
    layout->count = count;
    layout->size = size;
    /* Bold for a bold face or a bold style: the Bold file. Italic only when
     * asked for, slanted as SDL_ttf slants. */
    layout->style = ((font->style | font->face_style) & TTF_STYLE_BOLD ? TTF_SCRIPT_BOLD : 0) |
                    (font->style & TTF_STYLE_ITALIC ? TTF_SCRIPT_ITALIC : 0);
    /* The caller's runs are drawn without underline and strikethrough: the
     * lines are drawn across the whole text afterwards. */
    int lines = font->style & TTF_STYLE_NO_GLYPH_CHANGE;
    font->style &= ~TTF_STYLE_NO_GLYPH_CHANGE;
    int top = -font->ascent, bottom = font->height - font->ascent;
    int left = 0, right = 0, x = 0, ok = 1;
    for (int i = 0; ok && i < count; i++) {
        Piece *piece = &layout->pieces[i];
        piece->run = layout->runs[i];
        piece->x = x;
        const uint32_t *part = layout->text + piece->run.start;
        if (piece->run.script) {
            TTFScriptInk ink;
            piece->width = script_api->run(script_fonts[piece->run.script], size, layout->style,
                                           piece->run.script, piece->run.level & 1, part,
                                           piece->run.length, NULL, 0, 0, 0, 0, &ink);
            ok = piece->width >= 0;
            if (ok && ink.right > ink.left) {
                left = x + ink.left < left ? x + ink.left : left;
                right = x + ink.right > right ? x + ink.right : right;
                top = ink.top < top ? ink.top : top;
                bottom = ink.bottom > bottom ? ink.bottom : bottom;
            }
        }
        else {
            /* Right-to-left runs in the caller's font are spaces and
             * punctuation between Arabic words: reverse and mirror them. */
            piece->plain = malloc((size_t)(piece->run.length + 2) * sizeof *piece->plain);
            ok = piece->plain != NULL;
            if (ok) {
                piece->plain[0] = UNICODE_BOM_NATIVE;
                for (int k = 0; k < piece->run.length; k++) {
                    uint32_t c = piece->run.level & 1
                                     ? ttf_bidi_mirror(part[piece->run.length - 1 - k])
                                     : part[k];
                    piece->plain[1 + k] = (Uint16)c;
                }
                piece->plain[1 + piece->run.length] = 0;
                int w = 0, h = 0;
                ok = plain_SizeUNICODE(font, piece->plain, &w, &h) == 0;
                piece->width = w;
                if (ok && draw && w > 0) {
                    SDL_Color white = {255, 255, 255, 0};
                    piece->surface = plain_RenderUNICODE_Blended(font, piece->plain, white);
                }
            }
        }
        x += piece->width;
    }
    font->style |= lines;
    layout->origin = -left;
    layout->baseline = -top;
    layout->width = (x > right ? x : right) - left;
    layout->height = bottom - top;
    if (!ok || layout->width <= 0 || layout->height <= 0) {
        layout_free(layout);
        return 0;
    }
    return 1;
}

/* Coverage of the whole text, width x height bytes. */
static Uint8 *layout_draw(Layout *layout, TTF_Font *font)
{
    Uint8 *alpha = calloc((size_t)layout->width * (size_t)layout->height, 1);
    if (!alpha) {
        return NULL;
    }
    int theme_top = layout->baseline - font->ascent;
    for (int i = 0; i < layout->count; i++) {
        Piece *piece = &layout->pieces[i];
        int x = layout->origin + piece->x;
        if (piece->run.script) {
            script_api->run(script_fonts[piece->run.script], layout->size, layout->style,
                            piece->run.script, piece->run.level & 1,
                            layout->text + piece->run.start, piece->run.length, alpha,
                            layout->width, layout->height, x, layout->baseline, NULL);
            continue;
        }
        SDL_Surface *part = piece->surface;
        if (!part || SDL_LockSurface(part) < 0) {
            continue;
        }
        for (int row = 0; row < part->h; row++) {
            int ty = theme_top + row;
            if (ty < 0 || ty >= layout->height) {
                continue;
            }
            const Uint32 *source = (const Uint32 *)((const Uint8 *)part->pixels + row * part->pitch);
            Uint8 *target = alpha + (size_t)ty * (size_t)layout->width;
            for (int column = 0; column < part->w; column++) {
                int tx = x + column;
                unsigned b = source[column] >> 24;
                if (tx >= 0 && tx < layout->width && b) {
                    unsigned a = target[tx];
                    target[tx] = (Uint8)(a + b - a * b / 255);
                }
            }
        }
        SDL_UnlockSurface(part);
    }
    return alpha;
}

enum { MODE_SOLID, MODE_SHADED, MODE_BLENDED };

/* The lines of an underline or strikethrough style, as the original code
 * draws them, moved by how far the baseline moved. */
static void draw_lines(TTF_Font *font, SDL_Surface *textbuf, int mode, int shift, Uint32 pixel)
{
    int rows[2], count = 0;
    if (TTF_HANDLE_STYLE_UNDERLINE(font)) {
        rows[count++] = TTF_underline_top_row(font) + shift;
    }
    if (TTF_HANDLE_STYLE_STRIKETHROUGH(font)) {
        rows[count++] = TTF_strikethrough_top_row(font) + shift;
    }
    for (int i = 0; i < count; i++) {
        if (mode == MODE_SOLID) {
            TTF_drawLine_Solid(font, textbuf, rows[i]);
        }
        else if (mode == MODE_SHADED) {
            TTF_drawLine_Shaded(font, textbuf, rows[i]);
        }
        else {
            TTF_drawLine_Blended(font, textbuf, rows[i], pixel);
        }
    }
}

/* The surface for one mode, in the same format as the original code's. */
static SDL_Surface *layout_surface(Layout *layout, TTF_Font *font, int mode, SDL_Color fg,
                                   SDL_Color bg)
{
    Uint8 *alpha = layout_draw(layout, font);
    if (!alpha) {
        return NULL;
    }
    int w = layout->width, h = layout->height;
    SDL_Surface *textbuf =
        mode == MODE_BLENDED
            ? SDL_AllocSurface(SDL_SWSURFACE, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
            : SDL_AllocSurface(SDL_SWSURFACE, w, h, 8, 0, 0, 0, 0);
    if (!textbuf) {
        free(alpha);
        return NULL;
    }
    Uint32 pixel = (fg.r << 16) | (fg.g << 8) | fg.b;
    if (mode == MODE_SOLID) {
        SDL_Palette *palette = textbuf->format->palette;
        palette->colors[0].r = 255 - fg.r;
        palette->colors[0].g = 255 - fg.g;
        palette->colors[0].b = 255 - fg.b;
        palette->colors[1].r = fg.r;
        palette->colors[1].g = fg.g;
        palette->colors[1].b = fg.b;
        SDL_SetColorKey(textbuf, SDL_SRCCOLORKEY, 0);
    }
    else if (mode == MODE_SHADED) {
        SDL_Palette *palette = textbuf->format->palette;
        int rdiff = fg.r - bg.r, gdiff = fg.g - bg.g, bdiff = fg.b - bg.b;
        for (int index = 0; index < NUM_GRAYS; ++index) {
            palette->colors[index].r = bg.r + (index * rdiff) / (NUM_GRAYS - 1);
            palette->colors[index].g = bg.g + (index * gdiff) / (NUM_GRAYS - 1);
            palette->colors[index].b = bg.b + (index * bdiff) / (NUM_GRAYS - 1);
        }
    }
    if (SDL_MUSTLOCK(textbuf)) {
        SDL_LockSurface(textbuf);
    }
    for (int row = 0; row < h; row++) {
        const Uint8 *source = alpha + (size_t)row * (size_t)w;
        Uint8 *line = (Uint8 *)textbuf->pixels + row * textbuf->pitch;
        for (int column = 0; column < w; column++) {
            Uint8 a = source[column];
            if (mode == MODE_BLENDED) {
                ((Uint32 *)line)[column] = (Uint32)a << 24 | pixel;
            }
            else if (mode == MODE_SHADED) {
                line[column] = a; /* NUM_GRAYS is 256 */
            }
            else {
                line[column] = a >= 128;
            }
        }
    }
    draw_lines(font, textbuf, mode, layout->baseline - font->ascent, pixel);
    if (SDL_MUSTLOCK(textbuf)) {
        SDL_UnlockSurface(textbuf);
    }
    free(alpha);
    return textbuf;
}

/* --------------------------------------------------- the public functions */

int TTF_SizeUNICODE(TTF_Font *font, const Uint16 *text, int *w, int *h)
{
    if (TTF_initialized && font && text && script_maybe(text)) {
        pthread_mutex_lock(&script_lock);
        Layout layout;
        int done = layout_build(&layout, font, text, 0);
        if (done) {
            if (w) {
                *w = layout.width;
            }
            if (h) {
                *h = layout.height;
            }
            layout_free(&layout);
        }
        pthread_mutex_unlock(&script_lock);
        if (done) {
            return 0;
        }
    }
    return plain_SizeUNICODE(font, text, w, h);
}

static SDL_Surface *script_render(TTF_Font *font, const Uint16 *text, int mode, SDL_Color fg,
                                  SDL_Color bg, int *done)
{
    *done = 0;
    if (!TTF_initialized || !font || !text || !script_maybe(text)) {
        return NULL;
    }
    pthread_mutex_lock(&script_lock);
    Layout layout;
    SDL_Surface *surface = NULL;
    if (layout_build(&layout, font, text, 1)) {
        surface = layout_surface(&layout, font, mode, fg, bg);
        *done = surface != NULL;
        layout_free(&layout);
    }
    pthread_mutex_unlock(&script_lock);
    return surface;
}

SDL_Surface *TTF_RenderUNICODE_Solid(TTF_Font *font, const Uint16 *text, SDL_Color fg)
{
    int done;
    SDL_Surface *surface = script_render(font, text, MODE_SOLID, fg, fg, &done);
    return done ? surface : plain_RenderUNICODE_Solid(font, text, fg);
}

SDL_Surface *TTF_RenderUNICODE_Shaded(TTF_Font *font, const Uint16 *text, SDL_Color fg,
                                      SDL_Color bg)
{
    int done;
    SDL_Surface *surface = script_render(font, text, MODE_SHADED, fg, bg, &done);
    return done ? surface : plain_RenderUNICODE_Shaded(font, text, fg, bg);
}

SDL_Surface *TTF_RenderUNICODE_Blended(TTF_Font *font, const Uint16 *text, SDL_Color fg)
{
    int done;
    SDL_Surface *surface = script_render(font, text, MODE_BLENDED, fg, fg, &done);
    return done ? surface : plain_RenderUNICODE_Blended(font, text, fg);
}
