/**
 * @file test_formatters.c
 * @brief Unit tests for tweaks/formatters.h formatting functions
 *
 * Tests the pure formatting functions used by the Tweaks UI:
 * timezone display, time display, time→ID conversion, battery
 * warning/exit thresholds, font family/size labels, fast forward,
 * position offset, meter width, and time skip formatting.
 *
 * Build and run: make -f Makefile.unit test_formatters
 */

#include "onion_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Production code: tweaks/formatters_basic.h, on real ListItems (list.h
 * with the same lang.h prelude as test_list: the formatters do not use
 * language strings). */
#define SYSTEM_LANG_H__
typedef int lang_hash;
static char **lang_list = NULL;
#ifndef LANG_MAX
#define LANG_MAX 400
#endif

#include "../src/tweaks/formatters_basic.h"

void SDL_FreeSurface(SDL_Surface *surface) { (void)surface; }

typedef ListItem TestListItem;

/* ---- Helpers ---- */

static TestListItem make_item(int value)
{
    TestListItem item;
    memset(&item, 0, sizeof(item));
    item.value = value;
    return item;
}

/* ==== Tests: formatter_timezone ==== */

TEST(tz_utc_zero) {
    /* value=24 → (24/2)-12 = 0.0 → "UTC" */
    TestListItem item = make_item(24);
    char out[STR_MAX] = {0};
    formatter_timezone(&item, out);
    ASSERT_STREQ(out, "UTC");
}

TEST(tz_positive_whole) {
    /* value=30 → (30/2)-12 = 3.0 → "UTC+03:00" */
    TestListItem item = make_item(30);
    char out[STR_MAX] = {0};
    formatter_timezone(&item, out);
    ASSERT_STREQ(out, "UTC+03:00");
}

TEST(tz_negative_whole) {
    /* value=14 → (14/2)-12 = -5.0 → "UTC-05:00" */
    TestListItem item = make_item(14);
    char out[STR_MAX] = {0};
    formatter_timezone(&item, out);
    ASSERT_STREQ(out, "UTC-05:00");
}

TEST(tz_positive_half) {
    /* value=35 → (35/2)-12 = 5.5 → "UTC+05:30" (India) */
    TestListItem item = make_item(35);
    char out[STR_MAX] = {0};
    formatter_timezone(&item, out);
    ASSERT_STREQ(out, "UTC+05:30");
}

TEST(tz_negative_half) {
    /* value=17 → (17/2)-12 = -3.5 → "UTC-03:30" (Newfoundland) */
    TestListItem item = make_item(17);
    char out[STR_MAX] = {0};
    formatter_timezone(&item, out);
    ASSERT_STREQ(out, "UTC-03:30");
}

TEST(tz_utc_minus_12) {
    /* value=0 → (0/2)-12 = -12.0 → "UTC-12:00" */
    TestListItem item = make_item(0);
    char out[STR_MAX] = {0};
    formatter_timezone(&item, out);
    ASSERT_STREQ(out, "UTC-12:00");
}

TEST(tz_utc_plus_12) {
    /* value=48 → (48/2)-12 = 12.0 → "UTC+12:00" */
    TestListItem item = make_item(48);
    char out[STR_MAX] = {0};
    formatter_timezone(&item, out);
    ASSERT_STREQ(out, "UTC+12:00");
}

/* ==== Tests: formatter_Time ==== */

TEST(time_midnight) {
    TestListItem item = make_item(0);
    char out[STR_MAX] = {0};
    formatter_Time(&item, out);
    ASSERT_STREQ(out, "00:00");
}

TEST(time_one_am) {
    /* value=4 → 4/4=1h, 4%4=0 → "01:00" */
    TestListItem item = make_item(4);
    char out[STR_MAX] = {0};
    formatter_Time(&item, out);
    ASSERT_STREQ(out, "01:00");
}

TEST(time_quarter_past) {
    /* value=5 → 5/4=1h, 5%4=1 → "01:15" */
    TestListItem item = make_item(5);
    char out[STR_MAX] = {0};
    formatter_Time(&item, out);
    ASSERT_STREQ(out, "01:15");
}

TEST(time_half_past) {
    /* value=6 → 6/4=1h, 6%4=2 → "01:30" */
    TestListItem item = make_item(6);
    char out[STR_MAX] = {0};
    formatter_Time(&item, out);
    ASSERT_STREQ(out, "01:30");
}

