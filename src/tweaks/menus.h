#ifndef TWEAKS_MENUS_H__
#define TWEAKS_MENUS_H__

#include <SDL/SDL_image.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "components/kbinput_wrapper.h"
#include "components/list.h"
#include "system/device_model.h"
#include "system/display.h"
#include "utils/apps.h"

#include "./actions.h"
#include "./appstate.h"
#include "./diags.h"
#include "./formatters.h"
#include "./game_lists.h"
#include "./icons.h"
#include "./main_menu.h"
#include "./network.h"
#include "./reset.h"
#include "./tools.h"
#include "./values.h"

void menu_systemStartup(void *_)
{
    if (!_menu_system_startup._created) {
        _menu_system_startup = list_createWithTitle(3, LIST_SMALL, "Startup");

        list_addItemWithInfoNote(&_menu_system_startup,
                                 (ListItem){
                                     .label = "Resume the last game",
                                     .item_type = TOGGLE,
                                     .value = (int)settings.startup_auto_resume,
                                     .action = action_setStartupAutoResume},
                                 "If you turn the device off while a game\n"
                                 "is running, the game resumes where you\n"
                                 "left off at the next startup.");
        list_addItemWithInfoNote(&_menu_system_startup,
                                 (ListItem){
                                     .label = "Open at startup",
                                     .item_type = MULTIVALUE,
                                     .value_max = 3,
                                     .value_labels = {"MainUI", "GameSwitcher", "RetroArch", "AdvanceMENU"},
                                     .value = settings.startup_application,
                                     .action = action_setStartupApplication},
                                 "What opens when the device starts:\n"
                                 "MainUI, the GameSwitcher, RetroArch or\n"
                                 "AdvanceMENU.");
        list_addItemWithInfoNote(&_menu_system_startup,
                                 (ListItem){
                                     .label = "Home screen tab",
                                     .item_type = MULTIVALUE,
                                     .value_max = 5,
                                     .value_formatter = formatter_startupTab,
                                     .value = settings.startup_tab,
                                     .action = action_setStartupTab},
                                 "The tab MainUI shows at startup.");
    }
    menu_stack[++menu_level] = &_menu_system_startup;
    header_changed = true;
}

void menu_systemDisplay(void *_)
{
    if (!_menu_system_display._created) {
        _menu_system_display = list_createWithTitle(1, LIST_SMALL, "Display");
    }
    menu_stack[++menu_level] = &_menu_system_display;
    header_changed = true;
}

bool _writeDateString(char *label_out)
{
    char new_label[STR_MAX];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(new_label, STR_MAX - 1, "Now: %Y-%m-%d %H:%M:%S", &tm);
    if (strncmp(new_label, label_out, STR_MAX) != 0) {
        strcpy(label_out, new_label);
        return true;
    }
    return false;
}

void menu_datetime(void *_)
{
    if (!_menu_date_time._created) {
        _menu_date_time = list_create(7, LIST_SMALL);
        strcpy(_menu_date_time.title, "Date and time");
        list_addItem(&_menu_date_time,
                     (ListItem){
                         .label = "[DATESTRING]",
                         .disabled = 1,
                         .action = NULL});

        network_loadState();

        if (HAS_WIFI() || network_state.ntp) {
            list_addItemWithInfoNote(&_menu_date_time,
                                     (ListItem){
                                         .label = "Set automatically via internet",
                                         .item_type = TOGGLE,
                                         .value = (int)network_state.ntp,
                                         .action = network_setNtpState},
                                     "Use the internet connection to sync\n"
                                     "date and time on startup.");
        }
        if (HAS_WIFI()) {
            list_addItemWithInfoNote(&_menu_date_time,
                                     (ListItem){
                                         .label = "Wait for sync on startup",
                                         .item_type = TOGGLE,
                                         .disabled = !network_state.ntp,
                                         .value = (int)network_state.ntp_wait,
                                         .action = network_setNtpWaitState},
                                     "Wait at startup until the date and time\n"
                                     "are synced, so they are right before a\n"
                                     "game starts.");
            list_addItemWithInfoNote(&_menu_date_time,
                                     (ListItem){
                                         .label = "Enable Wi-Fi temporarily",
                                         .item_type = TOGGLE,
                                         .disabled = !network_state.ntp,
                                         .value = (int)network_state.force_wifi_on_startup,
                                         .action = network_setNtpForceState},
                                     "Enables Wi-Fi temporarily at startup\n"
                                     "to sync the time. It will be turned off\n"
                                     "again after the sync (if it was off).");
            list_addItemWithInfoNote(&_menu_date_time,
                                     (ListItem){
                                         .label = "Get time zone via IP address",
                                         .item_type = TOGGLE,
                                         .disabled = !network_state.ntp,
                                         .value = !network_state.manual_tz,
                                         .action = network_setTzManualState},
                                     "Find your time zone from your IP\n"
                                     "address.\n"
                                     " \n"
                                     "You may want to turn this off if you\n"
                                     "use a VPN.");
            list_addItemWithInfoNote(&_menu_date_time,
                                     (ListItem){
                                         .label = "Select time zone",
                                         .item_type = MULTIVALUE,
                                         .disabled = !network_state.ntp || !network_state.manual_tz,
                                         .value_max = 48,
                                         .value_formatter = formatter_timezone,
                                         .value = value_timezone(),
                                         .action = network_setTzSelectState},
                                     "Manually set your time zone.\n"
                                     "You need to adjust for DST as well.");
        }
        list_addItemWithInfoNote(&_menu_date_time,
                                 (ListItem){
                                     .label = "Emulated time skip",
                                     .item_type = MULTIVALUE,
                                     .disabled = network_state.ntp,
                                     .value_max = 24,
                                     .value_formatter = formatter_timeSkip,
                                     .value = settings.time_skip,
                                     .action = action_setTimeSkip},
                                 "Without a clock battery the time stands\n"
                                 "still while the device is off. This adds\n"
                                 "a number of hours at each startup.");
    }
    _writeDateString(_menu_date_time.items[0].label);
    menu_stack[++menu_level] = &_menu_date_time;
    header_changed = true;
}

