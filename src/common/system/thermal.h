#ifndef SYSTEM_THERMAL_H__
#define SYSTEM_THERMAL_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "system/axp.h"

// Temperature thresholds (in Celsius)
#define THERMAL_TEMP_SAFE 60       // Safe operating temperature
#define THERMAL_TEMP_WARNING 65    // Warning threshold
#define THERMAL_TEMP_CRITICAL 75   // Critical - force throttle
#define THERMAL_TEMP_EMERGENCY 80  // Emergency shutdown

// AXP223 temperature registers
#define AXP_REG_TEMP_MSB 0x5E
#define AXP_REG_TEMP_LSB 0x5F

typedef struct {
    int temperature;          // Current temperature in Celsius
    int max_temperature;      // Maximum temperature recorded
    time_t last_check;        // Last temperature check time
    bool throttle_active;     // Thermal throttling active
    int throttle_count;       // Number of throttling events
} ThermalInfo;

static ThermalInfo thermal_info = {
    .temperature = 0,
    .max_temperature = 0,
    .last_check = 0,
    .throttle_active = false,
    .throttle_count = 0
};

/**
 * Read temperature from AXP223 PMIC
 * 
 * Returns temperature in Celsius (0-100) or -1 on error
 * 
 * Formula: T(°C) = (value * 0.1) - 144.7
 * The AXP223 provides 12-bit temperature data in registers 0x5E-0x5F
 */
int thermal_get_temperature(void)
{
    int msb = axp_read(AXP_REG_TEMP_MSB);
    int lsb = axp_read(AXP_REG_TEMP_LSB);
    
    if (msb < 0 || lsb < 0) {
        return -1;  // Error reading temperature
    }
    
    // Combine MSB and LSB (12-bit value)
    int raw_value = ((msb & 0x0F) << 8) | lsb;
    
    // Convert to Celsius: (value * 0.1) - 144.7
    // Using integer math: (value - 1447) / 10
    int temp_celsius = (raw_value - 1447) / 10;
    
    // Clamp to reasonable range (0-100°C)
    if (temp_celsius < 0) temp_celsius = 0;
    if (temp_celsius > 100) temp_celsius = 100;
    
    return temp_celsius;
}

/**
 * Update thermal information
 * 
 * Should be called periodically (e.g., every 5 seconds)
 * Returns true if temperature is safe, false if throttling needed
 */
bool thermal_update(void)
{
    thermal_info.temperature = thermal_get_temperature();
    thermal_info.last_check = time(NULL);
    
    // Update maximum temperature
    if (thermal_info.temperature > thermal_info.max_temperature) {
        thermal_info.max_temperature = thermal_info.temperature;
    }
    
    // Check if thermal throttling is needed
    if (thermal_info.temperature >= THERMAL_TEMP_CRITICAL) {
        if (!thermal_info.throttle_active) {
            thermal_info.throttle_count++;
        }
        thermal_info.throttle_active = true;
        return false;  // Throttling needed
    }
    else if (thermal_info.temperature < THERMAL_TEMP_WARNING) {
        thermal_info.throttle_active = false;
    }
    
    return thermal_info.temperature < THERMAL_TEMP_CRITICAL;
}

/**
 * Check if temperature is in safe range
 */
bool thermal_is_safe(void)
{
    return thermal_info.temperature < THERMAL_TEMP_WARNING;
}

/**
 * Check if thermal throttling is active
 */
bool thermal_is_throttling(void)
{
    return thermal_info.throttle_active;
}

/**
 * Get current temperature
 */
int thermal_current_temp(void)
{
    return thermal_info.temperature;
}

/**
 * Get maximum recorded temperature
 */
int thermal_max_temp(void)
{
    return thermal_info.max_temperature;
}

/**
 * Reset thermal statistics
 */
void thermal_reset_stats(void)
{
    thermal_info.max_temperature = thermal_info.temperature;
    thermal_info.throttle_count = 0;
}

/**
 * Get throttle event count
 */
int thermal_throttle_count(void)
{
    return thermal_info.throttle_count;
}

/**
 * Check if emergency shutdown is needed
 */
bool thermal_emergency_check(void)
{
    return thermal_info.temperature >= THERMAL_TEMP_EMERGENCY;
}

#endif // SYSTEM_THERMAL_H__
