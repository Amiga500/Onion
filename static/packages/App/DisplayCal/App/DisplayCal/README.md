# DisplayCal v0.8.4

DisplayCal is a lightweight display-calibration app for **Miyoo Mini and Miyoo Mini Plus** running **Onion or RedOnion**. It provides live RGB/warmth adjustment, brightness and LCD color controls, and reference images for comparing color casts, tone and saturation.

The GUI is a small, freestanding ARM framebuffer executable. It does **not** use SDL and installs just one shell launcher, `launch.sh`. It does not patch MainUI or install an automatic startup hook.

**Device testing:** Has been tested successfully on multiple Miyoo devices. The LCD controls use the same Get/Set interface and value conversions as MainUI, while the app maintains its own lightweight framebuffer interface. Hardware support can still vary by firmware and display state.

## Installation

1. Extract the app ZIP at the **root of the SD card**.
2. Verify the installation directory is `/mnt/SDCARD/App/DisplayCal/`.
3. Open **Apps → DisplayCal** in Onion/RedOnion, or run `./launch.sh` from that directory.

No system files need to be patched. Calibration is automatically applied at boot in newer RedOnion. On other systems you might need to integrate `--apply` into your own startup scripts.

## Controls

DisplayCal has two pages. The reference-image panel remains visible while switching between them.

| Button | Action |
| --- | --- |
| **A** | Toggle between **RGB / Warmth** and **Display** pages |
| **L2 / R2** | Select the RGB or Display page |
| D-pad Up / Down | Select a setting |
| D-pad Left / Right | Adjust by **±1**; hold to repeat |
| L1 / R1 | Adjust by **±5**; hold to repeat |
| X | Reset the selected setting to its default |
| Y | Reset **all settings on the current page** |
| Select (tap) | Next reference image |
| Select (hold) | Toggle full-screen reference image |
| Start | Save changes and exit |
| B / Menu | Cancel unsaved changes and restore entry values; return to controls if viewing a full-screen image |

Held adjustment buttons start repeating after approximately **300 ms** and then repeat about every **75 ms**. Resets change the live values immediately when the relevant hardware path is available, but are **not saved** until you press Start.

The GUI uses RedOnion-inspired highlights in **`#EB5F7F`**; the red, green and blue channel labels retain their respective primary colors.

## Reference images

Tap **Select** to cycle through these five previews, in order:

1. **Color Bars** — examine primary colors and saturation.
2. **White / Gray** — identify color casts in bright, medium and dark neutrals.
3. **Gray Ramp** — inspect tonal transitions, banding and near-black detail.
4. **Natural Colors** — judge the overall appearance of familiar colors.
5. **Onion Topbar** — load `/mnt/SDCARD/miyoo/app/skin/miyoo-topbar.png` from the installed SD card.

The preview preserves each image's aspect ratio. Downscaled images use a cached filtered preview rather than simple nearest-neighbor sampling. The **Onion Topbar** is displayed at its **native pixel size**, without upscaling. Hold **Select** to inspect the current image full-screen.

## RGB / Warmth page

The RGB page controls **Red**, **Green**, **Blue** and **Warmth** independently of the five MainUI display settings.

| Setting | Range | Default |
| --- | --- | --- |
| Red | 0–255 | 128 |
| Green | 0–255 | 128 |
| Blue | 0–255 | 128 |
| Warmth | −100 to +100 | 0 |

**Warmth** is a convenience control rather than a measured color temperature in kelvin. It is stored separately from the base RGB values:

- Positive warmth reduces green and blue relative to red.
- Negative warmth reduces red and green relative to blue.
- Warmth `0` leaves the selected RGB channel values unchanged.

Warmth attenuates channels instead of boosting them above their base RGB values. The UI displays the **effective RGB** values sent to the display controller.

### Safety and neutral profiles

