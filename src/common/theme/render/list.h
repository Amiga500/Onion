#ifndef RENDER_MENU_H__
#define RENDER_MENU_H__

#include "components/list.h"
#include "theme/background.h"
#include "theme/config.h"
#include "theme/resources.h"
#include "../../include/arm_opt.h"
#include <string.h>

// static SDL_Color color_black = {0, 0, 0};

// TTF text surface cache for performance optimization
// Caches rendered text to avoid expensive TTF_RenderUTF8_Blended calls every frame
#define TEXT_CACHE_SIZE 128
typedef struct {
    char text[256];
    SDL_Color color;
    SDL_Surface *surface;
    uint32_t last_used;
    bool valid;
} TextCacheEntry;

static TextCacheEntry text_cache[TEXT_CACHE_SIZE] = {{0}};
static uint32_t text_cache_frame = 0;

// Fast hash function for text cache lookup - optimized for ARM
static ALWAYS_INLINE uint32_t text_hash(const char *str, SDL_Color color) {
    uint32_t hash = 5381;
    const unsigned char *p = (const unsigned char *)str;
    while (*p) {
        hash = ((hash << 5) + hash) + *p++;
    }
    // Mix in color for unique hash per color combination
    hash ^= (color.r << 16) | (color.g << 8) | color.b;
    return hash % TEXT_CACHE_SIZE;
}

// Get or create cached text surface - eliminates repeated TTF rendering
// Marked as hot function for compiler optimization
static HOT_FUNCTION SDL_Surface *get_cached_text_surface(TTF_Font *font, const char *text, SDL_Color color) {
    uint32_t hash = text_hash(text, color);
    TextCacheEntry *entry = &text_cache[hash];
    
    // Cache hit: return existing surface (LIKELY hint improves branch prediction)
    if (LIKELY(entry->valid && 
        strcmp(entry->text, text) == 0 &&
        entry->color.r == color.r && 
        entry->color.g == color.g && 
        entry->color.b == color.b)) {
        entry->last_used = text_cache_frame;
        return entry->surface;
    }
    
    // Cache miss or collision: evict old entry and render new (UNLIKELY = cold path)
    if (UNLIKELY(entry->surface != NULL)) {
        SDL_FreeSurface(entry->surface);
    }
    
    entry->surface = TTF_RenderUTF8_Blended(font, text, color);
    strncpy(entry->text, text, sizeof(entry->text) - 1);
    entry->text[sizeof(entry->text) - 1] = '\0';
    entry->color = color;
    entry->last_used = text_cache_frame;
    entry->valid = true;
    
    return entry->surface;
}

// Increment frame counter for cache aging (call once per render)
static inline void text_cache_next_frame(void) {
    text_cache_frame++;
    
    // Periodically evict old entries (every 256 frames, ~4 seconds at 60fps)
    if ((text_cache_frame & 0xFF) == 0) {
        for (int i = 0; i < TEXT_CACHE_SIZE; i++) {
            if (text_cache[i].valid && 
                (text_cache_frame - text_cache[i].last_used) > 512) {
                if (text_cache[i].surface != NULL) {
                    SDL_FreeSurface(text_cache[i].surface);
                    text_cache[i].surface = NULL;
                }
                text_cache[i].valid = false;
            }
        }
    }
}

