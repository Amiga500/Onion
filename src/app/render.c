/* SPDX-License-Identifier: GPL-3.0-only */
#include "app/render.h"
#include "platform/audio.h"
#include "platform/framebuffer_probe.h"
#include "platform/timing.h"
#include "ui/drawing.h"
#include "ui/panels.h"
#include "ui/search_label.h"
#ifdef MAINUI_ONION
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Read the line-oriented format used by legacy Favorite/Recent records.
 * This preview only displays labels: it does not claim to implement the full
 * Recent filtering or Favorite-folder model, and never rewrites the input.
 */
bool mainui_load_list(MainUIPreviewList *list, const char *path)
{
    list->data = mainui_read_text(path, 8 * 1024 * 1024);
    if (!list->data) {
        return false;
    }
    char *line = list->data;
    while (*line && list->count < 10000) {
        char *next = strchr(line, '\n');
        if (next) {
            *next = 0;
        }
        cJSON *item = cJSON_ParseWithOpts(line, NULL, true);
        const cJSON *label = cJSON_GetObjectItemCaseSensitive(item, "label");
        if (cJSON_IsObject(item) && cJSON_IsString(label)) {
            list->records[list->count] = item;
            list->labels[list->count++] = label->valuestring;
        }
        else {
            cJSON_Delete(item);
        }
        if (!next) {
            break;
        }
        line = next + 1;
    }
    return true;
}

void mainui_close_list(MainUIPreviewList *list)
{
    for (int i = 0; i < list->count; i++) {
        cJSON_Delete(list->records[i]);
    }
    free(list->data);
}

static void blit(SDL_Surface *surface, SDL_Surface *screen, int x, int y)
{
    SDL_Rect dest = {(Sint16)x, (Sint16)y, 0, 0};
    if (surface) {
        SDL_BlitSurface(surface, NULL, screen, &dest);
    }
}

const char *mainui_list_label_at(void *context, int index)
{
    MainUIListLabels *source = context;
    if (source->library) {
        return mainui_library_label(source->library, index);
    }
    if (source->catalog) {
        return mainui_browser_label(source->catalog, index);
    }
    return source->preview->labels[index];
}

/* Stock list headers retain the section/system name at every folder depth. */
const char *mainui_library_heading(const MainUILibrary *library)
{
    return library->recent ? mainui_translate(18, "Recents") : mainui_translate(1, "Favorites");
}

int mainui_console_mode(const MainUIApp *ui)
{
    if (ui->home || ui->apps || !ui->catalog || ui->library || ui->catalog->depth) {
        return -1;
    }
    return ui->catalog == ui->expert ? ui->console_view.expert : ui->console_view.games;
}

const char *mainui_catalog_heading(const MainUICatalog *catalog)
{
    return catalog->pages[catalog->depth > 0 ? 1 : 0].title;
}

/* Pagination keeps physical rows; counters exclude folders as in the patcher. */
static void draw_list_counter(MainUIApp *ui)
{
    SDL_Surface *screen = ui->screen;
    MainUITheme *theme = &ui->theme;
    const MainUILibrary *library = ui->library;
    MainUICatalog *catalog = ui->catalog;
    const MainUIViewport *view = &ui->view;
    if (library || !catalog || catalog->depth) {
        ui->console_games_catalog = NULL; /* counted again on the way back */
    }
    int current = view->selected + 1;
    int total = view->total;
    if (total <= 0) {
        mainui_draw_footer(screen, theme, 0, -1);
        return;
    }
    if (library) {
        if (mainui_library_is_folder(library, view->selected)) {
            int row = library->visible[view->selected];
            if (row != INT_MIN) {
                mainui_draw_folder_footer(screen, theme, library->folders[-row - 1].direct_games);
                return;
            }
            mainui_draw_footer(screen, theme, 0, -1);
            return;
        }
        current -= library->leading_folders;
        total = library->visible_games;
    }
    else if (catalog && !catalog->depth) {
        /* Consoles listed: the selected console's games, "(762)", as a
         * folder's, once it has a ROM list cache, else nothing. Read once
         * per selection, not every frame. "count": false shows nothing. */
        if (ui->console_view.hide_count) {
            mainui_draw_footer(screen, theme, 0, -1);
            return;
        }
        if (ui->console_games_catalog != catalog || ui->console_games_index != view->selected) {
            ui->console_games_catalog = catalog;
            ui->console_games_index = view->selected;
            ui->console_games = mainui_catalog_game_count(catalog, view->selected);
        }
        if (ui->console_games >= 0) {
            mainui_draw_folder_footer(
                screen, theme, ui->console_games > INT_MAX ? INT_MAX : (int)ui->console_games);
        }
        else {
            mainui_draw_footer(screen, theme, 0, -1);
        }
        return;
    }
    else if (catalog) {
        if (mainui_browser_folder(catalog, view->selected)) {
            mainui_draw_footer(screen, theme, 0, -1);
            return;
        }
        int folders = catalog->pages[catalog->depth].folder_count + (catalog->depth > 1);
        current -= folders;
        total -= folders;
    }
    mainui_draw_footer(screen, theme, current, total);
}

/* The selected game's video in place of its cover (.romListVideoPreview):
 * started once the selection has stayed for the delay, and stopped by any
 * change of selection or screen. Only on a list with nothing over it. */
static void update_list_video(MainUIApp *ui)
{
    MainUIListVideo *preview = &ui->list_video;
    Uint32 now = SDL_GetTicks();
    int delay = ui->config.video_preview_delay;
    bool list = delay >= 0 && !ui->home && !ui->apps && ui->catalog &&
                (ui->library || ui->catalog->depth) && !ui->details.open && !ui->settings_open &&
                !ui->settings_page.open && !ui->language_open && !ui->context_open &&
                ui->confirmation < 0 && !*ui->message_title && !ui->name_input.open &&
                !ui->search_keyboard && !ui->launch_pending && !ui->catalog_job.thread &&
                !ui->letter_jump.active && ui->device_status.sleeping != 1 &&
                ui->view.selected >= 0 && ui->view.selected < ui->view.total;
    /* Covered, left or asleep: a dismissed video stays dismissed for its
     * game, until another is selected. */
    if (!list) {
        mainui_list_video_hide(preview);
        return;
    }
    if (*preview->key && !strcmp(preview->key, ui->selected_identity)) {
        mainui_list_video_advance(preview, now);
        return;
    }
    mainui_list_video_stop(preview);
    /* A snapshot plays each selection's video at the instant it gives. */
    Uint32 elapsed = ui->snapshot ? ui->snapshot_elapsed : now - ui->selected_at;
    if (elapsed < (Uint32)delay) {
        preview->waiting = true;
        preview->start_at = ui->selected_at + (Uint32)delay;
        return;
    }

    /* 16 KiB of paths: kept off the stack. */
    struct {
        char rom[MAINUI_PATH_MAX], root[MAINUI_PATH_MAX], path[4096], library_dir[4096];
    } *paths = malloc(sizeof *paths);

    bool found = paths &&
                 mainui_details_locate(ui->catalog, ui->library, ui->view.selected, paths->rom,
                                       paths->root) &&
                 mainui_media_video(paths->root, paths->rom, paths->path);
    if (paths) {
        snprintf(paths->library_dir, sizeof paths->library_dir, "%s/.tmp_update/lib/parasyte",
                 ui->sd);
    }
    mainui_list_video_start(preview, ui->selected_identity, found ? paths->path : NULL,
                            paths ? paths->library_dir : NULL, ui->screen, now,
                            ui->config.video_preview_fullscreen, ui->snapshot != NULL);
    free(paths);
}

