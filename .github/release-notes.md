<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅⚡ OnionPlus stable update: play history fix and more

OnionPlus is a personal-use build based on Onion `4.4.0-beta`, with Miyoo Mini Flip support ported from `4.5-dev`. It is **not** an official Onion release and isn't affiliated with the Onion team. It runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

## ⚠️ Play history fix (please update)

The previous stable release (V4.4.0-beta-20260928) could **delete play history**. When a game ended, and before every suspend, it removed every Play Activity entry longer than 24 hours. Play times imported from older Onion versions are stored as one entry per game with its total, so games with more than 24 hours of imported play time could lose it. Sessions recorded normally were not affected.

This release only checks the session being closed, so stored play time is never removed for its length. Opening the database also never drops its tables anymore, so not even a transient SD error can wipe the history. Time already removed can't be recovered from the database; if you have a backup of `Saves/CurrentProfile/play_activity/` from before updating to OnionPlus, you can restore it.

Thanks to LincolnWinston for sharing the Codex review that found it.

## 🔧 Other fixes

- **Mini Flip lid (#228):** with the lid set to Suspend, closing it could shut the device down instead.
- **GameSwitcher (#233):** removing the running game and starting the one that takes its place went back to the main menu instead of launching it. A long-standing bug, also in official Onion.
- **RetroArch menu on vertical games (#235):** the RetroArch menu was drawn with the game's rotation. It's now always upright, and the game keeps its rotation. RetroArch is updated to `1.22.2-2`.
- **OTA updates:** the updater no longer runs a file system repair on the SD card while it's in use, which could damage files. If you see `FSCK0000.REC`-style files in the root of your card, check the card on a PC.
- **Mini v4 resolution:** a boot-time shortcut could keep the Mini v4 at 640x480 for the whole session. It now waits for the display driver, as official Onion does.

## 🧪 Tests

- A new test runs the Play Activity SQL on a real in-memory SQLite database and checks that stored play time survives; it fails with the old code. 1487 host tests pass in CI.
- **CI:** the infoPanel GTest suite never ran, because `make test` called the wrong binary name. It runs and passes now.
- Not every test suite exercises the production code: 4 suites (config, theme config, play activity paths, savestate paths) still test a local copy of the function, and those can stay green if the real code regresses. The README claimed none did; it's corrected, and those suites are being moved to the production code.

## 🚧 Known issues, fix in progress

- **Mini Flip charging:** a Flip that is charging doesn't wake up when the lid is opened. Use the power button for now.
- **Settings:** two programs saving settings at the same moment can corrupt the settings file, and a settings save that fails is not retried.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package. Verifying certificates and the package's SHA-256 is planned.

## 🆕 Open MainUI

The open-source MainUI replacement is still in **beta**: set the OTA updater to the beta channel to try it. See the pre-releases on the releases page.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Discussions: https://github.com/Amiga500/Onion/discussions

> ⚠️ **Back up your SD card before updating**, at least `Roms`, `Saves`, `BIOS` and `Screenshots`.
