/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/menu_view.h"
#include "menus/console_view.h"
#include "platform/timing.h"
#include "ui/image_scale.h"
#include "ui/panels.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void blit(SDL_Surface *screen, SDL_Surface *image, int x, int y)
{
    SDL_Rect destination = {(Sint16)x, (Sint16)y, 0, 0};
    if (image) {
        SDL_BlitSurface(image, NULL, screen, &destination);
    }
}

static void text(MainUITheme *theme, SDL_Surface *screen, TTF_Font *font, SDL_Color color,
                 const char *label, int x, int y, int width, int height)
{
    SDL_Surface *image = mainui_theme_text(theme, font, label, color);
    SDL_Rect clip = {(Sint16)x, (Sint16)y, (Uint16)width, (Uint16)height};
    SDL_SetClipRect(screen, &clip);
    if (image) {
        blit(screen, image, x + (width - image->w) / 2, y + (height - image->h) / 2);
    }
    SDL_SetClipRect(screen, NULL);
}

static void frame(MainUITheme *theme, SDL_Surface *screen, const char *title, int page, int pages)
{
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 24, 24, 24));
    blit(screen, theme->background, 0, 0);
    mainui_draw_header(screen, theme, title);
    mainui_draw_footer(screen, theme, page, pages);
}

static void clear_consoles(MainUIMenuView *view)
{
    for (int i = 0; i < 9; i++) {
        /* The selected icon may share the normal one's surface: free it once. */
        SDL_Surface *normal = view->console_icons[i][0], *selected = view->console_icons[i][1];
        if (selected && selected != normal) {
            SDL_FreeSurface(selected);
        }
        if (normal) {
            SDL_FreeSurface(normal);
        }
        free(view->pending_selected[i]);
        view->pending_selected[i] = NULL;
        for (int state = 0; state < 2; state++) {
            if (view->console_labels[i][state]) {
                SDL_FreeSurface(view->console_labels[i][state]);
            }
            view->console_icons[i][state] = NULL;
            view->console_labels[i][state] = NULL;
        }
    }
}

static void free_carousel_slot(MainUICarouselSlot *slot)
{
    if (slot->small) {
        SDL_FreeSurface(slot->small);
    }
    if (slot->selected_icon && slot->selected_icon != slot->icon) {
        SDL_FreeSurface(slot->selected_icon);
    }
    if (slot->icon) {
        SDL_FreeSurface(slot->icon);
    }
    if (slot->label) {
        SDL_FreeSurface(slot->label);
    }
    if (slot->count) {
        SDL_FreeSurface(slot->count);
    }
    *slot = (MainUICarouselSlot){.index = -1};
}

static void clear_carousel(MainUIMenuView *view)
{
    mainui_carousel_worker_close(&view->carousel_worker);
    for (int i = 0; i < MAINUI_CAROUSEL_CACHE_SIZE; i++) {
        free_carousel_slot(&view->carousel_cache[i]);
        view->carousel_age[i] = 0;
    }
    for (int i = 0; i < MAINUI_CAROUSEL_SLOTS; i++) {
        view->carousel[i] = (MainUICarouselSlot){.index = -1};
        view->carousel_visible[i] = -1;
    }
    if (view->carousel_tile) {
        SDL_FreeSurface(view->carousel_tile);
    }
    view->carousel_tile = NULL;
    view->carousel_tile_source = NULL;
    view->carousel_clock = 0;
    view->carousel_revealed = false;
    view->carousel_catalog = NULL;
    view->carousel_entries = NULL;
}

/* Menu icons are decoded at full size, so a large icon pack could hold far
 * more memory than the screen shows. All of them together stay within this
 * budget; Expert icons keep only the part Expert draws. */
#define MENU_ICON_BUDGET (24u * 1024u * 1024u)

static size_t surface_bytes(const SDL_Surface *surface)
{
    return surface ? (size_t)surface->pitch * (size_t)surface->h : 0;
}

size_t mainui_menu_view_bytes(const MainUIMenuView *view)
{
    size_t bytes = 0;
    for (int i = 0; i < MAINUI_MENU_SECTIONS; i++) {
        bytes += surface_bytes(view->home_icons[i][0]) + surface_bytes(view->home_icons[i][1]);
    }
    for (int i = 0; i < 9; i++) {
        bytes += surface_bytes(view->console_icons[i][0]);
        if (view->console_icons[i][1] != view->console_icons[i][0]) {
            bytes += surface_bytes(view->console_icons[i][1]);
        }
    }
    bytes += surface_bytes(view->carousel_tile);
    for (int i = 0; i < MAINUI_CAROUSEL_CACHE_SIZE; i++) {
        bytes += surface_bytes(view->carousel_cache[i].icon) +
                 surface_bytes(view->carousel_cache[i].label) +
                 surface_bytes(view->carousel_cache[i].count) +
                 surface_bytes(view->carousel_cache[i].small);
        if (view->carousel_cache[i].selected_icon != view->carousel_cache[i].icon) {
            bytes += surface_bytes(view->carousel_cache[i].selected_icon);
        }
    }
    return bytes;
}

/* The centered part of `image` that fits width x height, as a copy with the
 * same pixel format, transparency and palette; the original is freed. On
 * failure the original is returned. */
static SDL_Surface *crop(SDL_Surface *image, int width, int height)
{
    int w = image->w < width ? image->w : width, h = image->h < height ? image->h : height;
    if (w == image->w && h == image->h) {
        return image;
    }
    SDL_PixelFormat *format = image->format;
    SDL_Surface *out =
        SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, format->BitsPerPixel, format->Rmask,
                             format->Gmask, format->Bmask, format->Amask);
    if (!out || SDL_LockSurface(image)) {
        if (out) {
            SDL_FreeSurface(out);
        }
        return image;
    }
    int x = (image->w - w) / 2, y = (image->h - h) / 2, bytes = format->BytesPerPixel;
    for (int row = 0; row < h; row++) {
        memcpy((Uint8 *)out->pixels + row * out->pitch,
               (const Uint8 *)image->pixels + (y + row) * image->pitch + x * bytes,
               (size_t)(w * bytes));
    }
    SDL_UnlockSurface(image);
    if (format->palette) {
        SDL_SetColors(out, format->palette->colors, 0, format->palette->ncolors);
    }
    if (image->flags & SDL_SRCCOLORKEY) {
        SDL_SetColorKey(out, SDL_SRCCOLORKEY, format->colorkey);
    }
    SDL_SetAlpha(out, image->flags & SDL_SRCALPHA, format->alpha);
    SDL_FreeSurface(image);
    return out;
}

/* Downsize a decoded thumbnail once, freeing the original only if the
 * software resampling succeeds. */
