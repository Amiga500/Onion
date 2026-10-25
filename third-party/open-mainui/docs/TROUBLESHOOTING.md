# Troubleshooting

Start here if Open MainUI misbehaves. Bug reports are most useful when they name the device model, describe the SD card layout, and include a log.

## Getting a log

Open MainUI writes its diagnostics to stderr. How much of that you can keep depends on how it was installed:

* **With the [optional wrapper](BUILDING.md#optional-wrapper-install-with-a-stock-switch-and-logging):** output is saved to `.tmp_update/logs/MainUI.log` only when `.tmp_update/config/.logging` exists on the SD card; otherwise it is discarded. Create that empty marker file, then reproduce the problem.
* **With the [simple method](BUILDING.md#simple-method-replace-the-binaries-directly):** Onion does not save MainUI's output. Install with the wrapper if you need a log.

A log of at least 1 MiB is moved to `MainUI.log.1` at the next start, so copy the log off the card soon after reproducing the problem. Onion itself deletes `MainUI.log` at every boot, so a problem seen after a restart needs the log copied before the next one.

## Reading a log

The first line of each start names the build: `[startup] Open MainUI <version>, device <model>, sd <path>, theme <path>`. Other lines start with what they are about, one line per event:

* `[message]`: every message shown on screen, as it appears.
* `[cache]`: each ROM list cache built (from `miyoogamelist.xml` or the ROM files, with its row count and time) or removed, and why: the cache was missing, it was damaged, or Refresh roms. A failed build gives its error.
* `[scan]`: a console listed by scanning its folder because its cache could not be built or read.
* `[audio]`: the theme's background music could not start playing, with SDL_mixer's reason. It is tried again when Menu sound is raised above 0.
* `[job]`: a background read, search, refresh or repair that failed, with the console's `config.json`.
* `[menu]`: an L1 press from keymon ignored after a Menu press or release, at most 20 lines a session. If holding Menu still moves the selection and no such line appears, MainUI did not see the Menu button.
* `[return]` and `[restore]`: why the screen left for a game or app could not be shown again on return.
* `[write]`: a file that could not be saved, with the reason.
* `[settings]`: a setting that could not be saved to `system.json`, or, at start, a `system.json` too damaged to load into keymon's live settings (as stock does on every start); those are then left as they were.
* `[wifi]`: why a Wi-Fi action failed (a command wpa_supplicant refused, or a network not joined within 30 seconds), when the connection comes or goes and to which network, a saved network wpa_supplicant joined by itself and the DHCP restart for it, and saved networks enabled again. Passwords are never logged.
* `[battery]`: on the Mini Plus and Flip, Onion's `axp_test` did not answer within half a second, so the charging icon is not checked for a minute.
* `[launch]`: a Y press ignored because the Y shortcut is an app or tool (keymon's) or `keymap.json` cannot be read.
* `[delete]`: an interrupted ROM deletion resolved automatically.
* `[timing]`: timing counters; see [TIMING.md](TIMING.md).

## Wi-Fi does not connect

Settings › Wi-Fi shows stock's "Fail to connect, check your password or reboot router" at the bottom when a connection could not be made, or when the network was not joined within 30 seconds of entering its password, which is how a wrong password shows.
The notice goes away when the network is joined after all, or with the next button press. When Wi-Fi cannot be turned on, the reason is shown there instead. The log has a `[wifi]` line with the reason for each failure.

Networks you have joined stay saved, unlike stock MainUI, which keeps only the last one.
Choosing a saved network in Settings › Wi-Fi connects with its saved password, without asking; if that connection fails, choosing it again asks for the password, and the one entered is saved as a new entry beside the old one, which is kept; the newer one is used from then on.
When MainUI starts, and after you choose a network, every saved network is left able to connect, so wpa_supplicant joins whichever saved network is in reach (by its own priority rules) at home, at work, or when the current one goes away.
When it joins another one by itself while MainUI runs, MainUI starts the DHCP client again for an address on it.

## Battery drains faster with Wi-Fi connected

MainUI never changes Wi-Fi power save; `iw dev wlan0 get power_save` shows whether it is on.
When Wi-Fi is already running as MainUI starts, MainUI keeps it as it was started, power save included, and Onion's own Wi-Fi start (`update_networking.sh`) turns power save off. When MainUI starts Wi-Fi itself, as when it is turned on in Settings › Wi-Fi, the Wi-Fi driver's default applies, which is power save on.
To save battery, turn power save on with `iw dev wlan0 set power_save on`, or turn Wi-Fi off while it is not needed; with power save on, network services and netplay can respond more slowly.

## Display colours reset after a reboot (Mini Flip)

On the Mini Flip, Luminance, Hue, Saturation and Contrast from Settings › Display can be back at their defaults after a reboot, while Settings still shows the saved values.
At start, MainUI loads the four values from `system.json` into keymon's live settings, as stock MainUI does, but sets them on the screen only when one of them is changed in Settings › Display, as stock does too; changing one applies all four.
A 1.0.8 test build also set them on the screen at start: on RedOnion Beta 6 and 7 that made GameSwitcher freeze at times and the top of DraStic's screen flicker, and both went away without it, so it was taken out. Setting the saved colours at boot is left to the OS, which can do it at a point in its start-up that is safe for its display driver.

## A console lists file names instead of its gamelist titles

When a console's `miyoogamelist.xml` cannot be used at all (for example an empty file, or one with no `<gameList>`), the log says `miyoogamelist.xml is unusable (<reason>); listing the ROM files instead`, and the list is built from the file names. The XML is never changed.
A file that cannot be read because of a card or permission error is different: the log says `could not be read`, and the previous list is kept. Most sloppy files are read as they are: unescaped `&` or `<` in titles and Windows-1252 accents work. Once the file is fixed, run **Refresh roms** for that console. See [CATALOG_CACHE.md](CATALOG_CACHE.md#xml-import-checkpoint).

## A game does not start: "Unsupported launch path"

MainUI hands a game to Onion's runtime.sh as a command line with the launch script and the ROM path in double quotes, written as they are, as stock MainUI and GameSwitcher write them.
Some characters cannot pass through that: double quotes, backticks, backslashes, colons and control characters in either path, and a `$` in a launch script's path. MainUI refuses those launches with this message rather than run a different path. FAT cannot store most of them in a file name anyway.
A `$` in a ROM name works, as in `Super Game$ Collection (USA).zip`: runtime.sh escapes every `$` of a game's command before running it, so the name is used literally. It does that only when the ROM is under `Roms`, so a `$` in a ROM path outside `Roms` is refused too. Renaming the file or folder is the fix.

## Resolving an interrupted ROM deletion

Deleting a ROM first renames it to a staged name, `<rom>.mainui-delete.<16 hex digits>`, and records this in a journal next to the console's cache (`<cache>.delete.json`). If the deletion is interrupted, for example by a power cut, Open MainUI finishes or undoes it automatically the next time you enter that console.

Automatic recovery refuses when it cannot tell which file is the right one, for example when a ROM with the original name has appeared again, the staged copy differs from the recorded size, or the journal is unreadable. Recovery never guesses, because a wrong guess could delete a ROM you want to keep. Until the journal is resolved:

* you can still browse and launch games in that console;
* Delete is blocked for that console, and its error names the staged file, or the journal if it cannot be read. Long paths show their trailing portion.

To continue, run **Refresh roms** for that console, from inside its list or from the console selector. Refresh roms removes only the journal and rebuilds the list from the files on the card. It never deletes or renames a ROM, so the worst case is an extra copy left on the card:

* the ROM you were deleting may still exist under its staged name. Staged files are not shown in the list.
* if a file with the original name exists, it stays and is listed normally.

Afterwards, look in that console's ROM folder for files ending in `.mainui-delete` or `.mainui-delete.` plus 16 hex digits. To keep one, rename it back to its original name, removing that suffix, and run Refresh roms again.
Otherwise delete it with a file manager or on your computer. If both copies exist, compare them before removing either. With [logging enabled](#getting-a-log), the log names the staged file when recovery refuses and again when Refresh roms removes the journal.

How the journal, staging and automatic recovery work is described in [RESOURCE_LIMITS.md](RESOURCE_LIMITS.md#rom-deletion-recovery).
