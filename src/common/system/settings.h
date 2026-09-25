#ifndef SETTINGS_H__
#define SETTINGS_H__

#include <stdbool.h>

#include "display.h"
#include "system/volume.h"
#include "utils/config.h"
#include "utils/file.h"
#include "utils/json.h"

#define MAX_BRIGHTNESS 10
#define MAIN_UI_SETTINGS "/mnt/SDCARD/system.json"
#define CMD_TO_RUN_PATH "/mnt/SDCARD/.tmp_update/cmd_to_run.sh"
#define RETROARCH_CONFIG "/mnt/SDCARD/RetroArch/.retroarch/retroarch.cfg"
#define HISTORY_PATH \
    "/mnt/SDCARD/Saves/CurrentProfile/lists/content_history.lpl"
#define RECENTLIST_PATH "/mnt/SDCARD/Roms/recentlist.json"
#define RECENTLIST_HIDDEN_PATH "/mnt/SDCARD/Roms/recentlist-hidden.json"
#define RECENTLISTMIGRATED "/mnt/SDCARD/Saves/CurrentProfile/config/.recentListMigrated"
#define DEFAULT_THEME_PATH "/mnt/SDCARD/Themes/Silky by DiMo/"
#define RECORDED_DIR "/mnt/SDCARD/Media/Videos/Recorded"

typedef struct settings_s {
    int volume;
    char keymap[JSON_STRING_LEN];
    int mute;
    bool bgm_mute;
    int bgm_volume;
    int brightness;
    char language[JSON_STRING_LEN];
    int sleep_timer;
    int lumination;
    int hue;
    int saturation;
    int contrast;
    int wifi_on;
    char theme[JSON_STRING_LEN];
    int fontsize;
    int audiofix;
    bool show_recents;
    bool show_expert;
    bool startup_auto_resume;
    bool menu_button_haptics;
    int low_battery_autosave_at;
    int low_battery_warn_at;
    int time_skip;
    int vibration;
    int startup_tab;
    int startup_application;
    int mainui_single_press;
    int mainui_long_press;
    int mainui_double_press;
    int ingame_single_press;
    int ingame_long_press;
    int ingame_double_press;
    bool disable_standby;
    int pwmfrequency;
    bool enable_logging;
    bool rec_indicator;
    bool rec_hotkey;
    int rec_countdown;
    int blue_light_state;
    int blue_light_schedule;
    int blue_light_level;
    int blue_light_rgb;
    char blue_light_time[16];
    char blue_light_time_off[16];
    bool rtc_available;
    int lid_close_action;

    char mainui_button_x[JSON_STRING_LEN];
    char mainui_button_y[JSON_STRING_LEN];
} settings_s;

static bool settings_loaded = false;
static settings_s settings;
static settings_s __settings;
// true when __settings mirrors what is on disk (after a load or a save)
static bool __settings_snapshot_valid = false;
static settings_s __default_settings = (settings_s){
    // MainUI settings
    .volume = 20,
    .keymap = "L2,L,R2,R,X,A,B,Y",
    .mute = 0,
    .bgm_volume = 20,
    .brightness = 7,
    .language = "en.lang",
    .sleep_timer = 5,
    .lumination = 7,
    .hue = 10,
    .saturation = 10,
    .contrast = 10,
    .theme = DEFAULT_THEME_PATH,
    .fontsize = 24,
    .audiofix = 1,
    .wifi_on = 0,
    // Onion settings
    .bgm_mute = false,
    .show_recents = false,
    .show_expert = false,
    .startup_auto_resume = true,
    .menu_button_haptics = false,
    .low_battery_autosave_at = 4,
    .low_battery_warn_at = 10,
    .time_skip = 4,
    .vibration = 2,
    .startup_tab = 0,
    .startup_application = 0,
    .rtc_available = false,
    // Menu button actions
    .mainui_single_press = 1,
    .mainui_long_press = 0,
    .mainui_double_press = 2,
    .ingame_single_press = 1,
    .ingame_long_press = 2,
    .ingame_double_press = 3,
    .disable_standby = false,
    .enable_logging = false,
    .blue_light_state = false,
    .blue_light_schedule = false,
    .blue_light_level = 0,
    .blue_light_rgb = 8421504,
    .blue_light_time = "20:00",
    .blue_light_time_off = "08:00",
    .pwmfrequency = 7,
    .lid_close_action = 0,
    .mainui_button_x = "",
    .mainui_button_y = "",
    //utility
    .rec_countdown = false,
    .rec_indicator = false,
    .rec_hotkey = false};

