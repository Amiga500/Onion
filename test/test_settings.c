/**
 * @file test_settings.c
 * @brief Unit tests for src/common/system/settings.h
 *
 * Tests the pure-logic settings functions: _settings_clone,
 * _settings_reset, _settings_dirty_mainui, settings_setVolume,
 * and settings_setMute.  Hardware-dependent functions (display,
 * audio ioctl) are stubbed out.
 *
 * Build and run: make -f Makefile.unit test_settings
 */

#include "onion_test.h"
#include <stdlib.h>
#include <string.h>

/* ---- Stub out hardware-dependent headers ---- */

/* Provide JSON_STRING_LEN which settings.h needs for struct fields */
#define JSON_STRING_LEN 256

/* Stub display_setBrightness (used by settings_setBrightness) */
static int _stub_brightness_value __attribute__((unused)) = -1;
static void __attribute__((unused)) display_setBrightness(int value) { _stub_brightness_value = value; }

/* Stub setVolume (used by settings_setVolume / settings_setMute) */
static int _stub_volume_value = -1;
static int setVolume(int value) { _stub_volume_value = value; return value; }

/* Guard out the real headers that need hardware */
#define DISPLAY_H__
#define VOLUME_H__
#define CONFIG_H__
#define JSON_H__
#define SYSTEM_H__

/* Provide MAX_BRIGHTNESS used by settings.h */
#define MAX_BRIGHTNESS 10

/* ---- Inline the settings_s struct and pure-logic functions ---- */

typedef struct settings_s {
    int volume;
    char keymap[JSON_STRING_LEN];
    int mute;
    int bgm_mute; /* using int to match the bool in source */
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
    int show_recents;
    int show_expert;
    int startup_auto_resume;
    int menu_button_haptics;
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
    int disable_standby;
    int pwmfrequency;
    int enable_logging;
    int rec_indicator;
    int rec_hotkey;
    int rec_countdown;
    int blue_light_state;
    int blue_light_schedule;
    int blue_light_level;
    int blue_light_rgb;
    char blue_light_time[16];
    char blue_light_time_off[16];
    int rtc_available;
    char mainui_button_x[JSON_STRING_LEN];
    char mainui_button_y[JSON_STRING_LEN];
} settings_s;

static settings_s settings;
static settings_s __settings;

#define DEFAULT_THEME_PATH "/mnt/SDCARD/Themes/Silky by DiMo/"

static settings_s __default_settings = {
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
    .show_recents = 0,
    .show_expert = 0,
    .startup_auto_resume = 1,
    .menu_button_haptics = 0,
    .low_battery_autosave_at = 4,
    .low_battery_warn_at = 10,
    .time_skip = 4,
    .vibration = 2,
    .startup_tab = 0,
    .startup_application = 0,
    .mainui_single_press = 1,
    .mainui_long_press = 0,
    .mainui_double_press = 2,
    .ingame_single_press = 1,
    .ingame_long_press = 2,
    .ingame_double_press = 3,
    .disable_standby = 0,
    .enable_logging = 0,
    .blue_light_state = 0,
    .blue_light_schedule = 0,
    .blue_light_level = 0,
    .blue_light_rgb = 8421504,
    .blue_light_time = "20:00",
    .blue_light_time_off = "08:00",
    .pwmfrequency = 7,
    .mainui_button_x = "",
    .mainui_button_y = "",
    .rec_countdown = 0,
    .rec_indicator = 0,
    .rec_hotkey = 0,
};

/* Pure-logic functions copied from settings.h */

static void _settings_clone(settings_s *dst, settings_s *src)
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

static void _settings_reset(settings_s *_settings)
{
    _settings_clone(_settings, &__default_settings);
}

static int _settings_dirty_mainui(void)
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

static int settings_setVolume(int value, int apply)
{
    int changed = 0;

    if (value > 20)
        value = 20;
    else if (value < 0)
        value = 0;

    if (settings.volume != value) {
        settings.volume = value;
        changed = 1;
    }

    if (apply)
        setVolume(settings.mute ? 0 : settings.volume);

    return changed;
}

static int settings_setMute(int value, int apply)
{
    int changed = 0;

    if (settings.mute != value) {
        settings.mute = value;
        changed = 1;
    }

    if (apply)
        setVolume(settings.mute ? 0 : settings.volume);

    return changed;
}

/* ---- Tests ---- */

/* ---- _settings_clone ---- */

TEST(clone_copies_all_int_fields) {
    settings_s src = __default_settings;
    src.volume = 15;
    src.brightness = 3;
    src.hue = 5;

    settings_s dst;
    memset(&dst, 0, sizeof(dst));
    _settings_clone(&dst, &src);

    ASSERT_EQ(dst.volume, 15);
    ASSERT_EQ(dst.brightness, 3);
    ASSERT_EQ(dst.hue, 5);
}