void menu_system(void *_)
{
    if (!_menu_system._created) {
        _menu_system = list_createWithTitle(6, LIST_SMALL, "System");
        list_addItem(&_menu_system,
                     (ListItem){
                         .label = "Startup...",
                         .action = menu_systemStartup});
        // list_addItem(&_menu_system,
        //              (ListItem){
        //                  .label = "Display...",
        //                  .action = menu_systemDisplay});
        list_addItem(&_menu_system,
                     (ListItem){
                         .label = "Date and time...",
                         .action = menu_datetime});
        list_addItemWithInfoNote(&_menu_system,
                                 (ListItem){
                                     .label = "Low battery warning",
                                     .item_type = MULTIVALUE,
                                     .value_max = 5,
                                     .value_formatter = formatter_battWarn,
                                     .value = settings.low_battery_warn_at / 5,
                                     .action = action_setLowBatteryWarnAt},
                                 "Show a red battery icon in the top right\n"
                                 "corner when the battery is at or below\n"
                                 "this level.");
        list_addItemWithInfoNote(&_menu_system,
                                 (ListItem){
                                     .label = "Low battery: save and quit",
                                     .item_type = MULTIVALUE,
                                     .value_max = 5,
                                     .value_formatter = formatter_battExit,
                                     .value = settings.low_battery_autosave_at,
                                     .action = action_setLowBatteryAutoSave},
                                 "Set the battery percentage at which the\n"
                                 "system should save and exit RetroArch.");
        // Flip-specific: Lid close action
        if (DEVICE_ID == MIYOO285) {
            list_addItemWithInfoNote(&_menu_system,
                                     (ListItem){
                                         .label = "Lid close",
                                         .item_type = MULTIVALUE,
                                         .value_max = 2,
                                         .value_labels = {"Suspend", "Shutdown", "Nothing"},
                                         .value = settings.lid_close_action,
                                         .action = action_setLidCloseAction},
                                     "What happens when you close the lid.");
        }
        list_addItemWithInfoNote(&_menu_system,
                                 (ListItem){
                                     .label = "Vibration strength",
                                     .item_type = MULTIVALUE,
                                     .value_max = 3,
                                     .value_labels = {"Off", "Low", "Normal (default)", "High"},
                                     .value = settings.vibration,
                                     .action = action_setVibration},
                                 "Vibration strength for haptic feedback\n"
                                 "when you press system shortcuts.");
    }
    menu_stack[++menu_level] = &_menu_system;
    header_changed = true;
}

void menu_buttonActionMainUIMenu(void *_)
{
    if (!_menu_button_action_mainui_menu._created) {
        _menu_button_action_mainui_menu = list_create(3, LIST_SMALL);
        strcpy(_menu_button_action_mainui_menu.title, "MENU in MainUI");
        list_addItemWithInfoNote(&_menu_button_action_mainui_menu,
                                 (ListItem){
                                     .label = "Short press",
                                     .item_type = MULTIVALUE,
                                     .value_max = 2,
                                     .value_labels = BUTTON_MAINUI_LABELS,
                                     .value = settings.mainui_single_press,
                                     .action_id = 0,
                                     .action = action_setMenuButtonKeymap},
                                 "Set the action for a short press of\n"
                                 "MENU in MainUI (menus and game lists).");
        list_addItemWithInfoNote(&_menu_button_action_mainui_menu,
                                 (ListItem){
                                     .label = "Long press",
                                     .item_type = MULTIVALUE,
                                     .value_max = 2,
                                     .value_labels = BUTTON_MAINUI_LABELS,
                                     .value = settings.mainui_long_press,
                                     .action_id = 1,
                                     .action = action_setMenuButtonKeymap},
                                 "Set the action for a long press of\n"
                                 "MENU in MainUI (menus and game lists).");
        list_addItemWithInfoNote(&_menu_button_action_mainui_menu,
                                 (ListItem){
                                     .label = "Double press",
                                     .item_type = MULTIVALUE,
                                     .value_max = 2,
                                     .value_labels = BUTTON_MAINUI_LABELS,
                                     .value = settings.mainui_double_press,
                                     .action_id = 2,
                                     .action = action_setMenuButtonKeymap},
                                 "Set the action for a double press of\n"
                                 "MENU in MainUI (menus and game lists).");
    }
    menu_stack[++menu_level] = &_menu_button_action_mainui_menu;
    header_changed = true;
}

void menu_buttonActionInGameMenu(void *_)
{
    if (!_menu_button_action_ingame_menu._created) {
        _menu_button_action_ingame_menu = list_createWithTitle(3, LIST_SMALL, "MENU in game");
        list_addItemWithInfoNote(&_menu_button_action_ingame_menu,
                                 (ListItem){
                                     .label = "Short press",
                                     .item_type = MULTIVALUE,
                                     .value_max = 4,
                                     .value_labels = BUTTON_INGAME_LABELS,
                                     .value = settings.ingame_single_press,
                                     .action_id = 3,
                                     .action = action_setMenuButtonKeymap},
                                 "Set the action for a short press of\n"
                                 "MENU while playing.");
        list_addItemWithInfoNote(&_menu_button_action_ingame_menu,
                                 (ListItem){
                                     .label = "Long press",
                                     .item_type = MULTIVALUE,
                                     .value_max = 4,
                                     .value_labels = BUTTON_INGAME_LABELS,
                                     .value = settings.ingame_long_press,
                                     .action_id = 4,
                                     .action = action_setMenuButtonKeymap},
                                 "Set the action for a long press of\n"
                                 "MENU while playing.");
        list_addItemWithInfoNote(&_menu_button_action_ingame_menu,
                                 (ListItem){
                                     .label = "Double press",
                                     .item_type = MULTIVALUE,
                                     .value_max = 4,
                                     .value_labels = BUTTON_INGAME_LABELS,
                                     .value = settings.ingame_double_press,
                                     .action_id = 5,
                                     .action = action_setMenuButtonKeymap},
                                 "Set the action for a double press of\n"
                                 "MENU while playing.");
    }
    menu_stack[++menu_level] = &_menu_button_action_ingame_menu;
    header_changed = true;
}

