# Tweaks Application Architecture

This document describes the architecture of the Tweaks application in OnionOS.

## Overview

Tweaks is the system settings application that allows users to configure
various aspects of OnionOS. It uses a menu-based UI built on the `List`
component system.

## File Structure

```
src/tweaks/
├── tweaks.c          # Main entry point and render loop
├── menus.h           # Menu definitions and navigation
├── actions.h         # Action handlers for menu items
├── values.h          # Value getters for menu items
├── formatters.h      # Value display formatters
├── appstate.h        # Global state and menu instances
├── network.h         # Network configuration menus
├── icons.h           # Icon pack management
├── diags.h           # Diagnostic tools
├── reset.h           # Reset functionality
├── tools.h           # Utility tools
├── tools_defs.h      # Tool definitions
└── info_dialog.h     # Info dialog helpers
```

## Menu System

### Navigation Stack

Menus are organized as a stack:
- `menu_level`: Current depth (0 = main menu)
- `menu_stack[]`: Array of List pointers representing the navigation path

```
menu_stack[0] -> main menu
menu_stack[1] -> current submenu
menu_stack[2] -> sub-submenu
...
```

### Menu Hierarchy

```
menu_main
├── menu_system
│   ├── menu_systemStartup       (auto-resume, start app, start tab)
│   ├── menu_systemDisplay       (display settings)
│   ├── menu_datetime            (date/time configuration)
│   └── menu_systemSaveAndExit   (shutdown options)
├── menu_network                 (WiFi, services, NTP - Miyoo354 only)
│   ├── menu_wifi               (WiFi configuration)
│   ├── menu_ssh                (SSH server)
│   ├── menu_ftp                (FTP server)
│   └── ...
├── menu_buttonAction            (button shortcuts)
│   ├── menu_buttonActionMainUIMenu
│   └── menu_buttonActionInGameMenu
├── menu_userInterface           (appearance settings)
│   ├── menu_blueLight          (blue light filter)
│   ├── menu_themeOverrides     (theme customization)
│   └── menu_batteryPercentage
├── menu_advanced                (emulator tweaks, resets)
│   ├── menu_resetSettings
│   └── menu_diagnostics
└── menu_tools                   (utilities)
    ├── menu_screen_recorder
    └── menu_tools_m3uGenerator
```

### Menu Item Types

From `components/list.h`:
- `TOGGLE`: Boolean on/off toggle
- `MULTIVALUE`: Select from multiple options
- `ACTION`: Single action button

### Menu Creation Pattern

Menus are created lazily using the `_created` flag:

```c
void menu_example(void *_)
{
    if (!_menu_example._created) {
        _menu_example = list_createWithTitle(N, LIST_SMALL, "Title");
        // Add items...
    }
    menu_stack[++menu_level] = &_menu_example;
    header_changed = true;
}
```

## Action System

Actions are callbacks that respond to menu item interactions:

```c
void action_example(void *pt)
{
    ListItem *item = (ListItem *)pt;
    int value = item->value;           // Current value
    int id = item->action_id;          // Disambiguation ID
    char *payload = item->payload;     // Extra data
    // Handle action...
}
```

## Value System

Value getters retrieve current settings:

```c
int value_example(void)
{
    // Read from config, calculate, etc.
    return current_value;
}
```

## UI State Flags

- `quit`: Set to exit the application
- `all_changed`: Force complete redraw
- `header_changed`: Redraw header only
- `list_changed`: Redraw list area
- `footer_changed`: Redraw footer
- `reset_menus`: Rebuild all menus

## Future Refactoring Opportunities

### Split menus.h

The current 1000+ line `menus.h` could be split:
- `menus_system.h`: System menus
- `menus_network.h`: Network menus (already partially separate)
- `menus_ui.h`: UI/appearance menus
- `menus_tools.h`: Tools and utilities

### Separate Implementation from Headers

Current pattern uses inline functions in headers. This could be refactored to:
- `.h` files: Declarations only
- `.c` files: Implementations

This would:
- Reduce compile time
- Improve code organization
- Enable better testing

### Menu Registry

Replace static `List` instances with a dynamic registry:
```c
// Instead of:
static List _menu_system;

// Use:
List *menu_get(const char *name);
void menu_register(const char *name, MenuBuilder builder);
```

## Dependencies

```
tweaks.c
└── menus.h
    ├── actions.h
    │   ├── appstate.h
    │   ├── values.h
    │   └── reset.h
    ├── values.h
    │   ├── appstate.h
    │   └── formatters.h
    ├── network.h
    │   └── appstate.h
    ├── icons.h
    │   └── appstate.h
    └── tools.h
        └── appstate.h
```

## Build Notes

The Tweaks application is built as part of the OnionOS build:
```
make apps    # Build all applications including Tweaks
```

Target: ARM Cortex-A7 (Miyoo Mini)
