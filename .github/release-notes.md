<!-- One line per paragraph or list item: GitHub releases turn every line break into a visible break. -->
# 🧅🔴 RedOnion 4.5.1 beta 5 (tester): Open MainUI rc11, the MiniCinema and RedAmp players, two more new apps by robcodedev, a faster game menu, the Flip lid pauses videos

> ⚠️ **Always back up your SD card before updating!** At least copy `Roms`, `Saves`, `BIOS` and `Screenshots` to your PC.

> 🧪 **Testers only.** This build is for the RedOnion testers (testers OTA channel). Devices on the **beta** and **stable** channels are not offered it. Please report what you find.

RedOnion is still a personal-use build. It is **not** a replacement for Onion and **not** an official release. It runs on the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

This beta brings the **release candidate 11 of Open MainUI 1.1.1** by @robcodedev, as a prebuilt binary, two new apps of his, **MiniCinema**, a new video player by Rob and Veuks, and **RedAmp**, a new music player by Rob. Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource

## 🆕 New in 4.5.1 beta 5

### 📦 Apps by robcodedev (Package Manager › APPS)

- **New: SD Card Doctor 0.6.3** by @robcodedev: card and FAT32 information, quick and deep read scans, a speed test, folder sizes, a read-only FAT check, the card's identity and a reviewed cleanup of macOS leftovers (`.DS_Store`, `._*`…), moved to a quarantine you can restore from. It never formats or repairs the card. The write part of the speed test asks first, and needs 25 % battery and 160 MiB free.
- **New: AudioTune 0.2.9** by @robcodedev: tone, stereo and noise tests, a five-band EQ (250 Hz and 8 kHz as low/high shelves, 500 Hz, 1 kHz and 4 kHz) with presets (Flat, Presence, Full, Clear, Warm, Bright, Custom), and music to try it on (its demo, or your own `sample.mp3` next to its `launch.sh`, played at 48 kHz). **L2** picks the output you are listening on, SPEAKER or HEADPHONES (SPEAKER protects the small speaker from strong bass), **R2** turns on a soft limiter (EQ page) or switches SAFE/PUNCHY headroom with headphones (Sound Profiles page). On Sound Profiles, moving over a preset lets you hear it while the music plays; **A** applies it. On the firmware it was checked against, it can also carry the EQ into games when you leave the app (beta: when the firmware is different, the normal audio is left as it is). Start at a low volume.
- **DisplayCal 0.8.4** by @robcodedev: on the Display page, Lumination starts from and resets to (X, Y) your model's default when `system.json` has none: 7 on the Mini and Mini v4, 10 on the Mini+ and Mini Flip (it was 10 on every model).

### 🍿 New: MiniCinema 1.1.42 (Package Manager › APPS)

- **MiniCinema** by Rob (@robcodedev) and Veuks: a new video player for the videos in `Media/Videos` (and its folders), built on FFmpeg 4.4.8 and the KidsPlay player Veuks made for Kids Mode. Its list sorts names naturally (Episode 2 before Episode 10), remembers where you stopped in each video and shows it (a percentage, END when watched, BAD when it could not play), and can show a picture of each video. Playing: resume, chapters, audio and subtitle tracks (also `.srt` files next to the video), Fit/Fill/Stretch/Original, a sleep timer and the next video by itself if you want it. SELECT opens the options, in the list and while playing; while playing, MENU goes back to the list and MENU twice leaves the app. While a video plays the sleep timer leaves it alone, and POWER or closing the Mini Flip lid put the console to sleep as usual. The Video Player stays as well.
- This is its **first public test build**: please tell us how it works on your model.

### 🎵 New: RedAmp 1.1.15 (Package Manager › APPS)

- **RedAmp** by Rob (@robcodedev): a new music player with Winamp-style skins, for MP3, FLAC, WAV, Ogg Vorbis and Opus (also AAC/M4A/ALAC), and internet radio (the stations are in its `stations.m3u`, Wi-Fi needed). A music library from `Media/Music` (artists, albums, songs, folders), playlists, an equalizer with presets, visualizers, album covers from the music folders and the tags (and, if you turn it on, downloaded from MusicBrainz / Cover Art Archive), a sleep timer and a screen-off mode. **Play during standby** (on by default) keeps the music playing when POWER turns the screen off or the Mini Flip is closed; switched off, POWER and the lid pause it.
- **Saved playlists and favorites**: make, rename and delete playlists, mark songs with a heart, and **search** the library by title, artist, album or file name. The play queue can drop a song, be cleared or be saved as a playlist; playing one song from the library no longer clears it.
- This is its **first public test build**: please tell us how it works on your model.

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

