/* Software stand-in for the SDL 1.2 and SDL_ttf calls easter.c uses. */
#define _DEFAULT_SOURCE 1
#include <SDL/SDL.h>
#include <SDL/SDL_loadso.h>
#include <SDL/SDL_ttf.h>
#include <ft2build.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include FT_FREETYPE_H
int SDL_Init(Uint32 f)
{
    (void)f;
    return 0;
}
void SDL_Quit(void) {}
int SDL_InitSubSystem(Uint32 f)
{
    (void)f;
    return -1;
}
void SDL_QuitSubSystem(Uint32 f) { (void)f; }
const char *SDL_GetError(void) { return "shim"; }
SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int w, int h, int bpp, Uint32 r, Uint32 g, Uint32 b, Uint32 a)
{
    (void)bpp;
    SDL_Surface *s = calloc(1, sizeof *s);
    s->fmt = (SDL_PixelFormat){32, 4, r ? r : 0xff0000, g ? g : 0xff00, b ? b : 0xff, a};
    s->format = &s->fmt;
    s->w = w;
    s->h = h;
    s->pitch = (Uint16)(w * 4);
    s->pixels = calloc((size_t)w * h, 4);
    s->flags = flags | (a ? SDL_SRCALPHA : 0);
    return s;
}
SDL_Surface *SDL_SetVideoMode(int w, int h, int bpp, Uint32 f) { return SDL_CreateRGBSurface(f, w, h, bpp, 0, 0, 0, 0); }
SDL_Surface *SDL_DisplayFormatAlpha(SDL_Surface *s)
{
    SDL_Surface *d = SDL_CreateRGBSurface(0, s->w, s->h, 32, 0xff0000, 0xff00, 0xff, 0xff000000u);
    memcpy(d->pixels, s->pixels, (size_t)s->w * s->h * 4);
    return d;
}
void SDL_FreeSurface(SDL_Surface *s)
{
    if (s) {
        free(s->pixels);
        free(s);
    }
}
int SDL_SetAlpha(SDL_Surface *s, Uint32 flag, Uint8 a)
{
    (void)a;
    s->flags = (s->flags & ~SDL_SRCALPHA) | (flag & SDL_SRCALPHA);
    return 0;
}
Uint32 SDL_MapRGB(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b)
{
    (void)f;
    return (Uint32)r << 16 | (Uint32)g << 8 | b;
}
int SDL_FillRect(SDL_Surface *s, SDL_Rect *r, Uint32 c)
{
    int x0 = r ? r->x : 0, y0 = r ? r->y : 0, x1 = r ? r->x + r->w : s->w, y1 = r ? r->y + r->h : s->h;
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > s->w)
        x1 = s->w;
    if (y1 > s->h)
        y1 = s->h;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            ((Uint32 *)s->pixels)[y * s->w + x] = c;
    return 0;
}
int SDL_BlitSurface(SDL_Surface *src, SDL_Rect *sr, SDL_Surface *dst, SDL_Rect *dr)
{
    (void)sr;
    int dx = dr ? dr->x : 0, dy = dr ? dr->y : 0;
    int blend = src->format->Amask && (src->flags & SDL_SRCALPHA);
    for (int y = 0; y < src->h; y++)
        for (int x = 0; x < src->w; x++) {
            int tx = dx + x, ty = dy + y;
            if (tx < 0 || ty < 0 || tx >= dst->w || ty >= dst->h)
                continue;
            Uint32 p = ((Uint32 *)src->pixels)[y * src->w + x], *q = &((Uint32 *)dst->pixels)[ty * dst->w + tx];
            if (!blend) {
                *q = p & 0xffffff;
                continue;
            }
            Uint32 a = p >> 24, out = 0;
            for (int sh = 0; sh <= 16; sh += 8) {
                Uint32 s = (p >> sh) & 255, d = (*q >> sh) & 255;
                out |= ((s * a + d * (255 - a)) / 255) << sh;
            }
            *q = out;
        }
    return 0;
}
int SDL_Flip(SDL_Surface *s)
{
    (void)s;
    return 0;
}
int SDL_LockSurface(SDL_Surface *s)
{
    (void)s;
    return 0;
}
void SDL_UnlockSurface(SDL_Surface *s) { (void)s; }
int SDL_SaveBMP(SDL_Surface *s, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    int row = (s->w * 3 + 3) & ~3, size = 54 + row * s->h;
    unsigned char h[54] = {'B', 'M'};
    h[2] = size;
    h[3] = size >> 8;
    h[4] = size >> 16;
    h[10] = 54;
    h[14] = 40;
    h[18] = s->w;
    h[19] = s->w >> 8;
    h[22] = s->h;
    h[23] = s->h >> 8;
    h[26] = 1;
    h[28] = 24;
    fwrite(h, 1, 54, f);
    unsigned char *line = calloc(1, row);
    for (int y = s->h - 1; y >= 0; y--) {
        for (int x = 0; x < s->w; x++) {
            Uint32 p = ((Uint32 *)s->pixels)[y * s->w + x];
            line[x * 3] = p;
            line[x * 3 + 1] = p >> 8;
            line[x * 3 + 2] = p >> 16;
        }
        fwrite(line, 1, row, f);
    }
    free(line);
    fclose(f);
    return 0;
}
int SDL_ShowCursor(int t)
{
    (void)t;
    return 0;
}
int SDL_EnableKeyRepeat(int a, int b)
{
    (void)a;
    (void)b;
    return 0;
}
int SDL_PollEvent(SDL_Event *e)
{
    (void)e;
    return 0;
}
Uint8 *SDL_GetKeyState(int *n)
{
    static Uint8 keys[SDLK_LAST];
    if (n)
        *n = SDLK_LAST;
    return keys;
}
Uint32 SDL_GetTicks(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (Uint32)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
void SDL_Delay(Uint32 ms) { usleep(ms * 1000); }
SDL_mutex *SDL_CreateMutex(void) { return NULL; }
void SDL_DestroyMutex(SDL_mutex *m) { (void)m; }
int SDL_LockMutex(SDL_mutex *m)
{
    (void)m;
    return 0;
}
int SDL_UnlockMutex(SDL_mutex *m)
{
    (void)m;
    return 0;
}
SDL_Thread *SDL_CreateThread(int (*fn)(void *), void *d)
{
    (void)fn;
    (void)d;
    return NULL;
}
void SDL_WaitThread(SDL_Thread *t, int *s)
{
    (void)t;
    (void)s;
}
void SDL_LockAudio(void) {}
void SDL_UnlockAudio(void) {}
void SDL_PauseAudio(int p) { (void)p; }
void *SDL_LoadObject(const char *n)
{
    (void)n;
    return NULL;
}
void *SDL_LoadFunction(void *h, const char *n)
{
    (void)h;
    (void)n;
    return NULL;
}
void SDL_UnloadObject(void *h) { (void)h; }

static FT_Library library;
struct TTF_Font {
    FT_Face face;
    int ascent, height;
};
int TTF_Init(void) { return FT_Init_FreeType(&library); }
void TTF_Quit(void) {}
const char *TTF_GetError(void) { return "shim"; }
TTF_Font *TTF_OpenFont(const char *path, int size)
{
    FT_Face face;
    if (FT_New_Face(library, path, 0, &face))
        return NULL;
    FT_Set_Pixel_Sizes(face, 0, (unsigned)size);
    TTF_Font *f = calloc(1, sizeof *f);
    f->face = face;
    f->ascent = (int)(face->size->metrics.ascender >> 6);
    f->height = (int)((face->size->metrics.ascender - face->size->metrics.descender) >> 6) + 1;
    return f;
}
void TTF_CloseFont(TTF_Font *f)
{
    if (f) {
        FT_Done_Face(f->face);
        free(f);
    }
}
int TTF_SizeUTF8(TTF_Font *f, const char *s, int *w, int *h)
{
    int width = 0;
    for (; *s; s++)
        if (!FT_Load_Char(f->face, (unsigned char)*s, FT_LOAD_DEFAULT))
            width += (int)(f->face->glyph->advance.x >> 6);
    if (w)
        *w = width + 2;
    if (h)
        *h = f->height;
    return 0;
}
SDL_Surface *TTF_RenderUTF8_Blended(TTF_Font *f, const char *s, SDL_Color c)
{
    int w, h;
    TTF_SizeUTF8(f, s, &w, &h);
    SDL_Surface *out = SDL_CreateRGBSurface(0, w, h, 32, 0xff0000, 0xff00, 0xff, 0xff000000u);
    Uint32 rgb = (Uint32)c.r << 16 | (Uint32)c.g << 8 | c.b;
    int pen = 0;
    for (int i = 0; i < w * h; i++)
        ((Uint32 *)out->pixels)[i] = rgb;
    for (; *s; s++) {
        if (FT_Load_Char(f->face, (unsigned char)*s, FT_LOAD_RENDER))
            continue;
        FT_GlyphSlot g = f->face->glyph;
        for (unsigned y = 0; y < g->bitmap.rows; y++)
            for (unsigned x = 0; x < g->bitmap.width; x++) {
                int tx = pen + g->bitmap_left + (int)x, ty = f->ascent - g->bitmap_top + (int)y;
                if (tx < 0 || ty < 0 || tx >= w || ty >= h)
                    continue;
                Uint32 a = g->bitmap.buffer[y * g->bitmap.pitch + x], *q = &((Uint32 *)out->pixels)[ty * w + tx];
                if (a > (*q >> 24))
                    *q = rgb | a << 24;
            }
        pen += (int)(g->advance.x >> 6);
    }
    return out;
}
