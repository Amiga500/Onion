#ifndef SYSTEM_BATTERY_LEVEL_H__
#define SYSTEM_BATTERY_LEVEL_H__

// The battery level batmon publishes in /tmp/percBat, for a device that is
// charging: 0-100 on the Mini Plus and Flip (their AXP gauge keeps reading
// it), 500 on the Mini (no level while charging). -1 when it isn't known.
// No dependencies, so the theme's battery drawing can use it too.

#include <stdio.h>

#ifndef BATTERY_PERCBAT_PATH
#define BATTERY_PERCBAT_PATH "/tmp/percBat"
#endif

static inline int battery_chargingLevel(void)
{
    FILE *fp = fopen(BATTERY_PERCBAT_PATH, "r");
    if (fp == NULL)
        return -1;
    int level = -1;
    if (fscanf(fp, "%d", &level) != 1)
        level = -1;
    fclose(fp);
    return level >= 0 && level <= 100 ? level : -1;
}

#endif // SYSTEM_BATTERY_LEVEL_H__
