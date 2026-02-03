#ifndef ANALOG_MAPPER_H__
#define ANALOG_MAPPER_H__

#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "device_model.h"

/**
 * @file analog_mapper.h
 * @brief Analog stick mapping and processing for Miyoo Flip
 * 
 * Supports dual analog sticks with deadzone, sensitivity, and curve adjustments
 * Based on Phase 4 and Phase 5 documentation
 */

// Analog stick axes definitions (for Miyoo Flip)
#define HW_AXIS_LX ABS_X        // Left stick X axis
#define HW_AXIS_LY ABS_Y        // Left stick Y axis
#define HW_AXIS_RX ABS_RX       // Right stick X axis
#define HW_AXIS_RY ABS_RY       // Right stick Y axis
#define HW_BTN_L3 BTN_THUMBL    // Left stick button
#define HW_BTN_R3 BTN_THUMBR    // Right stick button

// ADC range for analog sticks (10-bit ADC)
#define ANALOG_MIN 0
#define ANALOG_MAX 1023
#define ANALOG_CENTER 512
#define ANALOG_DEADZONE_DEFAULT 50

// Curve types for analog input
typedef enum {
    ANALOG_CURVE_LINEAR = 0,
    ANALOG_CURVE_SQUARED,
    ANALOG_CURVE_CUBIC
} analog_curve_t;

// Analog stick configuration
typedef struct {
    int16_t deadzone;           // Deadzone threshold (0-512)
    float sensitivity;          // Sensitivity multiplier (0.5-2.0)
    analog_curve_t curve;       // Input curve type
    bool invert_x;              // Invert X axis
    bool invert_y;              // Invert Y axis
} analog_config_t;

// Analog stick state
typedef struct {
    int16_t lx, ly;             // Left stick position (-512 to +512)
    int16_t rx, ry;             // Right stick position (-512 to +512)
    bool l3_pressed;            // L3 button state
    bool r3_pressed;            // R3 button state
    analog_config_t left_config;   // Left stick config
    analog_config_t right_config;  // Right stick config
} analog_state_t;

// Global analog state
static analog_state_t analog_state = {
    .lx = 0, .ly = 0,
    .rx = 0, .ry = 0,
    .l3_pressed = false,
    .r3_pressed = false,
    .left_config = {
        .deadzone = ANALOG_DEADZONE_DEFAULT,
        .sensitivity = 1.0f,
        .curve = ANALOG_CURVE_LINEAR,
        .invert_x = false,
        .invert_y = false
    },
    .right_config = {
        .deadzone = ANALOG_DEADZONE_DEFAULT,
        .sensitivity = 1.0f,
        .curve = ANALOG_CURVE_LINEAR,
        .invert_x = false,
        .invert_y = false
    }
};

/**
 * @brief Apply deadzone to analog value
 * @param value Raw analog value (0-1023)
 * @param deadzone Deadzone threshold
 * @return Processed value (-512 to +512) or 0 if in deadzone
 */
static inline int16_t apply_deadzone(int16_t value, int16_t deadzone)
{
    // Convert to centered range (-512 to +512)
    int16_t centered = value - ANALOG_CENTER;
    
    // Apply deadzone
    if (abs(centered) < deadzone) {
        return 0;
    }
    
    // Scale remaining range
    int16_t sign = (centered < 0) ? -1 : 1;
    int16_t magnitude = abs(centered) - deadzone;
    int16_t max_range = ANALOG_CENTER - deadzone;
    
    return (magnitude * ANALOG_CENTER) / max_range * sign;
}

/**
 * @brief Apply curve to analog value
 * @param value Processed analog value (-512 to +512)
 * @param curve Curve type
 * @return Curved value (-512 to +512)
 */
static inline int16_t apply_curve(int16_t value, analog_curve_t curve)
{
    if (value == 0) return 0;
    
    float normalized = (float)value / ANALOG_CENTER;
    float result;
    
    switch (curve) {
        case ANALOG_CURVE_SQUARED:
            result = normalized * normalized;
            if (normalized < 0) result = -result;
            break;
            
        case ANALOG_CURVE_CUBIC:
            result = normalized * normalized * normalized;
            break;
            
        case ANALOG_CURVE_LINEAR:
        default:
            result = normalized;
            break;
    }
    
    return (int16_t)(result * ANALOG_CENTER);
}

