#include "sys/ioctl.h"
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "system/keymap_sw.h"
#include "system/lang.h"
#include "system/settings.h"
#include "utils/msleep.h"

#include "installTheme.h"
#include "theme/render/canvas_ui.h"

static bool quit = false;

#define PREVIEW_CACHE_SIZE 5

typedef struct {
    int theme_index;
    unsigned long last_used;
    SDL_Surface *surface;
} PreviewCacheEntry;

static unsigned long preview_cache_clock = 0;

static bool resolvePreviewPath(const char *theme_name, char *path,
                               size_t path_size)
{
    snprintf(path, path_size, THEMES_DIR "/%s/preview.png", theme_name);
    if (is_file(path))
        return true;

    snprintf(path, path_size, THEMES_DIR "/.previews/%s/preview.png",
             theme_name);
    return is_file(path);
}

static SDL_Surface *loadPreviewSurface(const char *theme_name)
{
    char path[STR_MAX * 2];
    if (!resolvePreviewPath(theme_name, path, sizeof(path)))
        return NULL;

    SDL_Surface *preview = IMG_Load(path);
    if (preview == NULL) {
        printf_debug("Could not load theme preview: %s (%s)\n", path,
                     IMG_GetError());
    }

    // Keep the IMG_Load software surface. Converting it with
    // SDL_DisplayFormat[Alpha]() flips previews on the Miyoo framebuffer.
    return preview;
}

static void previewCacheInit(PreviewCacheEntry cache[PREVIEW_CACHE_SIZE])
{
    for (int i = 0; i < PREVIEW_CACHE_SIZE; i++) {
        cache[i].theme_index = -1;
        cache[i].last_used = 0;
        cache[i].surface = NULL;
    }
}

static SDL_Surface *previewCacheGet(
    PreviewCacheEntry cache[PREVIEW_CACHE_SIZE],
    const char themes[NUMBER_OF_THEMES][STR_MAX], int theme_index)
{
    preview_cache_clock++;

    for (int i = 0; i < PREVIEW_CACHE_SIZE; i++) {
        if (cache[i].theme_index == theme_index) {
            cache[i].last_used = preview_cache_clock;
            return cache[i].surface;
        }
    }

    int slot = 0;
    for (int i = 0; i < PREVIEW_CACHE_SIZE; i++) {
        if (cache[i].theme_index < 0) {
            slot = i;
            break;
        }
        if (cache[i].last_used < cache[slot].last_used)
            slot = i;
    }

    if (cache[slot].surface != NULL)
        SDL_FreeSurface(cache[slot].surface);

    cache[slot].theme_index = theme_index;
    cache[slot].last_used = preview_cache_clock;
    cache[slot].surface = loadPreviewSurface(themes[theme_index]);
    return cache[slot].surface;
}

static void previewCacheFree(PreviewCacheEntry cache[PREVIEW_CACHE_SIZE])
{
    for (int i = 0; i < PREVIEW_CACHE_SIZE; i++) {
        if (cache[i].surface != NULL)
            SDL_FreeSurface(cache[i].surface);
    }
}

/* New UI: the preview at 360x270, kept for the theme it was made for.
 *
 * The preview.png is first copied to 32-bit RGB with a plain blit, which
 * reads any PNG IMG_Load gives (paletted, RGB, RGBA): SDL_SoftStretch needs
 * the same format on both sides, and a paletted preview (PAC-MIYOO, Super
 * Onion Entertainment System) came out black. The whole image is then
 * fitted in 360x270 with a bicubic filter (Catmull-Rom, widened when it
 * shrinks, so every source pixel counts): SoftStretch picked one pixel in
 * four and lost the fine lines of detailed previews. */
static SDL_Surface *scaled_preview = NULL;
static int scaled_preview_page = -1;

#define PREVIEW_W 360
#define PREVIEW_H 270

static float cubicWeight(float x)
{
    const float a = -0.5f;
    x = fabsf(x);
    if (x < 1.0f)
        return ((a + 2.0f) * x - (a + 3.0f)) * x * x + 1.0f;
    if (x < 2.0f)
        return ((a * x - 5.0f * a) * x + 8.0f * a) * x - 4.0f * a;
    return 0.0f;
}

