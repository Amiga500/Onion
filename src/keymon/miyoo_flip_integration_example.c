/**
 * @file miyoo_flip_integration_example.c
 * @brief Example integration of Miyoo Flip features in keymon
 * 
 * This file shows how to integrate the new Miyoo Flip support into
 * the existing keymon input monitoring system.
 * 
 * To integrate:
 * 1. Add these includes to keymon.c
 * 2. Call miyoo_flip_init() in main()
 * 3. Call miyoo_flip_process_event() in event loop
 * 4. Call miyoo_flip_cleanup() on exit
 */

#include <stdbool.h>
#include <stdio.h>
#include <linux/input.h>

// New Miyoo Flip headers
#include "../common/system/device_model.h"
#include "../common/system/analog_mapper.h"
#include "../common/system/lid_sensor.h"
#include "../common/system/rk809_pmic.h"

/**
 * @brief Initialize Miyoo Flip specific features
 * Call this once at startup in keymon main()
 */
void miyoo_flip_init(void)
{
    // Get device model
    getDeviceModel();
    
    if (!is_miyoo_flip()) {
        printf_debug("Not a Miyoo Flip, skipping Flip-specific init\n");
        return;
    }
    
    printf_debug("Miyoo Flip detected (ID: %d)\n", DEVICE_ID);
    
    // Initialize lid sensor
    if (lid_sensor_init()) {
        printf_debug("Lid sensor initialized\n");
        lid_sensor_set_suspend_on_close(true);
        lid_sensor_set_save_on_close(true);
    } else {
        printf_debug("Warning: Lid sensor initialization failed\n");
    }
    
    // Initialize RK809 PMIC for battery monitoring
    if (rk809_init()) {
        printf_debug("RK809 PMIC initialized\n");
        battery_state_t bat = rk809_get_battery_state();
        printf_debug("Battery: %d%%, %dmV, %s\n", 
                     bat.percent, bat.voltage_mv,
                     bat.charging ? "charging" : "discharging");
    } else {
        printf_debug("Warning: RK809 PMIC initialization failed\n");
    }
    
    // Initialize PWM vibration
    rumble_pwm_init();
    printf_debug("PWM vibration initialized\n");
    
    printf_debug("Miyoo Flip initialization complete\n");
}

/**
 * @brief Process Miyoo Flip specific events
 * Call this in the main event loop after reading input_event
 * 
 * @param ev Pointer to input_event structure
 * @return true if event was handled by Flip-specific code
 */
bool miyoo_flip_process_event(struct input_event *ev)
{
    if (!is_miyoo_flip()) {
        return false;
    }
    
    // Process analog stick events
    if (has_analog_sticks() && 
        (ev->type == EV_ABS || 
         (ev->type == EV_KEY && (ev->code == BTN_THUMBL || ev->code == BTN_THUMBR)))) {
        
        analog_update_state(ev);
        
        // Get current analog state
        analog_state_t state = analog_get_state();
        
        // Debug output for analog sticks (can be removed in production)
        if (ev->type == EV_ABS) {
            printf_debug("Analog: L(%d,%d) R(%d,%d) L3:%d R3:%d\n",
                         state.lx, state.ly, state.rx, state.ry,
                         state.l3_pressed, state.r3_pressed);
        }
        
        // Event handled by analog mapper
        return true;
    }
    
    // Process lid sensor events
    if (has_lid_sensor() && ev->type == EV_SW && ev->code == SW_LID) {
        lid_sensor_process_event(ev);
        
        if (lid_sensor_is_closed()) {
            printf_debug("Lid closed - suspending\n");
        } else {
            printf_debug("Lid opened - resuming\n");
        }
        
        // Event handled by lid sensor
        return true;
    }
    
    // Event not handled by Flip-specific code
    return false;
}

/**
 * @brief Poll for lid sensor events (non-blocking)
 * Call this periodically in the main loop
 */
void miyoo_flip_poll_lid_sensor(void)
{
    if (!is_miyoo_flip() || !has_lid_sensor()) {
        return;
    }
    
    // Non-blocking poll for lid events
    if (lid_sensor_poll()) {
        printf_debug("Lid event processed\n");
    }
}

/**
 * @brief Update battery status (for display/monitoring)
 * Call this periodically (e.g., every 5 seconds)
 */
