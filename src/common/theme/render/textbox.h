#ifndef RENDER_TEXTBOX_H__
#define RENDER_TEXTBOX_H__

#include "theme/config.h"
#include "theme/resources.h"

#include "header.h"
#include "textbox_fit.h"

typedef enum { ALIGN_LEFT,
               ALIGN_CENTER,
               ALIGN_RIGHT } text_alignment_e;

/**
 * @brief Creates an SDL_Surface containing a textbox with the given message.
 *        Newlines in the message are handled appropriately, creating multiple lines.
 *        Double newlines create paragraph spacing.
 *        The textbox will have a transparent background and the text will be
 *        rendered with the given font and color.
 * @param message The message to be displayed in the textbox.
 * @param font The TTF_Font to be used for rendering the text.
 * @param fg The foreground color (text color).
 * @param align The text alignment within the textbox.
 * @return An SDL_Surface containing the rendered textbox, or NULL if nothing could be rendered.
 */
/* Set by theme_textboxSurfaceWrap for one call: the textbox made at least
 * this wide, and a heading line (":: TOOLTIPS ::") centred in it. */
static int _textbox_width = 0;

SDL_Surface *theme_textboxSurface(const char *message, TTF_Font *font,
                                  SDL_Color fg, text_alignment_e align)
{
    char s[STR_MAX];
    strncpy(s, message, sizeof(s) - 1);
    s[sizeof(s) - 1] = '\0';

    int max_lines = 8; // start small, grow if needed
    int line_count = 0, max_width = 0, empty_lines = 0;
    int *line_widths = malloc(max_lines * sizeof(int));
    char **lines = malloc(max_lines * sizeof(char *));

    TTF_Font *cur_font = font;
    int font_lineskip = TTF_FontLineSkip(cur_font);
    int line_height = 1.2 * font_lineskip; // default height
    int paragraph_spacing = (int)(0.5 * font_lineskip);

    // --- First pass: measure each line
    size_t msglen = strlen(s);
    size_t start = 0;
    for (size_t i = 0; i <= msglen; i++) {
        if (s[i] != '\n' && s[i] != '\0') {
            continue;
        }
        size_t len = i - start;
        if (line_count >= max_lines) {
            max_lines *= 2;
            lines = realloc(lines, max_lines * sizeof(char *));
            line_widths = realloc(line_widths, max_lines * sizeof(int));
        }
        char *linebuf = malloc(len + 1);
        memcpy(linebuf, &s[start], len);
        linebuf[len] = 0;
        lines[line_count] = linebuf;

        int w = 0, h = 0;
        if (linebuf[0] == '\0') {
            empty_lines++;
            line_widths[line_count] = 0;
        }
        else {
            if (TTF_SizeUTF8(cur_font, linebuf, &w, &h) == 0) {
                if (h > line_height)
                    line_height = h;
            }
            else {
                fprintf(stderr, "TTF_SizeUTF8 failed: %s\n", TTF_GetError());
            }
            line_widths[line_count] = w;
            if (w > max_width)
                max_width = w;
        }
        line_count++;
        start = i + 1;
    }

    if (line_count == 0) {
        free(lines);
        free(line_widths);
        return NULL;
    }

    if (_textbox_width > max_width)
        max_width = _textbox_width;

    // --- Calculate textbox height
    int textbox_height = (line_count - empty_lines) * line_height +
                         empty_lines * paragraph_spacing;

    // --- Create textbox surface
    SDL_Surface *textbox = SDL_CreateRGBSurface(
        0, max_width, textbox_height, 32, 0x00FF0000, 0x0000FF00,
        0x000000FF, 0xFF000000);
    SDL_FillRect(textbox, NULL, 0x000000FF);

    // --- Second pass: render each line if not empty
    int y = 0;
    for (int i = 0; i < line_count; ++i) {
        if (lines[i][0] == '\0') {
            y += paragraph_spacing; // empty line, just add spacing
        }
        else {
            SDL_Surface *surf = TTF_RenderUTF8_Blended(cur_font, lines[i], fg);
            if (surf) {
                SDL_SetAlpha(surf, 0, 0);
                SDL_Rect r = {0, y};
                // horizontal alignment
                if (align == ALIGN_CENTER || (_textbox_width > 0 && strncmp(lines[i], "::", 2) == 0))
                    r.x = (max_width - surf->w) / 2;
                else if (align == ALIGN_RIGHT)
                    r.x = max_width - surf->w;
                r.y = y + (line_height - surf->h) / 2;

                SDL_BlitSurface(surf, NULL, textbox, &r);
                SDL_FreeSurface(surf);
            }
            else {
                fprintf(stderr, "TTF_RenderUTF8_Blended failed: %s\n", TTF_GetError());
            }
            y += line_height;
        }
        free(lines[i]);
    }

    free(lines);
    free(line_widths);

    return textbox;
}