static SDL_Surface *fit(SDL_Surface *image, int width, int height)
{
    SDL_Surface *scaled = mainui_image_fit(image, width, height);
    if (scaled) {
        SDL_FreeSurface(image);
        return scaled;
    }
    return image;
}

/* Icons dropped for the budget, with the bytes they needed, for the session:
 * a page change does not decode one again while it still cannot fit. A full
 * table only means an icon may be decoded once more before it is dropped. */
static struct {
    char *path;
    int width, height; /* the crop it was dropped with: Expert's or none */
    size_t bytes;
} dropped[32];

static bool over_budget(size_t retained, size_t needed)
{
    return retained > MENU_ICON_BUDGET || needed > MENU_ICON_BUDGET - retained;
}

static void report_budget(const char *path)
{
    static bool reported;
    if (!reported) {
        fprintf(stderr, "Menu icon budget exceeded by %s; showing no icon\n", path);
        reported = true;
    }
}

/* Cropped to width x height when they are set (Expert), or dropped (no icon)
 * over the budget. Other icons are kept whole: a large one is drawn clipped
 * by the screen, which no centered crop reproduces. */
static SDL_Surface *keep_icon(size_t retained, SDL_Surface *icon, int width, int height,
                              const char *path)
{
    if (!icon) {
        return NULL;
    }
    if (width > 0) {
        icon = crop(icon, width, height);
    }
    size_t needed = surface_bytes(icon);
    if (over_budget(retained, needed)) {
        report_budget(path);
        for (size_t i = 0; path && i < sizeof dropped / sizeof *dropped; i++) {
            if (!dropped[i].path && (dropped[i].path = strdup(path))) {
                dropped[i].width = width;
                dropped[i].height = height;
                dropped[i].bytes = needed;
                break;
            }
            if (dropped[i].path && !strcmp(dropped[i].path, path) && dropped[i].width == width &&
                dropped[i].height == height) {
                dropped[i].bytes = needed;
                break;
            }
        }
        SDL_FreeSurface(icon);
        return NULL;
    }
    return icon;
}

/* Thumbnails are scaled down to fit width x height; icons are cropped to it
 * when it is set, as keep_icon() does. */
static SDL_Surface *load_icon(MainUITheme *theme, const char *path, size_t retained, int width,
                              int height, bool scaled)
{
    if (!path || !*path) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof dropped / sizeof *dropped && dropped[i].path; i++) {
        if (!strcmp(dropped[i].path, path) && dropped[i].width == width &&
            dropped[i].height == height && over_budget(retained, dropped[i].bytes)) {
            report_budget(path);
            return NULL;
        }
    }
    struct timespec start = mainui_timing_start();
    SDL_Surface *icon = mainui_theme_console_icon(theme, path);
    mainui_timing_finish("icon-ms", start);
    if (icon && scaled) {
        struct timespec scale_start = mainui_timing_start();
        icon = fit(icon, width, height);
        mainui_timing_finish("carousel-scale-ms", scale_start);
    }
    return keep_icon(retained, icon, width, height, path);
}

SDL_Surface *mainui_menu_view_icon(MainUITheme *theme, const char *path, size_t retained, int width,
                                   int height)
{
    return load_icon(theme, path, retained, width, height, false);
}

void mainui_menu_view_open(MainUIMenuView *view, MainUITheme *theme)
{
    *view = (MainUIMenuView){.theme = theme, .cached_start = -1, .slid_to = -1};
    clear_carousel(view);
    for (int i = 0; i < MAINUI_MENU_SECTIONS; i++) {
        for (int selected = 0; selected < 2; selected++) {
            char name[80];
            snprintf(name, sizeof name, "skin/ic-%s-%c.png", mainui_menu_icon(i),
                     selected ? 'f' : 'n');
            view->home_icons[i][selected] = keep_icon(mainui_menu_view_bytes(view),
                                                      mainui_theme_image(theme, name), 0, 0, name);
        }
    }
}

void mainui_menu_view_close(MainUIMenuView *view)
{
    clear_consoles(view);
    clear_carousel(view);
    if (view->selection) {
        SDL_FreeSurface(view->selection);
    }
    for (int i = 0; i < MAINUI_MENU_SECTIONS; i++) {
        for (int selected = 0; selected < 2; selected++) {
            if (view->home_icons[i][selected]) {
                SDL_FreeSurface(view->home_icons[i][selected]);
            }
        }
    }
    *view = (MainUIMenuView){0};
}

/* The patch constrains home labels to a 136px lane, including when fewer
 * cards are visible. Keep two lines centered around the original label slot. */
static void home_label(MainUITheme *theme, SDL_Surface *screen, const char *label, SDL_Color color,
                       int x, int width)
{
    int measured = 0;
    TTF_SizeUTF8(theme->grid_font, label, &measured, NULL);
    int lane = (width < 156 ? width : 156) - 20;
    x += (width - lane) / 2;
    if (measured <= lane) {
        text(theme, screen, theme->grid_font, color, label, x, 260, lane, 50);
        return;
    }
    char first[256], second[256];
    snprintf(first, sizeof first, "%s", label);
    size_t split = strlen(first);
    while (split > 0) {
        split--;
        if (((unsigned char)first[split] & 0xc0) == 0x80) {
            continue;
        }
        char saved = first[split];
        first[split] = 0;
        TTF_SizeUTF8(theme->grid_font, first, &measured, NULL);
        first[split] = saved;
        if (measured <= lane) {
            break;
        }
    }
    for (size_t word = split; word > 0; word--) {
        if (first[word] == ' ') {
            split = word;
            break;
        }
    }
    snprintf(second, sizeof second, "%s", label + split + (label[split] == ' '));
    first[split] = 0;
    int height = TTF_FontHeight(theme->grid_font);
    int spacing = (3 * height + 3) / 4;
    text(theme, screen, theme->grid_font, color, first, x, 285 - spacing / 2 - height / 2, lane,
         height);
    text(theme, screen, theme->grid_font, color, second, x, 285 + spacing / 2 - height / 2, lane,
         height);
}

void mainui_menu_draw_home(MainUIMenuView *view, SDL_Surface *screen, const MainUIMenu *menu,
                           const MainUIViewport *position)
{
    MainUITheme *theme = view->theme;
    frame(theme, screen, "MIYOO", 0, -1);
    int columns = menu->count < 4 ? menu->count : 4;
    if (!columns) {
        return;
    }
    int width = 620 / columns;
    for (int i = position->start; i <= position->end; i++) {
        int selected = i == position->selected;
        int x = 10 + (i - position->start) * width;
        SDL_Surface *icon = view->home_icons[menu->sections[i]][selected];
        if (icon) {
            blit(screen, icon, x + (width - icon->w) / 2, 60 + (360 - icon->h) / 2 - 10);
        }
        if (!theme->hide_icons && !theme->hide_grid_text) {
            home_label(theme, screen, mainui_menu_label(menu->sections[i]),
                       theme->grid_color[selected], x, width);
        }
    }
    if (theme->dots[0]) {
        int stride = theme->dots[0]->w + 5;
        int x = (640 - menu->count * stride) / 2;
        for (int i = 0; i < menu->count; i++) {
            blit(screen, theme->dots[i == position->selected], x + i * stride, 390);
        }
    }
}

