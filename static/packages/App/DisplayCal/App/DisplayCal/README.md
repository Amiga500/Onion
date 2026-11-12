# DisplayCal v0.7.2

DisplayCal is a lightweight screen color-calibration app for the **Miyoo Mini and Miyoo Mini Plus** running Onion or RedOnion. Adjust red, green, blue, and warmth in real time while viewing calibration images.

The app writes directly to the SigmaStar display controller's color-temperature interface. It does not require SDL, modify MainUI, or install a startup hook.

## Installation

1. Extract the release ZIP to the root of your SD card.
2. Confirm the app is installed at `/mnt/SDCARD/App/DisplayCal/`.
3. Open **Apps → DisplayCal** in Onion or RedOnion.

No system files need to be patched, and calibration is **not** applied automatically at boot.

## Controls

| Button | Action |
| --- | --- |
| D-pad Up / Down | Select Red, Green, Blue, or Warmth |
| D-pad Left / Right | Adjust by ±1; hold to repeat |
| L1 / R1 | Adjust by ±5; hold to repeat |
| X | Reset the selected value (RGB to 128, Warmth to 0) |
| Y | Reset all values (RGB 128/128/128, Warmth 0) |
| Select (tap) | Next reference image |
| Select (hold) | Toggle full-screen reference image |
| Start | Save calibration and exit |
| B / Menu | Cancel unsaved changes and restore the prior calibration; in full-screen mode, return to the controls |

Held adjustment buttons begin repeating after approximately 300 ms and repeat every 75 ms. Resets are applied immediately but are **not saved** until you press Start.

## Reference images

Tap **Select** to cycle through:

1. **Color Bars** — check primary colors and saturation.
2. **White / Gray** — check for color casts across light, medium, and dark neutral areas.
3. **Gray Ramp** — inspect tone transitions and banding.
4. **Natural Colors** — judge the overall appearance of familiar colors.
5. **Onion Topbar** — preview `/mnt/SDCARD/miyoo/app/skin/miyoo-topbar.png` from your installed SD card.

Reference images are filtered and cached when downscaled for the preview pane. The Onion topbar is shown at its native pixel scale, without enlargement. Hold **Select** to inspect an image full-screen.

## RGB and warmth

Each RGB channel can be set from **0 to 255**. The neutral reference setting is **R=128, G=128, B=128**.

**Warmth** ranges from **−100 (cooler)** to **+100 (warmer)**, with `0` leaving the selected RGB gains unchanged. Warmth attenuates selected channels rather than boosting them, and is stored separately from RGB. The UI shows the **effective RGB** values sent to the display controller.

Warmth is a convenience adjustment, **not** a measured color-temperature setting in kelvin. DisplayCal adjusts global channel gains, not individual gamma curves or a panel's physical black level.

## Configuration

Profiles are saved per device:

```text
/mnt/SDCARD/.tmp_update/config/display/cal-SERIALNUMBER.cfg
```

DisplayCal uses Onion/RedOnion's `$SERIAL_NUMBER` when available, then `/tmp/deviceSN`. If neither provides a valid nonzero identifier, the interactive app uses `cal-default.cfg`. The fast `--apply` command deliberately skips that fallback to avoid applying a shared profile on an unidentified device.

Example configuration:

```ini
R=116
G=132
B=124
W=12
```

Opening the GUI applies an existing saved profile **only when Blue Light Filter is off**. **Start** saves and leaves calibration active. **B/Menu** discards unsaved changes and restores a previously saved profile. If there was no saved profile, Cancel does not write anything when no values were changed; after unsaved adjustments it restores the neutral starting baseline while Blue Light Filter is off.

### Blue Light Filter compatibility

Blue Light Filter and DisplayCal both write to the same SigmaStar `colortemp` control. **They are not independently composited.** If `/tmp/.blfOn` exists, the filter takes priority: the interactive tuner shows `BLF ON` and will not apply edits or save while the filter is on. Opening/Canceling it therefore will not replace the filter's color temperature with neutral RGB. Disable Blue Light Filter before calibrating.

`--apply` silently skips hardware writes while the filter is on. If the filter enables *after* `--apply` at boot, its later write still overrides the calibration. When the filter is disabled, Onion normally resets the hardware to neutral, not the DisplayCal profile. To make calibration automatically survive filter toggles, **RedOnion must call DisplayCal's `--apply` after disabling the filter**, and startup ordering must ensure `--apply` runs after display/filter initialization. This release does not patch Onion's Blue Light script or run a background watcher.

## Command line

All commands use the single launcher, `launch.sh`:

```sh
cd /mnt/SDCARD/App/DisplayCal

./launch.sh                     # Open the calibration UI
./launch.sh --serial            # Print detected device serial
./launch.sh --config            # Print the active config file path
./launch.sh --set 116 132 124 12  # Apply RGB and warmth without saving
./launch.sh --neutral           # Apply neutral 128/128/128 without saving
./launch.sh --apply             # Apply this device's saved profile without the UI
```

`--apply` is a silent, shell-only fast path. It does not launch the GUI, decode images, run `read_uuid`, create directories, or start external helper programs. It makes no change if the device has no saved profile **or if Blue Light Filter is active**. It reads Windows CRLF as well as Unix LF profiles. **No automatic startup integration is included.**

The hardware command is sent to `/proc/mi_modules/mi_disp/mi_disp0` in **Blue, Green, Red** order, as required by the driver.

GUI diagnostics are captured **only when enabled**:

```sh
DISPLAYCAL_DEBUG=1 ./launch.sh
```

Log path: `/mnt/SDCARD/.tmp_update/logs/displaycal.log`. By default, normal GUI launches do not append to the log, and `--apply` never logs.

## Credits and licensing

DisplayCal source code is MIT-licensed; see `LICENSE`.

The Gray Ramp, Natural Colors, and Color Bars images are from **onion-color-balance** by TR2NLG under the MIT license in `testimages/LICENSE`. The White / Gray reference image was provided for DisplayCal. The font artwork is based on Lato under the SIL Open Font License; see `LICENSE-LATO.txt`. The Onion topbar image is loaded from the user's SD card and is not bundled.
