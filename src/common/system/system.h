#ifndef SYSTEM_H__
#define SYSTEM_H__

#include <fcntl.h>
#include <stdbool.h>

#include "clock.h"
#include "utils/flags.h"

// system directories
#define GPIO_DIR1 "/sys/class/gpio/"
#define GPIO_DIR2 "/sys/devices/gpiochip0/gpio/"
#define PWM_DIR "/sys/devices/soc0/soc/1f003400.pwm/pwm/pwmchip0/"
#define CPU_DIR "/sys/devices/system/cpu/cpufreq/policy0/"

#define CPU_SCALING_MIN_FREQ CPU_DIR "scaling_min_freq"
#define CPU_SCALING_GOVERNOR CPU_DIR "scaling_governor"
// What powersave changed, kept while it is on (system_powersave_recover).
#ifndef POWERSAVE_STATE_PATH /* tests point it at a folder of their own */
#define POWERSAVE_STATE_PATH "/tmp/.powersave_state"
#endif

#define system_powersave_on() system_powersave(true)
#define system_powersave_off() system_powersave(false)

/**
 * @brief Creates the file `/tmp/.offOrder`,
 * which is used by `runtime.sh` for knowing when to show shutdown screen.
 *
 */
void set_system_shutdown(void)
{
    temp_flag_set(".offOrder", true);
    system_clock_get();
    system_clock_save();
    sync();
}

/**
 * @brief Toggle powersave mode
 *
 * @param enabled true: turn on powersave, false: turn off powersave
 */
void system_powersave(bool enabled)
{
    static uint32_t saved_min_freq = 0;
    static char saved_governor[16];
    static bool saved = false;
    FILE *fp;

    if (enabled) {
        char buffer[128];
        FILE *pipe = popen("cpuclock", "r");

        saved_min_freq = 0;
        if (pipe) {
            while (fgets(buffer, sizeof(buffer), pipe) != NULL)
                saved_min_freq = atoi(buffer);
            pclose(pipe); // was also called with NULL when popen() failed
        }

        // save values for restoring later
        saved_governor[0] = '\0';
        file_get(fp, CPU_SCALING_GOVERNOR, "%15s", saved_governor);
        saved = true;
        {
            FILE *state = fopen(POWERSAVE_STATE_PATH, "w");
            if (state != NULL) {
                fprintf(state, "%u %s\n", saved_min_freq, saved_governor);
                fclose(state);
            }
        }
        // set powersaving values
        file_put(fp, CPU_SCALING_MIN_FREQ, "%u", 0);
        file_put(fp, CPU_SCALING_GOVERNOR, "%s", "powersave");
    }
    else if (saved) {
        // Restore only what powersave_on() read: without it, this wrote an
        // empty governor and ran "cpuclock 0".
        if (saved_min_freq > 0) {
            char cmd[32]; // [15] overflowed for a value of 6+ digits
            snprintf(cmd, sizeof(cmd), "cpuclock %u", saved_min_freq);
            system(cmd);
        }
        if (saved_governor[0] != '\0')
            file_put(fp, CPU_SCALING_GOVERNOR, "%s", saved_governor);
        saved = false;
        remove(POWERSAVE_STATE_PATH);
    }
}

/**
 * @brief Undo a powersave left on by a program that ended during standby
 * (keymon after a crash): the CPU clock and governor saved in /tmp go back.
 * Does nothing when powersave is not on.
 */
void system_powersave_recover(void)
{
    FILE *fp;
    FILE *state = fopen(POWERSAVE_STATE_PATH, "r");
    if (state == NULL)
        return;
    unsigned int clock_mhz = 0;
    char governor[16] = "";
    if (fscanf(state, "%u %15s", &clock_mhz, governor) < 1)
        clock_mhz = 0;
    fclose(state);
    remove(POWERSAVE_STATE_PATH);

    if (clock_mhz > 0) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "cpuclock %u", clock_mhz);
        system(cmd);
    }
    if (governor[0] != 0)
        file_put(fp, CPU_SCALING_GOVERNOR, "%s", governor);
}

#endif // SYSTEM_H__
