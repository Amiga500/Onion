# Button Lab v0.1 - Input controller test & diagnostics

A compact, self-contained **Onion** controller diagnostic app for the **Miyoo Mini and Mini Plus**, using the **Mini Plus front panel** as the on-screen controller layout. Written in C99 with SDL 1.2 for drawing and Linux evdev for raw button input. The native UI is 640 × 480, with no external fonts, audio, SDL_image or SDL_ttf dependencies.

## Features

- A recognizable Mini Plus-shaped handheld, with its buttons.
- Each button lights up when held and remains tinted after its first press. The device screen and the left diagnostics panel show live test results.
- Tracks 15 core controls: four D-pad directions, A/B/X/Y, L1/L2/R1/R2, SELECT, START and MENU.
- Optional system event indicators for VOL− and VOL+. These do not count against core completion.
- Raw `/dev/input/event0` reads without `EVIOCGRAB`; this does **not** stop or reconfigure Onion keymon. Falls back to SDL keyboard events if the device cannot be opened.
- No saved settings, no file writes, and no external resources required while running.

## Controls

| Input | Action |
| --- | --- |
| Press any button | Light it up and count a press |
| Hold **SELECT + START** for 1.8 seconds | Exit |
| Hold **MENU** for 3 seconds | Alternative exit (for faulty SELECT or START) |
| Hold **X + Y** for 2 seconds | Reset all test results |
| VOL+/VOL− | Optional; only if the device generates volume key events |

The original Miyoo Mini's volume wheel is mechanical, so it does not produce separate volume button events. Onion may intercept system-level key actions. The app does not try to override those behaviors.

## Input and device compatibility

The raw input mapping follows [Onion's hardware keymap](https://github.com/OnionUI/Onion/blob/main/src/common/system/keymap_hw.h). The native renderer is designed for 640 × 480; output on higher-resolution Mini models may be scaled by the device's display software. The illustration depicts a **Mini Plus**, even when running on a compatible original Mini.

If no button events are detected, confirm that `/dev/input/event0` is accessible on your firmware. When fallback SDL keyboard input is used, some system buttons will not be available; the UI displays "SDL INPUT MODE" instead of the usual progress hint.

**License:** MIT (`LICENSE`).
