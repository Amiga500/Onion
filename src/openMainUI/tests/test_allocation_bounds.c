/* SPDX-License-Identifier: GPL-3.0-only */
#include "catalog/names.h"
#include "ui/artwork.h"
#include "ui/preview.h"
#ifdef main
#undef main
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void transparency(int depth, bool alpha)
{
    SDL_Surface *source = SDL_CreateRGBSurface(SDL_SWSURFACE, 2, 1, depth,
                                               depth == 8 ? 0 : 0xff0000, depth == 8 ? 0 : 0xff00,
                                               depth == 8 ? 0 : 0xff, alpha ? 0xff000000 : 0);
    assert(source);
    if (depth == 8) {
        SDL_Color colors[2] = {{0, 255, 0, 0}, {255, 0, 0, 0}};
        assert(SDL_SetColors(source, colors, 0, 2));
    }
    Uint32 green = SDL_MapRGBA(source->format, 0, 255, 0, 0);
    SDL_FillRect(source, NULL, green);
    SDL_Rect right = {1, 0, 1, 1};
    SDL_FillRect(source, &right, SDL_MapRGBA(source->format, 255, 0, 0, 255));
    if (!alpha) {
        assert(SDL_SetColorKey(source, SDL_SRCCOLORKEY, green) == 0);
    }
    SDL_Surface *image = mainui_artwork_software(source);
    assert(image && !(image->flags & SDL_HWSURFACE) && image->format->Amask);
    assert(!(image->flags & SDL_SRCCOLORKEY));
    Uint8 r, g, b, a;
    SDL_GetRGBA(*(Uint32 *)image->pixels, image->format, &r, &g, &b, &a);
    assert(a == 0); /* Hidden green must remain transparent. */
    SDL_Surface *canvas = SDL_CreateRGBSurface(SDL_SWSURFACE, 2, 1, 32, 0xff0000, 0xff00, 0xff, 0);
    assert(canvas);
    for (int repeat = 0; repeat < 2; ++repeat) {
        SDL_FillRect(canvas, NULL, SDL_MapRGB(canvas->format, 10, 20, 30));
        assert(SDL_BlitSurface(image, NULL, canvas, NULL) == 0);
        SDL_GetRGB(((Uint32 *)canvas->pixels)[0], canvas->format, &r, &g, &b);
        assert(r == 10 && g == 20 && b == 30);
        SDL_GetRGB(((Uint32 *)canvas->pixels)[1], canvas->format, &r, &g, &b);
        assert(r == 255 && g == 0 && b == 0);
    }
    SDL_FreeSurface(canvas);
    SDL_FreeSurface(image);
    SDL_FreeSurface(source);
}

int main(int argc, char **argv)
{
    assert(argc == 2 && SDL_Init(SDL_INIT_TIMER) == 0);
    transparency(8, false);
    transparency(24, false);
    transparency(32, true);
    const char *valid[] = {"small.png", "jpeg.png", "bmp.png", "edge.png", "palette.png"};
    const char *invalid[] = {"bomb.png",      "wide.png",      "tall.png",    "zero.png",
                             "truncated.png", "random.png",    "empty.png",   "jpeg-large.png",
                             "jpeg-bad.png",  "bmp-large.png", "unknown.png", "encoded-large.png"};
    char path[4096];
    for (size_t i = 0; i < sizeof valid / sizeof *valid; ++i) {
        snprintf(path, sizeof path, "%s/%s", argv[1], valid[i]);
        SDL_Surface *image = mainui_artwork_load(path);
        assert(image);
        SDL_FreeSurface(image);
    }
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        snprintf(path, sizeof path, "%s/%s", argv[1], invalid[i]);
        assert(!mainui_artwork_load(path));
    }
    MainUICatalog *catalog = calloc(1, sizeof *catalog);
    MainUIPreview *preview = calloc(1, sizeof *preview);
    assert(catalog && preview);
    snprintf(catalog->sd, sizeof catalog->sd, "%s", argv[1]);
    snprintf(path, sizeof path, "%s/bomb.png", argv[1]);
    MainUIEntry entry = {.path = path, .artwork = path};
    catalog->depth = 1;
    catalog->pages[1].entries = &entry;
    catalog->pages[1].count = 1;
    mainui_preview_update(preview, catalog, NULL, 0);
    assert(!preview->image && !preview->pending && preview->clock);
    mainui_preview_close(preview);
    mainui_preview_request(preview, catalog, NULL, 0, false);
    assert(preview->thread);
    Uint32 started = SDL_GetTicks();
    while (preview->thread) {
        assert(SDL_GetTicks() - started < 5000);
        SDL_Delay(1);
        mainui_preview_request(preview, catalog, NULL, 0, false);
    }
    assert(!preview->image && !preview->pending && preview->clock);
    mainui_preview_close(preview);
    free(preview);
    free(catalog);

    MainUITheme theme = {0};
    snprintf(theme.directory, sizeof theme.directory, "%s", argv[1]);
    assert(!mainui_theme_detail_default(&theme));
    assert(theme.detail_default_loaded);
    mainui_theme_close(&theme);

    snprintf(theme.directory, sizeof theme.directory, "%s", argv[1]);
    snprintf(theme.fallback, sizeof theme.fallback, "%s", argv[1]);
    /* Two retained 2000x2000 surfaces (about 30.5 MiB) leave too little of the
     * 32 MiB theme budget for a 1000x1000 image (about 3.8 MiB); one leaves room. */
    theme.background =
        SDL_CreateRGBSurface(SDL_SWSURFACE, 2000, 2000, 32, 0xff0000, 0xff00, 0xff, 0xff000000);
    theme.loading_background =
        SDL_CreateRGBSurface(SDL_SWSURFACE, 2000, 2000, 32, 0xff0000, 0xff00, 0xff, 0xff000000);
    assert(theme.background && theme.loading_background);
    assert(!mainui_theme_image(&theme, "theme-big.png"));
    SDL_FreeSurface(theme.background);
    theme.background = NULL;
    theme.background = mainui_theme_image(&theme, "theme-big.png");
    assert(theme.background);
    mainui_theme_close(&theme);

    MainUINameLookup names = {0};
    const char *maps[] = {"MapBytes", "MapLines"};
    for (size_t i = 0; i < sizeof maps / sizeof *maps; ++i) {
        snprintf(path, sizeof path, "%s/%s", argv[1], maps[i]);
        assert(!strcmp(mainui_name_lookup(&names, path, "alpha", "fallback"), "fallback"));
        mainui_names_close(&names);
    }
    SDL_Quit();
    puts("Image decode bounds, preview paths, theme rejection and name-map limits passed");
    return 0;
}
