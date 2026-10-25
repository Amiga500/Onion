/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/drawing.h"
#include "ui/panels.h"
#include <SDL_image.h>
#include <stdio.h>
#include <string.h>

void mainui_dim_popup_background(SDL_Surface *screen, MainUITheme *theme)
{
    if (!theme->popup_dim) {
        SDL_PixelFormat *format = screen->format;
        theme->popup_dim = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 420, format->BitsPerPixel,
                                                format->Rmask, format->Gmask, format->Bmask, 0);
        if (!theme->popup_dim) {
            return;
        }
        SDL_FillRect(theme->popup_dim, NULL, SDL_MapRGB(theme->popup_dim->format, 0, 0, 0));
        SDL_SetAlpha(theme->popup_dim, SDL_SRCALPHA, 0xaa);
    }
    SDL_Rect destination = {0, 0, 0, 0};
    SDL_BlitSurface(theme->popup_dim, NULL, screen, &destination);
}

/* A label that fits in room, the popup width right of the 20 px inset, is
 * shown whole: v1.0.9 showed it to the popup's right edge. A wider one, cut
 * off there mid-letter, is shortened to end in "..." 5 px clear of the edge,
 * at a character boundary. Cutting the logical end also suits right-to-left
 * text: a bidi library puts the trailing dots on its visual left. False for
 * a label shown whole, which leaves shown as it was. */
static bool fit_label(TTF_Font *font, const char *label, int room, char shown[512])
{
    int width = 0, height = 0;
    if (TTF_SizeUTF8(font, label, &width, &height) || width <= room) {
        return false;
    }
    size_t length = strlen(label);
    if (length > 508) {
        length = 508;
    }
    size_t low = 0, high = length, best = 0;
    while (low < high) {
        size_t middle = low + (high - low + 1) / 2;
        size_t end = middle;
        while (end && ((unsigned char)label[end] & 0xc0) == 0x80) {
            --end;
        }
        memcpy(shown, label, end);
        memcpy(shown + end, "...", 4);
        if (end && !TTF_SizeUTF8(font, shown, &width, &height) && width <= room - 5) {
            best = end;
            low = middle;
        }
        else {
            high = middle - 1;
        }
    }
    while (best && label[best - 1] == ' ') {
        --best;
    }
    memcpy(shown, label, best);
    memcpy(shown + best, "...", 4);
    return true;
}

/* The label as shown, from the theme's last 6, or fitted and kept in the
 * least recently used one, so a redraw measures nothing again. */
static const char *context_label(MainUITheme *theme, const char *label, int room, char scratch[512])
{
    TTF_Font *font = theme->menu_font;
    size_t length = strlen(label);
    if (length >= sizeof theme->context_labels[0].label) {
        return fit_label(font, label, room, scratch) ? scratch : label;
    }
    int slot = 0;
    for (int i = 0; i < 6; i++) {
        if (theme->context_labels[i].used && theme->context_labels[i].font == font &&
            theme->context_labels[i].room == room &&
            !strcmp(theme->context_labels[i].label, label)) {
            theme->context_labels[i].used = ++theme->context_label_clock;
            return theme->context_labels[i].shown;
        }
        if (theme->context_labels[i].used < theme->context_labels[slot].used) {
            slot = i;
        }
    }
    memcpy(theme->context_labels[slot].label, label, length + 1);
    if (!fit_label(font, label, room, theme->context_labels[slot].shown)) {
        memcpy(theme->context_labels[slot].shown, label, length + 1);
    }
    theme->context_labels[slot].font = font;
    theme->context_labels[slot].room = room;
    theme->context_labels[slot].used = ++theme->context_label_clock;
    return theme->context_labels[slot].shown;
}

void mainui_draw_context(SDL_Surface *screen, MainUITheme *theme, const MainUIContext *context,
                         bool ellipsis)
{
    if (!context->visible_count) {
        return;
    }
    mainui_draw_popup_footer(screen, theme);
    int rows = context->visible_count < 6 ? context->visible_count : 6;
    SDL_Surface *background = mainui_theme_popup_background(theme, rows);
    int width = background ? background->w : 120;
    int height = background ? background->h : rows * 60;
    /* PopupWindow draw at 0x326a8 sets both destination coordinates to zero. */
    int x = 0, y = 0;
    if (background) {
        mainui_blit(screen, background, x, y);
    }
    else {
        SDL_Rect rect = {(Sint16)x, (Sint16)y, (Uint16)width, (Uint16)height};
        SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 24, 24, 24));
    }
    SDL_Surface *selection = mainui_theme_popup_selection(theme);
    int start = context->selected >= rows ? context->selected - rows + 1 : 0;
    for (int i = start; i < start + rows && i < context->visible_count; i++) {
        int top = y + (i - start) * 60;
        SDL_Rect clip = {(Sint16)x, (Sint16)top, (Uint16)width, 60};
        SDL_SetClipRect(screen, &clip);
        if (i == context->selected) {
            mainui_blit(screen, selection, x, top);
        }
        const MainUIContextEntry *entry = &context->entries[context->visible[i]];
        char scratch[512];
        const char *label = mainui_context_label(entry);
        if (ellipsis) {
            label = context_label(theme, label, width - 20, scratch);
        }
        mainui_label(screen, theme->menu_font, theme->color, label, x + 20,
                     top + (60 - TTF_FontHeight(theme->menu_font)) / 2);
    }
    SDL_SetClipRect(screen, NULL);
}