void menu_buttonAction(void *_)
{
    if (!_menu_button_action._created) {
        _menu_button_action = list_createWithTitle(9, LIST_SMALL, "Buttons");
        list_addItem(&_menu_button_action,
                     (ListItem){
                         .label = "MENU in game...",
                         .action = menu_buttonActionInGameMenu});
        list_addItem(&_menu_button_action,
                     (ListItem){
                         .label = "MENU in MainUI...",
                         .action = menu_buttonActionMainUIMenu});

        getInstalledApps(true);
        list_addItemWithInfoNote(&_menu_button_action,
                                 (ListItem){
                                     .label = "X in MainUI",
                                     .item_type = MULTIVALUE,
                                     .value_max = installed_apps_count + NUM_TOOLS + MAINUI_SHORTCUT_EXTRA_COUNT,
                                     .value = value_appShortcut(0),
                                     .value_formatter = formatter_appShortcut,
                                     .action_id = 0,
                                     .action = action_setAppShortcut},
                                 "Set the X button action in MainUI:\n"
                                 "an app, a tool, GLO (the game's options),\n"
                                 "Toggle favorite (add or remove the game\n"
                                 "from Favorites) or Disabled (no action).");
        list_addItemWithInfoNote(&_menu_button_action,
                                 (ListItem){
                                     .label = "Y in MainUI",
                                     .item_type = MULTIVALUE,
                                     .value_max = installed_apps_count + NUM_TOOLS + MAINUI_SHORTCUT_EXTRA_COUNT,
                                     .value = value_appShortcut(1),
                                     .value_formatter = formatter_appShortcut,
                                     .action_id = 1,
                                     .action = action_setAppShortcut},
                                 "Set the Y button action in MainUI:\n"
                                 "an app, a tool, GLO (the game's options),\n"
                                 "Toggle favorite (add or remove the game\n"
                                 "from Favorites) or Disabled (no action).");
        list_addItemWithInfoNote(&_menu_button_action,
                                 (ListItem){
                                     .label = "POWER (short press)",
                                     .item_type = MULTIVALUE,
                                     .value_max = 1,
                                     .value_labels = {"Standby", "Shutdown"},
                                     .value = (int)settings.disable_standby,
                                     .action = action_setDisableStandby},
                                 "A short press of POWER: Standby (sleep)\n"
                                 "or Shutdown.");
        if (DEVICE_ID == MIYOO283) {
            list_addItemWithInfoNote(&_menu_button_action,
                                     (ListItem){
                                         .label = "Brightness keys",
                                         .item_type = MULTIVALUE,
                                         .value_max = 1,
                                         .value_labels = {"SELECT+R2/L2",
                                                          "MENU+UP/DOWN"},
                                         .value = config_flag_get(".altBrightness"),
                                         .action = action_setAltBrightness},
                                     "Change the shortcut for brightness.");
        }
        list_addItemWithInfoNote(&_menu_button_action,
                                 (ListItem){
                                     .label = "Vibrate on MENU",
                                     .item_type = TOGGLE,
                                     .value = (int)settings.menu_button_haptics,
                                     .action = action_setMenuButtonHaptics},
                                 "Vibrate when MENU is pressed once or\n"
                                 "twice (its shortcuts).");
        list_addItemWithInfoNote(&_menu_button_action,
                                 (ListItem){
                                     .label = "CPU speed keys (SEL+START+L/R)",
                                     .item_type = TOGGLE,
                                     .value = config_flag_get(".cpuClockHotkey"),
                                     .action = action_setCpuClockHotkey},
                                 "SELECT+START+R1 / L1 raise or lower the\n"
                                 "CPU speed by 100 MHz (overclocking).");
        list_addItemWithInfoNote(&_menu_button_action,
                                 (ListItem){
                                     .label = "Swap L/L2 and R/R2",
                                     .item_type = TOGGLE,
                                     .value = value_getSwapTriggers(),
                                     .action = action_advancedSetSwapTriggers},
                                 "Swap the function of L<>L2 and R<>R2\n"
                                 "(only affects in-game actions).");
    }
    menu_stack[++menu_level] = &_menu_button_action;
    header_changed = true;
}

// Battery percentage: its look only matters while it is shown: Visible On,
// or "-" with a theme that shows it.
static void battery_update_enabled(void)
{
    if (!_menu_battery_percentage._created)
        return;
    int visible = _menu_battery_percentage.items[0].value; // "-", Off, On
    bool off = visible == 1 || (visible == 0 && !resources.theme_back.batteryPercentage.visible);
    for (int i = 1; i < _menu_battery_percentage.item_count; i++)
        _menu_battery_percentage.items[i].disabled = off;
}

static void battery_action_visible(void *pt)
{
    action_batteryPercentageVisible(pt);
    battery_update_enabled();
}

