#ifndef TWEAKS_OVERCLOCK_ACTIONS_H__
#define TWEAKS_OVERCLOCK_ACTIONS_H__

#include "components/list.h"
#include "system/osd.h"
#include "system/settings.h"
#include "system/thermal.h"
#include "utils/config.h"

#include "./overclock.h"
#include "./appstate.h"

/**
 * Action: Toggle overclocking on/off
 */
void action_setOverclockEnabled(void *pt)
{
    ListItem *item = (ListItem *)pt;
    bool enabled = item->value;
    
    settings.overclock_enabled = enabled;
    config_flag_set(".overclockEnabled", enabled);
    
    if (enabled) {
        // Show warning if first time
        if (!overclock_is_warning_shown()) {
            // Warning will be shown in the menu
            overclock_set_warning_shown(true);
        }
        
        // Apply overclock with saved settings
        overclock_enable(settings.overclock_frequency, settings.overclock_thermal_limit);
        print_debug("[Overclock Action] Enabled at %d MHz", settings.overclock_frequency);
    }
    else {
        // Disable and revert to stock
        overclock_disable();
        print_debug("[Overclock Action] Disabled");
    }
    
    reset_menus = true;
    all_changed = true;
}

/**
 * Action: Set overclock frequency
 */
void action_setOverclockFrequency(void *pt)
{
    ListItem *item = (ListItem *)pt;
    // Value is 0-10, map to 1200-1700 MHz in 50 MHz steps
    int frequency = 1200 + (item->value * 50);
    
    settings.overclock_frequency = frequency;
    config_setNumber("overclock/frequency", frequency);
    
    if (settings.overclock_enabled) {
        overclock_enable(frequency, settings.overclock_thermal_limit);
        print_debug("[Overclock Action] Frequency set to %d MHz", frequency);
    }
    
    reset_menus = true;
}

/**
 * Action: Set overclock profile
 */
void action_setOverclockProfile(void *pt)
{
    ListItem *item = (ListItem *)pt;
    OverclockProfile profile = (OverclockProfile)item->value;
    
    settings.overclock_profile = profile;
    config_setNumber("overclock/profile", profile);
    
    if (profile != OC_PROFILE_CUSTOM) {
        int frequency = overclock_get_profile_frequency(profile);
        settings.overclock_frequency = frequency;
        config_setNumber("overclock/frequency", frequency);
        
        if (settings.overclock_enabled) {
            overclock_set_profile(profile);
            print_debug("[Overclock Action] Profile set to %d (%d MHz)", profile, frequency);
        }
    }
    
    reset_menus = true;
    all_changed = true;
}

/**
 * Action: Set thermal limit
 */
void action_setOverclockThermalLimit(void *pt)
{
    ListItem *item = (ListItem *)pt;
    // Value is 0-10, map to 60-75°C
    int thermal_limit = 60 + (item->value * 1.5);
    
    settings.overclock_thermal_limit = thermal_limit;
    config_setNumber("overclock/thermalLimit", thermal_limit);
    
    if (settings.overclock_enabled) {
        OverclockState *state = overclock_get_state();
        state->thermal_limit = thermal_limit;
        print_debug("[Overclock Action] Thermal limit set to %d°C", thermal_limit);
    }
}

/**
 * Action: Toggle thermal protection
 */
void action_setOverclockThermalProtection(void *pt)
{
    ListItem *item = (ListItem *)pt;
    bool protection = item->value;
    
    config_flag_set(".overclockThermalProtection", protection);
    
    OverclockState *state = overclock_get_state();
    state->thermal_protection = protection;
    
    print_debug("[Overclock Action] Thermal protection %s", protection ? "enabled" : "disabled");
}

#endif // TWEAKS_OVERCLOCK_ACTIONS_H__