/* For each of out_n outputs: the first input it reads, how many, and their
 * weights (normalised), into *first, *count and w[i * max_taps]. */
static int cubicTaps(int in_n, int out_n, int *first, int *count, float **weights)
{
    float scale = (float)in_n / (float)out_n;
    float filter = scale > 1.0f ? scale : 1.0f;
    float support = 2.0f * filter;
    int max_taps = (int)ceilf(support) * 2 + 1;
    float *w = calloc((size_t)out_n * (size_t)max_taps, sizeof(float));
    if (w == NULL)
        return 0;
    for (int i = 0; i < out_n; i++) {
        float center = (i + 0.5f) * scale;
        int lo = (int)(center - support + 0.5f), hi = (int)(center + support + 0.5f);
        if (lo < 0)
            lo = 0;
        if (hi > in_n)
            hi = in_n;
        if (hi - lo > max_taps)
            hi = lo + max_taps;
        float sum = 0.0f;
        for (int j = lo; j < hi; j++) {
            float v = cubicWeight((j + 0.5f - center) / filter);
            w[i * max_taps + (j - lo)] = v;
            sum += v;
        }
        if (sum != 0.0f)
            for (int j = 0; j < hi - lo; j++)
                w[i * max_taps + j] /= sum;
        first[i] = lo;
        count[i] = hi - lo;
    }
    *weights = w;
    return max_taps;
}

static Uint8 clampChannel(float v)
{
    return v <= 0.0f ? 0 : v >= 255.0f ? 255 : (Uint8)(v + 0.5f);
}

/* src (32-bit 0xRRGGBB) to dw x dh at dst + (dx, dy). */
static void resampleBicubic(SDL_Surface *src, SDL_Surface *dst, int dx, int dy, int dw, int dh)
{
    int sw = src->w, sh = src->h;
    int *fx = malloc(sizeof(int) * dw), *nx = malloc(sizeof(int) * dw);
    int *fy = malloc(sizeof(int) * dh), *ny = malloc(sizeof(int) * dh);
    float *wx = NULL, *wy = NULL, *rows = malloc(sizeof(float) * 3 * (size_t)dw * (size_t)sh);
    int tx = 0, ty = 0;
    if (fx && nx && fy && ny && rows) {
        tx = cubicTaps(sw, dw, fx, nx, &wx);
        ty = cubicTaps(sh, dh, fy, ny, &wy);
    }
    if (tx && ty) {
        /* across: each source row to dw columns */
        for (int y = 0; y < sh; y++) {
            const Uint32 *in = (const Uint32 *)((const Uint8 *)src->pixels + y * src->pitch);
            float *out = rows + 3 * (size_t)dw * y;
            for (int x = 0; x < dw; x++) {
                float r = 0, g = 0, b = 0;
                const float *w = wx + x * tx;
                for (int k = 0; k < nx[x]; k++) {
                    Uint32 p = in[fx[x] + k];
                    r += w[k] * ((p >> 16) & 0xFF);
                    g += w[k] * ((p >> 8) & 0xFF);
                    b += w[k] * (p & 0xFF);
                }
                out[3 * x] = r, out[3 * x + 1] = g, out[3 * x + 2] = b;
            }
        }
        /* down: each output row from those */
        for (int y = 0; y < dh; y++) {
            Uint32 *o = (Uint32 *)((Uint8 *)dst->pixels + (dy + y) * dst->pitch) + dx;
            const float *w = wy + y * ty;
            for (int x = 0; x < dw; x++) {
                float r = 0, g = 0, b = 0;
                for (int k = 0; k < ny[y]; k++) {
                    const float *p = rows + 3 * ((size_t)dw * (fy[y] + k) + x);
                    r += w[k] * p[0];
                    g += w[k] * p[1];
                    b += w[k] * p[2];
                }
                o[x] = ((Uint32)clampChannel(r) << 16) | ((Uint32)clampChannel(g) << 8) | clampChannel(b);
            }
        }
    }
    free(fx);
    free(nx);
    free(fy);
    free(ny);
    free(wx);
    free(wy);
    free(rows);
}