void theme_renderListLabel(SDL_Surface *screen, const char *label, SDL_Color fg,
                           int offset_x, int center_y, bool is_active,
                           int label_end, bool disabled)
{
    TTF_Font *list_font = resource_getFont(LIST);

    // Use cached text surface instead of rendering every frame
    SDL_Surface *item_label = get_cached_text_surface(list_font, label, fg);
    if (item_label == NULL) return; // Safety check
    
    SDL_Rect item_label_rect = {offset_x, center_y - item_label->h / 2};
    SDL_Rect label_crop = {0, 0, label_end - 30 * g_scale, item_label->h};

    /* Maybe shadows will be an option in the future
    SDL_Rect item_shadow_rect = {item_label_rect.x + 1, item_label_rect.y + 2};

    if (is_active) {
        SDL_Surface *item_shadow =
            get_cached_text_surface(list_font, label, color_black);
        SDL_BlitSurface(item_shadow, &label_crop, screen, &item_shadow_rect);
    }*/

    if (disabled) {
        surfaceSetAlpha(item_label, HIDDEN_ITEM_ALPHA);
    }

    SDL_BlitSurface(item_label, &label_crop, screen, &item_label_rect);
    // DO NOT free - surface is cached and reused
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

            if (item->preview_ptr == NULL && strlen(item->preview_path) > 0 && is_file(item->preview_path)) {
                item->preview_ptr = (void *)IMG_Load(item->preview_path);
            }

            if (item->preview_ptr != NULL)
                active_preview = item;
        }

        const int item_center_y = item_bg_rect.y + item_bg_size.h / 2;
        const int multivalue_width = 226 * g_scale;
        int label_end = 640 * g_scale;
        int offset_x = 20 * g_scale;

        if (item->icon_ptr != NULL) {
            SDL_Surface *icon = (SDL_Surface *)item->icon_ptr;
            if (icon->w > 1) {
                SDL_Rect icon_rect = {offset_x, item_center_y - icon->h / 2};
                SDL_BlitSurface(icon, NULL, screen, &icon_rect);
                offset_x += icon->w + 17 * g_scale;
            }
        }

        if (item->item_type == TOGGLE) {
            SDL_Surface *toggle = show_disabled ? (item->value == 1 ? g_hidden_items.toggle_on : g_hidden_items.toggle_off) : (resource_getSurface(item->value == 1 ? TOGGLE_ON : TOGGLE_OFF));
            toggle_rect.x = 620 * g_scale - toggle->w;
            toggle_rect.y = item_center_y - toggle->h / 2;
            label_end = toggle_rect.x;
            SDL_BlitSurface(toggle, NULL, screen, &toggle_rect);
        }
        else if (item->item_type == MULTIVALUE) {
            SDL_Surface *arrow_left = show_disabled ? g_hidden_items.arrow_left : resource_getSurface(LEFT_ARROW);
            SDL_Surface *arrow_right = show_disabled ? g_hidden_items.arrow_right : resource_getSurface(RIGHT_ARROW);
            SDL_Rect arrow_left_pos = {
                640 * g_scale - 20 * g_scale - arrow_right->w - multivalue_width - arrow_left->w,
                item_center_y - arrow_left->h / 2};
            SDL_Rect arrow_right_pos = {
                640 * g_scale - 20 * g_scale - arrow_right->w,
                item_center_y - arrow_right->h / 2};
            SDL_BlitSurface(arrow_left, NULL, screen, &arrow_left_pos);
            SDL_BlitSurface(arrow_right, NULL, screen, &arrow_right_pos);
            label_end = arrow_left_pos.x;

            char value_str[STR_MAX];
            list_getItemValueLabel(item, value_str);
            // Use cached text surface for value labels too
            SDL_Surface *value_label = get_cached_text_surface(list_font, value_str, theme()->list.color);
            if (value_label == NULL) continue; // Safety check
            
            if (show_disabled) {
                surfaceSetAlpha(value_label, HIDDEN_ITEM_ALPHA);
            }
            SDL_Rect value_size = {0, 0, multivalue_width, value_label->h};
            int label_width = value_label->w > value_size.w ? value_size.w : value_label->w;
            SDL_Rect value_pos = {
                640 * g_scale - 20 * g_scale - arrow_right->w - multivalue_width / 2 - label_width / 2,
                item_center_y - value_size.h / 2};
            SDL_BlitSurface(value_label, &value_size, screen, &value_pos);
            // DO NOT free - cached surface
        }

        theme_renderListLabel(screen, item->label, theme()->list.color,
                              offset_x, item_bg_rect.y + label_y,
                              list->active_pos == i, label_end, show_disabled);

        if (!list_small && strlen(item->description)) {
            theme_renderListLabel(
                screen, item->description, theme()->grid.color, offset_x,
                item_bg_rect.y + 62 * g_scale, list->active_pos == i, label_end, show_disabled);
        }
    }

    if (active_preview != NULL) {
        int preview_width = (double)params.preview_width * g_scale;
        int preview_x = 640.0 * g_scale;

        if (params.preview_bg) {
            SDL_Surface *preview_bg = resource_getSurface(PREVIEW_BG);
            SDL_Rect preview_bg_rect = {640 * g_scale - preview_bg->w, 60 * g_scale};
            SDL_BlitSurface(preview_bg, NULL, screen, &preview_bg_rect);
            preview_x -= preview_bg->w;
        }
        else {
            preview_x -= preview_width;
        }

        SDL_Surface *preview = (SDL_Surface *)active_preview->preview_ptr;
        if (preview) {
            bool free_after = false;

            if (preview->w > preview_width || (params.preview_stretch && preview->w < preview_width)) {
                double scale = (double)preview_width / (double)preview->w;
                preview = zoomSurface(preview, scale, scale, params.preview_smoothing ? SMOOTHING_ON : SMOOTHING_OFF);
                free_after = true;
            }

            SDL_Rect preview_rect = {preview_x + (preview_width - preview->w) / 2,
                                     240 * g_scale - preview->h / 2};
            SDL_BlitSurface(preview, NULL, screen, &preview_rect);

            if (free_after)
                SDL_FreeSurface(preview);
        }
    }
}

void theme_renderList(SDL_Surface *screen, List *list)
{
    // Advance text cache frame counter
    text_cache_next_frame();
    
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
