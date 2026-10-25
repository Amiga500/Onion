#ifndef RENDER_FOOTER_H__
#define RENDER_FOOTER_H__

#include <stdbool.h>
#include <unistd.h>

#include "theme/background.h"
#include "theme/config.h"
#include "theme/resources.h"

// Footer layout, shared with Open MainUI 1.0.10 (Tweaks > Appearance >
// Footer layout). Classic: hints A then B from the left, page number on the
// right. Alternative, when this flag exists: page number on the left, hints
// in button order, B then A, ending on the right.
#ifndef FOOTER_ALT_LAYOUT_FLAG
#define FOOTER_ALT_LAYOUT_FLAG "/mnt/SDCARD/.tmp_update/config/.mainuiFooterAltLayout"
#endif

// -1: not read yet. Read once per process; Tweaks sets it when it changes.
static int _footer_alt_layout = -1;

bool theme_footerAltLayout(void)
{
    if (_footer_alt_layout < 0)
        _footer_alt_layout = access(FOOTER_ALT_LAYOUT_FLAG, F_OK) == 0;
    return _footer_alt_layout == 1;
}

void theme_footerSetAltLayout(bool alt)
{
    _footer_alt_layout = alt ? 1 : 0;
}

// Cached hint label surfaces to avoid TTF_RenderUTF8_Blended every frame
static SDL_Surface *_cached_label_a = NULL;
static SDL_Surface *_cached_label_b = NULL;
static char _cached_label_a_str[STR_MAX] = "";
static char _cached_label_b_str[STR_MAX] = "";

void theme_renderStandardHint_cleanup(void)
{
    if (_cached_label_a) {
        SDL_FreeSurface(_cached_label_a);
        _cached_label_a = NULL;
    }
    if (_cached_label_b) {
        SDL_FreeSurface(_cached_label_b);
        _cached_label_b = NULL;
    }
    _cached_label_a_str[0] = '\0';
    _cached_label_b_str[0] = '\0';
}

static SDL_Surface *_footer_hintLabel(SDL_Surface **cache, char *cache_str,
                                      const char *str)
{
    if (strcmp(str, cache_str) != 0) {
        if (*cache)
            SDL_FreeSurface(*cache);
        TTF_Font *font_hint = resource_getFont(HINT);
        *cache = TTF_RenderUTF8_Blended(font_hint, str, theme()->hint.color);
        strncpy(cache_str, str, STR_MAX - 1);
        cache_str[STR_MAX - 1] = '\0';
    }
    return *cache;
}

// Button icon, 5 px, label: the width _footer_drawHint draws.
static int _footer_hintWidth(SDL_Surface *button, SDL_Surface *label)
{
    return (button ? button->w + 5 : 0) + (label ? label->w : 0);
}

static void _footer_drawHint(SDL_Surface *screen, SDL_Surface *button,
                             SDL_Surface *label, int x)
{
    if (button) {
        SDL_Rect button_rect = {x, 450.0 * g_scale - button->h / 2, 0, 0};
        SDL_BlitSurface(button, NULL, screen, &button_rect);
        x += button->w + 5;
    }
    if (label) {
        SDL_Rect label_rect = {x, 449.0 * g_scale - label->h / 2, 0, 0};
        SDL_BlitSurface(label, NULL, screen, &label_rect);
    }
}

void theme_renderStandardHint(SDL_Surface *screen, const char *btn_a_str,
                              const char *btn_b_str)
{
    char label_a_str[STR_MAX] = " ", label_b_str[STR_MAX] = " ";

    if (!theme()->hideLabels.hints) {
        if (btn_a_str != NULL)
            strncpy(label_a_str, btn_a_str, STR_MAX - 1);
        if (btn_b_str != NULL)
            strncpy(label_b_str, btn_b_str, STR_MAX - 1);
    }

    bool show_b = btn_b_str != NULL && label_b_str[0] != '\0';

    SDL_Surface *button_a = resource_getSurface(BUTTON_A);
    SDL_Surface *label_a = _footer_hintLabel(&_cached_label_a, _cached_label_a_str, label_a_str);
    SDL_Surface *button_b = show_b ? resource_getSurface(BUTTON_B) : NULL;
    SDL_Surface *label_b = show_b ? _footer_hintLabel(&_cached_label_b, _cached_label_b_str, label_b_str) : NULL;

    int gap = 30.0 * g_scale; // between the A and B hints
    int width_a = _footer_hintWidth(button_a, label_a);

    if (theme_footerAltLayout()) {
        int x_a = 620.0 * g_scale - width_a;
        _footer_drawHint(screen, button_a, label_a, x_a);
        if (show_b)
            _footer_drawHint(screen, button_b, label_b,
                             x_a - gap - _footer_hintWidth(button_b, label_b));
        return;
    }

    int x_a = 20.0 * g_scale;
    _footer_drawHint(screen, button_a, label_a, x_a);
    if (show_b)
        _footer_drawHint(screen, button_b, label_b,
                         x_a + width_a + (label_a ? gap : 0));
}

void theme_renderFooter(SDL_Surface *screen)
{
    int footer_height = 60.0 * g_scale;
    SDL_Rect footer_rect = {0, g_display.height - footer_height, g_display.width, footer_height};
    SDL_BlitSurface(theme_background(), &footer_rect, screen, &footer_rect);
    SDL_BlitSurface(resource_getSurface(BG_FOOTER), NULL, screen, &footer_rect);
}

static int old_status_width = -1;
static int old_status_x = 0;
static bool old_status_alt = false;

static SDL_Surface *_footer_current_cache = NULL;
static SDL_Surface *_footer_total_cache = NULL;
static int _footer_current_num = -1;
static int _footer_total_num = -1;

void theme_renderFooterStatus(SDL_Surface *screen, int current_num,
                              int total_num)
{
    bool alt = theme_footerAltLayout();

    // Clear the previous number, only where it was drawn in this layout: after
    // a layout change the whole footer has been redrawn, and the old place now
    // holds hints.
    if (old_status_width != -1 && old_status_alt == alt) {
        SDL_Rect status_pos = theme_scaleRect((SDL_Rect){0, 420, 0, 60});
        status_pos.x = old_status_x;
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
        int current_w = _footer_current_cache ? _footer_current_cache->w : 0;
        int current_x = alt ? (int)(20.0 * g_scale)
                            : (int)(620.0 * g_scale) - _footer_total_cache->w - current_w;
        SDL_Rect total_rect = {current_x + current_w, (int)(449.0 * g_scale) - _footer_total_cache->h / 2, 0, 0};

        SDL_BlitSurface(_footer_total_cache, NULL, screen, &total_rect);
        if (_footer_current_cache) {
            SDL_Rect current_rect = {current_x, (int)(449.0 * g_scale) - _footer_current_cache->h / 2, 0, 0};
            old_status_width = _footer_total_cache->w + current_w;
            old_status_x = current_x;
            old_status_alt = alt;
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