Individual RGB channels retain the full **0–255** range. To prevent a black-screen profile from being written or automatically restored at boot, the **sum of the effective RGB values after Warmth is applied must be at least 70**. The GUI refuses an adjustment below that limit, leaves the last valid hardware values active, and displays **RGB TOO DARK**. Saved or manually edited profiles below the threshold are rejected by `--apply` without a display write. The threshold is a practical guard, not a guarantee of readability for every panel.

A completely neutral profile (`R=128`, `G=128`, `B=128`, `W=0`) means **no calibration**. Saving it removes the existing per-device `.cfg` file (or leaves it absent). Existing manually created neutral profiles are ignored by `--apply`; the GUI also skips applying them on startup. An explicit `--neutral` command still writes neutral values when requested.

The app adjusts **global channel gains**, not per-channel gamma curves or the LCD panel's physical black level. It may improve a color cast without fully matching a different display panel.

### RGB hardware interface

DisplayCal writes directly to:

```text
/proc/mi_modules/mi_disp/mi_disp0
```

The kernel command uses **Blue, Green, Red** order, not RGB order:

```sh
printf 'colortemp 0 0 0 0 %s %s %s\n' "$B" "$G" "$R" > /proc/mi_modules/mi_disp/mi_disp0
```

The GUI uses a direct C `open()`/`write()` implementation; `launch.sh --apply` uses a shell `printf` redirection. Both send the same kernel command. RGB adjustment does **not** require initializing the SigmaStar userspace DISP SDK.

## Display page

The Display page exposes five settings shared with MainUI:

| Setting | Range | Default | Live hardware method |
| --- | --- | --- | --- |
| Brightness | 0–10 | 7 | PWM backlight |
| Lumination | 0–20 | 7 (Mini 283), 10 (Mini+ 354 / Flip 285) | SigmaStar DISP SDK |
| Hue | 0–20 | 10 | SigmaStar DISP SDK |
| Saturation | 0–20 | 10 | SigmaStar DISP SDK |
| Contrast | 0–20 | 10 | SigmaStar DISP SDK |

The Lumination factory default is selected using `/tmp/deviceModel` when the GUI starts.
Model **283** (Mini / Mini v4) resets to **7**, and models **354** (Mini+) and **285**
(Flip) reset to **10**. If the model cannot be identified, the existing fallback
of 10 is retained. A valid Lumination saved in `system.json` always takes precedence;
X and Y use the device-specific factory default, while Cancel restores the loaded value.

**Brightness** uses the MainUI mapping for levels 0–10:

```text
3, 4, 6, 9, 12, 17, 25, 35, 50, 70, 100
```

It writes the mapped duty-cycle value via the PWM sysfs interface.

**Lumination, Hue, Saturation and Contrast** are applied using:

```c
MI_DISP_GetLcdParam(0, &params);
/* Update the four LCD color values, preserving matrix and sharpness. */
MI_DISP_SetLcdParam(0, &params);
```

The conversions match MainUI:

| Setting | Value written to LCD parameters |
| --- | --- |
| Lumination | `lumination + 35` |
| Hue | `hue * 5` |
| Saturation | `saturation * 5` |
| Contrast | `(contrast + 15) * 2` |

The source uses the existing **24-byte LCD parameter layout** for the meaningful fields, with spare buffer padding. It preserves the matrix and sharpness fields obtained from the driver. There is **no raw procfs `csc` fallback** for these four live controls.

### LCD SDK initialization and errors

The LCD controls initialize their SigmaStar SDK session **lazily**, on the first LCD color adjustment. DisplayCal loads `libpthread`, `libmi_common`, `libmi_sys` and `libmi_disp` in order using `RTLD_NOW | RTLD_GLOBAL`, then calls `MI_SYS_Init()` and performs an initial `MI_DISP_GetLcdParam()` readiness read. Subsequent adjustments reuse the session and perform a fresh Get/Set pair. `MI_SYS_Exit()` runs when the GUI exits.