TEST(clone_copies_string_fields) {
    settings_s src = __default_settings;
    strncpy(src.keymap, "custom_keymap", sizeof(src.keymap) - 1);
    strncpy(src.language, "it.lang", sizeof(src.language) - 1);
    strncpy(src.theme, "/custom/theme/", sizeof(src.theme) - 1);

    settings_s dst;
    memset(&dst, 0, sizeof(dst));
    _settings_clone(&dst, &src);

    ASSERT_STREQ(dst.keymap, "custom_keymap");
    ASSERT_STREQ(dst.language, "it.lang");
    ASSERT_STREQ(dst.theme, "/custom/theme/");
}

TEST(clone_null_terminates_strings) {
    settings_s src = __default_settings;
    /* Fill keymap with non-null bytes */
    memset(src.keymap, 'A', sizeof(src.keymap));

    settings_s dst;
    _settings_clone(&dst, &src);

    /* Last byte must be null */
    ASSERT_EQ(dst.keymap[sizeof(dst.keymap) - 1], '\0');
    ASSERT_EQ(dst.language[sizeof(dst.language) - 1], '\0');
    ASSERT_EQ(dst.theme[sizeof(dst.theme) - 1], '\0');
    ASSERT_EQ(dst.mainui_button_x[sizeof(dst.mainui_button_x) - 1], '\0');
    ASSERT_EQ(dst.mainui_button_y[sizeof(dst.mainui_button_y) - 1], '\0');
}

TEST(clone_is_independent_copy) {
    settings_s src = __default_settings;
    settings_s dst;
    _settings_clone(&dst, &src);

    /* Modifying dst should not affect src */
    dst.volume = 99;
    strncpy(dst.language, "fr.lang", sizeof(dst.language) - 1);

    ASSERT_EQ(src.volume, 20);
    ASSERT_STREQ(src.language, "en.lang");
}

/* ---- _settings_reset ---- */

TEST(reset_restores_defaults) {
    settings_s s;
    memset(&s, 0xFF, sizeof(s));
    _settings_reset(&s);

    ASSERT_EQ(s.volume, 20);
    ASSERT_EQ(s.brightness, 7);
    ASSERT_EQ(s.sleep_timer, 5);
    ASSERT_STREQ(s.keymap, "L2,L,R2,R,X,A,B,Y");
    ASSERT_STREQ(s.language, "en.lang");
    ASSERT_STREQ(s.theme, DEFAULT_THEME_PATH);
    ASSERT_EQ(s.fontsize, 24);
    ASSERT_EQ(s.audiofix, 1);
}

TEST(reset_restores_button_defaults) {
    settings_s s;
    memset(&s, 0xFF, sizeof(s));
    _settings_reset(&s);

    ASSERT_EQ(s.mainui_single_press, 1);
    ASSERT_EQ(s.mainui_long_press, 0);
    ASSERT_EQ(s.mainui_double_press, 2);
    ASSERT_EQ(s.ingame_single_press, 1);
    ASSERT_EQ(s.ingame_long_press, 2);
    ASSERT_EQ(s.ingame_double_press, 3);
}

/* ---- _settings_dirty_mainui ---- */

TEST(dirty_mainui_clean_after_clone) {
    _settings_reset(&settings);
    _settings_clone(&__settings, &settings);

    ASSERT_FALSE(_settings_dirty_mainui());
}

TEST(dirty_mainui_volume_changed) {
    _settings_reset(&settings);
    _settings_clone(&__settings, &settings);

    settings.volume = 10;
    ASSERT_TRUE(_settings_dirty_mainui());
}

TEST(dirty_mainui_keymap_changed) {
    _settings_reset(&settings);
    _settings_clone(&__settings, &settings);

    strncpy(settings.keymap, "new_keymap", sizeof(settings.keymap) - 1);
    ASSERT_TRUE(_settings_dirty_mainui());
}

TEST(dirty_mainui_language_changed) {
    _settings_reset(&settings);
    _settings_clone(&__settings, &settings);

    strncpy(settings.language, "ja.lang", sizeof(settings.language) - 1);
    ASSERT_TRUE(_settings_dirty_mainui());
}

TEST(dirty_mainui_theme_changed) {
    _settings_reset(&settings);
    _settings_clone(&__settings, &settings);

    strncpy(settings.theme, "/other/theme/", sizeof(settings.theme) - 1);
    ASSERT_TRUE(_settings_dirty_mainui());
}

TEST(dirty_mainui_brightness_changed) {
    _settings_reset(&settings);
    _settings_clone(&__settings, &settings);

    settings.brightness = 1;
    ASSERT_TRUE(_settings_dirty_mainui());
}

TEST(dirty_mainui_multiple_fields_changed) {
    _settings_reset(&settings);
    _settings_clone(&__settings, &settings);

    settings.hue = 5;
    settings.contrast = 8;
    settings.wifi_on = 1;
    ASSERT_TRUE(_settings_dirty_mainui());
}

TEST(dirty_mainui_non_mainui_field_not_dirty) {
    _settings_reset(&settings);
    _settings_clone(&__settings, &settings);

    /* Changing non-mainUI fields should NOT mark as dirty */
    settings.vibration = 99;
    settings.startup_tab = 3;
    settings.low_battery_warn_at = 15;
    ASSERT_FALSE(_settings_dirty_mainui());
}