void menu_batteryPercentage(void *_)
{
    if (!_menu_battery_percentage._created) {
        _menu_battery_percentage = list_createWithTitle(7, LIST_SMALL, "Battery percentage");
        list_addItem(&_menu_battery_percentage,
                     (ListItem){
                         .label = "Visible",
                         .item_type = MULTIVALUE,
                         .value_max = 2,
                         .value_labels = THEME_TOGGLE_LABELS,
                         .value = value_batteryPercentageVisible(),
                         .action = battery_action_visible});
        list_addItem(&_menu_battery_percentage,
                     (ListItem){
                         .label = "Font family",
                         .item_type = MULTIVALUE,
                         .value_max = num_font_families,
                         .value_formatter = formatter_fontFamily,
                         .value = value_batteryPercentageFontFamily(),
                         .action = action_batteryPercentageFontFamily});
        list_addItem(&_menu_battery_percentage,
                     (ListItem){
                         .label = "Font size",
                         .item_type = MULTIVALUE,
                         .value_max = num_font_sizes,
                         .value_formatter = formatter_fontSize,
                         .value = value_batteryPercentageFontSize(),
                         .action = action_batteryPercentageFontSize});
        list_addItem(&_menu_battery_percentage,
                     (ListItem){
                         .label = "Text alignment",
                         .item_type = MULTIVALUE,
                         .value_max = 3,
                         .value_labels = {"-", "Left", "Center", "Right"},
                         .value = value_batteryPercentagePosition(),
                         .action = action_batteryPercentagePosition});
        list_addItem(&_menu_battery_percentage,
                     (ListItem){
                         .label = "Fixed position",
                         .item_type = MULTIVALUE,
                         .value_max = 2,
                         .value_labels = THEME_TOGGLE_LABELS,
                         .value = value_batteryPercentageFixed(),
                         .action = action_batteryPercentageFixed});
        list_addItem(&_menu_battery_percentage,
                     (ListItem){
                         .label = "Horizontal offset",
                         .item_type = MULTIVALUE,
                         .value_max = BATTPERC_MAX_OFFSET * 2 + 1,
                         .value_formatter = formatter_positionOffset,
                         .value = value_batteryPercentageOffsetX(),
                         .action = action_batteryPercentageOffsetX});
        list_addItem(&_menu_battery_percentage,
                     (ListItem){
                         .label = "Vertical offset",
                         .item_type = MULTIVALUE,
                         .value_max = BATTPERC_MAX_OFFSET * 2 + 1,
                         .value_formatter = formatter_positionOffset,
                         .value = value_batteryPercentageOffsetY(),
                         .action = action_batteryPercentageOffsetY});
        battery_update_enabled();
    }
    menu_stack[++menu_level] = &_menu_battery_percentage;
    header_changed = true;
}

void menu_themeOverrides(void *_)
{
    if (!_menu_theme_overrides._created) {
        _menu_theme_overrides = list_create(7, LIST_SMALL);
        strcpy(_menu_theme_overrides.title, "Theme tweaks");
        list_addItem(&_menu_theme_overrides,
                     (ListItem){
                         .label = "Battery percentage...",
                         .action = menu_batteryPercentage});
        list_addItemWithInfoNote(&_menu_theme_overrides,
                                 (ListItem){
                                     .label = "Mute background music",
                                     .item_type = TOGGLE,
                                     .value = settings.bgm_mute,
                                     .action = action_toggleBackgroundMusic},
                                 "Mute the theme's background music.");
        list_addItemWithInfoNote(&_menu_theme_overrides,
                                 (ListItem){
                                     .label = "Hide icon labels",
                                     .item_type = MULTIVALUE,
                                     .value_max = 2,
                                     .value_labels = THEME_TOGGLE_LABELS,
                                     .value = value_hideLabelsIcons(),
                                     .action = action_hideLabelsIcons},
                                 "Hide the labels under the main menu icons.");
        list_addItemWithInfoNote(&_menu_theme_overrides,
                                 (ListItem){
                                     .label = "Hide button hints",
                                     .item_type = MULTIVALUE,
                                     .value_max = 2,
                                     .value_labels = THEME_TOGGLE_LABELS,
                                     .value = value_hideLabelsHints(),
                                     .action = action_hideLabelsHints},
                                 "Hide the button hints at the bottom of\n"
                                 "the screen.");
        // list_addItem(&_menu_theme_overrides, (ListItem){
        // 	.label = "[Title] Font size", .item_type = MULTIVALUE,
        // .value_max = num_font_sizes, .value_formatter = formatter_fontSize
        // });
        // list_addItem(&_menu_theme_overrides, (ListItem){
        // 	.label = "[List] Font size", .item_type = MULTIVALUE, .value_max
        // = num_font_sizes, .value_formatter = formatter_fontSize
        // });
        // list_addItem(&_menu_theme_overrides, (ListItem){
        // 	.label = "[Hint] Font size", .item_type = MULTIVALUE, .value_max
        // = num_font_sizes, .value_formatter = formatter_fontSize
        // });
    }
    menu_stack[++menu_level] = &_menu_theme_overrides;
    header_changed = true;
}

