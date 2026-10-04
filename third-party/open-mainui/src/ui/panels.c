/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/panels.h"
#include "ui/drawing.h"
#include <SDL_image.h>
#include <stdio.h>
#include <string.h>

static void list_frame(SDL_Surface *screen, MainUITheme *theme, const char *title)
{
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 24, 24, 24));
    mainui_blit(screen, theme->background, 0, 0);
    mainui_draw_header(screen, theme, title);
    mainui_draw_footer(screen, theme, 0, -1);
}

void mainui_draw_settings(SDL_Surface *screen, MainUITheme *theme,
                          const MainUIStockSettings *settings)
{
    list_frame(screen, theme, mainui_translate(15, "Settings"));
    const MainUISettingsArtwork *art = mainui_theme_settings_artwork(theme);
    SDL_Surface *selection = art->selection;
    int start = settings->start;
    for (int i = start; i < settings->count && i < start + 6; i++) {
        int y = 60 + (i - start) * 60;
        SDL_Rect clip = {0, (Sint16)y, 640, 60};
        SDL_SetClipRect(screen, &clip);
        if (i == settings->selected) {
            mainui_blit(screen, selection, 0, y);
        }
        SDL_Surface *icon = art->icons[settings->rows[i]];
        if (icon) {
            mainui_blit(screen, icon, 20, y + (60 - icon->h) / 2);
        }
        mainui_label(screen, theme->menu_font, theme->color,
                     mainui_stock_setting_label(settings->rows[i]), 80,
                     y + (60 - TTF_FontHeight(theme->menu_font)) / 2);
        MainUISettingKind kind = settings->rows[i];
        if (kind == SET_WIFI && theme->wifi_online && *theme->wifi_address) {
            int width = 0;
            TTF_SizeUTF8(theme->menu_font, theme->wifi_address, &width, NULL);
            mainui_label(screen, theme->menu_font, theme->color, theme->wifi_address, 600 - width,
                         y + (60 - TTF_FontHeight(theme->menu_font)) / 2);
        }
        if (kind == SET_BRIGHTNESS || kind == SET_SOUND || kind == SET_SLEEP) {
            /* Stock Settings renderer 0x28f34 reserves a 200px value lane
             * between the independently themed 24px arrow surfaces. */
            SDL_Surface *left = art->left;
            SDL_Surface *right = art->right;
            int right_width = right ? right->w : 24;
            if (left) {
                mainui_blit(screen, left, 640 - right_width - left->w - 240,
                            y + (60 - left->h) / 2);
            }
            if (right) {
                mainui_blit(screen, right, 640 - right_width - 40, y + (60 - right->h) / 2);
            }
            char value[48];
            if (kind == SET_SLEEP) {
                if (settings->values[kind]) {
                    snprintf(value, sizeof value, "%d min", settings->values[kind]);
                }
                else {
                    snprintf(value, sizeof value, "%s", mainui_translate(114, "Off"));
                }
            }
            else {
                snprintf(value, sizeof value, "%02d/%02d", settings->values[kind],
                         kind == SET_BRIGHTNESS ? 10 : 20);
            }
            SDL_Surface *number = TTF_RenderUTF8_Blended(theme->menu_font, value, theme->color);
            if (number) {
                mainui_blit(screen, number, 640 - right_width - 240 + (200 - number->w) / 2,
                            y + (60 - number->h) / 2);
                SDL_FreeSurface(number);
            }
        }
    }
    SDL_SetClipRect(screen, NULL);
}

