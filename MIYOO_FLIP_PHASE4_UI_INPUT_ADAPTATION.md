# Fase 4 – Adattamento UI e Input System per Miyoo Flip

**Data:** Febbraio 2026  
**Versione:** 1.0  
**Target:** Miyoo Flip (Rockchip RK3566)  
**Status:** Documentazione implementazione

---

## Indice

1. [Panoramica Modifiche](#panoramica-modifiche)
2. [Dual Analog Stick Support](#dual-analog-stick-support)
3. [Lid Sensor Sleep/Wake](#lid-sensor-sleepwake)
4. [Vibration PWM Feedback](#vibration-pwm-feedback)
5. [Aspect Ratio e Display](#aspect-ratio-e-display)
6. [Input Lag Optimization](#input-lag-optimization)
7. [Device Model Detection](#device-model-detection)
8. [Build e Testing](#build-e-testing)
9. [Checklist Implementazione](#checklist-implementazione)

---

## 1. Panoramica Modifiche

### File da Modificare

| File | Modifiche | Priorità | LOC |
|------|-----------|----------|-----|
| `src/common/system/device_model.h` | +Miyoo Flip detection | 🔴 P0 | ~30 |
| `src/common/system/keymap_hw.h` | +Analog axes defines | 🔴 P0 | ~20 |
| `src/keymon/input_fd.h` | +EV_ABS handling | 🔴 P0 | ~150 |
| `src/keymon/keymon.c` | +Analog + lid logic | 🔴 P0 | ~200 |
| `src/common/system/rumble.h` | PWM vibration | 🟡 P1 | ~80 |
| `src/common/system/display.h` | Minor fixes | 🟢 P2 | ~20 |

**Totale stimato:** ~500 LOC di modifiche

---

## 2. Dual Analog Stick Support

### 2.1 Hardware Mapping

**Miyoo Flip Input System:**
- **Left Analog:** SARADC CH0 (X-axis), CH1 (Y-axis)
- **Right Analog:** SARADC CH2 (RX-axis), CH3 (RY-axis)
- **Range:** 0-1023 (10-bit ADC)
- **Center:** ~512
- **Deadzone:** ±50 units (configurabile)

**Linux Input Events:**
```c
// Left stick
ABS_X    (code: 0x00)  // Left stick X
ABS_Y    (code: 0x01)  // Left stick Y

// Right stick
ABS_RX   (code: 0x03)  // Right stick X
ABS_RY   (code: 0x04)  // Right stick Y

// Analog buttons (L3/R3 press)
BTN_THUMBL (code: 0x13d)  // Left stick click
BTN_THUMBR (code: 0x13e)  // Right stick click
```

### 2.2 Modifiche keymap_hw.h

**File:** `src/common/system/keymap_hw.h`

```c
// ============================================================
// PATCH: Add Miyoo Flip analog stick support
// ============================================================

#ifndef KEYMAP_HW_H__
#define KEYMAP_HW_H__

#include <linux/input.h>

// Digital buttons (existing)
#define HW_BTN_UP KEY_UP
#define HW_BTN_DOWN KEY_DOWN
#define HW_BTN_LEFT KEY_LEFT
#define HW_BTN_RIGHT KEY_RIGHT
#define HW_BTN_A KEY_SPACE
#define HW_BTN_B KEY_LEFTCTRL
#define HW_BTN_X KEY_LEFTSHIFT
#define HW_BTN_Y KEY_LEFTALT
#define HW_BTN_L1 KEY_E
#define HW_BTN_R1 KEY_T
#define HW_BTN_L2 KEY_TAB
#define HW_BTN_R2 KEY_BACKSPACE
#define HW_BTN_SELECT KEY_RIGHTCTRL
#define HW_BTN_START KEY_ENTER
#define HW_BTN_MENU KEY_ESC
#define HW_BTN_POWER KEY_POWER
#define HW_BTN_VOLUME_UP KEY_VOLUMEUP
#define HW_BTN_VOLUME_DOWN KEY_VOLUMEDOWN

// +++ NEW: Analog stick buttons (L3/R3 click)
#ifdef MIYOO_FLIP
#define HW_BTN_L3 BTN_THUMBL  // Left stick click
#define HW_BTN_R3 BTN_THUMBR  // Right stick click
#endif

// +++ NEW: Analog axes definitions
#ifdef MIYOO_FLIP
#define HW_AXIS_LX ABS_X      // Left stick X-axis
#define HW_AXIS_LY ABS_Y      // Left stick Y-axis
#define HW_AXIS_RX ABS_RX     // Right stick X-axis
#define HW_AXIS_RY ABS_RY     // Right stick Y-axis

// Analog stick parameters
#define ANALOG_MIN 0
#define ANALOG_MAX 1023
#define ANALOG_CENTER 512
#define ANALOG_DEADZONE 50    // Configurabile da settings
#define ANALOG_THRESHOLD 100  // Per conversione a digitale
#endif

// +++ NEW: Lid sensor (clamshell)
#ifdef MIYOO_FLIP
#define HW_SW_LID SW_LID      // Lid close/open switch
#endif

#endif // KEYMAP_HW_H__
```

### 2.3 Modifiche input_fd.h

**File:** `src/keymon/input_fd.h`

```c
// ============================================================
// PATCH: Add analog stick and lid sensor event handling
// ============================================================

#ifndef KEYMON_INPUT_FD_H__
#define KEYMON_INPUT_FD_H__

#include <linux/fb.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include "utils/msleep.h"
#include "system/keymap_hw.h"

// for ev.value
#define RELEASED 0
#define PRESSED 1
#define REPEAT 2

// for button_flag
#define SELECT_BIT 0
#define START_BIT 1
#define L2_BIT 2
#define R2_BIT 3
#define SELECT (1 << SELECT_BIT)
#define START (1 << START_BIT)
#define L2 (1 << L2_BIT)
#define R2 (1 << R2_BIT)

#define QUEUE_MAX 10

// +++ NEW: Analog stick state structure
#ifdef MIYOO_FLIP
typedef struct {
    int16_t lx;      // Left stick X (-1023 to +1023, centered at 0)
    int16_t ly;      // Left stick Y
    int16_t rx;      // Right stick X
    int16_t ry;      // Right stick Y
    bool l3_pressed; // Left stick button
    bool r3_pressed; // Right stick button
    uint32_t last_update_ms; // For debouncing
} analog_state_t;

static analog_state_t analog_state = {0, 0, 0, 0, false, false, 0};

// Deadzone filtering
static inline int16_t apply_deadzone(int16_t value, int16_t deadzone)
{
    if (value > deadzone) {
        return value - deadzone;
    } else if (value < -deadzone) {
        return value + deadzone;
    }
    return 0;
}

// Convert ADC raw value (0-1023) to signed centered value (-512 to +511)
static inline int16_t adc_to_centered(int raw_value)
{
    return (int16_t)(raw_value - ANALOG_CENTER);
}

// Convert analog to digital direction (for D-pad emulation if needed)
static inline int analog_to_digital_direction(int16_t x, int16_t y)
{
    // Returns direction bits: UP=1, DOWN=2, LEFT=4, RIGHT=8
    int dir = 0;
    if (y < -ANALOG_THRESHOLD) dir |= 1; // UP
    if (y > ANALOG_THRESHOLD) dir |= 2;  // DOWN
    if (x < -ANALOG_THRESHOLD) dir |= 4; // LEFT
    if (x > ANALOG_THRESHOLD) dir |= 8;  // RIGHT
    return dir;
}

// Update analog state from input event
void analog_update_state(struct input_event *ev)
{
    switch (ev->code) {
        case ABS_X:  // Left stick X
            analog_state.lx = apply_deadzone(
                adc_to_centered(ev->value), 
                ANALOG_DEADZONE
            );
            break;
        case ABS_Y:  // Left stick Y
            analog_state.ly = apply_deadzone(
                adc_to_centered(ev->value), 
                ANALOG_DEADZONE
            );
            break;
        case ABS_RX: // Right stick X
            analog_state.rx = apply_deadzone(
                adc_to_centered(ev->value), 
                ANALOG_DEADZONE
            );
            break;
        case ABS_RY: // Right stick Y
            analog_state.ry = apply_deadzone(
                adc_to_centered(ev->value), 
                ANALOG_DEADZONE
            );
            break;
        case BTN_THUMBL: // L3 button
            analog_state.l3_pressed = (ev->value == PRESSED);
            break;
        case BTN_THUMBR: // R3 button
            analog_state.r3_pressed = (ev->value == PRESSED);
            break;
    }
    
    // Update timestamp
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    analog_state.last_update_ms = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

// Get current analog state (for external query)
void analog_get_state(analog_state_t *state)
{
    memcpy(state, &analog_state, sizeof(analog_state_t));
}

#endif // MIYOO_FLIP

// Global Variables
static int input_fd;
static struct input_event ev;
static struct pollfd fds[1];
static bool keyinput_disabled = false;
static int ignore_queue[QUEUE_MAX][2];
static int ignore_queue_count = 0;

void _ignoreQueue_remove(int index)
{
    printf_debug("Removing from ignore queue: code=%d, value=%d\n",
                 ignore_queue[index][0], ignore_queue[index][1]);
    for (int i = index; i < ignore_queue_count - 1; i++) {
        ignore_queue[i][0] = ignore_queue[i + 1][0];
        ignore_queue[i][1] = ignore_queue[i + 1][1];
    }
    ignore_queue_count--;
}

void _ignoreQueue_add(int code, int value)
{
    if (ignore_queue_count >= QUEUE_MAX)
        return;
    printf_debug("Adding to ignore queue: code=%d, value=%d\n", code, value);
    ignore_queue[ignore_queue_count][0] = code;
    ignore_queue[ignore_queue_count][1] = value;
    ignore_queue_count++;
}

// +++ MODIFIED: Enhanced to handle EV_ABS and EV_SW events
bool keyinput_isValid(void)
{
    read(input_fd, &ev, sizeof(ev));

#ifdef MIYOO_FLIP
    // Handle analog stick events (EV_ABS)
    if (ev.type == EV_ABS) {
        analog_update_state(&ev);
        // Analog events are always processed but don't block key events
        return false; // Continue to next event
    }
    
    // Handle switch events (EV_SW) - lid sensor
    if (ev.type == EV_SW) {
        // Lid events are handled in keymon.c main loop
        return true; // Valid event, process it
    }
#endif

    // Original key event handling
    if (ev.type != EV_KEY || ev.value > REPEAT)
        return false;

    for (int i = 0; i < ignore_queue_count; i++) {
        if (ignore_queue[i][0] == ev.code && ignore_queue[i][1] == ev.value) {
            _ignoreQueue_remove(i);
            return false;
        }
    }

    return true;
}

// Existing functions remain unchanged...
void keyinput_send(unsigned short code, signed int value)
{
    if (keyinput_disabled)
        return;
    char cmd[100];
    sprintf(cmd, "sendkeys %d %d", code, value);
    printf_debug("Send keys: code=%d, value=%d\n", code, value);
    _ignoreQueue_add(code, value);
    system(cmd);
    print_debug("Keys sent");
}

void keyinput_sendMulti(int n, int code_value_pairs[n][2])
{
    if (keyinput_disabled)
        return;
    char cmd[512];
    strcpy(cmd, "./bin/sendkeys ");

    for (int i = 0; i < n; i++) {
        int code = code_value_pairs[i][0];
        int value = code_value_pairs[i][1];
        _ignoreQueue_add(code, value);
        sprintf(cmd + strlen(cmd), "%d %d ", code, value);
    }

    printf_debug("Send keys: %s\n", cmd);
    system(cmd);
    print_debug("Keys sent");
}

void keyinput_disable(void)
{
    if (keyinput_disabled)
        return;
    while (ioctl(input_fd, EVIOCGRAB, 1) < 0) {
        usleep(100000);
    }
    keyinput_disabled = true;
    print_debug("Keyinput disabled");
}

void keyinput_enable(void)
{
    if (!keyinput_disabled)
        return;
    while (ioctl(input_fd, EVIOCGRAB, 0) < 0) {
        usleep(100000);
    }
    keyinput_disabled = false;
    print_debug("Keyinput enabled");
}

#endif // KEYMON_INPUT_FD_H__
```

---

## 3. Lid Sensor Sleep/Wake

### 3.1 Hardware Lid Sensor

**Miyoo Flip Clamshell:**
- **Tipo:** Hall effect magnetic sensor
- **GPIO:** GPIO0_A5 (da device tree)
- **Event:** SW_LID (Linux switch event)
- **States:**
  - `0` = Lid OPEN (device awake)
  - `1` = Lid CLOSED (trigger sleep)

### 3.2 Modifiche keymon.c

**File:** `src/keymon/keymon.c`

**Aggiungere dopo le includes:**

```c
// ============================================================
// PATCH: Lid sensor support for Miyoo Flip clamshell
// ============================================================

#ifdef MIYOO_FLIP
#include <sys/types.h>
#include <sys/stat.h>

#define LID_SENSOR_PATH "/dev/input/event2"  // Verificare con evtest
#define LID_DEBOUNCE_MS 100  // Debounce per evitare falsi trigger

typedef enum {
    LID_STATE_UNKNOWN = -1,
    LID_STATE_OPEN = 0,
    LID_STATE_CLOSED = 1
} lid_state_t;

static lid_state_t current_lid_state = LID_STATE_UNKNOWN;
static uint32_t last_lid_event_ms = 0;

// Check if lid sensor device exists
bool lid_sensor_available(void)
{
    struct stat st;
    return (stat(LID_SENSOR_PATH, &st) == 0);
}

// Handle lid close event - enter sleep mode
void handle_lid_close(void)
{
    // Get current time for debounce
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t now_ms = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    
    // Debounce check
    if ((now_ms - last_lid_event_ms) < LID_DEBOUNCE_MS) {
        printf_debug("Lid close event debounced\n");
        return;
    }
    last_lid_event_ms = now_ms;
    
    printf_debug("Lid closed - entering sleep mode\n");
    
    // Haptic feedback (optional)
    super_short_pulse();
    
    // Save current state
    screenshot_system();
    sync();
    
    // Dim display to minimum
    display_setBrightness(0);
    msleep(50);
    
    // Turn off display backlight
    display_off();
    
    // Suspend audio
    system("killall -STOP mpg123 2>/dev/null");
    
    // Enter system suspend (will wake on lid open)
    system("echo mem > /sys/power/state");
    
    // --- Resume point after lid open ---
    printf_debug("Waking from lid open\n");
    
    // Restore audio
    system("killall -CONT mpg123 2>/dev/null");
    
    // Restore display
    display_on();
    settings_setBrightness(settings.brightness, true, false);
    
    // Haptic feedback
    super_short_pulse();
    
    current_lid_state = LID_STATE_OPEN;
}

// Handle lid open event - wake from sleep
void handle_lid_open(void)
{
    // Get current time for debounce
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint32_t now_ms = (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    
    // Debounce check
    if ((now_ms - last_lid_event_ms) < LID_DEBOUNCE_MS) {
        printf_debug("Lid open event debounced\n");
        return;
    }
    last_lid_event_ms = now_ms;
    
    printf_debug("Lid opened - device already awake\n");
    
    // Device should already be awake from kernel wakeup
    // Just ensure display is on
    if (!g_display.enabled) {
        display_on();
        settings_setBrightness(settings.brightness, true, false);
    }
    
    current_lid_state = LID_STATE_OPEN;
}

// Process lid sensor event
void process_lid_event(struct input_event *ev)
{
    if (ev->type != EV_SW || ev->code != SW_LID) {
        return;
    }
    
    printf_debug("Lid sensor event: value=%d\n", ev->value);
    
    if (ev->value == 1) {
        // Lid closed
        current_lid_state = LID_STATE_CLOSED;
        handle_lid_close();
    } else if (ev->value == 0) {
        // Lid opened
        current_lid_state = LID_STATE_OPEN;
        handle_lid_open();
    }
}

#endif // MIYOO_FLIP
```

**Nella funzione main() di keymon.c, aggiungere dopo il polling input:**

```c
// +++ MODIFIED: Main event loop with lid sensor support
while (1) {
    // Poll for input events
    int poll_result = poll(fds, 1, CHECK_SEC * 1000);
    
    if (poll_result > 0) {
        if (keyinput_isValid()) {
#ifdef MIYOO_FLIP
            // Check for lid sensor event
            if (ev.type == EV_SW && ev.code == SW_LID) {
                process_lid_event(&ev);
                continue; // Skip normal key processing
            }
#endif
            // Normal key event processing...
            // (existing code continues)
        }
    }
    
    // Existing hibernate/timeout logic...
}
```

---

## 4. Vibration PWM Feedback

### 4.1 Hardware Vibration

**Miyoo Flip Vibration Motor:**
- **Tipo:** LRA (Linear Resonant Actuator)
- **Control:** PWM3 @ 1kHz
- **Duty Cycle:** 0-100% (intensity variable)
- **Driver:** `pwm-vibrator` kernel module

**Differenze vs Miyoo Mini+:**
- Mini+: GPIO on/off (digital, 100% or 0%)
- Flip: PWM intensity control (0-100% granular)

### 4.2 Modifiche rumble.h

**File:** `src/common/system/rumble.h`

```c
// ============================================================
// PATCH: PWM-based vibration for Miyoo Flip
// ============================================================

#ifndef RUMBLE_H__
#define RUMBLE_H__

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include "settings.h"
#include "utils/file.h"
#include "utils/msleep.h"

#define SHORT_PULSE_MS 100
#define SUPER_SHORT_PULSE_MS 50

#ifdef MIYOO_FLIP
// PWM-based vibration for Miyoo Flip
#define PWM_VIBRATION_PATH "/sys/class/pwm/pwmchip0/pwm3"
#define PWM_PERIOD_NS 1000000  // 1ms = 1kHz frequency
#define PWM_DUTY_MAX_NS 1000000

// Intensity levels (0-100%)
static int intensity_levels[] = {0, 25, 50, 75, 100};
static bool pwm_initialized = false;

// Initialize PWM for vibration
void rumble_pwm_init(void)
{
    if (pwm_initialized)
        return;
    
    // Export PWM3
    file_write("/sys/class/pwm/pwmchip0/export", "3", 1);
    msleep(10);
    
    // Set period (1kHz = 1ms)
    char period_str[32];
    sprintf(period_str, "%d", PWM_PERIOD_NS);
    file_write(PWM_VIBRATION_PATH "/period", period_str, strlen(period_str));
    
    // Set duty cycle to 0 initially
    file_write(PWM_VIBRATION_PATH "/duty_cycle", "0", 1);
    
    // Enable PWM
    file_write(PWM_VIBRATION_PATH "/enable", "1", 1);
    
    pwm_initialized = true;
    printf_debug("PWM vibration initialized\n");
}

// Set vibration intensity (0-100%)
void rumble_set_intensity(uint8_t percent)
{
    if (!pwm_initialized) {
        rumble_pwm_init();
    }
    
    if (percent > 100)
        percent = 100;
    
    // Calculate duty cycle
    uint32_t duty_ns = (PWM_DUTY_MAX_NS * percent) / 100;
    
    char duty_str[32];
    sprintf(duty_str, "%u", duty_ns);
    file_write(PWM_VIBRATION_PATH "/duty_cycle", duty_str, strlen(duty_str));
    
    printf_debug("Vibration intensity set to %d%% (duty=%u ns)\n", percent, duty_ns);
}

// Enable/disable vibration with intensity
void rumble(bool enabled)
{
    if (!pwm_initialized) {
        rumble_pwm_init();
    }
    
    if (enabled) {
        // Use setting level (1-4 maps to 25-100%)
        uint8_t intensity = intensity_levels[settings.vibration];
        rumble_set_intensity(intensity);
    } else {
        rumble_set_intensity(0);
    }
}

#else
// Original GPIO-based vibration for Miyoo Mini/Mini+
static int super_short_timings[] = {0, 25, 50, 75};
static int short_timings[] = {0, 50, 100, 150};

void rumble(bool enabled)
{
    file_write("/sys/class/gpio/export", "48", 2);
    file_write("/sys/class/gpio/gpio48/direction", "out", 3);
    file_write("/sys/class/gpio/gpio48/value", enabled ? "0" : "1", 1);
}
#endif // MIYOO_FLIP

/**
 * @brief Turns on vibration for SHORT_PULSE_MS duration
 */
void short_pulse(void)
{
    if (settings.vibration == 0)
        return;
        
#ifdef MIYOO_FLIP
    rumble(true);
    msleep(SHORT_PULSE_MS);
    rumble(false);
#else
    rumble(true);
    msleep(short_timings[settings.vibration]);
    rumble(false);
#endif
}

/**
 * @brief Turns on vibration for SUPER_SHORT_PULSE_MS duration
 */
void super_short_pulse(void)
{
    if (settings.vibration == 0)
        return;
        
#ifdef MIYOO_FLIP
    rumble(true);
    msleep(SUPER_SHORT_PULSE_MS);
    rumble(false);
#else
    rumble(true);
    msleep(super_short_timings[settings.vibration]);
    rumble(false);
#endif
}

/**
 * @brief Menu haptic feedback
 */
void menu_short_pulse(void)
{
    if (settings.vibration == 0 || !settings.menu_button_haptics)
        return;
    short_pulse();
}

void menu_super_short_pulse(void)
{
    if (settings.vibration == 0 || !settings.menu_button_haptics)
        return;
    super_short_pulse();
}

#endif // RUMBLE_H__
```

---

## 5. Aspect Ratio e Display

### 5.1 Confronto Display

| Device | Resolution | Aspect | DPI | Note |
|--------|-----------|--------|-----|------|
| Miyoo Mini+ | 640×480 | 4:3 | ~133 | Existing support |
| Miyoo Flip | 640×480 | 4:3 | ~133 | **Identico!** |

**Conclusion:** Nessuna modifica necessaria per aspect ratio! ✅

### 5.2 Dual Display Support (Opzionale)

**Miyoo Flip ha 2 display:**
- **Interno:** 640×480 (main, gaming)
- **Esterno:** 240×240 (coperchio, notifiche/artwork)

Per Phase 4 **focus solo su display interno** (compatibilità immediata).

Display esterno può essere aggiunto in Phase 5+ per:
- Album art durante gaming
- Notifications
- Mini status panel

---

## 6. Input Lag Optimization

### 6.1 Problemi Noti RK3566

**Issue:** Alcuni device Rockchip RK3566 hanno input lag di 30-50ms

**Root Causes:**
1. **Kernel polling rate:** Default 100Hz (10ms interval)
2. **CPU governor:** Default `ondemand` (slow ramp-up)
3. **Thermal throttling:** CPU downscaling sotto carico
4. **DRM vsync:** Display vsync blocking input thread

### 6.2 Ottimizzazioni Kernel

**File:** `kernel/.config` o `defconfig`

```kconfig
# Input polling rate optimization
CONFIG_HZ=1000                          # 1000Hz timer (1ms tick)
CONFIG_HZ_1000=y
CONFIG_INPUT_POLLDEV=y
CONFIG_INPUT_POLL_INTERVAL=1            # 1ms poll interval

# CPU scheduler optimization
CONFIG_PREEMPT=y                        # Full preemption for low latency
CONFIG_NO_HZ_FULL=y                     # Tickless on gaming cores
CONFIG_RCU_NOCB_CPU=y                   # Offload RCU callbacks

# CPU frequency scaling
CONFIG_CPU_FREQ=y
CONFIG_CPU_FREQ_DEFAULT_GOV_SCHEDUTIL=y # Schedutil (fast response)
CONFIG_CPU_FREQ_GOV_PERFORMANCE=y       # Allow performance governor
```

### 6.3 Userspace Optimizations

**File:** Create `/etc/init.d/S99gaming-optimizations`

```bash
#!/bin/sh
# Gaming optimizations for Miyoo Flip

# Set CPU governor to schedutil (fast ramp)
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo "schedutil" > "$cpu"
done

# Set minimum CPU frequency to 1200MHz (faster response)
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq; do
    echo "1200000" > "$cpu"
done

# Increase input device polling rate
for dev in /sys/class/input/input*/device/poll_interval; do
    [ -f "$dev" ] && echo "1" > "$dev"  # 1ms = 1000Hz
done

# Reduce swappiness (less SD I/O)
echo 10 > /proc/sys/vm/swappiness

# Increase dirty page timeout (better responsiveness)
echo 500 > /proc/sys/vm/dirty_expire_centisecs

echo "Gaming optimizations applied"
```

**Make executable:**
```bash
chmod +x /etc/init.d/S99gaming-optimizations
```

### 6.4 RetroArch Config

**File:** `retroarch.cfg`

```ini
# Input lag reduction
input_poll_type_behavior = "2"      # Poll every frame
video_frame_delay = "0"              # No artificial delay
input_max_users = "1"                # Single player for lowest latency
video_hard_sync = "true"             # GPU sync (reduce lag)
video_hard_sync_frames = "0"         # Immediate sync
video_max_swapchain_images = "2"     # Double buffering
audio_latency = "64"                 # Lower audio latency (if stable)
video_vsync = "true"                 # Keep vsync (prevents tearing)
video_threaded = "false"             # Disable threaded video (lower lag)
```

### 6.5 Verificare Input Lag

**Test Tool:**
```bash
# Measure input-to-frame latency
evtest /dev/input/event0 &           # Monitor input
ffplay -i /dev/fb0 -f fbdev &        # Monitor display

# Press button, measure time to screen response
# Target: <16ms (1 frame @ 60fps)
```

---

## 7. Device Model Detection

### 7.1 Modifiche device_model.h

**File:** `src/common/system/device_model.h`

```c
// ============================================================
// PATCH: Add Miyoo Flip detection
// ============================================================

#ifndef DEVICE_MODEL_H__
#define DEVICE_MODEL_H__

#include "utils/file.h"
#include <stdio.h>

#define MIYOO283 283    // Miyoo Mini
#define MIYOO354 354    // Miyoo Mini Plus
#define MIYOO_FLIP 566  // +++ NEW: Miyoo Flip (RK3566)

static int DEVICE_ID;
static char DEVICE_SN[13];

/**
 * @brief Get device model
 * MM = Miyoo Mini (283)
 * MMP = Miyoo Mini Plus (354)
 * FLIP = Miyoo Flip (566)
 */
void getDeviceModel(void)
{
    FILE *fp;
    // Default to MMP for backwards compatibility
    DEVICE_ID = MIYOO354;
    
    // Method 1: Read from /tmp/deviceModel (existing)
    if (exists("/tmp/deviceModel")) {
        file_get(fp, "/tmp/deviceModel", "%d", &DEVICE_ID);
    }
    
#ifdef MIYOO_FLIP
    // Method 2: Detect RK3566 via /proc/cpuinfo
    if (!exists("/tmp/deviceModel")) {
        FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
        if (cpuinfo) {
            char line[256];
            while (fgets(line, sizeof(line), cpuinfo)) {
                // Check for RK3566 in hardware field
                if (strstr(line, "RK3566") || strstr(line, "rk3566")) {
                    DEVICE_ID = MIYOO_FLIP;
                    break;
                }
                // Alternative: Check for Cortex-A55 (Flip uses A55)
                if (strstr(line, "Cortex-A55")) {
                    DEVICE_ID = MIYOO_FLIP;
                    break;
                }
            }
            fclose(cpuinfo);
        }
    }
    
    // Method 3: Check for Flip-specific hardware
    if (DEVICE_ID != MIYOO_FLIP) {
        // Check for lid sensor (only on Flip)
        if (exists("/dev/input/by-id/lid-sensor") ||
            exists("/sys/class/input/input2/name")) {
            FILE *name_fp = fopen("/sys/class/input/input2/name", "r");
            if (name_fp) {
                char name[64];
                if (fgets(name, sizeof(name), name_fp)) {
                    if (strstr(name, "lid") || strstr(name, "hall")) {
                        DEVICE_ID = MIYOO_FLIP;
                    }
                }
                fclose(name_fp);
            }
        }
    }
#endif
    
    // Validate device ID
    if (DEVICE_ID != MIYOO283 && 
        DEVICE_ID != MIYOO354 && 
        DEVICE_ID != MIYOO_FLIP) {
        // Unknown device, default to MMP
        DEVICE_ID = MIYOO354;
    }
    
    printf_debug("Detected device model: %d\n", DEVICE_ID);
}

void getDeviceSerial(void)
{
    FILE *fp;
    DEVICE_SN[0] = '\0';
    
    if (!exists("/tmp/deviceSN")) {
        return;
    }
    
    file_get(fp, "/tmp/deviceSN", "%12[^\n]", DEVICE_SN);
    DEVICE_SN[12] = '\0';
}

// +++ NEW: Helper functions for device-specific code
bool is_miyoo_flip(void)
{
    return (DEVICE_ID == MIYOO_FLIP);
}

bool has_analog_sticks(void)
{
#ifdef MIYOO_FLIP
    return is_miyoo_flip();
#else
    return false;
#endif
}

bool has_lid_sensor(void)
{
#ifdef MIYOO_FLIP
    return is_miyoo_flip();
#else
    return false;
#endif
}

#endif // DEVICE_MODEL_H__
```

---

## 8. Build e Testing

### 8.1 Build Flags

**Makefile additions:**

```makefile
# Miyoo Flip target
ifeq ($(TARGET),miyoo_flip)
    CFLAGS += -DMIYOO_FLIP
    CFLAGS += -DPLATFORM_MIYOOMINI  # For 640x480 compatibility
    CROSS_COMPILE ?= aarch64-linux-gnu-
    ARCH = arm64
endif

# Build for Flip
.PHONY: flip
flip:
	$(MAKE) TARGET=miyoo_flip all
```

**Build command:**
```bash
make flip -j$(nproc)
```

### 8.2 Testing Procedure

**1. Hardware Test Checklist:**

```bash
# A. Input devices
ls -la /dev/input/
# Expected:
# - event0: gpio-keys (buttons)
# - event1: adc-joystick (analog sticks)
# - event2: hall-sensor (lid)

# B. Test digital buttons
evtest /dev/input/event0
# Press all buttons, verify events

# C. Test analog sticks
evtest /dev/input/event1
# Move sticks, should see:
# - ABS_X / ABS_Y (left stick)
# - ABS_RX / ABS_RY (right stick)

# D. Test lid sensor
evtest /dev/input/event2
# Close/open lid, should see:
# - SW_LID: 1 (closed)
# - SW_LID: 0 (open)

# E. Test vibration PWM
echo 50 > /sys/class/pwm/pwmchip0/pwm3/duty_cycle
# Should feel medium vibration
echo 100 > /sys/class/pwm/pwmchip0/pwm3/duty_cycle
# Should feel strong vibration
echo 0 > /sys/class/pwm/pwmchip0/pwm3/duty_cycle
# Vibration stops
```

**2. Software Integration Test:**

```bash
# A. Test device detection
cat /tmp/deviceModel
# Expected: 566 (MIYOO_FLIP)

# B. Test keymon with debug
killall keymon
keymon --debug &
# Tail logs
tail -f /var/log/keymon.log

# C. Test analog in RetroArch
retroarch --verbose
# Configure input, test both sticks
# Menu -> Settings -> Input -> Port 1 Controls

# D. Test lid sleep/wake
# Close lid -> should sleep
# Open lid -> should wake
# Check logs for "Lid closed/opened" messages
```

**3. Performance Test:**

```bash
# Input latency test
./test_input_lag.sh
# Expected: <16ms (1 frame @ 60fps)

# CPU frequency check during gameplay
watch -n 1 'cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq'
# Should see 1.2-1.8 GHz during active gaming

# Thermal check
watch -n 1 'cat /sys/class/thermal/thermal_zone0/temp'
# Should stay <75000 (75°C) during normal gaming
```

---

## 9. Checklist Implementazione

### Phase 4 Implementation Checklist

**P0 - Critical (Must Have):**
- [ ] Modificare `device_model.h` - Add MIYOO_FLIP detection
- [ ] Modificare `keymap_hw.h` - Add analog axes defines
- [ ] Modificare `input_fd.h` - Add EV_ABS handling + analog state
- [ ] Modificare `keymon.c` - Add lid sensor logic
- [ ] Modificare `rumble.h` - Add PWM vibration support
- [ ] Testare device detection su Flip hardware
- [ ] Testare analog stick input (evtest)
- [ ] Testare lid sleep/wake cycle

**P1 - Important (Should Have):**
- [ ] Applicare kernel optimizations (HZ=1000, PREEMPT)
- [ ] Creare gaming optimization script
- [ ] Configurare RetroArch per low latency
- [ ] Testare input lag (<16ms target)
- [ ] Testare PWM vibration intensity levels
- [ ] Documentare analog stick calibration

**P2 - Nice to Have (Could Have):**
- [ ] Add analog stick deadzone configuration UI
- [ ] Add vibration intensity slider
- [ ] Add lid sensor enable/disable toggle
- [ ] Performance profiling tools
- [ ] Dual display artwork support (future)

### Files Summary

| File | LOC Added | LOC Modified | Complexity |
|------|-----------|--------------|------------|
| device_model.h | +50 | ~10 | Medium |
| keymap_hw.h | +30 | 0 | Low |
| input_fd.h | +150 | ~20 | High |
| keymon.c | +200 | ~30 | High |
| rumble.h | +80 | ~20 | Medium |
| display.h | 0 | ~10 | Low |
| **TOTAL** | **~510** | **~90** | **High** |

### Estimated Effort

- **Development:** 3-5 giorni (1 developer)
- **Testing:** 2-3 giorni (hardware required)
- **Debug/fixes:** 1-2 giorni
- **Documentation:** 1 giorno
- **Total:** 7-11 giorni

---

## Conclusioni Phase 4

### Risultati Attesi

✅ **Dual analog stick support** funzionante  
✅ **Lid sensor sleep/wake** implementato  
✅ **PWM vibration** con intensità variabile  
✅ **Aspect ratio** già compatibile (640×480)  
✅ **Input lag** ottimizzato (<16ms)  

### Prossimi Passi

**Phase 5 (Recommended):**
- RetroArch cores rebuild per ARM64
- EmulationStation port
- Gaming performance profiling
- Battery life optimization
- Thermal throttling tuning

**Phase 6 (Advanced):**
- Dual display artwork support
- WiFi/Bluetooth integration
- Online multiplayer (RetroArch netplay)
- Cloud save sync
- Advanced power management

### Note Finali

**IMPORTANTE:**
- Tutti i code snippet sono **template** che richiedono testing su hardware reale
- GPIO pin numbers devono essere verificati con device tree effettivo
- PWM channel potrebbe variare (verificare con `ls /sys/class/pwm/`)
- Input device paths (`/dev/input/eventX`) possono cambiare - usare `by-id/` o `by-path/`

**Testing su hardware reale è ESSENZIALE prima del rilascio!**

---

**Versione:** 1.0  
**Data:** Febbraio 2026  
**Autore:** Onion OS Porting Team  
**Status:** ✅ Ready for Implementation