void menu_blueLight(void *_)
{
    bool schedule_show = (HAS_WIFI() || settings.rtc_available || settings.blue_light_schedule);
    bool schedule_disable = (!settings.rtc_available && !network_state.ntp && !settings.blue_light_schedule);
    if (!_menu_user_blue_light._created) {
        network_loadState();
        _menu_user_blue_light = list_createWithTitle(6, LIST_SMALL, "Blue light filter");
        if (schedule_show) {
            list_addItem(&_menu_user_blue_light,
                         (ListItem){
                             .label = "[DATESTRING]",
                             .disabled = 1,
                             .action = NULL});
        }
        list_addItemWithInfoNote(&_menu_user_blue_light,
                                 (ListItem){
                                     .label = "State",
                                     .disable_arrows = blf_changing,
                                     .disable_a_btn = blf_changing,
                                     .item_type = TOGGLE,
                                     .value = (int)settings.blue_light_state || exists("/tmp/.blfOn"),
                                     .action = action_blueLight},
                                 "Turn the filter on or off now, at the\n"
                                 "selected strength.");
        if (schedule_show) {
            list_addItemWithInfoNote(&_menu_user_blue_light,
                                     (ListItem){
                                         .label = "",
                                         .disabled = schedule_disable,
                                         .item_type = TOGGLE,
                                         .value = (int)settings.blue_light_schedule,
                                         .action = action_blueLightSchedule},
                                     "Turn the filter on and off by itself at\n"
                                     "the times below.");
        }
        list_addItemWithInfoNote(&_menu_user_blue_light,
                                 (ListItem){
                                     .label = "Strength",
                                     .item_type = MULTIVALUE,
                                     .disable_arrows = blf_changing,
                                     .disable_a_btn = blf_changing,
                                     .value_max = 4,
                                     .value_labels = BLUELIGHT_LABELS,
                                     .action = action_blueLightLevel,
                                     .value = value_blueLightLevel()},
                                 "How strong the blue light filter is.");
        if (schedule_show) {
            list_addItemWithInfoNote(&_menu_user_blue_light,
                                     (ListItem){
                                         .label = "Time (On)",
                                         .disabled = !settings.blue_light_schedule,
                                         .item_type = MULTIVALUE,
                                         .value_max = 95,
                                         .value_formatter = formatter_TimeOn,
                                         .action = action_blueLightTimeOn,
                                         .value = value_blueLightTimeOn()},
                                     "When the schedule turns the filter on.");
            list_addItemWithInfoNote(&_menu_user_blue_light,
                                     (ListItem){
                                         .label = "Time (Off)",
                                         .disabled = !settings.blue_light_schedule,
                                         .item_type = MULTIVALUE,
                                         .value_max = 95,
                                         .value_formatter = formatter_TimeOff,
                                         .action = action_blueLightTimeOff,
                                         .value = value_blueLightTimeOff()},
                                     "When the schedule turns the filter off.");
        }
    }
    if (schedule_show) {
        _writeDateString(_menu_user_blue_light.items[0].label);
        char scheduleToggleLabel[100];
        strcpy(scheduleToggleLabel, exists("/tmp/.blfIgnoreSchedule") ? "Schedule (ignored)" : "Schedule");
        strcpy(_menu_user_blue_light.items[2].label, scheduleToggleLabel);
    }
    menu_stack[++menu_level] = &_menu_user_blue_light;
    header_changed = true;
}

void menu_lookAndFeel(void *_)
{
    settings.blue_light_state = config_flag_get(".blfOn");
    all_changed = true;
    if (!_menu_user_interface._created) {
        _menu_user_interface = list_createWithTitle(7, LIST_SMALL, "Look and feel");
        list_addItem(&_menu_user_interface,
                     (ListItem){
                         .label = "Theme tweaks...",
                         .action = menu_themeOverrides});
        list_addItem(&_menu_user_interface,
                     (ListItem){
                         .label = "Icons...",
                         .action = menu_icons});
        list_addItem(&_menu_user_interface,
                     (ListItem){
                         .label = "Blue light filter...",
                         .action = menu_blueLight});
        list_addItemWithInfoNote(&_menu_user_interface,
                                 (ListItem){
                                     .label = "Loading/saving screen",
                                     .item_type = MULTIVALUE,
                                     .value_max = 3,
                                     .value_labels = {"Popup", "Full screen", "Black", "Full popup"},
                                     .value = settings.launch_screen,
                                     .action = action_setLaunchScreen},
                                 "Shown while a game starts or saves:\n"
                                 "the game screenshot with a popup,\n"
                                 "the screenshot alone, black, or the\n"
                                 "message on the theme's full screen.");
        list_addItemWithInfoNote(&_menu_user_interface,
                                 (ListItem){
                                     .label = "Button hints at the bottom",
                                     .item_type = MULTIVALUE,
                                     .value_max = 1,
                                     .value_labels = {"Classic", "B A, page left"},
                                     .value = settings.mainui_footer_alt,
                                     .action = action_setFooterLayout},
                                 "Bottom bar of the menus, lists, Tweaks\n"
                                 "and the GameSwitcher: Classic, as stock,\n"
                                 "or the page number on the left and the\n"
                                 "hints in button order, B then A.");
        list_addItemWithInfoNote(&_menu_user_interface,
                                 (ListItem){
                                     .label = "Long SELECT menu labels",
                                     .item_type = MULTIVALUE,
                                     .value_max = 1,
                                     .value_labels = {"Shorten with ...", "Cut"},
                                     .value = settings.mainui_menu_no_ellipsis,
                                     .action = action_setLongMenuLabels},
                                 "SELECT menu labels too long for the\n"
                                 "theme: shortened with ... or cut at\n"
                                 "the edge. Silky RedOnion fits them all.");
        display_init(true);
        list_addItemWithInfoNote(&_menu_user_interface,
                                 (ListItem){
                                     .label = "Volume/brightness bar",
                                     .item_type = MULTIVALUE,
                                     .value_max = 15,
                                     .value_formatter = formatter_meterWidth,
                                     .value = value_meterWidth(),
                                     .action = action_meterWidth},
                                 "Width of the bar on the left of the\n"
                                 "screen while you change the brightness,\n"
                                 "or the volume on the Mini Plus.");
    }
    menu_stack[++menu_level] = &_menu_user_interface;
    header_changed = true;
}

