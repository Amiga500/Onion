/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/name_input.h"
#include "ui/drawing.h"
#include "ui/panels.h"
#include <stdio.h>
#include <string.h>

/* Recovered from ImeWindow's key table (0x17fbd4) and descriptors in the
 * clean reference binary. Symbols replace letters without changing geometry. */
static const char characters[] = "1234567890qwertyuiopasdfghjkl-zxcvbnm()/";
static const char symbols[] = "1234567890!@#$%^&*/?~_+{}|:\"<>`-=[]\\;',.";

_Static_assert(sizeof characters == 41 && sizeof symbols == 41,
               "Stock keyboard has 40 character keys");

static char key_character(const MainUINameInput *input, int index)
{
    char value = input->symbols ? symbols[index] : characters[index];
    if (!input->symbols && input->shift && value >= 'a' && value <= 'z') {
        value -= 'a' - 'A';
    }
    return value;
}

void mainui_name_input_close(MainUINameInput *input)
{
    SDL_Surface *surfaces[] = {input->normal,      input->focused,     input->field,
                               input->space,       input->erase,       input->caps,
                               input->accept,      input->wide_normal, input->wide_focused,
                               input->caps_active, input->abc,         input->symbols_icon,
                               input->hints[0],    input->hints[1],    input->hints[2],
                               input->hints[3],    input->hints[4]};
    for (size_t i = 0; i < sizeof surfaces / sizeof surfaces[0]; i++) {
        if (surfaces[i]) {
            SDL_FreeSurface(surfaces[i]);
        }
    }
    for (int i = 0; i < 128; ++i) {
        if (input->glyphs[i]) {
            SDL_FreeSurface(input->glyphs[i]);
        }
    }
    *input = (MainUINameInput){0};
}

void mainui_name_input_open(MainUINameInput *input, const MainUITheme *theme, int title_id,
                            const char *initial)
{
    mainui_name_input_close(input);
    input->open = true;
    input->title_id = title_id;
    /* Refuse truncation through a UTF-8 sequence; existing overlong legacy names
     * must be replaced with a supported name rather than silently shortened. */
    if (initial && strlen(initial) < sizeof input->text) {
        strcpy(input->text, initial);
    }
    input->normal = mainui_theme_image(theme, "skin/bg-btn-01-n.png");
    input->focused = mainui_theme_image(theme, "skin/bg-btn-01-f.png");
    input->wide_normal = mainui_theme_image(theme, "skin/bg-btn-02-n.png");
    input->wide_focused = mainui_theme_image(theme, "skin/bg-btn-02-f.png");
    input->caps_active = mainui_theme_image(theme, "skin/icon-shift-active.png");
    input->abc = mainui_theme_image(theme, "skin/icon-abc.png");
    input->symbols_icon = mainui_theme_image(theme, "skin/icon-character.png");
    input->hints[0] = mainui_theme_image(theme, "skin/icon-L2.png");
    input->hints[1] = mainui_theme_image(theme, "skin/icon-R2.png");
    input->hints[2] = mainui_theme_image(theme, "skin/icon-x.png");
    input->hints[3] = mainui_theme_image(theme, "skin/icon-y.png");
    input->hints[4] = mainui_theme_image(theme, "skin/icon-START.png");
    input->field = mainui_theme_image(theme, "skin/input.png");
    input->space = mainui_theme_image(theme, "skin/icon-space.png");
    input->erase = mainui_theme_image(theme, "skin/icon-delete.png");
    input->caps = mainui_theme_image(theme, "skin/icon-shift.png");
    input->accept = mainui_theme_image(theme, "skin/icon-OK.png");
}

static void erase(MainUINameInput *input)
{
    size_t length = strlen(input->text);
    if (!length) {
        return;
    }
    do {
        length--;
    } while (length && ((unsigned char)input->text[length] & 0xc0) == 0x80);
    input->text[length] = 0;
}

static void character(MainUINameInput *input, unsigned code)
{
    char bytes[4] = {0};
    if (code < 32 || code == 127 || (code >= 0xd800 && code <= 0xdfff)) {
        return;
    }
    if (code < 0x80) {
        bytes[0] = (char)code;
    }
    else if (code < 0x800) {
        bytes[0] = (char)(0xc0 | (code >> 6));
        bytes[1] = (char)(0x80 | (code & 63));
    }
    else {
        bytes[0] = (char)(0xe0 | (code >> 12));
        bytes[1] = (char)(0x80 | ((code >> 6) & 63));
        bytes[2] = (char)(0x80 | (code & 63));
    }
    if (strlen(input->text) + strlen(bytes) < sizeof input->text) {
        strcat(input->text, bytes);
    }
}

