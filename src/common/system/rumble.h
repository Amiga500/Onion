#ifndef RUMBLE_H__
#define RUMBLE_H__

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "settings.h"
#include "utils/file.h"
#include "utils/msleep.h"

#define SHORT_PULSE_MS 100
#define SUPER_SHORT_PULSE_MS 50

static int super_short_timings[] = {0, 25, 50, 75};
static int short_timings[] = {0, 50, 100, 150};
#define VIBRATION_LEVEL_MAX ((int)(sizeof(short_timings) / sizeof(short_timings[0])) - 1)
#define CLAMP_VIBRATION(v) ((v) > VIBRATION_LEVEL_MAX ? VIBRATION_LEVEL_MAX : (v))

void rumble(bool enabled)
{
    static bool gpio_initialized = false;
    /*
     * GPIO pin for vibration motor — must be verified against actual hardware.
     * Miyoo Mini uses GPIO48; Miyoo Flip pin TBD (update when hardware available).
     */
    static const char *gpio_num = "48";
    static char gpio_dir_path[64] = {0};
    static char gpio_val_path[64] = {0};

    if (!gpio_initialized) {
        snprintf(gpio_dir_path, sizeof(gpio_dir_path),
                 "/sys/class/gpio/gpio%s/direction", gpio_num);
        snprintf(gpio_val_path, sizeof(gpio_val_path),
                 "/sys/class/gpio/gpio%s/value", gpio_num);
        file_write("/sys/class/gpio/export", gpio_num, strlen(gpio_num));
        file_write(gpio_dir_path, "out", 3);
        gpio_initialized = true;
    }
    file_write(gpio_val_path, enabled ? "0" : "1", 1);
}

/**
 * @brief Turns on vibration for 100ms
 *
 */
void short_pulse(void)
{
    if (settings.vibration == 0)
        return;
    int level = CLAMP_VIBRATION(settings.vibration);
    rumble(true);
    msleep(short_timings[level]);
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
    int level = CLAMP_VIBRATION(settings.vibration);
    rumble(true);
    msleep(super_short_timings[level]);
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
    int level = CLAMP_VIBRATION(settings.vibration);
    rumble(true);
    msleep(short_timings[level]);
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
    int level = CLAMP_VIBRATION(settings.vibration);
    rumble(true);
    msleep(super_short_timings[level]);
    rumble(false);
}

#endif // RUMBLE_H__
