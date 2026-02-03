#ifndef RUMBLE_H__
#define RUMBLE_H__

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "settings.h"
#include "device_model.h"
#include "utils/file.h"
#include "utils/msleep.h"

#define SHORT_PULSE_MS 100
#define SUPER_SHORT_PULSE_MS 50

// Miyoo Flip PWM vibration
#define PWM_VIBRATION_CHIP "/sys/class/pwm/pwmchip0"
#define PWM_VIBRATION_PWM "/sys/class/pwm/pwmchip0/pwm3"
#define PWM_PERIOD_NS 1000000  // 1kHz (1ms period)

static int super_short_timings[] = {0, 25, 50, 75};
static int short_timings[] = {0, 50, 100, 150};

static bool pwm_initialized = false;

/**
 * @brief Initialize PWM vibration for Miyoo Flip
 */
void rumble_pwm_init(void)
{
    if (!is_miyoo_flip() || pwm_initialized) {
        return;
    }
    
    // Export PWM3
    file_write(PWM_VIBRATION_CHIP "/export", "3", 1);
    msleep(10);
    
    // Set period (1kHz = 1ms = 1000000ns)
    file_write(PWM_VIBRATION_PWM "/period", "1000000", 7);
    
    // Set initial duty cycle to 0
    file_write(PWM_VIBRATION_PWM "/duty_cycle", "0", 1);
    
    // Enable PWM
    file_write(PWM_VIBRATION_PWM "/enable", "1", 1);
    
    pwm_initialized = true;
}

/**
 * @brief Set PWM vibration intensity (Miyoo Flip)
 * @param percent Intensity 0-100%
 */
void rumble_pwm_set_intensity(uint8_t percent)
{
    if (!is_miyoo_flip()) {
        return;
    }
    
    if (!pwm_initialized) {
        rumble_pwm_init();
    }
    
    if (percent > 100) percent = 100;
    
    // Calculate duty cycle (0-1000000ns)
    uint32_t duty_ns = (PWM_PERIOD_NS * percent) / 100;
    char buf[16];
    snprintf(buf, sizeof(buf), "%u", duty_ns);
    
    file_write(PWM_VIBRATION_PWM "/duty_cycle", buf, strlen(buf));
}

void rumble(bool enabled)
{
    if (is_miyoo_flip()) {
        // Use PWM for Miyoo Flip
        rumble_pwm_set_intensity(enabled ? 100 : 0);
    } else {
        // Use GPIO for Mini/Mini+
        file_write("/sys/class/gpio/export", "48", 2);
        file_write("/sys/class/gpio/gpio48/direction", "out", 3);
        file_write("/sys/class/gpio/gpio48/value", enabled ? "0" : "1", 1);
    }
}

/**
 * @brief Turns on vibration for 100ms
 *
 */
void short_pulse(void)
{
    if (settings.vibration == 0)
        return;
    rumble(true);
    msleep(short_timings[settings.vibration]);
    rumble(false);
}

/**
 * @brief Turns on vibration for 50ms
 *
 */
void super_short_pulse(void)
{
    if (settings.vibration == 0)
        return;
    rumble(true);
    msleep(super_short_timings[settings.vibration]);
    rumble(false);
}

/**
 * @brief Turns on vibration for 50ms
 *
 */
void menu_short_pulse(void)
{
    if (settings.vibration == 0 || !settings.menu_button_haptics)
        return;
    rumble(true);
    msleep(short_timings[settings.vibration]);
    rumble(false);
}

/**
 * @brief Turns on vibration for 50ms
 *
 */
void menu_super_short_pulse(void)
{
    if (settings.vibration == 0 || !settings.menu_button_haptics)
        return;
    rumble(true);
    msleep(super_short_timings[settings.vibration]);
    rumble(false);
}

#endif // RUMBLE_H__