/* Saves the console selected in Games or Expert as soon as it changes, so a
 * MainUI started again opens there (console_position.h). */
static void remember_console(MainUIApp *ui)
{
    int position = ui->catalog == ui->games    ? MAINUI_POSITION_GAMES
                   : ui->catalog == ui->expert ? MAINUI_POSITION_EXPERT
                                               : -1;
    const MainUICatalogPage *page = &ui->catalog->pages[0];
    if (position < 0 || ui->catalog->depth || !ui->handoff_dir || !page->entries ||
        ui->view.selected < 0 || ui->view.selected >= page->count) {
        return;
    }
    const char *identity = mainui_console_position_identity(&page->entries[ui->view.selected]);
    char *remembered = ui->console_position.console[position];
    ui->console_position_pending[position] = false;
    if (identity && strcmp(identity, remembered) && strlen(identity) < 4096) {
        strcpy(remembered, identity);
        mainui_console_position_save(&ui->console_position, ui->handoff_dir);
    }
}

void mainui_prepare_frame(MainUIApp *ui)
{
    Uint32 now = SDL_GetTicks();
    bool hidden_details = ui->details.media.open || *ui->message_title || ui->context_open ||
                          ui->confirmation >= 0 ||
                          (ui->device_enabled && ui->device_status.sleeping == 1);
    mainui_details_auto_hold(&ui->details, hidden_details, now);
    mainui_details_auto_step(&ui->details, now);
    /* Consoles listed keep a list's window, also after the grid's page was
     * saved or restored for them. */
    if (mainui_console_mode(ui) == MAINUI_CONSOLE_LIST) {
        mainui_viewport_refit(&ui->view, ui->view.total, ui->config.rows, ui->view.selected);
    }
    if (!ui->home && !ui->library && ui->catalog) {
        if (ui->catalog == ui->games) {
            ui->games_view = ui->view;
        }
        else if (ui->catalog == ui->expert) {
            ui->expert_view = ui->view;
        }
        remember_console(ui);
    }
    ui->list_labels = (MainUIListLabels){ui->catalog, ui->library, ui->list};
    int destination;
    if (mainui_letter_jump_step(&ui->letter_jump, mainui_list_label_at, &ui->list_labels,
                                &destination)) {
        mainui_viewport_move(&ui->view, ui->config.rows, destination - ui->view.selected, false);
        ui->selected_at = SDL_GetTicks();
    }
    /* Changes to the active selection produce one sample, independent of repaint
     * frequency and marquee timer ticks. Initial presentation is silent. */
    int section = 4;
    int selection = ui->view.selected;
    if (ui->name_input.open) {
        section = 7;
        selection = ui->name_input.selected;
    }
    else if (ui->context_open) {
        section = 5;
        selection = ui->context.selected;
    }
    else if (ui->language_open) {
        section = 6;
        selection = ui->languages.selected;
    }
    else if (ui->settings_open) {
        section = 2;
        selection = ui->settings.selected;
    }
    else if (ui->apps) {
        section = 1;
        selection = ui->apps_view.selected;
    }
    else if (ui->home) {
        section = 0;
        selection = ui->home_view.selected;
    }
    else if (ui->library) {
        section = 3;
    }
    if (ui->sound_section == section && ui->sound_selection != selection) {
        mainui_audio_change();
    }
    /* Own row identity: the index/label can survive deletion or replacement. */
    const char *key = "", *container = "", *label = "";
    if (section == 3 && selection >= 0 && selection < ui->library->visible_count) {
        label = mainui_library_label(ui->library, selection);
        int row = ui->library->visible[selection];
        container = ui->library->current >= 0 ? ui->library->folders[ui->library->current].id : "";
        /* A console Search's rows have no identity: their ROM tells two
         * games with one title apart, for their video previews too. */
        const MainUILibraryItem *item = row >= 0 ? &ui->library->items[row] : NULL;
        key = row == INT_MIN                      ? ".."
              : row < 0                           ? ui->library->folders[-row - 1].id
              : item->identity && *item->identity ? item->identity
                                                  : item->rom;
    }
    else if (section == 4 && ui->catalog) {
        container = ui->catalog->pages[ui->catalog->depth].path;
        MainUIEntry *entry =
            mainui_catalog_entry(ui->catalog, mainui_browser_index(ui->catalog, selection));
        key = entry ? entry->path : "..";
    }
    else if (section == 0 && selection >= 0 && selection < ui->menu.count) {
        key = mainui_menu_label(ui->menu.sections[selection]);
    }
    else if (section == 1) {
        MainUIEntry *entry = ui->apps ? mainui_catalog_entry(ui->apps, selection) : NULL;
        key = entry ? entry->path : "";
    }
    else if (section == 2 && selection >= 0 && selection < ui->settings.count) {
        key = mainui_stock_setting_label(ui->settings.rows[selection]);
    }
    else if (section == 5 && selection >= 0 && selection < ui->context.visible_count) {
        key = mainui_context_label(&ui->context.entries[ui->context.visible[selection]]);
    }
    else if (section == 6 && selection >= 0 && selection < ui->languages.count) {
        key = ui->languages.entries[selection].name;
    }
    else if (section == 4 && ui->list && selection >= 0 && selection < ui->list->count) {
        key = ui->list->labels[selection];
    }
    char identity[sizeof ui->selected_identity];
    snprintf(identity, sizeof identity, "%d:%d:%p:%p:%s:%s:%s", section, ui->confirmation,
             (void *)ui->catalog, (void *)ui->library, container ? container : "", key ? key : "",
             label ? label : "");
    bool selection_changed = strcmp(identity, ui->selected_identity) || ui->confirmation >= 0;
    if (selection_changed) {
        memcpy(ui->selected_identity, identity, strlen(identity) + 1);
        ui->selected_at = SDL_GetTicks();
    }
    ui->sound_section = section;
    ui->sound_selection = selection;
    /* No frame may be drawn while queued input is coalesced. Keep the
     * previously presented marquee state for that interval, but never
     * carry it across a changed selection. */
    ui->animate = (ui->presented_animating && !selection_changed) || ui->letter_jump.active;
    update_list_video(ui);
    SDL_Event queued_input;
    ui->draw_frame =
        ui->snapshot || ui->input_script ||
        (!ui->letter_jump.active &&
         SDL_PeepEvents(&queued_input, 1, SDL_PEEKEVENT, SDL_KEYDOWNMASK | SDL_KEYUPMASK) <= 0);
}

/* Idle ticks otherwise repaint the whole frame every 500 ms on the device (#9).
 * Skip only on static browsing screens, with nothing animating or in flight,
 * after a settle period, and while the header status is unchanged. A periodic
 * repaint bounds any staleness, e.g. if another process drew on the display. */
enum {
    IDLE_SETTLE_MS = 1000,
    IDLE_REPAINT_MS = 5000,
    /* A full repaint interrupts scrolling on the device; keep it rare there. */
    MARQUEE_REPAINT_MS = 30000
};

/* Nothing but time has changed since the last presented frame. The periodic
 * status refresh only affects the header, which is compared below. */
