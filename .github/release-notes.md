<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅🔴 RedOnion 4.5.1 beta 1: Kids Mode, Backup, Folder sync, new apps, Open MainUI 1.1.1 rc4

> ⚠️ **Always back up your SD card before updating!** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.

> 🧪 **Beta.** This build is offered only to devices on the **beta** OTA channel. Devices on the **stable** channel stay on 4.5.0. It was tried by the RedOnion testers first, but it is still a beta: please report what you find.

RedOnion is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

This beta uses a **release candidate of Open MainUI 1.1.1** by @robcodedev, as a prebuilt binary. Its source will be published with 1.1.1, before a stable RedOnion uses it. Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource

## 🆕 New in 4.5.1 beta 1

### 📦 New apps (Package Manager › Apps)

- **Kids Mode** by Veuks: games and videos in one protected launcher, with a PIN for the parent menu and a play timer. The parent's saves, RetroArch settings and keys are put back when Kids Mode is turned off. If the `.kidmode` file is deleted from a computer, RedOnion puts its own settings back at the next boot.
- **Backup (Saves and settings):** saves of every profile, RedOnion's settings, RetroArch's `retroarch.cfg`, DraStic, Favorites and Recents in one zip in `Backups/`, and back. A restore first makes a safety copy and checks there is room on the card.
- **Mixtape** by Keith Snyder: browse and install community ports and apps over Wi-Fi.
- **DisplayCal v0.8.3** by @robcodedev: red, green, blue and warmth of the screen, adjusted live and saved per device. Its calibration is applied at boot and when the Blue Light Filter turns off.
- **Button Lab v0.1.1** by @robcodedev: a controller test. Each button lights up while held, with press counts. MENU held for 3 s exits.

### 🔋 Battery Monitor (thanks ksielick)

- On the Mini+ and Mini Flip, **SELECT** opens a PMIC menu: live voltage, current and charge, a Coulomb counter (mAh), a battery health test, the shutdown voltage (not below 2.9 V) and a **charge limit** (Full, 4.20 V, or about 90%, 4.10 V).
- Sleep tracking, session and charging navigation, and a remaining time from your history.

### 🖥️ Open MainUI 1.1.1 rc4 (@robcodedev)

- Faster return from a game, and the screen you were on opens again after the GameSwitcher.
- A console's number of games no longer counts `~Filter` and `~Refresh roms`.
- A button pressed together with MENU (such as MENU+B for the blue light filter) no longer also acts in MainUI.
- If changing colours finds the display driver broken, MainUI asks, repairs it and starts again.
- Changing the Display page quickly no longer ends in "Could not save system.json".
- The Wi-Fi page says "Connecting..." while it joins a network, then "Connected" or why it could not.

### 🌐 Network and media

- **Folder sync (Syncthing):** Tweaks › Network › Folder sync. The first start downloads the official build in the background; it is paused during games. The web interface is on port 8384.
- **Scraper:** besides the cover, ScreenScraper can now fetch what Game Info shows: description, box art, 3D box, screenshots, and optionally video and manual. `gamelist.xml` is written every 25 descriptions and when scraping ends or is closed.
- **Lighter PDF manuals:** the first time a heavy scanned manual opens, a light copy is made next to it and the PDF Reader (Green) opens that. Tweaks › Game lists › Game Info menu › Lighter manuals turns it off.
- **Search** builds its results much faster (thanks @robcodedev).

### 🎮 Games and system

- **RetroArch 1.22.2-3:** CHD files made with FLAC (the `chdman` default) now work, so RetroAchievements no longer says "Hash Generation failed" for them. The battery level in the RetroArch menu updates by itself (@robcodedev).
- **Volume and brightness panel** (idea by lucas_mb): Tweaks › User interface › Volume/brightness indicator › Bottom or Top. Onion's edge bar stays the default.
- **Battery icon** levels are the same as stock MainUI and Open MainUI everywhere (5, 15, 45, 70%).
- **RedOnion OTA update** has its own icon.

## 🔧 Fixes

- **Mini Flip installer:** on some units the installer screen was drawn sheared (640x480 on the 752x560 panel). The installer now waits for the display driver.
- **Mini Flip:** display colours set at boot without waiting, and no display reset when MainUI is closed (thanks @robcodedev).
- **Boot:** the boot screen comes first, then the warm-up in the background (thanks @robcodedev).
- **Shutdown:** settings are saved to the SD card; a MainUI that comes back while shutting down is closed.
- **Blue Light Filter:** no second display init after a display repair.
- **GLO Set time** goes back to the game list, not into the game.
- **Kids Mode:** a profile switch stops if the parent's saves cannot be put back; no video starts once the play time is over; titles are cut on whole characters (accents, kana).

## ⚠️ Known issues

- **Long transfers:** with nobody touching the console, a long FTP, HTTP, Samba or Syncthing transfer can be cut by the sleep timer (Settings › Sleep timer). Press a button now and then, or keep the device charging.
- **Tweaks** is in English in every language.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package.

Please report RedOnion issues on the [RedOnion tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (from `.tmp_update/onionVersion/version.txt`) and the steps to reproduce. Turning on Tweaks › Advanced › Diagnostics › Enable logging and sending `.tmp_update/logs` helps a lot.

---

## 🙏 Thanks

Thanks to the Onion team and the community, whose work RedOnion sits on. Thanks to @robcodedev for Open MainUI, DisplayCal, Button Lab, the PDF Reader and many fixes; to Veuks for Kids Mode; to ksielick for the Battery Monitor; to Keith Snyder for Mixtape; to lucas_mb for the panel idea; and to the RedOnion testers.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource
- Expected behaviour is still the official guide: https://onionui.github.io/docs
