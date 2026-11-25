#ifndef RENDER_HEADER_H__
#define RENDER_HEADER_H__

#include "theme/background.h"
#include "theme/config.h"
#include "theme/resources.h"
#include "utils/surfaceSetAlpha.h"

#include "./unified.h"

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

/* Cache for the badge's letter */
static SDL_Surface *_cached_header_badge = NULL;
static char _cached_header_badge_char = '\0';

/* The badge's icon, for the title it was looked up for */
static SDL_Surface *_cached_header_icon = NULL;
static char _cached_header_icon_name[32] = "";

/* The title font at 70%, for the earlier levels of a path */
static TTF_Font *_header_path_font = NULL;

void theme_renderHeader_cleanup(void)
{
    if (_cached_header_badge) {
        SDL_FreeSurface(_cached_header_badge);
        _cached_header_badge = NULL;
    }
    _cached_header_badge_char = '\0';
    if (_cached_header_icon) {
        SDL_FreeSurface(_cached_header_icon);
        _cached_header_icon = NULL;
    }
    _cached_header_icon_name[0] = '\0';
    if (_cached_header_title) {
        SDL_FreeSurface(_cached_header_title);
        _cached_header_title = NULL;
    }
    _cached_header_title_str[0] = '\0';
    if (_header_path_font) {
        TTF_CloseFont(_header_path_font);
        _header_path_font = NULL;
    }
}

/* The icon drawn in the badge for a title: res/icons/ui/<name>.png (a theme
 * can ship its own as skin/extra/icons/ui/<name>.png). Titles not listed
 * keep their first letter. */
static const char *_header_iconName(const char *title)
{
    static const struct {
        const char *word, *icon;
    } map[] = {
        {"tweaks", "tweaks"}, {"kids mode", "kid"}, {"controls & tips", "kid"},
        {"backup", "archive"}, {"gallery", "image"}, {"random game", "dice"},
        {"guest mode", "user"}, {"package manager", "cube"}, {"battery", "battery"},
        {"clock", "clock"}, {"theme", "palette"}, {"activity", "chart"},
        {"update", "down"}, {"download", "down"}, {"scrap", "image"},
        {"sd card", "sd"}, {"manual", "book"}, {"pico", "game"},
        {"game core", "pad"}, {"glo", "pad"}, {"hide game", "pad"},
        {"hidden games", "pad"}, {"netplay", "pad"}, {"join", "pad"},
        {"host", "pad"}, {"multi-disc", "pad"}, {"port", "pad"},
        {"advancemenu", "pad"}, {"caching", "refresh"}, {"migration", "refresh"},
        {"error", "alert"}, {"not found", "alert"}, {"read-only", "alert"},
        {"read only", "alert"}, {"search", "search"}, {"music", "music"},
        {"video", "film"}, {"pdf", "doc"}, {"audiotune", "wave"}, {"displaycal", "monitor"},
    };
    char low[STR_MAX];
    int n = 0;
    for (; title[n] && n < STR_MAX - 1; n++)
        low[n] = (title[n] >= 'A' && title[n] <= 'Z') ? title[n] + 32 : title[n];
    low[n] = '\0';
    for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++)
        if (strstr(low, map[i].word))
            return map[i].icon;
    return NULL;
}

static SDL_Surface *_header_icon(const char *title)
{
    const char *name = _header_iconName(title);
    if (name == NULL)
        return NULL;
    if (strcmp(name, _cached_header_icon_name) != 0) {
        if (_cached_header_icon)
            SDL_FreeSurface(_cached_header_icon);
        char rel[64];
        snprintf(rel, sizeof(rel), "extra/icons/ui/%s", name);
        _cached_header_icon = theme_loadImage(theme()->path, rel);
        snprintf(_cached_header_icon_name, sizeof(_cached_header_icon_name), "%s", name);
    }
    return _cached_header_icon;
}

