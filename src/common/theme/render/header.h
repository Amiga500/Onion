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

/* A title too long for its room on a bar with art, at 70% (and cut with
 * "..." if need be), for the string it was made for */
static SDL_Surface *_cached_header_small = NULL;
static char _cached_header_small_str[STR_MAX] = "";

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
    if (_cached_header_small) {
        SDL_FreeSurface(_cached_header_small);
        _cached_header_small = NULL;
    }
    _cached_header_small_str[0] = '\0';
    if (_header_path_font) {
        TTF_CloseFont(_header_path_font);
        _header_path_font = NULL;
    }
}

/* A theme whose title bar carries art on its left (TechDweeb Orange's logo)
 * gets its title centered, without the badge, as Onion and MainUI draw
 * it: the badge and the left-aligned title went over the art (4.5.1,
 * Amiga500's video). A plain or gradient bar (Silky RedOnion) keeps the badge.
 * Found once per bar: the background and the bar as drawn, from x 16 to
 * 360 (the badge and a title), where neighbouring pixels differ sharply. */
static SDL_Surface *_header_checked_bar = NULL, *_header_checked_bg = NULL;
static bool _header_classic = false;
// where the art on the left of the bar ends (0: none found): a centered
// title longer than the room left of it starts after it instead (TechDweeb
// Orange's logo reaches x 250: "Game lists: scrolling" still began over it)
static int _header_art_end = 0;

// The bar's own picture (bg-title, without the background under it): the
// last column of the block its art starts with, before gap px with none,
// up to x xe; -1 when it has none. A step is sharp between two opaque
// pixels of very different colours, or between an opaque one and a
// see-through one (a plate's edge). The background seen through the bar
// is not looked at: TechDweeb Orange's bricks there made the whole left
// half "art", and the title went to x 410.
static int _header_barArtLast(SDL_Surface *bar, int xe, int gap, int step)
{
    if (bar == NULL || bar->format->BytesPerPixel != 4 || bar->h < 50.0 * g_scale)
        return -1;
    if (xe > bar->w - step)
        xe = bar->w - step;
    int last = -1;
    if (SDL_LockSurface(bar) != 0)
        return -1;
    for (int x = 0; x < xe; x += step) {
        int column = 0;
        for (int y = 10.0 * g_scale; y < 50.0 * g_scale; y += 4) {
            const Uint32 *row = (const Uint32 *)((Uint8 *)bar->pixels + y * bar->pitch);
            Uint8 r1, g1, b1, a1, r2, g2, b2, a2;
            SDL_GetRGBA(row[x], bar->format, &r1, &g1, &b1, &a1);
            SDL_GetRGBA(row[x + step], bar->format, &r2, &g2, &b2, &a2);
            if (a1 < 128 && a2 < 128)
                continue;
            if ((a1 < 128) != (a2 < 128) ||
                abs(r1 - r2) + abs(g1 - g2) + abs(b1 - b2) > 80)
                column++;
        }
        if (column > 0 && (last < 0 || x - last <= gap))
            last = x;
    }
    SDL_UnlockSurface(bar);
    return last;
}

static bool _header_titlebarHasArt(void)
{
    SDL_Surface *bar = resource_getSurface(BG_TITLE), *bg = theme_background();
    if (bar == _header_checked_bar && bg == _header_checked_bg)
        return _header_classic;
    _header_checked_bar = bar;
    _header_checked_bg = bg;
    _header_classic = false;
    _header_art_end = 0;
    SDL_Rect area = theme_scaleRect((SDL_Rect){0, 0, 640, 60});
    SDL_Surface *seen = SDL_CreateRGBSurface(SDL_SWSURFACE, area.w, area.h, 32,
                                             0x00FF0000, 0x0000FF00, 0x000000FF, 0);
    if (seen == NULL)
        return false;
    SDL_FillRect(seen, NULL, 0);
    if (bg)
        SDL_BlitSurface(bg, &area, seen, NULL);
    if (bar)
        SDL_BlitSurface(bar, &area, seen, NULL);
    int sharp = 0, pairs = 0;
    int x0 = 16.0 * g_scale, x1 = 360.0 * g_scale, step = 2;
    if (x1 > seen->w - step)
        x1 = seen->w - step;
    // the art's end: from the first column with sharp steps, the last one
    // before 60 px without any, looked for up to x 400
    int xe = 400.0 * g_scale, gap = 60.0 * g_scale, first = -1, last = -1;
    if (xe > seen->w - step)
        xe = seen->w - step;
    if (SDL_LockSurface(seen) == 0) {
        for (int x = 0; x < xe; x += step) {
            int column = 0;
            for (int y = 10.0 * g_scale; y < 50.0 * g_scale && y < seen->h; y += 4) {
                const Uint32 *row = (const Uint32 *)((Uint8 *)seen->pixels + y * seen->pitch);
                Uint32 a = row[x], b = row[x + step];
                int d = abs((int)((a >> 16) & 0xFF) - (int)((b >> 16) & 0xFF)) +
                        abs((int)((a >> 8) & 0xFF) - (int)((b >> 8) & 0xFF)) +
                        abs((int)(a & 0xFF) - (int)(b & 0xFF));
                column += d > 80;
                if (x >= x0 && x < x1) {
                    sharp += d > 80;
                    pairs++;
                }
            }
            if (column > 0 && (last < 0 || x - last <= gap)) {
                if (first < 0)
                    first = x;
                last = x;
            }
        }
        SDL_UnlockSurface(seen);
    }
    SDL_FreeSurface(seen);
    // more than 4 in 100 sharp steps: lines, letters, a picture
    _header_classic = pairs > 0 && sharp * 25 > pairs;
    if (_header_classic) {
        // where the art ends: from the bar's own picture when it has a
        // block on the left, else from the bar as drawn
        int own = _header_barArtLast(bar, xe, gap, step);
        if (own >= 0)
            last = own;
        // a block reaching the end of the look (a picture across the whole
        // bar, not a logo): nothing to keep clear of, the title centered
        if (last >= 0 && last < xe - gap)
            _header_art_end = last + step;
    }
    return _header_classic;
}