void _settings_clone(settings_s *dst, settings_s *src)
{
    *dst = *src;
    strncpy(dst->keymap, src->keymap, sizeof(dst->keymap) - 1);
    dst->keymap[sizeof(dst->keymap) - 1] = '\0';
    strncpy(dst->language, src->language, sizeof(dst->language) - 1);
    dst->language[sizeof(dst->language) - 1] = '\0';
    strncpy(dst->theme, src->theme, sizeof(dst->theme) - 1);
    dst->theme[sizeof(dst->theme) - 1] = '\0';
    strncpy(dst->mainui_button_x, src->mainui_button_x, sizeof(dst->mainui_button_x) - 1);
    dst->mainui_button_x[sizeof(dst->mainui_button_x) - 1] = '\0';
    strncpy(dst->mainui_button_y, src->mainui_button_y, sizeof(dst->mainui_button_y) - 1);
    dst->mainui_button_y[sizeof(dst->mainui_button_y) - 1] = '\0';
}

void _settings_reset(settings_s *_settings)
{
    _settings_clone(_settings, &__default_settings);
}

void _settings_load_keymap(void)
{
    if (!exists(CONFIG_PATH "keymap.json"))
        return;

    cJSON *keymap = json_load(CONFIG_PATH "keymap.json");
    if (keymap == NULL)
        return;
    json_getInt(keymap, "mainui_single_press", &settings.mainui_single_press);
    json_getInt(keymap, "mainui_long_press", &settings.mainui_long_press);
    json_getInt(keymap, "mainui_double_press", &settings.mainui_double_press);
    json_getInt(keymap, "ingame_single_press", &settings.ingame_single_press);
    json_getInt(keymap, "ingame_long_press", &settings.ingame_long_press);
    json_getInt(keymap, "ingame_double_press", &settings.ingame_double_press);
    json_getString(keymap, "mainui_button_x", settings.mainui_button_x);
    json_getString(keymap, "mainui_button_y", settings.mainui_button_y);
    cJSON_Delete(keymap);
}

void _settings_load_mainui(void)
{
    char *json_str = NULL;

    if (!(json_str = file_read(MAIN_UI_SETTINGS)))
        return;

    cJSON *json_root = cJSON_Parse(json_str);
    free(json_str);
    if (json_root == NULL)
        return;

    json_getInt(json_root, "vol", &settings.volume);
    json_getInt(json_root, "bgmvol", &settings.bgm_volume);
    json_getInt(json_root, "brightness", &settings.brightness);
    json_getInt(json_root, "hibernate", &settings.sleep_timer);
    json_getInt(json_root, "lumination", &settings.lumination);
    json_getInt(json_root, "hue", &settings.hue);
    json_getInt(json_root, "saturation", &settings.saturation);
    json_getInt(json_root, "contrast", &settings.contrast);
    json_getInt(json_root, "fontsize", &settings.fontsize);
    json_getInt(json_root, "audiofix", &settings.audiofix);
    json_getInt(json_root, "wifi", &settings.wifi_on);

    json_getString(json_root, "keymap", settings.keymap);
    json_getString(json_root, "language", settings.language);
    json_getString(json_root, "theme", settings.theme);

    if (strcmp(settings.theme, "./") == 0) {
        strncpy(settings.theme, DEFAULT_THEME_PATH, sizeof(settings.theme) - 1);
        settings.theme[sizeof(settings.theme) - 1] = '\0';
    }

    cJSON_Delete(json_root);
}

