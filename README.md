<p align="center"><img src="docs/redonion.jpg" alt="RedOnion" width="480"></p>

# RedOnion

### Onion, the way you know it. Faster, sturdier, and still growing.

RedOnion (formerly OnionPlus) is an independent build of [Onion](https://github.com/OnionUI/Onion) for the Miyoo Mini, Mini+, Mini v4 and Mini Flip. Same menus, same themes, same emulators, same folders. What changes is everything underneath.

## Why it exists

Onion is one of the best things that ever happened to the Miyoo Mini. But in the last years many fixes have stayed in pull requests and branches, and no corrective release has reached the people who use it every day.

RedOnion started as my personal build to apply those fixes, find more, and test them on real devices. It grew into a build that others now use too. Every change stays public, so the Onion team can take whatever they find useful.

## What you get

- ⚡ **A faster everyday.** Game launches, menus, sleep, volume and the GameSwitcher were all rebuilt for speed, and the launcher idles at a fraction of the CPU.
- 🆕 **Open MainUI.** An open-source launcher by [@robcodedev](https://github.com/robcodedev/onionos-mainui-opensource) replaces Miyoo's closed one: about 270 KB instead of 1.4 MB, menus at about 1% CPU instead of 5%, scrolling titles, letter jump, configurable rows and main menu, safer ROM deletion. The stock MainUI stays on the card, one file away.
- 🛡️ **Fixes for issues still present in Onion.** 44 bugs fixed in the shared code: crashes, memory leaks, lost settings, a wrong clock, play history that could be deleted.
- 🔌 **Settings that survive a power cut.** Settings, key map, recents and play history are written safely, even if the battery dies mid-save.
- 🎮 **Mini Flip support** ported from Onion's development branch, with lid and suspend fixes.
- 📡 **Updates over Wi-Fi.** A stable channel for everyone and a beta channel for early testers, straight from the device.
- 🧪 **Tested on every change.** More than 1,500 automated tests run on each update before it ships.

## Switching from Onion

Your games, saves and BIOS files carry over. RedOnion keeps Onion's file layout.

1. Back up your SD card, at least `Roms`, `Saves`, `BIOS` and `Screenshots`.
2. Download the latest release from the [Releases page](https://github.com/Amiga500/Onion/releases).
3. For the cleanest start, format the card, copy the release onto it, then copy back `Roms`, `Saves`, `BIOS` and `Screenshots`.
4. Insert the card and power on. Later updates arrive through the OTA app.

## Good to know

- RedOnion is not an official Onion release and is not affiliated with the Onion team. Onion's [documentation](https://onionui.github.io/docs) still describes how everything works.
- Found a bug? [Open an issue](https://github.com/Amiga500/Onion/issues) with your model, version and steps. Verified reports count for the bounty.
- Want the details? The [technical reference](docs/TECHNICAL.md) lists every change, measurement and test.

## Thanks

RedOnion exists because of the Onion team and its contributors: the menus, emulators, themes and almost all of the code are theirs. Thanks to @robcodedev for Open MainUI, and to the testers who sent reports and helped confirm the fixes, especially @Zazzago.