// Unified look: a badge in the theme's selection colour with the app's
// icon (or the title's first letter, or the theme's logo), then the title, left-aligned.
// Returns where the title starts (after the badge or the logo).
static int _theme_renderHeaderTitleText(SDL_Surface *screen, SDL_Surface *title, int x)
{
    if (title) {
        // Room up to the battery, on the right.
        SDL_Rect crop = {0, 0, 540.0 * g_scale - x, title->h};
        SDL_Rect title_rect = {x, 29.0 * g_scale - title->h / 2};
        SDL_BlitSurface(title, &crop, screen, &title_rect);
    }
    return x;
}

static int _theme_renderHeaderTitle(SDL_Surface *screen, SDL_Surface *title,
                                    const char *title_str, bool show_logo)
{
    int x = 16.0 * g_scale, cy = 30.0 * g_scale;
    if (show_logo) {
        SDL_Surface *logo = resource_getSurface(LOGO);
        SDL_Rect logo_rect = {x, cy - logo->h / 2};
        SDL_BlitSurface(logo, NULL, screen, &logo_rect);
        x += logo->w + 12.0 * g_scale;
    }
    else if (title_str && title_str[0]) {
        int size = 32.0 * g_scale;
        SDL_Rect badge = {x, cy - size / 2, size, size};
        ui_fillRoundRect(screen, badge, 8.0 * g_scale, ui_map(screen, ui_accent()));
        SDL_Surface *icon = _header_icon(title_str);
        if (icon) {
            SDL_Rect ir = {badge.x + (size - icon->w) / 2, cy - icon->h / 2};
            SDL_BlitSurface(icon, NULL, screen, &ir);
            return _theme_renderHeaderTitleText(screen, title, x + size + 12.0 * g_scale);
        }
        char letter = title_str[0];
        if (letter >= 'a' && letter <= 'z')
            letter -= 'a' - 'A';
        if (_cached_header_badge == NULL || _cached_header_badge_char != letter) {
            if (_cached_header_badge)
                SDL_FreeSurface(_cached_header_badge);
            char text[2] = {letter, '\0'};
            _cached_header_badge = TTF_RenderUTF8_Blended(resource_getFont(TITLE), text,
                                                          (SDL_Color){255, 255, 255});
            _cached_header_badge_char = letter;
        }
        if (_cached_header_badge) {
            SDL_Rect lr = {badge.x + (size - _cached_header_badge->w) / 2,
                           cy - _cached_header_badge->h / 2};
            SDL_BlitSurface(_cached_header_badge, NULL, screen, &lr);
        }
        x += size + 12.0 * g_scale;
    }
    return _theme_renderHeaderTitleText(screen, title, x);
}

void theme_renderHeader(SDL_Surface *screen, const char *title_str, bool show_logo)
{
    theme_renderHeaderBackground(screen);

    if (title_str) {
        if (strcmp(title_str, _cached_header_title_str) != 0) {
            if (_cached_header_title)
                SDL_FreeSurface(_cached_header_title);
            _cached_header_title = TTF_RenderUTF8_Blended(resource_getFont(TITLE), title_str, theme()->title.color);
            strncpy(_cached_header_title_str, title_str, STR_MAX - 1);
            _cached_header_title_str[STR_MAX - 1] = '\0';
        }
    }
    _theme_renderHeaderTitle(screen, title_str ? _cached_header_title : NULL, title_str, show_logo);
}

/* Between two levels of a path: a drawn chevron, not a "›" character,
 * which some fonts do not have. */
#define HEADER_PATH_GAP (22.0 * g_scale)

/* Width of the levels levels[from..count-2], each followed by a chevron gap,
 * after "..." and a gap when from > 0, in the path font. */
static int _header_pathWidth(const char *const *levels, int count, int from)
{
    int w = 0, piece;
    if (_header_path_font == NULL)
        return 0;
    if (from > 0 && TTF_SizeUTF8(_header_path_font, "...", &piece, NULL) == 0)
        w += piece + HEADER_PATH_GAP;
    for (int i = from; i < count - 1; i++)
        if (TTF_SizeUTF8(_header_path_font, levels[i], &piece, NULL) == 0)
            w += piece + HEADER_PATH_GAP;
    return w;
}