static bool idle_unchanged(const MainUIApp *ui, Uint32 now, Uint32 repaint_ms)
{
    bool status_only = !ui->device_job.thread ||
                       (ui->device_job.operation == 0 && !ui->device_job.queued_operation);
    return ui->idle_tick && !ui->snapshot && !ui->input_script && !ui->letter_jump.active &&
           !ui->catalog_job.thread && status_only && !ui->about_job.thread && !ui->preview.thread &&
           !ui->preview.pending && !ui->settings_open && !ui->settings_page.open &&
           !ui->language_open && !ui->details.open && !ui->name_input.open &&
           !ui->search_keyboard && !ui->context_open && ui->confirmation < 0 &&
           !*ui->message_title && !ui->launch_pending && now - ui->presented_at < repaint_ms &&
           ui->presented_battery == ui->theme.battery_percent &&
           ui->presented_charging == ui->theme.battery_charging &&
           ui->presented_wifi_online == ui->theme.wifi_online &&
           ui->presented_hotspot == ui->theme.hotspot_on &&
           ui->presented_wifi_signal == ui->theme.wifi_signal_level &&
           !strcmp(ui->presented_wifi_address, ui->theme.wifi_address);
}

bool mainui_load_deferred_icons(MainUIApp *ui)
{
    bool shown = false;
    while (ui->systems_drawn && !ui->catalog_job.thread && ui->catalog && !ui->catalog->depth &&
           !ui->library) {
        SDL_Event waiting;
        SDL_PumpEvents();
        if (SDL_PeepEvents(&waiting, 1, SDL_PEEKEVENT, SDL_ALLEVENTS) > 0 ||
            !mainui_menu_view_load_pending(&ui->menu_view, &ui->view, &shown) || shown) {
            break;
        }
    }
    return shown;
}

bool mainui_carousel_frame_deferred(MainUIApp *ui, Uint32 now)
{
    /* Skip composition, not input handling. Only pace a carousel already on
     * screen; entering or leaving a screen and opening a dialog stay immediate. */
    bool carousel = ui->real_device && !ui->snapshot && !ui->input_script && ui->systems_drawn &&
                    ui->menu_view.carousel_revealed && ui->catalog && !ui->catalog->depth &&
                    !ui->library && !ui->home && !ui->context_open && ui->confirmation < 0 &&
                    !*ui->message_title && mainui_console_mode(ui) == MAINUI_CONSOLE_CAROUSEL;
    if (!carousel || !ui->last_screen_present_at) {
        ui->carousel_frame_pending = false;
        return false;
    }
    if ((Sint32)(now - ui->last_screen_present_at) < MAINUI_MARQUEE_FRAME_MS) {
        ui->carousel_frame_pending = true;
        return true;
    }
    return false;
}

bool mainui_frame_current(const MainUIApp *ui, Uint32 now)
{
    if (ui->carousel_frame_pending) {
        return false;
    }
    /* A video and scrolling title are independent wake sources. Cap their
     * combined idle presentations to the marquee cadence (~25 fps); the
     * decoder keeps running, and the next tick uses its latest frame.
     * Do not defer input, status changes, gallery, or fullscreen playback. */
    if (ui->presented_animating && ui->list_video.video && !ui->list_video.fullscreen &&
        idle_unchanged(ui, now, MARQUEE_REPAINT_MS) &&
        (Sint32)(now - ui->last_screen_present_at) < MAINUI_MARQUEE_FRAME_MS - 5) {
        mainui_count_add("video-marquee-coalesced", 1);
        return true;
    }
    /* A video's new frame while a title scrolls on paced frames waits for
     * the next of them, which draws both: drawn at once, it came between
     * the title's frames and held the next one up, so the title stuttered. */
    if (ui->video_wake && ui->marquee_paced && ui->presented_animating &&
        idle_unchanged(ui, now, MARQUEE_REPAINT_MS)) {
        return true;
    }
    return idle_unchanged(ui, now, IDLE_REPAINT_MS) && !ui->presented_animating &&
           !ui->list_video.changed &&
           (ui->carousel_offscreen || now - ui->active_at >= IDLE_SETTLE_MS) &&
           !(mainui_menu_carousel_sliding(&ui->menu_view) &&
             mainui_console_mode(ui) == MAINUI_CONSOLE_CAROUSEL);
}

/* A list frame that can be recomposed in part: nothing but time changed,
 * and a video preview neither started nor ended (its frame changes the title
 * lanes, as a cover does). */
static bool list_partial(const MainUIApp *ui, Uint32 now)
{
    bool list = !ui->home && !ui->apps && (ui->library || ui->list || ui->catalog) &&
                (!(ui->catalog && !ui->library && !ui->catalog->depth) ||
                 mainui_console_mode(ui) == MAINUI_CONSOLE_LIST);
    /* Lists with the empty panel over their rows always take the full path. */
    bool empty = !ui->view.total ||
                 (ui->catalog && !ui->library && ui->catalog->depth > 1 && ui->view.total == 1) ||
                 (ui->library && ui->library->current >= 0 && ui->library->visible_count == 1 &&
                  ui->library->visible[0] == INT_MIN);
    /* A fullscreen video repaints the whole frame, as the gallery does. */
    return idle_unchanged(ui, now, MARQUEE_REPAINT_MS) && list && !empty &&
           !mainui_list_video_fullscreen(&ui->list_video) &&
           ui->presented_video == (ui->list_video.frame != NULL);
}

/* Scrolling titles otherwise repaint, rotate and flip the whole frame at 30 fps. */
bool mainui_frame_marquee_only(const MainUIApp *ui, Uint32 now)
{
    int row = ui->view.selected - ui->view.start;
    return list_partial(ui, now) && ui->presented_animating && row >= 0 && row < ui->config.rows &&
           ui->view.selected < ui->view.total && ui->config.row_height > 0 &&
           60 + (row + 1) * ui->config.row_height <= 420;
}

/* A video preview's new frame otherwise repaints the whole frame too. */
static bool cover_only(const MainUIApp *ui, Uint32 now)
{
    return list_partial(ui, now) && ui->list_video.frame && ui->list_video.changed;
}

/* Set only while the host check re-composes a frame for the same instant. */
static const Uint32 *pinned_elapsed;

static Uint32 list_elapsed(const MainUIApp *ui)
{
    return pinned_elapsed ? *pinned_elapsed
           : ui->snapshot ? ui->snapshot_elapsed
                          : SDL_GetTicks() - ui->selected_at;
}

/* Whether a cover, or a video in its place, is shown: titles stop at it. */
static bool cover_shown(const MainUIApp *ui)
{
    return ui->preview.image || (ui->list_video.frame && !ui->list_video.fullscreen);
}

static SDL_Rect intersect(SDL_Rect a, SDL_Rect b)
{
    int left = a.x > b.x ? a.x : b.x, top = a.y > b.y ? a.y : b.y;
    int right = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int bottom = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    return (SDL_Rect){(Sint16)left, (Sint16)top, (Uint16)(right > left ? right - left : 0),
                      (Uint16)(bottom > top ? bottom - top : 0)};
}

/* One list row, clipped to its own rectangle and to bound when given. Shared
 * by full and partial frames so both compose the row identically. */