void miyoo_flip_update_battery(void)
{
    if (!is_miyoo_flip()) {
        return;
    }
    
    battery_state_t bat = rk809_get_battery_state();
    
    // Check for low battery warning
    if (rk809_is_battery_low() && !bat.charging) {
        printf_debug("WARNING: Battery low (%d%%)\n", bat.percent);
        // Could trigger UI warning here
    }
    
    // Check for critical battery
    if (rk809_is_battery_critical() && !bat.charging) {
        printf_debug("CRITICAL: Battery critical (%d%%), preparing shutdown\n", bat.percent);
        // Could trigger emergency save and shutdown
    }
}

/**
 * @brief Cleanup Miyoo Flip features
 * Call this on keymon exit
 */
void miyoo_flip_cleanup(void)
{
    if (!is_miyoo_flip()) {
        return;
    }
    
    printf_debug("Cleaning up Miyoo Flip features\n");
    
    // Close lid sensor
    lid_sensor_close();
    
    // Close RK809 PMIC
    rk809_close();
    
    printf_debug("Miyoo Flip cleanup complete\n");
}

/**
 * @brief Example: Get analog stick state for game input
 * This shows how a game or emulator might read analog state
 */
void example_read_analog_for_game(void)
{
    if (!has_analog_sticks()) {
        return;
    }
    
    analog_state_t state = analog_get_state();
    
    // Example: Map to game controls
    // Left stick: Movement
    // Right stick: Camera/Aim
    
    // Normalized values are in range [-512, +512]
    // Convert to [-1.0, +1.0] for easier use
    float left_x = (float)state.lx / 512.0f;
    float left_y = (float)state.ly / 512.0f;
    float right_x = (float)state.rx / 512.0f;
    float right_y = (float)state.ry / 512.0f;
    
    printf_debug("Analog normalized: L(%.2f,%.2f) R(%.2f,%.2f)\n",
                 left_x, left_y, right_x, right_y);
}

/**
 * @brief Example: Configure analog stick settings
 * This shows how to adjust deadzone, sensitivity, curve
 */
void example_configure_analog(void)
{
    if (!has_analog_sticks()) {
        return;
    }
    
    // Set left stick deadzone to 60 (higher for loose sticks)
    analog_set_deadzone(0, 60);
    
    // Set right stick sensitivity to 1.2 (20% more sensitive for aiming)
    analog_set_sensitivity(1, 1.2f);
    
    // Set left stick to squared curve (more precision at center)
    analog_set_curve(0, ANALOG_CURVE_SQUARED);
    
    printf_debug("Analog configuration updated\n");
}

/**
 * @brief Example: Test vibration/rumble
 */
void example_test_vibration(void)
{
    if (!is_miyoo_flip()) {
        return;
    }
    
    printf_debug("Testing vibration...\n");
    
    // Light vibration (25%)
    rumble_pwm_set_intensity(25);
    msleep(200);
    
    // Medium vibration (50%)
    rumble_pwm_set_intensity(50);
    msleep(200);
    
    // Strong vibration (75%)
    rumble_pwm_set_intensity(75);
    msleep(200);
    
    // Maximum vibration (100%)
    rumble_pwm_set_intensity(100);
    msleep(200);
    
    // Off
    rumble_pwm_set_intensity(0);
    
    printf_debug("Vibration test complete\n");
}

/*
 * INTEGRATION EXAMPLE IN keymon.c:
 * 
 * int main(int argc, char *argv[])
 * {
 *     // ... existing initialization ...
 *     
 *     // Initialize Miyoo Flip features
 *     miyoo_flip_init();
 *     
 *     // Main event loop
 *     while (running) {
 *         // Read input event
 *         if (read(input_fd, &ev, sizeof(ev)) == sizeof(ev)) {
 *             
 *             // Try Miyoo Flip specific processing first
 *             if (miyoo_flip_process_event(&ev)) {
 *                 // Event was handled by Flip-specific code
 *                 continue;
 *             }
 *             
 *             // ... existing event processing ...
 *         }
 *         
 *         // Poll lid sensor (non-blocking)
 *         miyoo_flip_poll_lid_sensor();
 *         
 *         // Update battery status periodically
 *         static time_t last_battery_update = 0;
 *         time_t now = time(NULL);
 *         if (now - last_battery_update >= 5) {
 *             miyoo_flip_update_battery();
 *             last_battery_update = now;
 *         }
 *     }
 *     
 *     // Cleanup
 *     miyoo_flip_cleanup();
 *     
 *     return 0;
 * }
 */