static SDL_Surface *scaledPreview(SDL_Surface *preview, int page)
{
    if (scaled_preview_page == page && scaled_preview != NULL)
        return scaled_preview;
    if (scaled_preview)
        SDL_FreeSurface(scaled_preview);
    scaled_preview = NULL;
    scaled_preview_page = page;
    if (preview == NULL || preview->w <= 0 || preview->h <= 0)
        return NULL;

    /* any PNG to 32-bit RGB, on the panel grey (for transparent parts) */
    SDL_Surface *rgb = SDL_CreateRGBSurface(SDL_SWSURFACE, preview->w, preview->h, 32,
                                            0x00FF0000, 0x0000FF00, 0x000000FF, 0);
    scaled_preview = SDL_CreateRGBSurface(SDL_SWSURFACE, PREVIEW_W, PREVIEW_H, 32,
                                          0x00FF0000, 0x0000FF00, 0x000000FF, 0);
    if (rgb == NULL || scaled_preview == NULL) {
        if (rgb)
            SDL_FreeSurface(rgb);
        if (scaled_preview)
            SDL_FreeSurface(scaled_preview);
        scaled_preview = NULL;
        return NULL;
    }
    SDL_FillRect(rgb, NULL, CUI_PANEL);
    SDL_BlitSurface(preview, NULL, rgb, NULL);
    SDL_FillRect(scaled_preview, NULL, CUI_PANEL);

    /* the whole preview, fitted and centred (they are 4:3, 480x360) */
    int dw = PREVIEW_W, dh = PREVIEW_W * rgb->h / rgb->w;
    if (dh > PREVIEW_H) {
        dh = PREVIEW_H;
        dw = PREVIEW_H * rgb->w / rgb->h;
    }
    if (dw < 1)
        dw = 1;
    if (dh < 1)
        dh = 1;
    SDL_LockSurface(rgb);
    SDL_LockSurface(scaled_preview);
    resampleBicubic(rgb, scaled_preview, (PREVIEW_W - dw) / 2, (PREVIEW_H - dh) / 2, dw, dh);
    SDL_UnlockSurface(scaled_preview);
    SDL_UnlockSurface(rgb);
    SDL_FreeSurface(rgb);
    return scaled_preview;
}

/* The whole screen: header, the preview on the left with its page, the
 * theme's details on the right, and the footer. details: the install page
 * (A installs, X switches the icons). */