static void draw_list_row(MainUIApp *ui, int i, Uint32 elapsed, const SDL_Rect *bound)
{
    int list_origin = 0;
    int list_width = 640;
    int outer = ui->config.rows <= 9 ? 20 : (ui->config.rows >= 14 ? 15 : 29 - ui->config.rows);
    int gap = ui->config.rows <= 7 ? 15 : (ui->config.rows >= 17 ? 5 : 22 - ui->config.rows);
    int y = 60 + i * ui->config.row_height;
    SDL_Rect clip = {(Sint16)list_origin, (Sint16)y, (Uint16)list_width,
                     (Uint16)ui->config.row_height};
    if (bound) {
        clip = intersect(clip, *bound);
    }
    SDL_SetClipRect(ui->screen, &clip);
    bool selected = ui->view.start + i == ui->view.selected;
    if (selected) {
        if (ui->theme.selection) {
            blit(ui->theme.selection, ui->screen, list_origin, y);
        }
        else {
            SDL_FillRect(ui->screen, &clip, SDL_MapRGB(ui->screen->format, 68, 68, 68));
        }
    }
    /* Consoles listed (console-view.json) show the folder icon. */
    bool folder_row =
        ui->library ? mainui_library_is_folder(ui->library, ui->view.start + i)
                    : ui->catalog && (mainui_browser_folder(ui->catalog, ui->view.start + i) ||
                                      mainui_console_mode(ui) == MAINUI_CONSOLE_LIST);
    SDL_Surface *row_icon = folder_row ? ui->theme.folder : ui->theme.icon;
    /* "list": {"folder_icon": false}: listed consoles' names start at the
     * margin, as rows without an icon do. */
    if (ui->console_view.hide_folder_icon && mainui_console_mode(ui) == MAINUI_CONSOLE_LIST) {
        row_icon = NULL;
    }
    /* Transparent ui->theme spacers still have a meaningful width.
    * Reserving a synthetic 71px slot indented Silky by 70px. */
    int icon_width = row_icon ? row_icon->w : 0;
    int icon_x = list_origin + (ui->theme.icon_margin >= 0 ? ui->theme.icon_margin : 5);
    int text_x = row_icon ? icon_x + icon_width + gap : list_origin + outer;
    int available = list_width - outer - text_x;
    if (ui->favorite_rows[i] && (!ui->library || ui->library->recent || ui->search.results) &&
        ui->theme.favorite && !folder_row) {
        bool spacer = ui->theme.icon && ui->theme.icon->w >= 120 &&
                      ui->theme.icon->w >= 3 * ui->theme.icon->h;
        int origin = spacer || (ui->config.dynamic_favorite_position && !cover_shown(ui)) ? 0 : 250;
        int marker_x = 640 - ui->theme.favorite->w - origin - 20;
        blit(ui->theme.favorite, ui->screen, marker_x,
             y + (ui->config.row_height - ui->theme.favorite->h) / 2);
        if (marker_x - 6 - text_x < available) {
            available = marker_x - 6 - text_x;
        }
    }
    if (available < 1) {
        available = 1;
    }
    /* Preview opacity belongs to the later compositor, not the text
    * clip. Only overflow activation uses the live ui->preview edge. */
    int activation_width = available;
    int pane_width = mainui_preview_edge(&ui->theme) - text_x;
    if (cover_shown(ui) && pane_width > 0 && pane_width < activation_width) {
        activation_width = pane_width;
    }
    /* A right-to-left title shows its right end, where it begins: with a
     * cover shown it stops at the pane, as a Favorite marker makes it. */
    if (ui->label_rtl[i]) {
        available = activation_width;
    }
    if (row_icon) {
        SDL_Rect source = {0, 0, (Uint16)icon_width, (Uint16)ui->config.row_height};
        if (source.w > row_icon->w) {
            source.w = (Uint16)row_icon->w;
        }
        if (source.h > row_icon->h) {
            source.h = (Uint16)row_icon->h;
        }
        source.y = (Sint16)((row_icon->h - source.h) / 2);
        SDL_Rect dest = {(Sint16)icon_x, (Sint16)(y + (ui->config.row_height - source.h) / 2), 0,
                         0};
        SDL_BlitSurface(row_icon, &source, ui->screen, &dest);
    }
    SDL_Surface *label = !selected && ui->highlighted[i] ? ui->highlighted[i] : ui->labels[i];
    if (!label) {
        return;
    }
    int label_y = y + (ui->config.row_height - label->h) / 2;
    int rtl_start = ui->label_rtl[i] && label->w > available ? label->w - available : 0;
    ui->animate =
        ui->animate || (selected && label->w > activation_width && ui->config.scroll_status == 2);
    if (selected && ui->config.scroll_status == 2 && elapsed >= (unsigned)ui->config.scroll_delay &&
        label->w > activation_width) {
        MainUIBlit segments[2];
        /* Keep scrolling tied to elapsed time while a list video plays.
         * Decoded-frame events may interrupt paced marquee wakeups, so
         * counting wakeups can make scrolling pause and then jump. */
        bool instant = ui->snapshot || ui->marquee_origin != ui->selected_at ||
                       (ui->list_video.frame && !ui->list_video.fullscreen);
        if (ui->config.scroll_bounce) {
            /* Travel until the end of the title meets the edge of the lane
             * (the cover's edge when one is shown), pause, and come back. A
             * right-to-left title starts at its right end and moves the
             * other way. Paced frames count whole frame periods, as below. */
            uint64_t moving = instant ? elapsed - (unsigned)ui->config.scroll_delay
                                      : (uint64_t)ui->marquee_steps * MAINUI_MARQUEE_FRAME_MS;
            int distance = label->w - activation_width;
            int position = mainui_marquee_bounce(moving, ui->config.scroll_speed, distance);
            SDL_Rect source = {(Sint16)(ui->label_rtl[i] ? distance - position : position), 0,
                               (Uint16)available, (Uint16)label->h};
            SDL_Rect dest = {(Sint16)text_x, (Sint16)label_y, 0, 0};
            SDL_BlitSurface(label, &source, ui->screen, &dest);
            return;
        }
        /* One whole-pixel step per paced frame, so a late frame never jumps twice.
         * Snapshots render a given instant. 1000 px/s maps pixels to ms exactly. */
        uint64_t offset =
            instant ? mainui_marquee_pixels(elapsed - (unsigned)ui->config.scroll_delay,
                                            ui->config.scroll_speed)
                    : mainui_marquee_step_pixels(ui->marquee_steps, ui->config.scroll_speed);
        /* A right-to-left title starts from its right end, where it begins,
         * and moves right: the same strip of title and gap, walked back. */
        if (ui->label_rtl[i]) {
            uint64_t cycle = (uint64_t)label->w + 60;
            offset = (rtl_start + cycle - offset % cycle) % cycle;
        }
        int n = mainui_marquee_stream(offset, 1000, label->w, available, segments);
        for (int part = 0; part < n; part++) {
            SDL_Rect source = {(Sint16)segments[part].source_x, 0, (Uint16)segments[part].width,
                               (Uint16)label->h};
            SDL_Rect dest = {(Sint16)(text_x + segments[part].destination_x), (Sint16)label_y, 0,
                             0};
            SDL_BlitSurface(label, &source, ui->screen, &dest);
        }
    }
    else {
        /* A right-to-left title too long for the row shows its right end. */
        SDL_Rect source = {(Sint16)rtl_start, 0, (Uint16)available, (Uint16)label->h};
        SDL_Rect dest = {(Sint16)text_x, (Sint16)label_y, 0, 0};
        SDL_BlitSurface(label, &source, ui->screen, &dest);
    }
}

