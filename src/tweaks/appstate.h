#ifndef TWEAKS_APPSTATE_H__
#define TWEAKS_APPSTATE_H__

/**
 * @file appstate.h
 * @brief Global application state for the Tweaks application
 *
 * This file contains:
 * - Menu stack for navigation (menu_level, menu_stack[])
 * - Static List instances for all menus
 * - UI state flags (quit, changed flags, key states)
 * - Signal handler for graceful shutdown
 *
 * MENU MANAGEMENT:
 * ================
 * Menus are managed as a stack where:
 * - menu_level: current depth in the menu hierarchy
 * - menu_stack[]: array of List pointers representing the navigation path
 *
 * Each menu is a static List instance that is lazily initialized
 * (checked via _created flag) and cached for the application lifetime.
 *
 * UI STATE FLAGS:
 * ===============
 * - quit: Set to true to exit the main loop
 * - all_changed: Force complete redraw
 * - header_changed: Redraw header only
 * - list_changed: Redraw list items only
 * - footer_changed: Redraw footer only
 * - battery_changed: Update battery indicator
 *
 * FUTURE REFACTORING:
 * ===================
 * TODO: Convert static List instances to a dynamic registry
 * TODO: Separate UI state from menu definitions
 */

#include <signal.h>

#include "components/list.h"
#include "utils/keystate.h"
#include "utils/sdl_init.h"

/* ============================================================================
 * Menu Navigation Stack
 * ============================================================================ */

static int menu_level = 0;
static List *menu_stack[5];

/**
 * @brief Check if the current menu matches a target menu
 */
bool isMenu(List *target)
{
    return menu_stack[menu_level]->_created && target->_created && menu_stack[menu_level]->_id == target->_id;
}

/* ============================================================================
 * Network Menu Instances
 * ============================================================================ */

static List _menu_network;
static List _menu_wifi;
static List _menu_telnet;
static List _menu_ftp;
static List _menu_wps;
static List _menu_http;
static List _menu_ssh;
static List _menu_smbd;
static List _menu_vnc;

void menu_network_free_all(void)
{
    list_free(&_menu_network);
    list_free(&_menu_wifi);
    list_free(&_menu_telnet);
    list_free(&_menu_ftp);
    list_free(&_menu_wps);
    list_free(&_menu_http);
    list_free(&_menu_ssh);
    list_free(&_menu_smbd);
    list_free(&_menu_vnc);
}

/* ============================================================================
 * Icon Menu Instances
 * ============================================================================ */

static List _menu_icons;
static List _menu_icon_packs;
static List _menu_console_icons;
static List _menu_app_icons;
static List _menu_expert_icons;
static List _menu_temp;

void menu_icons_free_all(void)
{
    list_free(&_menu_icons);
    list_free(&_menu_icon_packs);
    list_free(&_menu_console_icons);
    list_free(&_menu_app_icons);
    list_free(&_menu_expert_icons);
    list_free(&_menu_temp);
}

/* ============================================================================
 * Main Menu Instances
 * ============================================================================ */

static List _menu_main;
static List _menu_system;
static List _menu_date_time;
static List _menu_system_display;
static List _menu_user_blue_light;
static List _menu_system_startup;
static List _menu_button_action;
static List _menu_button_action_mainui_menu;
static List _menu_button_action_ingame_menu;
static List _menu_user_interface;
static List _menu_theme_overrides;
static List _menu_battery_percentage;
static List _menu_advanced;
static List _menu_reset_settings;
static List _menu_tools;
static List _menu_tools_m3uGenerator;
static List _menu_diagnostics;
static List _menu_screen_recorder;

/**
 * @brief Free all menu instances
 *
 * Called when resetting menus or on application exit.
 */
void menu_free_all(void)
{
    list_free(&_menu_main);
    list_free(&_menu_system);
    list_free(&_menu_date_time);
    list_free(&_menu_system_display);
    list_free(&_menu_system_startup);
    list_free(&_menu_button_action);
    list_free(&_menu_button_action_mainui_menu);
    list_free(&_menu_button_action_ingame_menu);
    list_free(&_menu_user_interface);
    list_free(&_menu_theme_overrides);
    list_free(&_menu_battery_percentage);
    list_free(&_menu_advanced);
    list_free(&_menu_reset_settings);
    list_free(&_menu_tools);
    list_free(&_menu_tools_m3uGenerator);
    list_free(&_menu_diagnostics);
    list_free(&_menu_screen_recorder);
    list_free(&_menu_user_blue_light);

    menu_icons_free_all();
    menu_network_free_all();
}

/* ============================================================================
 * UI State Flags
 * ============================================================================ */

static bool quit = false;              /* Exit main loop when true */
static bool all_changed = true;        /* Force complete redraw */
static bool header_changed = true;     /* Redraw header */
static bool list_changed = true;       /* Redraw list items */
static bool footer_changed = true;     /* Redraw footer */
static bool battery_changed = true;    /* Update battery indicator */
static KeyState keystate[320] = {(KeyState)0};  /* Key press states */
static bool keys_enabled = true;       /* Process key input */
static bool reset_menus = false;       /* Rebuild all menus */
static bool skip_next_change = false;  /* Skip next change event */
static bool blf_changing = false;      /* Blue light filter changing */
static bool prev_blf_changing = false; /* Previous BLF state */

static bool _disable_confirm = false;
static SDL_Surface *background_cache = NULL;

static char ip_address_label[STR_MAX];

/* ============================================================================
 * Signal Handler
 * ============================================================================ */

/**
 * @brief Handle system signals for graceful shutdown
 */
static void sigHandler(int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        quit = true;
        break;
    default:
        break;
    }
}

#endif // TWEAKS_APPSTATE_H__