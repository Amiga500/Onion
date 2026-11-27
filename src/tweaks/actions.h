#ifndef TWEAKS_ACTIONS_H__
#define TWEAKS_ACTIONS_H__

#include "components/list.h"
#include "./formatters_basic.h"
#include "system/axp.h"
#include "system/osd.h"
#include "system/rumble.h"
#include "system/settings.h"
#include "theme/render/footer.h"
#include "theme/resources.h"
#include "theme/sound.h"
#include "utils/apps.h"
#include "utils/config.h"
#include "utils/file.h"
#include "utils/msleep.h"

#include "./appstate.h"
#include "./diags.h"
#include "./reset.h"
#include "./values.h"

void action_setAppShortcut(void *pt)
{
    ListItem *item = (ListItem *)pt;
    int value = item->value;
    char *sett_pt = item->action_id == 0 ? settings.mainui_button_x
                                         : settings.mainui_button_y;
    InstalledApp *apps = getInstalledApps(true);

    memset(sett_pt, 0, JSON_STRING_LEN * sizeof(char));

    if (value == 0)
        return;

    value -= 1;

    if (value < installed_apps_count) {
        strcpy(sett_pt, "app:");
        strncat(sett_pt, apps[value].dirName, JSON_STRING_LEN - 5);
        return;
    }

    value -= installed_apps_count;

    if (value < NUM_TOOLS) {
        strcpy(sett_pt, "tool:");
        strncat(sett_pt, tools_short_names[value], JSON_STRING_LEN - 6);
        return;
    }

    value -= NUM_TOOLS;

    if (value < MAINUI_SHORTCUT_EXTRA_COUNT)
        strcpy(sett_pt, mainui_shortcut_extra_values[value]);
}

void action_meterWidth(void *pt)
{
    config_setNumber("display/meterWidth", ((ListItem *)pt)->value);
    osd_showBrightnessBar(settings.brightness);
}

// 0 Onion's edge bar (default), 1 panel at the bottom, 2 panel at the top.
// Read by keymon each time the indicator appears, so it changes at once.
int value_osdStyle(void)
{
    if (!config_flag_get(OSD_PANEL_FLAG))
        return 0;
    return config_flag_get(OSD_PANEL_TOP_FLAG) ? 2 : 1;
}

void action_setOsdStyle(void *pt)
{
    int value = ((ListItem *)pt)->value;
    osd_hideBar();
    config_flag_set(OSD_PANEL_FLAG, value != 0);
    config_flag_set(OSD_PANEL_TOP_FLAG, value == 2);
    osd_showBrightnessBar(settings.brightness);
}

void action_blueLight(void *pt)
{
    blf_changing = true;

    if (settings.blue_light_state || exists("/tmp/.blfOn")) {
        system("/mnt/SDCARD/.tmp_update/script/blue_light.sh set_default &");
        remove("/tmp/.blfOn");
    }
    else {
        system("/mnt/SDCARD/.tmp_update/script/blue_light.sh enable &");
    }

    settings.blue_light_state = ((ListItem *)pt)->value;
    config_flag_set(".blfOn", ((ListItem *)pt)->value);
    reset_menus = true;
    all_changed = true;
}

void action_blueLightLevel(void *pt)
{
    if (settings.blue_light_state) {
        blf_changing = true;
        reset_menus = true;
    }

    ListItem *item = (ListItem *)pt;

    settings.blue_light_level = item->value;
    config_setNumber("display/blueLightLevel", item->value);

    if (settings.blue_light_state || exists("/tmp/.blfOn")) {
        system("/mnt/SDCARD/.tmp_update/script/blue_light.sh set_intensity &");
    }
}

void action_blueLightSchedule(void *pt)
{
    blf_changing = true;
    reset_menus = true;

    ListItem *item = (ListItem *)pt;
    settings.blue_light_schedule = ((ListItem *)pt)->value;
    config_flag_set(".blf", settings.blue_light_schedule);

    if (item->value == 0) {
        system("/mnt/SDCARD/.tmp_update/script/blue_light.sh set_default &");
        settings.blue_light_state = 0;
        remove("/tmp/.blfOn");
    }
    else {
        system("/mnt/SDCARD/.tmp_update/script/blue_light.sh check &"); // check if we're within the time values and start now
        remove("/tmp/.blfIgnoreSchedule");
    }

    reset_menus = true;
    all_changed = true;
}

