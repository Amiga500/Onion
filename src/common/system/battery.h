#ifndef BATTERY_H__
#define BATTERY_H__

#include <time.h>

#include "system/device_model.h"
#include "system/system.h"
#include "utils/file.h"
#include "utils/log.h"
#include "utils/msleep.h"
#include "utils/process.h"

static time_t battery_last_modified = 0;
static bool battery_is_charging = false;

/* Cache for battery_isCharging() to avoid repeated GPIO reads / subprocess spawns.
 * On MIYOO354, each uncached call forks+execs /customer/app/axp_test (~5-10ms).
 * Caching for 2 seconds eliminates ~99% of subprocess overhead in hot loops.
 * Note: Cache variables are per-translation-unit (static). Thread safety is not
 * required as all callers (batmon main loop, chargingState, keymon) are single-threaded. */
#define BATTERY_CHARGING_CACHE_MS 2000
static struct timespec _charging_cache_ts = {0, 0};
static bool _charging_cache_val = false;
static bool _charging_cache_valid = false;

/**
 * @brief Retrieve the current battery percentage as reported by batmon
 *
 * @return int : Battery percentage (0-100) or 500 if charging
 */

int battery_getPercentage(void)
{
    FILE *fp;
    int percentage = -1;
    int retry = 3;

    while (percentage == -1 && retry > 0) {
        if (exists("/tmp/percBat")) {
            file_get(fp, "/tmp/percBat", "%d", &percentage);
            break;
        }
        else {
            printf_debug("/tmp/percBat not found (%d)\n", retry);

            if (!process_isRunning("batmon")) {
                printf_debug("bin/batmon not running (%d)\n", retry);
                break;
            }
        }
        retry--;
        msleep(100);
    }

#ifndef PLATFORM_MIYOOMINI
#ifdef LOG_DEBUG
    return 78;
#endif
#endif

    if (percentage == -1)
        percentage = 0; // show zero when percBat not found

    return percentage;
}

/**
 * @brief Uncached implementation of charging state detection.
 * On MIYOO354 this forks /customer/app/axp_test (~5-10ms per call).
 */
static bool _battery_isCharging_impl(void)
{
#ifdef PLATFORM_MIYOOMINI
    if (DEVICE_ID == MIYOO283) {
        char charging = 0;
        int fd = open(GPIO_DIR2 "gpio59/value", O_RDONLY);

        if (fd < 0) {
            // export gpio59, direction: in
            file_write(GPIO_DIR1 "export", "59", 2);
            file_write(GPIO_DIR2 "gpio59/direction", "in", 2);
            fd = open(GPIO_DIR2 "gpio59/value", O_RDONLY);
        }

        if (fd >= 0) {
            read(fd, &charging, 1);
            close(fd);
        }

        return charging == '1';
    }
    else if (DEVICE_ID == MIYOO354) {
        char *cmd = "cd /customer/app/ ; ./axp_test";
        char buf[100];
        int charge_number = 0;

        FILE *fp;
        fp = popen(cmd, "r");
        if (fp != NULL) {
            if (fgets(buf, sizeof(buf), fp) != NULL) {
                if (sscanf(buf, "{\"battery\":%*d, \"voltage\":%*d, \"charging\":%d}",
                           &charge_number) != 1)
                    charge_number = 0;
            }
            pclose(fp);
        }
        return charge_number == 3;
    }
    return false;
#elif defined(PLATFORM_MIYOOFLIP)
    /* RK3566: use standard Linux power_supply sysfs interface */
    bool charging = false;
    char status[32] = {0};
    FILE *fp = fopen("/sys/class/power_supply/battery/status", "r");
    if (fp != NULL) {
        if (fgets(status, sizeof(status), fp) != NULL)
            charging = (strncmp(status, "Charging", 8) == 0);
        fclose(fp);
    }
    return charging;
#else
    return false;
#endif
}

/**
 * @brief Cached wrapper for charging state detection.
 * Returns a cached result if called within BATTERY_CHARGING_CACHE_MS (2s),
 * avoiding repeated subprocess spawns on MIYOO354.
 */
bool battery_isCharging(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);

    if (_charging_cache_valid) {
        long elapsed_ms = (now.tv_sec - _charging_cache_ts.tv_sec) * 1000L;
        long ns_diff = now.tv_nsec - _charging_cache_ts.tv_nsec;
        if (ns_diff < 0) {
            elapsed_ms -= 1000L;
            ns_diff += 1000000000L;
        }
        elapsed_ms += ns_diff / 1000000L;
        if (elapsed_ms >= 0 && elapsed_ms < BATTERY_CHARGING_CACHE_MS)
            return _charging_cache_val;
    }

    _charging_cache_val = _battery_isCharging_impl();
    _charging_cache_ts = now;
    _charging_cache_valid = true;
    return _charging_cache_val;
}

bool battery_hasChanged(int ticks, int *out_percentage)
{
    bool changed = false;

    if (battery_isCharging()) {
        if (!battery_is_charging) {
            *out_percentage = 500;
            battery_is_charging = true;
            changed = true;
        }
    }
    else if (battery_is_charging) {
        battery_is_charging = false;
    }

    if (file_isModified("/tmp/percBat", &battery_last_modified)) {
        int current_percentage = battery_getPercentage();

        if (current_percentage != *out_percentage) {
            *out_percentage = current_percentage;
            changed = true;
        }
    }

    return changed;
}

#endif // BATTERY_H__