void settings_load(void)
{
    _settings_reset(&settings);

    settings.startup_auto_resume = !config_flag_get(".noAutoStart");
    settings.menu_button_haptics = !config_flag_get(".noMenuHaptics");
    settings.bgm_mute = config_flag_get(".bgmMute");
    settings.show_recents = config_flag_get(".showRecents");
    settings.show_expert = config_flag_get(".showExpert");
    settings.mute = config_flag_get(".muteVolume");
    settings.disable_standby = config_flag_get(".disableStandby");
    settings.enable_logging = config_flag_get(".logging");
    settings.blue_light_state = config_flag_get(".blfOn");
    settings.blue_light_schedule = config_flag_get(".blf");
    settings.rec_indicator = config_flag_get(".recIndicator");
    settings.rec_hotkey = config_flag_get(".recHotkey");
    settings.rtc_available = temp_flag_get("rtc_available");

    if (config_flag_get(".noLowBatteryAutoSave")) // flag is deprecated, but keep compatibility
        settings.low_battery_autosave_at = 0;

    if (config_flag_get(".noBatteryWarning")) // flag is deprecated, but keep compatibility
        settings.low_battery_warn_at = 0;

    if (config_flag_get(".noVibration")) // flag is deprecated, but keep compatibility
        settings.vibration = 0;

    config_get("battery/warnAt", CONFIG_INT, &settings.low_battery_warn_at);
    config_get("battery/exitAt", CONFIG_INT, &settings.low_battery_autosave_at);
    config_get("startup/app", CONFIG_INT, &settings.startup_application);
    config_get("startup/addHours", CONFIG_INT, &settings.time_skip);
    config_get("vibration", CONFIG_INT, &settings.vibration);
    config_get("startup/tab", CONFIG_INT, &settings.startup_tab);
    config_get("display/blueLightLevel", CONFIG_INT, &settings.blue_light_level);
    {
        char _tmp_time[STR_MAX] = "";
        if (config_get("display/blueLightTime", CONFIG_STR, _tmp_time)) {
            strncpy(settings.blue_light_time, _tmp_time, sizeof(settings.blue_light_time) - 1);
            settings.blue_light_time[sizeof(settings.blue_light_time) - 1] = '\0';
        }
        if (config_get("display/blueLightTimeOff", CONFIG_STR, _tmp_time)) {
            strncpy(settings.blue_light_time_off, _tmp_time, sizeof(settings.blue_light_time_off) - 1);
            settings.blue_light_time_off[sizeof(settings.blue_light_time_off) - 1] = '\0';
        }
    }
    config_get("display/blueLightRGB", CONFIG_INT, &settings.blue_light_rgb);
    config_get("pwmfrequency", CONFIG_INT, &settings.pwmfrequency);
    config_get("recCountdown", CONFIG_INT, &settings.rec_countdown);
    config_get("flip/lidCloseAction", CONFIG_INT, &settings.lid_close_action);

    if (config_flag_get(".menuInverted")) { // flag is deprecated, but keep compatibility
        settings.ingame_single_press = 2;
        settings.ingame_long_press = 1;
    }

    if (config_flag_get(".noGameSwitcher")) { // flag is deprecated, but keep compatibility
        settings.mainui_single_press = 0;
        settings.ingame_single_press = 2;
        settings.ingame_long_press = 0;
    }

    _settings_load_keymap();
    _settings_load_mainui();

    _settings_clone(&__settings, &settings);
    __settings_snapshot_valid = true;

    settings_loaded = true;
}

bool _settings_dirty_keymap(void)
{
    return settings.mainui_single_press != __settings.mainui_single_press ||
           settings.mainui_long_press != __settings.mainui_long_press ||
           settings.mainui_double_press != __settings.mainui_double_press ||
           settings.ingame_single_press != __settings.ingame_single_press ||
           settings.ingame_long_press != __settings.ingame_long_press ||
           settings.ingame_double_press != __settings.ingame_double_press ||
           strcmp(settings.mainui_button_x, __settings.mainui_button_x) != 0 ||
           strcmp(settings.mainui_button_y, __settings.mainui_button_y) != 0;
}