void action_blueLightTimeOn(void *pt)
{
    char time_str[10];
    formatter_Time(pt, time_str);
    strcpy(settings.blue_light_time, time_str);
    config_setString("display/blueLightTime", time_str);
}

void action_blueLightTimeOff(void *pt)
{
    char time_str[10];
    formatter_Time(pt, time_str);
    strcpy(settings.blue_light_time_off, time_str);
    config_setString("display/blueLightTimeOff", time_str);
}

void action_setStartupAutoResume(void *pt)
{
    settings.startup_auto_resume = ((ListItem *)pt)->value == 1;
}

void action_setStartupApplication(void *pt)
{
    settings.startup_application = ((ListItem *)pt)->value;
}

void action_setVibration(void *pt)
{
    sound_change();
    skip_next_change = true;
    settings.vibration = ((ListItem *)pt)->value;
    short_pulse();
}

void action_setLowBatteryAutoSave(void *pt)
{
    settings.low_battery_autosave_at = battery_exitLevelPercent(((ListItem *)pt)->value);
}

// Tweaks > Look and feel > Accent colour (Silky RedOnion). Written at once,
// applied once chosen (below): script/accent_color.sh puts the theme
// images in the colour into the theme overrides, then Tweaks loads them
// again and is drawn in it. MainUI and the other programs read them when
// they start.
// Tweaks lists the accent colours in rainbow order; the number saved is the
// colour's line in res/accent/palette.txt (red, blue, purple, teal, pink,
// orange, green, gold, graphite), where new colours are added at the end so
// a saved choice keeps its colour.
static const int ACCENT_ORDER[] = {0, 5, 7, 6, 3, 1, 2, 4, 8};
#define ACCENT_COUNT ((int)(sizeof(ACCENT_ORDER) / sizeof(ACCENT_ORDER[0])))

// The row of Tweaks' list for a saved colour (red for an unknown one)
static int accent_listPosition(int palette_index)
{
    for (int i = 0; i < ACCENT_COUNT; i++)
        if (ACCENT_ORDER[i] == palette_index)
            return i;
    return 0;
}

// Applying a colour (its pictures copied, Tweaks' own reloaded) takes about
// half a second. LEFT/RIGHT step through the colours: it was done at every
// step, then once the colour stayed 500 ms; either way the list stuck
// while choosing (robcodedev, 4.5.1 beta 6: "nothing should happen until
// B"). The choice is only saved while on its page, which stays quick; the
// colour is applied once it is chosen: on leaving the page (B, or another
// page opened from it), and on leaving Tweaks, where only its pictures are
// copied (leaving: true), Tweaks has nothing more to draw. While choosing,
// the colour shows at once where Tweaks draws it (accent_preview). MainUI
// and Kids Mode apply the saved one when they start anyway (runtime.sh).
static bool accent_pending = false;
static List *accent_pending_page = NULL;

void accent_applyPending(bool leaving)
{
    if (!accent_pending)
        return;
    if (!leaving && menu_stack[menu_level] == accent_pending_page)
        return; // still choosing
    accent_pending = false;
    system("sh /mnt/SDCARD/.tmp_update/script/accent_color.sh");
    if (leaving)
        return;
    resources_reloadImages();
    ui_resetColors();
    theme_renderHeader_cleanup();
    theme_renderStandardHint_cleanup();
    theme_renderDialog_cleanup();
    all_changed = true;
}

