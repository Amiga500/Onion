# Changelog

## 1.0.8 - 2026-10-15

### Buttons

* Y, Select or Start held while the device goes to sleep no longer acts when it is released after waking. Y could launch the selected game, Select open the context menu and Start reload a list.
* A on an empty ROM folder, Favorites, Recents, Search results or Apps does nothing; it said "Launch unavailable". Menu on an empty Favorites, Recents or Search list no longer reads an entry removed from it, which could crash.

### Status bar

* The battery level and charging state are checked every second, as Onion's own screens do, instead of every 5 seconds with the other device status, so the charging icon appears or goes within about 3 seconds of plugging the charger in or out; it could take 5 to 10. Only a change redraws the screen. Thanks to [@Veuks](https://github.com/Veuks) for the suggestion.
* The Wi-Fi icon moves left beside the battery when the battery percentage would cover it, instead of being hidden behind it. It stays in stock's place otherwise, and Wi-Fi artwork wider than stock's slot, such as Super Onion Entertainment System Remix's, stays where stock puts it. Thanks to [@Ziko577](https://github.com/Ziko577) for the report.
* On the Mini Plus and Mini Flip, the battery icon changes to the charging one while the charger is plugged in, as on the Mini. MainUI waited for a charging mark Onion writes only on the Mini; it now asks the power chip, as stock MainUI does. Thanks to [@Zazzago](https://github.com/Zazzago) for the report.
* On the Mini Plus and Mini Flip, the battery percentage shows beside the charging icon while charging; stock MainUI hides it. On the Mini, Onion does not know the level while charging, so only the icon shows there, as before. Thanks to [@Veuks](https://github.com/Veuks) for the suggestion.
* The battery icon changes at the same levels as in stock MainUI (above 69, 44, 14 and 4 %); it used the levels of Onion's own screens (90, 60, 30 and 5 %).
* The Wi-Fi icon shows also when the battery icon cannot be drawn, such as before Onion has written the battery level; it was hidden with it.
* Onion's clock overlay (the theme's `time` settings, drawn by `libmainuihooks.so`) should show the time in MainUI, as with stock MainUI: it waits for a line stock MainUI prints at start, which MainUI now prints too. This is not yet confirmed on a device. Thanks to [@fengfrw](https://github.com/fengfrw) for the report.

### Favorites and menus

* Deleting a Favorites folder removes all it holds, the folders inside it and their Favorites, after asking first ("Delete folder", "Remove from favorites list?"). It moved them up a level. The games stay on the SD card. Thanks to [@Veuks](https://github.com/Veuks) for the suggestion.
* A game pasted on the folder it was moved out of moves up into the open folder. Before, a game could not be taken out of a folder that was all Favorites held, as there was no other row to paste it on. ([#30](https://github.com/robcodedev/onionos-mainui-opensource/issues/30)) Thanks to [@Veuks](https://github.com/Veuks) for the report.
* Search and Tweaks set to show in the main menu's context menu (`main-menu.json`) are left out while their app is not installed, as Settings already left out Tweaks; selecting them said the launcher was unavailable. Thanks to [@Veuks](https://github.com/Veuks) for the report.

### Themes and screens

* Confirmations (Shut down, Clear Recents, Delete ROM) and messages draw their title in the theme's `total` colour and their text in `grid.color`, as stock's dialogs do. They were always white and grey. Thanks to [@Zazzago](https://github.com/Zazzago) for the report.
* A theme can colour the part of a title that matches an in-list search with `gamelist.searchColor`. Without it, the match is purple or yellow as before.
* The About page's "Onion version" row is named "OS version".
* The About page's Website row shows the first line of `.tmp_update/onionVersion/website.txt` when that file exists; otherwise www.lomiyoo.com, as before.
* Messages such as "Launch unavailable" or "Catalog unavailable" are drawn on the theme's popup art (`skin/pop-bg.png`), as stock draws its notices and as confirmations already were. A message too long for it, or a theme without it, keeps the plain dark panel.
* Footer hints, the page counter and dialog buttons lean in a theme whose `hint.font` is upright, as in stock MainUI, which draws them in italic style. A hint font that is italic already looks as before.
* Refresh all roms keeps the console grid's cursor where it is; it jumped to the console last opened, or the first.
* Leaving a folder with A on ".." shows the selected game's artwork at once, as B does.
* When Delete ROM cannot reopen the list, the console grid it leaves on screen is drawn afresh.

### Sound

* With Onion's audio fix off, the menu's audio buffer is 4096 sample frames, as in stock MainUI. With it on, it stays 1024 as before; stock uses 128 there, which raised MainUI's idle CPU use from 0.9 to 1.6 % in testing, for a latency gain (about 21 ms down to 3 ms) that theme sounds and music do not need.

### Wi-Fi

* Turning Wi-Fi off and quickly on again no longer leaves it on "Scanning..." with no networks. MainUI started Wi-Fi again before the chip had finished switching off. Thanks to [@Amiga500](https://github.com/Amiga500) for the report.
* Settings › Wi-Fi says when a connection fails, with stock's notice ("Fail to connect / Check your password / Or reboot router"), also when the network is not joined within 30 seconds, which is how a wrong password shows. When Wi-Fi cannot be turned on, it says why. Its other notices, such as Wi-Fi being busy, show too; only "Scanning..." did.
* The Wi-Fi icon and the connected network's mark no longer go for a moment when connecting, turning Wi-Fi off or on, or launching a game: an interrupted status check removed the saved Wi-Fi status.

### Speed and memory

* Launching a game from a ROM list saves its position once instead of twice.
* Cover art without transparent pixels is drawn as a plain copy instead of being blended, as theme images already were. It is drawn again on every frame of a scrolling title.
* The Shut down, Clear Recents and Delete ROM question and the language list no longer read and decode their artwork from the SD card on every frame.
* Scrolling a ROM list checks its rows against Favorites with less work, and not at all when there are none.

### Documentation and tests

* [docs/NAVIGATION.md](docs/NAVIGATION.md) explains how the `state.json` MainUI writes differs from stock's, and what that means for Onion's list reset; [docs/THEMES.md](docs/THEMES.md) which battery icon shows at which level, where the Wi-Fi icon goes when the battery percentage would cover it, that Onion's clock overlay should work, how long the charging icon can take to change, and how messages use the popup art and which colours dialogs use, the `gamelist.searchColor` key, that the hint font is drawn in italic style and the headphone icon is not drawn; [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) what the Wi-Fi failure notice means, and why the Mini Flip's display colours (Luminance, Hue, Saturation, Contrast) can be back at their defaults after a reboot: MainUI sets them only from Settings › Display, as stock does, and leaves setting them at boot to the OS (thanks to [@QuackWalks](https://github.com/QuackWalks) for the report, and to [@Zazzago](https://github.com/Zazzago) for finding the problem on RedOnion); [docs/NAVIGATION.md](docs/NAVIGATION.md) that A, B and X act when pressed (stock acts on release), and that Y does nothing outside game lists for now.

## 1.0.7 - 2026-10-11

### Buttons

* Onion's Expert app opens Expert, as with stock, also when Expert is hidden on the main menu. Set as the X or Y shortcut in Tweaks, it only made the screen flash; started from Apps, it went back to Apps. Thanks to [@fengfrw](https://github.com/fengfrw) for the report.
* Y acts when it is released, as stock does. With an app or tool set as the Y shortcut in Tweaks, Y on a game could also launch it, or put it first in Recents, before Onion ran the shortcut.
* Y on a game opens Game List Options every time when that is the Y shortcut in Tweaks (Onion's default). Shortly after boot or a game, Onion could miss the Y press and start the game instead. With no Y shortcut ("A button"), Y launches the game as A does, and Recents lists it once. If the Y setting cannot be read, Y does nothing.
* Select opens the context menu, and Start reloads a list, when released, as in stock, and not after a combo: while Select or Start is held, the other buttons are Onion's (brightness, volume boost, CPU clock) and MainUI ignores them. Select with L2 or R2 or with Start and L1 or R1 no longer leaves the menu open, and those combos no longer reload, page or jump in a list.
* A Menu long press shortly after Select with L2 or R2 on the Mini opens the context menu. Up to 1.5 seconds after, it was taken for the Menu press Onion sends after a plain Select and ignored.

### Themes and screens

* A brightness changed in GameSwitcher stays when you go back to the main menu, also after exiting a game started from GameSwitcher. Onion put the old brightness back with the first button press. As stock does on every start, MainUI now loads the settings in `system.json` (brightness, volumes, sleep timer, display colours) into Onion's live settings and sets the screen's brightness from them. A setting missing from `system.json` takes stock's default: menu music starts at 8 (it started at 20), and Settings shows Menu sound 8 and Sleep timer off (it showed 20 and 5 minutes). Thanks to Zombiediplomat for the report.
* While Onion's Wi-Fi hotspot runs, the hotspot icon (`icon-hotspot-wifi.png`) takes the Wi-Fi slot in the header, as in stock.
* The startup tab set in Tweaks (Recents, Favorites, Games, Expert or Apps) opens at boot, as with stock; MainUI always started on the main menu.

### Lists

* A game's details from Favorites or Recents show its genre and description also when its system is not in the open grid (an Expert emulator while Games is shown, say).
* Line breaks and runs of spaces in a game's genre are collapsed, as in its description and in stock, so they no longer split the details' summary line.
* A `.bin` file is hidden beside its `.cue` only when that `.cue` is a file. A folder or link named like it hid the `.bin` and listed nothing in its place. Run Refresh roms for a console where this happened.

### Speed and memory

* After Wi-Fi is turned on, the network address comes within about a second of joining the network. MainUI's DHCP client tried three times, 3 s apart, starting before the join, then paused 20 s, so a join slower than about 9 s left Wi-Fi without an address for 10-20 s more. MainUI also waits for the Wi-Fi interface to appear after powering the chip on (up to 10 s), then starts Wi-Fi as stock does: 1 s later instead of 2 s, with no pause when the chip is on already, and letting `wpa_supplicant` bring the interface up, which joined faster on a noisy channel. Thanks to [@Amiga500](https://github.com/Amiga500) for the report.

### Documentation and tests

* [docs/CATALOG_CACHE.md](docs/CATALOG_CACHE.md) lists how Refresh all roms and game details differ from stock, and [docs/THEMES.md](docs/THEMES.md) which `fontascii` key is read.

## 1.0.6 - 2026-10-09

### Buttons

* X acts as B (back, close, cancel) on every screen, as stock does; it did nothing on most screens and opened search in a ROM list. Search there stays in the context menu (Select or Menu), and in the on-screen keyboard X still types a space. Thanks to [@Veuks](https://github.com/Veuks) for the report.
* The button mapping in `system.json` (`keymap`) is handed to the device's button driver at start, as stock does; before, it was ignored. A `keymap` that is not eight button names (`L2`, `L`, `R2`, `R`, `X`, `A`, `B`, `Y`) is ignored and the default is used, so a typo cannot leave the buttons unusable.

### Themes and screens

* A theme whose context-menu background is a full-screen image (Super Onion Entertainment System Remix, RetroRama and others) shows it as it is. A 5- or 6-row menu built it from the 4-row image by repeating its top band, which pushed the rest of the picture down. Thanks to [@QuackWalks](https://github.com/QuackWalks) for the report.
* The screen behind messages and confirmations is darkened, as behind the context menu and as in stock.
* Console and main-menu labels drawn in a theme's own font (`grid.font`) keep that font's weight. Before, MainUI made them bold, as stock does, which thickened pixel fonts such as Super Onion Entertainment System Remix's. Labels in the default font stay bold. Thanks to [@QuackWalks](https://github.com/QuackWalks) for the report.
* Expert console labels use stock's font for them: `grid.fontascii`, or without it the list font, at the `grid.grid3x4` size, in the font's own style. A theme that hides its Games labels with a blank `grid.font`, such as ONION PS, now shows its Expert labels, as stock does. `grid.grid3x4` set to 0 still hides them. A theme without `grid.grid1x4` gets stock's 25 for the Games labels, instead of 24. Thanks to [@Zazzago](https://github.com/Zazzago) for the report.
* Expert draws its selection mark 2 px further right and down, and its icons and labels 1 px lower, so the grid matches stock's pixel for pixel.
* The battery percentage and the page counter sit where stock puts them. Both were 1 px too low with some themes.
* The home screen's header logo (`miyoo-topbar.png`) is no longer cut off at the header. A theme can draw below it with a tall image, as stock allows; Super Onion Entertainment System shows its row of dots under the home icons again.
* The Wi-Fi icon is centered in stock's status slot left of the battery (x 504), so a theme's wide Wi-Fi image lands where the theme expects it. Super Onion Entertainment System Remix's cloud covered the end of the title, and the icon of themes with very wide images, such as BMO, was drawn off-screen. Thanks to [@QuackWalks](https://github.com/QuackWalks) for the report.
* The Apps list's rows start at y=62 again, as a stock screenshot shows they do. The fourth row is drawn in full, its highlight reaching 2 px over the footer as in stock, so a framed highlight is no longer cut off at the bottom; 1.0.5 moved the rows up 2 px instead.

### Lists

* The Apps list keeps the order the SD card lists the app folders in, as stock does, instead of sorting them by name. On FAT that is usually the order they were copied in, so Quick Guide may come first. An empty file named `.appsort` in `.tmp_update/config` sorts them by name instead.
* A `.bin` file is no longer listed beside the `.cue` of the same name, as in stock; the `.cue` starts the game. Unlike stock, the `.bin` stays when the `extlist` does not list `.cue` files, so the game is never left without an entry. Run Refresh roms for a console whose cache lists both.
* A console with an empty or no `extlist` leaves out files without an extension, such as `README`, as stock does. Run Refresh roms for such a console.

### Sound

* Theme music recorded at 48 kHz, as in most themes, plays at its proper speed and pitch instead of about 8% slow and low. Music recorded at 44.1 kHz now plays slightly fast, as with stock. Thanks to [@QuackWalks](https://github.com/QuackWalks) for the report.

### Documentation and tests

* [docs/CATALOG_CACHE.md](docs/CATALOG_CACHE.md) lists how ROM lists differ from stock: which files each leaves out, and the sort order, which also applies to the Games and Expert grids.

## 1.0.5 - 2026-10-07

### Buttons

* Y on a game in a ROM list, Favorites, Recents or Search results launches it as A does, as stock does, so Onion's default keymap opens Game List Options for it ([#14](https://github.com/robcodedev/onionos-mainui-opensource/issues/14)). Y on a folder does nothing. Thanks to VEUKS for the report, via [@Amiga500](https://github.com/Amiga500).
* Menu opens the context menu again, as Select does. Onion's default "Context menu" long press (Tweaks > Button shortcuts > MainUI: Menu button) makes keymon send MainUI only a release of Menu, which stock opens the menu on; Open MainUI ignored Menu, so the device vibrated and nothing happened. That release is now read from the input device, since SDL drops a release of a key it never saw pressed. The L1 presses keymon sends after it, and again for as long as Menu is held, are ignored, so holding Menu in a list no longer moves the selection. Thanks to VEUKS for the report, via [@Amiga500](https://github.com/Amiga500).

### Game List Options

* Backing out of Game List Options leaves Recents as it was. It removes the first game in Recents, taken to be the one just launched, so Y adds the game there without moving the others; if Recents cannot be written, Y shows Launch unavailable instead.
* A Y press MainUI ignores, on a folder for example, no longer makes the next A launch open Game List Options.
* Game List Options knows which list a game came from and shows that list's options, such as Filter list and Refresh list: `state.json` is written one field per line, as stock writes it.
* A game added to Favorites or launched from Onion's Search console, or from a search in a ROM list, keeps its ROM path as the list spells it (`/mnt/SDCARD/Emu/GBC/../../Roms/GBC/...`) instead of `/mnt/SDCARD/Roms/GBC/...`. Game List Options recognizes a game only by the first, so it showed no game options, such as the core, for these Favorites and Recents. Favorites added before keep their old path; remove and add them again.

### Themes and screens

* Theme overrides made in Tweaks (Appearance > Theme overrides), such as the battery percentage size or hidden labels, are applied again: `Saves/CurrentProfile/theme/config.json` is merged over the theme's `config.json` field by field, as Onion does ([#15](https://github.com/robcodedev/onionos-mainui-opensource/issues/15)). See [docs/THEMES.md](docs/THEMES.md). Thanks to VEUKS for the report, via [@Amiga500](https://github.com/Amiga500).
* The Apps list shows its row counter, as stock does (10/15 on row 10 of 15).
* The Apps list's rows start at the top of the list area instead of 2 px lower, so the last row is no longer cut off at the bottom. Thanks to ZZZ for the report.
* Footer hints, the page counter and dialog action hints use the theme's `hint.font`, or when it names none the default font (Exo 2 Bold Italic), as in stock. Before, they used the title font.
* The screen behind the context menu is darkened, as in stock. Thanks to ZZZ for the report.
* Expert marks the selected console with the theme's `bg-ra-list-item.png`, as stock does, instead of drawing the Games tiles behind every console. Thanks to [@Zazzago](https://github.com/Zazzago) for the report.

### Lists

* A `Manuals` folder in a console's ROM folder, in any case, is no longer listed as a ROM folder, the same as `Imgs`. Run Refresh roms for a console whose cache was built with it. Thanks to [@QuackWalks](https://github.com/QuackWalks) for the report.
* A folder in a console's ROM folder is listed only when it holds a ROM of that console somewhere below it. Empty folders and the data folders of ScummVM games, ports and other extracted games (`AUDIO`, `DRIVERS`, `MUSIC`, `SPEECH` and the like) no longer fill the list; a ScummVM list now shows just its games. This applies to lists built from the ROM files, not from `miyoogamelist.xml` or the scan MainUI falls back to when a cache cannot be built. Run Refresh roms for a console whose cache lists such folders. Thanks to [@Zazzago](https://github.com/Zazzago) for the report.
* A console whose `config.json` has an empty or no `extlist` lists all its files again, instead of none. Only the game lists, ROM list caches and other files MainUI and Onion keep beside the ROMs are left out. Run Refresh roms for such a console to list its games. Thanks to [@Ziko577](https://github.com/Ziko577) for the report.
* An Expert config with a `rompath` and an empty `extlist`, such as fMSX's, opens its ROM list of all files, as an empty `extlist` does in Games. 1.0.4 started the emulator without a game; only a config without a `rompath` launches directly. Thanks to [@Zazzago](https://github.com/Zazzago) for the report.

### Sound

* Onion's Mute background music (Tweaks) now silences MainUI. Onion mutes the music by renaming the theme's `bgm.mp3` before MainUI starts; Open MainUI then played the default theme's music in its place. A theme without `bgm.mp3` now plays no music, as in stock. The change sound still falls back to the default theme's.
* Background music that fails to start is logged and tried again when Menu sound is raised above 0. Before, it stayed silent until MainUI restarted.

### Speed and memory

* The Apps list decodes the icons of its visible rows once and keeps them, instead of decoding them again on every redraw.
* An icon left out for the 24 MiB icon budget is remembered with its size, so it is not decoded again on every page while it still cannot fit.
* Games and Expert appear sooner: a page of consoles decodes its normal icons and only the selected console's selected icon before it is shown, instead of both icons of every console on the page. The other selected icons follow while MainUI waits for input; a console selected before its icon is ready shows its normal icon for a moment.

### Logging

* The `[timing]` line has two new counters: `discover-ms`, the time spent reading the console and app folders with their configs, and `icon-ms`, the time spent loading and decoding console and app icons. They show whether a slow first entry into Games or Apps is reading the card or decoding icons. See [docs/TIMING.md](docs/TIMING.md).

### Documentation and tests

* The overlong gamelist path test from 1.0.4 now actually fails without the fix it covers.

## 1.0.4 - 2026-10-04

### Game lists

* A console whose `miyoogamelist.xml` is not strictly valid XML opens again. 1.0.3 refused the whole file on any flaw, so the console showed "Catalog unavailable" after every Refresh roms or reinstall; Onion's own gamelist generator writes such files. The file is now read as stock reads it: an unescaped `&` or `<` in a title, an unknown entity such as `&nbsp;`, Windows-1252 accents and quotes, a missing end tag, a repeated field and text outside the list are all accepted. Game details read `gamelist.xml` the same way. Thanks to @Zazzago for the report ([Amiga500/Onion#238](https://github.com/Amiga500/Onion/issues/238)).
* A `miyoogamelist.xml` that cannot be used at all (empty, over 16 MiB, or without a usable `<gameList>`) no longer blocks its console either: the list is built from the ROM files, the log says why, and the file is left as it is. Run Refresh roms after fixing it. A gamelist that cannot be read because of a card or permission error is not treated this way: the previous list is kept, so a passing error never replaces the gamelist titles with file names. See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

### ROM list caches

* A console whose cache opens but whose first page holds a damaged row is repaired again: its cache is rebuilt once, as when the cache does not open at all. Before, MainUI scanned the folder instead, on every entry, so the console seemed to refresh each time it was opened. A busy, I/O or memory error still never replaces a cache.
* When a list page cannot be read and recovery ends by scanning the console's folder, that scan now lasts for the current visit only. Before, the console's cache stayed unused until MainUI restarted, so one passing error made it rescan on every entry.

### Lists and navigation

* Reopening Favorites or Recents after its last entry went (for example a Favorite removed from its console list) no longer leaves an empty row below the list. Returning from a game to Favorites, Recents, a ROM list or Apps that lost entries meanwhile fills the window the same way.

### Screens

* A message too long for one line wraps instead of being cut off at the edge of its panel, and the panel grows down when it needs more lines. Before, "Cannot import miyoogamelist.xml; check for empty, invalid or unreadable XML" was cut off at "check for emp".

### Logging

* With Onion's logging on, the log now starts with the version, the device and the SD card and theme in use, and records, one line each: every message shown on screen, every ROM cache built or removed and why, a failed folder read, search, refresh or repair with its console, why a screen could not be restored after a game, a console listed by scanning its folder, a file that could not be saved, a setting that could not be saved, and an interrupted ROM deletion resolved. Nothing is logged per frame or per key press. [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) explains how to read it, and that Onion deletes `MainUI.log` at every boot.

## 1.0.3 - 2026-10-03

### Themes and Settings

* Theme text with a font size of 0 is hidden, as in stock: footer hints and the page counter (`hint.size`), main-menu and Games console labels (`grid.grid1x4`), Expert console labels (`grid.grid3x4`) and the screen title (`title.size`). Themes such as DS XS and Game Boy Scouts use this; 1.0.2 drew the text at a default size. See [docs/THEMES.md](docs/THEMES.md).
* Brightness, Menu sound and Sleep timer play the change sound on every Left/Right press, also at the lowest and highest value, as in stock. Menu sound clicks at the new volume, so it is silent at 0 (#10, thanks to @Amiga500).
* Large console and main-menu icons no longer take unbounded memory: Expert icons keep only the part Expert shows, an icon used for both the normal and selected look is loaded once, and all menu icons together stay within 24 MiB; an icon beyond that is left out.
* A setting that cannot be saved no longer stays half applied. Onion's keymon keeps its own live copy of volume, brightness, sleep timer and display values and writes it back to `system.json`. MainUI changed that copy first, and when `system.json` then could not be written, the device kept the new value while Settings showed the old one. Now the values this save changed are put back, unless something else changed them in the meantime. If one cannot be put back, Settings says so, and Settings and Display show keymon's current values whenever they are opened.
* A damaged `system.json` is reported once in Settings instead of "Could not save system.json." on every press. MainUI never rewrites the file.
* A setting or ROM-list position that appears twice in its file is saved properly; before, the old copy could win when the file was read back.
* Connecting to a Wi-Fi network while another connection is still being made no longer closes the password keyboard without a word. It stays open with the password, and says that Wi-Fi is busy; an open network shows the same message.

### Buttons and screens

* Delete ROM, Clear Recents and Shutdown need a separate press of A to confirm. Before, holding A for half a second after choosing one of them confirmed it through key repeat, so one long press could delete a ROM. A held A that dismisses a message no longer acts on the screen below either. See [docs/NAVIGATION.md](docs/NAVIGATION.md).
* An error shown over game details takes the buttons first: A or Back dismisses it. Before, A launched the game behind the message and Back closed the details.
* Game details open at once while a cover is still being read; the cover appears when it is ready. Before, a slow cover held up the screen and the buttons.

### Favorites and Recents

* Removing a Recent removes the selected entry when the same game is in Recents twice, played with two different emulators. Before, the other entry could disappear instead, and launching one of them removed the other from Recents.
* Removing one of two Favorites of the same game (with different names or emulators) removes the one you selected, and the other keeps its folder.
* After returning from a game or when a list is reloaded, Recents and Favorites select the right entry when the same game is listed under two emulators or names, or when Favorites have no ROM path (such as apps). A selected folder no longer jumps to such a Favorite.
* A game launched once from its console list and once from Search results is one Recent, not two.
* A game favorited from its console list is shown as a Favorite in Search results too, and the other way round. Adding it again, from any list and under any name, changes nothing instead of listing the game twice; Favorites already listed twice stay as they are. Stock compared how the path was spelled, and only the name when adding.
* Favorite stars and Remove Favorite come back without a restart when `favourite.json` could not be read at startup: they are read again whenever a section or the menu opens. Before, they stayed missing until MainUI restarted.

### Favorites folders

* When editing Favorites folders saves a repair of a damaged `favourite-folders.json` (for example a folder without a name, a duplicate folder, or folders inside each other in a loop), the damaged original is first kept as `favourite-folders.json.damaged`, which later edits never replace. Before, the only copy was `.bak`, which the next edit overwrote.
* When `favourite-folders.json` is damaged and its backup is not, folders can be edited again: the damaged file is kept as `.damaged` and editing continues from the backup, which browsing already showed. This includes a file that cannot be read as text at all, such as one filled with zero bytes by a power cut, or one over 8 MiB: it is moved aside unchanged.
* When `favourite-folders.json` is missing and only its backup is left, editing works on the whole backup. Before, the first edit kept the folders but dropped everything else in it, such as extra fields from other tools and the placement of games no longer in Favorites, and the second edit replaced the backup. A backup that cannot be used is kept as `.damaged` first; one of a newer format is left alone and the edit refused.
* Removing a Favorites folder no longer leaves two folders of the same name side by side when a folder inside it has the same name as one next to it. Rename one of them first; the message says so.

### Lists and navigation

* MainUI no longer checks its files every few seconds while a screen is open. Opening Games, Expert, Apps, Favorites or Recents reads it again instead, keeping the selected console, so changes made outside MainUI (over Wi-Fi, for example) show up there. A console whose configuration could not be read, for example while it was being copied, is found the next time Games or Expert is opened; before, it stayed missing from Games until a restart.
* Moving a Favorite (Move, then Paste) keeps the list where it is when the Favorite stays on screen, and otherwise scrolls only as far as needed. Before, the list jumped so the Favorite was on the bottom row. Pasting into an empty or short folder also keeps its ".." row in view.
* A game pasted on a Favorites folder goes into that folder, after its games, and the cursor stays on the folder. Before, it stayed in the open folder, above its other games. A folder pasted on a folder still takes its place.
* Creating the first folder in an empty Favorites folder keeps its ".." row in view.
* Editing inside a Favorites folder (Create, Rename, Sort, Delete, removing a Favorite) no longer forgets your place in the folders above it: Back returns to the row and window you entered from.
* When Favorites folders were reordered while a game ran, Back after the return selects the folder you came from, not whichever folder took its old row.
* Removing the last Favorite, Recent or ROM of a long list moves the rows above into view instead of leaving an empty row at the bottom.

### Search

* After starting Search with the X button, MainUI opens Games → Search with the results, as stock does; starting it from Apps already worked. Before, MainUI returned to the main menu: Onion starts Search this way by closing MainUI, and MainUI did not follow the place Search leaves for it (#11, thanks to @Amiga500).
* Search is recognised by its data folder instead of its name, so a renamed Search console still behaves as Search.

### Damaged files and read errors

* A ROM list whose cache has an unreadable row no longer reloads endlessly when that row comes into view. MainUI now repairs it in the background, keeping the selected row: it reloads the list, then rebuilds that console's cache as Refresh roms does if the cache itself is damaged, and as a last resort lists the console by scanning its folder for the rest of the session. A temporary read error never replaces the cache, and an unfinished ROM deletion is never set aside by this: the list is then left with a message pointing to Refresh roms. Unreadable Search results are reloaded once; if that does not help, MainUI asks you to run Search again.
* A Recents list with an unreadable line can be cleared again, and new games are added to it again; the damaged original is kept once as `recentlist.json.damaged`.
* ROM-list positions are saved again after `romwinidx.json` was damaged with zero bytes; the damaged file is kept as `romwinidx.json.bad`.
* A damaged launch-return file (`mainui-return.json` with zero bytes, or over 256 KiB) no longer blocks every later game launch until a reboot. When no launch is pending, it is moved aside as `mainui-return.json.bad` and MainUI starts at the main menu.
* Refresh roms, and the automatic repair of an unreadable list, only change the console they were started for. If it can no longer be found (its configuration removed or unreadable), they stop with a message instead of clearing the cache, and any unfinished deletion, of the console selected in its place.
* Refresh roms no longer reports a failure when it dropped a refused deletion journal but flushing the folder afterwards failed.

### Device and system

* Battery and Wi-Fi checks keep running if the system timer cannot be created.
* A Wi-Fi helper that hangs in the kernel can no longer hold up leaving MainUI or launching a game.

### Documentation and tests

* The 1.0.2 entry below said scroll speeds below 25 px/s stay exact. They do not: only 5, 6.25, 8.33 and 12.5 px/s are possible there. [docs/TIMING.md](docs/TIMING.md) lists the speed each setting gives.
* [docs/NAVIGATION.md](docs/NAVIGATION.md) describes how the cursor and the list window move, including where the console grid differs from stock on purpose: Right on the last console of a row goes to the next row, not to the next page.
* Tested on the Mini v4 and Mini Flip as well as the Mini Plus; the README no longer describes the other models as less tested.
* Tests can make one file read fail on purpose, to check recovery from read errors.
* CI also runs the tests under AddressSanitizer and UndefinedBehaviorSanitizer, and fails when a test is skipped unexpectedly.

## 1.0.2 - 2026-09-28

### Fixes

* Deleting a ROM whose cache entry was changed or removed after the list was read (for example by an external rebuild) no longer removes the ROM file while reporting it as preserved. Nothing is deleted, and the message asks to reopen the console (#6).
* A long title waiting out a long scroll delay no longer holds up battery, Wi-Fi and list checks until it starts to scroll.

### Performance

* Idle CPU use is lower: static screens are no longer redrawn every 500 ms when nothing on them has changed (#9).
* Cover images are scaled on the background decoder, so scrolling and opening lists no longer do this work on the UI thread (#8).
* Scrolling titles are much cheaper: only the selected row is redrawn and sent to the display, instead of the whole screen.
* Scrolling titles move smoothly: frames are paced on even 40 ms intervals with a constant whole-pixel step, and no longer pause for periodic full-screen repaints or Wi-Fi status checks. The `.romListTitleScroll` speed is rounded to a whole number of pixels per frame, that is to a multiple of 25 px/s (for example 120 px/s scrolls at 125 px/s). Below 25 px/s only 5, 6.25, 8.33 and 12.5 px/s are possible (for example 20 px/s scrolls at 25 px/s and 15 px/s at 12.5 px/s); see [docs/TIMING.md](docs/TIMING.md). This entry first said slow speeds stay exact.

### Other

* With logging enabled, timing figures are also written once a minute while MainUI runs, so sessions ended by the game switcher still leave them in the log. See [docs/TIMING.md](docs/TIMING.md).
* `--help` now lists `--refresh-caches` and `--version`, and neither option prints a timing report any more.
* CI no longer runs a second time for release tags.

## 1.0.1 - 2026-09-27

### Fixes

* Theme backgrounds are shown the right way up. Onion stores `skin/background.png` upside down, and stock rotates it on load; 1.0 showed it as stored (#1).
* An unreadable `Emu` (for example a file instead of a folder) no longer stops the launcher. It starts with an empty Games list and logs the reason (#5).
* When no theme font can be opened, Onion's built-in font on internal flash is used. If that fails too, the log names the missing font (#5).
* ROMs larger than 2 GiB (for example multi-disc PBP files) are no longer treated as missing (#2).
* A failure to save the ROM-list position no longer blocks launching a game (#3).
* A saved setting, list position, favourite or device request is no longer reported as failed when the file was written but flushing its folder failed. The same applies to rebuilt and removed ROM caches.
* Returning from a game keeps its list position even if removing the handoff file only partly succeeds.
* An unusable theme path in `system.json` falls back to the stock theme instead of stopping the launcher.
* Filesystem directory scans no longer follow symlink entries or other non-regular filesystem objects, and the scanned `Emu`, `App` and `RApp` folders must not be symlinks themselves. FAT cards are unaffected.
* The log names the failing step when theme audio is unavailable.
* A console or app whose `config.json` has an unusable value (for example an overlong path or `extlist`) is skipped and logged, instead of hiding the whole Games or Apps list or stopping the launcher.
* An unreadable `favourite.json` or `recentlist.json` shows a message instead of closing the launcher.
* A damaged `romwinidx.json` is moved aside to `romwinidx.json.bad` and replaced, so list positions are saved again.
* On non-FAT disks, file names containing a backslash are skipped rather than read as a path.
* When a theme font cannot be opened, the fallback font follows the language as in stock: `wqy-microhei.ttc` for non-English languages, Exo 2 for English. When a theme relies on that fallback, changing between English and another language restarts MainUI and returns to Settings, so the matching font is used at once.
* New [docs/THEMES.md](docs/THEMES.md) for theme authors: optional settings and assets, list colours, background orientation, fonts and languages, and safe margins.
* The language saved in `system.json` is used from startup. In 1.0, labels stayed in English until a language was chosen again in Settings, and after every game.
* A list row that cannot be read while a background task is running no longer closes the launcher; the list reloads when the task ends.
* Test wrapper: logging problems no longer prevent startup, and stock is only run when its backup is executable.

## 1.0 - 2026-09-26

Initial open-source release of an independent MainUI implementation for Onion on the Miyoo Mini, Mini Plus and Mini Flip.

Reimplements features from the [patched MainUI project](https://github.com/robcodedev/onionos-mainui-patcher) in an open-source launcher.

### What it does

* Console and ROM browsing against the existing `*_cache6.db` SQLite caches, including cache creation, refresh and folder navigation.
* Recents and Favorites, with folder creation, renaming, reordering and removal.
* Search, the Apps list, game launch and return, and the stock settings screens.
* Wi-Fi settings with signal strength, a marker on the connected network, and automatic rescanning while the menu is open, as in stock.
* Theme loading with fallback, and `miyoogamelist.xml` metadata for display names and box art.

### Beyond the stock launcher

* Auto-scrolling game titles.
* Configurable row count in game lists.
* Genre, rating and description from `gamelist.xml` in game details.
* Configurable main menu through `main-menu.json`, including hidden Recents and Expert sections.
* Letter jump in game lists, and a leading `..` row for going up a folder.
* Recents marks favourite games with the theme's favourite icon, as game lists do. Stock shows no icon in Recents. Display only: `recentlist.json` and `favourite.json` are unchanged.
* Sort A-Z for Favorites folders.
* Custom context menus.
* Special characters and spaces in Wi-Fi SSIDs and passwords, except double quotes in SSIDs.
* Weak networks in the Wi-Fi list show the correct signal icon; stock shows full bars for them.
* Preloads thumbnails for the two games before and after the selection.
* The selected game's thumbnail appears together with the list when opening a console, Recents or Favorites, or when returning from a game. An image that takes longer than 80 ms to load never delays opening; it appears when ready.
* Much faster ROM cache rebuilds. The ROM folder is read without checking each file individually, and the image folder is never scanned. A 5,000-ROM arcade folder with 16,000 images rebuilds in under a second, against over 5 minutes in stock MainUI and about a minute in the patched MainUI.
* Faster list navigation.
* Lower CPU use for better battery life: scrolling titles redraw at about 30 fps, the screen is not redrawn while background work such as a ROM refresh or Wi-Fi scan runs, battery and Wi-Fi status are polled every 5 s outside Settings, and each frame is presented in a single pass.
* Basic device information in Settings / About device.

### Reliability and recovery

* Missing emulator directories and blank settings files are handled gracefully. Nonempty malformed settings and existing caches are preserved on failure.
* ROM lists remain browsable when a missing cache cannot be written, with clearer errors for missing ROM folders and invalid gamelists.
* Wi-Fi power changes finish during shutdown, within at most 5 seconds. Background Wi-Fi services cannot retain MainUI's file locks and can be stopped normally.
* Memory limits for theme artwork (32 MiB) and fonts (64 MiB per file) protect against excessively large assets; rejected assets are logged.
* Long Favorites names and folder paths no longer prevent game launches.
* A console with an unreadable `config.json` is skipped instead of hiding every console.
* FAT32-compatible ROM deletion, with recovery after interrupted deletes across reboots and SD-card remounts. If recovery cannot tell which file to keep, it keeps both, and Refresh roms clears the pending state.
* Console ROM paths are confined to the SD card, with additional checks before deleting files.

### Implementation notes

* Links Onion's SQLite on the device rather than bundling a copy, which is a large part of why the binary is around 200 KB against the original's 1.4 MB.
* GPL-3.0-only, matching Onion. See [third-party notices](THIRD_PARTY_NOTICES.md) for cJSON and SQLite.

### Known limitations

* Invalid UTF-8 in miyoogamelist.xml prevents importing the file; save it as UTF-8 before rebuilding the cache.
* ROM paths containing dollar signs ($) or backticks are currently rejected for launch.
* The Max resolution row in About device can read "unknown" on some devices.
* The status bar doesn't show the hotspot icon while Wi-Fi hotspot mode is active (stock does). Onion mostly uses the hotspot during netplay, so this rarely shows.
* A single ROM folder can hold at most 65,536 entries; split larger folders into subfolders. Consoles have been tested with around 5,000 ROMs; much larger libraries (tens of thousands) may run out of memory while the cache is built.
* Testing has covered the Mini Plus more thoroughly than the Mini or Mini Flip.
* Newer labels (folder actions, Tweaks, the About device rows) appear in English whatever language is selected, because their translation IDs do not exist in Onion's language files yet. See [lang/README.md](lang/README.md).