void mainui_menu_view_page(MainUIMenuView *view, MainUICatalog *catalog,
                           const MainUIViewport *position)
{
    if (view->cached_start == position->start) {
        return;
    }
    MainUITheme *theme = view->theme;
    bool expert = !strcmp(catalog->pages[0].title, "Expert");
    int capacity = expert ? 9 : 8;
    /* Expert draws the centered 192x72 of an icon. */
    int width = expert ? 192 : 0, height = expert ? 72 : 0;
    clear_consoles(view);
    /* Grid pages have their own cache. Leaving the carousel must not evict
     * decoded carousel images: they remain reusable on re-entry. */
    view->crop_width = width;
    view->crop_height = height;
    for (int i = 0; i < capacity && position->start + i < position->total; i++) {
        MainUIEntry *entry = mainui_catalog_entry(catalog, position->start + i);
        if (!entry) {
            continue;
        }
        view->console_icons[i][0] =
            mainui_menu_view_icon(theme, entry->icon, mainui_menu_view_bytes(view), width, height);
        /* One file for both states is decoded once. */
        bool same =
            !entry->icon_selected || (entry->icon && !strcmp(entry->icon_selected, entry->icon));
        /* Until it is decoded, a selected icon left for later shows as the
         * normal one; the shared surface is freed once, as for one file. */
        if (!same && view->defer_selected && position->start + i != position->selected) {
            view->pending_selected[i] = strdup(entry->icon_selected);
            same = view->pending_selected[i] != NULL;
        }
        view->console_icons[i][1] =
            same ? view->console_icons[i][0]
                 : mainui_menu_view_icon(theme, entry->icon_selected, mainui_menu_view_bytes(view),
                                         width, height);
        if (!view->console_icons[i][1]) {
            view->console_icons[i][1] = view->console_icons[i][0];
        }
        for (int selected = 0; selected < 2; selected++) {
            /* Expert has its own label font (theme.c); grid3x4 = 0 hides it. */
            bool hidden = expert ? theme->hide_expert_text : theme->hide_grid_text;
            TTF_Font *font = expert && theme->expert_font ? theme->expert_font : theme->grid_font;
            view->console_labels[i][selected] =
                hidden ? NULL
                       : TTF_RenderUTF8_Blended(font, entry->label, theme->grid_color[selected]);
        }
    }
    view->cached_start = position->start;
}

bool mainui_menu_view_load_pending(MainUIMenuView *view, const MainUIViewport *position,
                                   bool *shown)
{
    *shown = false;
    if (view->cached_start < 0 || view->cached_start != position->start) {
        return false;
    }
    int slot = position->selected - position->start;
    if (slot < 0 || slot >= 9 || !view->pending_selected[slot]) {
        slot = -1;
        for (int i = 0; i < 9 && slot < 0; i++) {
            if (view->pending_selected[i]) {
                slot = i;
            }
        }
    }
    if (slot < 0) {
        return false;
    }
    char *path = view->pending_selected[slot];
    view->pending_selected[slot] = NULL;
    SDL_Surface *icon = mainui_menu_view_icon(view->theme, path, mainui_menu_view_bytes(view),
                                              view->crop_width, view->crop_height);
    free(path);
    if (icon) {
        view->console_icons[slot][1] = icon;
        *shown = position->start + slot == position->selected;
    }
    return true;
}

void mainui_menu_draw_systems(MainUIMenuView *view, SDL_Surface *screen, MainUICatalog *catalog,
                              const MainUIViewport *position)
{
    MainUITheme *theme = view->theme;
    bool expert = !strcmp(catalog->pages[0].title, "Expert");
    int columns = expert ? 3 : 4, capacity = expert ? 9 : 8;
    int width = expert ? 213 : 155, height = expert ? 120 : 170;
    int label_offset = expert ? 28 : 45;
    frame(theme, screen,
          !strcmp(catalog->pages[0].title, "Expert") ? mainui_menu_label(MAINUI_MENU_EXPERT)
                                                     : mainui_menu_label(MAINUI_MENU_GAMES),
          position->total ? position->start / capacity + 1 : 0,
          (position->total + capacity - 1) / capacity);
    mainui_menu_view_page(view, catalog, position);
    /* Stock Games: 4x2, margins 10, 155x170 cells. Expert: 3x3, no margins,
     * 213x120 cells, centered 192x72 icon crop, label bottom offset 28. */
    for (int i = 0; i < capacity && position->start + i < position->total; i++) {
        int selected = position->start + i == position->selected;
        int x = (expert ? 0 : 10) + (i % columns) * width;
        int y = (expert ? 60 : 75) + (i / columns) * height;
        SDL_Surface *icon = view->console_icons[i][selected];
        if (expert) {
            /* Stock draws the mark 2 px right and down of the centered spot. */
            SDL_Surface *mark = selected ? theme->expert_selection : NULL;
            if (mark) {
                blit(screen, mark, x + (width - mark->w) / 2 + 2, y + (height - mark->h) / 2 + 2);
            }
        }
        else if (theme->tiles[selected]) {
            SDL_Surface *tile = theme->tiles[selected];
            blit(screen, tile, x + (width - tile->w) / 2 + 2, y + (height - tile->h) / 2 + 2);
        }
        if (icon) {
            if (expert) {
                int w = icon->w < 192 ? icon->w : 192, h = icon->h < 72 ? icon->h : 72;
                SDL_Rect source = {(Sint16)((icon->w - w) / 2), (Sint16)((icon->h - h) / 2),
                                   (Uint16)w, (Uint16)h};
                /* 1 px lower than the Games icons' rule, as stock. */
                SDL_Rect dest = {(Sint16)(x + (width - w) / 2), (Sint16)(y + (height - h) / 2 - 9),
                                 0, 0};
                SDL_BlitSurface(icon, &source, screen, &dest);
            }
            else {
                blit(screen, icon, x + (width - icon->w) / 2, y + (height - icon->h) / 2 - 10);
            }
        }
        SDL_Surface *label = view->console_labels[i][selected];
        if (label) {
            /* Expert labels sit 1 px lower than this rule gives, as stock's. */
            int label_y = y + height - label_offset + (18 - label->h) / 2 + (expert ? 1 : 0);
            SDL_Rect clip = {(Sint16)x, (Sint16)(y + height - label_offset - 15), (Uint16)width,
                             50};
            SDL_SetClipRect(screen, &clip);
            blit(screen, label, x + (width - label->w) / 2, label_y);
            SDL_SetClipRect(screen, NULL);
        }
    }
    if (!position->total) {
        mainui_draw_empty(screen, theme);
    }
}