// The name and accent (first) colour of a palette line
// (res/accent/palette.txt: "name accent light dark", RRGGBB; comment lines
// skipped). False when it is not there.
#define ACCENT_RES "/mnt/SDCARD/.tmp_update/res/accent"
#define ACCENT_PALETTE ACCENT_RES "/palette.txt"
static bool accent_paletteColour(int index, SDL_Color *out, char *out_name)
{
    FILE *fp = fopen(ACCENT_PALETTE, "r");
    if (fp == NULL)
        return false;
    char line[128], name[32];
    unsigned int rgb;
    int n = 0;
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;
        if (n++ != index)
            continue;
        if (sscanf(line, "%31s %6x", name, &rgb) == 2) {
            *out = (SDL_Color){(rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF};
            strcpy(out_name, name);
            found = true;
        }
        break;
    }
    fclose(fp);
    return found;
}

// The A, B and X buttons of the footer in the colour chosen: its own
// small pictures (res/accent/<colour>/skin, the theme's for red), loaded
// in place of the ones in use (a few ms; nothing copied). Tweaks' images
// are all loaded again once the colour is applied (resources_reloadImages).
static void accent_previewButtons(const char *theme_path, const char *colour)
{
    static const struct {
        ThemeImages request;
        const char *rel;
    } buttons[] = {{BUTTON_A, "skin/icon-A-54.png"},
                   {BUTTON_B, "skin/icon-B-54.png"},
                   {BUTTON_X, "skin/extra/icon-X-54.png"}};
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        char path[STR_MAX * 2];
        if (strcmp(colour, "red") == 0)
            snprintf(path, sizeof(path), "%s/%s", theme_path, buttons[i].rel);
        else
            snprintf(path, sizeof(path), ACCENT_RES "/%s/%s", colour, buttons[i].rel);
        if (!exists(path))
            continue;
        SDL_Surface *s = theme_loadImageFile(path);
        if (s == NULL)
            continue;
        if (resources.surfaces[buttons[i].request] != NULL)
            SDL_FreeSurface(resources.surfaces[buttons[i].request]);
        resources.surfaces[buttons[i].request] = s;
    }
    _button_x_absent = false;
}

// The colour shows at once on what Tweaks draws in the accent (the
// selected row, switches, the header badge, SELECT) and on the A, B and X
// buttons: the other pictures follow on leaving the page. Only with Silky
// RedOnion, the theme it changes.
static void accent_preview(int palette_index)
{
    const char *path = theme()->path;
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/')
        len--;
    const char *name = path + len;
    while (name > path && name[-1] != '/')
        name--;
    if ((size_t)(path + len - name) != strlen("Silky RedOnion") ||
        strncmp(name, "Silky RedOnion", path + len - name) != 0)
        return;
    SDL_Color c;
    char colour[32];
    if (accent_paletteColour(palette_index, &c, colour)) {
        ui_previewAccent(c);
        char theme_path[STR_MAX];
        snprintf(theme_path, sizeof(theme_path), "%.*s", (int)len, path);
        accent_previewButtons(theme_path, colour);
        all_changed = true;
    }
}

void action_setAccentColor(void *pt)
{
    int pos = ((ListItem *)pt)->value;
    if (pos < 0 || pos >= ACCENT_COUNT)
        pos = 0;
    config_setNumber("display/accentColor", ACCENT_ORDER[pos]);
    accent_pending = true;
    accent_pending_page = menu_stack[menu_level];
    accent_preview(ACCENT_ORDER[pos]);
}

void action_setLaunchScreen(void *pt)
{
    settings.launch_screen = ((ListItem *)pt)->value;
}

// Written at once, not only when Tweaks closes: the footer of Tweaks and of
// the other RedOnion programs follows the same flag as Open MainUI.
void action_setFooterLayout(void *pt)
{
    settings.mainui_footer_alt = ((ListItem *)pt)->value == 1;
    config_flag_set(".mainuiFooterAltLayout", settings.mainui_footer_alt);
    theme_footerSetAltLayout(settings.mainui_footer_alt);
    footer_changed = true;
}

void action_setLongMenuLabels(void *pt)
{
    settings.mainui_menu_no_ellipsis = ((ListItem *)pt)->value == 1;
    config_flag_set(".noContextMenuEllipsis", settings.mainui_menu_no_ellipsis);
}

void action_setLidCloseAction(void *pt)
{
    settings.lid_close_action = ((ListItem *)pt)->value;
}

