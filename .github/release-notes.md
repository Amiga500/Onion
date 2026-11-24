<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅🔴 RedOnion 4.5.1 beta 5 (tester): Open MainUI rc9, two new apps by robcodedev, a faster game menu, the Flip lid pauses videos

> ⚠️ **Always back up your SD card before updating!** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.

> 🧪 **Testers only.** This build is for the RedOnion testers (testers OTA channel). Devices on the **beta** and **stable** channels are not offered it. Please report what you find.

RedOnion is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

This beta brings the **release candidate 9 of Open MainUI 1.1.1** by @robcodedev, as a prebuilt binary, and two new apps of his. Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource

## 🆕 New in 4.5.1 beta 5

### 📦 Apps by robcodedev (Package Manager › APPS)

- **New: SD Card Doctor 0.6.3** by @robcodedev: card and FAT32 information, quick and deep read scans, a speed test, folder sizes, a read-only FAT check, the card's identity and a reviewed cleanup of macOS leftovers (`.DS_Store`, `._*`…), moved to a quarantine you can restore from. It never formats or repairs the card. The write part of the speed test asks first, and needs 25 % battery and 160 MiB free.
- **New: AudioTune 0.2.9** by @robcodedev: tone, stereo and noise tests, a five-band EQ (250 Hz and 8 kHz as low/high shelves, 500 Hz, 1 kHz and 4 kHz) with presets (Flat, Presence, Full, Clear, Warm, Bright, Custom), and music to try it on (its demo, or your own `sample.mp3` next to its `launch.sh`, played at 48 kHz). **L2** picks the output you are listening on, SPEAKER or HEADPHONES (SPEAKER protects the small speaker from strong bass), **R2** turns on a soft limiter (EQ page) or switches SAFE/PUNCHY headroom with headphones (Sound Profiles page). On Sound Profiles, moving over a preset lets you hear it while the music plays; **A** applies it. On the firmware it was checked against, it can also carry the EQ into games when you leave the app (beta: when the firmware is different, the normal audio is left as it is). Start at a low volume.
- **DisplayCal 0.8.4** by @robcodedev: on the Display page, Lumination starts from and resets to (X, Y) your model's default when `system.json` has none: 7 on the Mini and Mini v4, 10 on the Mini+ and Mini Flip (it was 10 on every model).

### 🎬 Video Player (Mini Flip lid)