static void drawThemes(SDL_Surface *screen, SDL_Surface *preview, int page, int count,
                       const Theme_s *theme, bool installed, bool is_preview, bool has_icons,
                       bool details, bool apply_icons)
{
    CuiCanvas c = {(uint32_t *)screen->pixels, screen->pitch / 4};
    cui_rect(&c, 0, 0, CUI_W, CUI_H, CUI_BG);
    cui_header(&c, "palette", "Themes", installed ? "In use" : NULL, -1);

    /* preview, framed in the accent when it is the one being installed */
    cui_rrect(&c, 13, 73, 366, 276, 12, details ? CUI_ACCENT : CUI_TRACK);
    cui_rrect(&c, 16, 76, 360, 270, 10, CUI_PANEL);
    SDL_Surface *shown = scaledPreview(preview, page);
    if (shown) {
        SDL_Rect r = {16, 76};
        SDL_BlitSurface(shown, NULL, screen, &r);
    }
    else
        cui_text(&c, 196, 211, "No preview", 18, CUI_GREY, CUI_CENTER);

    /* page: arrows and "3 of 7" under the preview */
    char pages[32];
    snprintf(pages, sizeof(pages), "%d of %d", page + 1, count);
    cui_text(&c, 196, 374, pages, 17, CUI_GREY, CUI_CENTER);
    if (page > 0)
        cui_chevron(&c, 130, 374, -1, CUI_WHITE);
    if (page < count - 1)
        cui_chevron(&c, 262, 374, 1, CUI_WHITE);

    /* details on the right */
    int x = 396, w = 228;
    char line[STR_MAX * 2];
    cui_text(&c, x, 88, "THEME", 12, CUI_GREY, CUI_LEFT);
    cui_fit(theme->name, 22, w, line, sizeof(line));
    cui_text(&c, x, 116, line, 22, CUI_WHITE, CUI_LEFT);
    if (strlen(theme->author) > 0) {
        char author[STR_MAX * 2];
        snprintf(author, sizeof(author), "by %s", theme->author);
        cui_fit(author, 16, w, line, sizeof(line));
        cui_text(&c, x, 144, line, 16, CUI_GREY, CUI_LEFT);
    }
    cui_rrect(&c, x - 4, 168, w + 8, 96, 10, CUI_RGB(38, 38, 41));
    cui_text(&c, x + 8, 190, has_icons ? "Icons: included" : "Icons: none", 15, CUI_GREY, CUI_LEFT);
    cui_text(&c, x + 8, 214, installed ? "Installed: in use" : "Installed: no", 15, CUI_GREY, CUI_LEFT);
    cui_text(&c, x + 8, 238, is_preview ? "Kept as an archive" : "Ready on the card", 15, CUI_GREY, CUI_LEFT);

    if (details) {
        /* the icons switch, as a row */
        cui_rrect(&c, x - 4, 284, w + 8, 46, 10, CUI_ACCENT);
        cui_text(&c, x + 8, 307, has_icons ? "Apply its icons" : "Reset the icons", 18, CUI_WHITE, CUI_LEFT);
        int sx = x + w - 44;
        cui_rrect(&c, sx, 296, 40, 22, 11, apply_icons ? CUI_WHITE : cui_mix(CUI_ACCENT, CUI_WHITE, 0.3f));
        cui_disc(&c, apply_icons ? sx + 29 : sx + 11, 307, 8, apply_icons ? CUI_ACCENT : CUI_GREY);
    }

    cui_rect(&c, 0, 428, CUI_W, 52, CUI_HEAD);
    int hx = 16;
    if (details) {
        hx += cui_hint(&c, hx, 454, "A", "INSTALL", 20);
        hx += cui_hint(&c, hx, 454, "X", "ICONS", 20);
        cui_hint(&c, hx, 454, "B", "BACK", 20);
    }
    else {
        hx += cui_hint(&c, hx, 454, "A", "SELECT", 20);
        cui_rrect(&c, hx, 442, 36, 24, 6, CUI_TRACK);
        cui_chevron(&c, hx + 10, 454, -1, CUI_WHITE);
        cui_chevron(&c, hx + 26, 454, 1, CUI_WHITE);
        hx += 36 + 7 + cui_text(&c, hx + 43, 454, "BROWSE", 20, CUI_WHITE, CUI_LEFT) + 20;
        cui_hint(&c, hx, 454, "B", "EXIT", 20);
    }
}

/* Loading and installing: the header and one line. */
static void drawThemesMessage(SDL_Surface *video, SDL_Surface *screen, const char *message)
{
    CuiCanvas c = {(uint32_t *)screen->pixels, screen->pitch / 4};
    cui_rect(&c, 0, 0, CUI_W, CUI_H, CUI_BG);
    cui_header(&c, "palette", "Themes", "Working", -1);
    cui_text(&c, 320, 240, message, 22, CUI_GREY, CUI_CENTER);
    SDL_BlitSurface(screen, NULL, video, NULL);
    SDL_Flip(video);
}

