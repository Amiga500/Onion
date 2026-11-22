<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅🔴 RedOnion 4.5.1 beta 4: settings kept on update, Open MainUI rc8, an X button that matches your theme

> ⚠️ **Always back up your SD card before updating!** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.

> 🧪 **Testers only.** This build is for the RedOnion testers (testers OTA channel). Devices on the **beta** and **stable** channels are not offered it. Please report what you find.

RedOnion is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

This beta brings the **release candidate 8 of Open MainUI 1.1.1** by @robcodedev, as a prebuilt binary. Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource

Beta 4 only fixes what the testers found in beta 3. New features come after it.

## 🆕 New in 4.5.1 beta 4

### 🛟 Updating

- **Your settings stay after an update.** Every update to beta 3 put brightness, volume, menu sound and the display colours back to their defaults. The fix is in the installer of the version being installed, so updating from beta 3 to beta 4 already keeps them. Thanks to @robcodedev for finding it.

### 🎮 Open MainUI 1.1.1 rc8

- **RIGHT on a game in a list can open its gallery** straight away, instead of Game Info: Tweaks › Game lists › Game Info menu › **RIGHT in a list: gallery**. It is off by default and needs "RIGHT opens the gallery" on (it is greyed out while that is off). Closes #349.

### 💾 SD card

- **A card that has become read-only** (the system does that after a card or FAT error) no longer reopens the last app or game over and over with nothing saved. RedOnion now says so, explains how to check the card on a PC (Windows: Properties › Tools › Check; Mac: Disk Utility › First Aid) and turns the console off.

### 🎨 Themes

- **The X button matches your theme** in the GameSwitcher (Load › DELETE) and in Kids Mode (RESTART, HELP), and it is the same in both. A theme's own `skin/icon-X-54.png` (or `skin/extra/icon-X-54.png`) comes first, so theme makers can add theirs. Otherwise RedOnion makes one from the theme's B button: same shape and colours, with an X where the B is. For a B button it can't use (a label, a drawing) the plain X stays.
- **Kids Mode** no longer shows an "A" under its X.
- Themes whose B button is empty (the hints are drawn in the footer picture) still show no X in the GameSwitcher; Kids Mode shows the plain X there, so RESTART and HELP keep a button.

### 🥚 And

- The easter egg starts on the first try of the code (an UP pressed just before it made it miss).

## 🔧 Under the hood

- Tests for the X button maker (drawn buttons: a letter, a cut-out letter, a ring, things that are not buttons) and for the testers' OTA from beta 3 to beta 4.

## ⚠️ Known issues

- **Kids Mode:** the progress bar of videos and audio is still violet with every theme. Kids Mode now passes the theme's colour to its player; the player will use it from its next build.
- **SD card check after an OTA update:** the OTA still restarts the console without unmounting the card, so you may see a one-time "SD card check" notice. Checking the card on a PC clears it. The clean restart (already used by the installer) will come to the OTA after the testers have tried it.
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
