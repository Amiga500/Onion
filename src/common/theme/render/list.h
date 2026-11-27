#ifndef RENDER_MENU_H__
#define RENDER_MENU_H__

#include <stdint.h>

#include "components/list.h"
#include "theme/background.h"
#include "theme/config.h"
#include "theme/resources.h"

#include "./unified.h"

// Fast FNV-1a hash for cache invalidation
static inline uint32_t _label_hash_fnv1a(const char *str)
{
    uint32_t hash = 2166136261u;
    if (str == NULL)
        return hash;
    for (const char *p = str; *p; p++) {
        hash ^= (uint8_t)*p;
        hash *= 16777619u;
    }
    return hash;
}

// Blit a cached TTF surface. Never call surfaceSetAlpha() on the cache:
// it multiplies per-pixel alpha, and "restore" with 255 is identity, so
// disabled rows would dim further every frame.
static void _blit_cached_label(SDL_Surface *src, SDL_Rect *srcrect,
                               SDL_Surface *dst, SDL_Rect *dstrect,
                               bool disabled)
{
    if (src == NULL || dst == NULL)
        return;

    if (!disabled) {
        SDL_BlitSurface(src, srcrect, dst, dstrect);
        return;
    }

    SDL_Surface *dimmed = SDL_ConvertSurface(src, src->format, 0);
    if (dimmed == NULL) {
        SDL_BlitSurface(src, srcrect, dst, dstrect);
        return;
    }

    surfaceSetAlpha(dimmed, HIDDEN_ITEM_ALPHA);
    SDL_BlitSurface(dimmed, srcrect, dst, dstrect);
    SDL_FreeSurface(dimmed);
}

// A program that redraws its list about 30 times a second while
// theme_listValueScrolling is true (Tweaks) sets theme_listCanScroll: then
// the selected row's value or label, when too wide, scrolls back and forth.
// Elsewhere (the GameSwitcher menus, prompt) they show their start, cut.
static bool theme_listCanScroll = false;
// True after a render that drew a scrolling value or label (see
// list_valueScrollOffset): the program redraws the list to move it.
static bool theme_listValueScrolling = false;
static const ListItem *_value_scroll_item = NULL;
static uint32_t _value_scroll_hash = 0;
static uint32_t _value_scroll_start = 0;
static int _value_scroll_drawn = 0; // offset drawn last (list_valueScrollStep)
static const ListItem *_label_scroll_item = NULL;
static uint32_t _label_scroll_hash = 0;
static uint32_t _label_scroll_start = 0;
static int _label_scroll_drawn = 0;

// theme_renderListActiveRow: only the selected row is drawn (the row that
// scrolls), over its own part of the background; _list_row_rect is where.
static bool _list_only_active = false;
static bool _list_row_drawn = false;
static SDL_Rect _list_row_rect;

// Render a list label with per-item TTF surface caching
// Caches the TTF render in item->_label_cache; only re-renders when label changes.
// The selected row's label, when too wide, scrolls back and forth like a long
// value (was_scrolling: the previous render scrolled something, so the same
// row goes on from where it was instead of starting over).
static void _theme_renderListLabelCached(SDL_Surface *screen, ListItem *item,
                                         SDL_Color fg, int offset_x, int center_y,
                                         int label_end, bool disabled,
                                         bool is_active, bool was_scrolling)
{
    uint32_t hash = _label_hash_fnv1a(item->label);
    if (item->_label_cache == NULL || item->_label_hash != hash) {
        if (item->_label_cache != NULL)
            SDL_FreeSurface((SDL_Surface *)item->_label_cache);
        // Unified look: "Startup..." shows as "Startup" and a chevron.
        char shown[STR_MAX];
        strncpy(shown, item->label, STR_MAX - 1);
        shown[STR_MAX - 1] = '\0';
        if (item->item_type == ACTION && ui_opensPage(shown))
            shown[strlen(shown) - 3] = '\0';
        item->_label_cache = (void *)TTF_RenderUTF8_Blended(
            resource_getFont(LIST), shown, fg);
        item->_label_hash = hash;
    }
    SDL_Surface *item_label = (SDL_Surface *)item->_label_cache;
    if (item_label == NULL)
        return;

    SDL_Rect item_label_rect = {offset_x, center_y - item_label->h / 2};
    SDL_Rect label_crop = {0, 0, label_end - 30 * g_scale, item_label->h};

    if (theme_listCanScroll && is_active && label_crop.w > 0 &&
        item_label->w > label_crop.w) {
        uint32_t now = SDL_GetTicks();
        if (!was_scrolling || _label_scroll_item != item ||
            _label_scroll_hash != hash) {
            _label_scroll_item = item;
            _label_scroll_hash = hash;
            _label_scroll_start = now;
            _label_scroll_drawn = 0;
        }
        _label_scroll_drawn = list_valueScrollStep(
            _label_scroll_drawn,
            list_valueScrollOffset(now - _label_scroll_start, item_label->w - label_crop.w));
        label_crop.x = _label_scroll_drawn;
        theme_listValueScrolling = true;
    }

    _blit_cached_label(item_label, &label_crop, screen, &item_label_rect,
                       disabled);
}

