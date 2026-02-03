#ifndef LID_SENSOR_H__
#define LID_SENSOR_H__

#include <fcntl.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "device_model.h"
#include "utils/file.h"

/**
 * @file lid_sensor.h
 * @brief Lid sensor (clamshell) support for Miyoo Flip
 * 
 * Hall effect magnetic sensor for detecting lid open/close
 * Based on Phase 4 and Phase 5 documentation
 */

// Lid sensor device path
#define LID_SENSOR_DEVICE "/dev/input/by-path/platform-lid-sensor-event"
#define LID_SENSOR_FALLBACK "/dev/input/event2"

// Lid state
typedef enum {
    LID_STATE_OPEN = 0,
    LID_STATE_CLOSED = 1,
    LID_STATE_UNKNOWN = -1
} lid_state_t;

// Lid sensor data
typedef struct {
    int fd;                     // File descriptor
    lid_state_t state;          // Current state
    bool initialized;           // Init flag
    bool suspend_on_close;      // Auto-suspend enabled
    bool save_on_close;         // Auto-save state enabled
} lid_sensor_t;

// Global lid sensor state
static lid_sensor_t lid_sensor = {
    .fd = -1,
    .state = LID_STATE_UNKNOWN,
    .initialized = false,
    .suspend_on_close = true,
    .save_on_close = true
};

/**
 * @brief Initialize lid sensor
 * @return true if successful
 */
static inline bool lid_sensor_init(void)
{
    if (!has_lid_sensor()) {
        return false;
    }
    
    // Try primary device path
    lid_sensor.fd = open(LID_SENSOR_DEVICE, O_RDONLY | O_NONBLOCK);
    
    // Try fallback path
    if (lid_sensor.fd < 0) {
        lid_sensor.fd = open(LID_SENSOR_FALLBACK, O_RDONLY | O_NONBLOCK);
    }
    
    if (lid_sensor.fd < 0) {
        return false;
    }
    
    lid_sensor.initialized = true;
    lid_sensor.state = LID_STATE_OPEN; // Assume open on init
    
    return true;
}

/**
 * @brief Close lid sensor
 */
static inline void lid_sensor_close(void)
{
    if (lid_sensor.fd >= 0) {
        close(lid_sensor.fd);
        lid_sensor.fd = -1;
    }
    lid_sensor.initialized = false;
}

/**
 * @brief Get current lid state
 * @return Current lid state
 */
static inline lid_state_t lid_sensor_get_state(void)
{
    return lid_sensor.state;
}

/**
 * @brief Check if lid is open
 * @return true if open
 */
static inline bool lid_sensor_is_open(void)
{
    return lid_sensor.state == LID_STATE_OPEN;
}

/**
 * @brief Check if lid is closed
 * @return true if closed
 */
static inline bool lid_sensor_is_closed(void)
{
    return lid_sensor.state == LID_STATE_CLOSED;
}

/**
 * @brief Handle lid close event
 */
static inline void handle_lid_close_event(void)
{
    if (!lid_sensor.suspend_on_close) {
        return;
    }
    
    // Save screenshot if enabled
    if (lid_sensor.save_on_close) {
        // Take screenshot before suspending
        system("sync");
    }
    
    // Turn off display
    file_write("/sys/class/backlight/backlight/brightness", "0", 1);
    
    // Pause audio (if running)
    system("killall -STOP mpg123 2>/dev/null");
    
    // Suspend RetroArch (if running)
    system("killall -STOP retroarch 2>/dev/null");
    
    // Enter system suspend
    file_write("/sys/power/state", "mem", 3);
}

/**
 * @brief Handle lid open event
 */
static inline void handle_lid_open_event(void)
{
    // Wake from suspend (automatic by kernel)
    
    // Restore display brightness (read from settings)
    // This would be integrated with settings.h in real implementation
    file_write("/sys/class/backlight/backlight/brightness", "50", 2);
    
    // Resume audio
    system("killall -CONT mpg123 2>/dev/null");
    
    // Resume RetroArch
    system("killall -CONT retroarch 2>/dev/null");
}

/**
 * @brief Process lid sensor event
 * @param ev Input event from kernel
 */
static inline void lid_sensor_process_event(struct input_event *ev)
{
    if (!lid_sensor.initialized) {
        return;
    }
    
    if (ev->type == EV_SW && ev->code == SW_LID) {
        lid_state_t new_state = (ev->value == 0) ? LID_STATE_OPEN : LID_STATE_CLOSED;
        
        if (new_state != lid_sensor.state) {
            lid_sensor.state = new_state;
            
            if (new_state == LID_STATE_CLOSED) {
                handle_lid_close_event();
            } else {
                handle_lid_open_event();
            }
        }
    }
}

/**
 * @brief Read and process lid sensor event (non-blocking)
 * @return true if event was processed
 */
static inline bool lid_sensor_poll(void)
{
    if (!lid_sensor.initialized || lid_sensor.fd < 0) {
        return false;
    }
    
    struct input_event ev;
    ssize_t bytes = read(lid_sensor.fd, &ev, sizeof(ev));
    
    if (bytes == sizeof(ev)) {
        lid_sensor_process_event(&ev);
        return true;
    }
    
    return false;
}

/**
 * @brief Enable/disable auto-suspend on lid close
 * @param enabled true to enable
 */
static inline void lid_sensor_set_suspend_on_close(bool enabled)
{
    lid_sensor.suspend_on_close = enabled;
}

/**
 * @brief Enable/disable auto-save on lid close
 * @param enabled true to enable
 */
static inline void lid_sensor_set_save_on_close(bool enabled)
{
    lid_sensor.save_on_close = enabled;
}

#endif // LID_SENSOR_H__
