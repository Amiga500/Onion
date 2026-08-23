#ifndef THEME_RENDER_DIALOG_H__
#define THEME_RENDER_DIALOG_H__

#include "theme/config.h"
#include "theme/resources.h"
#include "utils/surfaceSetAlpha.h"

#include "./textbox.h"

#define DIALOG_WIDTH 450
#define DIALOG_LINE_HEIGHT 30
#define DIALOG_LINE_BENCHMARK "access, and modify files as if they were stored"

static int dialog_progress = 0;
static int dialog_font_size = 0;

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
    dialog_font_size = 0;
}

int __get_font_size()
{
    if (dialog_font_size == 0) {
        int w = 0, h = 0;
        if (TTF_SizeUTF8(resource_getFont(TITLE), DIALOG_LINE_BENCHMARK, &w, &h) == 0) {
            double scale_x = (double)DIALOG_WIDTH * g_scale / w;
            double scale_y = (double)DIALOG_LINE_HEIGHT * g_scale / h;
            dialog_font_size = (int)((scale_x > scale_y ? scale_y : scale_x) * theme()->title.size);
        }
        else {
            dialog_font_size = theme()->title.size;
        }
    }
    return dialog_font_size;
}

void theme_renderDialog(SDL_Surface *screen, const char *title_str, const char *message_str, bool show_hint)
{
    // Cache transparent background — avoids CreateRGBSurface+FillRect+FreeSurface per call
    if (_dialog_transparent_bg == NULL) {
        _dialog_transparent_bg = SDL_CreateRGBSurface(0, g_display.width, g_display.height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        SDL_FillRect(_dialog_transparent_bg, NULL, /* 33.3% transparent black */ 0x55000000);
    }
    SDL_BlitSurface(_dialog_transparent_bg, NULL, screen, NULL);

    SDL_Surface *pop_bg = resource_getSurface(POP_BG);
    SDL_Rect center_rect = {(g_display.width - pop_bg->w) / 2, (g_display.height - pop_bg->h) / 2};

    SDL_BlitSurface(pop_bg, NULL, screen, &center_rect);

    SDL_Surface *title = TTF_RenderUTF8_Blended(resource_getFont(TITLE), title_str, theme()->total.color);
    if (title) {
        SDL_Rect title_rect = {(g_display.width - title->w) / 2, center_rect.y + 25.0 * g_scale - title->h / 2};
        SDL_BlitSurface(title, NULL, screen, &title_rect);
        SDL_FreeSurface(title);
    }

    SDL_Surface *textbox = theme_textboxSurface(message_str, resource_getFont(TITLE), theme()->grid.color, ALIGN_CENTER);
    if (textbox != NULL && (textbox->w > DIALOG_WIDTH || textbox->h > 6 * (double)DIALOG_LINE_HEIGHT * g_scale)) {
        SDL_FreeSurface(textbox);
        TTF_Font *temp_font = theme_loadFont(theme()->path, theme()->title.font, __get_font_size());
        textbox = theme_textboxSurface(message_str, temp_font, theme()->grid.color, ALIGN_CENTER);
        TTF_CloseFont(temp_font);
    }
    if (textbox) {
        SDL_Rect textbox_rect = {(g_display.width - textbox->w) / 2, center_rect.y + 160.0 * g_scale - textbox->h / 2};
        SDL_BlitSurface(textbox, NULL, screen, &textbox_rect);
        SDL_FreeSurface(textbox);
    }

    SDL_Surface *button_a = resource_getSurface(BUTTON_A);
    if (show_hint && button_a != NULL && button_a->w < g_display.width) {
        SDL_Rect hint_rect = {center_rect.x + pop_bg->w - 30.0 * g_scale, center_rect.y + pop_bg->h - 60.0 * g_scale};

        SDL_Surface *button_b = resource_getSurface(BUTTON_B);
        // Cache OK/Cancel labels — they don't change between calls
        if (_dialog_label_ok == NULL)
            _dialog_label_ok = TTF_RenderUTF8_Blended(resource_getFont(HINT), lang_get(LANG_OK, LANG_FALLBACK_OK), theme()->hint.color);
        if (_dialog_label_cancel == NULL)
            _dialog_label_cancel = TTF_RenderUTF8_Blended(resource_getFont(HINT), lang_get(LANG_CANCEL, LANG_FALLBACK_CANCEL), theme()->hint.color);
        SDL_Surface *label_ok = _dialog_label_ok;
        SDL_Surface *label_cancel = _dialog_label_cancel;

        hint_rect.x -= button_a->w + 5.0 * g_scale;
        if (label_ok) {
            hint_rect.x -= label_ok->w + 30.0 * g_scale;
        }

        if (button_b != NULL)
            hint_rect.x -= button_b->w + 5.0 * g_scale;
        if (label_cancel) {
            hint_rect.x -= label_cancel->w + 30.0 * g_scale;
        }

        if (label_ok) {
            SDL_Rect button_a_rect = {hint_rect.x, hint_rect.y - button_a->h / 2};
            hint_rect.x += button_a->w + 5.0 * g_scale;
            SDL_BlitSurface(button_a, NULL, screen, &button_a_rect);

            SDL_Rect label_ok_rect = {hint_rect.x, hint_rect.y - label_ok->h / 2};
            hint_rect.x += label_ok->w + 30.0 * g_scale;
            SDL_BlitSurface(label_ok, NULL, screen, &label_ok_rect);
        }

        if (label_cancel && button_b != NULL) {
            SDL_Rect button_b_rect = {hint_rect.x, hint_rect.y - button_b->h / 2};
            hint_rect.x += button_b->w + 5.0 * g_scale;
            SDL_BlitSurface(button_b, NULL, screen, &button_b_rect);

            SDL_Rect label_cancel_rect = {hint_rect.x, hint_rect.y - label_cancel->h / 2};
            hint_rect.x += label_cancel->w + 30.0 * g_scale;
            SDL_BlitSurface(label_cancel, NULL, screen, &label_cancel_rect);
        }
    }
}

void theme_renderDialogProgress(SDL_Surface *screen, const char *title_str,
                                const char *message_str, bool show_hint)
{
    theme_renderDialog(screen, title_str, message_str, show_hint);

    SDL_Surface *dot = resource_getSurface(PROGRESS_DOT);
    SDL_Rect dot_rect = {(g_display.width - dot->w) / 2 - 32.0 * g_scale, 225.0 * g_scale - dot->h / 2};

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
        message = theme_textboxSurface(str ? str : message_newline, resource_getFont(TITLE), theme()->list.color, ALIGN_CENTER);
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