// x for a title w wide on a bar with art: centered, else just after the
// art on the left, never past the battery (then cut there)
static int _header_classicX(int w)
{
    int x = (int)(320.0 * g_scale) - w / 2;
    if (x + w > 540.0 * g_scale)
        x = 540.0 * g_scale - w;
    int after = _header_art_end > 0 ? _header_art_end + (int)(12.0 * g_scale) : 0;
    if (x < after)
        x = after;
    if (x < 16.0 * g_scale)
        x = 16.0 * g_scale;
    return x;
}

static TTF_Font *_header_pathFont(void)
{
    if (_header_path_font == NULL)
        _header_path_font = theme_loadFont(theme()->path, theme()->title.font,
                                           (int)(theme()->title.size * 0.7 + 0.5));
    return _header_path_font;
}

// The title on a bar with art (title: in the title font; str: its text).
// When it does not fit between the art and the battery, it is drawn at
// 70%, and cut with "..." if still too long, not cut off mid-word
// (TechDweeb Orange: "Game lists: scrolling" showed as "Game list:").
static void _header_classicTitle(SDL_Surface *screen, SDL_Surface *title, const char *str)
{
    if (title == NULL)
        return;
    int after = _header_art_end > 0 ? _header_art_end + (int)(12.0 * g_scale) : (int)(16.0 * g_scale);
    int room = 540.0 * g_scale - after;
    SDL_Surface *shown = title;
    TTF_Font *small = str ? _header_pathFont() : NULL;
    if (title->w > room && small) {
        if (strcmp(str, _cached_header_small_str) != 0) {
            if (_cached_header_small)
                SDL_FreeSurface(_cached_header_small);
            char buf[STR_MAX], text[STR_MAX], cut[STR_MAX + 4];
            snprintf(text, sizeof(text), "%s", ui_pageTitle(str, buf, sizeof(buf)));
            snprintf(cut, sizeof(cut), "%s", text);
            int w = 0;
            size_t n = strlen(text);
            while (TTF_SizeUTF8(small, cut, &w, NULL) == 0 && w > room && n > 1) {
                n--; // whole UTF-8 characters only
                while (n > 1 && ((unsigned char)text[n] & 0xC0) == 0x80)
                    n--;
                while (n > 1 && text[n - 1] == ' ')
                    n--;
                snprintf(cut, sizeof(cut), "%.*s...", (int)n, text);
            }
            _cached_header_small = TTF_RenderUTF8_Blended(small, cut, theme()->title.color);
            snprintf(_cached_header_small_str, sizeof(_cached_header_small_str), "%s", str);
        }
        if (_cached_header_small)
            shown = _cached_header_small;
    }
    int x = _header_classicX(shown->w);
    SDL_Rect crop = {0, 0, 540.0 * g_scale - x, shown->h};
    SDL_Rect r = {x, 29.0 * g_scale - shown->h / 2};
    SDL_BlitSurface(shown, &crop, screen, &r);
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
    if (!show_logo && _header_titlebarHasArt()) {
        // centered, no badge (see _header_titlebarHasArt)
        _header_classicTitle(screen, title, title_str);
        return title ? _header_classicX(title->w) : x;
    }
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
            char shown[STR_MAX];
            _cached_header_title = TTF_RenderUTF8_Blended(resource_getFont(TITLE), ui_pageTitle(title_str, shown, sizeof(shown)), theme()->title.color);
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
    for (int i = from; i < count - 1; i++) {
        char shown[STR_MAX];
        if (TTF_SizeUTF8(_header_path_font, ui_pageTitle(levels[i], shown, sizeof(shown)), &piece, NULL) == 0)
            w += piece + HEADER_PATH_GAP;
    }
    return w;
}

static int _header_pathPiece(SDL_Surface *screen, const char *text, int x, int baseline)
{
    SDL_Color grey = {163, 163, 168};
    char shown[STR_MAX];
    SDL_Surface *piece = TTF_RenderUTF8_Blended(_header_path_font, ui_pageTitle(text, shown, sizeof(shown)), grey);
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
        char shown[STR_MAX];
        _cached_header_title = TTF_RenderUTF8_Blended(resource_getFont(TITLE), ui_pageTitle(current, shown, sizeof(shown)), theme()->title.color);
        strncpy(_cached_header_title_str, current, STR_MAX - 1);
        _cached_header_title_str[STR_MAX - 1] = '\0';
    }
    /* badge only: the title is placed after the path below */
    int x = _theme_renderHeaderTitle(screen, NULL, levels[0], false);
    _header_pathFont();
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
    bool path_shown = _header_path_font && prefix_w > 0 && prefix_w + title_w <= avail;
    if (_header_titlebarHasArt()) {
        // no badge drawn: the title alone, centered, as in MainUI (the
        // path, wider, went over the art again)
        _header_classicTitle(screen, title, current);
        return;
    }
    if (path_shown) {
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

    char shown[STR_MAX];
    SDL_Surface *title = TTF_RenderUTF8_Blended(resource_getFont(TITLE), ui_pageTitle(title_str, shown, sizeof(shown)), theme()->title.color);
    if (title == NULL)
        return;
    _theme_renderHeaderTitle(screen, title, title_str, false);
    SDL_FreeSurface(title);
}

#endif // RENDER_HEADER_H__