/* The carousel: the selected console centred, its neighbours either side,
 * each at its own width with a gap between, the next ones cut by the
 * screen's edges. A console with a thumbnail shows it on a plain selection,
 * its neighbours smaller (scale_unselected); one without shows its theme icon
 * as a cell of the grid. Slots are by offset from the selection: slot
 * CENTRE_SLOT is the selected console, 0 the second to its left. */
enum {
    CENTRE_SLOT = MAINUI_CAROUSEL_SLOTS / 2,
    /* Thumbnails are scaled down to fit this: wide ones are fine, taller ones
     * would push the name off the screen. */
    THUMB_WIDTH = 560,
    THUMB_HEIGHT = 260,
    THUMB_HEIGHT_ALONE = 300 /* with the name or the number of games hidden */
};

/* The grid's cell for a theme icon, as mainui_menu_draw_systems() draws it:
 * Games 155x170, label 45 from the bottom; Expert 213x120, its icon cropped
 * to 192x72, label 28 from the bottom. */
typedef struct {
    int width, height, label_offset, icon_lift;
} CarouselCell;

static CarouselCell carousel_cell(bool expert)
{
    return expert ? (CarouselCell){213, 120, 28, 9} : (CarouselCell){155, 170, 45, 10};
}

/* Thumbnails' largest height and their middle in the list band 60..420, for
 * the lines shown beneath: the tallest with its selection above and the lines
 * beneath fill the band, or with none it is centred. */
static void carousel_layout(const MainUIConsoleView *console, TTF_Font *font, bool text_hidden,
                            int *height, int *centre)
{
    int lines = (!console->hide_labels && !text_hidden) + (!console->hide_count && !text_hidden);
    *height = lines == 2 ? THUMB_HEIGHT : THUMB_HEIGHT_ALONE;
    *centre = lines == 2 ? 212 : lines == 1 ? 222 : 240;
    /* The theme's font decides the lines' room: where the tallest picture,
     * its selection and the lines would not fit between the header and the
     * footer (60..416), the picture gets smaller and the whole is centred. */
    int pad = console->selection_padding;
    int text = lines && font ? 8 + lines * TTF_FontHeight(font) : 0;
    if (*centre - *height / 2 - pad < 60 || *centre + *height / 2 + pad + text > 416) {
        int room = 356 - 2 * pad - text;
        *height = room < *height ? (room > 40 ? room : 40) : *height;
        *centre = 60 + (356 - (2 * pad + *height + text)) / 2 + pad + *height / 2;
    }
    *centre += console->offset_y;
}

/* The catalog index at offset from selected, or -1: past the ends, or with
 * "wrap" round them, each console once. */
static void carousel_indices(int selected, int total, bool wrap, int direction,
                             int out[MAINUI_CAROUSEL_SLOTS])
{
    for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
        out[k] = -1;
    }
    /* Nearest first, so a short list fills both sides: right before left,
     * or left first after a move right, where the one left behind goes. */
    for (int step = 0; step <= 2 * CENTRE_SLOT; step++) {
        int offset = step % 2 ? (step + 1) / 2 : -(step / 2);
        offset = direction > 0 ? -offset : offset;
        int index = selected + offset;
        if (wrap && total > 0) {
            index = ((index % total) + total) % total;
        }
        if (index < 0 || index >= total) {
            continue;
        }
        bool used = false;
        for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
            used |= out[k] == index;
        }
        if (!used) {
            out[CENTRE_SLOT + offset] = index;
        }
    }
}

/* Resample directly from the cached icon, keeping it owned by the cache. */
/* A stable cache key survives JOB_DISCOVER rebuilding a catalog. Include the
 * artwork sources, console identity, and render settings, never an entry
 * pointer or index. No SD I/O or decoding is needed to compute it. */
static uint64_t hash_string(uint64_t hash, const char *value)
{
    if (!value) {
        value = "";
    }
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    }
    return (hash ^ 0xffu) * UINT64_C(1099511628211);
}

static bool carousel_job(MainUIMenuView *view, MainUICatalog *catalog,
                         const MainUIConsoleView *console, int height, int index,
                         MainUICarouselJob *job)
{
    MainUIEntry *entry = mainui_catalog_entry(catalog, index);
    if (!entry) {
        return false;
    }
    memset(job, 0, sizeof *job);
    bool expert = !strcmp(catalog->pages[0].title, "Expert");
    job->height = height;
    job->scale = console->scale_unselected;
    job->opacity = console->opacity_unselected;
    job->crop_width = expert ? 192 : 0;
    job->crop_height = expert ? 72 : 0;
    mainui_console_thumb(job->thumb, console->thumbs, entry);
    snprintf(job->icon, sizeof job->icon, "%s", entry->icon ? entry->icon : "");
    snprintf(job->selected, sizeof job->selected, "%s",
             entry->icon_selected ? entry->icon_selected : "");
    snprintf(job->fallback, sizeof job->fallback, "%s", view->theme->fallback);
    uint64_t hash = UINT64_C(14695981039346656037);
    hash = hash_string(hash, job->thumb);
    hash = hash_string(hash, job->icon);
    hash = hash_string(hash, job->selected);
    hash = hash_string(hash, job->fallback);
    hash = hash_string(hash, entry->config);
    hash = hash_string(hash, entry->path);
    hash = hash_string(hash, entry->label);
    hash = hash_string(hash, expert ? "expert" : "games");
    hash = (hash ^ (unsigned int)job->height) * UINT64_C(1099511628211);
    hash = (hash ^ (unsigned int)job->scale) * UINT64_C(1099511628211);
    hash = (hash ^ (unsigned int)job->opacity) * UINT64_C(1099511628211);
    job->key = hash ? hash : 1;
    return true;
}

/* LRU entries own their surfaces. Slots used for drawing are borrowed copies.
 * The current visible keys and any in-flight worker key are never evicted. */
