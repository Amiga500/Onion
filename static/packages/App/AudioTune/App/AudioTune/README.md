# AudioTune 0.2.9

AudioTune is a five-band sound tuning and diagnostic app for the Miyoo Mini and Mini Plus with Onion/RedOnion. It has an amber interface, tone and stereo tests, speaker/headphone presets and a software EQ preview. On specifically verified firmware, an optional SigmaStar VQE hook can carry the saved EQ into games and other audio using the same audio server. This is a **beta feature**, not a guarantee for every emulator or audio-output path.

## Sound and controls

The five bands are 250 Hz (low shelf), 500 Hz, 1 kHz, 4 kHz (peaking) and 8 kHz (high shelf). The shelving filters change all frequencies below or above their corner rather than only a narrow peak. Each supports −12 to +12 dB in 1 dB steps. Hold Up/Down in EQ to repeat. X resets the selected band, Y resets all bands, A compares Flat/EQ and START plays or stops music. On **both EQ and Sound Profiles, L2 switches the output target between SPEAKER and HEADPHONES**. The output choice is independent of the EQ preset and stays selected as you browse and apply sound profiles. This is a manual protection target; AudioTune does not detect headphone insertion. **R2 toggles the local soft limiter on EQ, and switches SAFE/PUNCHY headroom on Sound Profiles only when HEADPHONES is selected**. SPEAKER always forces SAFE in the native driver, so R2 does not switch mode while SPEAKER is selected (the screen shows "HP ONLY"). On HEADPHONES, the selected SAFE/PUNCHY mode is displayed immediately, even if a native hook is loaded; changes to the game EQ take effect when AudioTune exits. The EQ screen spells out the full SPEAKER/HEADPHONES output choice. These settings are saved with the EQ profile. Older profiles default to SAFE, SPEAKER output and the limiter off. Music stays stopped at launch, continues when moving between pages and stops when AudioTune closes. EQ presets include Flat, Presence, Full, Clear, Warm, Bright and Custom. Presence and Full replace the former Speaker and Headphones *EQ curve names*, not the separate output targets; existing saved preset numbers are preserved. The five built-in sound curves use more pronounced EQ values. Because automatic headroom compensation is retained, stronger curves can still become quieter overall. On the Sound Profiles page, moving the highlighted preset auditions its sound immediately while music plays; **A applies and saves** it. Leaving the page without pressing A restores the saved selection. Under an active VQE hook this preview requires a verified native snapshot and remains approximate. For the bands in the order listed above, their dB values are Presence −8/−4/+4/+7/+4, Full +6/+3/0/+4/+4, Clear −6/−3/+4/+8/+6, Warm +7/+5/+1/−3/−6 and Bright −3/−2/+2/+7/+9. These are listening-test starting points, not measured speaker corrections; stronger boosts can require more headroom and lower overall volume.

For custom music, put an MP3 named `sample.mp3` alongside `launch.sh`. The file is read when START first starts music, not on application launch; otherwise AudioTune loops its built-in demo. Stereo MP3 files up to 16 MiB are supported, with automatic fallback to the built-in demo if the file is invalid. START also controls the shared music player on the Sound Profiles page. Custom MP3 plays at its original PCM level (no fixed gain reduction), but an active EQ can still lower the output to preserve headroom. AudioTune first requests **48 kHz stereo SDL playback** (rather than 22.05 kHz), with a 22.05 kHz fallback if the bridge rejects 48 kHz. Higher playback rates increase CPU load. The optional soft limiter uses a smooth output knee; it affects only AudioTune's software playback, not other apps or games. It cannot prevent all audible distortion. A separate local soft-limit guard is automatically used while auditioning differences against a verified active native profile, to reduce pre-driver PCM clipping. That guard does not protect games. Test tones, pink noise and the built-in demo use reduced output for hearing and speaker safety; begin listening at a low device volume. AudioTune does not change global volume.

**Low battery:** High speaker volume may cause shutdowns when the battery is low. Reduce the volume or charge the device.

On the Home screen, B exits. On other screens, B returns Home. SELECT+START exits from anywhere after the buttons are released. If a native EQ change requires restarting `audioserver`, AudioTune first switches to a **black “APPLYING AUDIO SETTINGS” screen** and stays there until the process completes or a timeout occurs. A brief speaker pop can occur during the audio-server restart; this is not the test sound playing. When a hooked server is running, Home → X disables EQ and exits through the same restoration screen.

