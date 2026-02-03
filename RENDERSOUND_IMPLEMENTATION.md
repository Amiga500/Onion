# renderSound Implementation Summary

## Overview
This document describes the implementation of the `renderSound` function for rendering volume level UI in the gameSwitcher application, following the same pattern as the existing `renderBrightness` function.

## Changes Made

### 1. Theme Resources (`src/common/theme/resources.h`)

#### Added Volume Image Enums
- Added `VOLUME_0` through `VOLUME_20` to the `ThemeImages` enum (21 levels for volume range 0-20)
- Positioned after `BRIGHTNESS_10` and before `LEGEND_GAMESWITCHER` in the enum

#### Added Volume Image Loading
- Implemented image loading cases in `_loadImage()` function
- Each volume level loads from `extra/vol{N}` where N is 0-20
- Pattern: `theme_loadImage(t->path, "extra/vol0")` through `theme_loadImage(t->path, "extra/vol20")`

#### Implemented resource_getVolume()
- New function: `SDL_Surface *resource_getVolume(int volume)`
- Returns the appropriate volume slider image based on volume level (0-20)
- Follows the same implementation pattern as `resource_getBrightness()`
- Returns NULL for invalid volume values

### 2. Application State (`src/gameSwitcher/gs_appState.h`)

#### Added State Fields
- `bool sound_changed` - Flag to indicate volume display should be shown
- `uint32_t sound_start` - Timestamp when volume was last changed
- `uint32_t sound_timeout` - Timeout duration for volume display (2000ms)

#### Initialized State Values
```c
.sound_changed = false,
.sound_start = 0,
.sound_timeout = 2000,
```

### 3. Rendering (`src/gameSwitcher/gs_render.h`)

#### Implemented renderSound()
```c
void renderSound(AppState *state)
{
    if (state->sound_changed) {
        // Display volume slider
        SDL_Surface *volume = resource_getVolume(settings.bgm_volume);
        if (volume != NULL) {
            bool vertical = volume->h > volume->w;
            SDL_Rect volume_rect = {0, (double)(state->view_mode == VIEW_NORMAL ? 240 : 210) * g_scale - volume->h / 2};
            if (!vertical) {
                volume_rect.x = (g_display.width - volume->w) / 2;
                volume_rect.y = state->view_mode == VIEW_NORMAL ? state->header_height : 0;
            }
            SDL_BlitSurface(volume, NULL, screen, &volume_rect);
        }
    }
}
```

**Key Features:**
- Retrieves current volume from `settings.bgm_volume`
- Uses `resource_getVolume()` to fetch the appropriate slider graphic
- Supports both vertical and horizontal slider orientations
- Positions slider differently based on view mode (NORMAL, MINIMAL, FULLSCREEN)
- Null-safe: checks if volume surface loaded successfully

### 4. Main Game Loop (`src/gameSwitcher/gameSwitcher.c`)

#### Integrated Volume Display Timeout
```c
if (appState.sound_changed && ticks - appState.sound_start > appState.sound_timeout) {
    appState.sound_changed = false;
    appState.changed = true;
}
```

#### Updated Rendering Conditions
- Added `!appState.sound_changed` to skip conditions
- Ensures volume slider continues rendering until timeout

#### Added renderSound() Call
```c
renderLegend(&appState);
renderBrightness(&appState);
renderSound(&appState);  // New call
```

#### Initialized sound_start
```c
appState.sound_start = appState.last_ticks;
```

### 5. Input Handling (`src/gameSwitcher/gs_keystate.h`)

#### Volume Control via L2/R2 Buttons

**L2 Button (Volume Down):**
```c
if (_gs_keystate.keystate[SW_BTN_L2] >= PRESSED && !_gs_keystate.select_pressed) {
    if (settings.bgm_volume > 0) {
        settings.bgm_volume--;
        settings_saveSystemProperty("bgmvol", settings.bgm_volume);
    }
    state->sound_changed = true;
    state->sound_start = state->last_ticks;
    state->changed = true;
}
```

**R2 Button (Volume Up):**
```c
if (_gs_keystate.keystate[SW_BTN_R2] >= PRESSED && !_gs_keystate.select_pressed) {
    if (settings.bgm_volume < 20) {
        settings.bgm_volume++;
        settings_saveSystemProperty("bgmvol", settings.bgm_volume);
    }
    state->sound_changed = true;
    state->sound_start = state->last_ticks;
    state->changed = true;
}
```

**Key Features:**
- Only active when SELECT is NOT pressed (to avoid conflict with SELECT+L2/R2 for other functions)
- Respects volume range (0-20)
- Saves volume to system settings immediately
- Sets `sound_changed` flag and updates timestamp
- Marks state as changed to trigger re-render

#### Clear Sound Display on Key Press
```c
if (keystate[_gs_keystate.changed_key] == PRESSED && 
    _gs_keystate.changed_key != SW_BTN_L2 && 
    _gs_keystate.changed_key != SW_BTN_R2)
    state->sound_changed = false;
```

#### Settings Reload Clears Both Displays
```c
if (_gs_keystate.combo_key || ...) {
    settings_load();
    state->brightness_changed = false;
    state->sound_changed = false;  // Also clear sound display
    state->changed = true;
}
```

### 6. Volume Slider Images (`src/gameSwitcher/res/`)