static MainUICarouselSlot *carousel_cached(MainUIMenuView *view, uint64_t key, int index,
                                           bool create)
{
    if (!key) {
        return NULL;
    }
    for (int i = 0; i < MAINUI_CAROUSEL_CACHE_SIZE; i++) {
        if (view->carousel_cache[i].key == key) {
            if (index >= 0) {
                view->carousel_cache[i].index = index;
            }
            view->carousel_age[i] = ++view->carousel_clock;
            return &view->carousel_cache[i];
        }
    }
    if (!create) {
        return NULL;
    }
    int victim = -1;
    for (int i = 0; i < MAINUI_CAROUSEL_CACHE_SIZE; i++) {
        MainUICarouselSlot *item = &view->carousel_cache[i];
        if (!item->key) {
            victim = i;
            break;
        }
        bool protected = view->carousel_worker.thread && item->key == view->carousel_worker.job.key;
        for (int j = 0; j < MAINUI_CAROUSEL_SLOTS; j++) {
            protected |= item->key == view->carousel[j].key;
        }
        if (!protected && (victim < 0 || view->carousel_age[i] < view->carousel_age[victim])) {
            victim = i;
        }
    }
    if (victim < 0) {
        return NULL;
    }
    if (view->carousel_cache[victim].key) {
        mainui_count_add("carousel-cache-evict", 1);
    }
    free_carousel_slot(&view->carousel_cache[victim]);
    view->carousel_cache[victim].key = key;
    view->carousel_cache[victim].index = index;
    view->carousel_age[victim] = ++view->carousel_clock;
    return &view->carousel_cache[victim];
}

static void carousel_selected_text(MainUIMenuView *view, MainUICatalog *catalog,
                                   const MainUIConsoleView *console, MainUICarouselSlot *slot,
                                   int index)
{
    if (slot->loaded) {
        return;
    }
    slot->loaded = true;
    MainUIEntry *entry = mainui_catalog_entry(catalog, index);
    if (!entry) {
        return;
    }
    bool expert = !strcmp(catalog->pages[0].title, "Expert");
    MainUITheme *theme = view->theme;
    bool hidden = expert ? theme->hide_expert_text : theme->hide_grid_text;
    TTF_Font *font = expert && theme->expert_font ? theme->expert_font : theme->grid_font;
    slot->label = hidden || !font || console->hide_labels
                      ? NULL
                      : TTF_RenderUTF8_Blended(font, entry->label, theme->grid_color[1]);
    struct timespec start = mainui_timing_start();
    long long games =
        hidden || !font || console->hide_count ? -1 : mainui_catalog_game_count(catalog, index);
    mainui_timing_finish("carousel-count-ms", start);
    if (games >= 0) {
        char value[32];
        snprintf(value, sizeof value, "(%lld)", games);
        int style = TTF_GetFontStyle(font);
        TTF_SetFontStyle(font, style & ~TTF_STYLE_BOLD);
        slot->count = TTF_RenderUTF8_Blended(font, value, theme->grid_color[0]);
        TTF_SetFontStyle(font, style);
    }
}

/* Transfer finished worker surfaces on the UI thread. This also completes a
 * decode performed while Games was closed; its key remains reusable. */
static void carousel_reap(MainUIMenuView *view)
{
    MainUICarouselJob *job = &view->carousel_scratch;
    MainUICarouselDecoded decoded;
    if (!mainui_carousel_worker_take(&view->carousel_worker, job, &decoded)) {
        return;
    }
    MainUICarouselSlot *slot = carousel_cached(view, job->key, -1, false);
    if (!slot) {
        mainui_carousel_decoded_free(&decoded);
        return;
    }
    size_t retained = mainui_menu_view_bytes(view);
    size_t needed = surface_bytes(decoded.icon) + surface_bytes(decoded.selected) +
                    surface_bytes(decoded.small);
    /* Do not exceed the menu artwork budget. Keep normal art first; the
     * reduced neighbor variant and optional selected art are disposable. */
    if (over_budget(retained, needed) && decoded.small) {
        needed -= surface_bytes(decoded.small);
        SDL_FreeSurface(decoded.small);
        decoded.small = NULL;
    }
    if (over_budget(retained, needed) && decoded.selected) {
        needed -= surface_bytes(decoded.selected);
        SDL_FreeSurface(decoded.selected);
        decoded.selected = NULL;
    }
    if (over_budget(retained, needed)) {
        mainui_carousel_decoded_free(&decoded);
    }
    slot->icon = decoded.icon;
    slot->selected_icon = decoded.selected ? decoded.selected : decoded.icon;
    slot->small = decoded.small;
    slot->thumb = decoded.thumb && decoded.icon != NULL;
    slot->attempted = true;
    mainui_count_add("carousel-idle-loads", 1);
}

/* Decode a slot on the UI thread: snapshots, scripted input, and a worker
 * that could not be started. */
static void decode_now(MainUIMenuView *view, const MainUICarouselJob *job, MainUICarouselSlot *slot)
{
    MainUICarouselDecoded decoded;
    mainui_carousel_decode(job, &decoded);
    size_t retained = mainui_menu_view_bytes(view);
    size_t needed = surface_bytes(decoded.icon) + surface_bytes(decoded.selected) +
                    surface_bytes(decoded.small);
    if (over_budget(retained, needed)) {
        mainui_carousel_decoded_free(&decoded);
    }
    slot->icon = decoded.icon;
    slot->selected_icon = decoded.selected ? decoded.selected : decoded.icon;
    slot->small = decoded.small;
    slot->thumb = decoded.thumb && decoded.icon != NULL;
    slot->attempted = true;
}

/* Schedule the selected cover, its immediate neighbors, then the outer pair.
 * Continue through the other consoles in the background so a 16-console
 * collection stays warm across catalog discovery and main-menu visits. */
static bool carousel_queue(MainUIMenuView *view, MainUICatalog *catalog,
                           const MainUIViewport *position, const MainUIConsoleView *console,
                           int height, bool synchronous)
{
    int total = position->total;
    bool decoded_here = false;
    if (view->carousel_worker.thread && !synchronous) {
        return false;
    }
    int limit = total < MAINUI_CAROUSEL_CACHE_SIZE ? total : MAINUI_CAROUSEL_CACHE_SIZE;
    for (int step = 0; step < (synchronous ? 2 * CENTRE_SLOT + 1 : limit); step++) {
        int offset = step % 2 ? (step + 1) / 2 : -(step / 2);
        int index = position->selected + offset;
        if (console->carousel_wrap && total > 0) {
            index = ((index % total) + total) % total;
        }
        if (index < 0 || index >= total) {
            continue;
        }
        MainUICarouselJob *job = &view->carousel_scratch;
        if (!carousel_job(view, catalog, console, height, index, job)) {
            continue;
        }
        MainUICarouselSlot *slot = carousel_cached(view, job->key, index, true);
        if (!slot || slot->attempted ||
            (view->carousel_worker.thread && view->carousel_worker.job.key == job->key)) {
            continue;
        }
        if (synchronous) {
            decode_now(view, job, slot);
            decoded_here = true;
        }
        else if (!view->carousel_worker.thread) {
            if (mainui_carousel_worker_start(&view->carousel_worker, job)) {
                mainui_count_add("carousel-slot-miss", 1);
                mainui_count_add("carousel-image-loads", 1);
                return decoded_here;
            }
            /* No thread could be started: decode the visible ones here
             * instead, so the carousel is still revealed rather than kept
             * waiting; the rest are tried again later. */
            decode_now(view, job, slot);
            decoded_here = true;
            if (step >= 2 * CENTRE_SLOT) {
                return true;
            }
        }
    }
    return decoded_here;
}