// Unified look: a small list's description, right-aligned on the row in a
// smaller size of the list font (the font itself when it can't be opened),
// grey, white on the selected row. Rendered per draw: lists with
// descriptions are short (Tweaks' main menu).
static TTF_Font *_theme_listDescFont(void)
{
    static bool tried = false;
    static TTF_Font *font = NULL;
    if (!tried) {
        tried = true;
        font = theme_loadFont(theme()->path, theme()->list.font, 17.0 * g_scale);
    }
    return font ? font : resource_getFont(LIST);
}

// Draws it ending at right_x; returns where it starts (right_x when none).
static int _theme_renderListDescRight(SDL_Surface *screen, const char *desc,
                                      int right_x, int center_y, bool active,
                                      bool disabled)
{
    if (desc == NULL || desc[0] == '\0')
        return right_x;
    SDL_Color c = active ? (SDL_Color){255, 255, 255}
                         : (SDL_Color){UI_GREY_R, UI_GREY_G, UI_GREY_B};
    SDL_Surface *text = TTF_RenderUTF8_Blended(_theme_listDescFont(), desc, c);
    if (text == NULL)
        return right_x;
    // At most half the row, so the name keeps its room.
    int max_w = 300.0 * g_scale;
    SDL_Rect crop = {0, 0, text->w > max_w ? max_w : text->w, text->h};
    SDL_Rect pos = {right_x - crop.w, center_y - text->h / 2};
    _blit_cached_label(text, &crop, screen, &pos, disabled);
    SDL_FreeSurface(text);
    return pos.x;
}

// Uncached version for one-off labels (descriptions, sticky notes)
void theme_renderListLabel(SDL_Surface *screen, const char *label, SDL_Color fg,
                           int offset_x, int center_y, bool is_active,
                           int label_end, bool disabled)
{
    TTF_Font *list_font = resource_getFont(LIST);

    SDL_Surface *item_label = TTF_RenderUTF8_Blended(list_font, label, fg);
    if (item_label == NULL)
        return;
    SDL_Rect item_label_rect = {offset_x, center_y - item_label->h / 2};

    SDL_Rect label_crop = {0, 0, label_end - 30 * g_scale, item_label->h};

    if (disabled) {
        surfaceSetAlpha(item_label, HIDDEN_ITEM_ALPHA);
    }

    SDL_BlitSurface(item_label, &label_crop, screen, &item_label_rect);
    SDL_FreeSurface(item_label);
}

/* New look: the selected row's note under the list, in a box with an (i),
 * two lines at most (the rest stays in the SELECT note). */
static TTF_Font *_list_note_font = NULL;

static void _list_noteLine(SDL_Surface *screen, const char *text, int x, int y)
{
    SDL_Surface *s = TTF_RenderUTF8_Blended(_list_note_font, text, (SDL_Color){163, 163, 168});
    if (s) {
        SDL_Rect r = {x, y};
        SDL_BlitSurface(s, NULL, screen, &r);
        SDL_FreeSurface(s);
    }
}

/* Whether the note box under the list, as last drawn, showed the selected
 * row's note whole: then SELECT has nothing more to show (Tweaks hides its
 * INFO hint and opens no window, which only repeated the same words). */
static bool _list_note_whole = false;

static inline bool theme_listNoteShownWhole(void)
{
    return _list_note_whole;
}

