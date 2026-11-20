<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅🔴 RedOnion 4.5.1 beta 2: Battery Monitor 2.3.2, low-battery save and quit, standby fix, new scraper options, RedOnion everywhere

> ⚠️ **Always back up your SD card before updating!** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.

> 🧪 **Beta.** This build is offered only to devices on the **beta** OTA channel. Devices on the **stable** channel stay on 4.5.0. It was tried by the RedOnion testers first, but it is still a beta: please report what you find.

RedOnion is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

This beta uses a **release candidate of Open MainUI 1.1.1** by @robcodedev, as a prebuilt binary. Its source will be published with 1.1.1, before a stable RedOnion uses it. Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource

## 🆕 New in 4.5.1 beta 2

### 🔋 Battery Monitor 2.3.2 (thanks ksielick) — Mini+ and Mini Flip

- Updated to ksielick's v2.3.2, keeping RedOnion's graph and Shutdown Voltage fixes. "Set current capacity to 100%" is refused while charging or below 4.00 V, saved values are read back to confirm them, and the battery test restores the CPU speed and brightness when it ends. On the Mini+ the test has an extra 1800 MHz stage.
- **Battery Capacity** opens on the PMIC's real value, a learned one included (e.g. 3036 mAh). LEFT/RIGHT change it in 50 mAh steps (1500–4000), and A on an unchanged value writes nothing ("Capacity unchanged").
- **Shutdown Voltage** above 2.9 V is shown as "(risky)" in orange and asks for confirmation.
- **Capacity learning** can't be turned on before a capacity is saved ("Save Battery Capacity first"). Step 2 now says: discharge to 1%, then charge with the console on.
- Clearer messages for "Set current capacity to 100%": "I2C error: PMIC not read" and "Charging: counter left as it is".
- No more huge times (like 35000 h) in SESSION, CHARGE or OFF/SLEEP after a clock change or a boot without internet.

### 🪫 Low battery and standby

- **Low battery: save and quit** (Tweaks › System) now goes up to 20% (Off, 1–5, 10, 15, 20%). It never triggers on the charger or without a battery reading, and it only closes a running game, never the game list or an app. After it, the console powers off only if the battery is still low and not charging.
- **Standby powers off after its timeout**, counted from the start. Before, a Mini+ could stay on with the green LED for hours. With Tweaks › Advanced › Diagnostics › Enable logging, standby writes to `logs/keymon.log`.
- **Mini Flip:** power off takes priority over closing the lid, and opening the lid wakes the console even while charging.

### 🖼️ Scraper

- New: **Scrape all favorites** (in the Favorites tab), **Scrape all consoles**, and **Delete cover and media** for the selected game (press Y on a game › Scraper).
- **Delete media of deleted games** works from the main menu, keeps covers shared by MAME 2003/2000 sets and media named in `gamelist.xml`/`miyoogamelist.xml`, and skips (and tells you about) consoles it can't read.
- The menu shows the commands for the selected game first, and it runs in its own terminal.

### 🖥️ Menus and screens

- **Text screens** (Scraper, OTA, PDF Reader, Video Player): one layout everywhere (position top left, title centred, keys bottom right), in RedOnion style: red titles and cursor, keys in red and actions in white. Only A confirms, and "Press A" waits for A only.
- **OTA:** the download shows an onion filling up in red, with the percentage and MB. MENU stops it. The percentage is counted in KB, so big files no longer overflow it.
- **Tweaks:** "Enlarge small images" for the Game Info gallery, and an "Arcade games" switch for Saved Games.
- **GameSwitcher:** with RetroAchievements hardcore on, a note says the game will restart from the beginning.
- **Play Activity:** a game resumed after a pause adds its time instead of counting a new play, and quickly opening and closing the GameSwitcher no longer loses play time (#340).
- **Kids Mode** (Veuks): "Controls & tips" on X in the parent menu, and RESTART next to PLAY with the alternative button layout too.

### 🧅 RedOnion everywhere

- New top bar (Silky RedOnion and the system skin), boot screen, installer slides and screens ("Welcome to the RedOnion installer!", "Updating RedOnion..."), Package Manager waiting screen and ThemeSwitcher artwork. No more "onionos" or "Onion V4" screens.
- Windows shows the SD card as **RedOnion-v&lt;version&gt;**.
- Console tiles are 12 px wider, so names like "Commodore" fit.

### 🖥️ Open MainUI 1.1.1 rc7 (@robcodedev)

- Up from rc3 in beta 1: the carousel animation always plays, themes can give the console carousel and list their own pictures, the Wi-Fi page says "Connecting..." and then the result, saved Wi-Fi networks are kept when Wi-Fi is turned on, changing the Display page quickly no longer ends in "Could not save system.json", empty save states are not resumed and a game's save states are offered only with its core installed, plus smaller fixes (Search, folders in a console, game counts after a refresh, PDF manuals).

## 🔧 Other fixes

- **Battery Monitor:** no white line flashing under the PMIC screen.
- **Settings:** keymon no longer rewrites `system.json` on every key while Open MainUI saves it.
- **Guest Mode:** a profile without a theme gets Silky RedOnion, not Onion's theme and boot logo.
- **Time sync:** the last attempt asks for the time even when the router blocks ping.

## ⚠️ Known issues

- **SD card check after an OTA update:** the OTA restarts the console without unmounting the card, so at the next boot you may see a one-time "SD card check" notice. Checking the card on a PC clears it. A clean restart from the OTA is planned.
- **Low-battery icon in MainUI:** the red warning icon is not shown while MainUI runs, so it doesn't cover MainUI's own battery icon.
- **Long transfers:** with nobody touching the console, a long FTP, HTTP, Samba or Syncthing transfer can be cut by the sleep timer (Settings › Sleep timer). Press a button now and then, or keep the device charging.
- **Tweaks** is in English in every language.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package.

Please report RedOnion issues on the [RedOnion tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (from `.tmp_update/onionVersion/version.txt`) and the steps to reproduce. Turning on Tweaks › Advanced › Diagnostics › Enable logging and sending `.tmp_update/logs` helps a lot.

---

## 🙏 Thanks

Thanks to the Onion team and the community, whose work RedOnion sits on. Thanks to @robcodedev for Open MainUI and many fixes; to ksielick for the Battery Monitor; to Veuks for Kids Mode; and to the RedOnion testers Machiavellian, Veuks and zazzagone. Several features in this beta come from your comments on beta 1.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource
- Expected behaviour is still the official guide: https://onionui.github.io/docs