bool mainui_menu_carousel_service(MainUIMenuView *view, MainUICatalog *catalog,
                                  const MainUIViewport *position, const MainUIConsoleView *console,
                                  bool *handled)
{
    *handled = false;
    if (!view->carousel_worker.thread ||
        !atomic_load_explicit(&view->carousel_worker.done, memory_order_acquire)) {
        return false;
    }
    uint64_t key = view->carousel_worker.job.key;
    bool visible = false;
    for (int i = 0; i < MAINUI_CAROUSEL_SLOTS; i++) {
        if (view->carousel[i].index >= 0 && view->carousel[i].key == key) {
            visible = true;
            break;
        }
    }
    carousel_reap(view);
    *handled = true;
    if (catalog && position && console && view->carousel_catalog == catalog &&
        view->cached_start == MAINUI_MENU_VIEW_CAROUSEL) {
        /* Reuse the existing queue, just independently of a repaint. */
        (void)carousel_queue(view, catalog, position, console, view->carousel_height, false);
    }
    return visible;
}

static void carousel_slots(MainUIMenuView *view, MainUICatalog *catalog,
                           const MainUIViewport *position, const MainUIConsoleView *console,
                           int height)
{
    carousel_reap(view);
    if (view->carousel_catalog != catalog || view->carousel_entries != catalog->pages[0].entries ||
        view->carousel_total != position->total) {
        /* A rebuilt catalog can change game counts; retain decoded art. */
        for (int i = 0; i < MAINUI_CAROUSEL_CACHE_SIZE; i++) {
            MainUICarouselSlot *slot = &view->carousel_cache[i];
            if (slot->label) {
                SDL_FreeSurface(slot->label);
                slot->label = NULL;
            }
            if (slot->count) {
                SDL_FreeSurface(slot->count);
                slot->count = NULL;
            }
            slot->loaded = false;
        }
        view->carousel_catalog = catalog;
        view->carousel_entries = catalog->pages[0].entries;
        view->carousel_total = position->total;
        view->slid_to = -1;
        view->sliding = false;
        view->drawn_count = 0;
    }
    if (view->cached_start != MAINUI_MENU_VIEW_CAROUSEL) {
        clear_consoles(view);
        view->cached_start = MAINUI_MENU_VIEW_CAROUSEL;
        view->carousel_revealed = false;
        view->slid_to = -1;
        view->sliding = false;
        view->drawn_count = 0;
    }
    view->carousel_height = height;
    view->carousel_scale = console->scale_unselected;
    carousel_indices(position->selected, position->total, console->carousel_wrap,
                     view->carousel_direction, view->carousel_visible);
    /* Clear borrowed references before allocating LRU entries, so entries
     * from a previous visible window can be evicted safely. */
    for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
        view->carousel[k] = (MainUICarouselSlot){.index = -1};
    }
    for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
        int index = view->carousel_visible[k];
        MainUICarouselJob *job = &view->carousel_scratch;
        if (index < 0 || !carousel_job(view, catalog, console, height, index, job)) {
            continue;
        }
        MainUICarouselSlot *slot = carousel_cached(view, job->key, index, true);
        if (!slot) {
            continue;
        }
        if (k == CENTRE_SLOT) {
            carousel_selected_text(view, catalog, console, slot, index);
        }
        view->carousel[k] = *slot;
        view->carousel[k].index = index;
    }
    /* Async on hardware, synchronous for snapshots and scripted captures. */
    bool decoded_here =
        carousel_queue(view, catalog, position, console, height, !view->defer_selected);
    carousel_reap(view);
    /* If a synchronous load completed above, refresh the borrowed pointers. */
    if (decoded_here) {
        for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
            int index = view->carousel_visible[k];
            MainUICarouselJob *job = &view->carousel_scratch;
            if (index >= 0 && carousel_job(view, catalog, console, height, index, job)) {
                MainUICarouselSlot *slot = carousel_cached(view, job->key, index, false);
                if (slot) {
                    view->carousel[k] = *slot;
                }
            }
        }
    }
    /* First entry reveals the centre and nearest neighbors as a group. */
    if (!view->carousel_revealed) {
        bool ready = true;
        for (int k = CENTRE_SLOT - 1; k <= CENTRE_SLOT + 1; k++) {
            ready &= view->carousel[k].index < 0 || view->carousel[k].attempted;
        }
        view->carousel_revealed = ready;
    }
}

/* The plain selection behind a thumbnail: the selected names' colour, faint,
 * or the colour console-view.json gives, its corners cut. Kept for the next
 * frame while its size and colour stay. */
static void selection(MainUIMenuView *view, SDL_Surface *screen, SDL_Color colour, Uint8 alpha,
                      SDL_Rect area)
{
    SDL_Surface *back = view->selection;
    if (!back || back->w != area.w || back->h != area.h || view->selection_alpha != alpha ||
        memcmp(&view->selection_colour, &colour, sizeof colour)) {
        if (back) {
            SDL_FreeSurface(back);
        }
        back = view->selection =
            SDL_CreateRGBSurface(SDL_SWSURFACE, area.w, area.h, 32, 0xff0000, 0xff00, 0xff, 0);
        if (!back) {
            return;
        }
        view->selection_colour = colour;
        view->selection_alpha = alpha;
        Uint32 key = SDL_MapRGB(back->format, 255, 0, 255);
        if (colour.r == 255 && colour.g == 0 && colour.b == 255) {
            key = SDL_MapRGB(back->format, 0, 255, 0);
        }
        SDL_FillRect(back, NULL, SDL_MapRGB(back->format, colour.r, colour.g, colour.b));
        for (int corner = 0; corner < 4; corner++) {
            int x = corner & 1 ? area.w - 3 : 0, y = corner & 2 ? area.h - 3 : 0;
            SDL_Rect cut[2] = {{(Sint16)x, (Sint16)(corner & 2 ? area.h - 1 : 0), 3, 1},
                               {(Sint16)(corner & 1 ? area.w - 1 : 0), (Sint16)y, 1, 3}};
            SDL_FillRect(back, &cut[0], key);
            SDL_FillRect(back, &cut[1], key);
        }
        SDL_SetColorKey(back, SDL_SRCCOLORKEY, key);
        if (alpha < 255) {
            SDL_SetAlpha(back, SDL_SRCALPHA, alpha);
        }
    }
    SDL_Rect place = area;
    SDL_BlitSurface(back, NULL, screen, &place);
}