MainUINameResult mainui_name_input_key(MainUINameInput *input, const SDL_keysym *key)
{
    input->error[0] = 0;
    switch (key->sym) {
    case SDLK_ESCAPE:
    case SDLK_LCTRL:
        return NAME_CANCEL;
    case SDLK_F2:
        return NAME_SUBMIT;
    case SDLK_LEFT:
    case SDLK_RIGHT: {
        int columns = input->selected < 40 ? 10 : 5;
        int first = input->selected < 40 ? input->selected / 10 * 10 : 40;
        int direction = key->sym == SDLK_LEFT ? -1 : 1;
        input->selected = first + (input->selected - first + columns + direction) % columns;
        break;
    }
    case SDLK_UP:
        if (input->selected >= 40) {
            input->selected = 30 + (input->selected - 40) * 2;
        }
        else if (input->selected < 10) {
            input->selected = 40 + input->selected / 2;
        }
        else {
            input->selected -= 10;
        }
        break;
    case SDLK_DOWN:
        if (input->selected >= 40) {
            input->selected = (input->selected - 40) * 2;
        }
        else if (input->selected >= 30) {
            input->selected = 40 + (input->selected - 30) / 2;
        }
        else {
            input->selected += 10;
        }
        break;
    case SDLK_HOME:
        input->shift = !input->shift;
        break;
    case SDLK_END:
        input->symbols = !input->symbols;
        break;
    case SDLK_LSHIFT:
        character(input, ' ');
        break;
    case SDLK_LALT:
    case SDLK_BACKSPACE:
        erase(input);
        break;
    case SDLK_RETURN:
        if (input->selected < 40) {
            character(input, (unsigned char)key_character(input, input->selected));
        }
        else if (input->selected == 40) {
            input->shift = !input->shift;
        }
        else if (input->selected == 41) {
            input->symbols = !input->symbols;
        }
        else if (input->selected == 42) {
            character(input, ' ');
        }
        else if (input->selected == 43) {
            erase(input);
        }
        else {
            return NAME_SUBMIT;
        }
        break;
    default:
        if (key->unicode) {
            character(input, key->unicode);
        }
        break;
    }
    return NAME_EDITING;
}

void mainui_name_input_draw(MainUINameInput *input, SDL_Surface *screen, MainUITheme *theme)
{
    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 24, 24, 24));
    mainui_blit(screen, theme->background, 0, 0);
    const char *fallback = input->title_id == 151   ? "Input SSID"
                           : input->title_id == 152 ? "Input Password"
                           : input->title_id == 153 ? "Search"
                           : input->title_id == 400 ? "Create folder"
                                                    : "Rename folder";
    mainui_draw_header(screen, theme, mainui_translate(input->title_id, fallback));
    /* Stock ImeWindow clears only the area between the shared frame bars. */
    SDL_Rect body = {0, 62, 640, 358};
    SDL_FillRect(screen, &body, SDL_MapRGB(screen->format, 0, 0, 0));
    mainui_blit(screen, input->field, 24, 64);
    SDL_Rect field_clip = {40, 66, 560, 48};
    SDL_SetClipRect(screen, &field_clip);
    /* ImeWindow uses its zero-initialized black SDL_Color (0x182498) for
     * input.png, independently of the theme's light list text color. */
    SDL_Color field_color = {0, 0, 0, 0};
    char masked[128];
    memset(masked, '*', strlen(input->text));
    masked[strlen(input->text)] = 0;
    SDL_Surface *text =
        TTF_RenderUTF8_Blended(theme->menu_font, input->secret ? masked : input->text, field_color);
    if (text) {
        int x = text->w > field_clip.w ? field_clip.x + field_clip.w - text->w : field_clip.x;
        mainui_blit(screen, text, x, 90 - text->h / 2);
        SDL_FreeSurface(text);
    }
    SDL_SetClipRect(screen, NULL);
    for (int i = 0; i < 45; i++) {
        int x = i < 40 ? 24 + (i % 10) * 60 : 24 + (i - 40) * 120;
        int y = i < 40 ? 126 + (i / 10) * 60 : 362;
        int width = i < 40 ? 52 : 112;
        SDL_Rect clip = {(Sint16)x, (Sint16)y, (Uint16)width, 52};
        SDL_SetClipRect(screen, &clip);
        SDL_Surface *normal = i < 40 ? input->normal : input->wide_normal;
        SDL_Surface *focused = i < 40 ? input->focused : input->wide_focused;
        mainui_blit(screen, i == input->selected ? focused : normal, x, y);
        if (i < 40) {
            char value[2] = {key_character(input, i), 0};
            unsigned char code = (unsigned char)value[0];
            if (!input->glyphs[code]) {
                input->glyphs[code] = TTF_RenderUTF8_Blended(
                    theme->menu_font, value,
                    input->title_id == 153 ? (SDL_Color){255, 255, 255, 0} : theme->color);
            }
            SDL_Surface *glyph = input->glyphs[code];
            if (glyph) {
                mainui_blit(screen, glyph, x + (52 - glyph->w) / 2, y + (52 - glyph->h) / 2);
            }
        }
        else {
            SDL_Surface *controls[] = {input->shift ? input->caps_active : input->caps,
                                       input->symbols ? input->abc : input->symbols_icon,
                                       input->space, input->erase, input->accept};
            SDL_Surface *icon = controls[i - 40];
            SDL_Surface *hint = input->hints[i - 40];
            /* The stock renderer reserves a right-hand hint region, with six
             * pixels for wide START artwork and thirteen for smaller buttons. */
            int reserved = hint ? hint->w + (hint->w > 40 ? 6 : 13) : 0;
            if (hint) {
                mainui_blit(screen, hint, x + width - reserved, y + (52 - hint->h) / 2);
            }
            if (icon) {
                mainui_blit(screen, icon, x + (width - reserved - icon->w) / 2,
                            y + (52 - icon->h) / 2);
            }
        }
        SDL_SetClipRect(screen, NULL);
    }
    if (*input->error) {
        SDL_Rect clip = {24, 66, 592, 48};
        SDL_FillRect(screen, &clip, SDL_MapRGB(screen->format, 0, 0, 0));
        SDL_SetClipRect(screen, &clip);
        mainui_label(screen, theme->description_font, theme->color, input->error, 28, 90);
        SDL_SetClipRect(screen, NULL);
    }
    mainui_draw_popup_footer(screen, theme);
}
