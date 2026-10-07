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

int formatter_timeStringToID(const char *time_str)
{
    // A malformed time (edited or corrupt config) left hours and minutes
    // uninitialised: the list index was garbage. Treat it as 00:00.
    int hours = 0, minutes = 0;
    if (time_str == NULL || sscanf(time_str, "%02d:%02d", &hours, &minutes) != 2)
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
}

void formatter_battExit(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    if (item->value == 0)
        strcpy(out_label, "Off");
    else
        sprintf(out_label, "< %d%%", item->value);
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
}

void formatter_timeSkip(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    if (item->value == 0)
        strcpy(out_label, "Off");
    else
        sprintf(out_label, "+ %dh", item->value);
}

#endif // TWEAKS_FORMATTERS_BASIC_H__
