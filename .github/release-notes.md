<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅🔴 RedOnion 4.5.0 stable: Open MainUI 1.1.0, carousel and list views, video previews, Game Info menu, Saved Games

> ⚠️ **Always back up your SD card before updating!** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.

> ✅ **Stable.** This is the stable release, offered to devices on both the **stable** and the **beta** OTA channel. It follows 4.4.0 directly: there was no public 4.5.0 beta, the build was tested by the RedOnion testers with the Open MainUI 1.1.0 release candidates.

This release brings **Open MainUI 1.1.0** by @robcodedev: the console page as a **grid, carousel or list**, **video previews** in the game lists, a **Game Info menu** with a gallery, maps, walkthroughs and PDF manuals, and **Saved Games** to start a game from any of its save states. RedOnion adds **reorganised Tweaks**, **new console pictures**, a **system core** and **Hide game** in the game options, and **safer SD card handling** on install and update.

RedOnion is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It is based on Onion `4.4.0-beta`, with Miyoo Mini Flip support ported from `4.5-dev`, and runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip. Open MainUI is now **1.1.0**, built from its source: https://github.com/robcodedev/onionos-mainui-opensource

## 🆕 New in 4.5.0

### 🖥️ Open MainUI 1.1.0 by @robcodedev

- **Console page as a grid, carousel or list**, for Games and Expert, with its options in Tweaks › Home screen › Console page (size, spacing, animation, speed, wrap around…). The default is Rob's carousel.
- **Remember console:** optionally reopens on the last console used, even after a shutdown (thanks zazzagone).
- **Video previews in the game lists**, where the cover is or over the whole screen, after the wait you choose (thanks u/Kylie_Ray for the idea).
- **Game Info menu** (SELECT in Game Info): a gallery with box art, screenshots and video, game maps with zoom, walkthroughs (TXT) and PDF manuals, with a loading panel while a big PDF opens. Files go in `Roms/<console>/media/` (see Game media below).
- **Saved Games:** a game's save states with their pictures, SNES included, with 4:3 previews for most home consoles and computers. Starting an older save **replaces the auto save** by default (Overwrite); Backup keeps it as "Previous Session", Swap trades the two. The layout is **Framed** by default, or Overlay. Arcade games have no Saved Games by default, as their core cannot load a state while a game starts (`saved_games.disabled_rom_dirs` in `.tmp_update/config/game-menu.json`).
- **Long titles scroll**, rolling or bouncing, and the Game Info description can scroll by itself.
- Full list: https://github.com/robcodedev/onionos-mainui-opensource/blob/main/CHANGELOG.md

### 🧭 Tweaks

- **Menus reorganised**, with clearer names. On/Off options are switches grouped together, and options are greyed out while what they need is off.
- **"(default)"** marks the starting value, and long values scroll on the selected row.
- **Game lists › List menus** is split into Games, Favorites, Recents, Search and GLO, and each entry of the game options (GLO) can be hidden (thanks Veuks for the review).
- An option for every Open MainUI 1.1.0 setting: console page, game lists, previews, Game Info menu, Saved Games.

### 🎮 Games and game options (GLO)

- **System core:** after choosing a core in "Game core", pick "This game only" or "All games of this system". The system core is kept after updates, and a per-game core still comes first (thanks u/Joecascio2000).
- **Hide game** hides finished games from the list, Search and Recents; "Hidden games" brings them back. Saves and box art stay untouched.
- **Sega Saturn (experimental)** in the Package Manager: Yabadabadoo 0.4, with a Saturn BIOS. Performance varies a lot between games.

### 🖼️ Console pictures

