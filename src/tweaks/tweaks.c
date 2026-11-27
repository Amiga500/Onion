#include <dirent.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "components/list.h"
#include "system/battery.h"
#include "system/device_model.h"
#include "system/display.h"
#include "system/keymap_sw.h"
#include "system/lang.h"
#include "system/settings.h"
#include "theme/background.h"
#include "theme/sound.h"
#include "theme/theme.h"
#include "utils/file.h"
#include "utils/keystate.h"
#include "utils/log.h"
#include "utils/sdl_init.h"

#include "./appstate.h"
#include "./info_dialog.h"
#include "./menus.h"

#define FRAMES_PER_SECOND 60

bool check_menuHasChanges(List *menu)
{
    bool modified = false;
    if (menu->_created) {
        for (int i = 0; i < menu->item_count; i++) {
            ListItem *item = &menu->items[i];
            if (item->item_type != ACTION) {
                modified |= item->value != item->_reset_value;
            }
        }
    }
    return modified;
}

void check_networkChanged(void)
{
    bool modified = temp_flag_get("network_changed");
    modified |= check_menuHasChanges(&_menu_network);
    modified |= check_menuHasChanges(&_menu_telnet);
    modified |= check_menuHasChanges(&_menu_ftp);
    modified |= check_menuHasChanges(&_menu_http);
    modified |= check_menuHasChanges(&_menu_ssh);
    modified |= check_menuHasChanges(&_menu_date_time);
    temp_flag_set("network_changed", modified);
}

// The scrolling row's pixels from the picture to the screen, copied in
// memory. On the Mini+ the screen's memory is the panel's, turned 180
// degrees: the console's SDL turns the picture when it copies it there.
// Its hardware copy of the row's rectangle alone put it 6 px off (a row at
// y 212, 50 high, went to 480 - 212 - 50 = 218 turned): a line of the
// selection stayed above it (Amiga500's video). The whole picture is right but
// slow (33 frames a second against 50); a plain copy of the row came out
// upside down.
//
// So the first time, the whole picture is copied the usual way and 64
// points of the screen's memory compared with it, straight and turned:
// the way that matches is used for the rows (each pixel (x, y) to
// (x, y), or to (w-1-x, h-1-y)). Neither for sure (another panel, other
// formats or sizes): always the whole picture, as before.
enum { PUTROW_UNKNOWN = -1, PUTROW_STRAIGHT, PUTROW_TURNED, PUTROW_WHOLE };
static int putrow_mode = PUTROW_UNKNOWN;

static bool tweaks_lockBoth(void)
{
    if (SDL_LockSurface(screen) != 0)
        return false;
    if (SDL_LockSurface(video) != 0) {
        SDL_UnlockSurface(screen);
        return false;
    }
    return true;
}

static void tweaks_unlockBoth(void)
{
    SDL_UnlockSurface(video);
    SDL_UnlockSurface(screen);
}

static Uint32 tweaks_pixel(SDL_Surface *s, int x, int y)
{
    return ((Uint32 *)((Uint8 *)s->pixels + y * s->pitch))[x] & 0xFFFFFF;
}

static int tweaks_putRowMode(void)
{
    if (screen->format->BytesPerPixel != 4 || video->format->BytesPerPixel != 4 ||
        screen->format->Rmask != video->format->Rmask ||
        screen->format->Gmask != video->format->Gmask ||
        screen->format->Bmask != video->format->Bmask ||
        video->w != screen->w || video->h != screen->h)
        return PUTROW_WHOLE;
    SDL_BlitSurface(screen, NULL, video, NULL);
    if (!tweaks_lockBoth())
        return PUTROW_WHOLE;
    int straight = 0, turned = 0, n = 0;
    const int w = screen->w, h = screen->h;
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            int x = w * (2 * i + 1) / 16 + i, y = h * (2 * j + 1) / 16 + 3 * j;
            Uint32 p = tweaks_pixel(screen, x, y);
            straight += tweaks_pixel(video, x, y) == p;
            turned += tweaks_pixel(video, w - 1 - x, h - 1 - y) == p;
            n++;
        }
    tweaks_unlockBoth();
    if (straight == n && turned < n)
        return PUTROW_STRAIGHT;
    if (turned == n && straight < n)
        return PUTROW_TURNED;
    return PUTROW_WHOLE;
}

