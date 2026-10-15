<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅⚡ OnionPlus beta 8: the RedOnion look, Open MainUI 1.0.8, OTA fixes

> 🧪 **Beta.** This build is published as a pre-release: only devices on the **beta** OTA channel receive it. Stable installs stay on the stable release of 9 October (`8c2d8c75`) until this beta is promoted.

This beta brings the new **RedOnion** look, **Open MainUI 1.0.8** by @robcodedev, a choice of loading/saving screens and a round of OTA fixes. Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource

OnionPlus is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It is based on Onion `4.4.0-beta`, with Miyoo Mini Flip support ported from `4.5-dev`, and runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

## 🆕 New in beta 8

### 🎨 The RedOnion look

- **Silky RedOnion**, a crimson version of the Silky theme (original design by DiMo + fireball33x), is the default theme on new installs. Updates keep the theme you use: pick Silky RedOnion in Themes, with "Apply icons" on.
- **The look follows the theme:** with Silky RedOnion, the system screens and apps are crimson too: Package Manager, Quick Guide, Activity Tracker, the Search/Filter tools, the File Explorer icons, Tweaks' icons, the charging, brightness, boot, shutdown and saving screens, and the RetroArch, AdvanceMENU and PCSX menus. With Silky by DiMo or any other theme, they are Onion's purple as before. The colours change when you apply a theme; AdvanceMENU and RetroArch keep your other settings.
- **The installer** uses the RedOnion look with every theme. The File Explorer selection bar stays purple with every theme.
- **Search:** the matched letters in a game list search are crimson with Silky RedOnion and purple with other themes (Open MainUI 1.0.8, `gamelist.searchColor`).
- **Battery icon:** with Silky RedOnion the status bar battery is white; the empty and charging icons stay crimson.
- **Texts:** Settings › About shows "RedOnion version", in every language, and the Website row links to the project on GitHub. The OTA app is called "RedOnion OTA update".

### ⏳ Loading and saving screens

- **Tweaks › Appearance › Loading/saving screen** chooses what shows while a game loads or saves: **Popup** (the message over the game's screenshot, as before), **Full screen** (the screenshot without a message), **Black** or **Full popup** (the message on the theme's full-screen background). It applies when starting a game from the menu, when leaving with MENU and when resuming from the GameSwitcher.

### 🔄 OTA updater

- **Error 137 and black screen (inherited from Onion):** leaving the updater with START, or with a combo that includes it, killed it ("Error code: 137"), and a process left behind kept closing Open MainUI, leaving a black screen until a restart. Fixed: the updater closes cleanly.
- **Buttons:** every updater screen now uses Up/Down to move, **A** to confirm and **MENU** to exit, also right after moving the cursor. Right no longer confirms like A, and the other buttons do nothing. The version screen says "Press A to continue, MENU to exit".
- **File selector** (Video Player, PDF Reader): START closes it without an error, as before but cleanly.

### 🔧 Fixes

- **DraStic (Mini v4, Mini Flip):** leaving a DS game no longer shows a garbled screen for a moment.
- **RetroArch:** Settings › User Interface › Appearance › Border Filler is off, so no frame flashes around the menu when RetroArch opens. Set once on existing installs; you can turn it on again.
- **Package Manager:** the "applying changes" screen follows the theme like the rest of Package Manager.

## 🆕 Open MainUI 1.0.8

Full list: https://github.com/robcodedev/onionos-mainui-opensource/blob/main/CHANGELOG.md

### 🔋 Status bar

- **Charging on the Mini+ and Mini Flip:** the battery icon changes to the charging one while the charger is plugged in, and the percentage shows beside it. On the Mini only the icon shows, as Onion doesn't know the level while charging. The icon now changes within about 3 seconds of plugging in or out.
- The Wi-Fi icon moves beside the battery when the percentage would cover it, and the battery icon changes at the same levels as in stock.

### 🎮 Buttons and menus

- Y, Select or Start held while the device goes to sleep no longer act when released after waking.
- A on an empty list does nothing instead of "Launch unavailable", and Menu on an empty Favorites, Recents or Search list no longer reads a removed entry, which could crash.
- **Favorites:** deleting a folder removes everything in it, after asking first; a game can be moved out of a folder again by pasting it on the folder it came from.