TEST(time_quarter_to) {
    /* value=7 → 7/4=1h, 7%4=3 → "01:45" */
    TestListItem item = make_item(7);
    char out[STR_MAX] = {0};
    formatter_Time(&item, out);
    ASSERT_STREQ(out, "01:45");
}

TEST(time_2345) {
    /* value=95 → 95/4=23h, 95%4=3 → "23:45" */
    TestListItem item = make_item(95);
    char out[STR_MAX] = {0};
    formatter_Time(&item, out);
    ASSERT_STREQ(out, "23:45");
}

/* ==== Tests: formatter_timeStringToID ==== */

TEST(time_str_midnight) {
    ASSERT_EQ(formatter_timeStringToID("00:00"), 0);
}

TEST(time_str_one_am) {
    ASSERT_EQ(formatter_timeStringToID("01:00"), 4);
}

TEST(time_str_0115) {
    ASSERT_EQ(formatter_timeStringToID("01:15"), 5);
}

TEST(time_str_0130) {
    ASSERT_EQ(formatter_timeStringToID("01:30"), 6);
}

TEST(time_str_0145) {
    ASSERT_EQ(formatter_timeStringToID("01:45"), 7);
}

TEST(time_str_2345) {
    ASSERT_EQ(formatter_timeStringToID("23:45"), 95);
}

/* Malformed times read uninitialised variables before the fix. */
TEST(time_str_invalid) {
    ASSERT_EQ(formatter_timeStringToID("invalid"), 0);
    ASSERT_EQ(formatter_timeStringToID(""), 0);
    ASSERT_EQ(formatter_timeStringToID("12"), 0);
    ASSERT_EQ(formatter_timeStringToID(NULL), 0);
}

TEST(time_str_roundtrip) {
    /* Format then parse should give back the same ID */
    for (int id = 0; id < 96; id++) {
        TestListItem item = make_item(id);
        char out[STR_MAX];
        formatter_Time(&item, out);
        int result = formatter_timeStringToID(out);
        ASSERT_EQ(result, id);
    }
}

/* ==== Tests: formatter_battWarn ==== */

TEST(batt_warn_off) {
    TestListItem item = make_item(0);
    char out[STR_MAX] = {0};
    formatter_battWarn(&item, out);
    ASSERT_STREQ(out, "Off");
}

TEST(batt_warn_5_percent) {
    TestListItem item = make_item(1);
    char out[STR_MAX] = {0};
    formatter_battWarn(&item, out);
    ASSERT_STREQ(out, "< 5%");
}

TEST(batt_warn_20_percent) {
    TestListItem item = make_item(4);
    char out[STR_MAX] = {0};
    formatter_battWarn(&item, out);
    ASSERT_STREQ(out, "< 20%");
}

/* ==== Tests: formatter_battExit ==== */

TEST(batt_exit_off) {
    TestListItem item = make_item(0);
    char out[STR_MAX] = {0};
    formatter_battExit(&item, out);
    ASSERT_STREQ(out, "Off");
}

TEST(batt_exit_1_percent) {
    TestListItem item = make_item(1);
    char out[STR_MAX] = {0};
    formatter_battExit(&item, out);
    ASSERT_STREQ(out, "< 1%");
}

TEST(batt_exit_10_percent) {
    TestListItem item = make_item(10);
    char out[STR_MAX] = {0};
    formatter_battExit(&item, out);
    ASSERT_STREQ(out, "< 10%");
}

/* ==== Tests: formatter_fontFamily ==== */

TEST(font_family_default) {
    TestListItem item = make_item(0);
    char out[STR_MAX] = {0};
    formatter_fontFamily(&item, out);
    ASSERT_STREQ(out, "-");
}

TEST(font_family_first) {
    TestListItem item = make_item(1);
    char out[STR_MAX] = {0};
    formatter_fontFamily(&item, out);
    ASSERT_STREQ(out, "BPreplayBold.otf");
}

TEST(font_family_last) {
    TestListItem item = make_item(5);
    char out[STR_MAX] = {0};
    formatter_fontFamily(&item, out);
    ASSERT_STREQ(out, "wqy-microhei.ttc");
}

/* ==== Tests: formatter_fontSize ==== */

TEST(font_size_default) {
    TestListItem item = make_item(0);
    char out[STR_MAX] = {0};
    formatter_fontSize(&item, out);
    ASSERT_STREQ(out, "-");
}