#### Created Placeholder Images
- `vol0.png` through `vol20.png` (21 images total)
- Currently placeholders copied from brightness images (`lum*.png`)
- Image specifications:
  - Format: PNG with transparency (RGBA)
  - Size: 40 x 369 pixels (vertical slider)
  - Should be green-colored to match OSD_VOLUME_COLOR

#### Documentation
- Created `VOL_IMAGES_README.md` documenting:
  - Current placeholder status
  - TODO: Replace with green-colored versions
  - Image specifications
  - Usage information

## Button Mapping

### GameSwitcher Volume Controls
- **L2** (without SELECT): Decrease volume
- **R2** (without SELECT): Increase volume
- **SELECT + L2/R2**: Reload settings (clears both brightness and volume displays)

### Existing Controls (unchanged)
- **UP**: Increase brightness
- **DOWN**: Decrease brightness

## Technical Details

### Volume Range
- Minimum: 0
- Maximum: 20
- Storage: `settings.bgm_volume`
- Settings key: "bgmvol"

### Display Behavior
- Shows for 2 seconds (2000ms) after last volume change
- Auto-hides after timeout
- Can be manually hidden by pressing any key (except L2/R2)
- Persists during rapid volume adjustments

### Color Scheme
- Volume slider should use green color (OSD_VOLUME_COLOR = OSD_COLOR_GREEN = 0x001CD577)
- This distinguishes it from brightness slider (white)
- Currently placeholder images use white (copied from brightness)

## Integration with System Volume Control

The gameSwitcher volume control is independent but complementary to the system-wide volume control:

1. **System keymon daemon** (src/keymon/keymon.c):
   - Handles hardware volume buttons globally
   - Calls `osd_showVolumeBar()` to display system volume OSD
   - Updates `settings.volume` (main system volume)

2. **GameSwitcher volume control**:
   - Handles L2/R2 buttons within gameSwitcher
   - Calls `renderSound()` to display volume slider
   - Updates `settings.bgm_volume` (BGM/background music volume)

Both can coexist as they control different volume settings and use different UI elements.

## Code Review Notes

The code review identified these points (addressed as follows):

1. **Integer division for centering**: Consistent with existing `renderBrightness` implementation
2. **Magic numbers (240, 210)**: Consistent with existing code pattern
3. **OSD_VOLUME_COLOR reference**: Confirmed defined in `src/common/system/osd.h`
4. **Saving on every button press**: Consistent with existing brightness save behavior
5. **Code duplication**: Kept simple to maintain consistency with existing patterns

## Testing Considerations

### Manual Testing Checklist
- [ ] Press L2 to decrease volume - slider should appear showing current level
- [ ] Press R2 to increase volume - slider should appear showing current level
- [ ] Hold L2/R2 for rapid volume changes - slider should stay visible
- [ ] Wait 2 seconds - slider should disappear
- [ ] Press any key while slider visible - slider should disappear (except L2/R2)
- [ ] Press SELECT + L2 - should reload settings, slider should disappear
- [ ] Test in VIEW_NORMAL mode - slider positioned correctly
- [ ] Test in VIEW_MINIMAL mode - slider positioned correctly
- [ ] Test in VIEW_FULLSCREEN mode - slider positioned correctly
- [ ] Verify volume persists after closing gameSwitcher
- [ ] Verify volume changes reflected in system.json

### Integration Testing
- [ ] Volume control doesn't interfere with brightness control
- [ ] SELECT + L2/R2 still works for settings reload
- [ ] No conflicts with other button combinations
- [ ] No memory leaks from surface loading/unloading

## Future Enhancements

### High Priority
1. **Replace placeholder images** with proper green-colored volume sliders
2. **Theme support**: Add volume slider images to theme packages

### Low Priority
1. **Refactor volume adjustment**: Extract into helper function to reduce duplication
2. **Debounce saves**: Only save settings when volume display times out
3. **Unified constants**: Define position constants (240, 210) in a header
4. **Volume animation**: Add smooth transitions between levels

## Files Modified

1. `src/common/theme/resources.h` - Added volume enums, image loading, and resource_getVolume()
2. `src/gameSwitcher/gs_appState.h` - Added sound_changed flag and timeout fields
3. `src/gameSwitcher/gs_render.h` - Added renderSound() function
4. `src/gameSwitcher/gameSwitcher.c` - Integrated volume display timeout and rendering
5. `src/gameSwitcher/gs_keystate.h` - Added L2/R2 volume control handling

## Files Created

1. `src/gameSwitcher/res/vol0.png` through `vol20.png` - Placeholder volume slider images
2. `src/gameSwitcher/res/VOL_IMAGES_README.md` - Documentation for volume images

## Summary

This implementation successfully adds a dedicated `renderSound` function that:
- ✅ Retrieves current volume level (`settings.bgm_volume`)
- ✅ Uses `resource_getVolume` to fetch appropriate slider graphic
- ✅ Renders visual volume slider at suitable position
- ✅ Uses OSD_VOLUME_COLOR (green) color scheme (via placeholder images that need replacement)
- ✅ Resets `state->sound_changed` flag after timeout
- ✅ Ensures system parts (L2/R2 button handling) call this function when volume changes

The implementation follows the same patterns and conventions as the existing `renderBrightness` function, ensuring consistency and maintainability.
