<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅🔴 RedOnion beta 10.1: Open MainUI 1.0.10, footer layout, GameSwitcher pictures for big games, PDF Reader exit

> 🧪 **Beta.** This build is published as a pre-release: only devices on the **beta** OTA channel receive it. Stable installs stay on the current stable release until this beta is promoted.

This beta brings **Open MainUI 1.0.10**, a **footer layout** with the hints in button order (B then A) that the whole system follows, **GameSwitcher pictures and save states for Neo Geo and other big games** (broken since beta 9), a **PDF Reader and Video Player that let you leave the file list again**, and the end of the **GameSwitcher crash in Game & Watch**.

RedOnion is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It is based on Onion `4.4.0-beta`, with Miyoo Mini Flip support ported from `4.5-dev`, and runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip. Open MainUI is now **1.0.10**: https://github.com/robcodedev/onionos-mainui-opensource

## 🆕 New in beta 10.1

### 🖥️ Open MainUI 1.0.10 by @robcodedev

- **ROMs with `$` in their name start**, such as *WarioWare, Inc. - Mega Microgame$!*. MainUI refused them with "Unsupported launch path" (thanks u/SpeeDy_GjiZa for reporting it).
- **Refresh all roms** goes on past a console without a ROM folder. It stopped with "Catalog unavailable / ROM folder is missing" (thanks u/Kylie_Ray for finding it).
- **Folder covers:** a folder in a game list shows `<image folder>/<folder name>.png`, at any depth (thanks @fengfrw for the suggestion).
- **`[BIOS]` files are hidden** in the consoles whose `config.json` has `"hidebios": 1`, as in stock: they were listed. An existing list changes after Refresh roms (thanks @nukefrenzy for the report).
- **Long SELECT menu labels** end with "..." instead of being cut mid-letter at the edge.
- **Search** works with `%` and `_`.
- **Game Info** shows the right console name, and Favorites and Recents show their cover art, also when several emulators share a ROM folder.
- **X and Y** can open a game's options (GLO) or add and remove it from Favorites.

Full list: https://github.com/robcodedev/onionos-mainui-opensource/blob/main/CHANGELOG.md

### 🎨 Footer layout

- **Tweaks › Appearance › Footer layout:** "Classic", as stock, or "B A, page left": the page number on the left and the hints in button order, B then A, on the right (thanks u/back-in-flac for pointing out the old order).
- **The whole system follows it:** the menus and game lists, Tweaks, the GameSwitcher and its menus, the Info panel and the prompts. Tweaks changes its own footer as soon as you choose.
- **GameSwitcher bar in Silky RedOnion:** with "B A", SELECT and START on the left and Y X B A on the right, in the order the buttons have on the device. Other themes keep their own bar.
- **Tweaks › Appearance › Long menu labels:** "Shorten with ..." (the default) or "Cut", as before.

### 🎮 Buttons

