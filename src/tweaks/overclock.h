#ifndef TWEAKS_OVERCLOCK_H__
#define TWEAKS_OVERCLOCK_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "system/thermal.h"
#include "utils/file.h"
#include "utils/log.h"

// CPU frequency limits (in MHz)
#define CPU_FREQ_STOCK 1200
#define CPU_FREQ_MIN 1200
#define CPU_FREQ_MAX 1700
#define CPU_FREQ_SAFE_MAX 1500
#define CPU_FREQ_STEP 50

// Overclocking profiles
typedef enum {
    OC_PROFILE_STOCK = 0,      // 1200 MHz (default)
    OC_PROFILE_MILD = 1,       // 1300 MHz (+8%)
    OC_PROFILE_MODERATE = 2,   // 1400 MHz (+17%)
    OC_PROFILE_HIGH = 3,       // 1500 MHz (+25%)
    OC_PROFILE_EXTREME = 4,    // 1600-1700 MHz (risky)
    OC_PROFILE_CUSTOM = 5      // User-defined
} OverclockProfile;

typedef struct {
    bool enabled;
    int frequency;             // Current frequency in MHz
    int thermal_limit;         // Thermal limit in Celsius
    OverclockProfile profile;
    bool thermal_protection;   // Enable automatic thermal throttling
    bool warning_shown;        // Warning dialog shown to user
    time_t last_thermal_check;
    int target_frequency;      // Target frequency before throttling
} OverclockState;

static OverclockState oc_state = {
    .enabled = false,
    .frequency = CPU_FREQ_STOCK,
    .thermal_limit = THERMAL_TEMP_WARNING,
    .profile = OC_PROFILE_STOCK,
    .thermal_protection = true,
    .warning_shown = false,
    .last_thermal_check = 0,
    .target_frequency = CPU_FREQ_STOCK
};

/**
 * Get frequency for a given profile
 */
int overclock_get_profile_frequency(OverclockProfile profile)
{
    switch (profile) {
        case OC_PROFILE_STOCK: return 1200;
        case OC_PROFILE_MILD: return 1300;
        case OC_PROFILE_MODERATE: return 1400;
        case OC_PROFILE_HIGH: return 1500;
        case OC_PROFILE_EXTREME: return 1700;
        default: return CPU_FREQ_STOCK;
    }
}

/**
 * Apply CPU frequency using cpuclock tool
 */
bool overclock_apply_frequency(int freq_mhz)
{
    if (freq_mhz < CPU_FREQ_MIN || freq_mhz > CPU_FREQ_MAX) {
        print_debug("[Overclock] Invalid frequency: %d MHz", freq_mhz);
        return false;
    }
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/mnt/SDCARD/bin/cpuclock %d > /dev/null 2>&1", freq_mhz);
    
    int ret = system(cmd);
    if (ret == 0) {
        oc_state.frequency = freq_mhz;
        print_debug("[Overclock] Applied frequency: %d MHz", freq_mhz);
        return true;
    }
    
    print_debug("[Overclock] Failed to apply frequency: %d MHz", freq_mhz);
    return false;
}

/**
 * Enable overclocking with specified frequency
 */
bool overclock_enable(int freq_mhz, int thermal_limit)
{
    if (freq_mhz < CPU_FREQ_MIN || freq_mhz > CPU_FREQ_MAX) {
        return false;
    }
    
    if (thermal_limit < 60 || thermal_limit > 80) {
        thermal_limit = THERMAL_TEMP_WARNING;
    }
    
    print_debug("[Overclock] Enabling: %d MHz, thermal limit: %d°C", freq_mhz, thermal_limit);
    
    oc_state.target_frequency = freq_mhz;
    oc_state.thermal_limit = thermal_limit;
    oc_state.enabled = true;
    
    return overclock_apply_frequency(freq_mhz);
}

/**
 * Disable overclocking (revert to stock)
 */
bool overclock_disable(void)
{
    print_debug("[Overclock] Disabling, reverting to stock frequency");
    
    oc_state.enabled = false;
    oc_state.target_frequency = CPU_FREQ_STOCK;
    
    return overclock_apply_frequency(CPU_FREQ_STOCK);
}

/**
 * Set overclocking profile
 */
bool overclock_set_profile(OverclockProfile profile)
{
    oc_state.profile = profile;
    
    if (profile == OC_PROFILE_CUSTOM) {
        return true;  // Custom frequency set separately
    }
    
    int freq = overclock_get_profile_frequency(profile);
    
    if (oc_state.enabled) {
        oc_state.target_frequency = freq;
        return overclock_apply_frequency(freq);
    }
    else {
        oc_state.target_frequency = freq;
        return true;
    }
}

/**
 * Thermal management - check temperature and throttle if needed
 * Should be called periodically (e.g., every 5 seconds)
 */
void overclock_thermal_check(void)
{
    if (!oc_state.enabled || !oc_state.thermal_protection) {
        return;
    }
    
    time_t now = time(NULL);
    if (now - oc_state.last_thermal_check < 5) {
        return;  // Check every 5 seconds
    }
    oc_state.last_thermal_check = now;
    
    thermal_update();
    int temp = thermal_current_temp();
    
    // Emergency shutdown if too hot
    if (thermal_emergency_check()) {
        print_debug("[Overclock] EMERGENCY: Temperature too high (%d°C), disabling overclock", temp);
        overclock_disable();
        sync();
        return;
    }
    
    // Thermal throttling
    if (temp >= oc_state.thermal_limit) {
        if (oc_state.frequency > CPU_FREQ_STOCK) {
            int throttled_freq = oc_state.frequency - CPU_FREQ_STEP;
            if (throttled_freq < CPU_FREQ_STOCK) {
                throttled_freq = CPU_FREQ_STOCK;
            }
            print_debug("[Overclock] Thermal throttling: %d°C, reducing to %d MHz", temp, throttled_freq);
            overclock_apply_frequency(throttled_freq);
        }
    }
    else if (temp < oc_state.thermal_limit - 5) {
        // Temperature dropped, restore target frequency
        if (oc_state.frequency < oc_state.target_frequency) {
            print_debug("[Overclock] Temperature safe (%d°C), restoring to %d MHz", temp, oc_state.target_frequency);
            overclock_apply_frequency(oc_state.target_frequency);
        }
    }
}

/**
 * Get current overclock state
 */
OverclockState* overclock_get_state(void)
{
    return &oc_state;
}

/**
 * Check if overclocking is enabled
 */
bool overclock_is_enabled(void)
{
    return oc_state.enabled;
}

/**
 * Get current frequency
 */
int overclock_get_frequency(void)
{
    return oc_state.frequency;
}

/**
 * Set warning shown flag
 */
void overclock_set_warning_shown(bool shown)
{
    oc_state.warning_shown = shown;
}

/**
 * Check if warning was shown
 */
bool overclock_is_warning_shown(void)
{
    return oc_state.warning_shown;
}

/**
 * Validate frequency is safe
 */
bool overclock_is_frequency_safe(int freq_mhz)
{
    return freq_mhz >= CPU_FREQ_MIN && freq_mhz <= CPU_FREQ_SAFE_MAX;
}

/**
 * Get performance gain percentage
 */
int overclock_get_performance_gain(int freq_mhz)
{
    return ((freq_mhz - CPU_FREQ_STOCK) * 100) / CPU_FREQ_STOCK;
}

#endif // TWEAKS_OVERCLOCK_H__
