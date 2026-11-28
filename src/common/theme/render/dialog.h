#ifndef THEME_RENDER_DIALOG_H__
#define THEME_RENDER_DIALOG_H__

#include "theme/config.h"
#include "theme/resources.h"
#include "utils/surfaceSetAlpha.h"

#include "./textbox.h"
#include "./unified.h"

#define DIALOG_WIDTH 450
#define DIALOG_LINE_HEIGHT 30

static int dialog_progress = 0;

// Cached dialog surfaces to avoid per-call allocation
static SDL_Surface *_dialog_transparent_bg = NULL;
static SDL_Surface *_dialog_label_ok = NULL;
static SDL_Surface *_dialog_label_cancel = NULL;

void theme_renderDialog_cleanup(void)
{
    if (_dialog_transparent_bg) {
        SDL_FreeSurface(_dialog_transparent_bg);
        _dialog_transparent_bg = NULL;
    }
    if (_dialog_label_ok) {
        SDL_FreeSurface(_dialog_label_ok);
        _dialog_label_ok = NULL;
    }
    if (_dialog_label_cancel) {
        SDL_FreeSurface(_dialog_label_cancel);
        _dialog_label_cancel = NULL;
    }
}

/* One button of a dialog: a rounded box with the theme's button image and
 * its word. The safe one (B) is grey with a white outline, as it is the one
 * already chosen; the action (A) is outlined in the error red. */
static void _dialog_button(SDL_Surface *screen, SDL_Rect r, SDL_Surface *button,
                           SDL_Surface *label, bool safe)
{
    int rad = 10.0 * g_scale, line = (safe ? 3.0 : 2.0) * g_scale;
    Uint32 edge = safe ? SDL_MapRGB(screen->format, 255, 255, 255)
                       : SDL_MapRGB(screen->format, 255, 143, 143);
    Uint32 fill = safe ? SDL_MapRGB(screen->format, 58, 58, 61)
                       : SDL_MapRGB(screen->format, 46, 46, 49);
    ui_fillRoundRect(screen, r, rad, edge);
    SDL_Rect in = {r.x + line, r.y + line, r.w - 2 * line, r.h - 2 * line};
    ui_fillRoundRect(screen, in, rad - line, fill);
    int gap = 8.0 * g_scale;
    int w = (button ? button->w + gap : 0) + (label ? label->w : 0);
    int x = r.x + (r.w - w) / 2, cy = r.y + r.h / 2;
    if (button) {
        SDL_Rect br = {x, cy - button->h / 2};
        SDL_BlitSurface(button, NULL, screen, &br);
        x += button->w + gap;
    }
    if (label) {
        SDL_Rect lr = {x, cy - label->h / 2};
        SDL_BlitSurface(label, NULL, screen, &lr);
    }
}

/* The words on the dialog's buttons, in capitals as in the footer */
static SDL_Surface *_dialog_label(const char *text, SDL_Color color)
{
    char up[64];
    int n = 0;
    for (; text[n] && n < 63; n++)
        up[n] = (text[n] >= 'a' && text[n] <= 'z') ? text[n] - 32 : text[n];
    up[n] = '\0';
    return TTF_RenderUTF8_Blended(resource_getFont(HINT), up, color);
}

// The next dialog's message wrapped to the window's width (an info note
// written in short lines), see theme_renderDialogWrapped.
static bool _dialog_wrap = false;