/* Experimental Carousel-only VSync. SDL on Miyoo writes to a single visible
 * framebuffer, so SDL_Flip cannot synchronize the direct rotated copy. This
 * wait starts that copy at a refresh boundary; it is not double buffering.
 * Disable permanently if the driver rejects the request or stalls (>50 ms).
 * Note: O_NONBLOCK does not guarantee that this ioctl cannot block. */
#ifdef MAINUI_ONION
static void wait_for_carousel_vsync(void)
{
    static int fb = -2; /* -2: unopened; -1: unavailable/disabled. */
    if (fb == -2) {
        fb = open("/dev/fb0", O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fb < 0) {
            fprintf(stderr, "[carousel] VSync unavailable: open /dev/fb0: %s\n", strerror(errno));
            mainui_count_add("carousel-vsync-fail", 1);
            fb = -1;
        }
    }
    if (fb < 0) {
        return;
    }
    unsigned int crtc = 0;
    Uint32 before = SDL_GetTicks();
    struct timespec start = mainui_timing_start();
    if (ioctl(fb, FBIO_WAITFORVSYNC, &crtc) < 0) {
        fprintf(stderr, "[carousel] VSync unavailable: ioctl: %s\n", strerror(errno));
        mainui_count_add("carousel-vsync-fail", 1);
        close(fb);
        fb = -1;
        return;
    }
    mainui_count_add("carousel-vsync-waits", 1);
    mainui_timing_finish("carousel-vsync-ms", start);
    Uint32 elapsed = SDL_GetTicks() - before;
    if (elapsed > 50) {
        fprintf(stderr, "[carousel] VSync too slow (%u ms); disabling\n", elapsed);
        mainui_count_add("carousel-vsync-slow", 1);
        close(fb);
        fb = -1;
    }
}
#endif

static void present(MainUIApp *ui, const SDL_Rect *areas, int count);

void mainui_present_whole(MainUIApp *ui)
{
    present(ui, NULL, 0);
}

/* Rotate (on the device) and flip. Rectangles limit the rotated copy; the
 * logical frame stays upright and complete for later partial frames. */
static void present(MainUIApp *ui, const SDL_Rect *areas, int count)
{
    bool carousel = ui->systems_drawn && ui->catalog && !ui->catalog->depth && !ui->library &&
                    mainui_console_mode(ui) == MAINUI_CONSOLE_CAROUSEL;
    if (carousel && ui->real_device) {
        mainui_framebuffer_probe(ui->display);
    }
    struct timespec carousel_start = carousel ? mainui_timing_start() : (struct timespec){-1, 0};
    /* Spacing of consecutive animation frames, for the timing report. */
    static Uint32 previous, previous_carousel;
    static long peak_gap, peak_copy, peak_flip, peak_present;
    Uint32 now = SDL_GetTicks();
    /* Only frames of a slide or a moving title count: the gap after the
     * carousel stood still is no slow frame. */
    bool moving = mainui_menu_carousel_sliding(&ui->menu_view) || ui->animate;
    if (carousel && previous_carousel && moving) {
        long gap = (long)(now - previous_carousel);
        if (gap > peak_gap) {
            mainui_count("carousel-gap-max-ms", peak_gap = gap);
        }
        if (gap > 45) {
            mainui_count_add("carousel-gap-over45", 1);
        }
    }
    /* Carousel pacing happens before composition, not while presenting an
     * already-rendered frame. The framebuffer has no vertical sync. */
    if (ui->presented_animating && ui->animate) {
        Uint32 gap = now - previous;
        mainui_count_add(gap < 35    ? "gap-under35"
                         : gap <= 45 ? "gap-40"
                         : gap <= 65 ? "gap-50-60"
                                     : "gap-over65",
                         1);
    }
    previous = now;
    previous_carousel = carousel && moving ? now : 0;
    ui->last_screen_present_at = now;
    ui->carousel_frame_pending = false;
    ui->presented_animating = ui->animate;
    SDL_Rect shown[2] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
    count = areas && count > 0 ? (count > 2 ? 2 : count) : 0;
    for (int i = 0; i < count; i++) {
        shown[i] = areas[i];
    }
    /* Keep the VSync wait outside carousel-copy-us: presentation profiling
     * measures the CPU copy and SDL_Flip independently of synchronization. */
#ifdef MAINUI_ONION
    if (carousel && ui->console_view.vsync && ui->real_device && !ui->snapshot &&
        !ui->input_script && ui->device_status.sleeping != 1) {
        wait_for_carousel_vsync();
    }
#endif
    struct timespec copy_start = carousel ? mainui_timing_start() : (struct timespec){-1, 0};
    if (ui->real_device) {
        /* Copy inverted pixels directly to the display. */
        for (int i = 0; i < (count ? count : 1); i++) {
            if (count) {
                shown[i] = mainui_rotated_rect(ui->screen, areas[i]);
                SDL_SetClipRect(ui->display, &shown[i]);
            }
            mainui_blit_rotated(ui->screen, ui->display);
        }
        SDL_SetClipRect(ui->display, NULL);
    }
    if (carousel) {
        long us = mainui_timing_elapsed_us(copy_start);
        if (us >= 0) {
            mainui_count_add("carousel-copy-us", us);
            if (us > peak_copy) {
                mainui_count("carousel-copy-max-us", peak_copy = us);
            }
            if (us > 10000) {
                mainui_count_add("carousel-copy-over10", 1);
            }
        }
    }
    struct timespec flip_start = carousel ? mainui_timing_start() : (struct timespec){-1, 0};
    /* Without double buffering SDL_Flip is SDL_UpdateRect of the whole screen
     * (Onion's SDL: "use SDL_UpdateRect when flip"). A partial frame updates
     * just its rectangles: less copying and a far smaller window for tearing. */
    if (count && !(ui->display->flags & SDL_DOUBLEBUF)) {
        SDL_UpdateRects(ui->display, count, shown);
        if (carousel) {
            mainui_count_add("carousel-update-frames", 1);
        }
        mainui_count_add("frames", 1);
    }
    else if (SDL_Flip(ui->display) == 0) {
        if (carousel) {
            mainui_count_add("carousel-flip-frames", 1);
        }
        mainui_mark(MAINUI_MARK_FIRST_FRAME);
        mainui_count_add("frames", 1);
    }
    if (carousel) {
        long flip_us = mainui_timing_elapsed_us(flip_start);
        if (flip_us >= 0) {
            mainui_count_add("carousel-flip-us", flip_us);
            if (flip_us > peak_flip) {
                mainui_count("carousel-flip-max-us", peak_flip = flip_us);
            }
            if (flip_us > 10000) {
                mainui_count_add("carousel-flip-over10", 1);
            }
        }
        long present_us = mainui_timing_elapsed_us(carousel_start);
        if (present_us >= 0) {
            if (present_us > peak_present) {
                mainui_count("carousel-present-max-us", peak_present = present_us);
            }
            if (present_us > 16000) {
                mainui_count_add("carousel-present-over16", 1);
            }
            if (present_us > 25000) {
                mainui_count_add("carousel-present-over25", 1);
            }
        }
        mainui_timing_finish("carousel-present-ms", carousel_start);
    }
}

/* A list page could not be read. Each step runs once, in the background,
 * keeping the selected row and window: reload the list (it may have changed
 * outside MainUI); if the cache content itself is damaged, rebuild it as
 * Refresh roms does; then browse the console by scanning its folder for this
 * visit (the next entry reads the cache again). Search results get only the reload: nothing else reproduces them. A busy, I/O or memory failure never replaces the cache. Only if
 * the scan fails too, leave the list with a message: then the folder itself
 * cannot be read. True when it left. */
