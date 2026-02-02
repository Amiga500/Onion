#ifndef TWEAKS_OVERCLOCK_FORMATTERS_H__
#define TWEAKS_OVERCLOCK_FORMATTERS_H__

#include <stdio.h>

#include "components/list.h"
#include "./overclock.h"

/**
 * Format CPU frequency for display
 */
void formatter_cpuFrequency(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    // Value is 0-10, map to 1200-1700 MHz in 50 MHz steps
    int frequency = 1200 + (item->value * 50);
    int gain = overclock_get_performance_gain(frequency);
    
    if (frequency == 1200) {
        sprintf(out_label, "%d MHz (Stock)", frequency);
    }
    else {
        sprintf(out_label, "%d MHz (+%d%%)", frequency, gain);
    }
}

/**
 * Format thermal limit for display
 */
void formatter_thermalLimit(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    // Value is 0-10, map to 60-75°C
    int temp = 60 + (int)(item->value * 1.5);
    
    if (temp <= 60) {
        sprintf(out_label, "%d°C (Conservative)", temp);
    }
    else if (temp <= 65) {
        sprintf(out_label, "%d°C (Safe)", temp);
    }
    else if (temp <= 70) {
        sprintf(out_label, "%d°C (Moderate)", temp);
    }
    else {
        sprintf(out_label, "%d°C (Aggressive)", temp);
    }
}

/**
 * Format overclock profile for display
 */
void formatter_overclockProfile(void *pt, char *out_label)
{
    ListItem *item = (ListItem *)pt;
    OverclockProfile profile = (OverclockProfile)item->value;
    int freq = overclock_get_profile_frequency(profile);
    int gain = overclock_get_performance_gain(freq);
    
    switch (profile) {
        case OC_PROFILE_STOCK:
            sprintf(out_label, "Stock (1200 MHz)");
            break;
        case OC_PROFILE_MILD:
            sprintf(out_label, "Mild (1300 MHz, +%d%%)", gain);
            break;
        case OC_PROFILE_MODERATE:
            sprintf(out_label, "Moderate (1400 MHz, +%d%%)", gain);
            break;
        case OC_PROFILE_HIGH:
            sprintf(out_label, "High (1500 MHz, +%d%%)", gain);
            break;
        case OC_PROFILE_EXTREME:
            sprintf(out_label, "Extreme (1700 MHz, +%d%%) ⚠", gain);
            break;
        case OC_PROFILE_CUSTOM:
            sprintf(out_label, "Custom (%d MHz)", freq);
            break;
        default:
            sprintf(out_label, "Unknown");
            break;
    }
}

/**
 * Format current temperature for display
 */
void formatter_currentTemperature(void *pt, char *out_label)
{
    int temp = thermal_current_temp();
    
    if (temp < 0) {
        sprintf(out_label, "N/A");
    }
    else if (temp < THERMAL_TEMP_WARNING) {
        sprintf(out_label, "%d°C (Safe ✓)", temp);
    }
    else if (temp < THERMAL_TEMP_CRITICAL) {
        sprintf(out_label, "%d°C (Warning ⚠)", temp);
    }
    else {
        sprintf(out_label, "%d°C (Critical! ✗)", temp);
    }
}

#endif // TWEAKS_OVERCLOCK_FORMATTERS_H__
