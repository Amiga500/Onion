## 🧅⚡ OnionPlus

A personal, independent build of [Onion](https://github.com/OnionUI/Onion) `4.4.0-beta` for the
Miyoo Mini, Mini+, Mini v4 and Mini Flip. It keeps Onion's look, menus, emulators and file
layout. **Not an official Onion release.**

### ✨ What's in it

- 🌡️ **Menus stop heating the device** — GameSwitcher, Tweaks, Play Activity, Themes, Package
  Manager and Battery Monitor sleep between frames instead of keeping a CPU core at 100%.
- 💾 **Gentler on the SD card** — a volume or brightness step writes one file instead of about
  fourteen, and needless flushes to the card after keys, games and menus are gone.
- 🔌 **Settings survive power cuts** — `system.json`, the key map, every config value, the
  recent games list and the RetroArch options changed from Tweaks are replaced atomically.
- 📶 **Wi-Fi no longer holds the boot** — Wi-Fi, network services and the time sync come up in
  the background after the menu appears; "Enable Wi-Fi temporarily" now really syncs the clock.
- 🕒 **Clock fixes** — a failed time-zone lookup keeps your zone, and play time can no longer
  jump by decades when the clock is set from the network.
- 🧠 **GameSwitcher** — no memory leak on large histories, screenshots preloaded in the
  background, add/remove favorites from its menu.
- 📱 **Mini Flip support**, ported from OnionUI `v4.5-dev`.
- 🐛 **29 defects fixed in code shared with Onion**, with fixes available to the Onion team.

### 📦 Install & update

Same as Onion: see the [installation guide](https://onionui.github.io/docs/installation).
Installed builds update over the air from [`Amiga500/Onion` releases](https://github.com/Amiga500/Onion/releases).

### 🧭 Known issues

- Mini Flip lid and Hall-sensor handling is not yet confirmed on hardware.
- Some recent fixes still need a check on a real device — see
  [Known issues](https://github.com/Amiga500/Onion/tree/onionplus-compact#-known-issues--next-steps).

📖 Full details: [README](https://github.com/Amiga500/Onion/tree/onionplus-compact#readme)