The first LCD adjustment may pause briefly while the SDK session is initialized. Subsequent changes reuse that session. The initialization does not invoke an explicit display repair or restart, and the cause of the occasional pause has not been conclusively isolated.

DisplayCal checks whether `/proc/mi_modules/mi_disp/mi_disp0` contains `CscMatrix`. If it is absent, or if the DISP SDK rejects an operation, the GUI reports an error such as **DISP UNAVAILABLE**, **SDK ERR**, or **SDK FAILED - SAVE ONLY**. Values can still be saved for later application, but a failed live call must not be taken as proof that the screen was updated.

## Configuration and saving

### Per-device RGB/Warmth profiles

RGB and warmth are saved to:

```text
/mnt/SDCARD/.tmp_update/config/display/cal-SERIALNUMBER.cfg
```

Example:

```ini
R=116
G=132
B=124
W=12
```

DisplayCal reads Onion/RedOnion's **`$SERIAL_NUMBER`** when valid, otherwise **`/tmp/deviceSN`**. The all-zero serial is not treated as a valid device identifier.

When no valid serial is available, the interactive GUI uses **`cal-default.cfg`**. The headless `--apply` command deliberately **does not** apply that fallback profile, to avoid applying a shared calibration to the wrong device.

Opening the GUI applies a saved, non-neutral, valid RGB profile when the Blue Light Filter is inactive. **Start** saves changed RGB/Warmth values and leaves the applied calibration active. **Cancel** restores the saved profile if changes were made, or the neutral starting baseline when there is no saved profile and an RGB adjustment was made. If nothing changed, Cancel avoids an unnecessary RGB write.

### Shared MainUI settings

Brightness, Lumination, Hue, Saturation and Contrast are loaded from and saved to:

```text
/mnt/SDCARD/system.json
```

On **Start**, DisplayCal updates those five top-level JSON properties through a temporary-file write and atomic rename, preserving unrelated settings. On **Cancel**, it attempts to restore the live entry values without modifying the file.

