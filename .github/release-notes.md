<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅⚡ OnionPlus beta 7: Open MainUI 1.0.7, every label translated, CUE generator fixed

> 🧪 **Beta.** This build is published as a pre-release: only devices on the **beta** OTA channel receive it. Stable installs stay on the stable release of 9 October (`8c2d8c75`), which already has Open MainUI 1.0.5, until this beta is promoted.

This beta is the current stable release with **Open MainUI 1.0.7** by @robcodedev, its labels translated in every language, and a CUE generator that writes working files. Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource

OnionPlus is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It is based on Onion `4.4.0-beta`, with Miyoo Mini Flip support ported from `4.5-dev`, and runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

## 🆕 Open MainUI 1.0.7 (new in beta 7)

### 🎮 Buttons

- **Expert shortcut (robcodedev/onionos-mainui-opensource#22):** Onion's Expert app opens Expert, also when Expert is hidden on the main menu. Set as the X or Y shortcut in Tweaks, it only made the screen flash; started from Apps, it went back to Apps. Thanks to @fengfrw.
- **Y on a game** opens Game List Options every time when that is the Y shortcut in Tweaks (Onion's default). Shortly after boot or a game, the press could be missed and the game started instead. Y now acts when it is released, so an app set as the Y shortcut is no longer launched or added to Recents by mistake. With no Y shortcut, Y starts the game as A does.
- **Select and Start** act when they are released, as in stock, and not after a combo: while one of them is held, the other buttons are Onion's shortcuts (brightness, volume boost, CPU clock), and the context menu no longer stays open or a list reloads after them.
- On the Mini, a Menu long press just after Select with L2 or R2 opens the context menu again.

### 🎨 Screens

- **Startup tab:** the tab chosen in Tweaks (Recents, Favorites, Games, Expert or Apps) opens at boot, as with stock. Open MainUI always started on the main menu.
- **Hotspot:** while Onion's Wi-Fi hotspot runs, its icon takes the Wi-Fi slot in the header, as in stock.
- **Settings at start:** Open MainUI loads the settings in `system.json` (brightness, volumes, sleep timer, display colours) into Onion's live settings at start, as stock does. A setting missing from `system.json` takes stock's default: menu music 8 and sleep timer off.

### 📂 Lists

- A game's details from Favorites or Recents show its genre and description also when its system is not in the open grid, and line breaks in a genre no longer split the summary line.
- A `.bin` file is hidden beside its `.cue` only when that `.cue` is a real file. Run **Refresh roms** on a console where a `.bin` game had disappeared.

### 📶 Wi-Fi

- After turning Wi-Fi on in Settings, the network address comes within about a second of joining the network, instead of 10-20 seconds later when the join was slow. Open MainUI also waits for the Wi-Fi chip after powering it on and starts Wi-Fi in stock's order, which joins faster on a busy channel.
- **Tip:** if Wi-Fi often takes long to connect, set your router's 2.4 GHz network to channel 1, 6 or 11 with a 20 MHz width. The Miyoo's Wi-Fi only uses 2.4 GHz and WPA2.

## 🆕 Open MainUI 1.0.6 (since beta 6)

### 🎨 Themes and screens

- **Popup backgrounds (#255):** a theme whose context-menu background is a full-screen image (Super Onion Entertainment System Remix, RetroRama and others) shows it as it is, instead of repeating its top band. Thanks to QuackWalks.
- **Theme fonts:** console and main-menu labels drawn in a theme's own font keep that font's weight, so pixel fonts are no longer thickened. Thanks to QuackWalks.
- **Expert labels** use stock's font and size, so themes that hide the Games labels (such as ONION PS) show their Expert labels again, and the Expert grid matches stock pixel for pixel. Thanks to Zazzago.
- **Header and status bar:** the header logo (`miyoo-topbar.png`) is no longer cut off, the Wi-Fi icon sits in stock's slot left of the battery, and the battery percentage and page counter are where stock puts them. Thanks to QuackWalks.
- The screen behind messages and confirmations is darkened, as in stock, and the Apps list draws its fourth row in full.

### 🔊 Sound

- **Theme music (#256):** music recorded at 48 kHz, as in most themes, plays at its proper speed and pitch instead of about 8% slow. Music recorded at 44.1 kHz plays slightly fast, as with the stock MainUI. Thanks to QuackWalks.

### 🎮 Buttons

- **X acts as B** (back, close, cancel) on every screen, as in stock. With Onion's default button shortcut (Tweaks › Button shortcuts › MainUI: X button), X still opens the Search app when it is installed; Search is also in the Select menu. Thanks to Veuks.
- The button mapping in `system.json` (`keymap`) is applied at start, as stock does. An invalid value is ignored, so a typo cannot leave the buttons unusable.

### 📂 Lists

- **Apps order:** Apps stay in alphabetical order, as in earlier betas. To list them in the order of their folders on the SD card, as the stock MainUI does, delete the file `.appsort` in `.tmp_update/config`; updates don't create it again.
- A `.bin` file is no longer listed beside the `.cue` of the same name; the `.cue` starts the game. Run **Refresh roms** on consoles that show both.
- Consoles with an empty `extlist` no longer list files without an extension, such as `README`. Run **Refresh roms** on those consoles.

### ↩️ Going back to the stock MainUI

The stock MainUI is kept on the card. To switch back without reinstalling, create an empty file named `DISABLED` in `.tmp_update/mainui-test/` on the SD card and restart. Delete it to switch back to Open MainUI.

## 🔧 Since beta 5 (also in the stable release of 9 October)

- **Game List Options on the Mini Flip:** the network scripts (Netplay, Scraper) were hidden on the Flip. They show on the Mini+ and the Flip now.
- **Scraper:** leaving the scraper started the selected game instead of going back to the game list, and MENU could close Game List Options together with the scraper's terminal. Both fixed.
- **Rom list filter:** after Refresh list, Game List Options kept showing "Clear filter" for a filter that was gone. It now shows "Filter list" again.
- **OTA with Wi-Fi off:** the updater stayed on a black screen when Wi-Fi was off. It now turns Wi-Fi on, waits up to 20 seconds, and otherwise asks you to turn it on in Settings.
- **OTA beta channel:** the beta channel takes the newest published build. It could offer an older one, because GitHub doesn't always list releases newest first.

## 🔧 In beta 6

- **"Update available!" at every start (inherited from Onion):** the message stayed after the update was installed, and while it was shown the start-up check didn't run again. The installer now clears it, and so does the check when you're up to date.
- **OTA offering an older build:** with the same version number (4.4.0), any different build counted as an update, even an older one, so a beta could be offered the stable release. Builds of the same version are now compared by date, and an older one is never offered.

## 🆕 New in beta 7

### 🌐 Translations

- **Every language has every label:** the newer Open MainUI entries (the Favorites folder actions, Sort A-Z, Tweaks, and Model name, Max resolution and Onion version in About device) and the GameSwitcher's Add/Remove favorite were in English in every language but Traditional Chinese. All 33 languages have them now, kept short enough to fit the menus with the default theme (#274).
- **Chinese:** Simplified and Traditional Chinese updated by @fengfrw (#274, #275), including Exit, the save-state messages and the Open MainUI labels. In Simplified Chinese the Expert tab is now called RA复古.
- **Native speakers wanted:** the new labels in Uchinaguchi, Belarusian and Occitan haven't been checked by a native speaker. Corrections are welcome on the tracker.

### 💿 Generate CUE files (#264)

Tweaks › Tools › Generate CUE files, which the M3U generator also uses, wrote CUE files that didn't work: the paths in them pointed to files that don't exist, a game's CUE could land in the next game's folder, and every disc of a game went into one CUE. It now:

- writes one CUE per game, next to its files, with only the file name in each `FILE` line;
- gives each disc its own CUE, named as the M3U generator expects (`Game (USA) (Disc 1).cue`);
- keeps all the tracks of a game together, in number order (Track 10 after Track 2), also when the name has `[ ]` or a region after the track number;
- **never overwrites an existing CUE**, so the CUE files that came with your games are safe.

> If you used Generate CUE files before, delete the CUE files it made and run it again: it no longer replaces them. You can recognise them by a folder name in their `FILE` lines (for example `FILE "PS/...`). Don't delete CUE files that came with your games.

Thanks to @yuruyang for the report and the review.

### 🔧 Fixes

- **Brightness:** a brightness set in the GameSwitcher went back to the old value with the first button press in the main menu, also after exiting a game started from the GameSwitcher. Fixed in both places that kept the old value: keymon now takes the new settings as soon as the GameSwitcher or Tweaks save them, also during a game, and Open MainUI 1.0.7 loads them at start, as stock does. The display settings changed in Tweaks (contrast, hue, saturation, luminance), which could be undone the same way, are fixed too. Thanks to Zombiediplomat for the report.
- **OTA and Wi-Fi:** with Wi-Fi off in Settings, the updater turns it on for the update and now turns it off again when it closes; it used to stay on until the next restart. Opened just after start-up, while the network is still coming up in the background, it waits for that to finish first. With Wi-Fi on but still connecting, the updater waits for it instead of restarting it. Turning Wi-Fi on now waits for the Wi-Fi chip and for the network, up to about a minute: the 20 seconds it allowed before were often too short on the Mini Plus, and the updater then reported github.com unreachable.
- **Long messages cut off (robcodedev/onionos-mainui-opensource#21):** with a theme that uses a large font, long lines in message screens and popups (the gpSP save transfer question, a port's first-run notice) ran off the edge of the screen. The text now shrinks to fit. On the Mini v4 and Mini Flip the gpSP question is also centred. Thanks to QuackWalks.
- **OTA errors:** when GitHub doesn't answer (no connection, or too many checks in an hour), the updater says so instead of "Version is up to date", and the "Update available!" notice is kept. The downloaded package is deleted once extracted, instead of staying on the card. On the Mini, which has no Wi-Fi, the updater says so instead of waiting.

## ⚠️ Known issues

- **Refresh list** appears both in Game List Options and in the Select menu; the Game List Options entry will be removed.
- **Arabic and Bengali** labels show as boxes: no font Onion ships has their letters.
- **Mini Flip charging:** a Flip that is charging doesn't wake up when the lid is opened. Use the power button for now.
- **Settings:** two programs saving settings at the same moment can corrupt the settings file, and a settings save that fails is not retried.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package. Verifying certificates and the package's SHA-256 is planned.

## 🧪 If you'd like to try it

Switch the OTA updater to the **beta** channel to receive this build. Useful things to check:

- Themes with full-screen popup backgrounds, pixel fonts, wide Wi-Fi icons or a tall header logo
- Theme music speed with your usual themes
- X as back in menus, and X opening Search from a game list
- The X and Y shortcuts from Tweaks (Expert, Game List Options, apps) and the startup tab
- Turning Wi-Fi off and on in Settings: how long it takes to connect
- The labels in your language: the Select menu on a Favorites folder, Settings › About device and the GameSwitcher menu
- Tweaks › Tools › Generate CUE files on a multi-track or multi-disc game
- The Apps list in alphabetical order
- Consoles with `.cue` and `.bin` games after Refresh roms
- Message screens with a theme that uses a large font, such as the gpSP save transfer question

Please report OnionPlus issues on the [OnionPlus tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (from `.tmp_update/onionVersion/version.txt`) and the steps to reproduce.

---

## 🙏 Thanks

Thanks first to the Onion team and to the community. OnionPlus sits on their work: the OS and the years of fixes already in the tree. None of this exists without that.

Thanks to @robcodedev for Open MainUI, and to the testers who sent reports and helped confirm the fixes, especially QuackWalks, @Zazzago, @Ziko577, Veuks and Zombiediplomat. Thanks to @fengfrw for the Chinese translations and the Expert shortcut report and to @yuruyang for the CUE generator report and review.

## 💰 Bounty

The **$20 bounty** is still open through **31 October 2026**. It goes to whoever reports the most verified bugs and improvements in that window.

The score is the sum of integrated bug reports and pull requests on both OnionPlus and Open MainUI: issues closed as fixed and pull requests merged, on [Amiga500/Onion](https://github.com/Amiga500/Onion) and [robcodedev/onionos-mainui-opensource](https://github.com/robcodedev/onionos-mainui-opensource). Reports that are still open or were not accepted don't count yet. Reports opened by the maintainers on someone's behalf count for the person credited in them.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Expected behaviour is still the official guide: https://onionui.github.io/docs

> ⚠️ **Back up your SD card before updating.** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.
