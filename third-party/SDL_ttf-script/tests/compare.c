/* Draws every string of a list with SDL_ttf in each mode and prints a hash of
 * each result, so that two SDL_ttf builds can be compared run against run
 * (tests/compare.py). Optionally saves a sheet of some strings as a BMP.
 *
 *   compare STRINGS FONTDIR [SHEET.bmp INDEX...]
 *
 * STRINGS has one hex-encoded UTF-8 string per line. */
#include "SDL_ttf.h"
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *file;
    int size, style;
} Config;

/* Fonts and styles Onion and Open MainUI draw with. */
static const Config configs[] = {
    {"Exo-2-Bold-Italic.ttf", 25, 0},                  /* titles, the default font */
    {"Exo-2-Bold-Italic.ttf", 18, TTF_STYLE_BOLD},     /* menus: synthetic bold */
    {"wqy-microhei.ttc", 24, 0},                       /* other languages' fallback */
    {"Exo-2-Bold-Italic_Universal.ttf", 22, TTF_STYLE_ITALIC | TTF_STYLE_UNDERLINE},
};
#define CONFIGS (int)(sizeof configs / sizeof configs[0])

static uint64_t fnv(uint64_t h, const void *data, size_t n)
{
    const unsigned char *p = data;
    for (size_t i = 0; i < n; i++) {
        h = (h ^ p[i]) * 1099511628211ULL;
    }
    return h;
}

static uint64_t surface_hash(SDL_Surface *s)
{
    if (!s) {
        return 0;
    }
    uint64_t h = 14695981039346656037ULL;
    int bpp = s->format->BytesPerPixel;
    h = fnv(h, &s->w, sizeof s->w);
    h = fnv(h, &s->h, sizeof s->h);
    h = fnv(h, &bpp, sizeof bpp);
    if (s->format->palette) {
        for (int i = 0; i < s->format->palette->ncolors; i++) {
            SDL_Color c = s->format->palette->colors[i];
            unsigned char rgb[3] = {c.r, c.g, c.b};
            h = fnv(h, rgb, 3);
        }
        Uint32 key = s->flags & SDL_SRCCOLORKEY ? s->format->colorkey : 0xFFFFFFFF;
        h = fnv(h, &key, sizeof key);
    }
    SDL_LockSurface(s);
    for (int y = 0; y < s->h; y++) {
        h = fnv(h, (Uint8 *)s->pixels + y * s->pitch, (size_t)s->w * bpp);
    }
    SDL_UnlockSurface(s);
    return h;
}

static char *unhex(const char *line)
{
    size_t n = strlen(line) / 2;
    char *out = malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        unsigned v;
        sscanf(line + 2 * i, "%2x", &v);
        out[i] = (char)v;
    }
    out[n] = 0;
    return out;
}

/* The same text as UNICODE in the other byte order, behind a swapped BOM. */
static Uint16 *swapped_unicode(const char *utf8)
{
    size_t n = strlen(utf8);
    Uint16 *out = calloc(n + 2, sizeof *out);
    size_t k = 0;
    out[k++] = UNICODE_BOM_SWAPPED;
    const unsigned char *s = (const unsigned char *)utf8;
    while (*s) {
        Uint32 c = *s;
        int extra = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
        c &= extra == 0 ? 0x7F : extra == 1 ? 0x1F : extra == 2 ? 0x0F : 0x07;
        for (int i = 1; i <= extra && s[i]; i++) {
            c = c << 6 | (s[i] & 0x3F);
        }
        s += extra + 1;
        out[k++] = SDL_Swap16((Uint16)c);
    }
    return out;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: compare STRINGS FONTDIR [SHEET.bmp INDEX...]\n");
        return 2;
    }
    if (TTF_Init() < 0) {
        return 1;
    }
    TTF_Font *fonts[CONFIGS];
    for (int c = 0; c < CONFIGS; c++) {
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", argv[2], configs[c].file);
        fonts[c] = TTF_OpenFont(path, configs[c].size);
        if (!fonts[c]) {
            fprintf(stderr, "cannot open %s\n", path);
            return 1;
        }
        TTF_SetFontStyle(fonts[c], configs[c].style);
    }
    FILE *list = fopen(argv[1], "r");
    if (!list) {
        return 1;
    }
    char **texts = NULL;
    int count = 0;
    static char line[65536];
    while (fgets(line, sizeof line, list)) {
        line[strcspn(line, "\r\n")] = 0;
        texts = realloc(texts, (size_t)(count + 1) * sizeof *texts);
        texts[count++] = unhex(line);
    }
    fclose(list);
    SDL_Color fg = {240, 230, 200, 0}, bg = {20, 30, 40, 0};
    for (int i = 0; i < count; i++) {
        for (int c = 0; c < CONFIGS; c++) {
            int w = -1, h = -1;
            int sized = TTF_SizeUTF8(fonts[c], texts[i], &w, &h);
            SDL_Surface *blended = TTF_RenderUTF8_Blended(fonts[c], texts[i], fg);
            SDL_Surface *shaded = TTF_RenderUTF8_Shaded(fonts[c], texts[i], fg, bg);
            SDL_Surface *solid = TTF_RenderUTF8_Solid(fonts[c], texts[i], fg);
            /* The UNICODE path with swapped bytes must give the same image. */
            Uint16 *swapped = swapped_unicode(texts[i]);
            SDL_Surface *unicode = TTF_RenderUNICODE_Blended(fonts[c], swapped, fg);
            int same = surface_hash(unicode) == surface_hash(blended);
            /* A surface as large as its reported size. */
            int fits = !blended || (blended->w == w && blended->h == h);
            printf("%d %d size %d %d %d blended %016llx shaded %016llx solid %016llx swap %d fits %d\n",
                   i, c, sized, w, h, (unsigned long long)surface_hash(blended),
                   (unsigned long long)surface_hash(shaded), (unsigned long long)surface_hash(solid),
                   same, fits);
            SDL_FreeSurface(blended);
            SDL_FreeSurface(shaded);
            SDL_FreeSurface(solid);
            SDL_FreeSurface(unicode);
            free(swapped);
        }
    }
    /* Sheet: the chosen strings in the first two configurations, one row each. */
    if (argc > 3) {
        int rows = argc - 4;
        SDL_Surface *sheet = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 20 + rows * 2 * 40, 32,
                                                  0x00FF0000, 0x0000FF00, 0x000000FF, 0);
        SDL_FillRect(sheet, NULL, SDL_MapRGB(sheet->format, 32, 34, 40));
        int y = 10;
        for (int r = 0; r < rows; r++) {
            int index = atoi(argv[4 + r]);
            for (int c = 0; c < 2 && index >= 0 && index < count; c++) {
                SDL_Surface *text = TTF_RenderUTF8_Blended(fonts[c], texts[index], fg);
                SDL_Rect at = {12, (Sint16)y, 0, 0};
                if (text) {
                    SDL_BlitSurface(text, NULL, sheet, &at);
                    SDL_FreeSurface(text);
                }
                y += 40;
            }
        }
        SDL_SaveBMP(sheet, argv[3]);
        SDL_FreeSurface(sheet);
    }
    for (int c = 0; c < CONFIGS; c++) {
        TTF_CloseFont(fonts[c]);
    }
    for (int i = 0; i < count; i++) {
        free(texts[i]);
    }
    free(texts);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