**Keymon synchronization:** After saving `system.json`, DisplayCal creates `/tmp/settings_changed` so keymon can reload settings on a subsequent input event. It does not yet update keymon's shared memory directly or use MainUI's cross-process settings lock. This has worked in multi-device testing, but simultaneous settings changes from another process can still race with the save. See [Keymon integration](#keymon-integration) for the planned improvement.

Save failures are reported in the GUI. Since the RGB profile and `system.json` are separate files, a failure in one save operation may leave the other already saved.

## Blue Light Filter compatibility

Blue Light Filter and DisplayCal both use the same SigmaStar `colortemp` control. **Their adjustments are not automatically combined.**

- When `/tmp/.blfOn` indicates the filter is active, the GUI shows **BLF ON** and blocks live RGB/Warmth edits. The filter takes priority over the RGB calibration.
- `launch.sh --apply` silently skips its RGB write while the filter is active.
- An active filter can overwrite calibration applied earlier during startup.
- When the filter is turned off, Onion may reset RGB to neutral rather than reapply the saved DisplayCal profile.

To retain a per-device calibration after filter toggles, RedOnion must call DisplayCal's `--apply` **after** the filter is disabled. Startup ordering must also account for the Blue Light Filter and display initialization. DisplayCal does not modify `blue_light.sh` and has no background watcher.

> `--neutral` and `--set` are explicit manual overrides; unlike `--apply`, the launcher does not automatically skip them because BLF is active. Avoid using them while the filter is on.

## Command-line use

All commands are handled by the **single** `launch.sh`:

```sh
cd /mnt/SDCARD/App/DisplayCal

./launch.sh                       # Open the GUI
./launch.sh --serial              # Show the detected serial
./launch.sh --config              # Show the active RGB profile path
./launch.sh --set 116 132 124 12  # Apply RGB and Warmth, without saving
./launch.sh --neutral             # Apply RGB 128/128/128, without saving
./launch.sh --apply               # Apply the saved profile headlessly
```

**`--apply` is the fast boot-time path:** it uses shell built-ins, does not open the GUI, decode PNGs, call `read_uuid`, initialize SigmaStar userspace libraries, read `system.json`, write logs, or adjust brightness/CSC. If no valid device serial/profile is available, the saved profile is neutral, or BLF is active, it exits without altering the display. Profiles below the minimum effective RGB total are rejected before any hardware write. It accepts both Unix LF and Windows CRLF profile files.

No automatic startup integration is installed by DisplayCal. If you add one to RedOnion, coordinate it with Blue Light Filter initialization and any display reset/CSC restoration so later writes do not overwrite calibration.

## DISP recovery and troubleshooting

### Optional RedOnion repair before and after the GUI

On a **normal GUI launch**, `launch.sh` checks the DISP proc output for `CscMatrix`. If it is missing and a compatible RedOnion script is installed, the launcher calls:

```sh
disp_csc_reset.sh --repair
```

The launcher looks in `.tmp_update/script/`, then `.tmp_update/bin/`, then `PATH`. The lookup can be overridden with `DISPLAYCAL_REPAIR_SCRIPT`.

The launcher performs this check **before opening the GUI** and **again after the GUI exits**. When the CSC section is present, no repair is run. If a repair fails or the script is unavailable, the launcher reports the problem to stderr and continues; it does not embed or duplicate the RedOnion `l`/`disp_init` restart sequence.

**The C application does not invoke this external repair.** Its lazy SigmaStar SDK initialization may make a first LCD adjustment slow, but that is distinct from `disp_csc_reset.sh --repair`. If the entire CSC section is missing, the C LCD adjustment path refuses the operation rather than trying to restart DISP.

The fast `--apply` path **never** invokes repair. A present `CscMatrix` does not guarantee that every DISP SDK call will succeed.

### Diagnostics

Normal GUI launches do **not** write a persistent log. Enable it when troubleshooting:

```sh
DISPLAYCAL_DEBUG=1 ./launch.sh
```

Log location:

```text
/mnt/SDCARD/.tmp_update/logs/displaycal.log
```

The GUI also writes the most recent RGB procfs failure to **`/tmp/displaycal-last-error`** when a write fails. The `/tmp` diagnostics do not require SD-card writes. `--apply` does not write to the GUI log.

## Compatibility and technical notes

DisplayCal has been exercised on multiple Miyoo devices with stable results. It remains sensible to verify that saved Display settings are reflected in MainUI, particularly when trying a different Onion/RedOnion build. The usual Miyoo Mini L2/R2 mappings are supported; other device revisions may expose different input-event codes.

### Keymon integration

Onion/RedOnion uses a shared-memory settings interface alongside `system.json`. Keymon's brightness and LCD-color values can be synchronized through the existing `InitKeyShm` / `SetKeyShm` API. DisplayCal currently uses the supported reload marker (`/tmp/settings_changed`) instead of writing shared memory itself.

For tighter integration in a future release, the five values should be synchronized using **the exact keymon shared-memory ABI for the target Onion/RedOnion build**, together with MainUI-compatible file locking and an atomic `system.json` update. This should be one controlled save operation—not ad hoc shared-memory writes on every slider movement—and must account for keymon's pending/debounced saves. Until that integration is verified, avoid changing the same settings simultaneously in DisplayCal and another program.

The GUI's live display adjustments and the fast `launch.sh --apply` path do not depend on this future change.

## Credits and licensing

DisplayCal source code is released under the **MIT License**; see `LICENSE`.

- **Color Bars**, **Gray Ramp**, and **Natural Colors** reference images come from **onion-color-balance** by **TR2NLG**; see its MIT license in `testimages/LICENSE`.
- The **White / Gray** reference image was supplied for DisplayCal.
- The UI font artwork is based on **Lato**, licensed under the **SIL Open Font License**; see `LICENSE-LATO.txt`.
- The **Onion Topbar** image is loaded from the user's installed SD card and is **not bundled**.