static void theme_renderListNote(SDL_Surface *screen, const char *note, SDL_Rect box)
{
    _list_note_whole = false;
    if (_list_note_font == NULL)
        _list_note_font = theme_loadFont(theme()->path, theme()->list.font, 16);
    ui_fillRoundRect(screen, box, 10.0 * g_scale, SDL_MapRGB(screen->format, 38, 38, 41));
    /* the (i): a grey ring with an i */
    int r = 8.0 * g_scale, cx = box.x + 18.0 * g_scale, cy = box.y + box.h / 2;
    ui_fillRoundRect(screen, (SDL_Rect){cx - r, cy - r, 2 * r, 2 * r}, r, SDL_MapRGB(screen->format, 163, 163, 168));
    int ri = r - 1.5 * g_scale;
    ui_fillRoundRect(screen, (SDL_Rect){cx - ri, cy - ri, 2 * ri, 2 * ri}, ri, SDL_MapRGB(screen->format, 38, 38, 41));
    SDL_FillRect(screen, &(SDL_Rect){cx - 1, cy - 2 * g_scale, 2 * g_scale, 6 * g_scale}, SDL_MapRGB(screen->format, 163, 163, 168));
    SDL_FillRect(screen, &(SDL_Rect){cx - 1, cy - 5 * g_scale, 2 * g_scale, 2 * g_scale}, SDL_MapRGB(screen->format, 163, 163, 168));
    if (_list_note_font == NULL || note == NULL)
        return;

    char text[STR_MAX];
    int n = 0;
    for (const char *p = note; *p && n < STR_MAX - 1; p++)
        text[n++] = *p == '\n' ? ' ' : *p;
    text[n] = '\0';
    int x = box.x + 34.0 * g_scale, max_w = box.x + box.w - 12.0 * g_scale - x;
    int line_h = TTF_FontLineSkip(_list_note_font);
    char line[2][STR_MAX] = {"", ""};
    int li = 0;
    char *save = NULL;
    for (char *w = strtok_r(text, " ", &save); w && li < 2; w = strtok_r(NULL, " ", &save)) {
        char tryl[STR_MAX];
        snprintf(tryl, sizeof(tryl), "%s%s%s", line[li], line[li][0] ? " " : "", w);
        int tw = 0;
        TTF_SizeUTF8(_list_note_font, tryl, &tw, NULL);
        if (tw <= max_w || line[li][0] == '\0') {
            snprintf(line[li], STR_MAX, "%s", tryl);
            continue;
        }
        if (li == 1) {
            /* more than two lines: end the second with "..." */
            int lw = 0;
            size_t len = strlen(line[1]);
            do {
                snprintf(tryl, sizeof(tryl), "%.*s...", (int)len, line[1]);
                TTF_SizeUTF8(_list_note_font, tryl, &lw, NULL);
            } while (lw > max_w && len-- > 0);
            snprintf(line[1], STR_MAX, "%s", tryl);
            li = 3; /* cut short */
            break;
        }
        li++;
        snprintf(line[li], STR_MAX, "%s", w);
    }
    /* li 3: cut short with "..."; 2: a word left over after two lines */
    _list_note_whole = li < 2;
    int lines = line[1][0] ? 2 : 1;
    int y = cy - lines * line_h / 2;
    for (int i = 0; i < lines; i++)
        _list_noteLine(screen, line[i], x, y + i * line_h);
}

typedef struct {
    SDL_Surface *background;
    SDL_Rect dim;
    SDL_Rect pos;
    bool show_dividers;
    bool stretch_y;
    bool preview_bg;
    bool preview_stretch;
    int preview_width;
    bool preview_smoothing;
} ListRenderParams_s;