static void tweaks_putRow(SDL_Rect *row)
{
    SDL_Rect r = *row;
    if (putrow_mode == PUTROW_UNKNOWN) {
        putrow_mode = tweaks_putRowMode();
        return; // this frame: the whole picture, just copied
    }
    if (putrow_mode == PUTROW_WHOLE || r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0 ||
        r.x + r.w > screen->w || r.y + r.h > screen->h || !tweaks_lockBoth()) {
        SDL_BlitSurface(screen, NULL, video, NULL);
        return;
    }
    for (int y = r.y; y < r.y + r.h; y++) {
        const Uint32 *src = (const Uint32 *)((Uint8 *)screen->pixels + y * screen->pitch);
        if (putrow_mode == PUTROW_TURNED) {
            Uint32 *dst = (Uint32 *)((Uint8 *)video->pixels + (video->h - 1 - y) * video->pitch);
            for (int x = r.x; x < r.x + r.w; x++)
                dst[video->w - 1 - x] = src[x];
        }
        else
            memcpy((Uint32 *)((Uint8 *)video->pixels + y * video->pitch) + r.x, src + r.x, r.w * 4);
    }
    tweaks_unlockBoth();
}

int main(int argc, char *argv[])
{
    log_setName("tweaks");
    print_debug("Debug logging enabled");

    getDeviceModel();

    char apply_tool[STR_MAX] = "";
    bool use_display = true;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--apply_tool") == 0)
            strncpy(apply_tool, argv[++i], STR_MAX - 1);
        else if (strcmp(argv[i], "--no_display") == 0)
            use_display = false;
    }

    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    if (use_display || strlen(apply_tool) == 0)
        SDL_InitDefault();

    settings_load();

    lang_load();

    // Apply tool via command line
    if (strlen(apply_tool) > 0) {
        for (int i = 0; i < NUM_TOOLS; i++) {
            if (strncmp(tools_short_names[i], apply_tool, STR_MAX - 1) == 0) {
                printf("Tool '%s':\n", tools_short_names[i]);
                (*tools_pt[i])(NULL);
                break;
            }
        }
        return 0;
    }

    int battery_percentage = battery_getPercentage();
    int footer_info_shown = -1; // the SELECT hint as last drawn (-1: never)

    menu_main();

    uint32_t acc_ticks = 0, last_ticks = SDL_GetTicks(),
             time_step = 1000 / FRAMES_PER_SECOND;

    bool menu_combo_pressed = false;
    bool key_changed = false;
    SDLKey changed_key = SDLK_UNKNOWN;

    bool show_help_tooltip = !config_flag_get(".tweaksHelpCompleted");
    theme_listCanScroll = true; // redrawn below while a row scrolls

    // Measure (config/.tweaksFps): while a row scrolls, one line a second in
    // /tmp/tweaks_fps.log, the frames drawn (only the row, or everything).
    const bool fps_log = config_flag_get(".tweaksFps");
    uint32_t fps_at = SDL_GetTicks();
    int fps_row = 0, fps_full = 0;

    while (!quit) {
        uint32_t ticks = SDL_GetTicks();
        acc_ticks += ticks - last_ticks;
        last_ticks = ticks;

        if (updateKeystate(keystate, &quit, keys_enabled, &changed_key)) {
            if (keystate[SW_BTN_MENU] >= PRESSED && changed_key != SW_BTN_MENU)
                menu_combo_pressed = true;

            if (keystate[SW_BTN_UP] >= PRESSED) {
                key_changed = list_keyUp(menu_stack[menu_level], keystate[SW_BTN_UP] == REPEATING);
            }
            else if (keystate[SW_BTN_DOWN] >= PRESSED) {
                key_changed = list_keyDown(menu_stack[menu_level], keystate[SW_BTN_DOWN] == REPEATING);
            }
            else if (keystate[SW_BTN_LEFT] >= PRESSED) {
                key_changed = list_keyLeft(menu_stack[menu_level], keystate[SW_BTN_LEFT] == REPEATING);
            }
            else if (keystate[SW_BTN_RIGHT] >= PRESSED) {
                key_changed = list_keyRight(menu_stack[menu_level], keystate[SW_BTN_RIGHT] == REPEATING);
            }
            else if (keystate[SW_BTN_Y] == PRESSED) {
                key_changed = list_resetCurrentItem(menu_stack[menu_level]);
            }
            else if (keystate[SW_BTN_SELECT] == PRESSED) {
                // Always, even when the note box under the list shows the
                // note whole: the window has it in larger text.
                if (list_hasInfoNote(menu_stack[menu_level])) {
                    sound_change();
                    showInfoDialog(menu_stack[menu_level]);
                }
            }
            else if (keystate[SW_BTN_A] == PRESSED) {
                // An empty menu has no current item ("Edit app icon" for an
                // app no icon pack has an icon for, "Edit expert icon" with
                // no Expert system): A crashed Tweaks there
                ListItem *current = list_currentItem(menu_stack[menu_level]);
                bool has_action = current != NULL && current->action != NULL;
                if (has_action) {
                    sound_change();
                    skip_next_change = true;
                }
                key_changed = list_activateItem(menu_stack[menu_level]) || header_changed;
                // The action may have read the release itself (a dialog). A
                // plain RELEASED here turned every key repeat of a held A
                // into a new press, running the action again and again.
                if (has_action)
                    keystate[SW_BTN_A] = keystate_resync(SW_BTN_A);
            }
            else if (changed_key == SW_BTN_MENU && keystate[SW_BTN_MENU] == RELEASED) {
                if (!menu_combo_pressed)
                    quit = true;
                menu_combo_pressed = false;
            }
            else if (keystate[SW_BTN_B] == PRESSED) {
                if (menu_level == 0)
                    quit = true;
                else {
                    if (menu_stack[menu_level] == &_menu_game_lists_layout)
                        gamelists_on_menu_exit();
                    menu_stack[menu_level] = NULL;
                    menu_level--;
                    header_changed = true;
                    key_changed = true;
                }
            }

            if (!skip_next_change) {
                if (key_changed)
                    sound_change();
            }
            else
                skip_next_change = false;

            list_changed = list_changed || key_changed;
            key_changed = false;
        }

        // The accent colour chosen, once its page is left (actions.h).
        // Before all_changed is read below: applied after it, the redraw
        // it asks for was dropped (robcodedev, 4.5.1 beta 6).
        accent_applyPending(false);

        if (reset_menus)
            menu_resetAll();

        if (all_changed) {
            header_changed = true;
            list_changed = true;
            footer_changed = true;
            battery_changed = true;
        }

        if (quit)
            break;

        if (show_help_tooltip) {
            show_help_tooltip = false;
            config_flag_set(".tweaksHelpCompleted", true);
            __showInfoDialog("Welcome to Tweaks!",
                             ":: TOOLTIPS ::\n"
                             " \n"
                             "The selected option is described\n"
                             "at the bottom. Press SELECT to\n"
                             "read it in larger text.\n"
                             " \n"
                             "Press any button to close");
        }

        if (battery_hasChanged(ticks, &battery_percentage))
            battery_changed = true;

        blf_changing = exists("/tmp/blue_light_script.lock");

        if (acc_ticks >= time_step) {
            // A long value or label on the selected row scrolls: redrawn
            // every frame while it does (2-3 px each, list.h), none
            // otherwise. Every 33 ms it was slow or jerky (robcodedev).
            // When nothing else changed, only that row is drawn and only its
            // lines are put on the screen (below): the whole list at every
            // frame was too much work for 60 a second (robcodedev).
            bool scroll_frame = theme_listValueScrolling;
            if (isMenu(&_menu_date_time) || isMenu(&_menu_user_blue_light)) {
                if (isMenu(&_menu_date_time)) {
                    if (_writeDateString(_menu_date_time.items[0].label)) {
                        list_changed = true;
                    }
                }
                if (IS_MIYOO_PLUS_OR_FLIP()) {
                    if (isMenu(&_menu_user_blue_light)) {
                        if (_writeDateString(_menu_user_blue_light.items[0].label)) {
                            list_changed = true;
                        }
                    }
                }
            }
            if (isMenu(&_menu_network) || isMenu(&_menu_wifi)) {
                network_loadState();
                if (netinfo_getIpAddress(ip_address_label, network_state.hotspot ? "wlan1" : "wlan0")) {
                    if (_menu_network._created)
                        strcpy(_menu_network.items[0].label, ip_address_label);
                    if (_menu_wifi._created)
                        strcpy(_menu_wifi.items[0].label, ip_address_label);
                    list_changed = true;
                }
            }

            if (scroll_frame && !list_changed && !header_changed && !footer_changed && !battery_changed) {
                SDL_Rect row;
                if (theme_renderListActiveRow(screen, menu_stack[menu_level], &row)) {
                    tweaks_putRow(&row);
                    SDL_Flip(video);
                    fps_row++;
                }
                else
                    list_changed = true;
            }
            else if (scroll_frame)
                list_changed = true;
            if (scroll_frame && list_changed)
                fps_full++;
            if (fps_log && ticks - fps_at >= 1000) {
                if (fps_row + fps_full > 0) {
                    FILE *fp = fopen("/tmp/tweaks_fps.log", "a");
                    if (fp) {
                        fprintf(fp, "%d frames/s while scrolling: %d the row only, %d all\n",
                                (fps_row + fps_full) * 1000 / (int)(ticks - fps_at), fps_row, fps_full);
                        fclose(fp);
                    }
                }
                fps_at = ticks;
                fps_row = fps_full = 0;
            }

            if (header_changed || battery_changed) {
                /* Test branch (unified look): the path to this page. */
                const char *path[5];
                int levels = 0;
                for (int i = 0; i <= menu_level && levels < 5; i++)
                    if (menu_stack[i] != NULL && menu_stack[i]->title[0])
                        path[levels++] = menu_stack[i]->title;
                theme_renderHeaderPath(screen, path, levels);
            }

            if (list_changed)
                theme_renderList(screen, menu_stack[menu_level]);

            // SELECT opens the selected item's info box (the first-run
            // tooltip says so once): its hint is in the footer while the
            // item has one, so the footer is drawn again when that changes.
            // The hint only when there is more to read than the note box
            // under the list shows (a note cut short with "...", or no
            // box); SELECT opens the window either way, in larger text.
            int info_now = list_hasInfoNote(menu_stack[menu_level]) && !theme_listNoteShownWhole() ? 1 : 0;
            if (info_now != footer_info_shown)
                footer_changed = true;

            if (footer_changed) {
                footer_info_shown = info_now;
                theme_renderFooter(screen);
                theme_renderStandardHintSelect(screen, lang_get(LANG_SELECT, LANG_FALLBACK_SELECT),
                                               lang_get(LANG_BACK, LANG_FALLBACK_BACK),
                                               info_now ? "INFO" : NULL);
            }

            if (footer_changed || list_changed)
                theme_renderFooterStatus(screen, menu_stack[menu_level]->active_pos + 1, menu_stack[menu_level]->item_count);

            if (header_changed || battery_changed)
                theme_renderHeaderBattery(screen, battery_percentage);

            if (header_changed || list_changed || footer_changed || battery_changed) {
                SDL_BlitSurface(screen, NULL, video, NULL);
                SDL_Flip(video);
            }

            if (blf_changing != prev_blf_changing) {
                reset_menus = true;
                prev_blf_changing = blf_changing;
            }

            header_changed = false;
            footer_changed = false;
            list_changed = false;
            battery_changed = false;
            all_changed = false;

            acc_ticks -= time_step;
        }

        // Idle until the next frame instead of spinning on a CPU core
        if (acc_ticks < time_step)
            SDL_Delay(time_step - acc_ticks);
    }

    // A colour chosen just before leaving (actions.h)
    accent_applyPending(true);

    // Clear the screen when exiting
    SDL_FillRect(video, NULL, 0);
    SDL_Flip(video);

    // Rows changed and Tweaks closed with MENU, without going back from
    // Game lists with B: resize the theme's list icons now. Does nothing
    // when no resize is pending.
    gamelists_on_menu_exit();

    settings_save();
    value_setFrameThrottle();
    value_setSwapTriggers();

    if (DEVICE_ID == MIYOO354) {
        value_setLcdVoltage();
    }
    if (HAS_WIFI()) {
        check_networkChanged();
    }

    Mix_CloseAudio();

    network_freeSmbShares();
    diags_freeEntries();

    display_close();

    lang_free();
    menu_free_all();
    theme_renderHeader_cleanup();
    theme_renderStandardHint_cleanup();
    theme_renderDialog_cleanup();
    resources_free();
    SDL_FreeSurface(screen);
    SDL_FreeSurface(video);

#ifndef PLATFORM_MIYOOMINI
    msleep(200); // to clear SDL input on quit
#endif

    SDL_Quit();

    return EXIT_SUCCESS;
}
