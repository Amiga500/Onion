#ifndef DEVICE_MODEL_H__
#define DEVICE_MODEL_H__

#include "utils/file.h"
#include <stdio.h>

#define MIYOO283 283
#define MIYOO354 354
#define MIYOO_FLIP 566  // RK3566 SoC identifier

static int DEVICE_ID;
static char DEVICE_SN[13];

/**
 * @brief Get device model
 * MM = Miyoo mini (283)
 * MMP = Miyoo mini plus (354)
 * FLIP = Miyoo Flip (566)
 * FIXED: Added error handling and safe defaults
 * ADDED: Miyoo Flip detection support
 */

void getDeviceModel(void)
{
    FILE *fp;
    // FIX: Initialize with safe default (assume MMP/354 for better compatibility)
    DEVICE_ID = MIYOO354;
    
    // FIX: Check if file exists before trying to read
    if (!exists("/tmp/deviceModel")) {
        // Device model file not found, using default
        return;
    }
    
    // Try to read device model, keep default if read fails
    file_get(fp, "/tmp/deviceModel", "%d", &DEVICE_ID);
    
    // FIX: Validate device ID is one of the known models
    if (DEVICE_ID != MIYOO283 && DEVICE_ID != MIYOO354 && DEVICE_ID != MIYOO_FLIP) {
        // Unknown device model, reset to default
        DEVICE_ID = MIYOO354;
    }
}

/**
 * @brief Check if device is Miyoo Flip
 */
static inline bool is_miyoo_flip(void) {
    return DEVICE_ID == MIYOO_FLIP;
}

/**
 * @brief Check if device has analog sticks
 */
static inline bool has_analog_sticks(void) {
    return DEVICE_ID == MIYOO_FLIP;
}

/**
 * @brief Check if device has lid sensor
 */
static inline bool has_lid_sensor(void) {
    return DEVICE_ID == MIYOO_FLIP;
}

void getDeviceSerial(void)
{
    FILE *fp;
    // FIX: Initialize with empty string as safe default
    DEVICE_SN[0] = '\0';
    
    // FIX: Check if file exists before trying to read
    if (!exists("/tmp/deviceSN")) {
        // Serial number file not found, using default empty string
        return;
    }
    
    // Try to read serial number, keep empty if read fails
    file_get(fp, "/tmp/deviceSN", "%12[^\n]", DEVICE_SN);
    // FIX: Ensure null termination
    DEVICE_SN[12] = '\0';
}

#endif // DEVICE_MODEL_H__