TEST(font_size_13px) {
    TestListItem item = make_item(1);
    char out[STR_MAX] = {0};
    formatter_fontSize(&item, out);
    ASSERT_STREQ(out, "13 px");
}

TEST(font_size_40px) {
    TestListItem item = make_item(5);
    char out[STR_MAX] = {0};
    formatter_fontSize(&item, out);
    ASSERT_STREQ(out, "40 px");
}

/* ==== Tests: formatter_fastForward ==== */

TEST(fast_forward_unlimited) {
    TestListItem item = make_item(0);
    char out[STR_MAX] = {0};
    formatter_fastForward(&item, out);
    ASSERT_STREQ(out, "Unlimited (default)");
}

TEST(fast_forward_2x) {
    TestListItem item = make_item(2);
    char out[STR_MAX] = {0};
    formatter_fastForward(&item, out);
    ASSERT_STREQ(out, "2.0x");
}

TEST(fast_forward_8x) {
    TestListItem item = make_item(8);
    char out[STR_MAX] = {0};
    formatter_fastForward(&item, out);
    ASSERT_STREQ(out, "8.0x");
}

/* ==== Tests: formatter_positionOffset ==== */

TEST(position_offset_default) {
    TestListItem item = make_item(0);
    char out[STR_MAX] = {0};
    formatter_positionOffset(&item, out);
    ASSERT_STREQ(out, "-");
}

TEST(position_offset_zero) {
    /* value=49 → 49-1-48 = 0 → "0 px" */
    TestListItem item = make_item(49);
    char out[STR_MAX] = {0};
    formatter_positionOffset(&item, out);
    ASSERT_STREQ(out, "0 px");
}

TEST(position_offset_positive) {
    /* value=59 → 59-1-48 = 10 → "10 px" */
    TestListItem item = make_item(59);
    char out[STR_MAX] = {0};
    formatter_positionOffset(&item, out);
    ASSERT_STREQ(out, "10 px");
}

TEST(position_offset_negative) {
    /* value=1 → 1-1-48 = -48 → "-48 px" */
    TestListItem item = make_item(1);
    char out[STR_MAX] = {0};
    formatter_positionOffset(&item, out);
    ASSERT_STREQ(out, "-48 px");
}

/* ==== Tests: formatter_meterWidth ==== */

TEST(meter_width_zero) {
    TestListItem item = make_item(0);
    char out[STR_MAX] = {0};
    formatter_meterWidth(&item, out);
    ASSERT_STREQ(out, "0 px");
}

TEST(meter_width_10) {
    TestListItem item = make_item(10);
    char out[STR_MAX] = {0};
    formatter_meterWidth(&item, out);
    ASSERT_STREQ(out, "10 px");
}

/* ==== Tests: formatter_timeSkip ==== */

TEST(time_skip_off) {
    TestListItem item = make_item(0);
    char out[STR_MAX] = {0};
    formatter_timeSkip(&item, out);
    ASSERT_STREQ(out, "Off");
}

TEST(time_skip_1h) {
    TestListItem item = make_item(1);
    char out[STR_MAX] = {0};
    formatter_timeSkip(&item, out);
    ASSERT_STREQ(out, "+ 1h");
}

TEST(time_skip_12h) {
    TestListItem item = make_item(12);
    char out[STR_MAX] = {0};
    formatter_timeSkip(&item, out);
    ASSERT_STREQ(out, "+ 12h");
}

/* ---- main ---- */

/* ==== Tests: "(default)" on the value a new install starts with ==== */

static void label_of(void (*formatter)(void *, char *), int value, char *out)
{
    TestListItem item = make_item(value);
    out[0] = '\0';
    formatter(&item, out);
}