void action_setDisableStandby(void *pt)
{
    settings.disable_standby = ((ListItem *)pt)->value == 1;
}

void action_setEnableLogging(void *pt)
{
    settings.enable_logging = ((ListItem *)pt)->value == 1;
    char new_value[22];
    sprintf(new_value, "log_to_file = %s", settings.enable_logging ? "\"true\"" : "\"false\"");
    file_changeKeyValue(RETROARCH_CONFIG, "log_to_file =", new_value);
}

void action_runDiagnosticScript(void *payload_ptr)
{ // run the script based on what the payload_ptr gives us
    ListItem *item = (ListItem *)payload_ptr;
    char *filename = (char *)item->payload_ptr;
    char script_path[DIAG_MAX_PATH_LENGTH + 1];
    snprintf(script_path, sizeof(script_path), "%s/%s", DIAG_SCRIPT_PATH, filename);

    pthread_t thread;
    if (pthread_create(&thread, NULL, diags_runScript, payload_ptr) != 0) {
        list_updateStickyNote(item, "Failed to run script..."); // threading issues
        list_changed = true;
    }

    pthread_detach(thread);
}

void action_setMenuButtonHaptics(void *pt)
{
    settings.menu_button_haptics = ((ListItem *)pt)->value == 1;
}

void action_setMenuButtonKeymap(void *pt)
{
    ListItem *item = (ListItem *)pt;
    static int *dests[] = {
        &settings.mainui_single_press, &settings.mainui_long_press,
        &settings.mainui_double_press, &settings.ingame_single_press,
        &settings.ingame_long_press, &settings.ingame_double_press};
    *(dests[item->action_id]) = item->value;
}

void action_batteryPercentageVisible(void *pt)
{
    static bool applied_values[] = {false, false, true};
    applied_values[0] = resources.theme_back.batteryPercentage.visible;
    int item_value = ((ListItem *)pt)->value;
    resources.theme.batteryPercentage.visible = applied_values[item_value];

    static int value_types[] = {cJSON_NULL, cJSON_False, cJSON_True};
    theme_changeOverride("batteryPercentage", "visible", NULL,
                         value_types[item_value]);

    battery_changed = true;
}

void action_batteryPercentageFontFamily(void *pt)
{
    int item_value = ((ListItem *)pt)->value;
    char theme_value[JSON_STRING_LEN];
    strcpy(theme_value, resources.theme_back.batteryPercentage.font);

    if (item_value == 0) {
        strcpy(resources.theme.batteryPercentage.font, theme_value);
    }
    else {
        char font_path[JSON_STRING_LEN] = "/mnt/SDCARD/miyoo/app/";
        strcat(font_path, font_families[item_value - 1]);
        strcpy(resources.theme.batteryPercentage.font, font_path);
    }

    theme_changeOverride("batteryPercentage", "font",
                         resources.theme.batteryPercentage.font,
                         item_value == 0 ? cJSON_NULL : cJSON_String);
    resource_reloadFont(BATTERY);
    battery_changed = true;
}

void action_batteryPercentageFontSize(void *pt)
{
    int theme_value = resources.theme_back.batteryPercentage.size;
    int item_value = ((ListItem *)pt)->value;
    int new_value = item_value == 0 ? theme_value : font_sizes[item_value - 1];
    resources.theme.batteryPercentage.size = new_value;

    theme_changeOverride("batteryPercentage", "size", &new_value,
                         item_value == 0 ? cJSON_NULL : cJSON_Number);
    resource_reloadFont(BATTERY);
    battery_changed = true;
}

void action_batteryPercentagePosition(void *pt)
{
    static Theme_TextAlign applied_values[] = {LEFT, LEFT, CENTER, RIGHT};
    applied_values[0] = resources.theme_back.batteryPercentage.textAlign;
    int item_value = ((ListItem *)pt)->value;
    resources.theme.batteryPercentage.textAlign = applied_values[item_value];

    theme_changeOverride("batteryPercentage", "onleft", NULL, cJSON_NULL);

    static char *value_types[] = {"", "left", "center", "right"};
    if (item_value != 0)
        theme_changeOverride("batteryPercentage", "textAlign", value_types[item_value], cJSON_String);
    else
        theme_changeOverride("batteryPercentage", "textAlign", NULL, cJSON_NULL);

    battery_changed = true;
}