static int _header_pathPiece(SDL_Surface *screen, const char *text, int x, int baseline)
{
    SDL_Color grey = {163, 163, 168};
    SDL_Surface *piece = TTF_RenderUTF8_Blended(_header_path_font, text, grey);
    if (piece == NULL)
        return x;
    SDL_Rect r = {x, baseline - TTF_FontAscent(_header_path_font)};
    SDL_BlitSurface(piece, NULL, screen, &r);
    x += piece->w;
    SDL_FreeSurface(piece);
    /* chevron in the middle of the gap, on the capitals' middle */
    int cy = baseline - (int)(TTF_FontAscent(_header_path_font) * 0.35);
    ui_drawChevron(screen, x + HEADER_PATH_GAP / 2 + 2.0 * g_scale, cy, 4.0 * g_scale, 1,
                   SDL_MapRGB(screen->format, grey.r, grey.g, grey.b));
    return x + HEADER_PATH_GAP;
}

/**
 * @brief Header with the path to the current page (test branch, unified
 *        look): "Tweaks > System > Startup". The badge has the first level's
 *        letter, the earlier levels are smaller and grey with chevrons
 *        between them, the current one is the title. When it does not fit,
 *        the oldest levels give way to "...", so the current one stays whole.
 */
void theme_renderHeaderPath(SDL_Surface *screen, const char *const *levels, int count)
{
    if (count <= 1) {
        theme_renderHeader(screen, count == 1 ? levels[0] : NULL, false);
        return;
    }
    theme_renderHeaderBackground(screen);
    const char *current = levels[count - 1];
    if (strcmp(current, _cached_header_title_str) != 0) {
        if (_cached_header_title)
            SDL_FreeSurface(_cached_header_title);
        _cached_header_title = TTF_RenderUTF8_Blended(resource_getFont(TITLE), current, theme()->title.color);
        strncpy(_cached_header_title_str, current, STR_MAX - 1);
        _cached_header_title_str[STR_MAX - 1] = '\0';
    }
    /* badge only: the title is placed after the path below */
    int x = _theme_renderHeaderTitle(screen, NULL, levels[0], false);
    if (_header_path_font == NULL)
        _header_path_font = theme_loadFont(theme()->path, theme()->title.font,
                                           (int)(theme()->title.size * 0.7 + 0.5));
    SDL_Surface *title = _cached_header_title;
    int title_w = title ? title->w : 0, avail = 540.0 * g_scale - x;
    int from = 0, prefix_w = _header_pathWidth(levels, count, from);
    while (prefix_w + title_w > avail && from < count - 1) {
        from++;
        prefix_w = _header_pathWidth(levels, count, from);
    }
    TTF_Font *title_font = resource_getFont(TITLE);
    int title_y = 29.0 * g_scale - (title ? title->h / 2 : 0);
    int baseline = title_y + (title_font ? TTF_FontAscent(title_font) : 0);
    if (_header_path_font && prefix_w > 0 && prefix_w + title_w <= avail) {
        if (from > 0)
            x = _header_pathPiece(screen, "...", x, baseline);
        for (int i = from; i < count - 1; i++)
            x = _header_pathPiece(screen, levels[i], x, baseline);
    }
    if (title) {
        SDL_Rect crop = {0, 0, 540.0 * g_scale - x, title->h};
        SDL_Rect title_rect = {x, title_y};
        SDL_BlitSurface(title, &crop, screen, &title_rect);
    }
}

void theme_renderHeaderExtra(SDL_Surface *screen, const char *title_str,
                             const char *prev_title_str)
{
    theme_renderHeaderBackground(screen);

    SDL_Surface *title = TTF_RenderUTF8_Blended(resource_getFont(TITLE), title_str, theme()->title.color);
    if (title == NULL)
        return;
    _theme_renderHeaderTitle(screen, title, title_str, false);
    SDL_FreeSurface(title);
}

#endif // RENDER_HEADER_H__