/**
 * @brief Apply sensitivity to analog value
 * @param value Processed analog value (-512 to +512)
 * @param sensitivity Sensitivity multiplier
 * @return Scaled value (-512 to +512), clamped
 */
static inline int16_t apply_sensitivity(int16_t value, float sensitivity)
{
    int16_t result = (int16_t)((float)value * sensitivity);
    
    // Clamp to valid range
    if (result > ANALOG_CENTER) result = ANALOG_CENTER;
    if (result < -ANALOG_CENTER) result = -ANALOG_CENTER;
    
    return result;
}

/**
 * @brief Process raw analog value with config
 * @param raw_value Raw ADC value (0-1023)
 * @param config Analog configuration
 * @return Processed value (-512 to +512)
 */
static inline int16_t process_analog_value(int16_t raw_value, analog_config_t *config)
{
    // Apply deadzone
    int16_t value = apply_deadzone(raw_value, config->deadzone);
    
    if (value == 0) return 0;
    
    // Apply curve
    value = apply_curve(value, config->curve);
    
    // Apply sensitivity
    value = apply_sensitivity(value, config->sensitivity);
    
    return value;
}

/**
 * @brief Update analog state from input event
 * @param ev Input event from kernel
 */
static inline void analog_update_state(struct input_event *ev)
{
    if (!has_analog_sticks()) return;
    
    switch (ev->type) {
        case EV_ABS:
            switch (ev->code) {
                case ABS_X:  // Left stick X
                    analog_state.lx = process_analog_value(ev->value, &analog_state.left_config);
                    if (analog_state.left_config.invert_x) analog_state.lx = -analog_state.lx;
                    break;
                    
                case ABS_Y:  // Left stick Y
                    analog_state.ly = process_analog_value(ev->value, &analog_state.left_config);
                    if (analog_state.left_config.invert_y) analog_state.ly = -analog_state.ly;
                    break;
                    
                case ABS_RX: // Right stick X
                    analog_state.rx = process_analog_value(ev->value, &analog_state.right_config);
                    if (analog_state.right_config.invert_x) analog_state.rx = -analog_state.rx;
                    break;
                    
                case ABS_RY: // Right stick Y
                    analog_state.ry = process_analog_value(ev->value, &analog_state.right_config);
                    if (analog_state.right_config.invert_y) analog_state.ry = -analog_state.ry;
                    break;
            }
            break;
            
        case EV_KEY:
            switch (ev->code) {
                case BTN_THUMBL: // L3
                    analog_state.l3_pressed = (ev->value != 0);
                    break;
                    
                case BTN_THUMBR: // R3
                    analog_state.r3_pressed = (ev->value != 0);
                    break;
            }
            break;
    }
}

/**
 * @brief Get current analog state
 * @return Current analog stick state
 */
static inline analog_state_t analog_get_state(void)
{
    return analog_state;
}

/**
 * @brief Set analog deadzone
 * @param stick 0=left, 1=right
 * @param deadzone Deadzone value (0-512)
 */
static inline void analog_set_deadzone(int stick, int16_t deadzone)
{
    if (deadzone < 0) deadzone = 0;
    if (deadzone > ANALOG_CENTER) deadzone = ANALOG_CENTER;
    
    if (stick == 0) {
        analog_state.left_config.deadzone = deadzone;
    } else {
        analog_state.right_config.deadzone = deadzone;
    }
}

/**
 * @brief Set analog sensitivity
 * @param stick 0=left, 1=right
 * @param sensitivity Sensitivity value (0.5-2.0)
 */
static inline void analog_set_sensitivity(int stick, float sensitivity)
{
    if (sensitivity < 0.5f) sensitivity = 0.5f;
    if (sensitivity > 2.0f) sensitivity = 2.0f;
    
    if (stick == 0) {
        analog_state.left_config.sensitivity = sensitivity;
    } else {
        analog_state.right_config.sensitivity = sensitivity;
    }
}

/**
 * @brief Set analog curve type
 * @param stick 0=left, 1=right
 * @param curve Curve type
 */
static inline void analog_set_curve(int stick, analog_curve_t curve)
{
    if (stick == 0) {
        analog_state.left_config.curve = curve;
    } else {
        analog_state.right_config.curve = curve;
    }
}

#endif // ANALOG_MAPPER_H__