void _settings_save_keymap(void)
{
    FILE *fp;
    char tmp_path[PATH_MAX];
    char final_path[PATH_MAX];

    if ((fp = file_atomic_begin(CONFIG_PATH "keymap.json", tmp_path, sizeof(tmp_path),
                                final_path, sizeof(final_path))) == NULL)
        return;

    fprintf(fp, "{\n");
    fprintf(fp, JSON_FORMAT_NUMBER, "mainui_single_press",
            settings.mainui_single_press);
    fprintf(fp, JSON_FORMAT_NUMBER, "mainui_long_press",
            settings.mainui_long_press);
    fprintf(fp, JSON_FORMAT_NUMBER, "mainui_double_press",
            settings.mainui_double_press);
    fprintf(fp, JSON_FORMAT_NUMBER, "ingame_single_press",
            settings.ingame_single_press);
    fprintf(fp, JSON_FORMAT_NUMBER, "ingame_long_press",
            settings.ingame_long_press);
    fprintf(fp, JSON_FORMAT_NUMBER, "ingame_double_press",
            settings.ingame_double_press);
    fprintf(fp, JSON_FORMAT_STRING, "mainui_button_x",
            settings.mainui_button_x);
    fprintf(fp, JSON_FORMAT_STRING_NC, "mainui_button_y",
            settings.mainui_button_y);
    fprintf(fp, "}\n");

    file_atomic_commit(fp, tmp_path, final_path);
}

bool _settings_dirty_mainui(void)
{
    return settings.volume != __settings.volume ||
           strcmp(settings.keymap, __settings.keymap) != 0 ||
           settings.mute != __settings.mute ||
           settings.bgm_volume != __settings.bgm_volume ||
           settings.brightness != __settings.brightness ||
           strcmp(settings.language, __settings.language) != 0 ||
           settings.sleep_timer != __settings.sleep_timer ||
           settings.lumination != __settings.lumination ||
           settings.hue != __settings.hue ||
           settings.saturation != __settings.saturation ||
           settings.contrast != __settings.contrast ||
           strcmp(settings.theme, __settings.theme) != 0 ||
           settings.fontsize != __settings.fontsize ||
           settings.audiofix != __settings.audiofix ||
           settings.wifi_on != __settings.wifi_on;
}

void _settings_save_mainui_force(void)
{
    FILE *fp;
    char tmp_path[PATH_MAX];
    char final_path[PATH_MAX];

    if ((fp = file_atomic_begin(MAIN_UI_SETTINGS, tmp_path, sizeof(tmp_path),
                                final_path, sizeof(final_path))) == NULL)
        return;

    fprintf(fp, "{\n");
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "vol", settings.volume);
    fprintf(fp, JSON_FORMAT_TAB_STRING, "keymap", settings.keymap);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "mute", settings.mute);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "bgmvol", settings.bgm_volume);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "brightness", settings.brightness);
    fprintf(fp, JSON_FORMAT_TAB_STRING, "language", settings.language);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "hibernate", settings.sleep_timer);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "lumination", settings.lumination);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "hue", settings.hue);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "saturation", settings.saturation);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "contrast", settings.contrast);
    fprintf(fp, JSON_FORMAT_TAB_STRING, "theme", settings.theme);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "fontsize", settings.fontsize);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER, "audiofix", settings.audiofix);
    fprintf(fp, JSON_FORMAT_TAB_NUMBER_NC, "wifi", settings.wifi_on);
    fprintf(fp, "}");

    if (!file_atomic_commit(fp, tmp_path, final_path))
        return;

    // system.json on disk now matches these fields: keep the snapshot in
    // sync so the dirty check compares against what is really stored.
    __settings.volume = settings.volume;
    __settings.mute = settings.mute;
    __settings.bgm_volume = settings.bgm_volume;
    __settings.brightness = settings.brightness;
    __settings.sleep_timer = settings.sleep_timer;
    __settings.lumination = settings.lumination;
    __settings.hue = settings.hue;
    __settings.saturation = settings.saturation;
    __settings.contrast = settings.contrast;
    __settings.fontsize = settings.fontsize;
    __settings.audiofix = settings.audiofix;
    __settings.wifi_on = settings.wifi_on;
    snprintf(__settings.keymap, sizeof(__settings.keymap), "%s", settings.keymap);
    snprintf(__settings.language, sizeof(__settings.language), "%s", settings.language);
    snprintf(__settings.theme, sizeof(__settings.theme), "%s", settings.theme);
}

