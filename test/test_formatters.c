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

/* ==== Tests: Open MainUI game list settings ==== */

TEST(romlist_parse_comma)
{
    int d = -1, sp = -1;
    ASSERT_TRUE(romlist_parseScroll("1000,50", &d, &sp));
    ASSERT_EQ(d, 1000);
    ASSERT_EQ(sp, 50);
}

TEST(romlist_parse_space)
{
    int d = -1, sp = -1;
    ASSERT_TRUE(romlist_parseScroll("500 25", &d, &sp));
    ASSERT_EQ(d, 500);
    ASSERT_EQ(sp, 25);
}

TEST(romlist_parse_rejects)
{
    int d = 7, sp = 7;
    ASSERT_FALSE(romlist_parseScroll("1000,0", &d, &sp));
    ASSERT_FALSE(romlist_parseScroll("-1,50", &d, &sp));
    ASSERT_FALSE(romlist_parseScroll("1000", &d, &sp));
    ASSERT_FALSE(romlist_parseScroll("", &d, &sp));
    ASSERT_FALSE(romlist_parseScroll(NULL, &d, &sp));
    /* A rejected text leaves the outputs alone */
    ASSERT_EQ(d, 7);
    ASSERT_EQ(sp, 7);
}

TEST(romlist_nearest)
{
    /* Speeds without Off: 25 50 75 100 150 200 */
    ASSERT_EQ(romlist_nearestIndex(romlist_scroll_speeds + 1, 6, 50), 1);
    ASSERT_EQ(romlist_nearestIndex(romlist_scroll_speeds + 1, 6, 120), 3);
    ASSERT_EQ(romlist_nearestIndex(romlist_scroll_speeds + 1, 6, 400), 5);
    /* Tie between 500 and 1000 ms: the first one wins */
    ASSERT_EQ(romlist_nearestIndex(romlist_scroll_delays, 4, 750), 0);
}

TEST(romlist_speed_round_trip)
{
    for (int i = 1; i <= ROMLIST_SCROLL_SPEED_MAX; i++)
        ASSERT_EQ(1 + romlist_nearestIndex(romlist_scroll_speeds + 1, ROMLIST_SCROLL_SPEED_MAX,
                                           romlist_scroll_speeds[i]),
                  i);
}

TEST(romlist_font_size_mapping)
{
    ASSERT_EQ(romlist_fontSizeForIndex(0), 0);
    ASSERT_EQ(romlist_fontSizeForIndex(1), 16);
    ASSERT_EQ(romlist_fontSizeForIndex(ROMLIST_FONT_SIZE_MAX), 40);
    ASSERT_EQ(romlist_fontSizeForIndex(99), 40);
    ASSERT_EQ(romlist_indexForFontSize(0), 0);
    ASSERT_EQ(romlist_indexForFontSize(16), 1);
    ASSERT_EQ(romlist_indexForFontSize(22), 4);
    ASSERT_EQ(romlist_indexForFontSize(40), ROMLIST_FONT_SIZE_MAX);
    ASSERT_EQ(romlist_indexForFontSize(8), 1);
    ASSERT_EQ(romlist_indexForFontSize(60), ROMLIST_FONT_SIZE_MAX);
    for (int i = 0; i <= ROMLIST_FONT_SIZE_MAX; i++)
        ASSERT_EQ(romlist_indexForFontSize(romlist_fontSizeForIndex(i)), i);
}

TEST(romlist_labels)
{
    char out[STR_MAX] = {0};
    TestListItem item = make_item(8);
    formatter_romListRows(&item, out);
    ASSERT_STREQ(out, "8");

    item = make_item(0);
    formatter_romListFontSize(&item, out);
    ASSERT_STREQ(out, "Theme");
    item = make_item(3);
    formatter_romListFontSize(&item, out);
    ASSERT_STREQ(out, "20");

    item = make_item(0);
    formatter_romListScrollSpeed(&item, out);
    ASSERT_STREQ(out, "Off");
    item = make_item(2);
    formatter_romListScrollSpeed(&item, out);
    ASSERT_STREQ(out, "50 px/s");

    item = make_item(0);
    formatter_romListScrollDelay(&item, out);
    ASSERT_STREQ(out, "0.5 s");
    item = make_item(1);
    formatter_romListScrollDelay(&item, out);
    ASSERT_STREQ(out, "1 s");
    item = make_item(3);
    formatter_romListScrollDelay(&item, out);
    ASSERT_STREQ(out, "3 s");
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

    /* Open MainUI game list settings */
    RUN_TEST(romlist_parse_comma);
    RUN_TEST(romlist_parse_space);
    RUN_TEST(romlist_parse_rejects);
    RUN_TEST(romlist_nearest);
    RUN_TEST(romlist_speed_round_trip);
    RUN_TEST(romlist_font_size_mapping);
    RUN_TEST(romlist_labels);

    TEST_REPORT();
    return test_failures;
}