int main(int argc, char *argv[])
{
    settings_load();
    const char *installed_theme = basename(settings.theme);

    bool reapply = false;
    bool reapply_icons = false;
    bool update_previews = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--reapply") == 0) {
            reapply = true;
        }
        else if (strcmp(argv[i], "--reapply_icons") == 0) {
            reapply = true;
            reapply_icons = true;
        }
        else if (strcmp(argv[i], "--update") == 0) {
            reapply = true;
            update_previews = true;
        }
    }

    if (reapply) {
        reinstallTheme(installed_theme, reapply_icons, update_previews);
        return 0;
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_ShowCursor(SDL_DISABLE);
    SDL_EnableKeyRepeat(300, 50);
    TTF_Init();

    SDL_Surface *video = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE);
    SDL_Surface *screen =
        SDL_CreateRGBSurface(SDL_HWSURFACE, 640, 480, 32, 0, 0, 0, 0);

    int levelPage = 0;

    drawThemesMessage(video, screen, "Loading themes...");

    char themes[NUMBER_OF_THEMES][STR_MAX];
    int installed_page = 0;
    int themes_count = listAllThemes(themes, installed_theme, &installed_page);
    int current_page = installed_page;

    PreviewCacheEntry preview_cache[PREVIEW_CACHE_SIZE];
    previewCacheInit(preview_cache);

    SDL_Event event;
    Uint8 keystate[320] = {0};
    bool changed = true;

    Theme_s theme;
    char icon_pack_path[STR_MAX + 32];
    bool has_icons = false;
    bool apply_icons = false;
    bool is_preview = false;

    bool page_changed = true;
    bool render_dirty = true;

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            SDLKey key = event.key.keysym.sym;
            switch (event.type) {
            case SDL_QUIT:
                quit = true;
                break;
            case SDL_KEYDOWN:
                keystate[key] = keystate[key] != 0 ? 2 : 1;
                changed = true;
                break;
            case SDL_KEYUP:
                keystate[key] = 0;
                changed = true;
                break;
            default:
                break;
            }
        }

        if (!changed) {
            SDL_Delay(15); // wait for input without spinning
            continue;
        }

        if (keystate[SW_BTN_MENU] && levelPage == 0)
            quit = true;

        if (keystate[SW_BTN_B]) {
            if (levelPage == 0)
                quit = true; // exit program
            else
                levelPage = 0;

            render_dirty = true;
        }

        if (keystate[SW_BTN_A]) {
            if (levelPage == 1) {
                drawThemesMessage(video, screen, "Installing the theme...");

                // Install theme
                installTheme(theme.path, apply_icons);
                printf_debug("Theme installed: %s\n", themes[current_page]);

                quit = true;
            }
            else {
                // Go to theme details/confirmation page
                levelPage = 1;
            }
            render_dirty = true;
        }

        if (levelPage == 1 && keystate[SW_BTN_X]) {
            apply_icons = !apply_icons;
            render_dirty = true;
        }

        if (levelPage == 0) {
            if (keystate[SW_BTN_RIGHT]) {
                if (current_page < themes_count - 1) {
                    current_page++;
                    page_changed = true;
                }
            }
            if (keystate[SW_BTN_LEFT]) {
                if (current_page > 0) {
                    current_page--;
                    page_changed = true;
                }
            }
        }

        if (quit)
            break;

        if (page_changed) {
            loadTheme(themes[current_page], &theme);
            snprintf(icon_pack_path, STR_MAX + 32 - 1, "%sicons", theme.path);
            apply_icons = true;
            has_icons = is_dir(icon_pack_path);

            if (strstr(theme.path, "/.previews/") != NULL) {
                is_preview = true;
            }
            else {
                is_preview = false;
            }

            page_changed = false;
            render_dirty = true;
        }

        if (!render_dirty) {
            // Nothing to draw for this input (e.g. a key release): clear
            // `changed` too, or the loop skips the idle delay above and
            // spins at 100% CPU until the next redraw.
            changed = false;
            continue;
        }

        drawThemes(screen, previewCacheGet(preview_cache, themes, current_page), current_page,
                   themes_count, &theme, current_page == installed_page && !is_preview, is_preview,
                   has_icons, levelPage == 1, apply_icons);

        SDL_BlitSurface(screen, NULL, video, NULL);
        SDL_Flip(video);

        changed = false;
        render_dirty = false;
    }

    msleep(100);

    previewCacheFree(preview_cache);
    if (scaled_preview)
        SDL_FreeSurface(scaled_preview);

    SDL_Quit();

    return EXIT_SUCCESS;
}
