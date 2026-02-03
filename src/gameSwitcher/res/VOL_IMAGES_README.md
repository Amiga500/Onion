# Volume Slider Images

## Current Status
The volume slider images (vol0.png through vol20.png) are currently placeholders copied from the brightness slider images (lum*.png).

## TODO
These placeholder images should be replaced with proper green-colored volume slider graphics to distinguish them from the brightness slider (which is white).

The OSD_VOLUME_COLOR constant is set to green (0x001CD577) to match the system's volume bar color scheme.

## Image Specifications
- Format: PNG with transparency (RGBA)
- Size: 40 x 369 pixels (vertical slider)
- Count: 21 images (vol0.png through vol20.png) representing volume levels 0-20
- Color: Should use green color scheme to differentiate from brightness slider

## Usage
These images are loaded by the `resource_getVolume()` function in `src/common/theme/resources.h` and rendered by the `renderSound()` function in `src/gameSwitcher/gs_render.h`.

Volume is controlled using the hardware volume keys (Volume Up/Down buttons) on the device.
