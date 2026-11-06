/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/details.h"
#include "app/browser.h"
#include "ui/drawing.h"
#include "ui/panels.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mainui_details_close(MainUIDetails *details)
{
    bool sleeping = details->media.sleeping;
    free(details->description_lines);
    free(details->summary);
    mainui_media_close(&details->media);
    *details = (MainUIDetails){0};
    details->media.sleeping = sleeping;
}

bool mainui_details_media(MainUIDetails *details, const char *sd, SDL_Surface *screen, int start,
                          const MainUIMediaOptions *options)
{
    if (!details->open) {
        return false;
    }
    MainUIMediaOptions with = options ? *options : (MainUIMediaOptions){0};
    with.launch = *details->launch ? details->launch : NULL;
    if (!mainui_media_open(&details->media, details->rom_root, details->host_rom, sd, screen,
                           options ? &with : NULL, start)) {
        return false;
    }
    /* The description stops and goes back to the top as media opens, and
     * starts again after its delay on return. */
    mainui_details_reset_scroll(details);
    return true;
}

void mainui_details_reset_scroll(MainUIDetails *details)
{
    /* Returning from media starts Game Info's description from the top and
     * restarts its auto-scroll delay, without reallocating the layout. */
    details->scroll_line = details->scroll_px = 0;
    details->auto_started = details->auto_paused = details->auto_finished = false;
    details->auto_held = false;
    details->swallow_page_key = SDLK_UNKNOWN;
}

void mainui_details_progress(MainUIDetails *details, Uint32 wait_ms)
{
    if (details->open && details->preview) {
        mainui_preview_request_within(details->preview, details->catalog, details->library,
                                      details->index, wait_ms);
    }
}

void mainui_details_markers(MainUIDetails *details, const MainUILibrary *favorites)
{
    if (details->open) {
        details->favorite = mainui_library_contains(favorites, details->rom);
    }
}

static bool folder(MainUICatalog *catalog, const MainUILibrary *library, int row)
{
    return library ? mainui_library_is_folder(library, row) : mainui_browser_folder(catalog, row);
}

/* Runs of spaces, tabs and line breaks become one space; none lead or trail. */
static void collapse_space(char *text)
{
    char *write = text;
    bool space = false;
    for (const char *read = text; *read; ++read) {
        if (*read == ' ' || *read == '\n' || *read == '\r' || *read == '\t') {
            space = write != text;
        }
        else {
            if (space) {
                *write++ = ' ';
            }
            *write++ = *read;
            space = false;
        }
    }
    *write = 0;
}

/* A row's game: its title, ROM path as listed, launcher, and catalog index.
 * False for folders and rows that are not games. */
static bool game_at(MainUICatalog *catalog, const MainUILibrary *library, int row,
                    const char **label, const char **rom, const char **launch, int *index)
{
    if (!catalog || row < 0 || folder(catalog, library, row)) {
        return false;
    }
    *index = library ? row : mainui_browser_index(catalog, row);
    if (library) {
        if (row >= library->visible_count) {
            return false;
        }
        const MainUILibraryItem *item = &library->items[library->visible[row]];
        if (item->type == 3) {
            return false;
        }
        *label = item->label;
        *rom = item->rom;
        *launch = item->launch;
        return *label && *rom;
    }
    if (!catalog->depth) {
        return false;
    }
    MainUIEntry *entry = mainui_catalog_entry(catalog, *index);
    if (!entry) {
        return false;
    }
    *label = entry->label;
    *rom = entry->path;
    /* A Search result names its source console; other rows are the
     * open console's. */
    int system = catalog->pages[0].view.selected;
    *launch = entry->launch ? entry->launch
              : system >= 0 && system < catalog->pages[0].count
                  ? catalog->pages[0].entries[system].launch
                  : NULL;
    return *label && *rom;
}

bool mainui_details_locate(MainUICatalog *catalog, const MainUILibrary *library, int row,
                           char host_rom[MAINUI_PATH_MAX], char rom_root[MAINUI_PATH_MAX])
{
    const char *label = NULL, *rom = NULL, *launch = NULL;
    int index;
    if (!game_at(catalog, library, row, &label, &rom, &launch, &index) ||
        !mainui_catalog_path(host_rom, catalog->sd, catalog->sd, rom)) {
        return false;
    }
    /* The console by its launcher, as Game Info finds it. */
    const char *root = library ? NULL : catalog->pages[1].path;
    MainUISource *source = malloc(sizeof *source); /* 8 KiB: kept off the stack */
    if (source && mainui_catalog_source(catalog, rom, launch, source)) {
        root = source->rom_root;
    }
    snprintf(rom_root, MAINUI_PATH_MAX, "%s", root ? root : "");
    free(source);
    return true;
}