static bool recover_page(MainUIApp *ui, const MainUILaunchSource *source)
{
    MainUICatalog *catalog = ui->catalog;
    int step = ui->page_recovery + 1;
    /* Search's results come from its own database: after the reload, only
     * running Search again can repair them. */
    bool search = mainui_catalog_search_results(catalog);
    if (search && step > 1) {
        step = 4;
    }
    if (step == 2 && !mainui_catalog_page_damaged(catalog)) {
        step = 3;
    }
    /* An unfinished ROM deletion is never set aside automatically: the
     * rebuild only recovers it, and no scan bypasses it. Refresh roms, asked
     * for, may abandon it. */
    bool pending = mainui_catalog_deletion_pending(catalog);
    if (step == 3 && (pending || !mainui_catalog_scan_only(catalog->pages[1].path))) {
        step = 4;
    }
    ui->page_recovery = step;
    if (step <= 3) {
        fprintf(stderr, "Recovering an unreadable list page: %s\n",
                step == 1   ? "reloading"
                : step == 2 ? "rebuilding the cache"
                            : "scanning the folder");
        if (mainui_catalog_job_start(&ui->catalog_job, step == 2 ? JOB_REPAIR_SYSTEM : JOB_RELOAD,
                                     source, ui->sd, ui->config.case_sensitive, ui->config.rows,
                                     NULL, ++ui->catalog_generation)) {
            return false;
        }
    }
    if (step == 3) {
        mainui_catalog_cancel_scan_only(catalog->pages[1].path);
    }
    ui->page_recovery = 0;
    /* Remembered at the top, so reopening the list starts on a readable page. */
    mainui_viewport_restore(&ui->view, ui->view.total, ui->config.rows, 0, 0, ui->config.rows - 1);
    if (mainui_browser_back(catalog, &ui->view)) {
        ui->preview_sync_once = true;
        ui->menu_view.cached_start = -1;
    }
    ui->cached_start = -1;
    snprintf(ui->message_title, sizeof ui->message_title, "Catalog unavailable");
    snprintf(ui->message_body, sizeof ui->message_body, "%s",
             search    ? "Cannot read Search results. Run Search again."
             : pending ? "This list cannot be read while a ROM deletion is unfinished. Use "
                         "Refresh roms to rebuild it."
                       : "This list cannot be read.");
    return true;
}

