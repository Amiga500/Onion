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

#define STR_MAX 256
#define BATTPERC_MAX_OFFSET 48

/* ---- Minimal ListItem for tests ---- */

typedef struct {
    char label[STR_MAX];
    int value;
    int action_id;
} TestListItem;

/* ---- Inline functions under test ---- */

static void formatter_timezone(void *pt, char *out_label)
{
    TestListItem *item = (TestListItem *)pt;
    int value = item->value;
    double utc_value = ((double)value / 2.0) - 12.0;
    int half_past = (round(utc_value) != utc_value) ? 1 : 0;
    if (utc_value == 0.0) {
        strncpy(out_label, "UTC", STR_MAX - 1);
        out_label[STR_MAX - 1] = '\0';
    }
    else {
        snprintf(out_label, STR_MAX, utc_value > 0.0 ? "UTC+%02d:%02d" : "UTC-%02d:%02d",
                 (int)floor(fabs(utc_value)), half_past ? 30 : 0);
    }
}

static void formatter_Time(void *pt, char *out_label)
{
    TestListItem *item = (TestListItem *)pt;
    int value = item->value;
    int hours = value / 4;
    int minutes = (value % 4) * 15;
    snprintf(out_label, STR_MAX, "%02d:%02d", hours, minutes);
}

static int formatter_timeStringToID(const char *time_str)
{
    int hours = 0, minutes = 0;
    if (sscanf(time_str, "%02d:%02d", &hours, &minutes) != 2)
        return 0;
    int intervalsFromHours = hours * 4;
    int intervalsFromMinutes = minutes / 15;
    return intervalsFromHours + intervalsFromMinutes;
}

static void formatter_battWarn(void *pt, char *out_label)
{
    TestListItem *item = (TestListItem *)pt;
    if (item->value == 0)
        strncpy(out_label, "Off", STR_MAX - 1);
    else
        snprintf(out_label, STR_MAX, "< %d%%", item->value * 5);
}

static void formatter_battExit(void *pt, char *out_label)
{
    TestListItem *item = (TestListItem *)pt;
    if (item->value == 0)
        strncpy(out_label, "Off", STR_MAX - 1);
    else
        snprintf(out_label, STR_MAX, "< %d%%", item->value);
}

static const int num_font_families __attribute__((unused)) = 5;
static const char font_families[][STR_MAX] = {
    "BPreplayBold.otf", "Exo-2-Bold-Italic_Universal.ttf",
    "Helvetica-Neue-2.ttf", "HENB.TTF", "wqy-microhei.ttc"};

static void formatter_fontFamily(void *pt, char *out_label)
{
    TestListItem *item = (TestListItem *)pt;
    if (item->value == 0)
        strncpy(out_label, "-", STR_MAX - 1);
    else
        strncpy(out_label, font_families[item->value - 1], STR_MAX - 1);
    out_label[STR_MAX - 1] = '\0';
}

static const int num_font_sizes __attribute__((unused)) = 5;
static const int font_sizes[] = {13, 18, 24, 32, 40};

static void formatter_fontSize(void *pt, char *out_label)
{
    TestListItem *item = (TestListItem *)pt;
    if (item->value == 0) {
        strncpy(out_label, "-", STR_MAX - 1);
        out_label[STR_MAX - 1] = '\0';
    }
    else
        snprintf(out_label, STR_MAX, "%d px", font_sizes[item->value - 1]);
}

static void formatter_fastForward(void *pt, char *out_label)
{
    TestListItem *item = (TestListItem *)pt;
    if (item->value == 0)
        strncpy(out_label, "Unlimited", STR_MAX - 1);
    else
        snprintf(out_label, STR_MAX, "%d.0x", item->value);
}

static void formatter_positionOffset(void *pt, char *out_label)
{
    TestListItem *item = (TestListItem *)pt;
    if (item->value == 0)
        strncpy(out_label, "-", STR_MAX - 1);
    else
        snprintf(out_label, STR_MAX, "%d px", item->value - 1 - BATTPERC_MAX_OFFSET);
}

static void formatter_meterWidth(void *pt, char *out_label)
{
    TestListItem *item = (TestListItem *)pt;
    snprintf(out_label, STR_MAX, "%d px", item->value);
}

static void formatter_timeSkip(void *pt, char *out_label)
{
    TestListItem *item = (TestListItem *)pt;
    if (item->value == 0)
        strncpy(out_label, "Off", STR_MAX - 1);
    else
        snprintf(out_label, STR_MAX, "+ %dh", item->value);
}

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

TEST(time_str_invalid) {
    ASSERT_EQ(formatter_timeStringToID("invalid"), 0);
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
    ASSERT_STREQ(out, "Unlimited");
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

    TEST_REPORT();
    return test_failures;
}