/**
 * @brief Like theme_textboxSurface with the theme's title font, but shrinks
 *        the font until the text fits in max_w x max_h pixels.
 *
 * Message screens (prompt, infoPanel, dialogs) used the title font at its
 * full size, so with a large theme font a long line was cut off by the
 * screen edge. Lines are not wrapped: the font gets smaller instead, down to
 * TEXTBOX_FIT_MIN_SIZE.
 */
SDL_Surface *theme_textboxSurfaceFit(const char *message, SDL_Color fg,
                                     text_alignment_e align, int max_w, int max_h)
{
    SDL_Surface *textbox = theme_textboxSurface(message, resource_getFont(TITLE), fg, align);
    int size = theme()->title.size;

    for (int tries = 0; textbox != NULL && tries < 8; tries++) {
        int next = textbox_fit_size(size, textbox->w, textbox->h, max_w, max_h,
                                    TEXTBOX_FIT_MIN_SIZE);
        if (next == size)
            break;
        TTF_Font *font = theme_loadFont(theme()->path, theme()->title.font, next);
        if (font == NULL)
            break;
        SDL_Surface *smaller = theme_textboxSurface(message, font, fg, align);
        TTF_CloseFont(font);
        if (smaller == NULL)
            break;
        SDL_FreeSurface(textbox);
        textbox = smaller;
        size = next;
    }
    return textbox;
}

/* The text wrapped at max_w pixels in font: each paragraph (textbox_reflow
 * keeps its breaks) cut at spaces into lines that fit. Returns the number of
 * lines; out gets the text with '\n' between them. */
static int _textbox_wrap(TTF_Font *font, const char *text, int max_w, char *out, size_t n)
{
    size_t o = 0;
    int lines = 0;
    const char *para = text;
    while (o + 1 < n) {
        const char *end = strchr(para, '\n');
        size_t len = end ? (size_t)(end - para) : strlen(para);
        char line[STR_MAX] = "";
        size_t i = 0;
        bool empty = true;
        while (i <= len) {
            size_t j = i;
            while (j < len && para[j] != ' ')
                j++;
            char word[STR_MAX];
            snprintf(word, sizeof(word), "%.*s", (int)(j - i), para + i);
            char tryl[STR_MAX];
            snprintf(tryl, sizeof(tryl), "%s%s%s", line, empty ? "" : " ", word);
            int w = 0;
            TTF_SizeUTF8(font, tryl, &w, NULL);
            if (!empty && w > max_w) {
                o += snprintf(out + o, n - o, "%s\n", line);
                lines++;
                snprintf(line, sizeof(line), "%s", word);
            }
            else {
                snprintf(line, sizeof(line), "%s", tryl);
            }
            empty = false;
            i = j + 1;
            if (o + 1 >= n)
                break;
        }
        if (o + 1 < n)
            o += snprintf(out + o, n - o, "%s%s", line, end ? "\n" : "");
        lines++;
        if (!end || o + 1 >= n)
            break;
        para = end + 1;
    }
    if (o >= n)
        out[n - 1] = '\0';
    return lines;
}