- **Tweaks › Button shortcuts › MainUI: X button and Y button:** both can now be **GLO** (the game's options) or **Toggle favorite** (add or remove the selected game from Favorites), besides an app or a tool.

### 🕹️ GameSwitcher

- **Pictures and save states for Neo Geo, arcade, PS1 and big GBA games:** the first time the GameSwitcher opens in a game, RetroArch reads the whole ROM before it answers, which takes a few seconds for a big zip. Since beta 9 the GameSwitcher stopped waiting after about 2 seconds: no picture of the game, its box art stretched instead, no Save in its menu and a Load that only went back to the game. It now waits for the answer (thanks u/dgruetter for the PS1 report).
- **No more crash in Game & Watch** and 11 other cores (error 139, #283): BK (Elektronika BK), Cannonball (OutRun), EasyRPG (RPG Maker), FreeChaF (Fairchild Channel F), Lutro, Mu (Palm OS), NeoCD (Neo Geo CD), REminiscence (Flashback), Theodore (Thomson), VeMUlator (Dreamcast VMU) and X1 (Sharp X1). Their core info had no system id, and RetroArch crashed when the GameSwitcher asked what was running. Existing installs get the fix during the update (thanks @quackwalks for reporting it).
- **Cleaner RetroArch shutdown** when the GameSwitcher doesn't know its process: one signal instead of up to 999 (#295, thanks @coorasse for the pull request).

### 📄 PDF Reader and Video Player

- **START and MENU leave the file list again:** after a file had been opened once, START in the list opened the last file again instead of leaving, and the only way out was to power off (thanks u/Ok_Maintenance_6978 for reporting it).
- **The PDF Reader says how to close a PDF:** "A: open, START: exit, MENU: close a PDF."

### 📶 Wi-Fi and updates

- **Fast file transfers with Wi-Fi power save:** since beta 10 power saving was on all the time, and FTP transfers stuttered and new files took a minute to show on the other side. It now turns off while FTP, SSH/SFTP, the HTTP file server, Samba or Telnet is on in Tweaks › Network, and back on when you turn them all off, or while a game pauses them ("Disable services in game") (thanks u/Professional-Gold217 for reporting it).
- **The updater's internet check no longer depends on the clock:** it checked github.com's certificate, and on a device whose clock was wrong (no clock battery, or before the time sync) it stopped with "https://github.com not reachable" on a working network. It now skips the certificate, as the download already did, and when it still fails it says why (thanks u/Professional-Gold217 for the report).

### 🔐 SSH

- **The SSH host key survives updates** (#292): every update made a new key, and SSH and SFTP clients warned that the host key had changed. Starting with this update the key is kept (thanks @Ziko577 for reporting it).

### 🔊 Mini Flip

- **Volume follows the stock firmware's curve**, up to -3 dB: the loudest steps no longer crackle, and the start-up volume matches (#293, thanks @coorasse for the pull request).

### 🎨 Silky RedOnion and languages

- **SELECT menu wide enough for every label** in every language (385 px instead of 320).
- **Shorter "Add to favorites"** in German, Greek, Dutch, Portuguese, Brazilian Portuguese, Russian and Okinawan: each now uses its own "Add favorite", which matches "Remove favorite" and the GameSwitcher. Portuguese "Refresh roms" is "Atualizar ROMs", as in Brazilian Portuguese.

### 🛠️ Under the hood

- **Long ROM paths** (deep folders, long or CJK names) no longer overflow a buffer in screenshots and Play Activity.
- **OTA:** the version is read correctly whatever files are in the folder, a hand-made build keeps its date (no older stable offered to it), and fewer stray messages when offline.
- **Tests:** the PDF Reader and Video Player are now tested by pressing the keys in a terminal, plus the GameSwitcher's wait for RetroArch, the footer layout, the button shortcuts and the label widths. Tests that could not fail were dropped (#296, thanks @coorasse for the pull request).

## 🔧 In beta 10

- **Open MainUI 1.0.9**, with saved Wi-Fi networks.
- **Longer battery with Wi-Fi connected:** Wi-Fi power save is on, and off during Netplay.
- **List icons fit after installing a theme**, the **power button** no longer opens the arcade diagnostic menu, and the **RedOnion boot screen**.
- **OnionPlus is now RedOnion**, a name by @dredvm.

## ⚠️ Known issues

- **Mini Flip display colours** (Luminance, Hue, Saturation, Contrast) can be back at their defaults after a reboot: Open MainUI sets them only from Settings › Display, as stock does, and RedOnion doesn't apply them at boot yet.
- **Mini Flip charging:** a Flip that is charging doesn't wake up when the lid is opened. Use the power button for now.
- **Arabic:** lines stay left-aligned like every other language, and text drawn one letter at a time (the on-screen keyboard) shows Arabic letters unjoined.
- **Tweaks** is in English in every language.
- **"Refresh all roms" with other themes:** in 12 languages it is longer than a 320 px SELECT menu, and ends with "...". Silky RedOnion fits it.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package. Verifying certificates and the package's SHA-256 is planned.

## 🧪 If you'd like to try it

Switch the OTA updater to the **beta** channel to receive this build. Useful things to check:

- Metal Slug X or another big arcade zip, or a PS1 game: press MENU in game, and the GameSwitcher should show the game's own picture and offer Save
- A Game & Watch game: press MENU, and RetroArch should not close
- PDF Reader: open a PDF, close it with MENU, press START in the list: back to the main menu
- Tweaks › Appearance › Footer layout › "B A, page left": the menus, Tweaks and the GameSwitcher should all change, and "Classic" should bring them back
- Tweaks › Button shortcuts: set X or Y to Toggle favorite, then to GLO, and try them on a game
- A ROM with `$` in its name, and Refresh all roms with a console that has no ROM folder
- SSH: after this update, the next one should not warn about a changed host key
- Mini Flip: volume at the highest steps
- FTP or the web file manager with a large file: no stutter, and new files show at once; with every network service off, battery as in beta 10
- A console with `[BIOS]` files (hidebios): after Refresh roms they are gone from the list

Please report RedOnion issues on the [RedOnion tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (from `.tmp_update/onionVersion/version.txt`) and the steps to reproduce.

---

## 🙏 Thanks

Thanks first to the Onion team and to the community. RedOnion sits on their work: the OS and the years of fixes already in the tree. None of this exists without that.

Thanks to @robcodedev for Open MainUI 1.0.10 and the footer layout, X and Y and long label options in it, to @dredvm for the RedOnion name, the Silky RedOnion theme artwork and the boot logo, to @coorasse for the pull requests (the Mini Flip volume curve, the GameSwitcher shutdown fix and the test cleanup), to @quackwalks for the Game & Watch crash report, to @Ziko577 for the SSH host key report, to @nukefrenzy for the hidebios report and the testing, to @fengfrw for the folder covers idea, and to DiMo + fireball33x for the original Silky theme.

Thanks to the Reddit users who found bugs and asked for improvements: u/Ok_Maintenance_6978 (PDF Reader exit), u/Kylie_Ray (Refresh all roms), u/back-in-flac (footer layout), u/SpeeDy_GjiZa (`$` in ROM names), u/Professional-Gold217 (FTP stutter with Wi-Fi power save, the updater check) and u/dgruetter (GameSwitcher save states in PS1 games). And thanks to u/Mugen42, u/BetterLink1328, u/Old-Macaroon-4563, u/icystandards, u/Kooky-Minimum-2009, u/mariusmym, u/Smb158 and u/expera for testing, feedback and support.

## 💰 Bounty

The **$20 bounty** is still open through **31 October 2026**. It goes to whoever reports the most verified bugs and improvements in that window.

The score is the sum of integrated bug reports and pull requests on both RedOnion and Open MainUI: issues closed as fixed and pull requests merged, on [Amiga500/Onion](https://github.com/Amiga500/Onion) and [robcodedev/onionos-mainui-opensource](https://github.com/robcodedev/onionos-mainui-opensource). Reports that are still open or were not accepted don't count yet. Reports opened by the maintainers on someone's behalf count for the person credited in them.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Expected behaviour is still the official guide: https://onionui.github.io/docs

> ⚠️ **Back up your SD card before updating.** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.