/* ---- settings_setVolume ---- */

TEST(setVolume_clamps_above_20) {
    _settings_reset(&settings);
    settings.volume = 10;

    int changed = settings_setVolume(25, 0);

    ASSERT_EQ(settings.volume, 20);
    ASSERT_TRUE(changed);
}

TEST(setVolume_clamps_below_0) {
    _settings_reset(&settings);
    settings.volume = 10;

    int changed = settings_setVolume(-5, 0);

    ASSERT_EQ(settings.volume, 0);
    ASSERT_TRUE(changed);
}

TEST(setVolume_no_change_returns_false) {
    _settings_reset(&settings);
    settings.volume = 15;

    int changed = settings_setVolume(15, 0);

    ASSERT_EQ(settings.volume, 15);
    ASSERT_FALSE(changed);
}

TEST(setVolume_apply_calls_setVolume_with_volume) {
    _settings_reset(&settings);
    settings.volume = 5;
    settings.mute = 0;
    _stub_volume_value = -1;

    settings_setVolume(10, 1);

    ASSERT_EQ(settings.volume, 10);
    ASSERT_EQ(_stub_volume_value, 10);
}

TEST(setVolume_apply_muted_sends_zero) {
    _settings_reset(&settings);
    settings.volume = 5;
    settings.mute = 1;
    _stub_volume_value = -1;

    settings_setVolume(10, 1);

    ASSERT_EQ(settings.volume, 10);
    ASSERT_EQ(_stub_volume_value, 0);
}

TEST(setVolume_exact_boundary_20) {
    _settings_reset(&settings);
    settings.volume = 19;

    int changed = settings_setVolume(20, 0);

    ASSERT_EQ(settings.volume, 20);
    ASSERT_TRUE(changed);
}

TEST(setVolume_exact_boundary_0) {
    _settings_reset(&settings);
    settings.volume = 1;

    int changed = settings_setVolume(0, 0);

    ASSERT_EQ(settings.volume, 0);
    ASSERT_TRUE(changed);
}

/* ---- settings_setMute ---- */

TEST(setMute_toggle_on) {
    _settings_reset(&settings);
    settings.mute = 0;

    int changed = settings_setMute(1, 0);

    ASSERT_EQ(settings.mute, 1);
    ASSERT_TRUE(changed);
}

TEST(setMute_toggle_off) {
    _settings_reset(&settings);
    settings.mute = 1;

    int changed = settings_setMute(0, 0);

    ASSERT_EQ(settings.mute, 0);
    ASSERT_TRUE(changed);
}

TEST(setMute_no_change_returns_false) {
    _settings_reset(&settings);
    settings.mute = 1;

    int changed = settings_setMute(1, 0);

    ASSERT_FALSE(changed);
}

TEST(setMute_apply_muted_sends_zero) {
    _settings_reset(&settings);
    settings.volume = 15;
    settings.mute = 0;
    _stub_volume_value = -1;

    settings_setMute(1, 1);

    ASSERT_EQ(_stub_volume_value, 0);
}

TEST(setMute_apply_unmuted_sends_volume) {
    _settings_reset(&settings);
    settings.volume = 12;
    settings.mute = 1;
    _stub_volume_value = -1;

    settings_setMute(0, 1);

    ASSERT_EQ(_stub_volume_value, 12);
}

/* ---- main ---- */

int main(void)
{
    printf("\n=== settings.h Unit Tests ===\n\n");

    RUN_TEST(clone_copies_all_int_fields);
    RUN_TEST(clone_copies_string_fields);
    RUN_TEST(clone_null_terminates_strings);
    RUN_TEST(clone_is_independent_copy);

    RUN_TEST(reset_restores_defaults);
    RUN_TEST(reset_restores_button_defaults);

    RUN_TEST(dirty_mainui_clean_after_clone);
    RUN_TEST(dirty_mainui_volume_changed);
    RUN_TEST(dirty_mainui_keymap_changed);
    RUN_TEST(dirty_mainui_language_changed);
    RUN_TEST(dirty_mainui_theme_changed);
    RUN_TEST(dirty_mainui_brightness_changed);
    RUN_TEST(dirty_mainui_multiple_fields_changed);
    RUN_TEST(dirty_mainui_non_mainui_field_not_dirty);

    RUN_TEST(setVolume_clamps_above_20);
    RUN_TEST(setVolume_clamps_below_0);
    RUN_TEST(setVolume_no_change_returns_false);
    RUN_TEST(setVolume_apply_calls_setVolume_with_volume);
    RUN_TEST(setVolume_apply_muted_sends_zero);
    RUN_TEST(setVolume_exact_boundary_20);
    RUN_TEST(setVolume_exact_boundary_0);

    RUN_TEST(setMute_toggle_on);
    RUN_TEST(setMute_toggle_off);
    RUN_TEST(setMute_no_change_returns_false);
    RUN_TEST(setMute_apply_muted_sends_zero);
    RUN_TEST(setMute_apply_unmuted_sends_volume);

    TEST_REPORT();
    return test_failures;
}