void menu_resetSettings(void *_)
{
    if (!_menu_reset_settings._created) {
        _menu_reset_settings = list_createWithTitle(7, LIST_SMALL, "Reset settings");
        list_addItemWithInfoNote(&_menu_reset_settings,
                                 (ListItem){
                                     .label = "Reset system tweaks",
                                     .action = action_resetTweaks},
                                 "Reset all RedOnion system tweaks,\n"
                                 "including network setup.");
        list_addItem(&_menu_reset_settings,
                     (ListItem){
                         .label = "Reset theme tweaks",
                         .action = action_resetThemeOverrides});
        list_addItemWithInfoNote(&_menu_reset_settings,
                                 (ListItem){
                                     .label = "Reset MainUI settings",
                                     .action = action_resetMainUI},
                                 "Resets the settings stored on the device,\n"
                                 "such as theme, display options and\n"
                                 "volume. Also resets the Wi-Fi setup.");
        list_addItem(&_menu_reset_settings,
                     (ListItem){
                         .label = "Reset RetroArch main configuration",
                         .action = action_resetRAMain});
        list_addItem(&_menu_reset_settings,
                     (ListItem){
                         .label = "Reset all RetroArch core overrides",
                         .action = action_resetRACores});
        list_addItem(&_menu_reset_settings,
                     (ListItem){
                         .label = "Reset AdvanceMENU/MAME/MESS",
                         .action = action_resetAdvanceMENU});
        list_addItem(&_menu_reset_settings,
                     (ListItem){
                         .label = "Reset everything", .action = action_resetAll});
    }
    menu_stack[++menu_level] = &_menu_reset_settings;
    header_changed = true;
}

void menu_diagnostics(void *pt)
{
    if (!_menu_diagnostics._created) {
        diags_getEntries();

        _menu_diagnostics = list_createWithSticky(1 + diags_numScripts, "Diagnostics");
        list_addItemWithInfoNote(&_menu_diagnostics,
                                 (ListItem){
                                     .label = "Enable logging",
                                     .sticky_note = "Enable global logging",
                                     .item_type = TOGGLE,
                                     .value = (int)settings.enable_logging,
                                     .action = action_setEnableLogging},
                                 "Log what the system and the network do.\n"
                                 "The logs go to .tmp_update/logs on the\n"
                                 "SD card.");
        for (int i = 0; i < diags_numScripts; i++) {
            ListItem diagItem = {
                .label = "",
                .payload_ptr = &scripts[i].filename,
                .action = action_runDiagnosticScript,
            };

            const char *prefix;
            if (strncmp(scripts[i].filename, "util", 4) == 0) {
                prefix = "Util:%.62s";
            }
            else if (strncmp(scripts[i].filename, "fix", 3) == 0) {
                prefix = "Fix:%.62s";
            }
            else {
                prefix = "%.62s";
            }

            snprintf(diagItem.label, DIAG_MAX_LABEL_LENGTH - 1, prefix, scripts[i].label);
            strncpy(diagItem.sticky_note, "Idle: Selected script not running", STR_MAX - 1);

            char *parsed_Tooltip = diags_parseNewLines(scripts[i].tooltip);
            list_addItemWithInfoNote(&_menu_diagnostics, diagItem, parsed_Tooltip);
            free(parsed_Tooltip);
        }
    }

    menu_stack[++menu_level] = &_menu_diagnostics;
    header_changed = true;
}

void menu_advanced(void *_)
{
    if (!_menu_advanced._created) {
        _menu_advanced = list_createWithTitle(5, LIST_SMALL, "Advanced");
        list_addItemWithInfoNote(&_menu_advanced,
                                 (ListItem){
                                     .label = "Fast forward speed",
                                     .item_type = MULTIVALUE,
                                     .value_max = 50,
                                     .value = value_getFrameThrottle(),
                                     .value_formatter = formatter_fastForward,
                                     .action = action_advancedSetFrameThrottle},
                                 "Set the maximum fast forward rate.");
        list_addItemWithInfoNote(&_menu_advanced,
                                 (ListItem){
                                     .label = "Screen PWM frequency",
                                     .item_type = MULTIVALUE,
                                     .value_max = 9,
                                     .value_labels = PWM_FREQUENCIES,
                                     .value = value_getPWMFrequency(),
                                     .action = action_advancedSetPWMFreqency},
                                 "Frequency of the screen backlight PWM.\n"
                                 "Lower values can buzz less.\n"
                                 "Experimental.");
        if (DEVICE_ID == MIYOO354) {
            list_addItemWithInfoNote(&_menu_advanced,
                                     (ListItem){
                                         .label = "LCD undervolt",
                                         .item_type = MULTIVALUE,
                                         .value_max = 4,
                                         .value_labels = {"Off (default)", "-0.1V", "-0.2V", "-0.3V", "-0.4V"},
                                         .value = value_getLcdVoltage(),
                                         .action = action_advancedSetLcdVoltage},
                                     "Use this option if you're seeing\n"
                                     "small artifacts on the display.");
        }
        list_addItem(&_menu_advanced,
                     (ListItem){
                         .label = "Diagnostics...",
                         .action = menu_diagnostics});
        if (exists(RESET_CONFIGS_PAK)) {
            list_addItem(&_menu_advanced,
                         (ListItem){
                             .label = "Reset settings...",
                             .action = menu_resetSettings});
        }
    }
    menu_stack[++menu_level] = &_menu_advanced;
    header_changed = true;
}

