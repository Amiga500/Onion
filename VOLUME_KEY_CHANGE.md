# Volume Key Implementation Change

## Summary

Changed the volume control in gameSwitcher from using L2/R2 buttons to using the hardware volume keys (Volume Up/Down).

## Problem

The initial implementation used L2/R2 buttons for volume control, but this wasn't ideal because:
1. The hardware has dedicated volume keys that users expect to use
2. L2/R2 might be needed for other game functions
3. It's more intuitive to use volume keys for volume control

## Solution

### 1. Added Volume Key Mappings

**File: `src/common/system/keymap_sw.h`**

Added SDL key mappings for the hardware volume keys:
```c
#define SW_BTN_VOLUME_UP SDLK_WORLD_0
#define SW_BTN_VOLUME_DOWN SDLK_WORLD_1
```

These map to SDLK_WORLD_0 and SDLK_WORLD_1, which are unused SDL key codes suitable for custom hardware button mappings.

### 2. Updated Volume Control Logic

**File: `src/gameSwitcher/gs_keystate.h`**

Changed from:
```c
if (_gs_keystate.keystate[SW_BTN_L2] >= PRESSED && !_gs_keystate.select_pressed)
if (_gs_keystate.keystate[SW_BTN_R2] >= PRESSED && !_gs_keystate.select_pressed)
```

To:
```c
if (_gs_keystate.keystate[SW_BTN_VOLUME_DOWN] >= PRESSED)
if (_gs_keystate.keystate[SW_BTN_VOLUME_UP] >= PRESSED)
```

Also updated the key press detection to use volume keys instead of L2/R2:
```c
if (keystate[_gs_keystate.changed_key] == PRESSED && 
    _gs_keystate.changed_key != SW_BTN_VOLUME_DOWN && 
    _gs_keystate.changed_key != SW_BTN_VOLUME_UP)
    state->sound_changed = false;
```

### 3. Updated Documentation

Updated all documentation files to reflect the change:
- `RENDERSOUND_IMPLEMENTATION.md` - Updated button mappings and code examples
- `src/gameSwitcher/res/VOL_IMAGES_README.md` - Added note about volume key usage

## Benefits

1. **More Intuitive**: Users naturally press volume keys to adjust volume
2. **Consistent**: Matches system-wide volume control behavior
3. **No Conflicts**: Volume keys are dedicated to volume, no conflict with SELECT combos
4. **Simpler Code**: Removed the need for `!_gs_keystate.select_pressed` check

## Hardware to SDL Key Mapping

The hardware volume keys are mapped through multiple layers:

1. **Hardware Level** (`keymap_hw.h`):
   - `HW_BTN_VOLUME_UP` = `KEY_VOLUMEUP`
   - `HW_BTN_VOLUME_DOWN` = `KEY_VOLUMEDOWN`

2. **SDL Level** (`keymap_sw.h`):
   - `SW_BTN_VOLUME_UP` = `SDLK_WORLD_0`
   - `SW_BTN_VOLUME_DOWN` = `SDLK_WORLD_1`

The keymon daemon (at the system level) and gameSwitcher (at the application level) both handle these keys, but they control different volume settings:
- **keymon**: Controls `settings.volume` (main system volume)
- **gameSwitcher**: Controls `settings.bgm_volume` (background music volume)

## Testing

To test this implementation:
1. Launch gameSwitcher
2. Press the Volume Up button on the device
3. Verify the green volume slider appears and increases
4. Press the Volume Down button
5. Verify the slider appears and decreases
6. Verify the slider disappears after 2 seconds
7. Verify volume changes persist after exiting gameSwitcher

## Notes

- The L2/R2 buttons are now available for other functions if needed
- SELECT + L2/R2 still reloads settings (this functionality is unchanged)
- UP/DOWN buttons still control brightness (unchanged)
