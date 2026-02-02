#ifndef DEVICE_MODEL_H__
#define DEVICE_MODEL_H__

#include "utils/file.h"
#include <stdio.h>

#define MIYOO283 283
#define MIYOO354 354

static int DEVICE_ID;
static char DEVICE_SN[13];

/**
 * @brief Get device model
 * MM = Miyoo mini
 * MMP = Miyoo mini plus
 * FIXED: Added error handling and safe defaults
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
    if (DEVICE_ID != MIYOO283 && DEVICE_ID != MIYOO354) {
        // Unknown device model, reset to default
        DEVICE_ID = MIYOO354;
    }
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