bool mainui_details_launcher(MainUICatalog *catalog, const MainUILibrary *library, int row,
                             char host_rom[MAINUI_PATH_MAX], char launch[MAINUI_PATH_MAX])
{
    const char *label = NULL, *rom = NULL, *from = NULL;
    int index;
    return game_at(catalog, library, row, &label, &rom, &from, &index) && from &&
           mainui_catalog_path(host_rom, catalog->sd, catalog->sd, rom) &&
           mainui_catalog_path(launch, catalog->sd, catalog->sd, from);
}

bool mainui_details_open(MainUIDetails *details, MainUICatalog *catalog,
                         const MainUILibrary *library, const MainUILibrary *favorites, int row,
                         MainUIPreview *preview, const MainUIConfig *config)
{
    const char *label = NULL, *rom = NULL, *launch = NULL;
    int index;
    if (!preview || !game_at(catalog, library, row, &label, &rom, &launch, &index)) {
        return false;
    }
    char *host = malloc(MAINUI_PATH_MAX);
    if (!host) {
        return false;
    }
    if (!mainui_catalog_path(host, catalog->sd, catalog->sd, rom) ||
        strlen(label) >= sizeof details->title) {
        free(host);
        return false;
    }
    mainui_details_close(details);
    details->preview = preview;
    strcpy(details->title, label);
    /* The game's console by its launcher: consoles can share a ROM folder,
     * and a saved game's may be outside the catalog (an Expert console while
     * Games is open), named by its config.json with the ROM folder its
     * gamelist.xml may be in. */
    const char *root = library ? NULL : catalog->pages[1].path;
    MainUISource *source = malloc(sizeof *source);
    if (source && mainui_catalog_source(catalog, rom, launch, source)) {
        snprintf(details->system, sizeof details->system, "%s", source->label);
        root = source->rom_root;
    }
    snprintf(details->rom_root, sizeof details->rom_root, "%s", root ? root : "");
    free(source);
    snprintf(details->host_rom, sizeof details->host_rom, "%s", host);
    /* The launcher, for the game's core and save states. */
    if (!launch || !mainui_catalog_path(details->launch, catalog->sd, catalog->sd, launch)) {
        details->launch[0] = 0;
    }
    cJSON *record = library ? NULL : mainui_catalog_record(catalog, index);
    const cJSON *saved_rom = cJSON_GetObjectItemCaseSensitive(record, "rompath");
    snprintf(details->rom, sizeof details->rom, "%s",
             library                     ? rom
             : cJSON_IsString(saved_rom) ? saved_rom->valuestring
                                         : "");
    details->favorite = mainui_library_contains(favorites, details->rom);
    cJSON_Delete(record);
    mainui_gamelist_metadata(host, details->rom_root, &details->metadata);
    /* Match the reference metadata parser's collapsed XML whitespace, also in
     * the genre, where a line break would split the summary line. */
    collapse_space(details->metadata.description);
    collapse_space(details->metadata.genre);
    /* Keep the summary ready without two large render-time stack buffers. */
    size_t bytes = strlen(details->metadata.genre) + strlen(details->metadata.rating) + 4;
    details->summary = malloc(bytes);
    if (details->summary) {
        snprintf(details->summary, bytes, "%s%s%s", details->metadata.genre,
                 *details->metadata.genre && *details->metadata.rating ? " | " : "",
                 details->metadata.rating);
    }
    /* The system label is trimmed once, instead of copying 4 KiB on every draw. */
    char *system = details->system;
    while (*system == ' ') {
        system++;
    }
    if (system != details->system) {
        memmove(details->system, system, strlen(system) + 1);
    }
    size_t last_system = strlen(details->system);
    while (last_system && details->system[last_system - 1] == ' ') {
        details->system[--last_system] = 0;
    }
    /* Select the cover without waiting: a cached one shows at once, any
     * other when its decode is done (mainui_details_progress). */
    details->catalog = catalog;
    details->library = library;
    details->index = index;
    mainui_preview_request_within(details->preview, catalog, library, index, 0);
    int leading = library ? library->leading_folders
                          : catalog->pages[catalog->depth].folder_count + (catalog->depth > 1);
    details->ordinal = row + 1 - leading;
    details->total = library ? library->visible_games : mainui_browser_count(catalog) - leading;
    details->auto_delay = config ? config->details_auto_delay : 0;
    details->auto_speed = config ? config->details_auto_speed : 0;
    details->open = true;
    free(host);
    return true;
}