- **New pictures for the carousel and the list:** [KyleBing's Retro game console icons](https://github.com/KyleBing/retro-game-console-icons) (CC BY 4.0), also for Search and Saturn. Console folders renamed or copied by hand keep their picture.

### 🕹️ GameSwitcher

- **Load** shows "X DELETE", and save previews have the game's real shape.
- **X button** artwork for the featured themes; in Silky RedOnion the bar has B and A on the sides, Y and X in the middle.

### 💾 SD card

- **Clean reboot after an install or an update:** the installer and the OTA update now restart through RedOnion's clean shutdown, as when you power off. They used a plain reboot (inherited from Onion), which could leave the card marked as not properly unmounted.
- **SD card check notice:** if the card was not shut down cleanly (power lost, card removed, a crash), a notice at boot asks you to check it on a PC. It shows once.

### 📶 OTA update

- **More reliable connection check** with curl, asking GitHub for the headers only (#311, thanks u/Nyiz).
- **MENU asks** before stopping a download, and buttons pressed meanwhile no longer answer the next prompt (thanks u/kicksoda).
- The app is called **"Update RedOnion over internet"**.

## 🔧 Fixes

- **Settings:** brightness, volume and mute set with the buttons are no longer reset when leaving Tweaks or applying a theme (#305, thanks @Roland013).
- **Battery:** on the Mini+ and Flip, the GameSwitcher and the other RedOnion screens show the battery level while charging, like MainUI. The low battery warning falls back to 10%, as Tweaks shows.
- **Favorites:** favfix no longer removes apps from Favorites (our fork of SearchFilter, thanks Veuks).
- **Search:** each ROM folder is searched once (no more duplicates from Expert), and Expert systems are listed at the end (thanks zazzagone).
- **GLO:** Favorites and Recents maintenance no longer starts the game (thanks fantomas).
- **Saved Games:** starting a save no longer flashes the old GameSwitcher screenshot on the LOADING screen (thanks @robcodedev).
- **PDF manuals from Game Info:** MENU closes the PDF and goes back to Game Info, and the reader's own buttons work (SELECT rotates the page).
- **Multi-disc playlists (m3u):** no box art taken for the discs, titles with [ ] work, no space at the end of the name (#310, thanks u/tamamatuaai).
- **DraStic on the Mini v4 and Flip:** no garbled screen on the first game opened.
- **Ports and ScummVM:** "Import games" and "Change names" go back to the list.

## 📁 Game media (Open MainUI 1.1.0)

- Everything goes in `Roms/<console>/media/`, one lower-case folder per kind, each file named like the ROM without its extension: `box2dfront`, `box2dback`, `box3d`, `screenshot` (`Name.png`, `Name-1.png`…), `videos`, `maps` (`Name.png`, `Name-1.png`…) and `manuals` (`Name.txt` walkthrough, `Name.pdf` manual).
- Pictures and maps are PNG; maps up to 2000×2000 px (bigger ones are skipped). Videos up to 1280×720, H.264 MP4 recommended (the game-list preview uses the `.mp4`). Walkthroughs up to 2 MB.
- The cover in the game list is still `Imgs/<ROM name>.png`, or the `<image>` of `miyoogamelist.xml` when the folder has one.

## ⚠️ Known issues

- **Long transfers:** with nobody touching the console, a long FTP, HTTP or Samba transfer can be cut by the sleep timer (Settings › Sleep timer). Press a button now and then, or keep the device charging.
- **Tweaks** is in English in every language.
- **OTA security (inherited from Onion):** the updater downloads without checking TLS certificates and only verifies the size of the package. Verifying certificates and the package's SHA-256 is planned.

Please report RedOnion issues on the [RedOnion tracker](https://github.com/Amiga500/Onion/issues), with your model, the version (from `.tmp_update/onionVersion/version.txt`) and the steps to reproduce. Turning on Tweaks › Advanced › Diagnostics › Enable logging and sending `.tmp_update/logs` helps a lot.

---

## 🙏 Thanks

Thanks first to the Onion team and to the community. RedOnion sits on their work: the OS and the years of fixes already in the tree. None of this exists without that.

Thanks to @robcodedev for Open MainUI 1.1.0 and the close work on its integration, to zazzagone and Veuks for testing every release candidate and reviewing Tweaks, to @dredvm for the RedOnion name, logo and the Silky RedOnion theme, to KyleBing for the console icons, to @KNRBZ for the ideas behind Open MainUI's gallery, maps, walkthroughs and manuals, and to @Roland013, u/Joecascio2000, u/Nyiz, u/kicksoda, u/tamamatuaai, fantomas and u/Kylie_Ray for their reports, ideas and pull requests.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource
- Expected behaviour is still the official guide: https://onionui.github.io/docs