void theme_renderDialog(SDL_Surface *screen, const char *title_str, const char *message_str, bool show_hint)
{
    // Cache transparent background — avoids CreateRGBSurface+FillRect+FreeSurface per call
    if (_dialog_transparent_bg == NULL) {
        _dialog_transparent_bg = SDL_CreateRGBSurface(0, g_display.width, g_display.height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        SDL_FillRect(_dialog_transparent_bg, NULL, /* 66% black */ 0xAA000000);
    }
    SDL_BlitSurface(_dialog_transparent_bg, NULL, screen, NULL);

    /* New look: a box in the middle, as tall as its text. The title in
     * capitals on the left, the message under it in light grey, and, when
     * the dialog asks something, two buttons: B (safe, already chosen) on
     * the left, A (the action) on the right. */
    int pad = 24.0 * g_scale, box_w = 500.0 * g_scale, btn_h = 46.0 * g_scale;
    int inner_w = box_w - 2 * pad;

    char title_up[STR_MAX], title_page[STR_MAX];
    title_str = title_str ? ui_pageTitle(title_str, title_page, sizeof(title_page)) : NULL;
    int n = 0;
    for (; title_str && title_str[n] && n < STR_MAX - 1; n++)
        title_up[n] = (title_str[n] >= 'a' && title_str[n] <= 'z') ? title_str[n] - 32 : title_str[n];
    title_up[n] = '\0';
    SDL_Surface *title = n > 0 && strcmp(title_up, " ") != 0
                             ? TTF_RenderUTF8_Blended(resource_getFont(TITLE), title_up, (SDL_Color){244, 244, 245})
                             : NULL;
    /* No title and no buttons (the LOADING and SAVING screens over the
     * game's picture): the message alone, centred in the box. */
    bool centred = title == NULL && !show_hint;
    SDL_Surface *textbox =
        _dialog_wrap ? theme_textboxSurfaceWrap(message_str, (SDL_Color){200, 200, 206, 0},
                                                centred ? ALIGN_CENTER : ALIGN_LEFT, inner_w,
                                                (int)(9 * DIALOG_LINE_HEIGHT * g_scale))
                     : theme_textboxSurfaceFit(message_str, (SDL_Color){200, 200, 206, 0},
                                               centred ? ALIGN_CENTER : ALIGN_LEFT, inner_w,
                                               (int)(5 * DIALOG_LINE_HEIGHT * g_scale));
    SDL_Surface *button_a = resource_getSurface(BUTTON_A);
    SDL_Surface *button_b = resource_getSurface(BUTTON_B);
    bool buttons = show_hint && button_a != NULL && button_a->w < g_display.width;

    int box_h = pad + (title ? title->h + 10.0 * g_scale : 0) + (textbox ? textbox->h : 0) +
                (buttons ? 22.0 * g_scale + btn_h : 0) + pad;
    SDL_Rect box = {(g_display.width - box_w) / 2, (g_display.height - box_h) / 2, box_w, box_h};
    ui_fillRoundRect(screen, box, (int)(16.0 * g_scale), SDL_MapRGB(screen->format, 46, 46, 49));

    int y = box.y + pad;
    if (title) {
        SDL_Rect r = {box.x + pad, y};
        SDL_Rect crop = {0, 0, inner_w, title->h};
        SDL_BlitSurface(title, &crop, screen, &r);
        y += title->h + 10.0 * g_scale;
        SDL_FreeSurface(title);
    }
    if (textbox) {
        SDL_Rect r = {box.x + pad + (centred && textbox->w < inner_w ? (inner_w - textbox->w) / 2 : 0), y};
        SDL_BlitSurface(textbox, NULL, screen, &r);
        y += textbox->h;
        SDL_FreeSurface(textbox);
    }
    if (buttons) {
        y += 22.0 * g_scale;
        if (_dialog_label_ok == NULL)
            _dialog_label_ok = _dialog_label(lang_get(LANG_OK, LANG_FALLBACK_OK), (SDL_Color){255, 143, 143});
        if (_dialog_label_cancel == NULL)
            _dialog_label_cancel = _dialog_label(lang_get(LANG_CANCEL, LANG_FALLBACK_CANCEL), (SDL_Color){244, 244, 245});
        int gap = 12.0 * g_scale, bw = (inner_w - gap) / 2;
        _dialog_button(screen, (SDL_Rect){box.x + pad, y, bw, btn_h}, button_b, _dialog_label_cancel, true);
        _dialog_button(screen, (SDL_Rect){box.x + pad + bw + gap, y, bw, btn_h}, button_a, _dialog_label_ok, false);
    }
}

/* A dialog whose message is an info note: wrapped to the window's width
 * instead of kept in the lines it was written in (Tweaks' and the prompt's
 * SELECT notes, the first-run tooltip). */
void theme_renderDialogWrapped(SDL_Surface *screen, const char *title_str, const char *message_str)
{
    _dialog_wrap = true;
    theme_renderDialog(screen, title_str, message_str, false);
    _dialog_wrap = false;
}

void theme_renderDialogProgress(SDL_Surface *screen, const char *title_str,
                                const char *message_str, bool show_hint)
{
    theme_renderDialog(screen, title_str, message_str, show_hint);

    SDL_Surface *dot = resource_getSurface(PROGRESS_DOT);
    /* Test branch: in the middle of the card (the screen's middle). */
    SDL_Rect dot_rect = {(g_display.width - dot->w) / 2 - 32.0 * g_scale, g_display.height / 2 + 10.0 * g_scale - dot->h / 2};

    if (dialog_progress >= 1)
        SDL_BlitSurface(dot, NULL, screen, &dot_rect);
    dot_rect.x += 32.0 * g_scale;
    if (dialog_progress >= 2)
        SDL_BlitSurface(dot, NULL, screen, &dot_rect);
    dot_rect.x += 32.0 * g_scale;
    if (dialog_progress >= 3)
        SDL_BlitSurface(dot, NULL, screen, &dot_rect);

    dialog_progress = (dialog_progress + 1) % 4;
}

void theme_clearDialogProgress(void) { dialog_progress = 0; }

void theme_renderInfoPanel(SDL_Surface *screen, const char *title_str, const char *message_str, bool use_dialog)
{
    bool has_title = title_str != NULL && title_str[0] != '\0';
    bool has_message = message_str != NULL && message_str[0] != '\0';

    if (use_dialog) {
        theme_renderDialog(screen, has_title ? title_str : " ", has_message ? message_str : " ", false);
        return;
    }

    theme_renderHeader(screen, has_title ? title_str : NULL, false);

    if (has_message) {
        SDL_Surface *message = NULL;
        char message_newline[4096];
        strncpy(message_newline, message_str, sizeof(message_newline) - 1);
        message_newline[sizeof(message_newline) - 1] = '\0';
        char *str = str_replace(message_newline, "\\n", "\n");
        // Between the header and the bottom margin, 20 px from each side.
        message = theme_textboxSurfaceFit(str ? str : message_newline, theme()->list.color, ALIGN_CENTER,
                                          g_display.width - (int)(40.0 * g_scale),
                                          g_display.height - (int)(120.0 * g_scale));
        if (message) {
            SDL_Rect message_rect = {(g_display.width) / 2, (g_display.height) / 2};
            message_rect.x -= message->w / 2;
            message_rect.y -= message->h / 2;
            SDL_BlitSurface(message, NULL, screen, &message_rect);
            SDL_FreeSurface(message);
        }
        free(str);
    }
}

#endif // THEME_RENDER_DIALOG_H__