void mainui_details_key(MainUIDetails *details, MainUICatalog *catalog,
                        const MainUILibrary *library, const MainUILibrary *favorites,
                        MainUIViewport *view, int rows, SDLKey key, const MainUIConfig *config)
{
    if (key == SDLK_ESCAPE || key == SDLK_LEFT) {
        mainui_details_close(details);
        return;
    }
    if (key == SDLK_HOME || key == SDLK_END) {
        /* Before auto-scroll has moved, a page press cancels it and pages at
         * once. Once it has moved, the first physical press only stops it;
         * key-repeat from that press is swallowed upstream. */
        if (details->auto_speed && !details->auto_paused && !details->auto_finished &&
            details->scroll_max_px > 0) {
            bool moved = details->scroll_px != 0;
            details->auto_paused = true;
            if (moved) {
                details->swallow_page_key = key;
                return;
            }
        }
        if (details->auto_speed && details->description_line_height > 0) {
            details->scroll_line = details->scroll_px / details->description_line_height;
        }
        int last = details->line_count - details->lines_per_page;
        if (last < 0) {
            last = 0;
        }
        details->scroll_line +=
            key == SDLK_HOME ? -details->lines_per_page : details->lines_per_page;
        if (details->scroll_line < 0) {
            details->scroll_line = 0;
        }
        if (details->scroll_line > last) {
            details->scroll_line = last;
        }
        if (details->auto_speed && details->description_line_height > 0) {
            details->scroll_px = details->scroll_line * details->description_line_height;
        }
    }
    if (key != SDLK_UP && key != SDLK_DOWN) {
        return;
    }
    int total = library ? library->visible_count : mainui_browser_count(catalog);
    int target = view->selected, direction = key == SDLK_UP ? -1 : 1;
    for (int checked = 0; checked < total; checked++) {
        target = (target + total + direction) % total;
        if (!folder(catalog, library, target) &&
            mainui_details_open(details, catalog, library, favorites, target, details->preview,
                                config)) {
            mainui_viewport_move(view, rows, target - view->selected, false);
            return;
        }
    }
}

/* Font-measured wrapping never splits a UTF-8 continuation byte. Return the
 * next input byte, preserving explicit newlines and making progress on wide
 * glyphs. Whole words are measured first; only the word that crosses the
 * width is measured a character at a time. */
static const char *next_character(const char *p)
{
    const char *next = p + 1;
    while (((unsigned char)*next & 0xc0) == 0x80) {
        next++;
    }
    return next;
}

static bool prefix_fits(TTF_Font *font, const char *start, const char *end, char out[4096],
                        int width)
{
    size_t size = (size_t)(end - start);
    if (size >= 4096) {
        return false;
    }
    memcpy(out, start, size);
    out[size] = 0;
    int measured = 0;
    return TTF_SizeUTF8(font, out, &measured, NULL) >= 0 && measured <= width;
}

static const char *line(TTF_Font *font, const char *text, char out[4096], int width)
{
    const char *p = text, *fit = text, *space = NULL;
    while (*p && *p != '\n') {
        /* A space is a word of its own: wrapping prefers the last space. */
        const char *end = p;
        if (*end == ' ') {
            end++;
        }
        else {
            while (*end && *end != ' ' && *end != '\n' && (size_t)(end - text) < 4095) {
                end = next_character(end);
            }
        }
        if (end == p) {
            break;
        }
        if (prefix_fits(font, text, end, out, width)) {
            if (*p == ' ') {
                space = p;
            }
            p = fit = end;
            continue;
        }
        while (p < end) {
            const char *next = next_character(p);
            if (!prefix_fits(font, text, next, out, width)) {
                break;
            }
            fit = next;
            if (*p == ' ') {
                space = p;
            }
            p = next;
        }
        /* Shaping can make a word narrower than one of its prefixes. */
        if (p < end) {
            break;
        }
    }
    if (p == text && *p && *p != '\n') {
        fit = next_character(p);
    }
    else if (*p && *p != '\n' && space) {
        fit = space;
    }
    size_t size = (size_t)(fit - text);
    memcpy(out, text, size);
    out[size] = 0;
    if (*fit == ' ' || *fit == '\n') {
        fit++;
    }
    return fit;
}