### 🎮 Open MainUI 1.1.1 rc11

- MainUI can now keep the exact screen it is on when it is asked to close, and open it again: RedOnion will use it to bring you back from the GameSwitcher in a coming build.
- **Settings › Sleep** has its own icon (a moon), no longer About's.
- **Expert:** the names under the icons are bigger with the Silky RedOnion theme (22 instead of 18), still inside their boxes. Thanks to robcodedev.
- Lumination, when `system.json` has none, is your model's default (7 on the Mini and Mini v4, 10 on the Mini+ and Mini Flip): it was 7 on every model.

### 📜 Game lists

- **Game Info text scrolls by itself**, so long descriptions (from the scraper, for example) can be read to the end: Tweaks › Game lists › Previews › Game Info text: auto scroll is now **on**, 2 s before it starts, 20 px/s. It is turned on once with this update, even if you had switched it off: switch it off again and it stays off. Thanks to Machiavellian.

### 🔍 Scraper

- **Mini Flip: no more freeze while scraping box art from LaunchBox.** Box art was shrunk with the graphics hardware without checking that video memory was there: after a few minutes of scraping, a Flip could freeze (screen stuck, the console hot). Box art is now shrunk in software, and pngScale checks video memory before using it. A picture that cannot be converted is skipped ("Picture could not be converted."), and one cut by MENU is no longer kept as the game's picture: that game is scraped again next time. Thanks to ToastedLion for the report and AP for testing the fix.
- **A warning before scraping a whole console, all favorites or all consoles:** long sessions heat the device and drain the battery (on a PC it is faster), and if it ever freezes, hold POWER for at least 10 seconds until it turns off. You can still start or cancel.

### 🔋 RedOnion Battery Monitor (ksielick's 2.3.3 and 2.3.4)

- **Clearer first-time setup** (Mini+ and Mini Flip, PMIC page): "Set counter to 100% (battery full now)" (it read like it saved the battery capacity) is refused until a capacity is saved, as it set the counter alone. Battery Capacity shows **Not saved** in orange until you save it. **Enable capacity learning** with no capacity saved now saves it for you ("[A] Save & Enable", the value shown first), checks the PMIC kept it, and only then turns learning on. Thanks to robcodedev for the report.
- **A sleep that ran the battery flat** showed "-0%": it now shows what it used ("SLEEP 10H05 -98%"). A charge across a clock correction from Wi-Fi no longer shows a huge time. The session line reads "100% > 45%   PLAY 3H12   SLEEP 1H07 -15%".

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
- MiniCinema's and RedAmp's sources are kept in `third-party/minicinema` and `third-party/redamp`.
- AudioTune's launcher writes its whole-line checks in a form the firmware's BusyBox understands (with `grep -x` the saved EQ would never have been found).
- Tests for the model default Lumination, the OTA menu starting on your channel, the Console page choices, the SELECT hint, the Game Info scroll, the Video Player's standby and the faster game options menu (compared with the old one); the testers' OTA from beta 4 to beta 5. The script tests also pass under BusyBox 1.20.2, the version in the firmware.

## ⚠️ Known issues

- **RedAmp, "Music in games":** it needs RedOnion to start games and MainUI through RedAmp's audio hook, which is not in this build: leave it **off**. Turned on, the music does not reach games or MainUI, and its background player keeps the console from suspending games on POWER.
- **RedAmp, Play during standby:** a standby started while music plays has no automatic power-off, also after the music ends. Wake the console, or switch it off, when you are done.
- **MiniCinema after a power-off:** reopening the video you were watching at the next start is still experimental.
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

Thanks to the Onion team and the community, whose work RedOnion sits on. Thanks to @robcodedev for Open MainUI, MiniCinema, RedAmp, SD Card Doctor, AudioTune, DisplayCal, the faster scripts and many fixes; to ksielick for the Battery Monitor; to Veuks for Kids Mode and MiniCinema; to KyleBing for the console pictures; to SundownerSport for the GameTank core, Clyde Shaffer for the GameTank and its emulator, and tenlevels for the GameTank icon; to QuackWalks for the Mini Flip lid report; to ToastedLion and AP for the Mini Flip scraper freeze; and to the RedOnion testers Machiavellian, Veuks and zazzagone.

## 🔗 Links

- Repo: https://github.com/Amiga500/Onion
- Issues: https://github.com/Amiga500/Onion/issues
- Open MainUI: https://github.com/robcodedev/onionos-mainui-opensource
- Expected behaviour is still the official guide: https://onionui.github.io/docs