/* Stock's DialogWindow draws its title in the theme's total colour and its
 * question in grid.color (found by changing them in a theme). */
void mainui_draw_confirmation(SDL_Surface *screen, MainUITheme *theme, int title_id, int body_id)
{
    /* The theme's popup art, loaded once (as for the loading panel). */
    if (!theme->loading_background_loaded) {
        theme->loading_background = mainui_theme_image(theme, "skin/pop-bg.png");
        theme->loading_background_loaded = true;
    }
    SDL_Surface *background = theme->loading_background;
    int width = background ? background->w : 480, height = background ? background->h : 300;
    int x = (640 - width) / 2, y = (480 - height) / 2;
    mainui_blit(screen, background, x, y);
    const char *title_text = mainui_translate(title_id, title_id == 77    ? "Clear recent list?"
                                                        : title_id == 404 ? "Delete folder"
                                                                          : "Shutdown");
    SDL_Surface *title = TTF_RenderUTF8_Blended(theme->title_font, title_text, theme->total_color);
    if (title) {
        mainui_blit(screen, title, x + (width - title->w) / 2, y + (50 - title->h) / 2);
        SDL_FreeSurface(title);
    }
    const char *body = mainui_translate(
        body_id, body_id == 78   ? "Note: SD card content\nwill not be deleted."
                 : body_id == 81 ? "Remove from favorites list?"
                                 : "Are you sure you want to\nturn off your device?");
    int lines = 1;
    for (const char *p = body; *p; p++) {
        if (*p == '\n') {
            lines++;
        }
    }
    int top = y + 50 + (100 - lines * TTF_FontHeight(theme->title_font)) / 2;
    for (const char *p = body; *p;) {
        const char *end = strchr(p, '\n');
        size_t length = end ? (size_t)(end - p) : strlen(p);
        char line[1024];
        if (length >= sizeof line) {
            length = sizeof line - 1;
        }
        memcpy(line, p, length);
        line[length] = 0;
        SDL_Surface *text = TTF_RenderUTF8_Blended(theme->title_font, line, theme->grid_color[0]);
        if (text) {
            mainui_blit(screen, text, x + (width - text->w) / 2, top);
            SDL_FreeSurface(text);
        }
        top += TTF_FontHeight(theme->title_font);
        if (!end) {
            break;
        }
        p = end + 1;
    }
    /* DialogWindow 0x33d98/0x33ed4 uses the same action column for both buttons.
     * The patch redirects their text to the theme hint font and color. */
    int button_y = y + 160;
    for (int i = 0; i < 2; i++) {
        SDL_Surface *button = theme->buttons[i];
        int button_height = button ? button->h : 54;
        mainui_blit(screen, button, x + 230, button_y);
        mainui_label(screen, theme->hint_font, theme->hint_color,
                     mainui_translate(i ? 45 : 46, i ? "CANCEL" : "OK"),
                     x + 230 + (button ? button->w : 0) + 10,
                     button_y + 5 + (button_height - TTF_FontHeight(theme->hint_font)) / 2);
        button_y += button_height + 10;
    }
}

void mainui_draw_catalog_loading(SDL_Surface *screen, MainUITheme *theme, bool cancelling)
{
    if (!theme->loading_background_loaded) {
        theme->loading_background = mainui_theme_image(theme, "skin/pop-bg.png");
        theme->loading_background_loaded = true;
    }
    SDL_Surface *background = theme->loading_background;
    int width = background ? background->w : 480, height = background ? background->h : 300;
    int x = (640 - width) / 2, y = (480 - height) / 2;
    SDL_Rect panel = {(Sint16)x, (Sint16)y, (Uint16)width, (Uint16)height};
    SDL_SetClipRect(screen, &panel);
    if (background) {
        mainui_blit(screen, background, x, y);
    }
    else {
        SDL_FillRect(screen, &panel, SDL_MapRGB(screen->format, 24, 24, 24));
    }
    SDL_Surface *text = TTF_RenderUTF8_Blended(theme->title_font, mainui_translate(76, "Loading"),
                                               theme->title_color);
    if (text) {
        mainui_blit(screen, text, x + (width - text->w) / 2, y + height / 2 - text->h / 2);
        SDL_FreeSurface(text);
    }
    if (!cancelling) {
        SDL_Surface *button = theme->buttons[1];
        SDL_Surface *cancel = TTF_RenderUTF8_Blended(
            theme->hint_font, mainui_translate(45, "CANCEL"), theme->hint_color);
        int total = (button ? button->w + 10 : 0) + (cancel ? cancel->w : 0);
        int left = x + (width - total) / 2;
        if (button) {
            mainui_blit(screen, button, left, y + height - 45 - button->h / 2);
            left += button->w + 10;
        }
        if (cancel) {
            mainui_blit(screen, cancel, left, y + height - 45 - cancel->h / 2);
            SDL_FreeSurface(cancel);
        }
    }
    SDL_SetClipRect(screen, NULL);
}
