<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅🔴 RedOnion 4.5.1 beta 3: safer updates, standby and battery fixes, Netplay clean-ups, Play Activity you can trust

> ⚠️ **Always back up your SD card before updating!** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.

> 🧪 **Beta.** This build goes to the RedOnion testers first (testers OTA channel), then to devices on the **beta** channel. Devices on the **stable** channel stay on 4.5.0. Please report what you find.

RedOnion is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

This beta keeps the **release candidate of Open MainUI 1.1.1** (rc7) by @robcodedev of beta 2, as a prebuilt binary. Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource

Beta 3 is mostly fixes: a full review of RedOnion's code turned up about 80 problems, most of them inherited from Onion, and nearly all of them are fixed here. Many only showed up in rare cases (a power cut, a full card, a hand-edited file), but some could cost saves, play time or a working card.

## 🆕 New in 4.5.1 beta 3

### 🛟 Installing and updating

- **An OTA update cut short** (battery, power off, an extraction error) no longer leaves a console that doesn't start: the boot scripts installed on the card are kept until the new installer is in place.
- **Installer:** pressing POWER at the "Update / Reinstall" question no longer deletes the installer (every boot then ended in "Install script exited unexpectedly").
- **Installer:** a failed extraction is now seen as a failure (it could show 100% and delete the package), and **"Installation failed"** stays on screen for 5 s before the restart.
- **The installation ends with a clean shutdown:** the card is unmounted before the restart, so no "SD card check" notice after installing from a PC.
- **Shutdown started from a terminal** (Terminal app, SSH, Telnet, the OTA) now completes: it unmounts the card and restarts instead of freezing on the last screen.
- **Guest Mode on during an update:** the two profiles are no longer swapped at the next switch. On a card updated to beta 2 with Guest Mode on, the empty guest profile it created is renamed `GuestProfile-unused-<date>` and nothing is lost.
- An update **keeps your X and Y choices** for MainUI (they went back to Search and GLO), and no longer puts an old `Backup/retroarch.cfg` over RetroArch's settings.
- A folder named `temp` at the root of the card no longer stops the updates' migrations.
- **Syncthing** is no longer downloaded again (25 MB) after every update.

### 🔌 Boot

- A damaged time, "add hours" or backlight setting file no longer makes the console restart at every boot.
- MainUI and RedOnion always read the same `system.json` (after a start without the card, or straight after the stock firmware, they could use two different files for the whole session).
- **Mini v4 and Mini Flip:** DraStic and GnGeo start in 752x560 again.
- **Mini+ and Flip:** the time sync stores UTC in the hardware clock, so the clock is no longer off by your time zone after a restart.

### 😴 Standby, power and battery

- **MENU held between 0.3 and 0.7 s in a game** no longer freezes the button handling (no standby, lid or low-battery save until the next key).
- A screenshot on a **full card** no longer stops the buttons and the backlight.
- **Mini Flip:** the power-off at the end of a standby runs once, and the game is resumed at the next start. Woken with the lid still closed, the Flip goes back to standby.
- **DraStic** closes properly from standby (auto save state, a few seconds instead of about 90).
- A game **without auto save state** stays paused in standby instead of running on in the dark.
- **Mini:** after charging (with the console on or in standby) the battery level is the real one, not one close to the old level.
- A volume key released during MENU+VOL no longer mutes later, and ports that use MENU as a game key no longer turn POWER into a screenshot button.
- After the Scraper, however it was closed, the console goes to standby again and the CPU slows down again.

### 🕹️ Play Activity

- Pressing POWER twice quickly no longer counts the standby as play time.
- A session cut by a power loss is dropped at the next boot, instead of counting the whole time the console was off (a night on the charger).
- A play activity database left empty by a failed first write is repaired by itself.

### 🔀 GameSwitcher

- Turning the console off with the GameSwitcher open over a game now **resumes that game** at the next start.
- No crash on a game whose name is only brackets, like `(Prototype).nes`.
- When the last game played was deleted, the first entry no longer shows it and starts another game.
- Removing the last history entry over a game gives the game back instead of leaving it frozen.
- A failed "Save" no longer leaves an empty save state in "Load".

### 🛠️ Tweaks, apps and tools

