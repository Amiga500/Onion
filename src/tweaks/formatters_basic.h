#ifndef TWEAKS_FORMATTERS_BASIC_H__
#define TWEAKS_FORMATTERS_BASIC_H__

// Tweaks value formatters that only need a ListItem (no apps, tools or
// language), split from formatters.h so host tests can include them.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "components/list.h"
#include "utils/str.h"

#define BATTPERC_MAX_OFFSET 48

// "(default)" marks the value a new install starts with, so it can be found
// again after trying others. From __default_settings (system/settings.h),
// the shipped static/configs files and the readers in values.h; the tests
// check them against those.
#define DEFAULT_TIME_SKIP 4         // hours (startup/addHours)
#define DEFAULT_LOW_BATTERY_WARN 10 // % (battery/warnAt)
#define DEFAULT_LOW_BATTERY_EXIT 4  // % (battery/exitAt)
#define DEFAULT_METER_WIDTH 4       // px (display/meterWidth)
#define DEFAULT_FAST_FORWARD 0      // Unlimited (RetroArch fastforward_ratio)
#define DEFAULT_BLUE_LIGHT_ON "20:00"
#define DEFAULT_BLUE_LIGHT_OFF "08:00"
#define DEFAULT_VNC_FPS 20

static void formatter_markDefault(char *out_label, bool is_default)
{
    if (is_default)
        strcat(out_label, " (default)");
}

void formatter_timezone(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    int value = item->value;
    double utc_value = ((double)value / 2.0) - 12.0;
    bool half_past = round(utc_value) != utc_value;
    if (utc_value == 0.0) {
        strcpy(out_label, "UTC");
    }
    else {
        sprintf(out_label, utc_value > 0.0 ? "UTC+%02d:%02d" : "UTC-%02d:%02d", (int)floor(fabs(utc_value)), half_past ? 30 : 0);
    }
}

void formatter_Time(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    int value = item->value;
    int hours = value / 4;
    int minutes = (value % 4) * 15;
    sprintf(out_label, "%02d:%02d", hours, minutes);
}

// The blue light filter's times as shown (formatter_Time alone is also used
// to store them).
void formatter_TimeOn(void *pt, char *out_label)
{
    formatter_Time(pt, out_label);
    formatter_markDefault(out_label, strcmp(out_label, DEFAULT_BLUE_LIGHT_ON) == 0);
}

void formatter_TimeOff(void *pt, char *out_label)
{
    formatter_Time(pt, out_label);
    formatter_markDefault(out_label, strcmp(out_label, DEFAULT_BLUE_LIGHT_OFF) == 0);
}

int formatter_timeStringToID(const char *time_str)
{
    // A malformed time (edited or corrupt config) left hours and minutes
    // uninitialised: the list index was garbage. Treat it as 00:00.
    int hours = 0, minutes = 0;
    if (time_str == NULL || sscanf(time_str, "%02d:%02d", &hours, &minutes) != 2)
        return 0;
    if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59)
        return 0;
    int intervalsFromHours = hours * 4;
    int intervalsFromMinutes = minutes / 15;
    return intervalsFromHours + intervalsFromMinutes;
}

void formatter_battWarn(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    if (item->value == 0)
        strcpy(out_label, "Off");
    else
        sprintf(out_label, "< %d%%", item->value * 5);
    formatter_markDefault(out_label, item->value * 5 == DEFAULT_LOW_BATTERY_WARN);
}

void formatter_battExit(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    if (item->value == 0)
        strcpy(out_label, "Off");
    else
        sprintf(out_label, "< %d%%", item->value);
    formatter_markDefault(out_label, item->value == DEFAULT_LOW_BATTERY_EXIT);
}

static const int num_font_families = 5;
static const char font_families[][STR_MAX] = {
    "BPreplayBold.otf", "Exo-2-Bold-Italic_Universal.ttf",
    "Helvetica-Neue-2.ttf", "HENB.TTF", "wqy-microhei.ttc"};
void formatter_fontFamily(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    if (item->value == 0)
        strcpy(out_label, "-");
    else
        strcpy(out_label, font_families[item->value - 1]);
}

static const int num_font_sizes = 5;
static const int font_sizes[] = {13, 18, 24, 32, 40};
void formatter_fontSize(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    if (item->value == 0)
        strcpy(out_label, "-");
    else
        sprintf(out_label, "%d px", font_sizes[item->value - 1]);
}

void formatter_fastForward(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    if (item->value == 0)
        strcpy(out_label, "Unlimited");
    else
        sprintf(out_label, "%d.0x", item->value);
    formatter_markDefault(out_label, item->value == DEFAULT_FAST_FORWARD);
}

void formatter_positionOffset(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    if (item->value == 0)
        strcpy(out_label, "-");
    else
        sprintf(out_label, "%d px", item->value - 1 - BATTPERC_MAX_OFFSET);
}

void formatter_meterWidth(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    sprintf(out_label, "%d px", item->value);
    formatter_markDefault(out_label, item->value == DEFAULT_METER_WIDTH);
}

void formatter_timeSkip(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    if (item->value == 0)
        strcpy(out_label, "Off");
    else
        sprintf(out_label, "+ %dh", item->value);
    formatter_markDefault(out_label, item->value == DEFAULT_TIME_SKIP);
}

// Screen recorder countdown, in seconds (0: none, the default).
void formatter_recCountdown(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    if (item->value == 0)
        strcpy(out_label, "Off (default)");
    else
        sprintf(out_label, "%d", item->value);
}

void formatter_vncFps(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    sprintf(out_label, "%d fps", item->value);
    formatter_markDefault(out_label, item->value == DEFAULT_VNC_FPS);
}

#endif // TWEAKS_FORMATTERS_BASIC_H__
