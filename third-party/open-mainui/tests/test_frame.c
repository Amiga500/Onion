/* SPDX-License-Identifier: GPL-3.0-only */
#include "support.h"
#include "ui/drawing.h"
#include "ui/panels.h"
#include "ui/theme.h"
#include <stdio.h>
#include <string.h>

static MainUITheme theme;
static SDL_Surface *screen, *expected;

static void clear(SDL_Surface *target)
{
    SDL_FillRect(target, NULL, SDL_MapRGB(target->format, 40, 80, 120));
}

static bool same(void)
{
    for (int y = 0; y < screen->h; y++) {
        if (memcmp((Uint8 *)screen->pixels + y * screen->pitch,
                   (Uint8 *)expected->pixels + y * expected->pitch, (size_t)screen->w * 4)) {
            return false;
        }
    }
    return true;
}

static SDL_Surface *text(const char *value, SDL_Color color)
{
    return mainui_theme_text(&theme, theme.hint_font, value, color);
}

static void background(SDL_Surface *target)
{
    SDL_Rect clip = {0, 420, 640, 60};
    SDL_SetClipRect(target, &clip);
    if (theme.background) {
        SDL_Rect destination = clip;
        SDL_BlitSurface(theme.background, &clip, target, &destination);
    }
    mainui_blit(target, theme.footer, 0, 420);
}

/* The footer as v1.0.9 drew it. */
static void footer_v109(SDL_Surface *target, int page, int total, const char *open,
                        const char *back)
{
    background(target);
    int x = 20;
    const char *hints[] = {theme.hide_hints ? " " : open, theme.hide_hints ? " " : back};
    for (int i = 0; i < 2; i++) {
        SDL_Surface *button = theme.buttons[i];
        if (button) {
            mainui_blit(target, button, x, 450 - button->h / 2);
            x += button->w + 5;
        }
        if (!theme.hide_hint_text) {
            SDL_Surface *hint = text(hints[i], theme.hint_color);
            if (hint) {
                mainui_blit(target, hint, x, 449 - hint->h / 2);
                x += hint->w;
            }
        }
        x += 30;
    }
    if (total >= 0 && !theme.hide_hint_text) {
        char value[48];
        snprintf(value, sizeof value, "%d", total);
        SDL_Surface *last = text(value, theme.total_color);
        snprintf(value, sizeof value, "%d/", total ? page : 0);
        SDL_Surface *current = text(value, theme.page_color);
        int edge = total > 99 ? 600 : 620;
        if (last) {
            edge -= last->w;
            mainui_blit(target, last, edge, 449 - last->h / 2);
        }
        if (current) {
            mainui_blit(target, current, edge - current->w, 449 - current->h / 2);
        }
    }
    SDL_SetClipRect(target, NULL);
}

/* The alternative as specified: the counter from x 20, then B's hint and A's,
 * the last ending at x 620, each its button, 5 px and its text, 30 px apart. */
static void footer_alternative(SDL_Surface *target, int page, int total, const char *open,
                               const char *back)
{
    background(target);
    const char *hints[] = {theme.hide_hints ? " " : open, theme.hide_hints ? " " : back};
    int width[2] = {0, 0};
    for (int i = 0; i < 2; i++) {
        width[i] = (theme.buttons[i] ? theme.buttons[i]->w + 5 : 0) +
                   (theme.hide_hint_text ? 0 : text(hints[i], theme.hint_color)->w);
    }
    int x = 620 - width[1] - 30 - width[0];
    for (int i = 1; i >= 0; i--) {
        int left = x;
        if (theme.buttons[i]) {
            mainui_blit(target, theme.buttons[i], left, 450 - theme.buttons[i]->h / 2);
            left += theme.buttons[i]->w + 5;
        }
        if (!theme.hide_hint_text) {
            SDL_Surface *hint = text(hints[i], theme.hint_color);
            mainui_blit(target, hint, left, 449 - hint->h / 2);
        }
        x += width[i] + 30;
    }
    if (total >= 0 && !theme.hide_hint_text) {
        char value[48];
        snprintf(value, sizeof value, "%d/", total ? page : 0);
        SDL_Surface *current = text(value, theme.page_color);
        mainui_blit(target, current, 20, 449 - current->h / 2);
        snprintf(value, sizeof value, "%d", total);
        SDL_Surface *last = text(value, theme.total_color);
        mainui_blit(target, last, 20 + current->w, 449 - last->h / 2);
    }
    SDL_SetClipRect(target, NULL);
}