static bool compose_full_frame(MainUIApp *ui)
{
    ui->systems_drawn = false;
    if (ui->catalog_job.thread) {
        /* Retain the previous frame until the catalog result is ready. */
    }
    else if (ui->details.media.open) {
        /* A popup entered from Game Info uses that screen as its backdrop. */
        if (mainui_media_over_details(&ui->details.media)) {
            mainui_details_progress(&ui->details, ui->snapshot ? MAINUI_PREVIEW_WAIT_FOREVER : 0);
            mainui_details_draw(&ui->details, &ui->theme, ui->screen);
        }
        mainui_media_draw(&ui->details.media, &ui->theme, ui->screen, ui->snapshot);
    }
    else if (mainui_list_video_fullscreen(&ui->list_video)) {
        /* A list's fullscreen video preview, fitted and centred on black,
         * over header and footer; the list under it is not drawn. */
        SDL_Surface *frame = ui->list_video.frame;
        SDL_FillRect(ui->screen, NULL, SDL_MapRGB(ui->screen->format, 0, 0, 0));
        mainui_blit(ui->screen, frame, (ui->screen->w - frame->w) / 2,
                    (ui->screen->h - frame->h) / 2);
    }
    else if (ui->details.open) {
        mainui_details_progress(&ui->details, ui->snapshot ? MAINUI_PREVIEW_WAIT_FOREVER : 0);
        mainui_details_draw(&ui->details, &ui->theme, ui->screen);
    }
    else if (ui->settings_page.open) {
        mainui_settings_page_draw(&ui->settings_page, ui->screen, &ui->theme);
    }
    else if (ui->language_open) {
        mainui_draw_languages(ui->screen, &ui->theme, &ui->languages);
    }
    else if (ui->settings_open) {
        mainui_draw_settings(ui->screen, &ui->theme, &ui->settings);
    }
    else if (ui->apps) {
        /* The shown apps that are Favorites, from the list already read. */
        bool favorite[4] = {false};
        for (int i = 0; i < 4 && ui->favorites && ui->favorites->count &&
                        ui->apps_view.start + i < ui->apps_view.total;
             i++) {
            cJSON *record = mainui_catalog_record(ui->apps, ui->apps_view.start + i);
            const cJSON *rom = cJSON_GetObjectItemCaseSensitive(record, "rompath");
            favorite[i] =
                cJSON_IsString(rom) && mainui_library_contains(ui->favorites, rom->valuestring);
            cJSON_Delete(record);
        }
        mainui_draw_apps(ui->screen, &ui->theme, ui->apps, &ui->apps_view, favorite);
    }
    else if (ui->home) {
        mainui_menu_draw_home(&ui->menu_view, ui->screen, &ui->menu, &ui->home_view);
    }
    else if (ui->catalog && !ui->catalog->depth && !ui->library &&
             mainui_console_mode(ui) != MAINUI_CONSOLE_LIST) {
        if (mainui_console_mode(ui) == MAINUI_CONSOLE_CAROUSEL) {
            mainui_menu_draw_carousel(&ui->menu_view, ui->screen, ui->catalog, &ui->view,
                                      &ui->console_view, SDL_GetTicks(),
                                      !ui->snapshot          ? -1
                                      : ui->snapshot_elapsed ? (int)ui->snapshot_elapsed
                                                             : INT_MAX);
        }
        else {
            mainui_menu_draw_systems(&ui->menu_view, ui->screen, ui->catalog, &ui->view);
        }
        ui->systems_drawn = true;
    }
    else {
        /* Keep rendered row labels until the visible window changes. Marquee
 * frames reuse these surfaces rather than rasterizing every 40 ms frame.
 */
        /* The console page as a list (console-view.json) has labels of its
         * own: never the ones of the list it was left for, or back from. */
        bool consoles = mainui_console_mode(ui) == MAINUI_CONSOLE_LIST;
        if (consoles != ui->labels_consoles) {
            ui->cached_start = -1;
            ui->labels_consoles = consoles;
        }
        if (ui->cached_start != ui->view.start) {
            for (int i = 0; i < 20; i++) {
                if (ui->labels[i]) {
                    SDL_FreeSurface(ui->labels[i]);
                }
                ui->labels[i] = NULL;
                if (ui->highlighted[i]) {
                    SDL_FreeSurface(ui->highlighted[i]);
                }
                ui->highlighted[i] = NULL;
            }
            bool page_failed = false;
            for (int i = 0; i < ui->config.rows && ui->view.start + i < ui->view.total; i++) {
                const char *label = ui->library
                                        ? mainui_library_label(ui->library, ui->view.start + i)
                                    : ui->catalog ? NULL
                                                  : ui->list->labels[ui->view.start + i];
                if (ui->catalog && !ui->library) {
                    label = mainui_browser_label(ui->catalog, ui->view.start + i);
                    if (!label) {
                        fprintf(stderr, "%s\n", ui->catalog->error);
                        MainUILaunchSource source = {.section = ui->catalog == ui->expert
                                                                    ? MAINUI_MENU_EXPERT
                                                                    : MAINUI_MENU_GAMES,
                                                     .catalog = ui->catalog,
                                                     .view = &ui->view,
                                                     .home = &ui->home_view};
                        page_failed = true;
                        /* A job already running (a search or discovery) keeps its
                         * slot; recovery continues on a later frame. */
                        if (!ui->catalog_job.thread && recover_page(ui, &source)) {
                            return false; /* drawn as the console grid next */
                        }
                        break;
                    }
                }
                /* Listed consoles' names without the spaces some carry to
                 * sort differently; they keep their place. */
                char trimmed[1024];
                if (consoles) {
                    while (*label == ' ') {
                        label++;
                    }
                    int length = (int)strlen(label);
                    while (length && label[length - 1] == ' ') {
                        length--;
                    }
                    snprintf(trimmed, sizeof trimmed, "%.*s", length, label);
                    label = trimmed;
                }
                ui->favorite_rows[i] = false;
                if (ui->library && !mainui_library_is_folder(ui->library, ui->view.start + i)) {
                    ui->favorite_rows[i] = mainui_library_contains(
                        ui->favorites,
                        ui->library->items[ui->library->visible[ui->view.start + i]].rom);
                }
                else if (ui->catalog && !ui->library && ui->catalog->depth && ui->favorites &&
                         ui->favorites->count) {
                    cJSON *record = mainui_catalog_record(
                        ui->catalog, mainui_browser_index(ui->catalog, ui->view.start + i));
                    const cJSON *rom = cJSON_GetObjectItemCaseSensitive(record, "rompath");
                    ui->favorite_rows[i] = cJSON_IsString(rom) &&
                                           mainui_library_contains(ui->favorites, rom->valuestring);
                    cJSON_Delete(record);
                }
                char *marked = NULL;
                if (mainui_favorite_is_cut(&ui->favorite_editor, ui->library, ui->view.start + i)) {
                    size_t size = strlen(label) + 3;
                    marked = malloc(size);
                    if (marked) {
                        snprintf(marked, size, "> %s", label);
                    }
                }
                ui->labels[i] = TTF_RenderUTF8_Blended(ui->theme.font, marked ? marked : label,
                                                       ui->theme.color);
                ui->label_rtl[i] = mainui_text_rtl(label);
                if (ui->search.results) {
                    ui->highlighted[i] = mainui_search_label(&ui->theme, label, ui->search.query);
                }
                free(marked);
            }
            if (!page_failed) {
                ui->page_recovery = 0;
            }
            ui->cached_start = ui->view.start;
        }
        if (!ui->running) {
            return false;
        }
        if (ui->catalog_job.thread) {
            return false;
        }
        SDL_FillRect(ui->screen, NULL, SDL_MapRGB(ui->screen->format, 24, 24, 24));
        blit(ui->theme.background, ui->screen, 0, 0);
        if (consoles) {
            /* The grid's title: Games or Expert. */
            mainui_draw_header(ui->screen, &ui->theme,
                               mainui_menu_label(ui->catalog == ui->expert ? MAINUI_MENU_EXPERT
                                                                           : MAINUI_MENU_GAMES));
        }
        else {
            mainui_draw_list_header_image(ui->screen, &ui->theme, ui->heading);
        }
        /* A console's thumbnail is its cover, from console-view.json's folder,
         * unless the list's "thumbs" is false: then consoles have none. */
        ui->preview.thumbs = ui->console_view.hide_list_thumbs ? "" : ui->console_view.thumbs;
        ui->preview.theme_fallback = ui->theme.fallback;
        /* The first frame of a list waits briefly for its cover so it does not
         * pop in, but one slow image can never stall opening the list. */
        mainui_preview_request_within(&ui->preview, ui->catalog, ui->library,
                                      ui->library || !ui->catalog
                                          ? ui->view.selected
                                          : mainui_browser_index(ui->catalog, ui->view.selected),
                                      ui->snapshot            ? MAINUI_PREVIEW_WAIT_FOREVER
                                      : ui->preview_sync_once ? 80
                                                              : 0);
        ui->preview_sync_once = false;
        Uint32 elapsed = list_elapsed(ui);
        for (int i = 0; i < ui->config.rows && ui->view.start + i < ui->view.total; i++) {
            draw_list_row(ui, i, elapsed, NULL);
        }
        /* Row clips must never leak into the next frame's header/background. */
        SDL_SetClipRect(ui->screen, NULL);
        mainui_preview_draw_image(&ui->preview, &ui->theme, ui->screen,
                                  ui->list_video.fullscreen ? NULL : ui->list_video.frame);
        if (!ui->view.total ||
            (ui->catalog && !ui->library && ui->catalog->depth > 1 && ui->view.total == 1) ||
            (ui->library && ui->library->current >= 0 && ui->library->visible_count == 1 &&
             ui->library->visible[0] == INT_MIN)) {
            mainui_draw_empty(ui->screen, &ui->theme);
        }
        draw_list_counter(ui);
    }
    /* Stock's popup, message and dialog windows darken the screen above the
     * footer before drawing, once however many are open. A frame kept while a
     * catalog job runs is already darkened, so it is not darkened again. */
    if (!ui->catalog_job.thread && ((ui->context_open && ui->context.visible_count) ||
                                    ui->confirmation >= 0 || *ui->message_title)) {
        mainui_dim_popup_background(ui->screen, &ui->theme);
    }
    if (ui->context_open) {
        mainui_draw_context(ui->screen, &ui->theme, &ui->context, ui->config.context_ellipsis);
    }
    if (ui->confirmation >= 0) {
        mainui_draw_confirmation(ui->screen, &ui->theme,
                                 ui->confirmation == CONTEXT_CLEAR_RECENT      ? 77
                                 : ui->confirmation == CONTEXT_DELETE_ROM      ? 116
                                 : ui->confirmation == CONTEXT_FAVORITE_DELETE ? 404
                                                                               : 86,
                                 ui->confirmation == CONTEXT_CLEAR_RECENT      ? 78
                                 : ui->confirmation == CONTEXT_DELETE_ROM      ? 117
                                 : ui->confirmation == CONTEXT_FAVORITE_DELETE ? 81
                                                                               : 108);
    }
    if (ui->name_input.open) {
        mainui_name_input_draw(&ui->name_input, ui->screen, &ui->theme);
    }
    if (*ui->message_title) {
        /* Log each message once as it appears: many are only shown. */
        char shown[sizeof ui->logged_message];
        snprintf(shown, sizeof shown, "%s: %s", ui->message_title, ui->message_body);
        if (strcmp(shown, ui->logged_message)) {
            fprintf(stderr, "[message] %s\n", shown);
            strcpy(ui->logged_message, shown);
        }
        mainui_draw_message(ui->screen, &ui->theme, ui->message_title, ui->message_body);
    }
    else {
        ui->logged_message[0] = 0;
    }
    if (ui->catalog_job.thread && !atomic_load(&ui->catalog_job.done) &&
        (SDL_GetTicks() - ui->catalog_job.started_at >= 500 ||
         atomic_load(&ui->catalog_job.cancel))) {
        mainui_draw_catalog_loading(ui->screen, &ui->theme, atomic_load(&ui->catalog_job.cancel));
    }
    if (ui->snapshot && !ui->catalog_job.thread && !ui->device_job.thread &&
        !(ui->about_visible && ui->about_job.thread) &&
        (ui->launch_pending || (!ui->letter_jump.active && !ui->script_release &&
                                (!ui->input_script || !*ui->input_script)))) {
        if (SDL_SaveBMP(ui->screen, ui->snapshot) < 0) {
            fprintf(stderr, "Snapshot: %s\n", SDL_GetError());
            ui->status = 4;
        }
        ui->running = false;
        return false;
    }
    return true;
}