void theme_renderListCustom(SDL_Surface *screen, List *list, ListRenderParams_s params)
{
    // Coming back to a row starts its scroll over, not from where it was.
    const bool was_scrolling = theme_listValueScrolling;

    // Only the selected row: not with a sticky note or a preview picture,
    // which are drawn over other rows (then the caller draws it all).
    _list_row_drawn = false;
    if (_list_only_active &&
        (list->has_sticky || list->active_pos < list->scroll_pos ||
         list->active_pos >= list->scroll_pos + list->scroll_height ||
         list->active_pos >= list->item_count ||
         list->items[list->active_pos].preview_path[0] != '\0' ||
         list->items[list->active_pos].preview_ptr != NULL))
        return;

    theme_listValueScrolling = false;

    if (params.background && !_list_only_active)
        SDL_BlitSurface(params.background, &params.dim, screen, &params.pos);

    bool list_small = list->list_type == LIST_SMALL;

    TTF_Font *list_font = resource_getFont(LIST);

    SDL_Rect item_bg_rect = {0, 60.0 * g_scale},
             item_bg_size = {0, 0, params.dim.w, (list_small ? 56.0 : 90.0) * g_scale};

    int list_height = params.stretch_y ? params.dim.h - 10.0 * g_scale : 420.0 * g_scale;
    int item_padding = list_small && params.show_dividers ? 4.0 * g_scale : 0;
    int item_height = params.stretch_y ? list_height / list->scroll_height : (list_small ? 60.0 : 90.0) * g_scale;
    int label_y = (list_small ? 27.0 : 37.0) * g_scale;

    int menu_pos_y = params.stretch_y ? params.pos.y + 4.0 * g_scale + (list_height - list->scroll_height * item_height) / 2
                                      : 420.0 * g_scale - list->scroll_height * item_height;

    /* New look: a full-screen small list whose rows have notes shows the
     * selected row's note in a box under it; the rows get 50 px for room. */
    bool with_note = false;
    if (list_small && !params.stretch_y && list->scroll_height <= 6 &&
        params.dim.w >= 600.0 * g_scale && list->active_pos < list->item_count)
        for (int i = 0; i < list->item_count && !with_note; i++)
            with_note = list->items[i].info_note[0] != '\0';
    if (with_note) {
        item_height = 50.0 * g_scale;
        item_bg_size.h = 50.0 * g_scale;
        item_padding = 0;
        label_y = 25.0 * g_scale;
        menu_pos_y = 62.0 * g_scale;
    }
    int last_item = list->scroll_pos + list->scroll_height;

    if (last_item > list->item_count)
        last_item = list->item_count;

    ListItem *active_preview = NULL;

    // Check if there are hidden items
    bool has_disabled_items = false;
    for (int i = list->scroll_pos; i < last_item; i++) {
        if (list->items[i].disabled && !list->items[i].show_opaque) {
            has_disabled_items = true;
            break;
        }
    }

    // Load hidden items if needed
    if (has_disabled_items) {
        theme_loadHiddenItems();
    }

    if (list->has_sticky) {
        ListItem *active_item = &list->items[list->active_pos];
        theme_renderListLabel(screen, active_item->sticky_note, theme()->list.color, 20 * g_scale, item_bg_rect.y + label_y, false, 640 * g_scale, true);
    }

    // Pre-compute scaled constants used in the loop
    const int scaled_640 = 640 * g_scale;
    const int scaled_620 = 620 * g_scale;
    const int scaled_60 = 60 * g_scale;
    const int scaled_20 = 20 * g_scale;
    const int scaled_17 = 17 * g_scale;
    const int scaled_62 = 62 * g_scale;
    const int scaled_226 = 226 * g_scale;

    // Hoist theme colors out of per-item loop
    const SDL_Color list_color = theme()->list.color;
    const SDL_Color grid_color = theme()->grid.color;

    for (int i = list->scroll_pos; i < last_item; i++) {
        if (_list_only_active && i != list->active_pos)
            continue;
        ListItem *item = &list->items[i];
        bool show_disabled = item->disabled && !item->show_opaque;

        item_bg_rect.y = menu_pos_y + (i - list->scroll_pos) * item_height;

        if (params.stretch_y) {
            item_bg_rect.y += (item_height - item_bg_size.h) / 2;
        }

        // Unified look: no divider lines between rows.
        item_bg_rect.y += item_padding;

        if (_list_only_active) {
            // the row's own part of the background, then the row
            _list_row_rect = (SDL_Rect){params.pos.x, item_bg_rect.y, params.dim.w, item_bg_size.h};
            if (params.background) {
                SDL_Rect src = {params.dim.x, params.dim.y + (item_bg_rect.y - params.pos.y),
                                params.dim.w, item_bg_size.h};
                SDL_Rect dst = _list_row_rect;
                SDL_BlitSurface(params.background, &src, screen, &dst);
            }
            _list_row_drawn = true;
        }

        if (i == list->active_pos) {
            /* New look: the selected row is a rounded pill in the theme's
             * selection colour, 10 px in from each side. */
            int inset = 10.0 * g_scale, vpad = 3.0 * g_scale;
            SDL_Rect pill = {item_bg_rect.x + inset, item_bg_rect.y + vpad,
                             params.dim.w - 2 * inset, item_bg_size.h - 2 * vpad};
            ui_fillRoundRect(screen, pill, 10.0 * g_scale, ui_map(screen, ui_accent()));

            if (list_item_wantsPreviewLoad(item)) {
                if (is_file(item->preview_path))
                    item->preview_ptr = (void *)IMG_Load(item->preview_path);
                if (item->preview_ptr == NULL)
                    item->_preview_missing = true; // stop re-checking until the path is reset
            }

            if (item->preview_ptr != NULL)
                active_preview = item;
        }

        const int item_center_y = item_bg_rect.y + item_bg_size.h / 2;
        int label_end = scaled_640;
        int offset_x = scaled_20;

        if (item->icon_ptr != NULL) {
            SDL_Surface *icon = (SDL_Surface *)item->icon_ptr;
            if (icon->w > 1) {
                SDL_Rect icon_rect = {offset_x, item_center_y - icon->h / 2};
                SDL_BlitSurface(icon, NULL, screen, &icon_rect);
                offset_x += icon->w + scaled_17;
            }
        }

        if (item->item_type == TOGGLE) {
            // Unified look: the switch is drawn, in the theme's colours.
            int w = ui_drawSwitch(screen, scaled_620, item_center_y, item->value == 1,
                                  i == list->active_pos, show_disabled);
            label_end = scaled_620 - w;
        }
        else if (item->item_type == ACTION &&
                 (ui_opensPage(item->label) || (list_small && item->description[0] != '\0'))) {
            SDL_Color c = i == list->active_pos ? (SDL_Color){255, 255, 255}
                                                : (SDL_Color){122, 122, 128};
            int half = 7.0 * g_scale;
            ui_drawChevron(screen, scaled_620 - 2, item_center_y, half, 1, ui_map(screen, c));
            label_end = scaled_620 - half * 2;
        }
        else if (item->item_type == MULTIVALUE) {
            SDL_Surface *arrow_left = show_disabled ? g_hidden_items.arrow_left : resource_getSurface(LEFT_ARROW);
            SDL_Surface *arrow_right = show_disabled ? g_hidden_items.arrow_right : resource_getSurface(RIGHT_ARROW);
            SDL_Rect arrow_left_pos = {
                scaled_640 - scaled_20 - arrow_right->w - scaled_226 - arrow_left->w,
                item_center_y - arrow_left->h / 2};
            SDL_Rect arrow_right_pos = {
                scaled_640 - scaled_20 - arrow_right->w,
                item_center_y - arrow_right->h / 2};
            // Unified look: the arrows only on the selected row, the value
            // in grey on the others.
            if (i == list->active_pos) {
                SDL_BlitSurface(arrow_left, NULL, screen, &arrow_left_pos);
                SDL_BlitSurface(arrow_right, NULL, screen, &arrow_right_pos);
            }
            label_end = arrow_left_pos.x;
            const SDL_Color value_color = i == list->active_pos
                                              ? list_color
                                              : (SDL_Color){UI_GREY_R, UI_GREY_G, UI_GREY_B};

            // Cache MULTIVALUE label — only re-render when value or color changes
            /* Miyoo SDL_Color is RGB-only (no unused/.a field). */
            const uint32_t list_color_key = ((uint32_t)value_color.r << 16) |
                                            ((uint32_t)value_color.g << 8) |
                                            (uint32_t)value_color.b;
            char value_str[STR_MAX];
            list_getItemValueLabel(item, value_str);
            const uint32_t value_hash = list_labelHash(value_str);
            if (item->_value_cache == NULL || item->_cached_value_hash != value_hash ||
                item->_cached_color != list_color_key) {
                if (item->_value_cache != NULL)
                    SDL_FreeSurface((SDL_Surface *)item->_value_cache);
                item->_value_cache = (void *)TTF_RenderUTF8_Blended(list_font, value_str, value_color);
                item->_cached_value_hash = value_hash;
                item->_cached_color = list_color_key;
            }
            SDL_Surface *value_label = (SDL_Surface *)item->_value_cache;
            if (value_label == NULL)
                continue;
            SDL_Rect value_size = {0, 0, scaled_226, value_label->h};
            int label_width = value_label->w > value_size.w ? value_size.w : value_label->w;
            // The selected row's value, when too wide, scrolls back and forth
            // instead of being cut; the others show their start.
            if (theme_listCanScroll && i == list->active_pos &&
                value_label->w > scaled_226) {
                uint32_t now = SDL_GetTicks();
                if (!was_scrolling || _value_scroll_item != item ||
                    _value_scroll_hash != value_hash) {
                    _value_scroll_item = item;
                    _value_scroll_hash = value_hash;
                    _value_scroll_start = now;
                    _value_scroll_drawn = 0;
                }
                _value_scroll_drawn = list_valueScrollStep(
                    _value_scroll_drawn,
                    list_valueScrollOffset(now - _value_scroll_start, value_label->w - scaled_226));
                value_size.x = _value_scroll_drawn;
                theme_listValueScrolling = true;
            }
            SDL_Rect value_pos = {
                scaled_640 - scaled_20 - arrow_right->w - scaled_226 / 2 - label_width / 2,
                item_center_y - value_size.h / 2};
            _blit_cached_label(value_label, &value_size, screen, &value_pos,
                               show_disabled);
        }

        if (list_small && item->description[0] != '\0') {
            int desc_x = _theme_renderListDescRight(screen, item->description,
                                                    label_end - 12.0 * g_scale,
                                                    item_center_y, i == list->active_pos,
                                                    show_disabled);
            label_end = desc_x - 4.0 * g_scale;
        }

        // Use cached label rendering for main item label (avoids TTF_RenderUTF8 per frame)
        _theme_renderListLabelCached(screen, item, list_color,
                                     offset_x, item_bg_rect.y + label_y,
                                     label_end, show_disabled,
                                     i == list->active_pos, was_scrolling);

        if (!list_small && item->description[0] != '\0') {
            theme_renderListLabel(
                screen, item->description, grid_color, offset_x,
                item_bg_rect.y + scaled_62, list->active_pos == i, label_end, show_disabled);
        }
    }

    if (_list_only_active)
        return; // the note and the preview are not in the row

    _list_note_whole = false;
    if (with_note) {
        SDL_Rect box = {12.0 * g_scale, 366.0 * g_scale, params.dim.w - 24.0 * g_scale, 50.0 * g_scale};
        theme_renderListNote(screen, list->items[list->active_pos].info_note, box);
    }

    if (active_preview != NULL) {
        int preview_width = (double)params.preview_width * g_scale;
        int preview_x = scaled_640;

        if (params.preview_bg) {
            SDL_Surface *preview_bg = resource_getSurface(PREVIEW_BG);
            SDL_Rect preview_bg_rect = {scaled_640 - preview_bg->w, scaled_60};
            SDL_BlitSurface(preview_bg, NULL, screen, &preview_bg_rect);
            preview_x -= preview_bg->w;
        }
        else {
            preview_x -= preview_width;
        }

        SDL_Surface *preview = (SDL_Surface *)active_preview->preview_ptr;
        if (preview) {
            if (preview->w > preview_width || (params.preview_stretch && preview->w < preview_width)) {
                // Use cached scaled preview if available and width matches
                if (list_item_hasScaledPreview(active_preview, preview_width)) {
                    preview = (SDL_Surface *)active_preview->_scaled_preview;
                }
                else {
                    // Free old cache if width changed
                    if (active_preview->_scaled_preview != NULL)
                        SDL_FreeSurface((SDL_Surface *)active_preview->_scaled_preview);
                    double scale = (double)preview_width / (double)preview->w;
                    preview = zoomSurface(preview, scale, scale, params.preview_smoothing ? SMOOTHING_ON : SMOOTHING_OFF);
                    active_preview->_scaled_preview = (void *)preview;
                    active_preview->_scaled_preview_w = preview_width;
                }
            }

            SDL_Rect preview_rect = {preview_x + (preview_width - preview->w) / 2,
                                     240 * g_scale - preview->h / 2};
            SDL_BlitSurface(preview, NULL, screen, &preview_rect);
        }
    }
}

void theme_renderList(SDL_Surface *screen, List *list)
{
    ListRenderParams_s params = {
        .background = theme_background(),
        .dim = {0, 60 * g_scale, 640 * g_scale, 360 * g_scale},
        .pos = {0, 60 * g_scale},
        .show_dividers = true,
        .stretch_y = false,
        .preview_bg = true,
        .preview_stretch = false,
        .preview_width = 250,
        .preview_smoothing = false,
    };

    theme_renderListCustom(screen, list, params);
}

// Only the selected row of theme_renderList, over its part of the
// background: a row that scrolls (Tweaks) is redrawn at every frame, and
// the whole list was too much work for 60 a second (robcodedev: "requires
// higher fps"). False when it can't be done alone (a sticky note, a
// preview picture): then the caller draws the whole list. *rect: where it
// was drawn, to put on the screen.
bool theme_renderListActiveRow(SDL_Surface *screen, List *list, SDL_Rect *rect)
{
    _list_only_active = true;
    theme_renderList(screen, list);
    _list_only_active = false;
    if (_list_row_drawn && rect != NULL)
        *rect = _list_row_rect;
    return _list_row_drawn;
}

#endif // RENDER_MENU_H__