void action_batteryPercentageFixed(void *pt)
{
    static bool applied_values[] = {false, false, true};
    applied_values[0] = resources.theme_back.batteryPercentage.fixed;
    int item_value = ((ListItem *)pt)->value;
    resources.theme.batteryPercentage.fixed = applied_values[item_value];

    static int value_types[] = {cJSON_NULL, cJSON_False, cJSON_True};
    theme_changeOverride("batteryPercentage", "fixed", NULL, value_types[item_value]);

    battery_changed = true;
}

void action_batteryPercentageOffsetX(void *pt)
{
    int theme_value = resources.theme_back.batteryPercentage.offsetX;
    int item_value = ((ListItem *)pt)->value;
    int new_value = item_value == 0 ? theme_value : item_value - 1 - BATTPERC_MAX_OFFSET;
    resources.theme.batteryPercentage.offsetX = new_value;

    theme_changeOverride("batteryPercentage", "offsetX", &new_value, item_value == 0 ? cJSON_NULL : cJSON_Number);

    battery_changed = true;
}

void action_batteryPercentageOffsetY(void *pt)
{
    int theme_value = resources.theme_back.batteryPercentage.offsetY;
    int item_value = ((ListItem *)pt)->value;
    int new_value = item_value == 0 ? theme_value : item_value - 1 - BATTPERC_MAX_OFFSET;
    resources.theme.batteryPercentage.offsetY = new_value;

    theme_changeOverride("batteryPercentage", "offsetY", &new_value, item_value == 0 ? cJSON_NULL : cJSON_Number);

    battery_changed = true;
}

void action_hideLabelsIcons(void *pt)
{
    static bool applied_values[] = {false, false, true};
    applied_values[0] = resources.theme_back.hideLabels.icons;
    int item_value = ((ListItem *)pt)->value;
    resources.theme.hideLabels.icons = applied_values[item_value];

    static int value_types[] = {cJSON_NULL, cJSON_False, cJSON_True};
    theme_changeOverride("hideLabels", "icons", NULL, value_types[item_value]);
}

void action_toggleBackgroundMusic(void *pt)
{
    settings.bgm_mute = ((ListItem *)pt)->value == 1;
};

void action_hideLabelsHints(void *pt)
{
    static bool applied_values[] = {false, false, true};
    applied_values[0] = resources.theme_back.hideLabels.hints;
    int item_value = ((ListItem *)pt)->value;
    resources.theme.hideLabels.hints = applied_values[item_value];

    static int value_types[] = {cJSON_NULL, cJSON_False, cJSON_True};
    theme_changeOverride("hideLabels", "hints", NULL, value_types[item_value]);

    footer_changed = true;
}

void action_setShowRecents(void *pt)
{
    settings.show_recents = ((ListItem *)pt)->value == 1;
}

void action_setShowExpert(void *pt)
{
    settings.show_expert = ((ListItem *)pt)->value == 1;
}

void action_setLowBatteryWarnAt(void *pt)
{
    settings.low_battery_warn_at = ((ListItem *)pt)->value * 5;
    config_setNumber("battery/warnAt", settings.low_battery_warn_at);
}

void action_setStartupTab(void *pt)
{
    settings.startup_tab = ((ListItem *)pt)->value;
}

void action_setTimeSkip(void *pt)
{
    settings.time_skip = ((ListItem *)pt)->value;
}

void action_advancedSetFrameThrottle(void *pt)
{
    int item_value = ((ListItem *)pt)->value;
    stored_value_frame_throttle = item_value;
    stored_value_frame_throttle_changed = true;
}

void action_advancedSetPWMFreqency(void *pt)
{
    FILE *fp;
    int item_value = ((ListItem *)pt)->value;
    int pwmfrequency = atoi(((ListItem *)pt)->value_labels[item_value]);
    char *filename = "/sys/class/pwm/pwmchip0/pwm0/period";
    file_put(fp, filename, "%d", pwmfrequency);
    config_setNumber(".pwmfrequency", item_value);
}