/**
 * @brief Like theme_textboxSurfaceFit, but the text is wrapped to the width
 *        first: its soft line breaks joined (textbox_reflow), then cut at
 *        spaces to fill max_w, and the font made smaller only while the lines
 *        do not fit in max_h.
 *
 * Tweaks' notes are written in short lines for the old dialog: shown as they
 * were, they kept to the left of the wider new window, a short line here and
 * there (4.5.1, Amiga500). Wrapped at the window's width they fill it.
 */
SDL_Surface *theme_textboxSurfaceWrap(const char *message, SDL_Color fg,
                                      text_alignment_e align, int max_w, int max_h)
{
    char joined[STR_MAX], wrapped[STR_MAX * 2];
    textbox_reflow(message ? message : "", joined, sizeof(joined));
    int size = theme()->title.size;
    TTF_Font *font = resource_getFont(TITLE), *owned = NULL;
    for (int tries = 0; font != NULL && tries < 24; tries++) {
        int lines = _textbox_wrap(font, joined, max_w, wrapped, sizeof(wrapped));
        int h = (int)(1.2 * TTF_FontLineSkip(font)) * lines;
        if (h <= max_h || size <= TEXTBOX_FIT_MIN_SIZE)
            break;
        size = size - 2 < TEXTBOX_FIT_MIN_SIZE ? TEXTBOX_FIT_MIN_SIZE : size - 2;
        TTF_Font *smaller = theme_loadFont(theme()->path, theme()->title.font, size);
        if (smaller == NULL)
            break;
        if (owned)
            TTF_CloseFont(owned);
        font = owned = smaller;
    }
    SDL_Surface *textbox = NULL;
    if (font) {
        _textbox_wrap(font, joined, max_w, wrapped, sizeof(wrapped));
        _textbox_width = max_w;
        textbox = theme_textboxSurface(wrapped, font, fg, align);
        _textbox_width = 0;
    }
    if (owned)
        TTF_CloseFont(owned);
    return textbox;
}

/**
 * @brief Creates an SDL_Surface containing text with a solid background color
 *
 * @param text The text to be rendered.
 * @param fg The foreground color (text color).
 * @param bg The background color.
 * @param padding The padding around the text relative to background.
 * @return The created surface containing the rendered text with margin.
 *         Returns NULL if the text rendering fails.
 */
SDL_Surface *theme_createTextOverlay(const char *text, SDL_Color fg, SDL_Color bg, double bgAlpha, int padding)
{
    TTF_Init();
    TTF_Font *font = theme_loadFont(theme()->path, theme()->list.font, (int)(18.0 * g_scale));
    TTF_SetFontStyle(font, TTF_STYLE_BOLD);

    SDL_Surface *text_surface = TTF_RenderUTF8_Blended(font, text, fg);
    if (!text_surface) {
        fprintf(stderr, "TTF_RenderUTF8 failed: %s\n", TTF_GetError());
        return 0;
    }

    TTF_CloseFont(font);

    padding = (int)(padding * g_scale);

    int overlayW = text_surface->w + 2 * padding;
    int overlayH = text_surface->h + 2 * padding;

    SDL_Surface *overlay_surface = SDL_CreateRGBSurface(SDL_SWSURFACE, overlayW, overlayH, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_FillRect(overlay_surface, NULL, SDL_MapRGBA(overlay_surface->format, bg.r, bg.g, bg.b, (Uint8)(bgAlpha * 255.0)));

    SDL_Rect dest = {padding, padding};
    SDL_BlitSurface(text_surface, NULL, overlay_surface, &dest);

    SDL_FreeSurface(text_surface);
    return overlay_surface;
}

#endif // RENDER_TEXTBOX_H__
