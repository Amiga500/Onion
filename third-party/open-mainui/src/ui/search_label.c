/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui/search_label.h"
#include <string.h>

static unsigned char fold(unsigned char value)
{
    return value >= 'A' && value <= 'Z' ? (unsigned char)(value + 'a' - 'A') : value;
}

SDL_Surface *mainui_search_label(MainUITheme *theme, const char *label, const char *query)
{
    size_t count = strlen(query), length = strlen(label);
    if (!count || count > length || length >= 4096) {
        return NULL;
    }
    SDL_Surface *result = TTF_RenderUTF8_Blended(theme->font, label, theme->color);
    if (!result) {
        return NULL;
    }
    SDL_Color color = theme->color.r + 2 * theme->color.g + theme->color.b < 512
                          ? (SDL_Color){123, 44, 191, 0}
                          : (SDL_Color){255, 255, 0, 0};
    char prefix[4096], match[128];
    if (count >= sizeof match) {
        SDL_FreeSurface(result);
        return NULL;
    }
    for (size_t i = 0; i + count <= length; i++) {
        size_t j = 0;
        while (j < count && fold((unsigned char)label[i + j]) == fold((unsigned char)query[j])) {
            j++;
        }
        if (j != count) {
            continue;
        }
        memcpy(prefix, label, i);
        prefix[i] = 0;
        memcpy(match, label + i, count);
        match[count] = 0;
        int x = 0, height = 0;
        if (i && TTF_SizeUTF8(theme->font, prefix, &x, &height)) {
            continue;
        }
        SDL_Surface *part = TTF_RenderUTF8_Blended(theme->font, match, color);
        if (part) {
            SDL_Rect destination = {(Sint16)x, 0, 0, 0};
            SDL_BlitSurface(part, NULL, result, &destination);
            SDL_FreeSurface(part);
        }
        i += count - 1;
    }
    return result;
}
