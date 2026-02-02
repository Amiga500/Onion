#ifndef TWEAKS_OVERCLOCK_MENU_H__
#define TWEAKS_OVERCLOCK_MENU_H__

#include "components/list.h"
#include "system/thermal.h"
#include "system/settings.h"

#include "./overclock.h"
#include "./overclock_actions.h"
#include "./overclock_formatters.h"
#include "./appstate.h"

// Static menu instances
static List _menu_performance;
static List _menu_overclock;

/**
 * Overclock submenu with detailed settings
 */
void menu_overclock(void *_)
{
    if (!_menu_overclock._created) {
        _menu_overclock = list_createWithTitle(7, LIST_SMALL, "Overclocking");
        
        // Warning info at top
        list_addItemWithInfoNote(&_menu_overclock,
                                 (ListItem){
                                     .label = "⚠ READ THIS FIRST",
                                     .disabled = 1},
                                 "OVERCLOCKING WARNING:\n"
                                 "Overclocking increases CPU frequency\n"
                                 "beyond stock settings. This can:\n"
                                 "- Improve performance (+8-25%)\n"
                                 "- Reduce battery life\n"
                                 "- Generate more heat\n"
                                 "- Potentially reduce hardware lifespan\n"
                                 "\n"
                                 "Thermal protection is STRONGLY\n"
                                 "recommended. Use at your own risk!");
        
        // Enable/Disable toggle
        list_addItemWithInfoNote(&_menu_overclock,
                                 (ListItem){
                                     .label = "Enable overclocking",
                                     .item_type = TOGGLE,
                                     .value = (int)settings.overclock_enabled,
                                     .action = action_setOverclockEnabled},
                                 "Enable or disable CPU overclocking.\n"
                                 "When enabled, the CPU will run at the\n"
                                 "selected frequency. When disabled,\n"
                                 "the CPU runs at stock 1200 MHz.");
        
        // Profile selector
        list_addItemWithInfoNote(&_menu_overclock,
                                 (ListItem){
                                     .label = "Overclock profile",
                                     .item_type = MULTIVALUE,
                                     .value_max = 5,
                                     .value = settings.overclock_profile,
                                     .value_formatter = formatter_overclockProfile,
                                     .action = action_setOverclockProfile},
                                 "Select a preset overclock profile:\n"
                                 "Stock: 1200 MHz (default)\n"
                                 "Mild: 1300 MHz (+8%)\n"
                                 "Moderate: 1400 MHz (+17%)\n"
                                 "High: 1500 MHz (+25%)\n"
                                 "Extreme: 1700 MHz (+42%) ⚠\n"
                                 "Custom: Set your own frequency");
        
        // Custom frequency slider
        list_addItemWithInfoNote(&_menu_overclock,
                                 (ListItem){
                                     .label = "CPU frequency",
                                     .item_type = MULTIVALUE,
                                     .value_max = 10,
                                     .value = (settings.overclock_frequency - 1200) / 50,
                                     .value_formatter = formatter_cpuFrequency,
                                     .action = action_setOverclockFrequency},
                                 "Set custom CPU frequency.\n"
                                 "Range: 1200-1700 MHz in 50 MHz steps.\n"
                                 "Higher = faster but hotter.\n"
                                 "Recommended safe maximum: 1500 MHz");
        
        // Thermal limit
        list_addItemWithInfoNote(&_menu_overclock,
                                 (ListItem){
                                     .label = "Thermal limit",
                                     .item_type = MULTIVALUE,
                                     .value_max = 10,
                                     .value = (settings.overclock_thermal_limit - 60) / 1.5,
                                     .value_formatter = formatter_thermalLimit,
                                     .action = action_setOverclockThermalLimit},
                                 "Set temperature threshold for automatic\n"
                                 "frequency reduction.\n"
                                 "When CPU reaches this temperature,\n"
                                 "frequency will be reduced to cool down.\n"
                                 "Recommended: 65°C (safe)");
        
        // Thermal protection toggle
        list_addItemWithInfoNote(&_menu_overclock,
                                 (ListItem){
                                     .label = "Thermal protection",
                                     .item_type = TOGGLE,
                                     .value = 1,  // Always enabled by default
                                     .action = action_setOverclockThermalProtection},
                                 "Enable automatic thermal protection.\n"
                                 "When enabled, CPU frequency will be\n"
                                 "automatically reduced if temperature\n"
                                 "exceeds the thermal limit.\n"
                                 "STRONGLY RECOMMENDED: Keep this ON!");
        
        // Current temperature display (read-only)
        list_addItemWithInfoNote(&_menu_overclock,
                                 (ListItem){
                                     .label = "Current temperature",
                                     .item_type = MULTIVALUE,
                                     .value_max = 0,
                                     .value = 0,
                                     .value_formatter = formatter_currentTemperature,
                                     .disabled = 1},
                                 "Current CPU/SoC temperature reading.\n"
                                 "Safe: < 65°C\n"
                                 "Warning: 65-75°C\n"
                                 "Critical: > 75°C\n"
                                 "Monitor this while overclocking!");
    }
    
    // Update temperature reading
    thermal_update();
    
    menu_stack[++menu_level] = &_menu_overclock;
    header_changed = true;
}

/**
 * Performance menu (contains overclocking and other performance options)
 */
void menu_performance(void *_)
{
    if (!_menu_performance._created) {
        _menu_performance = list_createWithTitle(2, LIST_SMALL, "Performance");
        
        // Link to overclock submenu
        list_addItem(&_menu_performance,
                     (ListItem){
                         .label = "Overclocking...",
                         .action = menu_overclock});
        
        // Current CPU info (read-only display)
        char cpu_info[STR_MAX];
        if (settings.overclock_enabled) {
            int gain = overclock_get_performance_gain(settings.overclock_frequency);
            snprintf(cpu_info, STR_MAX, "Currently: %d MHz (+%d%%)", 
                     settings.overclock_frequency, gain);
        }
        else {
            snprintf(cpu_info, STR_MAX, "Currently: 1200 MHz (Stock)");
        }
        
        list_addItemWithInfoNote(&_menu_performance,
                                 (ListItem){
                                     .label = cpu_info,
                                     .disabled = 1},
                                 "Current CPU frequency setting.\n"
                                 "Use the Overclocking submenu to\n"
                                 "adjust CPU frequency and thermal\n"
                                 "protection settings.");
    }
    
    menu_stack[++menu_level] = &_menu_performance;
    header_changed = true;
}

#endif // TWEAKS_OVERCLOCK_MENU_H__