### 🎨 Themes and screens

- Confirmations and messages use the theme's colours and popup art, as stock does.
- Settings › About names the version row "OS version" (shown as "RedOnion version" in OnionPlus) and can show a Website row.

### 📶 Wi-Fi

- Turning Wi-Fi off and quickly on again no longer leaves it on "Scanning..." with no networks.
- Settings › Wi-Fi says when a connection fails (for example a wrong password) and why Wi-Fi can't be turned on.

### ⚡ Speed

- Less work when scrolling lists and drawing dialogs and cover art, and the menu's audio buffer matches stock's with Onion's audio fix off.

## 🔧 In beta 7

- **Open MainUI 1.0.7:** startup tab, Expert shortcut, Y/Select/Start on release, hotspot icon, faster Wi-Fi address.
- **Translations:** every label in all 33 languages, Chinese updated by @fengfrw.
- **Generate CUE files** writes working CUE files and never overwrites existing ones.
- **Brightness:** a brightness set in the GameSwitcher went back to the old value with the first button press in the main menu. Fixed: keymon takes the new settings as soon as the GameSwitcher or Tweaks save them, and Open MainUI loads them at start.
- **OTA:** turns Wi-Fi off again when it closes, waits for the network, and reports GitHub errors instead of "Version is up to date".

## ⚠️ Known issues

- **Mini Flip display colours** (Luminance, Hue, Saturation, Contrast) can be back at their defaults after a reboot: Open MainUI sets them only from Settings › Display, as stock does, and OnionPlus doesn't apply them at boot yet.
- **Refresh list** appears both in Game List Options and in the Select menu; the Game List Options entry will be removed.
- **Arabic and Bengali** labels show as boxes: no font Onion ships has their letters.
- **Mini Flip charging:** a Flip that is charging doesn't wake up when the lid is opened. Use the power button for now.
- **Settings:** two programs saving settings at the same moment can corrupt the settings file, and a settings save that fails is not retried.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package. Verifying certificates and the package's SHA-256 is planned.

## 🧪 If you'd like to try it

Switch the OTA updater to the **beta** channel to receive this build. Useful things to check:

- Silky RedOnion and Silky by DiMo, switching between them: Package Manager, Quick Guide, Activity Tracker, Search tools, Tweaks icons, charging and brightness screens
- Tweaks › Appearance › Loading/saving screen, each option: starting a game, leaving with MENU, resuming from the GameSwitcher
- The OTA updater: every screen with Up/Down, A and MENU
- Charging on the Mini+ and Mini Flip: icon and percentage
- Leaving a DS game on the Mini v4 and Mini Flip
- Opening RetroArch from Apps: no frame around the menu

Please report OnionPlus issues on the [OnionPlus tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (from `.tmp_update/onionVersion/version.txt`) and the steps to reproduce.

---

## 🙏 Thanks

Thanks first to the Onion team and to the community. OnionPlus sits on their work: the OS and the years of fixes already in the tree. None of this exists without that.

Thanks to @robcodedev for Open MainUI, to the contributor of the RedOnion artwork and to DiMo + fireball33x for the original Silky theme, and to the testers who sent reports and helped confirm the fixes, especially QuackWalks, @Zazzago, @Ziko577, Veuks and Zombiediplomat. Thanks to @fengfrw for the Chinese translations and to @yuruyang for the CUE generator report and review.

## 💰 Bounty

The **$20 bounty** is still open through **31 October 2026**. It goes to whoever reports the most verified bugs and improvements in that window.

The score is the sum of integrated bug reports and pull requests on both OnionPlus and Open MainUI: issues closed as fixed and pull requests merged, on [Amiga500/Onion](https://github.com/Amiga500/Onion) and [robcodedev/onionos-mainui-opensource](https://github.com/robcodedev/onionos-mainui-opensource). Reports that are still open or were not accepted don't count yet. Reports opened by the maintainers on someone's behalf count for the person credited in them.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Expected behaviour is still the official guide: https://onionui.github.io/docs

> ⚠️ **Back up your SD card before updating.** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.