bool mainui_carousel_hold_frame(const MainUIApp *ui)
{
    /* The worker reveals the centre and adjacent covers as one group. Until
     * that group is ready, leave the previously presented framebuffer
     * untouched rather than showing an empty carousel first, but only while
     * nothing is open over it: a menu, confirmation, keyboard or message
     * opened meanwhile takes input, so it shows at once, over the carousel's
     * frame without its pictures. */
    return ui->systems_drawn && ui->catalog && !ui->catalog->depth && !ui->library &&
           mainui_console_mode(ui) == MAINUI_CONSOLE_CAROUSEL && ui->view.total > 0 &&
           !ui->menu_view.carousel_revealed && !ui->snapshot && !ui->input_script &&
           !ui->context_open && ui->confirmation < 0 && !ui->name_input.open && !*ui->message_title;
}

static bool draw_full_frame(MainUIApp *ui)
{
    if (!compose_full_frame(ui)) {
        return false;
    }
    if (mainui_carousel_hold_frame(ui)) {
        return true;
    }
    ui->presented_at = SDL_GetTicks();
    ui->presented_battery = ui->theme.battery_percent;
    ui->presented_charging = ui->theme.battery_charging;
    ui->presented_wifi_online = ui->theme.wifi_online;
    ui->presented_hotspot = ui->theme.hotspot_on;
    ui->presented_wifi_signal = ui->theme.wifi_signal_level;
    snprintf(ui->presented_wifi_address, sizeof ui->presented_wifi_address, "%s",
             ui->theme.wifi_address);
    ui->presented_video = ui->list_video.frame != NULL;
    ui->list_video.changed = false;
    present(ui, NULL, 0);
    return true;
}

/* Recompose only the selected row of a scrolling title, the cover area of a
 * playing video preview, or both, each bottom-up as a full frame layers it:
 * fill, background, the rows in it, cover. The header (y < 60) and footer
 * (y >= 420) keep their own clips, so nothing else is drawn in the list band
 * on these screens. */
static void draw_partial(MainUIApp *ui, bool row_area, bool cover_area)
{
    SDL_Rect areas[2];
    int count = 0;
    /* A video's next frame alone: its frames are opaque, all of one size,
     * and nothing is drawn over them, so the new frame over the last one is
     * the whole change, without the background and rows under it. */
    SDL_Surface *frame = ui->list_video.frame;
    int edge = mainui_preview_edge(&ui->theme);
    bool alone = false;
    if (cover_area && !row_area && frame && !frame->format->Amask &&
        !(frame->flags & (SDL_SRCALPHA | SDL_SRCCOLORKEY))) {
        SDL_Rect place = {(Sint16)(edge + (250 - frame->w) / 2),
                          (Sint16)(60 + (360 - frame->h) / 2), (Uint16)frame->w, (Uint16)frame->h};
        SDL_Rect target = place;
        if (place.x >= 0 && place.y >= 0 && place.x + place.w <= ui->screen->w &&
            place.y + place.h <= ui->screen->h) {
            SDL_SetClipRect(ui->screen, NULL);
            if (SDL_BlitSurface(frame, NULL, ui->screen, &target) == 0) {
                areas[count++] = place;
                cover_area = false;
                alone = true;
                /* No row was drawn: whether a title moves is as it was. */
                ui->animate = ui->presented_animating;
            }
        }
    }
    if (row_area) {
        int row = ui->view.selected - ui->view.start;
        /* If the video also changed, the cover pass recomposes everything
         * to the right of edge, including this row. Do not draw and rotate
         * those pixels a second time as part of the row pass. */
        int width = cover_area ? (edge < 0 ? 0 : edge > 640 ? 640 : edge) : 640;
        if (width > 0) {
            areas[count++] = (SDL_Rect){0, (Sint16)(60 + row * ui->config.row_height),
                                        (Uint16)width, (Uint16)ui->config.row_height};
        }
    }
    if (cover_area) {
        edge = edge < 0 ? 0 : edge > 640 ? 640 : edge;
        if (edge < 640) {
            areas[count++] = (SDL_Rect){(Sint16)edge, 60, (Uint16)(640 - edge), 360};
        }
    }
    Uint32 elapsed = list_elapsed(ui);
    for (int a = alone ? count : 0; a < count; a++) {
        SDL_Rect *area = &areas[a];
        SDL_SetClipRect(ui->screen, area);
        SDL_FillRect(ui->screen, area, SDL_MapRGB(ui->screen->format, 24, 24, 24));
        blit(ui->theme.background, ui->screen, 0, 0);
        for (int i = 0; i < ui->config.rows && ui->view.start + i < ui->view.total; i++) {
            int y = 60 + i * ui->config.row_height;
            if (y < area->y + area->h && y + ui->config.row_height > area->y) {
                draw_list_row(ui, i, elapsed, area);
            }
        }
        SDL_SetClipRect(ui->screen, area);
        mainui_preview_draw_image(&ui->preview, &ui->theme, ui->screen, ui->list_video.frame);
    }
    SDL_SetClipRect(ui->screen, NULL);
#ifndef MAINUI_ONION
    /* Host check: a partial frame must equal a full frame, pixel for pixel. */
    if (getenv("MAINUI_VERIFY_PARTIAL")) {
        size_t size = (size_t)ui->screen->pitch * (size_t)ui->screen->h;
        void *partial = malloc(size);
        bool animate = ui->animate;
        if (partial) {
            memcpy(partial, ui->screen->pixels, size);
            ui->animate = ui->letter_jump.active;
            pinned_elapsed = &elapsed;
            bool composed = compose_full_frame(ui);
            pinned_elapsed = NULL;
            if (composed && memcmp(partial, ui->screen->pixels, size)) {
                fprintf(stderr, "MAINUI_VERIFY_PARTIAL: partial frame differs from full frame\n");
                abort();
            }
            mainui_count_add("verified-partial", 1);
            static bool reported, reported_alone;
            if (cover_area && !reported) {
                fprintf(stderr, "MAINUI_VERIFY_PARTIAL: a video preview frame matches\n");
                reported = true;
            }
            if (alone && !reported_alone) {
                fprintf(stderr, "MAINUI_VERIFY_PARTIAL: a video frame drawn alone matches\n");
                reported_alone = true;
            }
            free(partial);
        }
        ui->animate = animate;
    }
#endif
    ui->list_video.changed = false;
    present(ui, areas, count);
}

bool mainui_draw_frame(MainUIApp *ui)
{
    Uint32 now = SDL_GetTicks();
    bool row = mainui_frame_marquee_only(ui, now), cover = cover_only(ui, now);
    if (row || cover) {
        /* A cover-only redraw does not recompute whether the title moves. */
        ui->animate = ui->presented_animating || ui->letter_jump.active;
        draw_partial(ui, row, cover);
        return true;
    }
    /* Only an actual full composition recomputes marquee animation. An
     * unsuccessful one must preserve the previous state for event pacing. */
    bool previous = ui->animate;
    ui->animate = ui->letter_jump.active;
    if (!draw_full_frame(ui)) {
        ui->animate = previous;
        return false;
    }
    return true;
}
