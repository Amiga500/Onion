<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅⚡ OnionPlus stable: Open MainUI 1.0.5

This stable release promotes beta 5. It replaces the stock MainUI with **Open MainUI 1.0.5** by @robcodedev, and adds the fixes made since the stable release of 7 October (`9e96c27c`). Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource

OnionPlus is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It is based on Onion `4.4.0-beta`, with Miyoo Mini Flip support ported from `4.5-dev`, and runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

> ⚠️ **Back up your SD card before updating.** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC. This release replaces the main launcher, and a backup is the quickest way back if anything goes wrong.

## 🆕 Open MainUI replaces the stock MainUI

An open-source (GPL-3.0) rewrite of Miyoo's closed-source MainUI, installed on all three models (Mini `283`, Mini+ `354`, Flip `285`), in both Expert and normal mode. It was tested through five betas on the Mini+, Mini v4 and Mini Flip.

- ⚡ **~270 KB** launcher instead of ~1.4 MB, idle CPU in menus **~5% → ~1%** (Mini+), scrolling long titles **~35% → ~6%**, box art scaled in the background.
- 🎮 Letter jump, configurable row count, auto-scrolling titles, gamelist details, custom context menus, configurable main menu, safe ROM deletion on FAT32 with recovery after a power cut, new **About device** screen.
- 🎛️ **Tweaks › Appearance › Game lists... and Main menu...:** rows (with theme list icons resized to fit), text size, title scrolling, button repeat speed, sorting and the favorite star; which sections the main menu shows (Show recents and Show expert move here) and which entries the Select menu has. From @robcodedev's MainUI patcher, adapted for OnionPlus.
- 🎮 **Buttons:** Y opens Game List Options, and the Menu long press (Onion's default Context menu) opens the context menu.
- 🔍 **Search with X** opens Games → Search with the results, as stock does.
- 🔒 **Delete ROM, Clear Recents and Shutdown** need a separate press of A to confirm, so a long press can no longer delete a ROM by accident.
- 📋 **Game lists:** consoles whose `miyoogamelist.xml` isn't strictly valid XML open normally; `Manuals`, empty folders and ScummVM data folders no longer show up as ROM folders (run Refresh roms on affected consoles).
- 🎨 Theme overrides from Tweaks are applied, and theme text with a font size of 0 is hidden, as in stock.
- 📂 Many fixes for Favorites, Recents and Favorites folders, including games listed twice and recovery from damaged files.
- 📶 Wi-Fi networks with spaces or special characters in the name or password.
- 🔁 ROM caches, Recents and Favorites are used as they are, and stay compatible with the stock MainUI.

### ↩️ Going back to the stock MainUI

The stock MainUI is kept on the card. To switch back without reinstalling, create an empty file named `DISABLED` in `.tmp_update/mainui-test/` on the SD card and restart. Delete it to switch back to Open MainUI. If the Open MainUI binary is missing, the stock MainUI starts on its own.

## 🔧 Fixes since the last stable release

- **Game List Options on the Mini Flip:** the network scripts (Netplay, Scraper) were hidden on the Flip, as if it had no Wi-Fi. They show on the Mini+ and the Flip now.
- **Scraper:** leaving the scraper started the selected game instead of going back to the game list, and MENU could close Game List Options together with the scraper's terminal. Both fixed.
- **Rom list filter:** after Refresh list, Game List Options kept showing "Clear filter" for a filter that was gone. It now shows "Filter list" again.
- **OTA with Wi-Fi off:** the updater stayed on a black screen when Wi-Fi was off. It now turns Wi-Fi on, waits up to 20 seconds, and otherwise asks you to turn it on in Settings.
- **OTA beta channel:** the beta channel takes the newest published build. It could offer an older one, because GitHub doesn't always list releases newest first.

## 💡 Tips

- **Scrolling titles:** long titles scroll after one second. Change the speed and delay, or turn it off, in **Tweaks › Appearance › Game lists...**.
- **GameSwitcher shows the box art:** the GameSwitcher saves a screenshot of a game the first time you open it while that game is running. Until then, opening it from the menu shows the game's box art, which for some systems is portrait.
- **RetroArch settings that don't stick (#224):** RetroArch doesn't save settings on exit, and cores like gpSP ship a core override with Keep Aspect Ratio on. Use **Quick Menu > Overrides > Save Core Overrides** to change it for good.
- **RetroArch global settings** (such as the language): change them from the **RetroArch** app in Apps, not while a game is running. During a game RetroArch uses a combined configuration that Onion restores on exit.
- **Switching from another build (#231):** format the card, or replace the `App` folder too, then bring back only Roms, Saves, BIOS and Screenshots. Leftover app files from other builds can stop apps from starting.

## ⚠️ Known issues

- **Themes (#255, #256):** with some themes the popup background images are drawn twice, and the music of some older themes plays slowed down. Both are reported to @robcodedev; switching to the stock MainUI (see above) avoids them in the meantime.
- **Refresh list** appears both in Game List Options and in the Select menu; the Game List Options entry will be removed.
- Some new Open MainUI labels are English-only for now.
- **Mini Flip charging:** a Flip that is charging doesn't wake up when the lid is opened. Use the power button for now.
- **Settings:** two programs saving settings at the same moment can corrupt the settings file, and a settings save that fails is not retried.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package. Verifying certificates and the package's SHA-256 is planned.

Please report OnionPlus issues on the [OnionPlus tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (from `.tmp_update/onionVersion/version.txt`) and the steps to reproduce. If a bug turns out to be in official Onion too, I'll pass it on with the fix.

---

## 🙏 Thanks

Thanks first to the Onion team and to the community. OnionPlus sits on their work: the OS and the years of fixes already in the tree. None of this exists without that.

Thanks to @robcodedev for Open MainUI, and to the testers who sent reports and helped confirm the fixes, especially @Zazzago, @Ziko577 and Veuks.

## 💰 Bounty

The **$20 bounty** is still open through **31 October 2026**. It goes to whoever reports the most verified bugs and improvements in that window.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Expected behaviour is still the official guide: https://onionui.github.io/docs