/* The lines beneath the selected console, from y, centred on x, kept above
 * the footer. */
static void beneath(SDL_Surface *screen, const MainUICarouselSlot *slot, int x, int y)
{
    int lines = (slot->label ? slot->label->h : 0) + (slot->count ? slot->count->h : 0);
    y = y + lines > 416 ? 416 - lines : y;
    if (slot->label) {
        blit(screen, slot->label, x - slot->label->w / 2, y);
        y += slot->label->h;
    }
    if (slot->count) {
        blit(screen, slot->count, x - slot->count->w / 2, y);
    }
}

/* What a slot shows: the selected picture for the big one, else the smaller
 * thumbnail or the icon. */
static SDL_Surface *shown(const MainUICarouselSlot *slot, bool big)
{
    return big           ? slot->selected_icon ? slot->selected_icon : slot->icon
           : slot->small ? slot->small
                         : slot->icon;
}

/* The neighbours' tile at scale percent, scaled once and kept while the
 * theme's tile and the scale stay; the tile itself where it cannot be. */
static SDL_Surface *small_tile(MainUIMenuView *view, SDL_Surface *tile, int scale)
{
    if (!tile || scale >= 100) {
        return tile;
    }
    if (view->carousel_tile_source != tile || view->carousel_tile_scale != scale) {
        if (view->carousel_tile) {
            SDL_FreeSurface(view->carousel_tile);
        }
        int width = tile->w * scale / 100, height = tile->h * scale / 100;
        view->carousel_tile =
            mainui_image_fit(tile, width > 0 ? width : 1, height > 0 ? height : 1);
        view->carousel_tile_source = tile;
        view->carousel_tile_scale = scale;
    }
    return view->carousel_tile ? view->carousel_tile : tile;
}

bool mainui_menu_carousel_sliding(const MainUIMenuView *view)
{
    return view->sliding && view->cached_start == MAINUI_MENU_VIEW_CAROUSEL;
}