void _settings_save_mainui(void)
{
    if (!_settings_dirty_mainui()) {
        print_debug("Skipped saving system.json (not dirty)");
        return;
    }
    _settings_save_mainui_force();
}

// Write a flag only when it changed since the last load/save, or when the
// file that should represent it on disk is missing.
static void _settings_save_flag(const char *key, bool value, bool old_value, bool force)
{
    if (!force && value == old_value) {
        char path[STR_MAX];
        snprintf(path, sizeof(path), "%s%s%s", CONFIG_PATH, key, value ? "" : "_");
        if (exists(path))
            return;
    }
    config_flag_set(key, value);
}

static void _settings_save_number(const char *key, int value, int old_value, bool force)
{
    if (!force && value == old_value) {
        char path[STR_MAX];
        concat(path, CONFIG_PATH, key);
        if (exists(path))
            return;
    }
    config_setNumber(key, value);
}

static void _settings_save_string(const char *key, const char *value, const char *old_value, bool force)
{
    if (!force && strcmp(value, old_value) == 0) {
        char path[STR_MAX];
        concat(path, CONFIG_PATH, key);
        if (exists(path))
            return;
    }
    config_setString(key, value);
}

static bool _settings_deprecated_cleaned = false;

// Selective save: only what differs from the on-disk snapshot is written.
// A volume step used to rewrite ~30 files with 13 fsyncs; now it touches
// system.json only.
void _settings_save_impl(bool notify)
{
    const settings_s *old = &__settings;
    bool force = !__settings_snapshot_valid;

    // Deprecated flags are folded into the current values at load time.
    // If any is still on disk, write everything once before deleting them,
    // otherwise the migrated values would be lost on the next load.
    if (!_settings_deprecated_cleaned &&
        (exists(CONFIG_PATH ".noLowBatteryAutoSave") ||
         exists(CONFIG_PATH ".noBatteryWarning") ||
         exists(CONFIG_PATH ".noVibration") ||
         exists(CONFIG_PATH ".menuInverted") ||
         exists(CONFIG_PATH ".noGameSwitcher")))
        force = true;

    _settings_save_flag(".noAutoStart", !settings.startup_auto_resume, !old->startup_auto_resume, force);
    _settings_save_flag(".noMenuHaptics", !settings.menu_button_haptics, !old->menu_button_haptics, force);
    _settings_save_flag(".bgmMute", settings.bgm_mute, old->bgm_mute, force);
    _settings_save_flag(".showRecents", settings.show_recents, old->show_recents, force);
    _settings_save_flag(".showExpert", settings.show_expert, old->show_expert, force);
    _settings_save_flag(".muteVolume", settings.mute, old->mute, force);
    _settings_save_flag(".disableStandby", settings.disable_standby, old->disable_standby, force);
    _settings_save_flag(".logging", settings.enable_logging, old->enable_logging, force);
    _settings_save_flag(".blfOn", settings.blue_light_state, old->blue_light_state, force);
    _settings_save_flag(".blf", settings.blue_light_schedule, old->blue_light_schedule, force);
    _settings_save_flag(".recIndicator", settings.rec_indicator, old->rec_indicator, force);
    _settings_save_flag(".recHotkey", settings.rec_hotkey, old->rec_hotkey, force);
    _settings_save_number("battery/warnAt", settings.low_battery_warn_at, old->low_battery_warn_at, force);
    _settings_save_number("battery/exitAt", settings.low_battery_autosave_at, old->low_battery_autosave_at, force);
    _settings_save_number("startup/app", settings.startup_application, old->startup_application, force);
    _settings_save_number("startup/addHours", settings.time_skip, old->time_skip, force);
    _settings_save_number("vibration", settings.vibration, old->vibration, force);
    _settings_save_number("startup/tab", settings.startup_tab, old->startup_tab, force);
    _settings_save_number("recCountdown", settings.rec_countdown, old->rec_countdown, force);
    _settings_save_number("display/blueLightLevel", settings.blue_light_level, old->blue_light_level, force);
    _settings_save_number("display/blueLightRGB", settings.blue_light_rgb, old->blue_light_rgb, force);
    _settings_save_string("display/blueLightTime", settings.blue_light_time, old->blue_light_time, force);
    _settings_save_string("display/blueLightTimeOff", settings.blue_light_time_off, old->blue_light_time_off, force);
    _settings_save_number("flip/lidCloseAction", settings.lid_close_action, old->lid_close_action, force);
    _settings_save_number("pwmfrequency", settings.pwmfrequency, old->pwmfrequency, force);

    // remove deprecated flags (once per process is enough)
    if (!_settings_deprecated_cleaned) {
        remove(CONFIG_PATH ".noLowBatteryAutoSave");
        remove(CONFIG_PATH ".noBatteryWarning");
        remove(CONFIG_PATH ".noVibration");
        remove(CONFIG_PATH ".menuInverted");
        remove(CONFIG_PATH ".noGameSwitcher");
        _settings_deprecated_cleaned = true;
    }

    if (force || _settings_dirty_keymap() || !exists(CONFIG_PATH "keymap.json"))
        _settings_save_keymap();

    if (force || !exists(MAIN_UI_SETTINGS))
        _settings_save_mainui_force();
    else
        _settings_save_mainui();

    // Disk now matches memory: later saves only write what changes next.
    _settings_clone(&__settings, &settings);
    __settings_snapshot_valid = true;

    if (notify)
        temp_flag_set("settings_changed", true);
}