void action_advancedSetSwapTriggers(void *pt)
{
    int item_value = ((ListItem *)pt)->value;
    stored_value_swap_triggers = item_value;
    stored_value_swap_triggers_changed = true;
}

void action_setCpuClockHotkey(void *pt)
{
    config_flag_set(".cpuClockHotkey", ((ListItem *)pt)->value);
}

void action_setAltBrightness(void *pt)
{
    config_flag_set(".altBrightness", ((ListItem *)pt)->value);
}

void action_toggleScreenRecIndicator(void *pt)
{
    config_flag_set(".recIndicator", ((ListItem *)pt)->value);
    settings.rec_indicator = ((ListItem *)pt)->value == 1;
}

void action_toggleScreenRecCountdown(void *pt)
{
    config_setNumber("recCountdown", ((ListItem *)pt)->value);
    settings.rec_countdown = ((ListItem *)pt)->value;
}

void action_toggleScreenRecHotkey(void *pt)
{
    config_flag_set(".recHotkey", ((ListItem *)pt)->value);
    settings.rec_hotkey = ((ListItem *)pt)->value == 1;
}

void action_hardKillFFmpeg(void *pt)
{
    ListItem *item = (ListItem *)pt;
    int status = system("/mnt/SDCARD/.tmp_update/script/screen_recorder.sh hardkill");
    if (status != 0) {
        list_updateStickyNote(item, "Status: Error occurred.");
    }
    else {
        list_updateStickyNote(item, "Status: FFmpeg process stopped");
    }
    list_changed = true;
}

void action_deleteAllRecordings(void *pt)
{
    ListItem *item = (ListItem *)pt;
    int fileCheck;
    fileCheck = exists("/tmp/recorder_active");

    if (fileCheck) {
        if (!_disable_confirm && !_confirmReset("Recording!", "You're still recording!\nAre you sure you want to\ndelete all recordings?")) {
            return;
        }
        else {
            system("/mnt/SDCARD/.tmp_update/script/screen_recorder.sh hardkill &");
            strncpy(_menu_screen_recorder.items[0].sticky_note, "Status: Idle.", sizeof(_menu_screen_recorder.items[0].sticky_note) - 1);
            _menu_screen_recorder.items[0].sticky_note[sizeof(_menu_screen_recorder.items[0].sticky_note) - 1] = '\0';
        }
    }
    else {
        if (!_disable_confirm && !_confirmReset("Delete?", "Are you sure you want to\ndelete all recordings?")) {
            list_updateStickyNote(item, "Cancelled");
            return;
        }
    }

    system("rm -f /mnt/SDCARD/Media/Videos/Recorded/*.mp4");
    list_updateStickyNote(item, "Recorded directory emptied!");
    if (!_disable_confirm)
        _notifyResetDone("Deleted!");
}

void action_advancedSetLcdVoltage(void *pt)
{
    int value = 0x0e - ((ListItem *)pt)->value;

    if (value == 0x0e) {
        axp_lcd_set(0x0e);
        config_flag_set(".lcdvolt", false);
        return;
    }

    if (value < 0x09 || value > 0x0e) {
        config_flag_set(".lcdvolt", false);
        return;
    }

    int res = axp_lcd_set(value);

    if (res != 0) {
        printf_debug("Error: Failed to set LCD voltage: %d\n",
                     1600 + value * 100);
        config_flag_set(".lcdvolt", false);
        msleep(200);
        return;
    }

    printf_debug("LCD voltage set to: %d\n", 1600 + value * 100);

    config_flag_set(".lcdvolt", true);

    FILE *fp;
    file_put(fp, LCD_VOLT_CONFIG, "%x", 0x0e);
}

const char *action_LaunchKeyboardWrapper(const char *initial_value, const char *title)
{
    const char *result = launch_keyboard(initial_value, title);
    all_changed = true;
    return result;
}

#endif // TWEAKS_ACTIONS_H__
