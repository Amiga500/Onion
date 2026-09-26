# 🧅⚡ OnionPlus

### The same Onion you know — with its hot paths rebuilt.

**OnionPlus is a personal, independent build of [Onion](https://github.com/OnionUI/Onion)
(`4.4.0-beta`)** for the Miyoo Mini, Mini+, Mini v4 and Mini Flip. It is not a replacement
for Onion and not an official release: it is an experiment in how far Onion's everyday
paths can be optimized — **what happens every time you press a key, launch a game, go back
to the menu, put the device to sleep, or change the volume** — shared in the hope that some
of it is useful to the Onion team. It keeps Onion's look, menus, emulators and file layout.
Everything here is built on their work; any change they find worthwhile is theirs to take.

> 🚀 **This build boots to the menu in ~1.7 s** on a Miyoo Mini+ — with Wi-Fi on (5.51 s at the
> first measurement, **3.2× faster**) and with Wi-Fi off + "Enable Wi-Fi temporarily" (5.16 s
> before) · ⏱️ **~0.5 s** of its own work around each game · 🐛 **25** issues found in the
> shared codebase, with fixes ready for Onion · 🧪 **1,423** host tests.

[![branch](https://img.shields.io/badge/branch-onionplus--compact-8A2BE2?style=for-the-badge&logo=git)](https://github.com/Amiga500/Onion/tree/onionplus-compact)
[![commits](https://img.shields.io/badge/commits-41-blueviolet?style=for-the-badge)](#-11--commit-timeline)
[![files](https://img.shields.io/badge/files%20changed-188-blue?style=for-the-badge)](#-10--grand-totals)
[![neon](https://img.shields.io/badge/NEON%20kernels-8-orange?style=for-the-badge)](#️-1--vectorized-pixel-paths-neon)
[![tests](https://img.shields.io/badge/tests-1%2C423%20%2F%2071%2C436%20assertions-success?style=for-the-badge)](#-8--testing--the-safety-net)
[![ota](https://img.shields.io/badge/updates-OTA%20enabled-2ea44f?style=for-the-badge)](#️-9--build-ci--release)
[![fixes](https://img.shields.io/badge/fixes%20ready%20for%20Onion-25-critical?style=for-the-badge)](#️-6--security--memory-hardening)
[![boot](https://img.shields.io/badge/boot%20to%20menu-~1.7%20s%20(Mini%2B)-brightgreen?style=for-the-badge)](#-measured-on-a-miyoo-mini)
[![status](https://img.shields.io/badge/status-ALL%20GREEN-brightgreen?style=for-the-badge)](#-final-word)

> 📡 **OnionPlus ships and updates itself over-the-air** — `ota_update.sh` checks
> `Amiga500/Onion` releases directly on-device, so every optimization and hardening
> pass below reaches installs without a manual re-flash. See
> [§9 · Build, CI & release](#️-9--build-ci--release) for the wiring.

---

## 🗺️ Table of Contents

**Why OnionPlus**

| | Section |
|:--|:--|
| 📊 | [At a glance](#-at-a-glance) |
| ✨ | [What you'll notice](#-what-youll-notice) |
| 📱 | [Measured on a Miyoo Mini+](#-measured-on-a-miyoo-mini) |
| ⏱️ | [Every action, before and after](#️-every-action-before-and-after) |
| 🔌 | [Your settings survive power cuts](#-your-settings-survive-power-cuts) |
| 🧠 | [Memory that stays free](#-memory-that-stays-free) |
| 📦 | [Install & update](#-install--update) |
| 🧭 | [Known issues & next steps](#-known-issues--next-steps) |
| 🤝 | [Credits & giving back](#-credits--giving-back) |
| 🔬 | [How these numbers were obtained](#-how-these-numbers-were-obtained) |

**Technical reference**

| | Section |
|:--|:--|
| 🎯 | [Why this document exists](#-why-this-document-exists) |
| 🖼️ | [1 · Vectorized pixel paths (NEON)](#️-1--vectorized-pixel-paths-neon) |
| ⚡ | [2 · Algorithmic wins (O(n²) → O(n))](#-2--algorithmic-wins-on²--on) |
| 🎨 | [3 · Rendering & UI caches](#-3--rendering--ui-caches) |
| 🔋 | [4 · Power, battery & idle CPU](#-4--power-battery--idle-cpu) |
| ⚙️ | [5 · Process & syscall diet](#️-5--process--syscall-diet) |
| 🛡️ | [6 · Security & memory hardening](#️-6--security--memory-hardening) |
| 🕹️ | [7 · AdvanceMENU frontend](#️-7--advancemenu-frontend) |
| 🧪 | [8 · Testing safety net](#-8--testing--the-safety-net) |
| 🏗️ | [9 · Build, CI & release](#️-9--build-ci--release) |
| 📊 | [10 · Grand totals](#-10--grand-totals) |
| 🔀 | [11 · Commit timeline](#-11--commit-timeline) |
| ✅ | [Final word](#-final-word) |

---

## 📊 At a glance

> 🟥🟧🟩 Counted from the code of both projects or measured — never estimated.

Every number below is either **counted from the source code of both projects** or
**measured** (on a Miyoo Mini+ or on a host machine), and says so. Nothing here is an estimate dressed up as a
benchmark. The comparisons are here to show **what could be brought back to Onion**, not to
rank the two.

### 🚀 Speed

| | ⚪ OnionUI | 🟢 OnionPlus | 💬 What it means |
|:--|:-:|:-:|:--|
| 🔌 Onion boot to the menu (Mini+) | 3 blocking waits | **1.74 s** | 📱 **3.2× faster** than the first measured build with Wi-Fi on (5.51 s); 1.78 s with temporary Wi-Fi (5.16 s) |
| ⏱️ Onion's own work around a game | — | **~0.5 s** | 📱 launch 0.16 s · exit 0.1 s · back to the menu 0.1 s |
| 🎮 Menu CPU while idle | 100% of a core | **sleeps** | 🔋 GameSwitcher, Tweaks, Play Activity & co. stop heating the device |
| 🌙 Blue-light schedule, every 15 s | ~20 processes | **0** | ⌨️ checked in-process: no more key freezes of up to 4 s |
| 🧩 Processes to parse a game launch | ~25 | **~2** | 🚀 **−90%** between pressing A and the emulator starting |
| 💾 SD-card writes per volume step | ~14 | **1** | 🚀 **−93%**: 12 fewer flushes to the card on every press |
| 🔁 SD-card flushes per return to the menu | 2 | **1** | ⬇️ **−50%** after every game |
| 📊 Opening Play Activity (60,000 sessions) | 66 ms | **34 ms** | 📏 **2× faster**, identical results |

### 🛡️ Reliability & quality

| | ⚪ OnionUI | 🟢 OnionPlus | 💬 What it means |
|:--|:-:|:-:|:--|
| 🐛 Issues found in the shared code | — | **25 fixed** | ✅ crashes, leaks, lost settings, a wrong clock — fixes available for Onion, [listed in §6](#️-6--security--memory-hardening) |
| ⚡ Settings that survive a power cut mid-write | none | **all** | ✅ `system.json`, key map, config values, JSON, recent games |
| 🧠 Memory leaked per MainUI-cache lookup | ~570 KB | **0** | 📏 was tens of MB with a large GameSwitcher history, on a 128 MB device |
| 🧪 Automated tests | 1 | **1,423** | 🚀 **×1,400**: 71,436 assertions, run on any PC in ~2.5 s |
| 🖼️ NEON (SIMD) pixel kernels | 0 | **8** | 🆕 vectorized pixel conversion, rotation and alpha |
| 🐚 `system()` calls in the C code | 73 | **46** | ⬇️ **−37%** shells spawned |
| ⚠️ Unbounded string calls | 347 | **235** | ⬇️ **−32%**; none left in the hardened core |

> 📱 measured on a Miyoo Mini+ · 📏 measured on a host machine · everything else counted
> from the code of both projects (see [how these numbers were obtained](#-how-these-numbers-were-obtained)).

---

## ✨ What you'll notice

> 🟩 Things you see and feel on the device. Nothing to configure.

- ⚡ **It boots to the menu in under two seconds of Onion time**, with Wi-Fi on or off. Wi-Fi,
  network services and time sync now come up in the background a few seconds after the
  menu, instead of holding the boot (unless you ask for **Wait for sync on startup**).
- 📶 **"Enable Wi-Fi temporarily" now really sets the clock.** With Wi-Fi off it used to turn
  Wi-Fi on and off at boot without syncing anything; now it turns Wi-Fi on in the
  background, syncs the time, and turns it off again.
- 🌡️ **Menus stop heating the device.** In stock Onion, the GameSwitcher, Tweaks, Play
  Activity, Themes, Package Manager and Battery Monitor keep one CPU core at 100% even
  when you are just looking at them. OnionPlus sleeps until the next frame.
- 💾 **Volume and brightness don't hammer the SD card.** One step used to rewrite about
  fourteen files, each flushed to the card. Now it is one file, written 0.5 s after your
  last press, safely.
- 🔊 **Your volume is actually remembered.** In stock Onion a change is saved only on the
  first key press after the next 15-second tick; switch off before that and it's gone.
- 🧠 **The GameSwitcher doesn't eat memory.** Every recent game triggered a lookup that
  leaked about half a megabyte on large collections. Fixed, and screenshots are now
  decoded in the background with the neighbours preloaded.
- 🌙 **No more freezes when the blue-light schedule kicks in.** Keymon used to run a script
  every 15 seconds and ignore your keys while it ran — up to ~4 s during a transition.
- 🔌 **Settings survive a dead battery.** `system.json`, the key map and every config file
  are now written to a temporary file, flushed, and swapped in atomically.
- 📋 **Your recent games list stays correct.** Stock Onion can delete the wrong game from
  the recents when an entry is longer than 1 KB; OnionPlus counts lines the same way
  everywhere.
- 🕒 **The clock and time zone stay right.** A failed time-zone lookup no longer resets your
  zone to UTC, and a successful sync is not repeated after every game.
- 🎮 **Play time can't jump by decades.** Setting the clock from the network could add the
  whole jump (from 1970, on a fresh device) to the game being played; it can't anymore.
- 🚀 **Faster launches, faster returns.** Dozens of helper processes removed from the path
  between pressing A and seeing the game, and between quitting and seeing the menu.

---

## 📱 Measured on a Miyoo Mini+

> 🟦 Real numbers from a real device, taken with the built-in timing log.

Miyoo Mini+ (640×480 panel), logging on, network time on, "Enable Wi-Fi temporarily" on,
"Disable services in game" on.
Each session: boot, three games launched from MainUI (PlayStation, NES, SNES, BS-X, …)
and quit from the RetroArch menu, GameSwitcher opened once. Numbers come from the
[timing log](#️-measuring-on-the-device).

### 🔌 Boot with Wi-Fi on, step by step

| Phase | First measurement<br>`1592866e` | Wi-Fi off the boot<br>`10f2369e` | **Faster `boot_init`**<br>**`29791efc`** |
|:--|--:|--:|--:|
| 🐧 Kernel → Onion *(firmware, not Onion)* | 3 s | 2 s | 2 s |
| ⚙️ `boot_init` | 1.53 s | 1.59 s | **1.05 s** |
| 📶 `boot_network` | 3.34 s | **0.09 s** | 0.09 s |
| 🏁 **`boot` — Onion start → menu** | **5.51 s** | **2.28 s** | **1.74 s** |

```
Onion boot to the menu, Miyoo Mini+

first measurement        ██████████████████████████████████████  5.51 s
Wi-Fi off the boot path  ████████████████                        2.28 s
faster boot_init         ████████████                            1.74 s   (3.2× faster)
```

Where the remaining `boot_init` goes: swap **0.01 s** (now in the background), audio server
**0.29 s**, display detection **0.39 s**, the rest **~0.36 s** (of which 0.25 s is a fixed
wait after display init, kept on purpose).

### 📴 Boot with Wi-Fi off and "Enable Wi-Fi temporarily" on

| Phase | Before<br>`29791efc` | **After**<br>**`9768ae02`** |
|:--|--:|--:|
| 📶 `boot_network` | 3.50 s | **0.12 s** |
| 🏁 **`boot` — Onion start → menu** | **5.16 s** | **1.78 s** |
| 🕒 Time synced at boot? | **no** (Wi-Fi on and off again, nothing synced) | **yes**, in the background |

On the device, Wi-Fi came on right after the menu, the router took **29 s** to hand out an
address (within the new 30 s background limit; the old 10 s limit would have failed), the
time was synced through `ntpdate`, and Wi-Fi was turned off again — all while the menu was
already usable.

### 🎮 Around every game

| Phase | Measured (all sessions) | What it covers |
|:--|--:|:--|
| 🎮 `game_prepare` | **0.15–0.18 s** | from pressing A to the emulator starting |
| 🚪 `game_exit` | **0.09–0.27 s** | from quitting the emulator to the post-processing done |
| 🏠 `mainui_prepare` | **0.04–0.20 s** | before MainUI starts (battery icon drawn only when it changed) |
| ↩️ `mainui_return` | **0.06–0.10 s** | after MainUI exits (2.2 s when Wi-Fi was just turned on from the menu — under investigation) |
| 🔀 `switcher_prepare` | **0.01–0.02 s** | before the GameSwitcher starts |

- ⏱️ **Onion's own work around a game is about half a second in total.** The rest of the wait is
  RetroArch and the core loading the game, and MainUI starting, which is a closed binary.
- 📶 **Network in the background:** Wi-Fi, SSH/FTP/HTTP/Samba/Telnet and time sync arrive about
  three seconds after the menu is on screen; nothing waits for them.
- 🙋 **One device so far.** Post your `timing.log` (Mini, Mini v4, Mini Flip especially) in an issue
  to extend these tables.

---

## ⏱️ Every action, before and after

> 🟧 What the system does behind the scenes for each thing you do, counted from the code of
> both projects.

### 🔌 Powering on

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 📶 Wi-Fi and network services | brought up **inside** the boot (fixed 2 s wait) | ✅ **in the background** (unless Wait for sync is on) |
| 📴 "Enable Wi-Fi temporarily", Wi-Fi off | Wi-Fi on and off at boot, **no time sync** (3.4 s lost) | ✅ **background: on → sync → off** |
| 💾 128 MB swap file | `swapon` before the boot continues | ✅ **in the background** |
| 🔊 Audio server | fixed 0.5 s wait after starting it | ✅ **checked every 50 ms** (512 → 61 ms 📏) |
| ☀️ Brightness setting | `jsonval` process | ✅ **shell built-ins** |
| 🥾 `system.json` | rewritten in place | ✅ **temp file → flush → rename** |

### 🕒 Setting the clock from the network

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 🎮 Game being played during the change | `ntpdate` adds the whole jump to it (decades from 1970) | ✅ **session closed and reopened**; >24 h discarded |
| 🌍 Time-zone lookup fails | zone rewritten as **UTC** | ✅ **your zone is kept** |
| 🔁 After a successful `ntpdate` | synced again after every game | ✅ **marked as done** |
| ⏳ Waiting for an IP address at boot | 10 s, then the sync is skipped | ✅ **30 s in the background** (a router took 29 s 📱) |

### 🔊 Pressing volume or brightness

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 📄 Files written | ~14 (12 config values + key map + `system.json`) | ✅ **1** (`system.json`, only fields that changed) |
| 💾 `fsync` to the SD card | 13 | ✅ **1** |
| 🗂️ Extra FAT metadata operations | 12 flags × create + delete, 5 deletes | ✅ **0** |
| ⏲️ When it is saved | on the first key press **after** the next 15 s tick | ✅ **0.5 s after your last press** |
| 🔁 After saving | keymon re-reads all its own settings, then flushes every filesystem | ✅ nothing |

### ⌨️ Every key press (keymon)

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 💾 Global `sync()` after touching flags in `/tmp` (RAM) | yes | ✅ **no** |
| 🔎 Config flags checked on the SD card | up to 3 `stat` calls | ✅ **0** (cached) |
| ⚡ CPU-clock hotkey | spawns `cpuclock` twice, overflows a 5-byte buffer | ✅ **once, no overflow** |
| 🐚 Shell spawns (`touch`, scripts, `playActivity`) | via `system()` | ✅ **direct `fork`/`exec`** |

### 🌙 Every 15 seconds, with the blue-light schedule on

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| ⚙️ Work done | runs `blue_light.sh check`: 2 global syncs, ~20 processes | ✅ **compares two times in C** |
| ⌨️ Keys ignored while it runs | yes, up to ~4 s on a transition | ✅ **never** |

### 🎮 Launching a game

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 🐚 Helper processes to parse the launch command | ~25 (`echo`/`grep`/`awk`/`sed`…) | ✅ **~2** |
| 💾 `cmd_to_run.sh` rewritten on the SD card | **every launch** (a check that is always true) | ✅ only when the path contains `$` |
| 🔊 Audio-server volume computed | every launch (3 processes) | ✅ only if the server isn't running |
| 🗃️ Play-history cache query | full scan of the MainUI cache, every launch | ✅ **only for games never seen before** |
| 📂 Play-history database opened | twice | ✅ **once**, indexed by path |
| 🖥️ Mini+/Mini Flip 560p check | 4–5 processes, command file read 2–3× | ✅ **0** |
| 🔁 Global syncs in the main loop | 4 per loop | ✅ **1**, when a game or app exits |

### 🏠 Returning to the menu

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 🔋 Battery icon | decoded, rendered and **written to the SD card every time** | ✅ **only when the theme or percentage changed** |
| 💾 Global SD flushes | 2 | ✅ **1** |
| 🔎 Checking which MainUI is mounted | ~6 processes | ✅ **0** |
| 📄 Reading `system.json` (Wi-Fi ×2, theme) | 3 `jsonval` processes | ✅ **0** |
| 🖥️ Mini+/Mini Flip framebuffer probe parsing | 8 processes | ✅ **0** |

### 😴 Sleep and wake

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 📊 Closing the play session | shell + full scan of the play history | ✅ **no shell, indexed, one transaction** |
| ⏱️ Host timing at 60,000 sessions | 10.8 ms | ✅ **5.0 ms** 📏 |
| 🧾 Process list parsing | breaks on names with spaces; one slot past the array | ✅ **robust parsing, bounded** |

### 🌐 Leaving a game with SSH / FTP / Samba / HTTP / Telnet enabled

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 💾 Global syncs when services restart | up to 5 | ✅ **0** |
| 📶 `jsonval` processes to read the Wi-Fi setting | 8–10 | ✅ **1** |

### 🔁 Recent games list

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 🧹 Removing duplicates when the GameSwitcher opens | one full file rewrite **per duplicate** | ✅ **one rewrite total** |
| ⚡ Quick switch (move a game to the top) | two full rewrites, a moment with no file at all | ✅ **one atomic rewrite** |
| 🔢 Line numbering | three functions counting lines three different ways | ✅ **one way everywhere** |

---

## 🔌 Your settings survive power cuts

> 🟥 A flat battery at the wrong moment used to cost you your settings.

The Miyoo Mini has no battery-backed shutdown: a flat battery or a yanked cable can stop
the system in the middle of a write. In stock Onion, these files are truncated first and
rewritten afterwards — a cut in between leaves them **empty**.

| File | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| ⚙️ `system.json` (volume, brightness, theme, Wi-Fi…) | truncated, then rewritten | ✅ **temp file → `fsync` → atomic rename** |
| 🥾 `system.json` at every boot | rewritten in place; an empty per-device file **wipes it** | ✅ **only replaced by a complete, non-empty file** |
| 🎮 `keymap.json` | truncated, then rewritten | ✅ **atomic** |
| 🔧 Every `config/` value | truncated, then rewritten | ✅ **atomic** |
| 🧾 Every JSON written by the system (`json_save`) | truncated, then rewritten | ✅ **atomic** |
| 📋 Recent games list | deleted, then renamed | ✅ **atomic** |

Symlinked files are resolved first, so the link itself is never replaced.

---

## 🧠 Memory that stays free

> 🟥 Leaks that grew with your collection, on a device with 128 MB of RAM.

| Leak | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 🗃️ MainUI cache lookup (GameSwitcher names, new games) | SQLite connection leaked **every lookup** — ~570 KB + 1 descriptor on a 5,000-game cache 📏 | ✅ **0** |
| 🗃️ Cache lookup with no cache database | name buffer leaked | ✅ **0** |
| 📦 Package Manager, per emulator package | whole parsed config leaked (`free` instead of `cJSON_Delete`) | ✅ **0** |
| ⚙️ System property save | JSON tree leaked when the value was unchanged | ✅ **0** |
| 🖼️ GameSwitcher screenshots | loader thread never joined; surfaces freed from two threads | ✅ **single owner, joined at exit** |

On a 128 MB device with the GameSwitcher showing up to 100 recent games, the first leak
alone could grow to tens of megabytes.

---

## 📦 Install & update

> 🟩 Same install as Onion; updates arrive on the device.

| | |
|:--|:--|
| 🎮 Devices | Miyoo Mini, Mini+ (measured above), Mini v4 and Mini Flip |
| 🧅 Base | OnionUI `4.4.0-beta` — themes, emulators, ROM folders and saves stay where Onion keeps them |
| 📡 Updates | built-in OTA from [`Amiga500/Onion` releases](https://github.com/Amiga500/Onion/releases) (`OnionPlus-v…` assets); **stable** follows the latest release, **beta** installs prereleases only |
| 🏷️ Releases | `OnionPlus-v4.4.0-beta-YYYYMMDD-<commit>`, built by GitHub Actions |
| ⏱️ Timing log | Tweaks → Advanced → Diagnostics → **Enable logging** → `.tmp_update/logs/timing.log`; **Util: System log snapshot** packs all logs into `SD:/log_export.7z` for sharing |
| 📖 Settings reference | Onion's own documentation: [Tweaks](https://onionui.github.io/docs/apps/tweaks) |

## 🧭 Known issues & next steps

> 🟧 What is known, and what comes next.

- 🌐 **Network service toggles are never switched off by the network script** (an OnionUI
  defect: `disable_flag` builds the wrong file name). Services still come back after games
  and Wi-Fi changes; a fix is ready to be tested on a device.
- 🔌 **Boot:** display detection (0.39 s) and audio-server start (0.29 s) are the largest
  parts left of the ~1.7 s Onion boot; the ~2 s before Onion starts belong to the firmware.
- 🕒 **Time sync:** on networks where the web time services fail, the time comes from
  `ntpdate` a few seconds later. It no longer blocks anything.
- ↩️ **Return to the menu after turning Wi-Fi on from MainUI** takes ~2.2 s instead of ~0.1 s
  (seen twice); a global `sync` or `freemma` is the suspect, finer timing marks will tell.
- 🔋 **Mini Flip:** lid/Hall-sensor handling is unchanged and still to be confirmed on hardware.
- 📏 **Measurements:** on-device numbers come from one Mini+ so far.

## 🤝 Credits & giving back

> 🟩 Built on Onion, offered back to Onion.

- 🧅 **OnionPlus exists because of [Onion](https://github.com/OnionUI/Onion)** and the
  OnionUI team and contributors: the menus, the emulator setup, the themes and almost all of
  the code are theirs. This build is not affiliated with or endorsed by the Onion team. For
  what each setting does, see [Onion's documentation](https://onionui.github.io/docs).
- 🙏 Thanks to **@robcodedev**, whose still-open Onion pull requests #1936–#1946 are carried here.
- 📬 **For the Onion team:** if any of these changes would be useful as pull requests, I'm
  happy to split them out and adapt them to Onion's own branches. The fixes that apply to
  Onion as it is today — the MainUI-cache memory leak, the time zone and play time after a
  clock change, Wi-Fi no longer holding the boot, the recent-list line numbering — are the
  first candidates.

## 🔬 How these numbers were obtained

> 🟩 Every figure can be checked by anyone with a checkout of both projects.

| Mark | Meaning |
|:--|:--|
| *(none)* | Counted directly from the source code of `OnionUI/Onion:main` and of this branch |
| 📱 | Measured on a Miyoo Mini+ with the built-in timing log |
| 📏 | Measured on a host machine (x86 Linux, real SQLite, same C code) |

Reproduce the static counts from a checkout of each project:

```sh
# system() calls in C code (comments excluded)
find src -name '*.[ch]' | xargs sed 's#//.*##' | grep -cE '(^|[^_a-zA-Z])system\('
# unbounded string calls
find src -name '*.[ch]' | xargs sed 's#//.*##' | grep -cE '(^|[^_a-zA-Z])(sprintf|strcpy|strcat|strtok)\('
# host test suite
make unit-test
```

**Not claimed here:** frame rates or battery life. The changes remove work from the paths
that decide them, but this page only publishes numbers that were counted or measured.

---

> 📚 **Technical reference.** Everything below documents each change by category, with
> commit links, for reviewers and maintainers.

## 🎯 Why this document exists

OnionPlus is measured against **[`OnionUI/Onion:main`](https://github.com/OnionUI/Onion/tree/main)**
at merge-base [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) (`4.4.0-beta`).
That is the comparison that matters for this build. Some NEON kernels and early hardening
were first written elsewhere; code-level **percentages in this document are OnionPlus vs
`OnionUI/Onion:main`**, not vs that sibling branch (on-device timings compare OnionPlus
builds, see the icon legend below).

The integration branch is **[`onionplus-compact`](https://github.com/Amiga500/Onion/tree/onionplus-compact)**:
the long `OnionPlus` history (97 commits to [`fa5bb007`](https://github.com/Amiga500/Onion/commit/fa5bb007))
squashed into topic commits, then the **@robcodedev** ports of still-open
[`OnionUI/Onion` PRs #1936–#1946](https://github.com/OnionUI/Onion/pulls?q=1936)
([Amiga500 #217](https://github.com/Amiga500/Onion/pull/217)), and the **2026-09-09**
review fixes ([`fbd26d06`](https://github.com/Amiga500/Onion/commit/fbd26d06) list-cache
dimming + installer Mini Flip detect; [`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e)
Mini Flip 640 lock, `fbmode` before the FB driver, AXP `percBat` clamp).
`git rev-list --count 07505ea5..HEAD` on this branch is the **compact** count, not 97.

On top of `07505ea5` the branch carries, oldest first:

- 🔋 the power/CPU batch, the security review and the hot-path passes;
- 🕹️ an **AdvanceMENU** frontend pass;
- 📱 a **surgical Miyoo Mini Flip port** from `OnionUI/Onion:v4.5-dev` that does **not** merge that branch;
- 🔎 an **OnionUI-parity review** (charging-icon sentinel, RetroArch `killall` semantics, path
  bounds, rumble GPIO retry) and the **2026-09-01 independent review** (findings A–G);
- ⚡ the **2026-09-25 performance passes**: [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804) (atomic I/O, in-process
  blue-light schedule, GameSwitcher preload worker, idle UI loops, parallel `make`),
  [`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21) (SQLite connection leak, recent-list numbering, suspend index, launch
  parsing), [`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8) (return to MainUI, battery icon, quick switch, networking check)
  and [`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e) (on-device timing log, fork-free launch helpers, crash-safe
  `system.json` at boot, Play Activity / Package Manager fixes);
- 📱 four rounds of fixes driven by the on-device timing log: [`874ea325`](https://github.com/Amiga500/Onion/releases/tag/OnionPlus-v4.4.0-beta-20260925-874ea325),
  [`10f2369e`](https://github.com/Amiga500/Onion/commit/10f2369e), [`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc), [`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02).

Every pass reaches installs through the built-in **OTA updater** (`Amiga500/Onion`, assets
`OnionPlus-v…`). The last **code** commit is [`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02). This README is the **single
reference** for the branch (the former `docs/` reports were retired with it) and groups
**everything shipped to date** by *category* rather than by commit.

### 🔑 Reading the icons

| Icon | Meaning |
|:---:|:---|
| 🟥 | **Order-of-magnitude** win — vectorized paths, quadratic → linear, busy-wait removal |
| 🟧 | **Structural** win — a syscall/shell-out/scan eliminated on a repeatable path |
| 🟨 | **Incremental** win — smaller but still measurable saving |
| 🟦 | **Robustness** — correctness / memory-safety fix, **no performance claim** |
| 🟩 | **Quality floor** — tests, CI, tooling |
| 📏 | **Measured on a host machine** — OnionPlus code path vs the `OnionUI/Onion:main` equivalent |
| 📱 | **Measured on a Miyoo Mini+** with the built-in timing log |
| 📐 | Figure **estimated analytically** (algorithmic complexity / syscall count) |
| 🧪 | **Verified by unit test** in this repository |
| 🧪* | Verified by an **out-of-tree** host check (see §8), not yet in `test/` |
| 🛡️ | Correctness/safety fix carrying **no** performance claim |

> ⚠️ Percentages compare **OnionPlus code vs `OnionUI/Onion:main`** (`07505ea5`) and are 📏
> host-measured or 📐 analytical (complexity / syscall count / files written, read from the
> source). The 📱 on-device figures (boot, per-game timings) compare **OnionPlus builds on
> the same Miyoo Mini+**; OnionUI itself has not been timed on the device.

---

## 🖼️ 1 · Vectorized pixel paths (NEON)

> 🟥 The single biggest performance category — whole scalar loops replaced by ARM NEON
> vector kernels, each with a scalar C fallback so non-NEON builds still work.

| Kernel | What it replaced | Speedup | Evidence |
|:--|:--|--:|:--:|
| 🔄 `neon_rotate180_inplace` | rotozoom blit + extra surface alloc | 🚀 **+5000%** | 📏 |
| 🎨 `neon_swap_rb_inplace` | scalar per-pixel loop | 🚀 **~+800%** | 📏 |
| 🎨 `neon_argb_to_rgba` | scalar per-pixel loop | 🚀 **~+800%** | 📏 |
| 🎨 `neon_rgb888_to_argb` | scalar per-pixel loop | 🚀 **~+800%** | 📏 |
| ⚪ `neon_gray8_to_argb` | scalar per-pixel loop | 🚀 **~+600%** | 📏 |
| ⚪ `neon_gray8a_to_argb` | scalar per-pixel loop | 🚀 **~+500%** | 📏 |
| 🎨 `neon_argb_to_rgba_alpha` | scalar per-pixel + branch | 🚀 **~+600%** | 📏 |
| 🌫️ `surfaceSetAlpha` (NEON intrinsics) | float mul + `SDL_GetRGBA` | 🚀 **~+400%** | 📏 |

- 📦 **8 kernels total** (7 hand-written ARM assembly + 1 NEON intrinsics), all guarded by
  `#ifdef __ARM_NEON` with a correct scalar tail loop for the remainder.
- 🧪 Backed by `test_neon`, `test_neon_pixel` and `test_alpha_scale` — **109 tests /
  67,353 assertions** cross-checking NEON output against the scalar oracle.
- 🔬 Every scalar fallback is exercised on the x86-64 host CI; a separate `neon-arm` job
  cross-compiles the assembly and runs it under `qemu-user`.

---

## ⚡ 2 · Algorithmic wins (O(n²) → O(n))

> 🟥🟧 String and path handling rewritten to drop a re-scan hidden inside a loop.

### 🔤 Strings & paths

| Function | Before | After | Class | Evidence |
|:--|:--|:--|:--:|:--:|
| `str_count_char` | `strlen()` re-evaluated every iteration | single pointer walk | O(n²)→O(n) 🚀 **−90%** | 📏🧪 |
| `file_removeExtension` | `strlen` + `strcpy` rescans | one scan + length-known `memcpy` | **−50% scans** | 📏🧪 |
| `file_path_relative_to` | `strcat` loop rescanning from byte 0 | explicit `offset` + `memcpy` | O(n²)→O(n) | 📏🧪 |
| `file_resolvePath` | `strcat` loop per path component | bounds-checked `memcpy` at `offset` | O(n²)→O(n) | 📐🧪 |
| `file_read()` | `fopen`+`fseek`×2+`ftell`+buffered `fread` | `stat64` + one `read()` loop | **2 seeks removed** | 📐🧪 |

### 🗃️ State & databases

| Function | Before | After | Class | Evidence |
|:--|:--|:--|:--:|:--:|
| 🔎 `system_state_update()` | one full `/proc` scan **per candidate** (up to 5) + a `cmd_to_run.sh` read per check | single `/proc` pass for all candidates, file read once | **5 scans → 1** | 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🗃️ playActivity `rom` lookup by `file_path` | full table scan on every start/stop | `rom_file_path_index` (created on open, no-op once present) | O(n)→O(log n) | 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🗃️ MainUI cache query on game start | `LIKE '%…' OR disp` full scan of the cache DB on **every** launch, known ROM or not | run only for rows still missing type/name | **−1 full scan per launch** | 📐🧪* ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🗃️ `playActivity stop_all` (before every suspend) | full scan of `play_activity` (one row per start **and** per resume) + two transactions | `play_activity_play_time_index` + one transaction | O(n)→O(log n), 2 → 1 journal cycles | 📐🧪* ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)) |
| 📊 Play Activity app, `play_activity_find_all()` | the GROUP BY over the whole play history ran **twice** (count, then `sqlite3_reset()` and read) | one pass into a growing array | 🚀 **66 → 34 ms** on host, 60,000 sessions, identical results | 📏🧪* ([`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e)) |
| 🎮 GameSwitcher `readHistory()` duplicates | a full rewrite of the recent list **per duplicate** | duplicates collected, removed in **one** atomic rewrite (`file_delete_lines`) | O(d·n)→O(n) | 📐🧪* ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)) |

### 🕹️ AdvanceMENU scripts

| Function | Before | After | Class | Evidence |
|:--|:--|:--|:--:|:--:|
| 🕹️ `move_Roms_Without_Preview.ps1` | rescans the Snaps folder per ROM | Snaps folder read **once** into a lookup set | O(n²)→O(n) | 📐 |
| 🕹️ `move_incompatible_Roms.ps1` | linear XML scan per ROM | ROM names indexed into a hashtable | O(n²)→O(n) | 📐 |

> 🎁 **Bonus fixes bundled in:** `str_count_char` also closed a 1-byte over-read
> (`i <= strlen`); the path-assembly rewrites add `PATH_MAX` bounds checks the old
> `strcat` versions lacked; the PowerShell hashtable rewrite also fixes a **crash on ROM
> names containing an apostrophe**.

---

## 🎨 3 · Rendering & UI caches

> 🟥 Redrawing the same pixels every frame is the classic "free win" — cache it once,
> invalidate on change.

| Cache | Before | After | Impact |
|:--|:--|:--|:--|
| 🔤 TTF label / list / footer / header / dialog surfaces | `TTF_RenderUTF8_Blended` on **every frame** | hash-invalidated cached `SDL_Surface`; hidden-row dim uses a `SDL_ConvertSurface` copy (`_blit_cached_label`) so `surfaceSetAlpha` never mutates the cache | 🚀 **5–15 ms/frame saved** 📏 |
| 🖼️ infoPanel `drawImage()` | `zoomSurface()` + free on **every redraw** | scaled surface cached per (source, w, h) | O(w·h) scale eliminated on repeats 📐 |
| 🎮 playActivityUI page render | 4× `IMG_Load`+`SoftStretch`+alloc **per page flip** | 4 surfaces cached, reloaded only on page change | page flips skip all image I/O 📐 |
| 🖥️ `display_readOrWriteBuffer` | per-pixel loop on every row | `memcpy` fast path for contiguous rows | row copy vectorized 📐 |
| 🔋 MainUI battery icon (`mainUiBatPerc`, every return to MainUI) | theme background decoded, icon rendered, PNG **encoded and written to the SD card** every time | skipped while theme, percentage and theme/override files are unchanged (key in `/tmp`); never rendered while charging | full decode + encode + SD write → **0** when unchanged 📐🧪* ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)) |
| 🎮 GameSwitcher play time | Play Activity DB opened **on the UI thread** the first time each game is shown | computed by the preload worker with name and core (UI fallback kept) | no DB open on the UI thread for prefetched games 📐🧪* ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)) |
| 🎮 GameSwitcher romscreens | PNG decode + scale **on the UI thread**, under the same mutex as a one-shot loader | persistent worker decodes outside the lock and prefetches **±2** entries; only the UI thread frees surfaces (±5 window) | scrolling no longer waits for decoding 📐🧪* ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |

> 🧹 Every cache above ships with its own teardown: `list_free()` releases the TTF slots,
> `cleanImagesCache()` frees the infoPanel scaled cache, and `free_resources()` releases
> the playActivityUI page cache — so this is a speed win **without** a new leak. The
> romscreen worker is stopped and joined before `freeRomScreens()` releases its surfaces.

---

## 🔋 4 · Power, battery & idle CPU

> 🟥🟧 The category with the most direct battery-life relevance: fewer wake-ups, fewer
> forked subprocesses, fewer duplicate sysfs writes.

### 🔌 Boot

| Subsystem | Before | After | Impact |
|:--|:--|:--|:--|
| 📶 Wi-Fi at boot | `update_networking.sh check` brought Wi-Fi up **inside** the boot (a fixed 2 s `sleep` after powering the chip) | background, unless **Wait for sync on startup** is on (or Wi-Fi is off and forced on) | 📱 **`boot_network` 3.34 s → 0.09 s** ([`874ea325`](https://github.com/Amiga500/Onion/releases/tag/OnionPlus-v4.4.0-beta-20260925-874ea325), [`10f2369e`](https://github.com/Amiga500/Onion/commit/10f2369e)) |
| 🔊 Audio server start (`runifnecessary`) | fixed `sleep 0.5` after starting, before checking | checked every 50 ms, same 0.5 s ceiling and 8 retries | 📏 **512 → 61 ms** · 📱 `boot_init` 1.59 → 1.05 s ([`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc)) |
| 📴 "Enable Wi-Fi temporarily", Wi-Fi off | Wi-Fi turned on and off **inside** the boot, time never synced unless Wait for sync was on | Wi-Fi on → sync → update check → off, **in the background**; IP wait 30 s | 📱 **`boot` 5.16 s → 1.78 s**, time synced ([`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02)) |
| 💾 128 MB swap file | `swapon` before the boot continues | `swapon` in the background | 📱 `boot_swap` **0.01 s** ([`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc)) |

### 🌀 CPU & wake-ups

| Subsystem | Before | After | Impact |
|:--|:--|:--|:--|
| 🔊 OSD volume/brightness bar thread | `usleep(100)` busy-wait (~10,000 loops/s) | `usleep(16000)` (~60 fps) | 🚀 **idle CPU ~10% → <1%** 📏 |
| 🖼️ OSD overlay draw loop | full-throttle spin for the overlay's duration | `msleep(2)` per iteration + demoted logging | overlay CPU burn capped 📐 |
| 🌀 SDL UI loops (GameSwitcher, Tweaks, prompt, playActivityUI, themeSwitcher, packageManager, batteryMonitorUI + dialogs) | non-blocking poll with **no sleep**: one core at 100% while a menu just sits there | sleep until the next frame (or 15 ms) when idle | 🚀 **idle UI CPU ~100% of a core → near 0** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🌙 keymon blue-light schedule | `blue_light.sh check` **every 15 s**, synchronous: 2 global `sync`, ~20 `fork`/`exec`, keymon deaf to keys for up to ~4 s on a transition | evaluated in-process (same `.tz`, same window logic); script started in background **only on a transition** | **~20 spawns/15 s → 0** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🪫 batmon low-battery thread | `usleep(0x4000)` (~16 ms) | `usleep(500000)` (500 ms) | 🚀 **~−97% wake-ups** 📐 |
| ⏱️ batmon main loop | `config_get("battery/warnAt")` every tick | read only at check timeout | **−100% hot-loop config reads** 📐 |
| 🎮 GameSwitcher battery poll | `stat()` on `/tmp/percBat` every loop (~1 kHz) | checked once/second (matches batmon's write rate) | **~99.9% fewer `stat` calls** 📐 |

### 💾 SD-card writes & syncs

| Subsystem | Before | After | Impact |
|:--|:--|:--|:--|
| 💾 Volume / brightness step | `settings_save()` rewrote every setting: ~30 files, **13 `fsync`** on the SD card | selective save against the on-disk snapshot | **1 file (`system.json`)** 📐🧪* ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 💾 keymon key handling | global `sync()` after deleting flags in `/tmp` (tmpfs) — flushed the SD card on a key press | no sync for tmpfs flags | **−1 global sync per flagged key** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🔁 `runtime.sh` main loop | `pgrep keymon` + `touch` + global `sync`, **4× per loop** | `/proc/<pid>` check, shell builtin, one `sync` when a game/app exits | **4 syncs → 1**, `pgrep` only if keymon died 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🔁 Return to MainUI (`check_hide_recents`) | runs twice per cycle, each ending in a global `sync` | `sync` only when a list is moved, plus one explicit `sync` after MainUI exits | **2 syncs → 1 per cycle** 📐🧪* ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)) |
| 🌐 `update_networking.sh check` (after every game while a service is on) | global `sync` before starting each of Samba/FTP/Telnet/HTTP; SSH key-folder `sync` every run | no sync for service starts (page cache already serves the files); SSH `sync` only when the folder is created | **up to 5 → 0 global syncs per game exit** 📐 ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)) |
| 🗂️ Quick switch (move game to top of recents) | two full rewrites (add to top, then delete) | one atomic rewrite (`file_move_line_to_top`) | **2 → 1 SD rewrites** 🧪 ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)) |
| 🗄️ Play Activity DB | journal file created/deleted per transaction; `SQLITE_BUSY` on concurrent access | `journal_mode=TRUNCATE`, 2 s `busy_timeout` (set in [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)), plus `sqlite3_close_v2` | fewer FAT directory updates, no lost writes on contention 📐 |
| 🔋 batmon `/tmp/percBat` | `fsync` on tmpfs | write + `rename()` (no fsync; readers never see an empty file) | **−1 useless fsync per % change** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |

### 🔋 Battery, display & rumble

| Subsystem | Before | After | Impact |
|:--|:--|:--|:--|
| 🔌 `battery_isCharging()` (`HAS_AXP()` — MM+ and Mini Flip) | `fork`+`exec` of `axp_test` every call (~5–10 ms) | 2 s cached wrapper | 🚀 **~−99% subprocess spawns** 📐 |
| 🔋 `battery_hasChanged` while charging | OnionPlus used to overwrite the `500` charging sentinel from `/tmp/percBat` | early-return like `OnionUI/Onion:main` (`500` stays while plugged in) | charging icon no longer drops after the first percBat tick 🛡️ |
| 🪫 `getBatPercMMP()` AXP percent | `axp_test` garbage (e.g. `1735289191`) and `-1` written to `/tmp/percBat` | last sane **0–100** kept; out-of-range samples dropped | GS/keymon never read a bogus percent 🛡️ |
| 💡 `display_setBrightnessRaw` | sysfs write on **every** call | cached, duplicate writes skipped | **−100% duplicate PWM writes** 📏 |
| 📳 `rumble()` GPIO init | `export`+`direction` sysfs writes on **every** pulse | one-time init + retry if `gpio48` missing, value-only writes after | **−2 sysfs writes/pulse** 📏 |
| 🔆 AdvanceMENU quick-switch (PWM) | backlight PWM always re-enabled on exit | re-enabled **only** when returning from a game (`quick_switch`) | fewer redundant PWM writes 📐 |

---

## ⚙️ 5 · Process & syscall diet

> 🟧 Every `system()` call forks a shell **and** the real binary — two processes for
> one line of intent. These were replaced with direct syscalls or `fork`+`exec`.

### 🧱 C code

| Call site | Before | After | Result |
|:--|:--|:--|:--|
| `mkdirs()` | `system("mkdir -p …")` | iterative `mkdir()` walk | **2 → 0 processes** |
| `file_copy()` | `system("cp -f …")` | `open`/`read`/`write` loop | **2 → 0 processes**, shell-injection surface closed |
| `config.h` `_config_prepare` | `system("mkdir -p …")` | direct `mkdirs()` | **2 → 0 processes** |
| GS overlay `playActivity` | `system("… &")` | double-fork + `execl` (async, no zombies) | 🚀 **−80% process overhead** 📏 |
| GS overlay RetroArch kill/poll | `killall` / `pidof` shell-outs | `process_killall_signal` / `process_isRunning` (all matching PIDs) | **−100% shell, killall semantics restored** |
| playActivity DB ops | 2× open/close per operation | 1× open/exec/close | **−50% DB I/O** 📏 |
| Reset paths (tweaks/theme/RA overrides) | `rm -rf` via `system()` | `nftw()`-based `file_remove_recursive()` | shell-free recursive delete |
| keymon `touch`, `tools favfix`, `playActivity stop_all/resume`, `bootScreen`, recorder / blue-light scripts | `system("… &")` / `system("touch …")` | `process_run_wait()` / double-fork `process_spawn_detached()` / `creat()` | **one shell less per call** ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| keymon CPU-clock hotkey | `cpuclock` spawned **twice** per press | last value cached (reset on every state change) | **2 → 1 spawns** ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| keymon config flags (`.altBrightness`, `.cpuClockHotkey`, `.recHotkey`) | `stat()` on the SD card on every key | cached, refreshed on settings change + every 15 s | **−1 SD `stat` per key** ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |

### 🐚 Shell scripts

| Call site | Before | After | Result |
|:--|:--|:--|:--|
| `blue_light.sh` time parsing | `echo \| cut \| xargs` ×2 + `awk` per value | shell parameter expansion | **~6 → 0 forks per value** ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| `runtime.sh` launch-command parsing | `echo \| grep/awk/basename/dirname` pipelines | parameter expansion (multi-line commands keep awk) | **~15 fewer processes per launch** 🧪* ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)) |
| `start_audioserver` on every launch | `jsonval` + `awk` + subshell to compute a volume, even when already running | skipped when audioserver is running | **3 → 0 processes** in the usual case ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)) |
| `fbmode --probe` parsing (return to MainUI, `change_resolution`, GameSwitcher) | 2–4 `echo \| awk` / `echo \| cut` pipelines | `fb_probe_fields` (builtins, globbing off) | **4–8 → 0 processes** 🧪* ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)) |
| `mount_main_ui` | `cat \| grep \| cut` + `basename` + subshell | `read` loop over `/proc/self/mountinfo` | **~6 → 0 processes per return** 🧪* ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)) |
| `update_networking.sh` Wi-Fi checks | `jsonval` on **every** `wifi_enabled`/`wifi_disabled` (8–10 per run) | read once per `check` run | **8–10 → 1 process** 🧪* ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)) |
| `get_full_resolution_path` (Mini+/Mini Flip, every launch) | `cmd_to_run.sh` re-read 2–3× with `grep`/`cut`/`sed` | one builtin read + parameter expansion | **4–5 → 0 processes** (1 `grep` for ports) 🧪* ([`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e)) |
| `get_info_value` (game with a custom core) | `echo \| grep \| awk \| awk \| tr` | builtin loop, same word-boundary match | **5 → 0 processes** 🧪* ([`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e)) |
| `system.json` reads around MainUI (`wifi` ×2, `theme`) | `jsonval` process per read | `sysjson_get` (builtins; `jsonval` fallback for escapes/missing keys) | **3 → 0 processes per MainUI cycle** 🧪* ([`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e)) |
| Brightness at boot (`init_system`) | `jsonval brightness` process | `sysjson_get` | **1 → 0 processes** ([`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc)) |

### 🕹️ AdvanceMENU

| Call site | Before | After | Result |
|:--|:--|:--|:--|
| 🕹️ AdvanceMENU romscripts | temp file written **CWD-relative** | temp file next to `advmenu.rc`, PID-suffixed | race condition + read-only-CWD failure fixed |
| 🕹️ AdvanceMENU `launch.sh` | no reentrancy guard | early exit if `advmenu` already running | duplicate-instance guard |

> 📉 Net result across the 25-file hardening core: **`system()` call sites 3 → 1**
> (the one survivor, `process_start()`, is confirmed **dead code** with no live caller).
> 🆕 [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804) extends the diet to **keymon**, the busiest daemon: every `system()` left on
> its key-press and periodic paths is gone (`shutdown` on the power-off path remains).

---

## 🛡️ 6 · Security & memory hardening

> 🟦 No performance claim attached to anything in this section — pure correctness and
> memory-safety.

| Category | Before → After | Count |
|:--|:--|--:|
| 🔴 Unbounded `sprintf` | → bounded `snprintf` | **23 → 0** ✅ |
| 🔴 Unbounded `strcpy` + `strcat` | → bounded copies / `memcpy` | **37 → 0** ✅ |
| 🔴 Non-reentrant `strtok` | → `strtok_r` with owned save-pointer | **4 → 0** ✅ |
| 🟢 NULL-pointer / I/O guards added | new `if (!ptr)` / return-value checks | **+57** *(25-file set)* |
| 🟢 Leaked descriptors closed | `fclose`/`close` on error paths | **+18** |
| 🟢 Division-by-zero guards added | early return before `% total_count` | **+2** |

### 🕵️ Notable defects fixed (pre-existing, not ports)

> 🆕 The most recent fixes, found with the on-device timing log, are listed at the end of
> this section.

- 🔓 **`hash.h` FNV1A load** — removed a **7-byte out-of-bounds read**, an **unaligned
  64-bit load** (traps on ARMv7), and an **oversized shift** (`x << 64`, UB). Hashes stay
  **bit-identical** — verified against 264 reference vectors at 5 optimization levels.
- 🕳️ **`gs_popMenu.h` save thread** — used to run with an **uninitialised 4 KB stack
  buffer** as a path when path construction failed; now returns early instead of polling
  a garbage path for up to 30 s.
- 🎯 **`currentGame()` NULL derefs** — 3 call sites now guard against an empty game list.
- 🧮 **Dead slot-bounds check** — the reject condition was
  `selected_slot < 0 && selected_slot >= slot_count`, which can never be true for a
  single value, so out-of-range slots were never rejected; changed to `||`.
- 📖 **`_isContentNameInInfo` OOB read** — a match at offset 0 no longer reads one byte
  before the buffer.
- 🔠 **`includeCJK()` UTF-8 validation** — all 3 continuation bytes checked, not just the
  first.
- 💾 **File I/O consistency** — `fsync()` before rename on key-value writes;
  `file_remove_recursive()` errors are now logged instead of swallowed.
- 🧩 **`const`-correctness** — `file_basename()` no longer discards `const` via a cast
  (`-Wcast-qual` clean).
- 🕹️ **AdvanceMENU biosset false positive** — indexing only `//game[@name]` so BIOS-set
  XML entries (which also carry a `name` attribute) no longer cause incompatible ROMs to
  be kept by mistake.
- 🔋 **`battery_hasChanged` charging icon** — OnionPlus no longer lets `/tmp/percBat`
  overwrite the `500` sentinel while the cable is plugged in. Matches
  `OnionUI/Onion:main` early-return.
- 🕹️ **AdvanceMENU `advmenu.rc` safety** — the rewrite script only overwrites the live
  config if **both** `grep` and `echo` succeeded; otherwise the partial temp file is
  removed and the original is left untouched.
- 🔐 **`mp4_to_mng.ps1`** — restored the official HTTPS ffmpeg download permalink (was an
  insecure/unreliable HTTP mirror), forced TLS 1.2 for old PowerShell, and scoped the
  ffmpeg search to the extraction directory.
- 🎲 **`randomGamePicker` division-by-zero** — recents/favorites and single-system modes now
  bail out with `ERROR_CODE_NO_GAME_FOUND` instead of computing `rand() % total_games_count`
  when the list is empty.
- 🖼️ **`batteryMonitorUI` missing assets & OOB graph write** — every blit is routed through a
  `safeBlitSurface()` helper that no-ops on a NULL surface (e.g. a failed `IMG_Load`), and
  `compute_graph()` now stops processing a record set if a corrupt duration would index
  outside the `graphic[]` array.
- 🎨 **`themeSwitcher` NULL assets & theme-count overflow** — the same `safeBlitSurface()`
  pattern plus `SURF_W`/`SURF_H` macros guard every icon blit against a missing PNG, and
  `loadThemeDirectory()` now stops scanning once `NUMBER_OF_THEMES` is reached instead of
  overflowing the theme array.
- 📦 **`packageManager` loading screen** — `IMG_Load("res/loading.png")` result is now
  NULL-checked before blitting/flipping/freeing.
- 🧵 **`gs_romscreen.h` format-string bug** — `sprintf(currPicture, game->recentItem.imgpath)`
  used the artwork path as a format string; changed to `sprintf(currPicture, "%s", ...)`.
- 🔤 **List TTF cache dimming** — `surfaceSetAlpha` on a cached label mutated the pixels;
  restoring with alpha 255 is a no-op. Hidden rows now dim a `SDL_ConvertSurface` copy
  (`_blit_cached_label`, [`fbd26d06`](https://github.com/Amiga500/Onion/commit/fbd26d06)).
- 🪫 **AXP `percBat` garbage** — `getBatPercMMP()` keeps the last sane 0–100 instead of
  writing `-1` or timestamps to `/tmp/percBat`
  ([`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e)).

#### 🆕 Fixed in [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804) (all present in `OnionUI/Onion:main`)

- 💥 **keymon CPU-clock stack overflow** — `cpuclockstr[5]` was handed to
  `process_start_read_return()`, which copies (and `strncpy`-pads) up to `STR_MAX` bytes:
  a stack overwrite on **every** use of the hotkey. Buffer is now `STR_MAX`.
- 📦 **`suspendpid[]` overflow** — the counter lives in `[0]`, but the bound allowed a write
  to `suspendpid[32]`; now capped at `PIDMAX - 1`.
- 🧾 **`/proc/<pid>/stat` parsing** — `%127s` split process names containing spaces and
  shifted `state`/`ppid`/`flags`; on `fopen` failure the previous process's fields were
  reused. Now parsed from the **last `)`**, skipped on any failure.
- ⏲️ **Settings debounce never fired** — the timestamp used `ticks`, which only advances
  every 15 s, so a volume change was saved on some later, unrelated key press (or never,
  on power-off). Saved 500 ms after the last change, key or no key.
- 🧮 **`system.json` dirty check** — the snapshot was refreshed only by keymon re-reading
  its own write; returning to the loaded value was silently **not** saved. The snapshot
  now follows every save.
- ⚡ **Power loss during a settings write** — `system.json`, `keymap.json`, `json_save()` and
  `config_set*()` truncated the file in place. Now `.tmp` + `fsync` + `rename()`
  (`file_atomic_write`), symlinks resolved first, so a cut leaves the previous file intact.
- 🔒 **`blue_light.sh` lock race** — test-then-`touch` let two instances start; now an
  atomic `mkdir` lock (a legacy plain-file lock is cleared once).

#### 🆕 Fixed in [`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21), [`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8) and [`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e) (all present in `OnionUI/Onion:main`)

- 🧠 **SQLite connection leak on every MainUI cache lookup** — `cache_db_prepare()` closed
  the database while its statement was alive; `sqlite3_close()` failed with `SQLITE_BUSY`
  and the handle was dropped. About **570 KB and one descriptor per lookup** on a
  5,000-game cache, one lookup per GameSwitcher entry. Now `sqlite3_close_v2()`: 0 leaked
  ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)).
- 💧 **`cache_db_find()` leak** — `game_name` not freed when no cache DB exists ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)).
- 🗂️ **Recent list could lose the wrong entry** — `file_delete_line()`/`file_read_lineN()`
  counted in 1 KB chunks, `readHistory()` in 1.5 KB: any longer line was numbered
  differently, so a valid game was deleted instead of the duplicate. All now count real
  lines with `getline()` ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)).
- 💾 **`cmd_to_run.sh` rewritten on every launch** — the `$` check was `grep -q "\$"`
  (regex end-of-line, always true); now only when the path really contains `$` ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)).
- ⚡ **Recent list briefly missing** — `file_add_line_to_beginning()` deleted the file
  before renaming the new one; now an atomic replace ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)).
- 🔗 **Quick switch merged two entries** — a moved last line without a trailing newline
  was glued to the next line; it now gets its newline ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)).
- 🥾 **`system.json` wiped at boot** — `load_settings` rewrote it in place (`cp -f`, and
  `sed > temp; mv` with `temp` in the current folder); an empty per-device settings file
  or a power cut could leave it empty. Now `.tmp` + flush + rename, never with an empty
  file ([`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e)).
- 📦 **Package Manager leak** — `checkRoms()` released the parsed config with `free()`
  instead of `cJSON_Delete()`, leaking every child node per emulator package ([`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e)).

> ⚠️ An alternative implementation of [`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8) proposed on a `copilot/` branch was **not
> merged**: with recents hidden it rebuilt `recentlist-hidden.json` from the new entries
> only, cutting the GameSwitcher history to the last game on every return to MainUI. Its
> unit tests for the recent-list move were kept.

#### 🆕 Fixed after the first on-device tests ([`874ea325`](https://github.com/Amiga500/Onion/releases/tag/OnionPlus-v4.4.0-beta-20260925-874ea325), [`10f2369e`](https://github.com/Amiga500/Onion/commit/10f2369e), [`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02))

Present in `OnionUI/Onion:main` unless noted:

- 🕒 **Play time of decades after the clock is set** — the `ntpdate` fallback in `get_time`
  changed the clock without closing and reopening open play sessions; a session started
  at a 1970 clock got ~497,000 h. Now closed/reopened on every path, and sessions longer
  than 24 h (or negative) are discarded, which also cleans up existing bad rows.
- 🌍 **Time zone reset to UTC** — a failed time-zone lookup (worldtimeapi or timeapi.io)
  was converted to offset 0 and written to `.tz`; the zone is now only rewritten when a
  lookup returns a real offset.
- 🔁 **Time re-synced after every game** — only the web path marked the sync as done;
  a successful `ntpdate` now does too.
- 🖼️ *(OnionPlus regression, not upstream)* GameSwitcher captures kept their aspect ratio
  on 752×560 panels since `fee6c4b`; OnionPlus's fill-the-screen rule is restored.
- 📶 **Boot waited for Wi-Fi with "Enable Wi-Fi temporarily" set** even with Wi-Fi on
  (OnionPlus background bring-up refined); boot waits only when something needs the network.
- 📴 **"Enable Wi-Fi temporarily" never synced the time** without "Wait for sync on startup"
  (OnionUI defect): with Wi-Fi off it turned Wi-Fi on at boot, checked the Wi-Fi *setting*
  (off) before syncing, and turned Wi-Fi off again — 3.4 s of boot for nothing, and the
  clock left where it was. Now it syncs in the background ([`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02)).

---

## 🕹️ 7 · AdvanceMENU frontend

> 🟨🛡️ A self-contained optimization and hardening pass on the AdvanceMENU frontend:
> fonts, power handling, script robustness and tooling.

| Area | Change | Kind |
|:--|:--|:--:|
| 🔤 Fonts | `advmenu.rc` now uses fonts bundled in `BIOS/.advance` instead of Onion core fonts | 🟨 UX/consistency |
| 🔆 Backlight | PWM restored only on `quick_switch` return-from-game, not on every exit | 🔋 power |
| 🚦 Single-instance | `launch.sh` skips launch (with a log message) if AdvanceMENU is already running | 🛡️ robustness |
| 🐎 PS tooling | `move_Roms_Without_Preview.ps1` / `move_incompatible_Roms.ps1` rewritten from per-item rescans to lookup tables | ⚡ O(n²)→O(n) |
| 📝 Romscripts | Temp files moved next to `advmenu.rc` with a PID suffix; only committed if the rewrite fully succeeded | 🛡️ atomicity |
| 🎬 Media tooling | `mp4_to_mng.ps1` ffmpeg download hardened (HTTPS, TLS 1.2, scoped search, `-Force`) | 🛡️ reliability |

> 📌 This pass shipped in [PR #210](https://github.com/Amiga500/Onion/pull/210), with an
> explicit code-review follow-up round (`a25abb81`) that fixed the BIOS-set false
> positive and improved the launch-guard log message.

---

## 🧪 8 · Testing — the safety net

> 🟩 Zero host unit tests existed at the OnionPlus base commit. All of the following was
> added **during** this branch.

| Metric | Value |
|:--|--:|
| 🧪 Active test suites | **68** |
| ✅ Tests | **1,423** |
| ✅ Assertions | **71,436** |
| ❌ Failures | **0** |
| ⏱️ Suite runtime (prebuilt) | **~2.5 s** |
| 🔐 Security-focused suites | 10 suites · 219 tests · 960 assertions (**15%** of all tests) |

- 🏗️ Runs entirely on the **host** — no cross-toolchain, no SDL, no device — via a single
  `make unit-test` target, making it usable as a fast CI gate.
- 🧬 A separate `neon-arm` CI job cross-compiles the NEON assembly and runs it under
  `qemu-user`; a `unit-test-san` job runs a sanitizer subset (ASan/UBSan).
- 🧯 `test_hash` alone grew from 12 tests/21 assertions to **15 tests / 350 assertions**
  to lock in the hash bit-identity guarantee above.
- 📟 `test_device_model` now covers `MIYOO285` and the `HAS_AXP()` / `HAS_WIFI()` /
  `IS_MIYOO_PLUS_OR_FLIP()` macros (**13 tests / 23 assertions**, host-run green).
- 🔤 `test_alpha_scale` includes `scale_alpha_255_does_not_undo_dim` (list-cache dimming).
- 🪫 `test_battery` clamp contract: `axp_percent_keeps_valid` / `rejects_negative` /
  `rejects_garbage` / `first_failure_is_zero`.
- ⚡ [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804) passes the full suite unchanged (**1,419 / 71,410**) and was additionally
  checked with **out-of-tree** host programs (marked 🧪* above, not yet in `test/`):
  selective save / atomic writes / deprecated-flag migration on a mock `/mnt/SDCARD`,
  playActivity against real SQLite (index, `journal_mode`, cache-refresh rule), and a
  ThreadSanitizer stress of the romscreen worker (random scrolling + removals: no race,
  no leak, no corruption). All modules re-checked for new compiler warnings: none.
- 🗂️ [`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8) adds four `test_file` cases for `file_move_line_to_top()` (move, first line,
  out of range leaves the file unchanged, missing trailing newline): **1,423 / 71,436**.
- 🔬 [`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21), [`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8) and [`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e) were also checked out of tree (🧪*): cache-lookup
  descriptor count, `stop_all` query plan on 60,000 sessions, recent-list line numbers,
  battery-icon skip (rendering stubbed), hidden-recents history kept, old-vs-new equivalence
  of every rewritten `runtime.sh` helper under dash and bash `--posix` (launch parsing,
  `fbmode` probe, `mountinfo`, `get_full_resolution_path`, `get_info_value`, `sysjson_get`,
  `load_settings`), and `play_activity_find_all()` under ASan/UBSan.
- 📱 [`874ea325`](https://github.com/Amiga500/Onion/releases/tag/OnionPlus-v4.4.0-beta-20260925-874ea325), [`10f2369e`](https://github.com/Amiga500/Onion/commit/10f2369e), [`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc) and [`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02) add host checks for time sync
  (failed zone lookup, `ntpdate` marker), boot network paths (forced Wi-Fi, Wait for sync,
  after a game, temporary Wi-Fi with and without Wait for sync, user enabling Wi-Fi
  meanwhile), `runifnecessary`, play sessions across a clock jump and GameSwitcher
  capture scaling — and the shell changes have since **run on a Miyoo Mini+** (busybox),
  with the timings in [Measured on a Miyoo Mini+](#-measured-on-a-miyoo-mini).

### ⏱️ Measuring on the device

[`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e) adds timing marks to `runtime.sh`, active **only with logging on** (Tweaks →
Advanced → Diagnostics → Enable logging). They use `/proc/uptime` and shell builtins, so
they are immune to the clock jump when the time is restored at boot, and cost one file
test per mark when logging is off. Results go to `.tmp_update/logs/timing.log` (the
previous session is kept as `timing.prev.log`):

| Mark | Measures |
|:--|:--|
| `boot: runtime.sh started N s after kernel start` | kernel + init before Onion |
| `boot_init` / `boot_network` / `boot` | `init_system`, `start_networking`, runtime start → first menu/game |
| `boot_swap` / `boot_audio` / `boot_display` | inside `boot_init`: swap start, audio server, screen detection (added in [`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc)) |
| `game_prepare` / `game_run` / `game_exit` | launch preparation, the session itself, post-processing (with the ROM name) |
| `mainui_prepare` / `mainui_session` / `mainui_return` | before, during and after MainUI |
| `switcher_prepare` / `switcher_session` | the same for the GameSwitcher |

> 📱 First results from a Miyoo Mini+ are in [Measured on a Miyoo Mini+](#-measured-on-a-miyoo-mini).
> Samples from a Mini, Mini v4 and Mini Flip are still welcome.

---

## 🏗️ 9 · Build, CI & release

| Change | Detail |
|:--|:--|
| 📦 Release flags | `-O2 -ffunction-sections -fdata-sections -Wl,--gc-sections` → 🚀 **−5–15% binary size** 📏 |
| 🎯 New build target | `make unit-test` — host-only, zero device dependency |
| ⚙️ Parallel `make` | `core` builds `bootScreen` + `gameSwitcher` first (they compile every shared `../common` object), then the other 27 modules as independent targets; `$(MAKE)` passes the jobserver, so `make -j` works without two sub-makes racing on one `.o` ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| ♻️ Setup stamp | `cache/.setup` re-runs when `static/`, `lib/` or any `src/*/res`/`script` file is newer (was: only after `make clean`) ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🗜️ RetroArch package | `retroarch.pak` reused from `cache/` while a content hash (paths, modes, symlinks, data) is unchanged — skips the slowest `7z` step ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 📊 Opt-in profiling | `src/common/utils/perf.h` — `PERF_START`/`PERF_END` compile to nothing unless `-DPERF_ENABLED` |
| 🏷️ Release naming | `OnionPlus V4.4.0-beta-YYYYMMDD`, zip `OnionPlus-v…-<sha>.zip` — real dated GitHub Releases, no more overwritten `latest`. Base remains **4.4.0-beta**; Mini Flip support is a port, not a rebase onto official `v4.5-dev`. |
| 📡 OTA | `ota_update.sh` points at `Amiga500/Onion`, filters `OnionPlus-v` assets. Stable = `/releases/latest`. Beta installs **only GitHub prereleases** — no fallback to `releases[0]` (finding D). Host CI (`.github/workflows/test.yml`) runs on push to `onionplus-compact`. |
| 📱 Mini Flip | Device id `285`, MainUI-285 binaries, lid-close Tweaks. Runtime probes AXP first (354 Mini+), then the `hall-mh248` sysfs node (285 Mini Flip). The **installer** probes **hall first** (Mini Flip stays Mini Flip if `axp` is not on PATH yet); `axp` / `axp_test` = Plus. `/dev/input/event*` is **not** a Mini Flip signal. Installer preclears the framebuffer (`fbmode` if present, else `dd` + `fbset 640x480/2`) before `check_device_model`. Ported from `OnionUI/Onion:v4.5-dev` without merging that branch. Lid/Hall **untested** on a physical Mini Flip. |
| 🖥️ Boot FB | Plus/Mini Flip: a dmesg hint of `640x480` no longer skips the `mi_fb0` poll (avoids locking a 752 panel at 640 for the boot). `commit_mainui_fbmode()` waits for the FB driver; on timeout it uses `fbset`, not `fbmode` with the driver still down. |
| 🧵 Signal handling | Shared `signal_handler_quit()` deduplicated across 6 apps; `volatile sig_atomic_t` used correctly for signal-shared state |

---

## 📊 10 · Grand totals

| Metric | Value |
|:--|--:|
| 🔧 Commits (`07505ea5..9768ae02`) | **41** *(through the last code commit [`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02), including the #220 artifact clean-up and its merge; README-only commits after it are not counted. The long `OnionPlus` branch was 97.)* |
| 📁 Files changed | **188** *(182 at `bf3deb8e`; `fee6c4b` +6 and `747d102a`/`1592866e` +3 files that were still identical to upstream; the 3 `docs/` reports are removed)* |
| ➕➖ Lines | **+30,487 / −1,109** at `bf3deb8e`, then `fee6c4b` **+1,164 / −226**, the `docs/` removal **−2,157**, `85bc9f21` **+176 / −65**, `747d102a` **+311 / −43**, `1592866e` **+256 / −35**, `874ea325` **+65 / −5**, `10f2369e` **+33 / −8**, `29791efc` **+20 / −5**, `9768ae02` **+52 / −13** *(net figure: `git diff --shortstat 07505ea5..HEAD`)* |
| 🧩 Production (`src/` + `static/` + CI/Makefile) at `bf3deb8e` | **101 files · +4,572 / −1,080** *(excludes `.gitignore` + `SDL.h`, 2 · +35 / −0; later passes in the Lines row)* |
| 🧪 Tests (`test/`) | **75 files · +23,424 / −10** *(`747d102a`: +97 in `test_file.c`)* |
| 📚 README | **1 file** *(the three `docs/` reports are retired; this README is the reference)* |
| ⚡ NEON kernels | **8** (7 asm + 1 intrinsics) |
| 🧪 Test suites / tests / assertions | **68 / 1,423 / 71,436** — **all green** ✅ |
| 🛡️ Unsafe `sprintf`/`strcpy`+`strcat`/`strtok` remaining (hardened set) | **0 / 0 / 0** |
| 🛡️ NULL-guards / closed descriptors added | **+57 / +18** *(25-file set)* |
| 🔐 Issues fixed in code shared with Onion | **25** *(6 + 7 in `fee6c4b` + 8 in `85bc9f21` / `747d102a` / `1592866e` + 4 found with the on-device tests)* |
| 🕹️ AdvanceMENU scripts hardened/optimized | **7 files** |

> 📎 Everything here is reproducible from git: `git rev-list --count 07505ea5..HEAD`,
> `git diff --stat 07505ea5..HEAD`, `git show --stat <sha>` for any commit below.

---

## 🔀 11 · Commit timeline

A bird's-eye view of the branch's evolution, oldest first:

1. 🖼️ **NEON foundation** — vector kernels vs OnionUI scalar pixel loops.
2. 🛡️ **Hardening wave** — crash/memory-safety port across the common layer.
3. 🧪 **Test harness** — 68-suite host unit-test scaffold added from scratch.
4. 📚 **Docs** — first optimization report published.
5. 🔋 **Power/CPU batch** — OSD busy-wait, brightness cache, battery cache, batmon, SQLite, config, GS overlay fork+exec, infoPanel hardening.
6. 🐛 **Defect fixes** — hash over-read, save-state uninitialised buffer, `const` cast, plus two external PRs (#206, #207) merged in.
7. 🔎 **Review pass 1** — `currentGame()` NULL derefs, async `playActivity` restored, dead slot check, OSD overlay throttle.
8. 🏗️ **Release/OTA** — dated GitHub Releases, `Amiga500/Onion` OTA wiring, `TARGET=OnionPlus`.
9. 🎯 **GameSwitcher fixes** — framebuffer stride and romscreen stretch corrections.
10. 🔎 **Review pass 2** — rumble caching, infoPanel image cache, GS battery-poll throttle, playActivityUI page cache, randomGamePicker dedup.
11. 🕹️ **AdvanceMENU pass** — fonts, PWM handling, script speedups, race-condition and false-positive fixes ([PR #210](https://github.com/Amiga500/Onion/pull/210)).
12. 🔎 **Review pass 3** — randomGamePicker division-by-zero guard, batteryMonitorUI/themeSwitcher NULL-asset & bounds hardening, packageManager NULL guard, gs_romscreen format-string fix.
13. 📱 **Mini Flip port** — surgical carry of Miyoo Mini Flip + MainUI-285 from upstream `v4.5-dev` (`921155e8`); OnionPlus battery cache / `file_copy` reset / settings bounds kept.
14. 🔎 **OnionUI-parity review** — restore `battery_hasChanged` early-return while charging; `process_killall` for RetroArch; `file_read("")` parity; rumble GPIO retry; remaining `sprintf` bounds on GS/chargingState; TTF cache cleanup on exit.
15. 🩹 **2026-09-01 review (A–G)** — empty-file test contract; Mini Flip `suspend_exec` lid-already-closed; AXP-then-hall detect; OTA beta without stable fallback; brightness write-through; infoPanel scale identity; theme TTF cleanup-before-free. On the long branch: duplicate tree (`9ab47af` / `2f90bbe`); CI push trigger (`fa5bb007`). Compact CI is `onionplus-compact` (`22004cce`).
16. 📦 **Compact history** — long `OnionPlus` (97 commits to `fa5bb007`) squashed onto `onionplus-compact`.
17. 🔀 **@robcodedev ports** — `OnionUI/Onion` PRs **#1936–#1946** (still open upstream) via [Amiga500 #217](https://github.com/Amiga500/Onion/pull/217) (`c7a1a7e9` + `587c35ec` + merge `f87e7781`): keymon SELECT refresh, `lt.lang` JSON, ThemeSwitcher on-demand previews, GameSwitcher favorites + crash fixes, `fbmode` framebuffer transitions, `.forceKillRetroarch`, `romwinidx` on SD, theme per `SERIAL_NUMBER`, recents cap 200, skip RA cfg patch, overlap launch.
18. 🔤 **List cache + installer Mini Flip** — `fbd26d06`: dim a copy of cached TTF labels; installer hall-first (never `event*`); framebuffer preclear before device detect.
19. 🖥️ **Boot FB + AXP percent** — `bf3deb8e`: Plus/Mini Flip keep polling `mi_fb0` when dmesg says 640; `commit_mainui_fbmode` honors `wait_for_fb_driver`; `getBatPercMMP` never writes garbage to `/tmp/percBat`.

20. ⚡ **Performance pass** — [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804): atomic settings/JSON/config I/O and selective save; in-process blue-light schedule; keymon without `sync`/shell on key paths; single `/proc` pass; playActivity `file_path` index + `TRUNCATE` journal + busy timeout; GameSwitcher romscreen preload worker; idle sleep in every SDL UI loop; `runtime.sh` sync diet; parallel `make`, setup stamp, cached `retroarch.pak`; 7 upstream defects (CPU-clock stack overflow, `suspendpid` overflow, `/proc` parsing, debounce, dirty snapshot, non-atomic writes, BLF lock).
21. 📚 **Docs retired** — `docs/` (`ONIONPLUS_OPTIMIZATION.md`, `OPTIMIZATIONS_OVERVIEW.md`, `OnionPlus-vs-base.md`) removed; this README is the single reference.
22. 🗃️ **Performance pass 2** — [`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21): MainUI cache connection leak (`sqlite3_close_v2`), `cache_db_find` leak, recent-list line numbering (`getline` everywhere), `readHistory` duplicates in one rewrite, `stop_all` index + single transaction, GameSwitcher play time on the preload worker, fork-free launch parsing, audioserver volume skip, `$`-check rewrite fix.
23. 🧹 **Artifact clean-up** — [#220](https://github.com/Amiga500/Onion/pull/220): working files committed by `85bc9f21` removed, `.gitignore` guards added.
24. 🔋 **Performance pass 3** — [`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8): battery icon regenerated only on change, one `sync` per MainUI cycle, `fb_probe_fields`, `mount_main_ui` via `read`, `file_move_line_to_top` + atomic `file_add_line_to_beginning`, Wi-Fi read once and no service-start `sync` in `update_networking.sh`, four new `test_file` cases. A `copilot/` alternative was rejected (hidden-recents history loss).
25. ⏱️ **Performance pass 4** — [`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e): `timing.log` marks, fork-free `get_full_resolution_path` / `get_info_value` / `sysjson_get`, crash-safe `system.json` in `load_settings`, single-pass `play_activity_find_all`, Package Manager `cJSON_Delete` + single installed-file check.
26. 📚 **README refresh** — passes 2–4 folded into the categories above.
27. 🧅 **README front page** — at-a-glance comparison, before/after tables, first on-device timings.
28. 🩹 **On-device fixes 1** — [`874ea325`](https://github.com/Amiga500/Onion/releases/tag/OnionPlus-v4.4.0-beta-20260925-874ea325): Wi-Fi bring-up off the boot path, play time after clock jumps (`ntpdate` path + 24 h guard), GameSwitcher capture scaling regression.
29. 🕒 **On-device fixes 2** — [`10f2369e`](https://github.com/Amiga500/Onion/commit/10f2369e): boot no longer waits with "Enable Wi-Fi temporarily" set, time zone kept on failed lookups, `ntpdate` sync marked. Boot 5.51 s → **2.28 s**.
30. 🔌 **Faster `boot_init`** — [`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc): audio server polled every 50 ms, swap in the background, finer boot timing marks. Boot → **1.74 s**.
31. 📚 **README polish** — Mini Flip naming, grouped at-a-glance tables, full review, OnionPlus presented as an independent build offered back to Onion.
32. 📴 **Temporary Wi-Fi at boot** — [`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02): "Enable Wi-Fi temporarily" syncs the time in the background (on → sync → update check → off), 30 s IP wait in the background paths. Boot with Wi-Fi off 5.16 s → **1.78 s**, time synced.

> 🔍 Per-commit detail: `git log --stat 07505ea5..HEAD` on `onionplus-compact`.

---

## ✅ Final word

`onionplus-compact` is **41 commits** ahead of upstream `OnionUI/Onion:main` through the last
code commit (`07505ea5` → `9768ae02`; README-only commits not counted; the long `OnionPlus`
branch was 97). In one tree:

- 🖼️ **8 vectorized NEON kernels**, a dozen O(n²)→O(n) rewrites and five render/UI caches;
- 🔋 a power/battery batch and a syscall diet that removed every avoidable `system()` call
  from the hardened core and from keymon;
- 🛡️ **25 issues in the code shared with Onion** fixed, with the fixes available to the Onion team,
  plus OnionPlus's own regressions caught by review;
- 🧪 a **68-suite / 1,423-test** host test harness that did not exist before this branch;
- 🕹️ an **AdvanceMENU** pass, a **Miyoo Mini Flip** port from `v4.5-dev` (not a merge), the
  OnionUI-parity and A–G reviews, and the **@robcodedev** ports of `OnionUI/Onion` **#1936–#1946**;
- ⚡ the **2026-09-25** performance passes and the fixes found **on the device itself**.

Measured on a Miyoo Mini+, this build boots to the menu in **~1.7 s** with Wi-Fi on or off
(5.51 s and 5.16 s before) and spends about **half a second** of its own work around each
game. Base
remains `4.4.0-beta`; OTA stays on `Amiga500/Onion`. Still open: Mini Flip lid/Hall
confirmation on hardware, the network script's `disable_flag` defect, and on-device samples
from more models (see [Known issues & next steps](#-known-issues--next-steps)).

---

<sub>Repository: [Amiga500/Onion](https://github.com/Amiga500/Onion) · Branch: `onionplus-compact` ·
Base: [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) (`OnionUI/Onion:main`) → last code [`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02) (**41**,
`git rev-list --count 07505ea5..9768ae02`) · On-device figures: Miyoo Mini+ · Refreshed **2026-09-26**</sub>
