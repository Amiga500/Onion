#ifndef RENDER_FOOTER_H__
#define RENDER_FOOTER_H__

#include "theme/background.h"
#include "theme/config.h"
#include "theme/resources.h"

// Cached hint label surfaces to avoid TTF_RenderUTF8_Blended every frame
static SDL_Surface *_cached_label_a = NULL;
static SDL_Surface *_cached_label_b = NULL;
static char _cached_label_a_str[STR_MAX] = "";
static char _cached_label_b_str[STR_MAX] = "";

void theme_renderStandardHint_cleanup(void)
{
    if (_cached_label_a) { SDL_FreeSurface(_cached_label_a); _cached_label_a = NULL; }
    if (_cached_label_b) { SDL_FreeSurface(_cached_label_b); _cached_label_b = NULL; }
    _cached_label_a_str[0] = '\0';
    _cached_label_b_str[0] = '\0';
}

void theme_renderStandardHint(SDL_Surface *screen, const char *btn_a_str,
                              const char *btn_b_str)
{
    int offsetX = 20.0 * g_scale;
    char label_a_str[STR_MAX] = " ", label_b_str[STR_MAX] = " ";

    if (!theme()->hideLabels.hints) {
        if (btn_a_str != NULL)
            strncpy(label_a_str, btn_a_str, STR_MAX - 1);
        if (btn_b_str != NULL)
            strncpy(label_b_str, btn_b_str, STR_MAX - 1);
    }

    SDL_Rect btn_a_rect = {offsetX, 450.0 * g_scale};
    SDL_Rect label_open_rect = {0, 449.0 * g_scale};

    SDL_Surface *button_a = resource_getSurface(BUTTON_A);

    if (button_a) {
        btn_a_rect.y -= button_a->h / 2;
        SDL_BlitSurface(button_a, NULL, screen, &btn_a_rect);
        offsetX += button_a->w + 5;
    }

    // Cache label_a surface — only re-render when text changes
    if (strcmp(label_a_str, _cached_label_a_str) != 0) {
        if (_cached_label_a) SDL_FreeSurface(_cached_label_a);
        TTF_Font *font_hint = resource_getFont(HINT);
        _cached_label_a = TTF_RenderUTF8_Blended(font_hint, label_a_str, theme()->hint.color);
        strncpy(_cached_label_a_str, label_a_str, STR_MAX - 1);
        _cached_label_a_str[STR_MAX - 1] = '\0';
    }

    if (_cached_label_a) {
        label_open_rect.x = offsetX;
        label_open_rect.y -= _cached_label_a->h / 2;
        SDL_BlitSurface(_cached_label_a, NULL, screen, &label_open_rect);
        offsetX += _cached_label_a->w + 30.0 * g_scale;
    }

    if (btn_b_str != NULL && label_b_str[0] != '\0') {
        SDL_Surface *button_b = resource_getSurface(BUTTON_B);
        SDL_Rect btn_b_rect = {offsetX, 450.0 * g_scale - button_b->h / 2};
        SDL_BlitSurface(button_b, NULL, screen, &btn_b_rect);
        offsetX += button_b->w + 5;

        // Cache label_b surface — only re-render when text changes
        if (strcmp(label_b_str, _cached_label_b_str) != 0) {
            if (_cached_label_b) SDL_FreeSurface(_cached_label_b);
            TTF_Font *font_hint = resource_getFont(HINT);
            _cached_label_b = TTF_RenderUTF8_Blended(font_hint, label_b_str, theme()->hint.color);
            strncpy(_cached_label_b_str, label_b_str, STR_MAX - 1);
            _cached_label_b_str[STR_MAX - 1] = '\0';
        }

        if (_cached_label_b) {
            SDL_Rect label_back_rect = {offsetX, 449.0 * g_scale - _cached_label_b->h / 2};
            SDL_BlitSurface(_cached_label_b, NULL, screen, &label_back_rect);
        }
    }
}

void theme_renderFooter(SDL_Surface *screen)
{
    int footer_height = 60.0 * g_scale;
    SDL_Rect footer_rect = {0, g_display.height - footer_height, g_display.width, footer_height};
    SDL_BlitSurface(theme_background(), &footer_rect, screen, &footer_rect);
    SDL_BlitSurface(resource_getSurface(BG_FOOTER), NULL, screen, &footer_rect);
}

static int old_status_width = -1;

static SDL_Surface *_footer_current_cache = NULL;
static SDL_Surface *_footer_total_cache = NULL;
static int _footer_current_num = -1;
static int _footer_total_num = -1;

void theme_renderFooterStatus(SDL_Surface *screen, int current_num,
                              int total_num)
{
    if (old_status_width != -1) {
        SDL_Rect status_pos = theme_scaleRect((SDL_Rect){620, 420, 0, 60});
        status_pos.x -= old_status_width;
        status_pos.w = old_status_width;

        SDL_Rect status_size = {status_pos.x, 0, old_status_width, 60.0 * g_scale};

        SDL_BlitSurface(theme_background(), &status_pos, screen, &status_pos);
        SDL_BlitSurface(resource_getSurface(BG_FOOTER), &status_size, screen, &status_pos);
    }

    TTF_Font *font_hint = resource_getFont(HINT);

    if (total_num == 0)
        current_num = 0;

    // Cache current number surface — only re-render when value changes
    if (current_num != _footer_current_num) {
        if (_footer_current_cache)
            SDL_FreeSurface(_footer_current_cache);
        char current_str[16];
        snprintf(current_str, sizeof(current_str), "%d/", current_num);
        _footer_current_cache = TTF_RenderUTF8_Blended(font_hint, current_str, theme()->currentpage.color);
        _footer_current_num = current_num;
    }

    // Cache total number surface — only re-render when value changes
    if (total_num != _footer_total_num) {
        if (_footer_total_cache)
            SDL_FreeSurface(_footer_total_cache);
        char total_str[16];
        snprintf(total_str, sizeof(total_str), "%d", total_num);
        _footer_total_cache = TTF_RenderUTF8_Blended(font_hint, total_str, theme()->total.color);
        _footer_total_num = total_num;
    }

    if (_footer_total_cache) {
        SDL_Rect current_rect = {0, 0};
        SDL_Rect total_rect = {(int)(620.0 * g_scale) - _footer_total_cache->w, (int)(449.0 * g_scale) - _footer_total_cache->h / 2};

        SDL_BlitSurface(_footer_total_cache, NULL, screen, &total_rect);
        if (_footer_current_cache) {
            current_rect.x = total_rect.x - _footer_current_cache->w;
            current_rect.y = (int)(449.0 * g_scale) - _footer_current_cache->h / 2;
            old_status_width = _footer_total_cache->w + _footer_current_cache->w;
            SDL_BlitSurface(_footer_current_cache, NULL, screen, &current_rect);
        }
    }
}

void theme_renderListFooter(SDL_Surface *screen, int current_num, int total_num,
                            const char *label_a_str, const char *label_b_str)
{
    theme_renderFooter(screen);
    theme_renderStandardHint(screen, label_a_str, label_b_str);
    theme_renderFooterStatus(screen, current_num, total_num);
}

#endif // RENDER_FOOTER_H__