void menu_screen_recorder(void *pt)
{
    if (!_menu_screen_recorder._created) {
        _menu_screen_recorder = list_createWithSticky(7, "Screen recorder setup");
        list_addItemWithInfoNote(&_menu_screen_recorder,
                                 (ListItem){
                                     .label = "Start/stop recorder",
                                     .sticky_note = "Status:...",
                                     .action = tool_screenRecorder},
                                 "Start or stop the recorder.");
        list_addItemWithInfoNote(&_menu_screen_recorder,
                                 (ListItem){
                                     .label = "Countdown (seconds)",
                                     .sticky_note = "Specify the countdown",
                                     .item_type = MULTIVALUE,
                                     .value_max = 10,
                                     .value = (int)settings.rec_countdown,
                                     .value_formatter = formatter_recCountdown,
                                     .action = action_toggleScreenRecCountdown},
                                 "Countdown before recording starts.\n"
                                 " \n"
                                 "The screen pulses white once for each\n"
                                 "second, to show recording has started\n"
                                 "or stopped.");
        list_addItemWithInfoNote(&_menu_screen_recorder,
                                 (ListItem){
                                     .label = "Toggle indicator icon",
                                     .sticky_note = "Turn the indicator on/off",
                                     .item_type = TOGGLE,
                                     .value = (int)settings.rec_indicator,
                                     .action = action_toggleScreenRecIndicator},
                                 "Show a flashing icon to remind you that\n"
                                 "you're still recording.");
        list_addItemWithInfoNote(&_menu_screen_recorder,
                                 (ListItem){
                                     .label = "Toggle hotkey",
                                     .sticky_note = "Turn the hotkey (MENU+A) on/off",
                                     .item_type = TOGGLE,
                                     .value = (int)settings.rec_hotkey,
                                     .action = action_toggleScreenRecHotkey},
                                 "Turn the hotkey on or off.\n"
                                 " \n"
                                 "Recording can be started and stopped\n"
                                 "with MENU+A.");
        list_addItemWithInfoNote(&_menu_screen_recorder,
                                 (ListItem){
                                     .label = "Reset screen recorder",
                                     .sticky_note = "Hard kill ffmpeg if it's crashed",
                                     .action = action_hardKillFFmpeg},
                                 "Performs a hard kill of ffmpeg.\n\n"
                                 "WARNING: If you're currently\n"
                                 "recording, you may lose the file!");
        list_addItemWithInfoNote(&_menu_screen_recorder,
                                 (ListItem){
                                     .label = "Delete all recordings",
                                     .sticky_note = "Empties the recordings directory",
                                     .action = action_deleteAllRecordings},
                                 "Deletes all recorded videos.\n"
                                 " \n"
                                 "WARNING: This action cannot\n"
                                 "be undone!");
    }

    int isRecordingActive = exists("/tmp/recorder_active");
    const char *recordingStatus = isRecordingActive ? "Status: Now recording..." : "Status: Idle.";
    strncpy(_menu_screen_recorder.items[0].sticky_note, recordingStatus, sizeof(_menu_screen_recorder.items[0].sticky_note) - 1);
    _menu_screen_recorder.items[0].sticky_note[sizeof(_menu_screen_recorder.items[0].sticky_note) - 1] = '\0';
    menu_stack[++menu_level] = &_menu_screen_recorder;
    header_changed = true;
}

void menu_tools_m3uGenerator(void *_)
{
    if (!_menu_tools_m3uGenerator._created) {
        _menu_tools_m3uGenerator = list_createWithTitle(2, LIST_SMALL, "M3U Generator");
        list_addItemWithInfoNote(&_menu_tools_m3uGenerator,
                                 (ListItem){
                                     .label = "Multiple directories (.Game_Name)",
                                     .action = tool_generateM3uFiles_md},
                                 "One folder for each game (.Game_Name).");
        list_addItemWithInfoNote(&_menu_tools_m3uGenerator,
                                 (ListItem){
                                     .label = "Single directory (.multi-disc)",
                                     .action = tool_generateM3uFiles_sd},
                                 "One folder (.multi-disc) holds all the\n"
                                 "multi-disc files.");
    }
    menu_stack[++menu_level] = &_menu_tools_m3uGenerator;
    header_changed = true;
}

void menu_tools(void *_)
{
    if (!_menu_tools._created) {
        _menu_tools = list_create(NUM_TOOLS, LIST_SMALL);
        strcpy(_menu_tools.title, "Tools");
        list_addItemWithInfoNote(&_menu_tools,
                                 (ListItem){
                                     .label = "Make CUE files for BIN games",
                                     .action = tool_generateCueFiles},
                                 "PS1 games in .bin format need a\n"
                                 "matching .cue file: this tool makes\n"
                                 "them.");
        list_addItemWithInfoNote(&_menu_tools,
                                 (ListItem){
                                     .label = "Make M3U files for multi-disc games...",
                                     .action = menu_tools_m3uGenerator},
                                 "Multi-disc PS1 games need a playlist\n"
                                 "(.m3u): one entry in the list and one\n"
                                 "save file for the whole game.");
        list_addItemWithInfoNote(&_menu_tools,
                                 (ListItem){
                                     .label = "Make game list for short-name roms",
                                     .action = tool_buildShortRomGameList},
                                 "This tool replaces the short names in\n"
                                 "game caches with their equivalent real\n"
                                 "names. This ensures the list is sorted\n"
                                 "correctly.");
        list_addItemWithInfoNote(&_menu_tools,
                                 (ListItem){
                                     .label = "Make miyoogamelist with digest names",
                                     .action = tool_generateMiyoogamelists},
                                 "Use this tool to clean your game names\n"
                                 "without having to rename the rom files\n"
                                 "(removes parens, rankings, and much more).\n"
                                 "This generates a 'miyoogamelist.xml' file\n"
                                 "which comes with some limitations, such\n"
                                 "as no subfolder support.");
        list_addItemWithInfoNote(&_menu_tools,
                                 (ListItem){
                                     .label = "Sort apps: A to Z",
                                     .action = tool_sortAppsAZ},
                                 "Sort the Apps list from A to Z.");
        list_addItemWithInfoNote(&_menu_tools,
                                 (ListItem){
                                     .label = "Sort apps: Z to A",
                                     .action = tool_sortAppsZA},
                                 "Sort the Apps list from Z to A.");
        list_addItem(&_menu_tools,
                     (ListItem){
                         .label = "Screen recorder...",
                         .action = menu_screen_recorder});
    }
    menu_stack[++menu_level] = &_menu_tools;
    header_changed = true;
}