- **Closing the lid, or pressing POWER, pauses the video again** (#350). The screen turned off but the video and its sound went on: the Video Player kept the console "awake" so that the sleep timer would not stop a long video, and that also kept it from pausing in standby. Now the sleep timer still leaves a playing video alone, and the lid and POWER pause it, on every model. The same for the MAME 2003-Plus save state tutorial video. Thanks to QuackWalks for the report and the test.
- The Music Player is unchanged: music goes on with the lid closed.

### ⚡ Faster (by robcodedev)

- **The game options menu (Y on a game) opens faster**: each script entry is read once instead of through a dozen small programs. A console whose name has a `/` or a `&` (for example "Sega CD / Mega-CD") no longer gets empty script entries.
- **Video Player and PDF Reader file lists** open faster in big folders; an empty folder no longer shows a stray "/" row.
- **The core list** (Game core in the game options) is built faster the first time, and an interrupted build no longer leaves half a list.
- Console pictures are set up faster at install.

### 🕹️ New console: GameTank (Package Manager › VERIFIED)

- **GameTank** (Clydeware - GameTank), the open-source 8-bit console by Clyde Shaffer: put `.gtr` games (also zipped) in `Roms/GAMETANK`, no BIOS needed. Core 1.0.3 by SundownerSport, a libretro port of Clyde Shaffer's GameTankEmulator, built for the Miyoo; icon by tenlevels. Games: https://gametank.zone/ It is marked experimental by its author.

### 🎮 Open MainUI 1.1.1 rc9

- Lumination, when `system.json` has none, is your model's default (7 on the Mini and Mini v4, 10 on the Mini+ and Mini Flip): it was 7 on every model.

### 📜 Game lists

- **Game Info text scrolls by itself**, so long descriptions (from the scraper, for example) can be read to the end: Tweaks › Game lists › Previews › Game Info text: auto scroll is now **on**, 2 s before it starts, 20 px/s. It is turned on once with this update, even if you had switched it off: switch it off again and it stays off. Thanks to Machiavellian.

### 🛠️ Tweaks

- **A "[SELECT] INFO" hint** at the bottom of the screen whenever the selected option has an explanation: SELECT has always opened it, but nothing said so. Thanks to Machiavellian. (Also hidden until now: **Y** puts the selected option back to the value it had when you opened Tweaks.)
- **Console page:** no more "Default" choices. Each one says what it does, and the ones that were the same as another are gone (Animation: Slide or None; Slide motion: Ease-out or Smooth; 180 ms, 70 %, Dimmed, 40 px, 12 px and Centred are now listed in their place). Thanks to Machiavellian.
- **Vertical position** of the carousel is now on the Console page itself (it was inside Carousel look): the one setting you may need when your consoles' pictures are taller or wider.
- **Reset system tweaks** resets only the settings: it no longer moves you back to the stable update channel (testers lost the testers channel), and it keeps the cores you chose per console, the applied theme and icons, the MainUI settings saved for each device and AudioTune's settings. Game Info auto scroll, like the other RedOnion defaults, is on again after it. **Reset MainUI settings** also resets this device's saved copy, so a power-off without a shutdown no longer brings the old settings back.

### 🔄 Updates (OTA)

- **The Update channel menu starts on the channel you chose last time** (it always started on stable, so pressing A straight away moved a tester to the stable channel).

### 🖥️ Display

- **Lumination default by model everywhere:** when `system.json` has no Lumination (or a broken one), the Mini+ and Mini Flip now get 10 (their default) and the Mini 7, at boot on the Flip, after a display reset, and in the programs that save your settings. Before, it was 7 everywhere, so a Flip could come up a little darker and keep it. Your own colour settings are not changed.

### 🗂️ Terminal and File Explorer

- **Terminal:** F7 on the on-screen keyboard sends F7 (from F7 on, every key sent the next one, so in Midnight Commander F7 "Mkdir" was "Delete" and F9 "Menu" was "Quit"). Font drawing stays inside the screen.
- **File Explorer:** copying or moving several files picks the right choice in the overwrite and error dialogs, and it no longer crashes on an image or a folder title it cannot draw.

## 🔧 Under the hood

- Terminal and File Explorer are now built from RedOnion's own forks, which already had these fixes.
- SD Card Doctor, AudioTune and DisplayCal are built from source. AudioTune's MP3 decoder (minimp3, CC0) is kept in the tree at the exact version its author checked, so the build needs no download.
- AudioTune's launcher writes its whole-line checks in a form the firmware's BusyBox understands (with `grep -x` the saved EQ would never have been found).
- Tests for the model default Lumination, the OTA menu starting on your channel, the Console page choices, the SELECT hint, the Game Info scroll, the Video Player's standby and the faster game options menu (compared with the old one); the testers' OTA from beta 4 to beta 5. The script tests also pass under BusyBox 1.20.2, the version in the firmware.

## ⚠️ Known issues

- **Kids Mode:** the progress bar of videos and audio is still violet with every theme. Kids Mode passes the theme's colour to its player; the player will use it from its next build.
- **AudioTune in games:** the EQ outside the app is a beta and needs the exact firmware audio library it was checked against; on other firmware only the app's own preview uses it. It is set up when you leave AudioTune: after a restart, open and close AudioTune once to have it in games again.
- **Game Info auto scroll** after a Reinstall on a card where it was switched off stays off: switch it on in Tweaks.
- **SD card check after an OTA update:** the OTA still restarts the console without unmounting the card, so you may see a one-time "SD card check" notice. Checking the card on a PC clears it.
- **Easy Netplay:** the hotspot has the same password on every console; during a session anyone nearby who knows it can join. Use it with people you know.
- **Low-battery icon in MainUI:** the red warning icon is not shown while MainUI runs, so it doesn't cover MainUI's own battery icon.
- **Long transfers:** with nobody touching the console, a long FTP, HTTP, Samba or Syncthing transfer can be cut by the sleep timer (Settings › Sleep timer). Press a button now and then, or keep the device charging.
- **Tweaks** is in English in every language.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package.

Please report RedOnion issues on the [RedOnion tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (Tweaks › About, or `.tmp_update/onionVersion/version.txt`) and the steps to reproduce. Turning on Tweaks › Advanced › Diagnostics › Enable logging and sending `.tmp_update/logs` helps a lot.

---

## 🙏 Thanks

Thanks to the Onion team and the community, whose work RedOnion sits on. Thanks to @robcodedev for Open MainUI, SD Card Doctor, AudioTune, DisplayCal, the faster scripts and many fixes; to ksielick for the Battery Monitor; to Veuks for Kids Mode; to KyleBing for the console pictures; to SundownerSport for the GameTank core, Clyde Shaffer for the GameTank and its emulator, and tenlevels for the GameTank icon; to QuackWalks for the Mini Flip lid report; and to the RedOnion testers Machiavellian, Veuks and zazzagone.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource
- Expected behaviour is still the official guide: https://onionui.github.io/docs
