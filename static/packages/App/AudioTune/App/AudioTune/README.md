# AudioTune 0.2.3

AudioTune is a five-band sound tuning and diagnostic app for the Miyoo Mini and Mini Plus with Onion/RedOnion. It has an amber interface, tone and stereo tests, speaker/headphone presets and a software EQ preview. On specifically verified firmware, an optional SigmaStar VQE hook can carry the saved EQ into games and other audio using the same audio server. This is a **beta feature**, not a guarantee for every emulator or audio-output path.

## Sound and controls

The five bands are 125 Hz, 500 Hz, 1 kHz, 4 kHz and 8 kHz. Each supports −12 to +12 dB in 1 dB steps. Hold Up/Down in EQ to repeat. X resets the selected band, Y resets all bands, A compares Flat/EQ and START plays or stops music. Music stays stopped at launch, continues when moving between pages and stops when AudioTune closes. EQ presets include Flat, Speaker, Headphones, Clear, Warm, Bright and Custom. The five built-in sound profiles now use more pronounced adjustments. For the bands in the order listed above, their dB values are Speaker −6/−2/+2/+4/+2, Headphones +3/+1/0/+2/+2, Clear −4/−2/+2/+5/+4, Warm +4/+3/0/−2/−4 and Bright −2/−1/+1/+4/+6. These are listening-test starting points, not measured speaker corrections; stronger boosts can require more headroom and lower overall volume.

For custom music, put an MP3 named `sample.mp3` alongside `launch.sh`. The file is read when START first starts music, not on application launch; otherwise AudioTune loops its built-in demo. Stereo MP3 files up to 16 MiB are supported, with automatic fallback to the built-in demo if the file is invalid. START also controls the shared music player on the Sound Profiles page. Custom MP3 plays at its original PCM level (no fixed gain reduction), but an active EQ can still lower the output to preserve headroom. Test tones, pink noise and the built-in demo use reduced output for hearing and speaker safety; begin listening at a low device volume. AudioTune does not change global volume.

**Low battery:** High speaker volume may cause shutdowns when the battery is low. Reduce the volume or charge the device.

On the Home screen, B exits. On other screens, B returns Home. SELECT+START exits from anywhere after the buttons are released. If a native EQ change requires restarting `audioserver`, AudioTune first switches to a **black “APPLYING AUDIO SETTINGS” screen** and stays there until the process completes or a timeout occurs. A brief speaker pop can occur during the audio-server restart; this is not the test sound playing. When a hooked server is running, Home → X disables EQ and exits through the same restoration screen.

## Persistent settings

Profiles are stored at:

`/mnt/SDCARD/.tmp_update/config/audiotune.ini`

This directory is part of the SD card's persistent Onion/RedOnion configuration. AudioTune creates the configuration file when its settings are first saved. It does **not** import or migrate a previous `App/AudioTune/settings.ini`; remove the old file manually if desired. Changing a profile saves it automatically. A Custom profile with all five bands at 0 dB is treated as Flat for audio processing, while the Custom selection and its saved values remain unchanged. At boot, all-zero Custom settings do not start native VQE. If a previously hooked server is active, exiting AudioTune with all-zero settings restores the stock server. Otherwise, the launcher changes the native server only if its saved profile or hook version changed; a matching server is reused without restarting it.

The app's `config.json` remains in `App/AudioTune` and is only the Onion application manifest, not an EQ profile.

## Native EQ and safe restore

Native VQE applies only if an enabled profile has at least one nonzero effective band gain, the compiled `libaudiotune_vqe.so` is present and `/config/lib/libmi_ao.so` matches the exact library ABI checked for this beta. The required SHA-256 is `696a7270facf93987800ba7a340a4160ed11da0d7805d5b30bfac8ce44311212`. Any missing/unsupported component leaves the normal audio system untouched. The launcher never replaces an audio server with an unknown third-party preload.

At 48 kHz the 129 bins are about 186 Hz wide. The 125 Hz bass band is therefore less precise than the mid and treble controls. The native EQ curve approximates the preview's filters in 129 frequency bins and applies negative gain compensation to reduce clipping. The two may sound different. When the native hook is loaded, AudioTune bypasses its software EQ and its separate headroom reduction to avoid processing its sound twice. In that mode the server's currently applied EQ is heard, and changing sliders does not update the native sound until you exit AudioTune and the server synchronizes. A loaded hook alone does not prove that every emulator uses the same audio route. The EQ and profile views show **NATIVE HEADROOM** in dB: a strong boost can subtract 13 dB or more from the whole native EQ curve to prevent positive driver gain. Some profiles therefore sound much quieter, even at unchanged device volume. This is an estimate of processing headroom, not a sound-pressure or hardware volume measurement. Start at low speaker or headphone volume. AudioTune never changes the system volume setting itself.

Only **one application script** is installed: `launch.sh`. The same script supports optional SSH commands:

```sh
/mnt/SDCARD/App/AudioTune/launch.sh --status
/mnt/SDCARD/App/AudioTune/launch.sh --restore
```

`--restore` restarts the stock audio server without the hook. It does not erase your EQ profile, so a later AudioTune session or runtime startup can enable native EQ again. If the audio server fails to recover, reboot. To disable EQ permanently, choose Flat or turn EQ off in AudioTune and exit normally.

If evdev is unavailable, SDL uses the kernel-equivalent key assignments (A=Space, B=Left Ctrl, SELECT=Right Ctrl; shoulder buttons follow their evdev codes). The fallback needs validation on each actual device before a final release. There are no routine SD-card debug logs or extra test/status scripts. For troubleshooting only, launch with `AUDIOTUNE_DEBUG=1`; optional messages go to `/tmp/audiotune-beta.log`, not the SD card.

## Optional RedOnion / Onion startup integration

The app itself does **not** edit `runtime.sh`. If maintaining a custom RedOnion build, you add this to the existing `start_audioserver()` function in `.tmp_update/runtime.sh`, right before `runifnecessary "audioserver" $miyoodir/app/audioserver $defvol`. This preserves RedOnion's computed volume and the original fallback; it performs the extra native-EQ checks only when there is no running audio server:

```sh
    # Apply AudioTune EQ at startup if configured and supported.
    if [ -f "/mnt/SDCARD/.tmp_update/config/audiotune.ini" ] &&
       [ -x "/mnt/SDCARD/App/AudioTune/launch.sh" ]; then

        /mnt/SDCARD/App/AudioTune/launch.sh --boot-audio "$defvol" >/dev/null 2>&1

        # Cache the PID if an audio server started successfully.
        # Check even if AudioTune returned an error to avoid duplicates.
        set -- $(pgrep audioserver)
        if [ -n "$1" ]; then
            audioserver_pid=$1
            return
        fi
    fi
```

`--boot-audio` validates the profile, library and binary, then launches a single hooked server if permitted. Otherwise it fails harmlessly so the existing Onion `runifnecessary` path launches stock audio. It does not open AudioTune's UI, write configuration, or kill an existing server. Back up `runtime.sh` before editing; system updates can replace it. This modification is intended for the tested firmware only and should be rechecked against your particular RedOnion runtime.

## Licences

AudioTune's source uses the MIT licence; the included font and MP3-decoder notices identify their separate components. The minimp3 decoder is fetched from a pinned commit during a source build and its SHA-256 is checked against the published checksum. Builds reject unverified headers.