## Persistent settings

Profiles are stored at:

`/mnt/SDCARD/.tmp_update/config/audiotune.ini`

This directory is part of the SD card's persistent Onion/RedOnion configuration. AudioTune creates the configuration file when its settings are first saved. It does **not** import or migrate a previous `App/AudioTune/settings.ini`; remove the old file manually if desired. Changing a profile saves it automatically. A Custom profile with all five bands at 0 dB is treated as Flat for audio processing, while the Custom selection and its saved values remain unchanged. At boot, all-zero Custom settings do not start native VQE. If a previously hooked server is active, exiting AudioTune with all-zero settings restores the stock server. Otherwise, the launcher changes the native server only if its saved profile or hook version changed; a matching server is reused without restarting it.

The `output_mode=0` setting means SPEAKER; `output_mode=1` means HEADPHONES. This selects the EQ protection target, not the physical audio connector. The app's `config.json` remains in `App/AudioTune` and is only the Onion application manifest, not an EQ profile.

## Native EQ and safe restore

Native VQE applies only if an enabled profile has at least one nonzero effective band gain, the compiled `libaudiotune_vqe.so` is present and `/config/lib/libmi_ao.so` matches the exact library ABI checked for this beta. The required SHA-256 is `696a7270facf93987800ba7a340a4160ed11da0d7805d5b30bfac8ce44311212`. Any missing/unsupported component leaves the normal audio system untouched. The launcher never replaces an audio server with an unknown third-party preload.

At 48 kHz the native 129 bins are roughly 186 Hz apart. The 250 Hz low shelf is still only approximated by this coarse table; the mid and treble bands are more precise. The software preview and native hook now share the *same shelving/peaking filter equations*, but their different sample rates and VQE bin quantization mean they will not sound exactly identical.

**Speaker protection:** The SPEAKER output target reduces the first native frequency bin (around 93 Hz) to at most -16 dB, never boosts the second bin (around 279 Hz), and forces zero positive native gain even if PUNCHY is selected. This is designed to reduce the risk of low-frequency noise or distortion on the tiny built-in speaker, not a measured cure for humming. Select HEADPHONES only when using headphones; then the full low shelf and the +3 dB PUNCHY option remain available. Switching output targets affects the native hook only on exit, when a necessary server refresh occurs. If you hear continuous humming, select Flat, exit and verify with stock audio; do not assume it is normal.

**SAFE (default)** removes the largest positive response plus a 1 dB guard before software EQ and permits **no positive native-bin gains**. A strong preset can therefore sound quieter. **PUNCHY** relaxes this protection for HEADPHONES and lets the native table reach **at most +3 dB** (not the SDK's +20 dB capability). It also relaxes compensation in AudioTune's software preview. **PUNCHY CAN CLIP OR DISTORT LOUD GAME AUDIO.** There is no native/game-side limiter; AudioTune's optional software limiter does **not** protect game audio. Use low device volume when first testing Punchy, and return to SAFE if distortion occurs. The EQ screen shows the approximate native level cut and current mode; values and mode changes reach the driver only after AudioTune exits. A loaded hook alone does not prove every emulator uses the same audio route.

When the native hook is running and a verified EQ-readback snapshot is available, AudioTune can audition **an approximate live difference** between the currently applied native curve and the selected curve, rather than stacking the full software EQ a second time. This local audition is not an exact game preview because native EQ is discretized into 129 bins; changing a band's gain or selecting a preset still changes the **game** EQ only after AudioTune exits and synchronizes the server. A short-lived, per-process state file under `/tmp` supports the preview; it is not an SD-card log. If the readback snapshot is absent, the app does not invent an applied profile and uses the unchanged native output until exit. The differential audition applies local protective headroom and soft limiting; the limiter cannot protect games. Outside that audition, an active native hook is not processed by a second software EQ.

Safe/Punchy continue to provide bounded headroom rather than RedAmp's unrestricted preamp. The SPEAKER output forces Safe native gain even when Punchy is selected, because there is no game-audio limiter. AudioTune never changes the system volume setting itself.

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
