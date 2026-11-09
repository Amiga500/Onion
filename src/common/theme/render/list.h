#ifndef RENDER_MENU_H__
#define RENDER_MENU_H__

#include <stdint.h>

#include "components/list.h"
#include "theme/background.h"
#include "theme/config.h"
#include "theme/resources.h"

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
static const ListItem *_label_scroll_item = NULL;
static uint32_t _label_scroll_hash = 0;
static uint32_t _label_scroll_start = 0;

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
        item->_label_cache = (void *)TTF_RenderUTF8_Blended(
            resource_getFont(LIST), item->label, fg);
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
        }
        label_crop.x = list_valueScrollOffset(now - _label_scroll_start,
                                              item_label->w - label_crop.w);
        theme_listValueScrolling = true;
    }

    _blit_cached_label(item_label, &label_crop, screen, &item_label_rect,
                       disabled);
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
    theme_listValueScrolling = false;

    if (params.background)
        SDL_BlitSurface(params.background, &params.dim, screen, &params.pos);

    bool list_small = list->list_type == LIST_SMALL;

    TTF_Font *list_font = resource_getFont(LIST);

    SDL_Rect item_bg_rect = {0, 60.0 * g_scale},
             item_div_size = {0, 0, params.dim.w, 4.0 * g_scale},
             item_bg_size = {0, 0, params.dim.w, (list_small ? 56.0 : 90.0) * g_scale},
             toggle_rect = {0, 0};

    int list_height = params.stretch_y ? params.dim.h - 10.0 * g_scale : 420.0 * g_scale;
    int item_padding = list_small && params.show_dividers ? 4.0 * g_scale : 0;
    int item_height = params.stretch_y ? list_height / list->scroll_height : (list_small ? 60.0 : 90.0) * g_scale;
    int label_y = (list_small ? 27.0 : 37.0) * g_scale;
    SDL_Surface *item_bg = resource_getSurface(list_small ? BG_LIST_S : BG_LIST_L);

    int menu_pos_y = params.stretch_y ? params.pos.y + 4.0 * g_scale + (list_height - list->scroll_height * item_height) / 2
                                      : 420.0 * g_scale - list->scroll_height * item_height;
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
        if (params.show_dividers)
            SDL_BlitSurface(resource_getSurface(HORIZONTAL_DIVIDER), &item_div_size, screen, &item_bg_rect);
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
        ListItem *item = &list->items[i];
        bool show_disabled = item->disabled && !item->show_opaque;

        item_bg_rect.y = menu_pos_y + (i - list->scroll_pos) * item_height;

        if (params.stretch_y) {
            item_bg_rect.y += (item_height - item_bg_size.h) / 2;
        }

        if (params.show_dividers)
            SDL_BlitSurface(resource_getSurface(HORIZONTAL_DIVIDER), &item_div_size, screen, &item_bg_rect);
        item_bg_rect.y += item_padding;

        if (i == list->active_pos) {
            SDL_BlitSurface(item_bg, &item_bg_size, screen, &item_bg_rect);

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
            SDL_Surface *toggle = show_disabled ? (item->value == 1 ? g_hidden_items.toggle_on : g_hidden_items.toggle_off) : (resource_getSurface(item->value == 1 ? TOGGLE_ON : TOGGLE_OFF));
            toggle_rect.x = scaled_620 - toggle->w;
            toggle_rect.y = item_center_y - toggle->h / 2;
            label_end = toggle_rect.x;
            SDL_BlitSurface(toggle, NULL, screen, &toggle_rect);
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
            SDL_BlitSurface(arrow_left, NULL, screen, &arrow_left_pos);
            SDL_BlitSurface(arrow_right, NULL, screen, &arrow_right_pos);
            label_end = arrow_left_pos.x;

            // Cache MULTIVALUE label — only re-render when value or color changes
            /* Miyoo SDL_Color is RGB-only (no unused/.a field). */
            const uint32_t list_color_key = ((uint32_t)list_color.r << 16) |
                                            ((uint32_t)list_color.g << 8) |
                                            (uint32_t)list_color.b;
            char value_str[STR_MAX];
            list_getItemValueLabel(item, value_str);
            const uint32_t value_hash = list_labelHash(value_str);
            if (item->_value_cache == NULL || item->_cached_value_hash != value_hash ||
                item->_cached_color != list_color_key) {
                if (item->_value_cache != NULL)
                    SDL_FreeSurface((SDL_Surface *)item->_value_cache);
                item->_value_cache = (void *)TTF_RenderUTF8_Blended(list_font, value_str, list_color);
                item->_cached_value_hash = value_hash;
                item->_cached_color = list_color_key;
            }
            SDL_Surface *value_label = (SDL_Surface *)item->_value_cache;
            // An empty value renders nothing (NULL): only the value is skipped. The
            // `continue` that was here left the row without its label too.
            if (value_label != NULL) {
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
                    }
                    value_size.x = list_valueScrollOffset(now - _value_scroll_start,
                                                          value_label->w - scaled_226);
                    theme_listValueScrolling = true;
                }
                SDL_Rect value_pos = {
                    scaled_640 - scaled_20 - arrow_right->w - scaled_226 / 2 - label_width / 2,
                    item_center_y - value_size.h / 2};
                _blit_cached_label(value_label, &value_size, screen, &value_pos,
                                   show_disabled);
            }
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
                    SDL_Surface *scaled = zoomSurface(preview, scale, scale, params.preview_smoothing ? SMOOTHING_ON : SMOOTHING_OFF);
                    active_preview->_scaled_preview = (void *)scaled;
                    // Width 0 after a failed zoom: scaled again next frame, never taken
                    // from the cache as a NULL preview.
                    active_preview->_scaled_preview_w = scaled != NULL ? preview_width : 0;
                    // NULL (out of memory): drawn unscaled this time (preview->w crashed).
                    if (scaled != NULL)
                        preview = scaled;
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

#endif // RENDER_MENU_H__