void mainui_draw_apps(SDL_Surface *screen, MainUITheme *theme, MainUICatalog *apps,
                      const MainUIViewport *view)
{
    list_frame(screen, theme, mainui_translate(107, "Apps"));
    if (!view->total) {
        mainui_draw_empty(screen, theme);
        return;
    }
    /* Apps constructor 0x1bc80 requests four rows, then sets row height90
     * at 0x1bc94; it passes bg-list-l.png, not the console-grid tiles. */
    if (!theme->apps_selection_loaded) {
        theme->apps_selection = mainui_theme_image(theme, "skin/bg-list-l.png");
        theme->apps_selection_loaded = true;
    }
    SDL_Surface *selection = theme->apps_selection;
    /* TextMenu 0x1f8b0 draws row separators before selection/text. */
    SDL_Rect content = {0, 60, 640, 360};
    SDL_SetClipRect(screen, &content);
    for (int i = 1; i < 4; i++) {
        mainui_blit(screen, theme->divider, 0, 60 + i * 90);
    }
    for (int i = view->start; i <= view->end; i++) {
        MainUIEntry *app = mainui_catalog_entry(apps, i);
        if (!app) {
            continue;
        }
        int y = 62 + (i - view->start) * 90;
        SDL_Rect clip = {0, (Sint16)y, 640, (Uint16)(y + 90 > 420 ? 420 - y : 90)};
        SDL_SetClipRect(screen, &clip);
        if (i == view->selected) {
            mainui_blit(screen, selection, 0,
                        y + (selection && selection->h < 90 ? (90 - selection->h) / 2 : 0));
        }
        SDL_Surface *icon = mainui_theme_console_icon(theme, app->icon);
        int x = 20;
        if (icon) {
            /* Stock 0x20090..0x201e8 reserves a fixed 71px icon lane.
             * Only icons exceeding the row in both dimensions are center-cropped. */
            if (icon->w > 90 && icon->h > 90) {
                SDL_Rect source = {(Sint16)((icon->w - 71) / 2), (Sint16)((icon->h - 71) / 2), 71,
                                   71};
                SDL_Rect destination = {20, (Sint16)y, 0, 0};
                SDL_BlitSurface(icon, &source, screen, &destination);
            }
            else {
                mainui_blit(screen, icon, 20, y + (90 - icon->h) / 2);
            }
            x = 111;
            SDL_FreeSurface(icon);
        }
        bool description = app->description && *app->description;
        mainui_label(screen, theme->menu_font, theme->color, app->label, x,
                     description ? y + 20 : y + (90 - TTF_FontHeight(theme->menu_font)) / 2);
        if (description) {
            mainui_label(screen, theme->menu_font, theme->grid_color[0], app->description, x,
                         y + 45);
        }
    }

    SDL_SetClipRect(screen, NULL);
}

void mainui_draw_message(SDL_Surface *screen, MainUITheme *theme, const char *title,
                         const char *body)
{
    SDL_Rect panel = {20, 145, 600, 190};
    SDL_FillRect(screen, &panel, SDL_MapRGB(screen->format, 24, 24, 24));
    SDL_Rect clip = {35, 150, 570, 180};
    SDL_SetClipRect(screen, &clip);
    mainui_label(screen, theme->title_font, theme->title_color, title, 35, 160);
    /* Messages are deliberately bounded. Long device paths remain clipped;
     * no path is ever interpreted as a format string or a host command. */
    mainui_label(screen, theme->title_font, theme->color, body, 35, 215);
    mainui_label(screen, theme->title_font, theme->hint_color, "A / B: close", 35, 280);
    SDL_SetClipRect(screen, NULL);
}

void mainui_draw_languages(SDL_Surface *screen, MainUITheme *theme, const MainUILanguages *list)
{
    list_frame(screen, theme, mainui_translate(23, "Change language"));
    SDL_Surface *selection = mainui_theme_image(theme, "skin/bg-list-s.png");
    int start = list->start;
    for (int i = start; i < list->count && i < start + 6; i++) {
        int y = 60 + (i - start) * 60;
        SDL_Rect clip = {0, (Sint16)y, 640, 60};
        SDL_SetClipRect(screen, &clip);
        if (i == list->selected) {
            mainui_blit(screen, selection, 0, y);
        }
        mainui_label(screen, theme->menu_font, theme->color, list->entries[i].name, 20,
                     y + (60 - TTF_FontHeight(theme->menu_font)) / 2);
    }
    if (selection) {
        SDL_FreeSurface(selection);
    }
    SDL_SetClipRect(screen, NULL);
}