void mainui_menu_draw_carousel(MainUIMenuView *view, SDL_Surface *screen, MainUICatalog *catalog,
                               const MainUIViewport *position, const MainUIConsoleView *console,
                               Uint32 now, int pinned)
{
    MainUITheme *theme = view->theme;
    bool expert = !strcmp(catalog->pages[0].title, "Expert");
    bool text_hidden = expert ? theme->hide_expert_text : theme->hide_grid_text;
    struct timespec compose_start = mainui_timing_start();
    TTF_Font *font = expert && theme->expert_font ? theme->expert_font : theme->grid_font;
    int height, centre;
    carousel_layout(console, font, text_hidden, &height, &centre);
    CarouselCell cell = carousel_cell(expert);
    /* A cell's middle, with only the number of games beneath it, raised
     * when the theme's font would put that in the footer. */
    int cell_centre = console->hide_count || text_hidden ? 240 : 228;
    if (!console->hide_count && !text_hidden && font) {
        int over = cell_centre + cell.height / 2 + 6 + 4 + TTF_FontHeight(font) - 416;
        cell_centre -= over > 0 ? over : 0;
    }
    cell_centre += console->offset_y;
    frame(theme, screen,
          expert ? mainui_menu_label(MAINUI_MENU_EXPERT) : mainui_menu_label(MAINUI_MENU_GAMES),
          position->total ? position->selected + 1 : 0, position->total);
    if (!position->total || position->selected < 0) {
        mainui_draw_empty(screen, theme);
        view->sliding = false;
        mainui_timing_finish("carousel-compose-ms", compose_start);
        return;
    }
    mainui_timing_finish("carousel-compose-ms", compose_start);
    struct timespec slot_start = mainui_timing_start();
    Uint32 slot_tick = SDL_GetTicks();
    carousel_slots(view, catalog, position, console, height);
    Uint32 slot_elapsed = SDL_GetTicks() - slot_tick;
    mainui_timing_finish("carousel-slot-ms", slot_start);
    if (position->selected != view->slid_to) {
        if (slot_elapsed >= 25) {
            mainui_count_add("carousel-stall25", 1);
        }
        if (slot_elapsed >= 100) {
            mainui_count_add("carousel-stall100", 1);
        }
    }
    if (!view->carousel_revealed) {
        /* The worker is still preparing one of the first three covers.
         * Draw the frame without partial artwork until all are ready. */
        return;
    }
    compose_start = mainui_timing_start();
    /* A new selection slides from where the consoles were drawn. */
    if (position->selected != view->slid_to) {
        view->sliding =
            console->slide && !view->carousel_snap && view->slid_to >= 0 && view->drawn_count > 0;
        view->carousel_snap = false;
        view->slid_from = view->slid_to;
        view->slid_to = position->selected;
        view->slide_start = now;
        view->from_count = view->drawn_count;
        memcpy(view->from_index, view->drawn_index, sizeof view->from_index);
        memcpy(view->from_x, view->drawn_x, sizeof view->from_x);
    }
    /* How far along, eased out, and which console is drawn as selected: the
     * one before, then the new one from halfway. */
    int progress = 1000;
    if (view->sliding) {
        Uint32 elapsed = pinned >= 0 ? (Uint32)pinned : now - view->slide_start;
        if (elapsed >= (Uint32)console->duration_ms) {
            view->sliding = false;
        }
        else {
            int t = (int)(elapsed * 1000 / (Uint32)console->duration_ms);
            if (console->smooth_easing) {
                /* Smoothstep: gentle acceleration and deceleration. No
                 * intermediate image resizing or additional blits. */
                progress = (int)((int64_t)t * t * (3000 - 2 * t) / 1000000);
            }
            else {
                int rest = 1000 - t;
                progress = 1000 - rest * rest / 1000 * rest / 1000; /* 1 - (1-t)^3 */
            }
        }
    }
    int big = position->selected;
    if (view->sliding && progress < 500) {
        /* On a jump, the old selection may have left the visible window.
         * Never leave the new centre in its unselected (small) appearance. */
        for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
            if (view->carousel[k].index == view->slid_from) {
                big = view->slid_from;
                break;
            }
        }
    }
    /* Each one's place round the selection: the selected at the screen's
     * middle, the others out from it by their own widths. Cells side by side
     * as in the grid, which spaces them itself; the gap beside a thumbnail. */
    int middle[MAINUI_CAROUSEL_SLOTS], width[MAINUI_CAROUSEL_SLOTS];
    for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
        MainUICarouselSlot *slot = &view->carousel[k];
        SDL_Surface *icon = shown(slot, k == CENTRE_SLOT);
        width[k] = slot->thumb        ? icon ? icon->w : 120
                   : k == CENTRE_SLOT ? cell.width
                                      : cell.width * console->scale_unselected / 100;
    }
    middle[CENTRE_SLOT] = 320;
    for (int k = CENTRE_SLOT + 1; k < MAINUI_CAROUSEL_SLOTS; k++) {
        int gap = view->carousel[k - 1].thumb || view->carousel[k].thumb ? console->gap : 0;
        middle[k] = middle[k - 1] + (width[k - 1] + width[k]) / 2 + gap;
    }
    for (int k = CENTRE_SLOT - 1; k >= 0; k--) {
        int gap = view->carousel[k + 1].thumb || view->carousel[k].thumb ? console->gap : 0;
        middle[k] = middle[k + 1] - (width[k + 1] + width[k]) / 2 - gap;
    }
    /* Sliding: from the places drawn last, a console not drawn then coming
     * in with the one that was selected. */
    if (view->sliding) {
        int shift = 0;
        for (int f = 0; f < view->from_count; f++) {
            for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
                if (view->carousel[k].index == view->from_index[f] &&
                    view->from_index[f] == view->slid_from) {
                    shift = view->from_x[f] - middle[k];
                }
            }
        }
        for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
            int from = middle[k] + shift;
            for (int f = 0; f < view->from_count; f++) {
                if (view->from_index[f] == view->carousel[k].index) {
                    from = view->from_x[f];
                }
            }
            /* Round the ends of a short list a console can change sides:
             * never across the middle, but in from the side the row moves
             * from, as one not drawn before. */
            if (view->carousel[k].index != view->slid_from &&
                ((from < 320 && middle[k] > 320) || (from > 320 && middle[k] < 320) ||
                 (k == CENTRE_SLOT && (shift > 0 ? from < 320 : shift < 0 && from > 320)))) {
                from = middle[k] + shift;
            }
            middle[k] = from + (middle[k] - from) * progress / 1000;
        }
    }
    view->drawn_count = 0;
    bool plain = console->selection != MAINUI_CONSOLE_SELECTION_NONE;
    SDL_Color colour = console->selection == MAINUI_CONSOLE_SELECTION_COLOUR
                           ? console->selection_colour
                           : theme->grid_color[1];
    /* About 12% by default: size and opacity already mark the selection. */
    Uint8 alpha =
        console->selection == MAINUI_CONSOLE_SELECTION_COLOUR ? console->selection_alpha : 31;
    int pad = console->selection_padding;
    SDL_Rect band = {0, 60, 640, 360};
    SDL_SetClipRect(screen, &band);
    for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
        MainUICarouselSlot *slot = &view->carousel[k];
        if (slot->index < 0) {
            continue;
        }
        int x = middle[k];
        view->drawn_index[view->drawn_count] = slot->index;
        view->drawn_x[view->drawn_count++] = x;
        bool selected = slot->index == big;
        /* "opacity_unselected": 0 hides the neighbours' pictures. */
        SDL_Surface *icon =
            selected || console->opacity_unselected > 0 ? shown(slot, selected) : NULL;
        if (slot->thumb) {
            if (!icon) {
                continue; /* not decoded yet, or hidden */
            }
            int w = icon->w, h = icon->h;
            if (selected && plain) {
                SDL_Rect area = {(Sint16)(x - w / 2 - pad), (Sint16)(centre - h / 2 - pad),
                                 (Uint16)(w + 2 * pad), (Uint16)(h + 2 * pad)};
                selection(view, screen, colour, alpha, area);
            }
            blit(screen, icon, x - w / 2, centre - h / 2);
            continue;
        }
        /* A cell of the grid: its tile (Games) or selection mark (Expert)
         * and the icon, a neighbour's both at scale_unselected. */
        int scale = selected ? 100 : console->scale_unselected;
        int cell_w = cell.width * scale / 100, cell_h = cell.height * scale / 100;
        int left = x - cell_w / 2, top = cell_centre - cell_h / 2;
        SDL_Surface *tile = console->selection == MAINUI_CONSOLE_SELECTION_NONE ? NULL
                            : expert ? (selected ? theme->expert_selection : NULL)
                                     : theme->tiles[selected];
        tile = selected ? tile : small_tile(view, tile, scale);
        if (tile) {
            int nudge = 2 * scale / 100;
            blit(screen, tile, left + (cell_w - tile->w) / 2 + nudge,
                 top + (cell_h - tile->h) / 2 + nudge);
        }
        if (icon) {
            blit(screen, icon, x - icon->w / 2,
                 cell_centre - icon->h / 2 - cell.icon_lift * scale / 100);
        }
    }
    /* The selected console's name and number of games, once, centred
     * beneath the middle: they stay put while the pictures slide and change
     * with the size, halfway through a slide, or at once without one. Never
     * scaled. */
    MainUICarouselSlot *current = &view->carousel[CENTRE_SLOT];
    for (int k = 0; k < MAINUI_CAROUSEL_SLOTS; k++) {
        if (view->carousel[k].index == big) {
            current = &view->carousel[k];
            break;
        }
    }
    if (current->index == big && current->thumb) {
        SDL_Surface *icon = shown(current, true);
        if (icon) {
            beneath(screen, current, 320, centre + icon->h / 2 + pad + 8);
        }
    }
    else if (current->index == big) {
        int left = 320 - cell.width / 2, top = cell_centre - cell.height / 2;
        int bottom = top + cell.height;
        SDL_Surface *tile = console->selection == MAINUI_CONSOLE_SELECTION_NONE ? NULL
                            : expert ? theme->expert_selection
                                     : theme->tiles[1];
        if (tile) {
            int ty = top + (cell.height - tile->h) / 2 + 2;
            bottom = ty + tile->h > bottom ? ty + tile->h : bottom;
        }
        if (current->label) {
            /* As the grid places names, Expert's 1 px lower. */
            int label_y = top + cell.height - cell.label_offset + (18 - current->label->h) / 2 +
                          (expert ? 1 : 0);
            SDL_Rect clip = {(Sint16)left, (Sint16)(top + cell.height - cell.label_offset - 15),
                             (Uint16)cell.width, 50};
            SDL_SetClipRect(screen, &clip);
            blit(screen, current->label, 320 - current->label->w / 2, label_y);
            SDL_SetClipRect(screen, &band);
        }
        if (current->count) {
            MainUICarouselSlot count_only = {.count = current->count};
            beneath(screen, &count_only, 320, bottom + 4);
        }
    }
    SDL_SetClipRect(screen, NULL);
    mainui_timing_finish("carousel-compose-ms", compose_start);
}