static void check_list(int page, int total)
{
    clear(screen);
    clear(expected);
    mainui_draw_footer(screen, &theme, page, total);
    (theme.footer_alt ? footer_alternative : footer_v109)(
        expected, page, total, mainui_translate(88, "SELECT"), mainui_translate(89, "BACK"));
    assert(same());
}

static void check_popup(void)
{
    clear(screen);
    clear(expected);
    mainui_draw_popup_footer(screen, &theme);
    (theme.footer_alt ? footer_alternative : footer_v109)(
        expected, 0, -1, mainui_translate(46, "OK"), mainui_translate(45, "CANCEL"));
    assert(same());
}

static void check_action(void)
{
    clear(screen);
    clear(expected);
    mainui_draw_action_footer(screen, &theme, "Apply changes", "BACK");
    (theme.footer_alt ? footer_alternative : footer_v109)(expected, 0, -1, "Apply changes", "BACK");
    assert(same());
}

/* A Favorites folder's footer: hints, and its game count where the page
 * counter goes, at the right as in v1.0.9 or from x 20. */
static void check_folder(int count)
{
    clear(screen);
    clear(expected);
    mainui_draw_folder_footer(screen, &theme, count);
    (theme.footer_alt ? footer_alternative : footer_v109)(
        expected, 0, -1, mainui_translate(88, "SELECT"), mainui_translate(89, "BACK"));
    if (!theme.hide_hint_text) {
        char value[32];
        snprintf(value, sizeof value, "(%d)", count);
        SDL_Surface *games = text(value, theme.total_color);
        int left = theme.footer_alt ? 20 : (count > 99 ? 600 : 620) - games->w;
        mainui_blit(expected, games, left, 450 - games->h / 2);
    }
    assert(same());
}

static void check_all(void)
{
    check_list(3, 24);
    check_list(7, 130);
    check_list(0, 0);
    check_list(0, -1);
    check_popup();
    check_action();
    check_folder(5);
    check_folder(120);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: fixture-frame <onion-builtin-theme-dir>\n");
        return 2;
    }
    assert(SDL_Init(0) == 0 && TTF_Init() == 0);
    screen = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 32, 0xff0000, 0xff00, 0xff, 0);
    expected = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 480, 32, 0xff0000, 0xff00, 0xff, 0);
    assert(screen && expected);
    for (int alternative = 0; alternative < 2; alternative++) {
        MainUIConfig config;
        mainui_config_parse(&config, NULL, NULL, NULL, NULL);
        assert(!config.footer_alt);
        config.footer_alt = alternative;
        assert(mainui_theme_open(&theme, argv[1], argv[1], &config));
        assert(theme.footer_alt == (bool)alternative && theme.buttons[0] && theme.buttons[1]);
        check_all();
        /* Theme settings that hide hints or their text apply to both. */
        theme.hide_hints = true;
        check_all();
        theme.hide_hints = false;
        theme.hide_hint_text = true;
        check_all();
        theme.hide_hint_text = false;
        /* Without button art, the hints are their text alone. */
        SDL_Surface *buttons[2] = {theme.buttons[0], theme.buttons[1]};
        theme.buttons[0] = theme.buttons[1] = NULL;
        check_all();
        theme.buttons[0] = buttons[0];
        theme.buttons[1] = buttons[1];
        mainui_theme_close(&theme);
    }
    SDL_FreeSurface(screen);
    SDL_FreeSurface(expected);
    TTF_Quit();
    SDL_Quit();
    puts("Footers: stock's layout as in v1.0.9, and the alternative with the counter left and "
         "B then A right, also with hints, their text or button art hidden");
    return 0;
}