void *_get_menu_icon(const char *name)
{
    char path[STR_MAX * 2] = {0};
    const char *config_path = "/mnt/SDCARD/App/Tweaks/config.json";

    if (is_file(config_path)) {
        cJSON *config = json_load(config_path);
        char icon_path[STR_MAX];
        if (json_getString(config, "icon", icon_path))
            snprintf(path, STR_MAX * 2 - 1, "%s/%s.png", dirname(icon_path),
                     name);
    }

    if (!is_file(path))
        snprintf(path, STR_MAX * 2 - 1, "res/%s.png", name);

    return (void *)IMG_Load(path);
}

// Tweaks > Game lists: everything about Games, Favorites, Recents, Search
// and Game Info.
void menu_gameListsMenu(void *_)
{
    (void)_;
    if (!_menu_game_lists._created) {
        _menu_game_lists = list_createWithTitle(6, LIST_SMALL, "Game lists");
        list_addItemWithInfoNote(&_menu_game_lists,
                                 (ListItem){.label = "Layout...", .action = menu_gameListsLayout},
                                 "Rows per page, font size, sorting and\n"
                                 "the favorite star in game lists.");
        list_addItemWithInfoNote(&_menu_game_lists,
                                 (ListItem){.label = "Scrolling...", .action = menu_gameListsScrolling},
                                 "How long game names scroll, and how a\n"
                                 "held button repeats.");
        list_addItemWithInfoNote(&_menu_game_lists,
                                 (ListItem){.label = "Previews...", .action = menu_gameListsPreviews},
                                 "Video preview in the lists and the Game\n"
                                 "Info text scrolling by itself.");
        list_addItemWithInfoNote(&_menu_game_lists,
                                 (ListItem){.label = "List menus...", .action = menu_listMenus},
                                 "Which actions the SELECT menus of Games,\n"
                                 "Favorites, Recents and Search show, and\n"
                                 "what GLO (the game's options) shows.");
        list_addItemWithInfoNote(&_menu_game_lists,
                                 (ListItem){.label = "Game Info menu...", .action = menu_gameInfoMenu},
                                 "What the Game Info menu (SELECT in Game\n"
                                 "Info) shows: gallery, Saved Games, maps,\n"
                                 "walkthrough, manual.");
        list_addItemWithInfoNote(&_menu_game_lists,
                                 (ListItem){.label = "Saved Games...", .action = menu_savedGames},
                                 "Shortcut, auto save, pictures, and what\n"
                                 "playing an older save does to the last\n"
                                 "auto save.");
        savedgames_update_enabled();
    }
    menu_stack[++menu_level] = &_menu_game_lists;
    header_changed = true;
}

void menu_main(void)
{
    if (!_menu_main._created) {
        _menu_main = list_createWithTitle(8, LIST_LARGE, "Tweaks");
        list_addItem(&_menu_main,
                     (ListItem){
                         .label = "Home screen",
                         .description = "Tabs, consoles, SELECT menu",
                         .action = menu_homeScreen,
                         .icon_ptr = _get_menu_icon("tweaks_user_interface")});
        list_addItem(&_menu_main,
                     (ListItem){
                         .label = "Game lists",
                         .description = "Layout, scrolling, previews, menus",
                         .action = menu_gameListsMenu,
                         .icon_ptr = _get_menu_icon("tweaks_icons")});
        list_addItem(&_menu_main,
                     (ListItem){
                         .label = "Buttons",
                         .description = "MENU, X, Y, POWER, hotkeys",
                         .action = menu_buttonAction,
                         .icon_ptr = _get_menu_icon("tweaks_menu_button")});
        list_addItem(&_menu_main,
                     (ListItem){
                         .label = "Look and feel",
                         .description = "Theme, icons, blue light, hints",
                         .action = menu_lookAndFeel,
                         .icon_ptr = _get_menu_icon("tweaks_user_interface")});
        list_addItem(&_menu_main,
                     (ListItem){
                         .label = "System",
                         .description = "Startup, date and time, battery",
                         .action = menu_system,
                         .icon_ptr = _get_menu_icon("tweaks_system")});
        if (IS_MIYOO_PLUS_OR_FLIP()) {
            list_addItem(&_menu_main,
                         (ListItem){
                             .label = "Network",
                             .description = "Wi-Fi, file sharing, SSH, FTP",
                             .action = menu_network,
                             .icon_ptr = _get_menu_icon("tweaks_network")});
        }
        list_addItem(&_menu_main,
                     (ListItem){
                         .label = "Tools",
                         .description = "CUE, M3U, game lists, recorder",
                         .action = menu_tools,
                         .icon_ptr = _get_menu_icon("tweaks_tools")});
        list_addItem(&_menu_main,
                     (ListItem){
                         .label = "Advanced",
                         .description = "Screen, fast forward, reset",
                         .action = menu_advanced,
                         .icon_ptr = _get_menu_icon("tweaks_advanced")});
    }
    menu_level = 0;
    menu_stack[0] = &_menu_main;
    header_changed = true;
}

void menu_resetAll(void)
{
    int current_state[10][2];
    int current_level = menu_level;
    for (int i = 0; i <= current_level; i++) {
        current_state[i][0] = menu_stack[i]->active_pos;
        current_state[i][1] = menu_stack[i]->scroll_pos;
    }
    menu_free_all();
    menu_main();
    for (int i = 0; i <= current_level; i++) {
        menu_stack[i]->active_pos = current_state[i][0];
        menu_stack[i]->scroll_pos = current_state[i][1];
        if (i < current_level)
            list_activateItem(menu_stack[i]);
    }
    reset_menus = false;
}

#endif