/* The title's line breaks, from the theme's layouts of the last 5 titles,
 * or worked out and kept in the least recently used one. NULL for a title
 * too long to keep (over 4095 bytes or 256 lines): it is wrapped each time. */
static const MainUITitleLayout *title_lines(MainUITheme *theme, const char *title, int width)
{
    MainUITitleLayout *layout = &theme->title_layouts[0];
    for (int i = 0; i < 5; i++) {
        MainUITitleLayout *kept = &theme->title_layouts[i];
        if (kept->used && kept->font == theme->title_font && kept->width == width &&
            !strcmp(kept->title, title)) {
            kept->used = ++theme->title_layout_clock;
            return kept;
        }
        if (kept->used < layout->used) {
            layout = kept;
        }
    }
    size_t bytes = strlen(title);
    if (bytes >= sizeof layout->title) {
        return NULL;
    }
    layout->used = 0;
    layout->count = 0;
    char wrapped[4096];
    for (const char *p = title; *p;) {
        if (layout->count >= sizeof layout->lines / sizeof layout->lines[0]) {
            return NULL;
        }
        const char *start = p;
        p = line(theme->title_font, p, wrapped, width);
        layout->lines[layout->count].start = (unsigned short)(start - title);
        layout->lines[layout->count++].length = (unsigned short)strlen(wrapped);
    }
    memcpy(layout->title, title, bytes + 1);
    layout->font = theme->title_font;
    layout->width = width;
    layout->used = ++theme->title_layout_clock;
    return layout;
}

/* Zero extra work on missing config, paused scroll or completed scroll.
 * Compare wrapping font before using cached offsets: themes may reload. */
static void description_layout(MainUIDetails *details, TTF_Font *font)
{
    if (details->description_font == font && details->description_lines) {
        return;
    }
    free(details->description_lines);
    details->description_lines = NULL;
    details->description_font = font;
    const char *text = details->metadata.description;
    size_t length = strlen(text);
    unsigned short *offsets = malloc((length + 1) * sizeof *offsets);
    char wrapped[4096];
    int count = 0;
    /* Even one UTF-8 byte per line fits: description is at most 4095 B. */
    while (*text) {
        if (offsets) {
            offsets[count] = (unsigned short)(text - details->metadata.description);
        }
        const char *next = line(font, text, wrapped, 360);
        if (next <= text) {
            break;
        }
        text = next;
        count++;
    }
    details->description_lines = offsets;
    details->line_count = count;
}

/* Compute the same description viewport used by the draw path before a
 * queued L2/R2 key is consumed. The first draw may be coalesced away. */
void mainui_details_prepare(MainUIDetails *details, MainUITheme *theme)
{
    if (!details->open || !theme || !theme->title_font || !theme->detail_font) {
        return;
    }
    /* Only on an early page press: avoid stacking this 4 KiB scratch buffer
     * with title_lines()'s wrapping buffer. Normal drawing uses its own. */
    char *wrapped = malloc(4096);
    if (!wrapped) {
        return;
    }
    int height = TTF_FontHeight(theme->title_font) * 7 / 8;
    if (height < 20) {
        height = 20;
    }
    if (height > 40) {
        height = 40;
    }
    int y = 130;
    const MainUITitleLayout *layout = title_lines(theme, details->title, 360);
    if (layout) {
        y += (int)layout->count * height;
    }
    else {
        for (const char *p = details->title; *p;) {
            p = line(theme->title_font, p, wrapped, 360);
            y += height;
        }
    }
    y += 14;
    int meta_height = TTF_FontHeight(theme->detail_font);
    if (meta_height < 10) {
        meta_height = 10;
    }
    if (meta_height > 40) {
        meta_height = 40;
    }
    bool drawn = false;
    for (const char *p = details->summary ? details->summary : ""; *p && y + meta_height <= 420;) {
        p = line(theme->detail_font, p, wrapped, 360);
        y += meta_height + 2;
        drawn = true;
    }
    free(wrapped);
    if (drawn) {
        y += 5;
    }
    int line_height = meta_height + 2;
    int visible = (420 - y + 2) / line_height;
    if (visible < 1) {
        visible = 0;
    }
    description_layout(details, theme->detail_font);
    details->lines_per_page = visible;
    details->description_y = y;
    details->description_line_height = line_height;
    int last = details->line_count > visible ? details->line_count - visible : 0;
    details->scroll_max_px = visible ? last * line_height : 0;
    if (details->scroll_line > last) {
        details->scroll_line = last;
    }
    if (details->scroll_px > details->scroll_max_px) {
        details->scroll_px = details->scroll_max_px;
    }
}

