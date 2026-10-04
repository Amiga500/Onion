# 🧅⚡ OnionPlus beta: Open MainUI 1.0.3 instead of stock MainUI, plus bug fixes

> 🧪 **Beta.** This build is published as a pre-release: only devices on the
> **beta** OTA channel receive it. Stable installs stay on V4.4.0-beta-20260928
> until it is promoted.

This build replaces the stock MainUI with **Open MainUI 1.0.3** by @robcodedev.
Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource

OnionPlus is still a personal-use build. It is **not** a replacement for Onion and
**not** an official release. I use it on my own Miyoo Mini Plus. It is based on
Onion `4.4.0-beta`, with Miyoo Mini Flip support ported from `4.5-dev`, and runs on
the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

## 🎯 Why this build exists

Several of these bugs are also in official Onion. Fixes for them have sat around
for a long time without being merged, and no official corrective build has
shipped for them. OnionPlus is where I apply and test those corrections for
personal use. Reports, patches and concrete proposals stay public, so the Onion
team can take them if they want. The point is that the work is visible instead
of sitting unreleased.

---

## 🆕 Open MainUI replaces the stock MainUI

An open-source (GPL-3.0) rewrite of Miyoo's closed-source MainUI, installed on all
three models (Mini `283`, Mini+ `354`, Flip `285`), in both Expert and normal mode.

- ⚡ **~250 KB** launcher instead of ~1.4 MB, idle CPU in menus **~5% → ~1%** (Mini+),
  scrolling long titles **~35% → ~6%**, box art scaled in the background.
- 🎮 Letter jump, configurable row count, auto-scrolling titles, gamelist details,
  custom context menus, configurable main menu, safe ROM deletion on FAT32 with
  recovery after a power cut, new **About device** screen.
- 🔍 **Search with X** opens Games → Search with the results, as stock does.
- 🔊 **Brightness, Menu sound and Sleep timer** click on every step, also at the
  lowest and highest value.
- 🔒 **Delete ROM, Clear Recents and Shutdown** need a separate press of A to
  confirm, so a long press can no longer delete a ROM by accident.
- 🎨 Theme text with a font size of 0 is hidden, as in stock (DS XS, Game Boy
  Scouts and similar themes now look right).
- 📂 Many fixes for Favorites, Recents and Favorites folders, including games listed
  twice and recovery from damaged files.
- 📶 Wi-Fi networks with spaces or special characters in the name or password.
- 🔁 ROM caches, Recents and Favorites are used as they are, and stay compatible
  with the stock MainUI.

### ↩️ Going back to the stock MainUI

The stock MainUI is kept on the card. To switch back without reinstalling, create
an empty file named `DISABLED` in `.tmp_update/mainui-test/` on the SD card and
restart. Delete it to switch back to Open MainUI. If the Open MainUI binary is
missing, the stock MainUI starts on its own. If you tested Open MainUI before and
created a `DISABLED` file, delete it to use Open MainUI.

## 🔧 Fixes

- **Flip, lid close (#228):** with *Lid close action = Suspend* and *Power single
  press = Shutdown*, closing the lid powered the device off. The lid now follows
  only its own setting.
- **GameSwitcher (#233):** removing the running game and starting the one that
  takes its place went back to the main menu instead of launching it. A
  long-standing bug, also present in official Onion.
- **RetroArch menu on vertical games (#235):** the RetroArch menu was drawn with
  the game's rotation. It now always displays upright, and the game returns to its
  rotation when the menu closes. RetroArch is updated to `1.22.2-2`.

All three confirmed fixed on the Mini v4, Mini+ and Mini Flip.

## 🧪 CI

- Every combination of lid close action and Power single press is now covered by
  tests, including the #228 case.
- The release workflow can publish a build as a pre-release, so it reaches only
  the beta OTA channel until it is promoted.

## 💡 Tips

- **Scrolling titles:** long titles scroll after one second. To change the delay
  and speed, edit `.tmp_update/config/.romListTitleScroll` (for example `1000,50`
  = 1000 ms delay, 50 px/s); delete the file to turn scrolling off.
- **GameSwitcher shows the box art:** the GameSwitcher saves a screenshot of a game
  the first time you open it while that game is running. Until then, opening it
  from the menu shows the game's box art, which for some systems is portrait.
- **RetroArch settings that don't stick (#224):** RetroArch doesn't save settings
  on exit, and cores like gpSP ship a core override with Keep Aspect Ratio on. Use
  **Quick Menu > Overrides > Save Core Overrides** to change it for good.
- **Switching from another build (#231):** format the card, or replace the `App`
  folder too, then bring back only Roms, Saves, BIOS and Screenshots. Leftover app
  files from other builds can stop apps from starting.

## ⚠️ Known limitations

- Some new Open MainUI labels are English-only for now.
- Open MainUI 1.0.3 is tested on the Mini Plus, Mini v4 and Mini Flip. On the
  Mini v4 and Mini Flip, switching between MainUI and a game's own resolution relies
  on Onion 4.5-dev fixes not yet merged in official Onion; OnionPlus already
  includes them.

## 🧪 If you'd like to try it

Switch the OTA updater to the **beta** channel to receive this build. Useful
things to check:

- Browsing, launching games and returning to Open MainUI
- Search with X, Recents, Favorites and the GameSwitcher
- Brightness, Menu sound and Sleep timer in Settings
- Switching to the stock MainUI with the `DISABLED` file, and back
- A vertical game: open and close the RetroArch menu
- On the Flip: closing and opening the lid with different settings

Please report OnionPlus issues on the [OnionPlus tracker](https://github.com/Amiga500/Onion/issues),
with your model, the version (from `.tmp_update/onionVersion/version.txt`) and the
steps to reproduce. If a bug turns out to be in official Onion too, I'll pass it on
with the fix.

---

## 🙏 Thanks

Thanks first to the Onion team and to the community. OnionPlus sits on their work:
the OS and the years of fixes already in the tree. None of this exists without that.

Thanks to @robcodedev for Open MainUI, and to the testers who sent new reports and
helped me confirm the fixes, especially @Zazzago and @Ziko577.

## 💰 Bounty

The **$20 bounty** is still open through **31 October 2026**. It goes to whoever
reports the most verified bugs in that window.

Right now **Zazzago** is ahead: 3 bugs that actually exist, all patched in this
release.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Expected behaviour is still the official guide: https://onionui.github.io/docs

> ⚠️ **Back up your SD card before updating.** At least copy `Roms`, `Saves`,
> `BIOS` and `Screenshots` to your PC. This release replaces the main launcher,
> and a backup is the quickest way back if anything goes wrong.