TEST(default_values_are_marked) {
    char out[STR_MAX];
    label_of(formatter_timeSkip, 4, out);
    ASSERT_STREQ(out, "+ 4h (default)");
    label_of(formatter_timeSkip, 5, out);
    ASSERT_STREQ(out, "+ 5h");
    label_of(formatter_battWarn, 2, out);
    ASSERT_STREQ(out, "< 10% (default)");
    label_of(formatter_battWarn, 3, out);
    ASSERT_STREQ(out, "< 15%");
    label_of(formatter_battExit, 4, out);
    ASSERT_STREQ(out, "< 4% (default)");
    label_of(formatter_battExit, 0, out);
    ASSERT_STREQ(out, "Off");
    label_of(formatter_meterWidth, 4, out);
    ASSERT_STREQ(out, "4 px (default)");
    label_of(formatter_meterWidth, 6, out);
    ASSERT_STREQ(out, "6 px");
    label_of(formatter_fastForward, 3, out);
    ASSERT_STREQ(out, "3.0x");
    label_of(formatter_TimeOn, 80, out);
    ASSERT_STREQ(out, "20:00 (default)");
    label_of(formatter_TimeOn, 32, out);
    ASSERT_STREQ(out, "08:00");
    label_of(formatter_TimeOff, 32, out);
    ASSERT_STREQ(out, "08:00 (default)");
    label_of(formatter_TimeOff, 80, out);
    ASSERT_STREQ(out, "20:00");
    /* formatter_Time stays plain: actions.h stores what it writes. */
    label_of(formatter_Time, 80, out);
    ASSERT_STREQ(out, "20:00");
    label_of(formatter_recCountdown, 0, out);
    ASSERT_STREQ(out, "Off (default)");
    label_of(formatter_recCountdown, 3, out);
    ASSERT_STREQ(out, "3");
    label_of(formatter_vncFps, 20, out);
    ASSERT_STREQ(out, "20 fps (default)");
    label_of(formatter_vncFps, 10, out);
    ASSERT_STREQ(out, "10 fps");
}

static bool text_has(const char *path, const char *text)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return false;
    static char buf[1 << 20];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return strstr(buf, text) != NULL;
}

/* The marked values are the ones the device really starts with. */
TEST(defaults_match_settings_and_configs) {
    const char *settings = "../src/common/system/settings.h";
    const char *cfg = "../static/configs/.tmp_update/config";
    char path[256];
    char line[64];

    ASSERT_TRUE(text_has(settings, ".time_skip = 4,"));
    ASSERT_EQ(DEFAULT_TIME_SKIP, 4);
    ASSERT_TRUE(text_has(settings, ".low_battery_warn_at = 10,"));
    ASSERT_EQ(DEFAULT_LOW_BATTERY_WARN, 10);
    ASSERT_TRUE(text_has(settings, ".low_battery_autosave_at = 4,"));
    ASSERT_EQ(DEFAULT_LOW_BATTERY_EXIT, 4);
    ASSERT_TRUE(text_has(settings, ".vibration = 2,"));
    ASSERT_TRUE(text_has("../src/tweaks/menus.h", "{\"Off\", \"Low\", \"Normal (default)\", \"High\"}"));
    ASSERT_TRUE(text_has(settings, ".blue_light_level = 0,"));
    ASSERT_TRUE(text_has("../src/tweaks/formatters.h", "\"Subtle 1/5 (default)\", \"Moderate 2/5\""));
    ASSERT_TRUE(text_has(settings, ".blue_light_time = \"" DEFAULT_BLUE_LIGHT_ON "\","));
    ASSERT_TRUE(text_has(settings, ".blue_light_time_off = \"" DEFAULT_BLUE_LIGHT_OFF "\","));
    ASSERT_TRUE(text_has(settings, ".rec_countdown = false,"));
    ASSERT_TRUE(text_has("../src/tweaks/network.h", ".vncfps = 20,"));
    ASSERT_EQ(DEFAULT_VNC_FPS, 20);

    /* Shipped files win over settings.h where they exist. */
    snprintf(path, sizeof(path), "%s/battery/warnAt", cfg);
    FILE *fp = fopen(path, "r");
    ASSERT_TRUE(fp != NULL && fgets(line, sizeof(line), fp) != NULL);
    fclose(fp);
    ASSERT_EQ(atoi(line), DEFAULT_LOW_BATTERY_WARN);
    snprintf(path, sizeof(path), "%s/startup/addHours", cfg);
    fp = fopen(path, "r");
    ASSERT_TRUE(fp != NULL && fgets(line, sizeof(line), fp) != NULL);
    fclose(fp);
    ASSERT_EQ(atoi(line), DEFAULT_TIME_SKIP);
    snprintf(path, sizeof(path), "%s/display/meterWidth", cfg);
    fp = fopen(path, "r");
    ASSERT_TRUE(fp != NULL && fgets(line, sizeof(line), fp) != NULL);
    fclose(fp);
    ASSERT_EQ(atoi(line), DEFAULT_METER_WIDTH);
    ASSERT_TRUE(text_has("../src/tweaks/values.h", "int meterWidth = 4;"));
    snprintf(path, sizeof(path), "%s/battery/exitAt", cfg);
    ASSERT_TRUE(fopen(path, "r") == NULL); /* none shipped: settings.h */

    /* PWM: the shipped .pwmfrequency, Tweaks without it and runtime.sh. */
    snprintf(path, sizeof(path), "%s/.pwmfrequency", cfg);
    fp = fopen(path, "r");
    ASSERT_TRUE(fp != NULL && fgets(line, sizeof(line), fp) != NULL);
    fclose(fp);
    ASSERT_EQ(atoi(line), 7);
    ASSERT_TRUE(text_has("../src/tweaks/values.h", "int pwmfrequency = 7;"));
    ASSERT_TRUE(text_has("../static/build/.tmp_update/runtime.sh", "frequency=800"));
    ASSERT_TRUE(text_has("../src/tweaks/formatters.h", "\"700 Hz\", \"800 Hz (default)\", \"900 Hz\""));

    /* Fast forward: RetroArch's shipped ratio (0: unlimited). */
    ASSERT_TRUE(text_has("../static/configs/RetroArch/.retroarch/retroarch.cfg", "fastforward_ratio = \"0.000000\""));
    ASSERT_EQ(DEFAULT_FAST_FORWARD, 0);

    /* LCD undervolt: Off without its config file. */
    ASSERT_TRUE(text_has("../src/tweaks/values.h", "if (!is_file(LCD_VOLT_CONFIG))\n        return 0;"));
    ASSERT_TRUE(text_has("../src/tweaks/menus.h", "{\"Off (default)\", \"-0.1V\""));
}

