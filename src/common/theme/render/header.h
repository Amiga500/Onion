#ifndef RENDER_HEADER_H__
#define RENDER_HEADER_H__

#include "theme/background.h"
#include "theme/config.h"
#include "theme/resources.h"
#include "utils/surfaceSetAlpha.h"

void theme_renderHeaderBackground(SDL_Surface *screen)
{
    SDL_Rect header_size = theme_scaleRect((SDL_Rect){0, 0, 640, 60});
    SDL_BlitSurface(theme_background(), &header_size, screen, &header_size);
    SDL_BlitSurface(resource_getSurface(BG_TITLE), &header_size, screen, &header_size);
}

void theme_renderHeaderBattery(SDL_Surface *screen, int battery_percentage)
{
    SDL_Surface *battery = theme_batterySurface(battery_percentage);
    SDL_Rect battery_rect = {596.0 * g_scale - battery->w / 2, 30.0 * g_scale - battery->h / 2};
    SDL_BlitSurface(battery, NULL, screen, &battery_rect);
    SDL_FreeSurface(battery);
}

void theme_renderHeaderBatteryCustom(SDL_Surface *screen,
                                     int battery_percentage, int header_height)
{
    SDL_Surface *battery = theme_batterySurface(battery_percentage);
    SDL_Rect battery_rect = {596.0 * g_scale - battery->w / 2,
                             header_height / 2 - battery->h / 2};
    SDL_BlitSurface(battery, NULL, screen, &battery_rect);
    SDL_FreeSurface(battery);
}

/* Cache for header title surface — avoids TTF_RenderUTF8_Blended every frame */
static SDL_Surface *_cached_header_title = NULL;
static char _cached_header_title_str[STR_MAX] = "";

void theme_renderHeader_cleanup(void)
{
    if (_cached_header_title) {
        SDL_FreeSurface(_cached_header_title);
        _cached_header_title = NULL;
    }
    _cached_header_title_str[0] = '\0';
}

void theme_renderHeader(SDL_Surface *screen, const char *title_str, bool show_logo)
{
    theme_renderHeaderBackground(screen);

    if (show_logo) {
        SDL_Surface *logo = resource_getSurface(LOGO);
        SDL_Rect logo_rect = {20.0 * g_scale, 30.0 * g_scale - logo->h / 2};
        SDL_BlitSurface(logo, NULL, screen, &logo_rect);
    }

    if (title_str) {
        if (strcmp(title_str, _cached_header_title_str) != 0) {
            if (_cached_header_title)
                SDL_FreeSurface(_cached_header_title);
            _cached_header_title = TTF_RenderUTF8_Blended(resource_getFont(TITLE), title_str, theme()->title.color);
            strncpy(_cached_header_title_str, title_str, STR_MAX - 1);
            _cached_header_title_str[STR_MAX - 1] = '\0';
        }
        if (_cached_header_title) {
            SDL_Rect title_rect = {(g_display.width - _cached_header_title->w) / 2, 29.0 * g_scale - _cached_header_title->h / 2};
            SDL_Rect title_bg = {title_rect.x - 10.0 * g_scale, 0, _cached_header_title->w + 20.0 * g_scale, 60.0 * g_scale};
            SDL_BlitSurface(theme_background(), &title_bg, screen, &title_bg);
            SDL_BlitSurface(resource_getSurface(BG_TITLE), &title_bg, screen, &title_bg);
            SDL_BlitSurface(_cached_header_title, NULL, screen, &title_rect);
        }
    }
}

void theme_renderHeaderExtra(SDL_Surface *screen, const char *title_str,
                             const char *prev_title_str)
{
    theme_renderHeaderBackground(screen);

    SDL_Surface *title = TTF_RenderUTF8_Blended(resource_getFont(TITLE), title_str, theme()->title.color);
    if (title == NULL)
        return;
    SDL_Rect title_rect = {(g_display.width - title->w) / 2, 29.0 * g_scale - title->h / 2};
    SDL_BlitSurface(title, NULL, screen, &title_rect);
    SDL_FreeSurface(title);
}

#endif // RENDER_HEADER_H__