void mainui_details_auto_hold(MainUIDetails *details, bool hold, Uint32 now)
{
    if (!details->open || !details->auto_speed) {
        return;
    }
    if (hold && !details->auto_held) {
        details->auto_hold_at = now;
        details->auto_held = true;
    }
    else if (!hold && details->auto_held) {
        if (details->auto_started) {
            details->auto_epoch += now - details->auto_hold_at;
        }
        details->auto_held = false;
    }
}

int mainui_details_auto_due(const MainUIDetails *details, Uint32 now)
{
    if (!details->open || !details->auto_speed || !details->auto_started || details->auto_held ||
        details->auto_paused || details->auto_finished || details->scroll_max_px <= 0) {
        return -1;
    }
    Uint32 elapsed = now - details->auto_epoch;
    if (elapsed < (Uint32)details->auto_delay) {
        return (int)((Uint32)details->auto_delay - elapsed);
    }
    /* At low speeds a whole pixel can take longer than 40 ms. Don't
     * repaint an unchanged description just to animate a subpixel. */
    uint64_t moving = elapsed - (Uint32)details->auto_delay;
    uint64_t pixels = moving * (unsigned)details->auto_speed / 1000;
    uint64_t next_ms = (pixels + 1) * 1000 + (unsigned)details->auto_speed - 1;
    next_ms = next_ms / (unsigned)details->auto_speed - moving;
    return next_ms > MAINUI_MARQUEE_FRAME_MS ? (int)next_ms : MAINUI_MARQUEE_FRAME_MS;
}

bool mainui_details_auto_step(MainUIDetails *details, Uint32 now)
{
    if (mainui_details_auto_due(details, now) < 0 ||
        (Uint32)(now - details->auto_epoch) < (Uint32)details->auto_delay) {
        return false;
    }
    Uint32 moving = now - details->auto_epoch - (Uint32)details->auto_delay;
    uint64_t pixels = (uint64_t)moving * (unsigned)details->auto_speed / 1000;
    if (pixels >= (unsigned)details->scroll_max_px) {
        pixels = (unsigned)details->scroll_max_px;
        details->auto_finished = true;
    }
    if (details->scroll_px == (int)pixels) {
        return false;
    }
    details->scroll_px = (int)pixels;
    return true;
}