- **Reset system tweaks** resets the settings only: the installer's state, Syncthing's identity and the Netplay files stay, and RedOnion's defaults come back (Apps by name, console pictures, Saved Games: Overwrite).
- **Blue light filter** turned off with MENU+B stays off, and a flag changed in Tweaks is no longer undone by a volume change.
- **Icons:** no crash after changing several icons in one visit; PICO-8, TI-83 and the "xtreme" systems get the icons of a pack; applying a pack or a theme with icons can no longer leave empty `config.json` files after a power cut.
- No crash in an empty Tweaks menu ("Edit app icon" for Button Lab, "Edit expert icon" with no Expert system).
- No crash at 70% battery with a theme that has no full-battery icon of its own.
- **File sharing (Samba):** a `smb.conf` edited on a PC (Windows line ends, indented shares) is read correctly, and switching one share no longer switches the whole SD card share.
- **Backup app:** a restore puts switched-off Tweaks options back as they were.
- **Package Manager:** a removed package stays removed when one of its folders still holds a file of yours (a BIOS, PICO-8's binaries).
- **Random Game** starts the game it shows on a new card or profile.
- An app installed after the boot can be started with the X/Y shortcut.
- **Tweaks › Tools › Make CUE files** and the M3U tools make one CUE per multi-track game again (every track became a game of its own on the console).
- **Scraper:** "Delete media of deleted games" keeps the covers and media of games hidden with "Hide game".
- **System log snapshot:** no Wi-Fi names or passwords left in the archive, and the network services are shown as they really are.

### 🌐 Netplay

- **MENU during the set-up** of an Easy Netplay session no longer leaves the hotspot and an open FTP server running: Wi-Fi comes back.
- The CPU goes back to its normal speed after the Netplay menu.
- With RetroArch's "Save configuration on exit" on, a session no longer turns off auto save state for every game.
- **Easy Netplay:** consoles without Netplay (NDS, PS, PICO, ports, ScummVM) say so instead of starting a hotspot; the host's files are served read-only; the joining console only accepts a core and a game from the host; the SNES CPU speed is used on both consoles.
- **Pokémon trade (GB/GBC):** each player has 5 minutes to close the game on their console, and a save of the session is never deleted before it is returned. A TGB Dual save of the joining console is kept.
- **Pokémon trade (GBA):** a session that never started no longer hides the game's auto save state, and the game is no longer switched to mGBA afterwards.

### 👶 Kids Mode (Veuks)

- Help on three pages (X on the profile screen), a reminder of how to leave Kids Mode when it starts, notes on SELECT, and its own icon.
- "Time's up" also ends DraStic and ports; a double press of MENU no longer runs Quick switch; the play timer survives updates; the parent's saves come back even if the Kids Mode app was removed; the child's game no longer becomes the parent's game cover.

## 🔧 Under the hood

- The tests now run on every push, with the console's BusyBox and the real SQLite, and test the real code instead of copies of it.

## ⚠️ Known issues

- **SD card check after an OTA update:** the OTA still restarts the console without unmounting the card, so you may see a one-time "SD card check" notice. Checking the card on a PC clears it. The clean restart (now used by the installer) will come to the OTA after the testers have tried it.
- **Easy Netplay:** the hotspot has the same password on every console; during a session anyone nearby who knows it can join. Use it with people you know.
- **Low-battery icon in MainUI:** the red warning icon is not shown while MainUI runs, so it doesn't cover MainUI's own battery icon.
- **Long transfers:** with nobody touching the console, a long FTP, HTTP, Samba or Syncthing transfer can be cut by the sleep timer (Settings › Sleep timer). Press a button now and then, or keep the device charging.
- **Tweaks** is in English in every language.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package.

Please report RedOnion issues on the [RedOnion tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (Tweaks › About, or `.tmp_update/onionVersion/version.txt`) and the steps to reproduce. Turning on Tweaks › Advanced › Diagnostics › Enable logging and sending `.tmp_update/logs` helps a lot.

---

## 🙏 Thanks

Thanks to the Onion team and the community, whose work RedOnion sits on. Thanks to @robcodedev for Open MainUI and many fixes; to ksielick for the Battery Monitor; to Veuks for Kids Mode; and to the RedOnion testers Machiavellian, Veuks and zazzagone.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource
- Expected behaviour is still the official guide: https://onionui.github.io/docs