// Save and notify keymon (it reloads settings when it sees the flag).
void settings_save(void)
{
    _settings_save_impl(true);
}

// Save without notifying: for keymon itself, which already holds the
// current values and would otherwise reload its own write from disk.
void settings_save_local(void)
{
    _settings_save_impl(false);
}

bool settings_saveSystemProperty(const char *prop_name, int value)
{
    cJSON *json_root = json_load(MAIN_UI_SETTINGS);
    if (json_root == NULL)
        return false;
    cJSON *prop = cJSON_GetObjectItem(json_root, prop_name);
    if (prop == NULL) {
        cJSON_Delete(json_root);
        return false;
    }

    if (cJSON_GetNumberValue(prop) == value) {
        cJSON_Delete(json_root);
        return false;
    }

    cJSON_SetNumberValue(prop, value);
    json_save(json_root, MAIN_UI_SETTINGS);
    cJSON_Delete(json_root);
    temp_flag_set("settings_changed", true);

    return true;
}

void settings_setBrightness(uint32_t value, bool apply, bool save)
{
    settings.brightness = value;

    if (apply)
        display_setBrightness(settings.brightness);

    if (save)
        settings_saveSystemProperty("brightness", settings.brightness);
}

bool settings_setVolume(int value, bool apply)
{
    bool changed = false;

    if (value > 20)
        value = 20;
    else if (value < 0)
        value = 0;

    if (settings.volume != value) {
        settings.volume = value;
        changed = true;
    }

    if (apply)
        setVolume(settings.mute ? 0 : settings.volume);

    return changed;
}

bool settings_setMute(uint32_t value, bool apply)
{
    bool changed = false;

    if (settings.mute != value) {
        settings.mute = value;
        changed = true;
    }

    if (apply)
        setVolume(settings.mute ? 0 : settings.volume);

    return changed;
}

#endif // SETTINGS_H__
