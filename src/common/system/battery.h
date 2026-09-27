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
 * On HAS_AXP() devices (MM+/Flip), each uncached call forks+execs /customer/app/axp_test (~5-10ms).
 * Caching for 2 seconds eliminates ~99% of subprocess overhead in hot loops.
 * Note: Cache variables are per-translation-unit (static). Thread safety is not
 * required as all callers (batmon main loop, chargingState, keymon) are single-threaded. */
#define BATTERY_CHARGING_CACHE_MS 2000
static struct timespec _charging_cache_ts = {0, 0};
static bool _charging_cache_val = false;
static bool _charging_cache_valid = false;

// Milliseconds from `since` to `now` (negative if `now` is earlier).
static inline long battery_elapsedMs(const struct timespec *since,
                                     const struct timespec *now)
{
    long elapsed_ms = (now->tv_sec - since->tv_sec) * 1000L;
    long ns_diff = now->tv_nsec - since->tv_nsec;
    if (ns_diff < 0) {
        elapsed_ms -= 1000L;
        ns_diff += 1000000000L;
    }
    elapsed_ms += ns_diff / 1000000L;
    return elapsed_ms;
}

// Whether a charging state read at `cached` can still be used at `now`.
static inline bool battery_cacheFresh(const struct timespec *cached,
                                      const struct timespec *now)
{
    long elapsed_ms = battery_elapsedMs(cached, now);
    return elapsed_ms >= 0 && elapsed_ms < BATTERY_CHARGING_CACHE_MS;
}

// Battery percentage reported by axp_test (Mini+, Mini Flip): only 0-100 is
// accepted, and then remembered in *last_good. Anything else (axp_test has
// printed garbage such as 1735289191, and -1 means it failed) is rejected
// and the caller keeps *last_good, which stays -1 until a good sample.
static inline bool battery_acceptAxpPercent(int sample, int *last_good)
{
    if (sample < 0 || sample > 100)
        return false;
    *last_good = sample;
    return true;
}

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
 * On HAS_AXP() devices this forks /customer/app/axp_test (~5-10ms per call).
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
    else if (HAS_AXP()) {
        char buf[100];
        int charge_number = 0;

        // Run axp_test directly (was popen("cd /customer/app/ ; ./axp_test"):
        // a shell more per call, every 2 s in batmon on the Mini+ and Flip).
        if (process_readFirstLine("/customer/app/", "./axp_test", buf, sizeof(buf))) {
            if (sscanf(buf, "{\"battery\":%*d, \"voltage\":%*d, \"charging\":%d}",
                       &charge_number) != 1)
                charge_number = 0;
        }
        return charge_number == 3;
    }
    return false;
#else
    return false;
#endif
}

/**
 * @brief Cached wrapper for charging state detection.
 * Returns a cached result if called within BATTERY_CHARGING_CACHE_MS (2s),
 * avoiding repeated subprocess spawns on HAS_AXP() devices.
 */
bool battery_isCharging(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);

    if (_charging_cache_valid && battery_cacheFresh(&_charging_cache_ts, &now))
        return _charging_cache_val;

    _charging_cache_val = _battery_isCharging_impl();
    _charging_cache_ts = now;
    _charging_cache_valid = true;
    return _charging_cache_val;
}

bool battery_hasChanged(int ticks, int *out_percentage)
{
    bool changed = false;

    if (battery_isCharging()) {
        /* Keep the 500 "charging icon" sentinel. Do not let percBat
         * overwrite it with a raw percentage (OnionUI/Onion:main). */
        if (!battery_is_charging) {
            *out_percentage = 500;
            battery_is_charging = true;
            return true;
        }
        return false;
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
