# Overclocking Feature - Integration Patch

This document provides the integration instructions for adding the overclocking feature to the Onion OS Tweaks menu.

## Files Overview

### New Files Created
1. `src/common/system/thermal.h` - Temperature monitoring system
2. `src/tweaks/overclock.h` - Core overclocking logic
3. `src/tweaks/overclock_actions.h` - UI action handlers
4. `src/tweaks/overclock_formatters.h` - Display formatters
5. `src/tweaks/overclock_menu.h` - Menu definitions
6. `OVERCLOCKING_GUIDE.md` - User documentation

### Modified Files
1. `src/common/system/settings.h` - Added overclock settings

## Integration Steps

### Step 1: Include Headers

Add to `src/tweaks/menus.h` after existing includes:

```c
#include "./overclock_menu.h"
```

### Step 2: Add Menu Entry

Add to the main tweaks menu in `src/tweaks/menus.h`:

```c
// In the appropriate menu creation function (e.g., menu_advanced or main tweaks menu)
list_addItem(&_menu_tweaks,  // or appropriate menu
             (ListItem){
                 .label = "Performance...",
                 .action = menu_performance});
```

### Step 3: Settings Save/Load

The overclock settings are already integrated into the settings structure. Ensure your settings save/load functions handle the new fields:

```c
// In settings save function
config_flag_set(".overclockEnabled", settings.overclock_enabled);
config_setNumber("overclock/frequency", settings.overclock_frequency);
config_setNumber("overclock/thermalLimit", settings.overclock_thermal_limit);
config_setNumber("overclock/profile", settings.overclock_profile);

// In settings load function
config_flag_get(".overclockEnabled", &settings.overclock_enabled);
config_get("overclock/frequency", CONFIG_INT, &settings.overclock_frequency);
config_get("overclock/thermalLimit", CONFIG_INT, &settings.overclock_thermal_limit);
config_get("overclock/profile", CONFIG_INT, &settings.overclock_profile);
```

### Step 4: Apply Overclock on Boot (Optional)

Add to system startup script or initialization code:

```c
#include "system/thermal.h"
#include "tweaks/overclock.h"

// After settings are loaded
if (settings.overclock_enabled) {
    overclock_enable(settings.overclock_frequency, settings.overclock_thermal_limit);
    print_debug("[Boot] Overclocking enabled: %d MHz", settings.overclock_frequency);
}
```

### Step 5: Thermal Monitoring (Optional)

Add periodic thermal checks in main loop or background task:

```c
// Every 5-10 seconds during operation
static time_t last_thermal_check = 0;
time_t now = time(NULL);

if (now - last_thermal_check >= 5) {
    if (settings.overclock_enabled) {
        overclock_thermal_check();
    }
    last_thermal_check = now;
}
```

## Makefile Updates

No changes required - uses existing cpuclock binary at `/mnt/SDCARD/bin/cpuclock`.

## Testing Checklist

- [ ] Menu appears in Tweaks → Performance
- [ ] Warning message displays on first enable
- [ ] Temperature reading updates in menu
- [ ] Frequency changes apply correctly
- [ ] Thermal throttling activates when hot
- [ ] Settings persist across reboots
- [ ] Emergency disable file works
- [ ] CPU returns to stock when disabled

## Configuration File Locations

- **Settings:** `/mnt/SDCARD/system.json`
- **Config flags:** `/mnt/SDCARD/.tmp_update/config/`
  - `.overclockEnabled`
  - `.overclockThermalProtection`
- **Emergency disable:** `/mnt/SDCARD/.tmp_update/config/.disableOC`

## Diff Summary

```diff
diff --git a/src/common/system/settings.h b/src/common/system/settings.h
@@ -71,6 +71,11 @@ typedef struct settings_s {
     char mainui_button_x[JSON_STRING_LEN];
     char mainui_button_y[JSON_STRING_LEN];
+    
+    // Overclocking settings
+    bool overclock_enabled;
+    int overclock_frequency;
+    int overclock_thermal_limit;
+    int overclock_profile;
 } settings_s;

@@ -126,7 +131,12 @@ static settings_s __default_settings = (settings_s){
     //utility
     .rec_countdown = false,
     .rec_indicator = false,
-    .rec_hotkey = false};
+    .rec_hotkey = false,
+    // Overclocking (disabled by default for safety)
+    .overclock_enabled = false,
+    .overclock_frequency = 1200,
+    .overclock_thermal_limit = 65,
+    .overclock_profile = 0};
```

## API Reference

### Temperature Monitoring

```c
#include "system/thermal.h"

int thermal_get_temperature(void);     // Get current temperature in °C
bool thermal_update(void);             // Update thermal info, returns false if throttling needed
bool thermal_is_safe(void);            // Check if temperature is safe
bool thermal_is_throttling(void);      // Check if currently throttling
int thermal_current_temp(void);        // Get last temperature reading
int thermal_max_temp(void);            // Get maximum recorded temperature
void thermal_reset_stats(void);        // Reset thermal statistics
int thermal_throttle_count(void);      // Get number of throttle events
bool thermal_emergency_check(void);    // Check if emergency shutdown needed
```

### Overclocking Control

```c
#include "tweaks/overclock.h"

bool overclock_enable(int freq_mhz, int thermal_limit);   // Enable with frequency
bool overclock_disable(void);                             // Disable and revert to stock
bool overclock_set_profile(OverclockProfile profile);     // Set profile
void overclock_thermal_check(void);                       // Perform thermal check
bool overclock_is_enabled(void);                          // Check if enabled
int overclock_get_frequency(void);                        // Get current frequency
int overclock_get_profile_frequency(OverclockProfile p);  // Get frequency for profile
bool overclock_is_frequency_safe(int freq_mhz);           // Validate frequency
int overclock_get_performance_gain(int freq_mhz);         // Calculate performance gain %
```

## Notes

1. **Safety First:** Feature is disabled by default and requires explicit user action
2. **Thermal Protection:** Always enabled unless user explicitly disables (not recommended)
3. **Temperature Source:** Read from AXP223 PMIC via I2C
4. **Frequency Range:** Hard-limited to 1200-1700 MHz
5. **Polling:** Thermal checks every 5 seconds during active overclock
6. **Emergency:** Auto-disables at 80°C to prevent hardware damage

## Support

For issues or questions about this feature:
- See `OVERCLOCKING_GUIDE.md` for user documentation
- Check `/tmp/oc.log` for debug information
- Review thermal statistics in overclock menu

## Disclaimer

⚠️ **USE AT YOUR OWN RISK**

Overclocking may reduce hardware lifespan, void warranties, and cause system instability. Always monitor temperature and use conservative settings. The developers are not responsible for any hardware damage.
