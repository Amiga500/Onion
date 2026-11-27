<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅🔴 RedOnion 4.5.1 beta 6 (tester): the new look, six accent colours, RedAmp 1.2.1, back to the same screen after the GameSwitcher

> ⚠️ **Always back up your SD card before updating!** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.

> 🧪 **Testers only.** This build is for the RedOnion testers (testers OTA channel). Devices on the **beta** and **stable** channels are not offered it. Please report what you find.

RedOnion is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

This beta is the first test of RedOnion's **new look**, designed with @robcodedev: the Silky RedOnion theme and RedOnion's own screens redrawn, an accent colour to choose, and the updater, the scraper and several apps no longer in a terminal. Everything from beta 5 is still here; below is what changed since.

## 🆕 New in 4.5.1 beta 6

### 🎨 The new look (Silky RedOnion)

- **A cleaner style everywhere:** a darker grey background, the app's icon in the header, the selected row as a rounded pill, line icons in the menus and Settings, and confirmations as a box with two buttons (B cancels on the left, A acts on the right).
- **Accent colour** (Tweaks › Look and feel › Accent colour): red, blue, purple, teal, pink or orange, for the selection, the buttons, the menu icons and the bars, in MainUI and RedOnion's own apps, and for the "Red" of the RedOnion logo at the top left. Other themes keep their own colours.
- **Tweaks:** the selected option's explanation is shown in a box under the list, and **SELECT** opens it whole, in larger text. The header shows the path to the page you are on.
- **No more terminal** for the **OTA updater** (questions, messages and the download on screens of their own), the **scraper** (menus, messages and progress; the terminal opens only for its on-screen keyboard), and the file lists of the **PDF Reader** and the **Video Player** (a file you opened before shows "(seen)").
- **Redrawn:** Package Manager, Themes (every theme's preview is shown, sharper: some showed black before), Activity Tracker, Clock (a box for day, month, year, hour and minute; START saves, B leaves without saving), the Search keyboard, the Quick Guide (now about RedOnion), the installer, and Rob's SD Card Doctor, AudioTune, DisplayCal and Button Lab.
- **Battery Monitor** is now **RedOnion Battery Monitor**, still based on ksielick's Battery Monitor (credited on its waiting screen).
- **Installation:** 20 tips, in a new order each time, also about RedOnion's own apps, and the status line centred above the progress bar.
- **LOADING and SAVING** over the game's picture: the word is centred in its box.
- **RedAmp, MiniCinema and Mixtape** keep their own look.

### 🎮 GameSwitcher and MainUI

- **Back from the GameSwitcher, MainUI is where you left it:** the same screen and list position, also when you resume your last game. Thanks to robcodedev.
- **Silky RedOnion:** the selected row of a context menu is rounded at both ends (it was cut off on the right). Thanks to robcodedev.
- **PICO-8:** the logo on the console's icon and picture redrawn sharp.

### 🎵 RedAmp 1.2.1 (by robcodedev)

- **Saved playlists and favorites**: make, rename and delete playlists, mark songs with a heart, and **search** the library by title, artist, album or file name. The play queue can drop a song, be cleared or be saved as a playlist; playing one song from the library no longer clears it.
- **New interface**: four screens, Library, Now Playing, Queue and Settings, that SELECT moves between. It opens in the new modern look; the classic Winamp window is in Settings › Display & Skins › Playing appearance. No feature needs a long press any more: Y opens the actions for the selected item, X marks a favorite, and the equalizer and the full-screen visualizer are in Now Playing's Y menu.

### 🧒 Kids Mode

- **The parent menu in five rows:** Switch to Main/Guest profile, Add play time, Carousel settings, **Other options**, Exit Kids Mode. Other options holds the switches you set once: Save/load states, **Reminder** (the note on how to leave Kids Mode, shown before it starts: ON by default, it can now be turned off) and the new **Video overlay**: Off (the default), LCD grid or Scanlines, over videos only (not games or music). Thanks to Veuks and zazzagone.
- **Controls & tips** in the theme's colours (it was white, unreadable on a light theme), and no hint labels over the footer art of themes that hide them (NanoSwitch). Thanks to zazzagone.

### 🔍 Scraper

- **Mini Flip: no more freeze while scraping box art from LaunchBox.** Box art was shrunk with the graphics hardware without checking that video memory was there: after a few minutes of scraping, a Flip could freeze (screen stuck, the console hot). Box art is now shrunk in software, and pngScale checks video memory before using it. A picture that cannot be converted is skipped, and one cut by MENU is no longer kept as the game's picture: that game is scraped again next time. Thanks to ToastedLion for the report and AP for testing the fix.
- **A warning before scraping a whole console, all favorites or all consoles:** long sessions heat the device and drain the battery (on a PC it is faster), and if it ever freezes, hold POWER for at least 10 seconds until it turns off.

### 🔋 Battery

- **RedOnion Battery Monitor with ksielick's 2.3.3 and 2.3.4:** a clearer first-time setup on the Mini+ and Mini Flip (Battery Capacity shows **Not saved** until you save it, and **[A] Save & Enable** saves it before turning capacity learning on); a sleep that ran the battery flat shows what it used ("SLEEP 10h05 -98%") instead of "-0%"; a charge across a clock correction from Wi-Fi no longer shows a huge time. Thanks to robcodedev for the report.
- **The red low-battery icon goes away as soon as you plug in the charger** (Mini+ and Mini Flip): their level stays low for a while after the charger is connected, and the icon stayed until it rose above the warning level. Thanks to robcodedev.

### 💾 Backup and the SD card

- **Backup** shows a progress bar while it backs up and restores, with the file it is on. Thanks to robcodedev.
- **SD card notice at start-up** (after a power-off that did not unmount the card): it now asks, with **Check it now (SD Card Doctor)**, **How to repair it on a PC** (Windows, Mac and Linux) and **OK**. **SD Card Doctor is now installed by default** for that.

### 🔄 Updates

- **An OTA update keeps the settings you changed since the console was last switched off** (brightness, volume, display colours…): they came back as they were at that switch-off.
- **The installation screens use your brightness** instead of nearly full brightness. Thanks to robcodedev.

## 🔧 Under the hood

- When the card cannot be unmounted at power-off, the shutdown writes what still holds it to `/appconfigs/shutdown_busy.log` (in the console's own memory), to find out why the SD card check notice sometimes shows after an update.
- Button Lab's source (v0.1.1, by robcodedev) is kept in the tree.
- Tests for the accent colours, the SD card notice, the low-battery icon while charging, the MainUI stop for the GameSwitcher, the installer's brightness and the settings kept by an OTA update, Kids Mode's video overlay choice and Backup's progress. The script tests also pass under BusyBox 1.20.2, the version in the firmware.

## ⚠️ Known issues

- **RedAmp, "Music in games":** it needs RedOnion to start games and MainUI through RedAmp's audio hook, which is not in this build: leave it **off**. Turned on, the music does not reach games or MainUI, and its background player keeps the console from suspending games on POWER.
- **RedAmp, Play during standby:** a standby started while music plays has no automatic power-off, also after the music ends. Wake the console, or switch it off, when you are done.
- **MiniCinema after a power-off:** reopening the video you were watching at the next start is still experimental.
- **Kids Mode:** the progress bar of videos and audio is still violet with every theme, and the **Video overlay** does not show yet. Kids Mode passes both to its player; the player will use them from its next build.
- **AudioTune in games:** the EQ outside the app is a beta and needs the exact firmware audio library it was checked against; on other firmware only the app's own preview uses it. It is set up when you leave AudioTune: after a restart, open and close AudioTune once to have it in games again.
- **Game Info auto scroll** after a Reinstall on a card where it was switched off stays off: switch it on in Tweaks.
- **SD card check after an OTA update:** the OTA still restarts the console without unmounting the card, so you may see a one-time "SD card check" notice. Checking the card on a PC clears it. If you see it after an update, please send `/appconfigs/shutdown_busy.log` (see below).
- **Easy Netplay:** the hotspot has the same password on every console; during a session anyone nearby who knows it can join. Use it with people you know.
- **Low-battery icon in MainUI:** the red warning icon is not shown while MainUI runs, so it doesn't cover MainUI's own battery icon.
- **Long transfers:** with nobody touching the console, a long FTP, HTTP, Samba or Syncthing transfer can be cut by the sleep timer (Settings › Sleep timer). Press a button now and then, or keep the device charging.
- **Tweaks** is in English in every language.
- **Other themes:** the new look is made for Silky RedOnion. With another theme, some Settings icons may be white where the theme has none of its own (Wi-Fi, Themes, Tweaks, Sleep).
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package.

Please report RedOnion issues on the [RedOnion tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (Tweaks › About, or `.tmp_update/onionVersion/version.txt`) and the steps to reproduce. Turning on Tweaks › Advanced › Diagnostics › Enable logging and sending `.tmp_update/logs` helps a lot.

---

## 🙏 Thanks

Thanks to the Onion team and the community, whose work RedOnion sits on. Thanks to @robcodedev for Open MainUI, the ideas and choices behind the new look, MiniCinema, RedAmp, SD Card Doctor, AudioTune, DisplayCal, the faster scripts and many fixes; to ksielick for the Battery Monitor; to Veuks for Kids Mode and MiniCinema; to KyleBing for the console pictures; to SundownerSport for the GameTank core, Clyde Shaffer for the GameTank and its emulator, and tenlevels for the GameTank icon; to QuackWalks for the Mini Flip lid report; to ToastedLion and AP for the Mini Flip scraper freeze; and to the RedOnion testers Machiavellian, Veuks and zazzagone.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource
- Expected behaviour is still the official guide: https://onionui.github.io/docs