int main(void)
{
    printf("\n=== tweaks/formatters.h Unit Tests ===\n\n");

    /* Timezone */
    RUN_TEST(tz_utc_zero);
    RUN_TEST(tz_positive_whole);
    RUN_TEST(tz_negative_whole);
    RUN_TEST(tz_positive_half);
    RUN_TEST(tz_negative_half);
    RUN_TEST(tz_utc_minus_12);
    RUN_TEST(tz_utc_plus_12);

    /* Time */
    RUN_TEST(time_midnight);
    RUN_TEST(time_one_am);
    RUN_TEST(time_quarter_past);
    RUN_TEST(time_half_past);
    RUN_TEST(time_quarter_to);
    RUN_TEST(time_2345);

    /* Time string → ID */
    RUN_TEST(time_str_midnight);
    RUN_TEST(time_str_one_am);
    RUN_TEST(time_str_0115);
    RUN_TEST(time_str_0130);
    RUN_TEST(time_str_0145);
    RUN_TEST(time_str_2345);
    RUN_TEST(time_str_invalid);
    RUN_TEST(time_str_roundtrip);

    /* Battery warn/exit */
    RUN_TEST(batt_warn_off);
    RUN_TEST(batt_warn_5_percent);
    RUN_TEST(batt_warn_20_percent);
    RUN_TEST(batt_exit_off);
    RUN_TEST(batt_exit_1_percent);
    RUN_TEST(batt_exit_10_percent);

    /* Font family/size */
    RUN_TEST(font_family_default);
    RUN_TEST(font_family_first);
    RUN_TEST(font_family_last);
    RUN_TEST(font_size_default);
    RUN_TEST(font_size_13px);
    RUN_TEST(font_size_40px);

    /* Fast forward */
    RUN_TEST(fast_forward_unlimited);
    RUN_TEST(fast_forward_2x);
    RUN_TEST(fast_forward_8x);

    /* Position offset */
    RUN_TEST(position_offset_default);
    RUN_TEST(position_offset_zero);
    RUN_TEST(position_offset_positive);
    RUN_TEST(position_offset_negative);

    /* Meter width */
    RUN_TEST(meter_width_zero);
    RUN_TEST(meter_width_10);

    /* Time skip */
    RUN_TEST(time_skip_off);
    RUN_TEST(time_skip_1h);
    RUN_TEST(time_skip_12h);
    RUN_TEST(default_values_are_marked);
    RUN_TEST(defaults_match_settings_and_configs);

    TEST_REPORT();
    return test_failures;
}