void mainui_details_draw(MainUIDetails *details, MainUITheme *theme, SDL_Surface *screen)
{
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 24, 24, 24));
    mainui_blit(screen, theme->background, 0, 0);
    mainui_draw_header(screen, theme, mainui_translate(84, "GAME INFO"));
    mainui_draw_detail_counter(screen, theme, details->ordinal);
    if (details->favorite && theme->favorite) {
        int x = 634 - theme->favorite->w;
        mainui_blit(screen, theme->favorite, x < 274 ? 274 : x, 60);
    }
    SDL_Surface *image = details->preview && details->preview->image
                             ? details->preview->image
                             : mainui_theme_detail_default(theme);
    if (image) {
        mainui_blit(screen, image, (250 - image->w) / 2, 67);
    }
    SDL_Rect clip = {274, 98, 360, 322};
    SDL_SetClipRect(screen, &clip);
    mainui_label(screen, theme->menu_font, theme->color, details->system, 274, 98);
    char wrapped[4096];
    const char *text = details->title;
    int y = 130, height = TTF_FontHeight(theme->title_font) * 7 / 8, count = 0;
    if (height < 20) {
        height = 20;
    }
    if (height > 40) {
        height = 40;
    }
    /* The title is the same on every frame: its line breaks and the lines'
     * surfaces are kept rather than measured and rendered again. */
    const MainUITitleLayout *layout = title_lines(theme, text, 360);
    for (unsigned i = 0; layout && i < layout->count; i++) {
        memcpy(wrapped, text + layout->lines[i].start, layout->lines[i].length);
        wrapped[layout->lines[i].length] = 0;
        SDL_Surface *surface = mainui_theme_text(theme, theme->title_font, wrapped, theme->color);
        mainui_blit(screen, surface, 274, y);
        y += height;
    }
    while (!layout && *text) {
        text = line(theme->title_font, text, wrapped, 360);
        mainui_label(screen, theme->title_font, theme->color, wrapped, 274, y);
        y += height;
    }
    /* meta_draw: 14px after title, font height + 2px between lines,
     * then 5px after the summary before the description. */
    y += 14;
    int metadata_height = TTF_FontHeight(theme->detail_font);
    if (metadata_height < 10) {
        metadata_height = 10;
    }
    if (metadata_height > 40) {
        metadata_height = 40;
    }
    bool summary_drawn = false;
    text = details->summary ? details->summary : "";
    while (*text && y + metadata_height <= 420) {
        text = line(theme->detail_font, text, wrapped, 360);
        mainui_label(screen, theme->detail_font, theme->color, wrapped, 274, y);
        y += metadata_height + 2;
        summary_drawn = true;
    }
    if (summary_drawn) {
        y += 5;
    }
    height = metadata_height + 2;
    int visible = (420 - y + 2) / height;
    if (visible < 1) {
        details->lines_per_page = 0;
        SDL_SetClipRect(screen, NULL);
        mainui_draw_footer(screen, theme, 0, -1);
        return;
    }
    /* Both manual paging and auto-scroll share cached wrap offsets while
     * keeping their original labels and line spacing. */
    description_layout(details, theme->detail_font);
    if (!details->auto_speed) {
        count = details->line_count;
        details->lines_per_page = visible;
        int start = details->scroll_line;
        int last = count > visible ? count - visible : 0;
        if (start > last) {
            start = last;
        }
        details->scroll_line = start;
        text = details->metadata.description;
        if (details->description_lines && start < count) {
            text += details->description_lines[start];
        }
        else {
            for (int i = 0; *text && i < start; i++) {
                text = line(theme->detail_font, text, wrapped, 360);
            }
        }
        for (int i = 0; *text && i < visible; i++) {
            text = line(theme->detail_font, text, wrapped, 360);
            mainui_label(screen, theme->detail_font, theme->color, wrapped, 274, y + i * height);
        }
        SDL_SetClipRect(screen, NULL);
        mainui_draw_footer(screen, theme, 0, -1);
        return;
    }
    count = details->line_count;
    details->lines_per_page = visible;
    details->description_y = y;
    details->description_line_height = height;
    /* Always stop on the last full page, even if its last advance is short. */
    int last = count > visible ? count - visible : 0;
    details->scroll_max_px = last * height;
    if (details->scroll_line > last) {
        details->scroll_line = last;
    }
    if (details->scroll_px > details->scroll_max_px) {
        details->scroll_px = details->scroll_max_px;
    }
    if (details->auto_speed && !details->auto_started) {
        details->auto_epoch = SDL_GetTicks();
        details->auto_started = true;
    }
    int start = details->scroll_px / height;
    int offset = details->scroll_px % height;
    text = details->metadata.description;
    if (details->description_lines && start < count) {
        text += details->description_lines[start];
    }
    else {
        for (int i = 0; *text && i < start; i++) {
            text = line(theme->detail_font, text, wrapped, 360);
        }
    }
    /* An extra clipped line at the bottom makes sub-line pixel movement
     * continuous; no overflow enters the footer. */
    SDL_Rect description_clip = {274, (Sint16)y, 360, (Uint16)(420 - y)};
    SDL_SetClipRect(screen, &description_clip);
    for (int i = start; *text && i < count && i <= start + visible; i++) {
        text = line(theme->detail_font, text, wrapped, 360);
        mainui_label(screen, theme->detail_font, theme->color, wrapped, 274,
                     y + (i - start) * height - offset);
    }
    SDL_SetClipRect(screen, NULL);
    mainui_draw_footer(screen, theme, 0, -1);
}
