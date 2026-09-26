<div align="center">

# 🧅⚡ OnionPlus

**The same Onion you know — with its hot paths rebuilt.**

An independent, performance- and reliability-focused build of [Onion](https://github.com/OnionUI/Onion) `4.4.0-beta`<br>
for the Miyoo Mini, Mini+, Mini v4 and Mini Flip.

[![branch](https://img.shields.io/badge/branch-onionplus--compact-8A2BE2?style=for-the-badge&logo=git)](https://github.com/Amiga500/Onion/tree/onionplus-compact)
[![commits](https://img.shields.io/badge/commits-59-blueviolet?style=for-the-badge)](#-11--commit-timeline)
[![files](https://img.shields.io/badge/files%20changed-191-blue?style=for-the-badge)](#-10--grand-totals)
[![neon](https://img.shields.io/badge/NEON%20kernels-8-orange?style=for-the-badge)](#%EF%B8%8F-1--vectorized-pixel-paths-neon)
[![tests](https://img.shields.io/badge/tests-1%2C459%20%2F%2071%2C522%20assertions-success?style=for-the-badge)](#-8--testing)
[![ota](https://img.shields.io/badge/updates-OTA%20enabled-2ea44f?style=for-the-badge)](#-install--update)
[![fixes](https://img.shields.io/badge/fixes%20ready%20for%20Onion-29-critical?style=for-the-badge)](#%EF%B8%8F-6--security--memory-hardening)
[![boot](https://img.shields.io/badge/boot%20to%20menu-~1.7%20s%20(Mini%2B)-brightgreen?style=for-the-badge)](#-measured-on-a-miyoo-mini)

✨ [Highlights](#-highlights) ·
🎮 [What you'll notice](#-what-youll-notice) ·
📱 [Measured](#-measured-on-a-miyoo-mini) ·
📦 [Install](#-install--update) ·
🧭 [Known issues](#-known-issues--next-steps) ·
📚 [Technical reference](#-technical-reference)

</div>

---

> [!NOTE]
> **OnionPlus is a personal, independent build — not a replacement for Onion and not an
> official release.** It explores how far Onion's everyday paths can be optimized: what
> happens every time you press a key, launch a game, go back to the menu, put the device to
> sleep or change the volume. It keeps Onion's look, menus, emulators and file layout.
> Everything here is built on the Onion team's work, and any change they find worthwhile is
> theirs to take.

## ✨ Highlights

<table>
<tr>
<td align="center" width="25%"><h3>🚀 ~1.7 s</h3>boot to the menu<br><sub>Mini+, Wi-Fi on or off<br>(5.5 s at first measurement)</sub></td>
<td align="center" width="25%"><h3>⏱️ ~0.5 s</h3>Onion's own work<br>around each game<br><sub>launch · exit · menu</sub></td>
<td align="center" width="25%"><h3>🐛 29</h3>defects fixed in code<br>shared with Onion<br><sub>fixes ready for upstream</sub></td>
<td align="center" width="25%"><h3>🧪 1,459</h3>host tests<br><sub>71,522 assertions<br>(1 at the base)</sub></td>
</tr>
</table>

### 🚀 Speed

| | ⚪ OnionUI | 🟢 OnionPlus | 💬 What it means |
|:--|:-:|:-:|:--|
| 🔌 Boot to the menu, Onion's part (Mini+, Wi-Fi on) | 3 blocking waits | **1.74 s** 📱 | 3.2× faster than the first measured build (5.51 s); 1.78 s with temporary Wi-Fi (5.16 s) |
| ⏱️ Onion's own work around a game | — | **~0.5 s** 📱 | launch 0.16 s · exit 0.1 s · back to the menu 0.1 s |
| 🎮 Menu CPU while idle | 100% of a core | **sleeps** | GameSwitcher, Tweaks, Play Activity & co. stop heating the device |
| 🌙 Blue-light schedule, every 15 s | ~20 processes | **0** | checked in-process: no more key freezes of up to 4 s |
| 🧩 Processes to parse a game launch | ~25 | **~2** | −90% between pressing A and the emulator starting |
| 💾 SD-card writes per volume step | ~14 | **1** | −93%: 12 fewer flushes to the card on every press |
| 🔁 SD-card flushes per return to the menu | 2 | **1** | −50% after every game |
| 📊 Opening Play Activity (60,000 sessions) | 66 ms | **34 ms** 📏 | 2× faster, identical results |

### 🛡️ Reliability & quality

| | ⚪ OnionUI | 🟢 OnionPlus | 💬 What it means |
|:--|:-:|:-:|:--|
| 🐛 Defects found in the shared code | — | **29 fixed** | crashes, leaks, lost settings, a wrong clock, a stack overflow — [listed in §6](#%EF%B8%8F-6--security--memory-hardening) |
| ⚡ Settings that survive a power cut mid-write | none | **all** | `system.json`, key map, config values, JSON, recent games, `retroarch.cfg` edits |
| 🧠 Memory leaked per MainUI-cache lookup | ~570 KB | **0** 📏 | was tens of MB with a large GameSwitcher history, on a 128 MB device |
| 🧪 Automated tests | 1 | **1,459** | 70 suites, 71,522 assertions, run on any PC in a few seconds |
| 🖼️ NEON (SIMD) pixel kernels | 0 | **8** | vectorized pixel conversion, rotation and alpha |
| 🐚 `system()` calls in the C code | 73 | **46** | −37% shells spawned |
| ⚠️ Unbounded string calls | 347 | **235** | −32%; none left in the hardened core |

<sub>📱 measured on a Miyoo Mini+ · 📏 measured on a host machine · everything else counted
from the code of both projects — see [how these numbers were obtained](#-how-these-numbers-were-obtained).</sub>

---

## 🎮 What you'll notice

> 🟩 Things you see and feel on the device. Nothing to configure.

- ⚡ **It boots to the menu in under two seconds of Onion time**, with Wi-Fi on or off. Wi-Fi,
  network services and time sync come up in the background a few seconds after the menu,
  instead of holding the boot (unless you ask for **Wait for sync on startup**).
- 📶 **"Enable Wi-Fi temporarily" really sets the clock.** With Wi-Fi off it used to turn Wi-Fi
  on and off at boot without syncing anything; now it turns Wi-Fi on in the background,
  syncs the time, and turns it off again.
- 🌡️ **Menus stop heating the device.** In stock Onion the GameSwitcher, Tweaks, Play Activity,
  Themes, Package Manager and Battery Monitor keep one CPU core at 100% even while you are
  just looking at them. OnionPlus sleeps until the next frame.
- 💾 **Volume and brightness don't hammer the SD card.** One step used to rewrite about fourteen
  files, each flushed to the card. Now it is one file, written 0.5 s after your last press.
- 🔊 **Your volume is actually remembered.** Stock Onion saves a change only on the first key
  press after the next 15-second tick; switch off before that and it's gone.
- 🧠 **The GameSwitcher doesn't eat memory.** Every recent game triggered a lookup that leaked
  about half a megabyte on large collections. Screenshots are now decoded in the background
  with the neighbours preloaded.
- 🖼️ **Save-state previews match the slot you pick** in the GameSwitcher's Load menu.
- 🌙 **No more freezes when the blue-light schedule kicks in.** Keymon used to run a script
  every 15 seconds and ignore your keys while it ran — up to ~4 s during a transition.
- 🔌 **Settings survive a dead battery.** `system.json`, the key map, every config file and the
  RetroArch options changed from Tweaks are written to a temporary file, flushed, and
  swapped in atomically.
- 📋 **Your recent games list stays correct.** Stock Onion can delete the wrong game from the
  recents when an entry is longer than 1 KB; OnionPlus counts lines the same way everywhere.
- 🕒 **The clock and time zone stay right.** A failed time-zone lookup no longer resets your
  zone to UTC, and a successful sync is not repeated after every game.
- 🎮 **Play time can't jump by decades.** Setting the clock from the network could add the whole
  jump (from 1970, on a fresh device) to the game being played; it can't anymore.
- 🔤 **Korean and full-width game names** use the CJK font in Play Activity.
- 🚀 **Faster launches, faster returns.** Dozens of helper processes removed from the path
  between pressing A and seeing the game, and between quitting and seeing the menu.

---

## 📱 Measured on a Miyoo Mini+

> 🟦 Real numbers from a real device, taken with the built-in timing log.

Miyoo Mini+ (640×480 panel), logging on, network time on, "Enable Wi-Fi temporarily" on,
"Disable services in game" on. Each session: boot, three games launched from MainUI
(PlayStation, NES, SNES, BS-X, …) and quit from the RetroArch menu, GameSwitcher opened
once. Numbers come from the [timing log](#%EF%B8%8F-measuring-on-the-device).

### 🔌 Boot with Wi-Fi on, step by step

| Phase | First measurement<br>`1592866e` | Wi-Fi off the boot<br>`10f2369e` | **Faster `boot_init`**<br>**`29791efc`** |
|:--|--:|--:|--:|
| 🐧 Kernel → Onion *(firmware, not Onion)* | 3 s | 2 s | 2 s |
| ⚙️ `boot_init` | 1.53 s | 1.59 s | **1.05 s** |
| 📶 `boot_network` | 3.34 s | **0.09 s** | 0.09 s |
| 🏁 **`boot` — Onion start → menu** | **5.51 s** | **2.28 s** | **1.74 s** |

```
Onion boot to the menu, Miyoo Mini+, Wi-Fi on

first measurement        ██████████████████████████████████████  5.51 s
Wi-Fi off the boot path  ████████████████                        2.28 s
faster boot_init         ████████████                            1.74 s   (3.2× faster)
```

Where the remaining `boot_init` goes: swap **0.00–0.01 s** (now in the background), audio
server **0.27–0.29 s**, display detection **0.37–0.41 s**, the rest **~0.36 s** (of which
0.25 s is a fixed wait after display init, kept on purpose).

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
| 🎮 `game_prepare` | **0.15–0.20 s** | from pressing A to the emulator or app starting |
| 🚪 `game_exit` | **0.09–0.27 s** | from quitting the emulator to the post-processing done |
| 🏠 `mainui_prepare` | **0.04–0.20 s** | before MainUI starts (battery icon drawn only when it changed) |
| ↩️ `mainui_return` | **0.06–0.10 s** | after MainUI exits (2.2 s when Wi-Fi was just turned on from the menu — [under investigation](#-known-issues--next-steps)) |
| 🔀 `switcher_prepare` | **0.01–0.02 s** | before the GameSwitcher starts |

- ⏱️ **Onion's own work around a game is about half a second in total.** The rest of the wait is
  RetroArch and the core loading the game, and MainUI starting, which is a closed binary.
- 📶 **Network in the background:** Wi-Fi and SSH/FTP/HTTP/Samba/Telnet come up about three
  seconds after the menu is on screen; the time sync follows as soon as the router hands out
  an address. Nothing waits for them.

> [!TIP]
> **One device so far.** Post your `timing.log` (Mini, Mini v4, Mini Flip especially) in an
> issue to extend these tables — see [Install & update](#-install--update) for how to enable it.

---

## ⏱️ Every action, before and after

> 🟧 What the system does behind the scenes for each thing you do, counted from the code of
> both projects.

<details open>
<summary><b>🔌 Powering on</b></summary>

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 📶 Wi-Fi and network services | brought up **inside** the boot (fixed 2 s wait) | ✅ **in the background** (unless Wait for sync is on) |
| 📴 "Enable Wi-Fi temporarily", Wi-Fi off | Wi-Fi on and off at boot, **no time sync** (3.4 s lost) | ✅ **background: on → sync → off** |
| 💾 128 MB swap file | `swapon` before the boot continues | ✅ **in the background** |
| 🔊 Audio server | fixed 0.5 s wait after starting it | ✅ **checked every 50 ms** (512 → 61 ms 📏) |
| ☀️ Brightness setting | `jsonval` process | ✅ **shell built-ins** |
| 🥾 `system.json` | rewritten in place | ✅ **temp file → flush → rename** |
| 📟 Device model | AXP probe only (Mini / Mini+) | ✅ **Hall sensor → Mini Flip, then AXP → Mini+**, same order as the installer |

</details>

<details open>
<summary><b>🕒 Setting the clock from the network</b></summary>

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 🎮 Game being played during the change | `ntpdate` adds the whole jump to it (decades from 1970) | ✅ **session closed and reopened**; >24 h discarded |
| 🌍 Time-zone lookup fails | zone rewritten as **UTC** | ✅ **your zone is kept** |
| 🔁 After a successful `ntpdate` | synced again after every game | ✅ **marked as done** |
| ⏳ Waiting for an IP address at boot | 10 s, then the sync is skipped | ✅ **30 s in the background** (a router took 29 s 📱) |

</details>

<details open>
<summary><b>🔊 Pressing volume or brightness</b></summary>

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 📄 Files written | ~14 (12 config values + key map + `system.json`) | ✅ **1** (`system.json`, only fields that changed) |
| 💾 `fsync` to the SD card | 13 | ✅ **1** |
| 🗂️ Extra FAT metadata operations | 12 flags × create + delete, 5 deletes | ✅ **0** |
| ⏲️ When it is saved | on the first key press **after** the next 15 s tick | ✅ **0.5 s after your last press** |
| 🔁 After saving | keymon re-reads all its own settings, then flushes every filesystem | ✅ nothing |

</details>

<details open>
<summary><b>⌨️ Every key press (keymon)</b></summary>

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 💾 Global `sync()` after touching flags in `/tmp` (RAM) | yes | ✅ **no** |
| 🔎 Config flags checked on the SD card | up to 3 `stat` calls | ✅ **0** (cached) |
| ⚡ CPU-clock hotkey | spawns `cpuclock` twice, overflows a 5-byte buffer | ✅ **once, no overflow** |
| 🐚 Shell spawns (`touch`, scripts, `playActivity`) | via `system()` | ✅ **direct `fork`/`exec`** |

</details>

<details open>
<summary><b>🌙 Every 15 seconds, with the blue-light schedule on</b></summary>

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| ⚙️ Work done | runs `blue_light.sh check`: 2 global syncs, ~20 processes | ✅ **compares two times in C** |
| ⌨️ Keys ignored while it runs | yes, up to ~4 s on a transition | ✅ **never** |

</details>

<details open>
<summary><b>🎮 Launching a game</b></summary>

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 🐚 Helper processes to parse the launch command | ~25 (`echo`/`grep`/`awk`/`sed`…) | ✅ **~2** |
| 💾 `cmd_to_run.sh` rewritten on the SD card | **every launch** (a check that is always true) | ✅ only when the path contains `$` |
| 🔊 Audio-server volume computed | every launch (3 processes) | ✅ only if the server isn't running |
| 🗃️ Play-history cache query | full scan of the MainUI cache, every launch | ✅ **only for games never seen before** |
| 📂 Play-history database opened | twice | ✅ **once**, indexed by path |
| 🖥️ Mini+/Mini Flip 560p check | 4–5 processes, command file read 2–3× | ✅ **0** |
| 🔁 Global syncs in the main loop | 4 per loop | ✅ **1**, when a game or app exits |

</details>

<details open>
<summary><b>🏠 Returning to the menu</b></summary>

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 🔋 Battery icon | decoded, rendered and **written to the SD card every time** | ✅ **only when the theme or percentage changed** |
| 💾 Global SD flushes | 2 | ✅ **1** |
| 🔎 Checking which MainUI is mounted | ~6 processes | ✅ **0** |
| 📄 Reading `system.json` (Wi-Fi ×2, theme) | 3 `jsonval` processes | ✅ **0** |
| 🖥️ Mini+/Mini Flip framebuffer probe parsing | 8 processes | ✅ **0** |

</details>

<details open>
<summary><b>😴 Sleep and wake</b></summary>

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 📊 Closing the play session | shell + full scan of the play history | ✅ **no shell, indexed, one transaction** |
| ⏱️ Host timing at 60,000 sessions | 10.8 ms | ✅ **5.0 ms** 📏 |
| 🧾 Process list parsing | breaks on names with spaces; one slot past the array | ✅ **robust parsing, bounded** |

</details>

<details open>
<summary><b>🌐 Leaving a game with SSH / FTP / Samba / HTTP / Telnet enabled</b></summary>

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 💾 Global syncs when services restart | up to 5 | ✅ **0** |
| 📶 `jsonval` processes to read the Wi-Fi setting | 8–10 | ✅ **1** |
| 🔌 Service toggles when Wi-Fi is off | meant to be switched off, but never were (wrong file name) | ✅ **switched off** (to be confirmed on a device) |

</details>

<details open>
<summary><b>🔁 Recent games list</b></summary>

| | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 🧹 Removing duplicates when the GameSwitcher opens | one full file rewrite **per duplicate** | ✅ **one rewrite total** |
| ⚡ Quick switch (move a game to the top) | two full rewrites, a moment with no file at all | ✅ **one atomic rewrite** |
| 🔢 Line numbering | three functions counting lines three different ways | ✅ **one way everywhere** |

</details>

---

## 🔌 Your settings survive power cuts

> 🟥 A flat battery at the wrong moment used to cost you your settings.

The Miyoo Mini has no battery-backed shutdown: a flat battery or a yanked cable can stop the
system in the middle of a write. In stock Onion these files are truncated first and rewritten
afterwards — a cut in between leaves them **empty**.

| File | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| ⚙️ `system.json` (volume, brightness, theme, Wi-Fi…) | truncated, then rewritten | ✅ **temp file → `fsync` → atomic rename** |
| 🥾 `system.json` at every boot | rewritten in place; an empty per-device file **wipes it** | ✅ **only replaced by a complete, non-empty file** |
| 🎮 `keymap.json` | truncated, then rewritten | ✅ **atomic** |
| 🔧 Every `config/` value | truncated, then rewritten | ✅ **atomic** |
| 🧾 Every JSON written by the system (`json_save`) | truncated, then rewritten | ✅ **atomic** |
| 📋 Recent games list | deleted, then renamed | ✅ **atomic** |
| 🕹️ `retroarch.cfg` edits from Tweaks | deleted, then renamed; a write error replaced it with a short copy | ✅ **atomic; on a write error the original is kept** |

Symlinked files are resolved first, so the link itself is never replaced.

## 🧠 Memory that stays free

> 🟥 Leaks that grew with your collection, on a device with 128 MB of RAM.

| Leak | ⚪ OnionUI | 🟢 OnionPlus |
|:--|:--|:--|
| 🗃️ MainUI cache lookup (GameSwitcher names, new games) | SQLite connection leaked **every lookup** — ~570 KB + 1 descriptor on a 5,000-game cache 📏 | ✅ **0** |
| 🗃️ Cache lookup with no cache database | name buffer leaked | ✅ **0** |
| 📦 Package Manager, per emulator package | whole parsed config leaked (`free` instead of `cJSON_Delete`) | ✅ **0** |
| ⚙️ System property save | JSON tree leaked when the value was unchanged | ✅ **0** |
| 🖼️ GameSwitcher screenshots | loader thread never joined; surfaces freed from two threads | ✅ **single owner, joined at exit** |

On a 128 MB device with the GameSwitcher showing up to 100 recent games, the first leak alone
could grow to tens of megabytes.

---

## 📦 Install & update

> 🟩 Same install as Onion; updates arrive on the device.

| | |
|:--|:--|
| 🎮 **Devices** | Miyoo Mini, Mini+ (measured above), Mini v4 and Mini Flip |
| 🧅 **Base** | OnionUI `4.4.0-beta` — themes, emulators, ROM folders and saves stay where Onion keeps them |
| 📡 **Updates** | built-in OTA from [`Amiga500/Onion` releases](https://github.com/Amiga500/Onion/releases) (`OnionPlus-v…` assets); **stable** follows the latest release, **beta** installs prereleases only |
| 🏷️ **Releases** | `OnionPlus-v4.4.0-beta-YYYYMMDD-<commit>`, built by GitHub Actions |
| ⏱️ **Timing log** | Tweaks → Advanced → Diagnostics → **Enable logging** → `.tmp_update/logs/timing.log`; **Util: System log snapshot** packs all logs into `SD:/log_export.7z` for sharing |
| 📖 **Settings reference** | Onion's own documentation: [Tweaks](https://onionui.github.io/docs/apps/tweaks) |

## 🧭 Known issues & next steps

> 🟧 What is known, and what comes next.

> [!IMPORTANT]
> **Waiting for a test on a device** — the fixes from [PR #221](https://github.com/Amiga500/Onion/pull/221)
> pass every host test, but these need a real console:
> - 🖼️ GameSwitcher → **Load** with two or more save slots: the preview follows the slot.
> - 🌐 **Wi-Fi off with a network service on:** the service toggle now switches off and stays off
>   when Wi-Fi comes back (upstream's intent; it never worked before).
> - 📟 **Model detection** on a Mini+ and a Mini Flip (`timing.log` / `/tmp/deviceModel`).
> - 🔋 **Mini Flip** suspend with the lid closed: powers off at the suspend timeout, as in `v4.5-dev`.
> - 🎮 **RetroArch app after a game**, then MENU → exit: the previous game's GameSwitcher picture is unchanged.
> - 🔤 A **Korean ROM name** in Play Activity; the **volume OSD** during a game on a 752×560 panel.

- ↩️ **Return to the menu after turning Wi-Fi on from MainUI** takes ~2.2 s instead of ~0.1 s
  (seen twice); a global `sync` or freeing memory is the suspect, finer timing marks will tell.
- 🔌 **Boot:** display detection (~0.4 s) and audio-server start (~0.3 s) are the largest parts
  left of the ~1.7 s Onion boot; the ~2 s before Onion starts belong to the firmware.
- 🕒 **Time sync:** on networks where the web time services fail, the time comes from `ntpdate`
  a few seconds later. It no longer blocks anything.
- 🔋 **Mini Flip:** lid/Hall-sensor handling follows `OnionUI/Onion:v4.5-dev` and is still to be
  confirmed on hardware.
- 📏 **Measurements:** on-device numbers come from one Mini+ so far.
- 🧪 **Tests:** 31 of the 70 suites still check a local copy of the code rather than the
  production header; they are being moved over (see [§8](#-8--testing)).

## 🤝 Credits & giving back

> 🟩 Built on Onion, offered back to Onion.

- 🧅 **OnionPlus exists because of [Onion](https://github.com/OnionUI/Onion)** and the OnionUI team
  and contributors: the menus, the emulator setup, the themes and almost all of the code are
  theirs. This build is not affiliated with or endorsed by the Onion team. For what each
  setting does, see [Onion's documentation](https://onionui.github.io/docs).
- 🙏 Thanks to **@robcodedev**, whose still-open Onion pull requests #1936–#1946 are carried here.
- 📬 **For the Onion team:** if any of these changes would be useful as pull requests, I'm happy
  to split them out and adapt them to Onion's own branches. The fixes that apply to Onion as
  it is today — the MainUI-cache memory leak and the cache-path stack overflow, the time zone
  and play time after a clock change, Wi-Fi no longer holding the boot, the recent-list line
  numbering, the network `disable_flag` — are the first candidates.

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
# host test suite (and the ASan/UBSan subset)
make unit-test
cd test && make -f Makefile.unit unit-test-san
```

**Not claimed here:** frame rates or battery life. The changes remove work from the paths that
decide them, but this page only publishes numbers that were counted or measured.

---

<div align="center">

## 📚 Technical reference

Everything below documents each change by category, with commit links, for reviewers and
maintainers.

</div>

| | Section |
|:--:|:--|
| 🎯 | [Scope and baseline](#-scope-and-baseline) |
| 🖼️ | [1 · Vectorized pixel paths (NEON)](#%EF%B8%8F-1--vectorized-pixel-paths-neon) |
| ⚡ | [2 · Algorithmic wins (O(n²) → O(n))](#-2--algorithmic-wins-on²--on) |
| 🎨 | [3 · Rendering & UI caches](#-3--rendering--ui-caches) |
| 🔋 | [4 · Power, battery & idle CPU](#-4--power-battery--idle-cpu) |
| ⚙️ | [5 · Process & syscall diet](#%EF%B8%8F-5--process--syscall-diet) |
| 🛡️ | [6 · Security & memory hardening](#%EF%B8%8F-6--security--memory-hardening) |
| 🕹️ | [7 · AdvanceMENU frontend](#%EF%B8%8F-7--advancemenu-frontend) |
| 🧪 | [8 · Testing](#-8--testing) |
| 🏗️ | [9 · Build, CI & release](#%EF%B8%8F-9--build-ci--release) |
| 📊 | [10 · Grand totals](#-10--grand-totals) |
| 🔀 | [11 · Commit timeline](#-11--commit-timeline) |

## 🎯 Scope and baseline

OnionPlus is measured against **[`OnionUI/Onion:main`](https://github.com/OnionUI/Onion/tree/main)**
at merge-base [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) (`4.4.0-beta`).
Code-level percentages in this document are **OnionPlus vs `OnionUI/Onion:main`**; on-device
timings compare **OnionPlus builds on the same Miyoo Mini+** (OnionUI itself has not been
timed on the device).

The integration branch is **[`onionplus-compact`](https://github.com/Amiga500/Onion/tree/onionplus-compact)**.
On top of `07505ea5` it carries, oldest first:

1. the long `OnionPlus` history (97 commits to [`fa5bb007`](https://github.com/Amiga500/Onion/commit/fa5bb007))
   squashed into topic commits: NEON kernels, hardening, the power/CPU batch, the host test
   harness, an **AdvanceMENU** pass and a **surgical Miyoo Mini Flip port** from
   `OnionUI/Onion:v4.5-dev` that does **not** merge that branch;
2. the **@robcodedev** ports of still-open [`OnionUI/Onion` PRs #1936–#1946](https://github.com/OnionUI/Onion/pulls?q=1936)
   ([Amiga500 #217](https://github.com/Amiga500/Onion/pull/217));
3. the **2026-09-09 review** fixes ([`fbd26d06`](https://github.com/Amiga500/Onion/commit/fbd26d06), [`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e));
4. the **2026-09-25 performance passes** [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804),
   [`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21), [`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8),
   [`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e);
5. four rounds of fixes driven by the on-device timing log:
   [`874ea325`](https://github.com/Amiga500/Onion/releases/tag/OnionPlus-v4.4.0-beta-20260925-874ea325),
   [`10f2369e`](https://github.com/Amiga500/Onion/commit/10f2369e),
   [`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc),
   [`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02);
6. the **2026-09-26 correctness review (Pass 1)**, findings F1–F14, merged in
   [PR #221](https://github.com/Amiga500/Onion/pull/221) — one concern per commit, each with a
   host test where the code allows it.

Every pass reaches installs through the built-in **OTA updater**. This README is the single
reference for the branch and groups everything shipped to date by *category* rather than by
commit.

<details open>
<summary><b>🔑 Reading the icons</b></summary>

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

</details>

---

## 🖼️ 1 · Vectorized pixel paths (NEON)

> 🟥 Whole scalar loops replaced by ARM NEON vector kernels, each with a scalar C fallback so
> non-NEON builds still work.

| Kernel | What it replaced | Speedup | Evidence |
|:--|:--|--:|:--:|
| 🔄 `neon_rotate180_inplace` | rotozoom blit + extra surface alloc | **+5000%** | 📏 |
| 🎨 `neon_swap_rb_inplace` | scalar per-pixel loop | **~+800%** | 📏 |
| 🎨 `neon_argb_to_rgba` | scalar per-pixel loop | **~+800%** | 📏 |
| 🎨 `neon_rgb888_to_argb` | scalar per-pixel loop | **~+800%** | 📏 |
| ⚪ `neon_gray8_to_argb` | scalar per-pixel loop | **~+600%** | 📏 |
| ⚪ `neon_gray8a_to_argb` | scalar per-pixel loop | **~+500%** | 📏 |
| 🎨 `neon_argb_to_rgba_alpha` | scalar per-pixel + branch | **~+600%** | 📏 |
| 🌫️ `surfaceSetAlpha` (NEON intrinsics) | float mul + `SDL_GetRGBA` | **~+400%** | 📏 |

- 📦 **8 kernels** (7 hand-written ARM assembly + 1 NEON intrinsics), all guarded by
  `#ifdef __ARM_NEON` with a correct scalar tail loop for the remainder.
- 🧪 Backed by `test_neon`, `test_neon_pixel` and `test_alpha_scale` — **109 tests / 67,353
  assertions** cross-checking NEON output against the scalar oracle.
- 🔬 Every scalar fallback runs on the x86-64 host CI; a separate `neon-arm` job cross-compiles
  the assembly and runs it under `qemu-user`.

## ⚡ 2 · Algorithmic wins (O(n²) → O(n))

> 🟥🟧 String and path handling rewritten to drop a re-scan hidden inside a loop.

**🔤 Strings & paths**

| Function | Before | After | Class | Evidence |
|:--|:--|:--|:--:|:--:|
| `str_count_char` | `strlen()` re-evaluated every iteration | single pointer walk | O(n²)→O(n), **−90%** | 📏🧪 |
| `file_removeExtension` | `strlen` + `strcpy` rescans | one scan + length-known `memcpy` | **−50% scans** | 📏🧪 |
| `file_path_relative_to` | `strcat` loop rescanning from byte 0 | explicit `offset` + `memcpy` | O(n²)→O(n) | 📏🧪 |
| `file_resolvePath` | `strcat` loop per path component | bounds-checked `memcpy` at `offset` | O(n²)→O(n) | 📐🧪 |
| `file_read()` | `fopen`+`fseek`×2+`ftell`+buffered `fread` | `stat64` + one `read()` loop | **2 seeks removed** | 📐🧪 |

**🗃️ State & databases**

| Function | Before | After | Class | Evidence |
|:--|:--|:--|:--:|:--:|
| 🔎 `system_state_update()` | one full `/proc` scan **per candidate** (up to 5) + a `cmd_to_run.sh` read per check | single `/proc` pass for all candidates, file read once | **5 scans → 1** | 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🗃️ playActivity `rom` lookup by `file_path` | full table scan on every start/stop | `rom_file_path_index` (created on open, no-op once present) | O(n)→O(log n) | 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🗃️ MainUI cache query on game start | `LIKE '%…' OR disp` full scan of the cache DB on **every** launch | run only for rows still missing type/name | **−1 full scan per launch** | 📐🧪* ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🗃️ `playActivity stop_all` (before every suspend) | full scan of `play_activity` + two transactions | `play_activity_play_time_index` + one transaction | O(n)→O(log n), 2 → 1 journal cycles | 📐🧪* ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)) |
| 📊 Play Activity app, `play_activity_find_all()` | the GROUP BY over the whole play history ran **twice** | one pass into a growing array | **66 → 34 ms** on host, 60,000 sessions | 📏🧪* ([`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e)) |
| 🎮 GameSwitcher `readHistory()` duplicates | a full rewrite of the recent list **per duplicate** | removed in **one** atomic rewrite (`file_delete_lines`) | O(d·n)→O(n) | 📐🧪* ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)) |

**🕹️ AdvanceMENU scripts**

| Function | Before | After | Class | Evidence |
|:--|:--|:--|:--:|:--:|
| 🕹️ `move_Roms_Without_Preview.ps1` | rescans the Snaps folder per ROM | Snaps folder read **once** into a lookup set | O(n²)→O(n) | 📐 |
| 🕹️ `move_incompatible_Roms.ps1` | linear XML scan per ROM | ROM names indexed into a hashtable | O(n²)→O(n) | 📐 |

> [!NOTE]
> Bonus fixes bundled in: `str_count_char` also closed a 1-byte over-read (`i <= strlen`);
> the path-assembly rewrites add `PATH_MAX` bounds checks the old `strcat` versions lacked;
> the PowerShell hashtable rewrite also fixes a crash on ROM names containing an apostrophe.

## 🎨 3 · Rendering & UI caches

> 🟥 Redrawing the same pixels every frame is the classic free win — cache it once, invalidate
> on change.

| Cache | Before | After | Impact |
|:--|:--|:--|:--|
| 🔤 TTF label / list / footer / header / dialog surfaces | `TTF_RenderUTF8_Blended` on **every frame** | cached `SDL_Surface`; list labels keyed on a hash of the label text, multi-value labels on a hash of the **rendered** value ([`f778946f`](https://github.com/Amiga500/Onion/commit/f778946f)); hidden rows dim a `SDL_ConvertSurface` copy so the cache is never mutated | **5–15 ms/frame saved** 📏 |
| 🖼️ List preview images | re-scaled with `zoomSurface` on every frame | scaled copy cached per width; `list_item_clearPreview()` drops it with the image, and a missing file is remembered with a flag instead of blanking the path ([`f778946f`](https://github.com/Amiga500/Onion/commit/f778946f)) | O(w·h) scale once per image 📐🧪 |
| 🖼️ infoPanel `drawImage()` | `zoomSurface()` + free on **every redraw** | scaled surface cached per (source, w, h) | O(w·h) scale eliminated on repeats 📐 |
| 🎮 playActivityUI page render | 4× `IMG_Load`+`SoftStretch`+alloc **per page flip** | 4 surfaces cached, reloaded only on page change | page flips skip all image I/O 📐 |
| 🖥️ `display_readOrWriteBuffer` | per-pixel loop on every row | `memcpy` fast path for contiguous rows, stride from the current mode ([`6dcc91a4`](https://github.com/Amiga500/Onion/commit/6dcc91a4)) | row copy vectorized 📐 |
| 🔋 MainUI battery icon (`mainUiBatPerc`) | theme background decoded, icon rendered, PNG **encoded and written to the SD card** on every return | skipped while theme, percentage and theme/override files are unchanged; never rendered while charging | full decode + encode + SD write → **0** when unchanged 📐🧪* ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)) |
| 🎮 GameSwitcher play time | Play Activity DB opened **on the UI thread** the first time each game is shown | computed by the preload worker with name and core (UI fallback kept) | no DB open on the UI thread for prefetched games 📐🧪* ([`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21)) |
| 🎮 GameSwitcher romscreens | PNG decode + scale **on the UI thread** | persistent worker decodes outside the lock and prefetches **±2** entries; only the UI thread frees surfaces (±5 window); the overlay's entry is **pinned** while autosave encodes it ([`a979fc30`](https://github.com/Amiga500/Onion/commit/a979fc30)) | scrolling no longer waits for decoding 📐🧪 |

Every cache ships with its own teardown: `list_free()` releases the TTF and preview slots,
`cleanImagesCache()` frees the infoPanel scaled cache, and `free_resources()` releases the
playActivityUI page cache. The romscreen worker is stopped and joined before
`freeRomScreens()` releases its surfaces.

## 🔋 4 · Power, battery & idle CPU

> 🟥🟧 Fewer wake-ups, fewer forked subprocesses, fewer duplicate sysfs writes.

**🔌 Boot**

| Subsystem | Before | After | Impact |
|:--|:--|:--|:--|
| 📶 Wi-Fi at boot | `update_networking.sh check` brought Wi-Fi up **inside** the boot (a fixed 2 s `sleep`) | background, unless **Wait for sync on startup** is on (or Wi-Fi is off and forced on) | 📱 **`boot_network` 3.34 s → 0.09 s** ([`874ea325`](https://github.com/Amiga500/Onion/releases/tag/OnionPlus-v4.4.0-beta-20260925-874ea325), [`10f2369e`](https://github.com/Amiga500/Onion/commit/10f2369e)) |
| 🔊 Audio server start (`runifnecessary`) | fixed `sleep 0.5` after starting, before checking | checked every 50 ms, same 0.5 s ceiling and 8 retries | 📏 **512 → 61 ms** · 📱 `boot_init` 1.59 → 1.05 s ([`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc)) |
| 📴 "Enable Wi-Fi temporarily", Wi-Fi off | Wi-Fi on and off **inside** the boot, time never synced unless Wait for sync was on | Wi-Fi on → sync → update check → off, **in the background**; IP wait 30 s | 📱 **`boot` 5.16 s → 1.78 s**, time synced ([`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02)) |
| 💾 128 MB swap file | `swapon` before the boot continues | `swapon` in the background | 📱 `boot_swap` **0.01 s** ([`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc)) |

**🌀 CPU & wake-ups**

| Subsystem | Before | After | Impact |
|:--|:--|:--|:--|
| 🔊 OSD volume/brightness bar thread | `usleep(100)` busy-wait (~10,000 loops/s) | `usleep(16000)` (~60 fps) | **idle CPU ~10% → <1%** 📏 |
| 🖼️ OSD overlay draw loop | full-throttle spin for the overlay's duration | `msleep(2)` per iteration + demoted logging | overlay CPU burn capped 📐 |
| 🌀 SDL UI loops (GameSwitcher, Tweaks, prompt, playActivityUI, themeSwitcher, packageManager, batteryMonitorUI + dialogs) | non-blocking poll with **no sleep**: one core at 100% | sleep until the next frame (or 15 ms) when idle | **idle UI CPU ~100% of a core → near 0** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🌙 keymon blue-light schedule | `blue_light.sh check` **every 15 s**, synchronous: 2 global `sync`, ~20 `fork`/`exec`, keys ignored for up to ~4 s | evaluated in-process; script started in background **only on a transition** | **~20 spawns/15 s → 0** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🪫 batmon low-battery thread | `usleep(0x4000)` (~16 ms) | `usleep(500000)` (500 ms) | **~−97% wake-ups** 📐 |
| ⏱️ batmon main loop | `config_get("battery/warnAt")` every tick | read only at check timeout | **−100% hot-loop config reads** 📐 |
| 🎮 GameSwitcher battery poll | `stat()` on `/tmp/percBat` every loop (~1 kHz) | checked once per second (matches batmon's write rate) | **~99.9% fewer `stat` calls** 📐 |

**💾 SD-card writes & syncs**

| Subsystem | Before | After | Impact |
|:--|:--|:--|:--|
| 💾 Volume / brightness step | `settings_save()` rewrote every setting: ~30 files, **13 `fsync`** | selective save against the on-disk snapshot | **1 file (`system.json`)** 📐🧪* ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 💾 keymon key handling | global `sync()` after deleting flags in `/tmp` (tmpfs) | no sync for tmpfs flags | **−1 global sync per flagged key** 📐 |
| 🔁 `runtime.sh` main loop | `pgrep keymon` + `touch` + global `sync`, **4× per loop** | `/proc/<pid>` check, shell builtin, one `sync` when a game/app exits | **4 syncs → 1** 📐 |
| 🔁 Return to MainUI (`check_hide_recents`) | runs twice per cycle, each ending in a global `sync` | `sync` only when a list is moved, plus one after MainUI exits | **2 syncs → 1 per cycle** 📐🧪* ([`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8)) |
| 🌐 `update_networking.sh check` (after every game while a service is on) | global `sync` before starting each service; SSH key-folder `sync` every run | no sync for service starts; SSH `sync` only when the folder is created | **up to 5 → 0 syncs per game exit** 📐 |
| 🗂️ Quick switch (move game to top of recents) | two full rewrites (add to top, then delete) | one atomic rewrite (`file_move_line_to_top`) | **2 → 1 SD rewrites** 🧪 |
| 🗄️ Play Activity DB | journal file created/deleted per transaction; `SQLITE_BUSY` on concurrent access | `journal_mode=TRUNCATE`, 2 s `busy_timeout`, `sqlite3_close_v2` | fewer FAT directory updates, no lost writes on contention 📐 |
| 🔋 batmon `/tmp/percBat` | `fsync` on tmpfs | write + `rename()` | **−1 useless fsync per % change** 📐 |

**🔋 Battery, display & rumble**

| Subsystem | Before | After | Impact |
|:--|:--|:--|:--|
| 🔌 `battery_isCharging()` (Mini+ and Mini Flip) | `fork`+`exec` of `axp_test` every call (~5–10 ms) | 2 s cached wrapper | **~−99% subprocess spawns** 📐 |
| 🔋 `battery_hasChanged` while charging | OnionPlus used to overwrite the `500` charging sentinel | early return like `OnionUI/Onion:main` | charging icon stays while plugged in 🛡️ |
| 🪫 `getBatPercMMP()` AXP percent | `axp_test` garbage (e.g. `1735289191`) and `-1` written to `/tmp/percBat` | last sane **0–100** kept | GS/keymon never read a bogus percent 🛡️ |
| 💡 `display_setBrightnessRaw` | sysfs write on **every** call | cached, duplicate writes skipped | **−100% duplicate PWM writes** 📏 |
| 📳 `rumble()` GPIO init | `export`+`direction` sysfs writes on **every** pulse | one-time init + retry if `gpio48` is missing | **−2 sysfs writes/pulse** 📏 |
| 🔆 AdvanceMENU quick-switch (PWM) | backlight PWM always re-enabled on exit | re-enabled **only** when returning from a game | fewer redundant PWM writes 📐 |

## ⚙️ 5 · Process & syscall diet

> 🟧 Every `system()` call forks a shell **and** the real binary — two processes for one line
> of intent. These were replaced with direct syscalls or `fork`+`exec`.

**🧱 C code**

| Call site | Before | After | Result |
|:--|:--|:--|:--|
| 📁 `mkdirs()` | `system("mkdir -p …")` | iterative `mkdir()` walk | **2 → 0 processes** |
| 📄 `file_copy()` | `system("cp -f …")` | `open`/`read`/`write` loop | **2 → 0 processes**, shell-injection surface closed |
| ⚙️ `config.h` `_config_prepare` | `system("mkdir -p …")` | direct `mkdirs()` | **2 → 0 processes** |
| 🎮 GS overlay `playActivity` | `system("… &")` | double-fork + `execl` (async, no zombies) | **−80% process overhead** 📏 |
| 🎮 GS overlay RetroArch kill/poll | `killall` / `pidof` shell-outs | `process_killall_signal` / `process_isRunning` | **−100% shell, killall semantics restored** |
| 🗄️ playActivity DB ops | 2× open/close per operation | 1× open/exec/close | **−50% DB I/O** 📏 |
| 🧹 Reset paths (tweaks/theme/RA overrides) | `rm -rf` via `system()` | `nftw()`-based `file_remove_recursive()` | shell-free recursive delete |
| 🐚 keymon `touch`, `tools favfix`, `playActivity stop_all/resume`, `bootScreen`, recorder / blue-light scripts | `system("… &")` / `system("touch …")` | `process_run_wait()` / double-fork `process_spawn_detached()` / `creat()` | **one shell less per call** ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| ⚡ keymon CPU-clock hotkey | `cpuclock` spawned **twice** per press | last value cached (reset on every state change) | **2 → 1 spawns** |
| ⌨️ keymon config flags (`.altBrightness`, `.cpuClockHotkey`, `.recHotkey`) | `stat()` on the SD card on every key | cached, refreshed on settings change + every 15 s | **−1 SD `stat` per key** |

**🐚 Shell scripts**

| Call site | Before | After | Result |
|:--|:--|:--|:--|
| 🌙 `blue_light.sh` time parsing | `echo \| cut \| xargs` ×2 + `awk` per value | shell parameter expansion | **~6 → 0 forks per value** |
| 🚀 `runtime.sh` launch-command parsing | `echo \| grep/awk/basename/dirname` pipelines | parameter expansion (multi-line commands keep awk) | **~15 fewer processes per launch** 🧪* |
| 🔊 `start_audioserver` on every launch | `jsonval` + `awk` + subshell to compute a volume, even when already running | skipped when audioserver is running | **3 → 0 processes** in the usual case |
| 🖥️ `fbmode --probe` parsing | 2–4 `echo \| awk` / `echo \| cut` pipelines | `fb_probe_fields` (builtins, globbing off) | **4–8 → 0 processes** 🧪* |
| 🔎 `mount_main_ui` | `cat \| grep \| cut` + `basename` + subshell | `read` loop over `/proc/self/mountinfo` | **~6 → 0 processes per return** 🧪* |
| 📶 `update_networking.sh` Wi-Fi checks | `jsonval` on **every** `wifi_enabled`/`wifi_disabled` (8–10 per run) | read once per `check` run | **8–10 → 1 process** 🧪* |
| 🖥️ `get_full_resolution_path` (Mini+/Mini Flip, every launch) | `cmd_to_run.sh` re-read 2–3× with `grep`/`cut`/`sed` | one builtin read + parameter expansion | **4–5 → 0 processes** 🧪* |
| 🎮 `get_info_value` (game with a custom core) | `echo \| grep \| awk \| awk \| tr` | builtin loop, same word-boundary match | **5 → 0 processes** 🧪* |
| 📄 `system.json` reads around MainUI | `jsonval` process per read | `sysjson_get` (builtins; `jsonval` fallback) | **3 → 0 processes per MainUI cycle** 🧪* |
| ☀️ Brightness at boot (`init_system`) | `jsonval brightness` process | `sysjson_get` | **1 → 0 processes** |

**🕹️ AdvanceMENU**

| Call site | Before | After | Result |
|:--|:--|:--|:--|
| 📝 romscripts | temp file written **CWD-relative** | temp file next to `advmenu.rc`, PID-suffixed | race + read-only-CWD failure fixed |
| 🚦 `launch.sh` | no reentrancy guard | early exit if `advmenu` is already running | duplicate-instance guard |

Across the 25-file hardening core, `system()` call sites went **3 → 1** (the survivor,
`process_start()`, has no live caller). On keymon's key-press and periodic paths every
`system()` is gone (`shutdown` on the power-off path remains).

## 🛡️ 6 · Security & memory hardening

> 🟦 No performance claim attached to anything in this section — correctness and memory safety.

| Category | Before → After | Count |
|:--|:--|--:|
| 🔴 Unbounded `sprintf` | → bounded `snprintf` | **23 → 0** |
| 🔴 Unbounded `strcpy` + `strcat` | → bounded copies / `memcpy` | **37 → 0** |
| 🔴 Non-reentrant `strtok` | → `strtok_r` with owned save-pointer | **4 → 0** |
| 🟢 NULL-pointer / I/O guards added | new `if (!ptr)` / return-value checks | **+57** *(25-file set)* |
| 🟢 Leaked descriptors closed | `fclose`/`close` on error paths | **+18** |
| 🟢 Division-by-zero guards added | early return before `% total_count` | **+2** |

**29 defects fixed in code shared with Onion:** 6 in the early passes, 7 in `fee6c4b`, 8 in
`85bc9f21` / `747d102a` / `1592866e`, 4 found with the on-device tests, 4 in the Pass 1
review. The lists below also include OnionPlus's own regressions caught by review, marked as
such.

<details open>
<summary><b>🩺 Pass 1 correctness review — <a href="https://github.com/Amiga500/Onion/pull/221">PR #221</a> (2026-09-26)</b></summary>

Present in `OnionUI/Onion:main`:

- 💥 **Stack overflow building cache-DB paths** — `cache_get_path_and_version()` passed
  `PATH_MAX` to `snprintf` into 256-byte buffers; a ROM under a long folder name wrote past the
  caller's stack (ASan: 326 bytes into 256). Runs on every launch and GameSwitcher name lookup.
  Buffer size now passed through, truncated paths never probed ([`939c1983`](https://github.com/Amiga500/Onion/commit/939c1983)).
- ⚡ **`retroarch.cfg` could be lost or truncated** — `file_changeKeyValue()` deleted the file
  before renaming the new one, and ignored write errors (a full card cut a 38 KB file to
  4 KB in the test). Now `file_atomic_begin/commit`; on any error the original is kept
  ([`d45cdd5b`](https://github.com/Amiga500/Onion/commit/d45cdd5b)).
- 🌐 **Network service toggles never switched off** — `disable_flag` built `".$flag_"` (an
  unset variable), so the move always failed ([`5def2330`](https://github.com/Amiga500/Onion/commit/5def2330)).
- 📜 **`file_readLastLine` dropped the final byte** of files of 255 bytes or more
  ([`e43b025b`](https://github.com/Amiga500/Onion/commit/e43b025b)).

OnionPlus regressions and port issues:

- 🖼️ **GameSwitcher Load preview stuck on the first slot** — the scaled preview was cached on
  width alone and survived a slot change ([`f778946f`](https://github.com/Amiga500/Onion/commit/f778946f)).
- 🎮 **Wrong game's GameSwitcher picture overwritten** — the recent-list lookup fell through to an
  older game when the top entry was an app; the romscreen now uses the top entry only
  ([`6702b433`](https://github.com/Amiga500/Onion/commit/6702b433)).
- 🧵 **Use-after-free in the overlay autosave** — scrolling 6+ entries during the save freed the
  surface being encoded ([`a979fc30`](https://github.com/Amiga500/Onion/commit/a979fc30)).
- 📟 **Mini Flip detected as a Mini** when the AXP probe failed — the Hall sensor is now checked
  first, as in the installer and `v4.5-dev` ([`4c2f330d`](https://github.com/Amiga500/Onion/commit/4c2f330d)).
- 🔋 **Closed Mini Flip never powered off** at the suspend timeout — restored `v4.5-dev`'s logic
  ([`bd7d9eb4`](https://github.com/Amiga500/Onion/commit/bd7d9eb4)).
- 🔤 **Korean names on the wrong font** — `includeCJK()` now decodes UTF-8 and covers Hangul
  and full-width forms ([`59a53a15`](https://github.com/Amiga500/Onion/commit/59a53a15)).
- 🖥️ **Stale framebuffer pitch** after a resolution change ([`6dcc91a4`](https://github.com/Amiga500/Onion/commit/6dcc91a4)),
  **uninitialised status** in `process_spawn_detached` ([`ad9c8078`](https://github.com/Amiga500/Onion/commit/ad9c8078)).

</details>

<details open>
<summary><b>📱 Fixed after the first on-device tests</b> — <code>874ea325</code>, <code>10f2369e</code>, <code>9768ae02</code></summary>

Present in `OnionUI/Onion:main` unless noted:

- 🕒 **Play time of decades after the clock is set** — the `ntpdate` fallback changed the clock
  without closing and reopening open play sessions; a session started at a 1970 clock got
  ~497,000 h. Now closed/reopened on every path, and sessions longer than 24 h (or negative)
  are discarded, which also cleans up existing bad rows.
- 🌍 **Time zone reset to UTC** — a failed time-zone lookup was converted to offset 0 and written
  to `.tz`; the zone is now only rewritten when a lookup returns a real offset.
- 🔁 **Time re-synced after every game** — only the web path marked the sync as done; a
  successful `ntpdate` now does too.
- 🖼️ *(OnionPlus regression)* GameSwitcher captures kept their aspect ratio on 752×560 panels
  since `fee6c4b`; the fill-the-screen rule is restored.
- 📶 **Boot waited for Wi-Fi with "Enable Wi-Fi temporarily" set** even with Wi-Fi on; the boot
  now waits only when something needs the network.
- 📴 **"Enable Wi-Fi temporarily" never synced the time** without "Wait for sync on startup":
  with Wi-Fi off it turned Wi-Fi on at boot, checked the Wi-Fi *setting* before syncing, and
  turned it off again. Now it syncs in the background ([`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02)).

</details>

<details open>
<summary><b>⚡ Fixed in the performance passes</b> — <code>fee6c4b</code>, <code>85bc9f21</code>, <code>747d102a</code>, <code>1592866e</code></summary>

All present in `OnionUI/Onion:main`:

- 💥 **keymon CPU-clock stack overflow** — `cpuclockstr[5]` was handed to a function that copies
  up to `STR_MAX` bytes: a stack overwrite on every use of the hotkey.
- 📦 **`suspendpid[]` overflow** — the bound allowed a write to `suspendpid[32]`.
- 🧾 **`/proc/<pid>/stat` parsing** — process names containing spaces shifted `state`/`ppid`/`flags`;
  now parsed from the last `)`.
- ⏲️ **Settings debounce never fired** — a volume change was saved on some later key press (or
  never, on power-off). Now saved 500 ms after the last change.
- 🧮 **`system.json` dirty check** — returning to the loaded value was silently not saved.
- ⚡ **Power loss during a settings write** — `system.json`, `keymap.json`, `json_save()` and
  `config_set*()` truncated the file in place; now `.tmp` + `fsync` + `rename()`.
- 🔒 **`blue_light.sh` lock race** — test-then-`touch` let two instances start; now an atomic
  `mkdir` lock.
- 🧠 **SQLite connection leak on every MainUI cache lookup** — about 570 KB and one descriptor per
  lookup on a 5,000-game cache; now `sqlite3_close_v2()`.
- 💧 **`cache_db_find()` leak** when no cache DB exists.
- 🗂️ **Recent list could lose the wrong entry** — three functions counted lines in different chunk
  sizes; all now use `getline()`.
- 💾 **`cmd_to_run.sh` rewritten on every launch** — the `$` check (`grep -q "\$"`) was always true.
- ⚡ **Recent list briefly missing** — deleted before the new one was renamed; now atomic.
- 🔗 **Quick switch merged two entries** when the moved last line had no trailing newline.
- 🥾 **`system.json` wiped at boot** by an empty per-device file or a power cut.
- 📦 **Package Manager leak** — `free()` instead of `cJSON_Delete()` per emulator package.

> [!NOTE]
> An alternative implementation of `747d102a` proposed on a `copilot/` branch was **not
> merged**: with recents hidden it cut the GameSwitcher history to the last game on every
> return to MainUI.

</details>

<details open>
<summary><b>🕵️ Early hardening passes</b></summary>

- 🔓 **`hash.h` FNV1A load** — removed a 7-byte out-of-bounds read, an unaligned 64-bit load
  (traps on ARMv7) and an oversized shift (UB). Hashes stay bit-identical — verified against
  264 reference vectors at 5 optimization levels.
- 🕳️ **`gs_popMenu.h` save thread** — no longer uses an uninitialised 4 KB stack buffer as a path.
- 🎯 **`currentGame()` NULL derefs** — 3 call sites guard against an empty game list.
- 🧮 **Dead slot-bounds check** — `selected_slot < 0 && selected_slot >= slot_count` could never be
  true; changed to `||`.
- 📖 **`_isContentNameInInfo` OOB read** — a match at offset 0 no longer reads before the buffer.
- 💾 **File I/O consistency** — `fsync()` before rename on key-value writes;
  `file_remove_recursive()` errors are logged.
- 🧩 **`const`-correctness** — `file_basename()` no longer discards `const`.
- 🕹️ **AdvanceMENU** — BIOS-set false positive fixed; `advmenu.rc` only overwritten after a
  complete rewrite; `mp4_to_mng.ps1` back on the HTTPS ffmpeg permalink with TLS 1.2.
- 🎲 **`randomGamePicker` division by zero** on an empty list.
- 🎨 **`batteryMonitorUI` / `themeSwitcher`** — NULL assets via `safeBlitSurface()`, graph and
  theme-count bounds.
- 📦 **`packageManager`** loading screen NULL check; **`gs_romscreen.h`** format-string bug.
- 🔤 *(OnionPlus)* list TTF cache dimming mutated cached pixels ([`fbd26d06`](https://github.com/Amiga500/Onion/commit/fbd26d06));
  AXP `percBat` garbage ([`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e));
  charging sentinel overwritten while plugged in.

</details>

## 🕹️ 7 · AdvanceMENU frontend

> 🟨🛡️ A self-contained optimization and hardening pass ([PR #210](https://github.com/Amiga500/Onion/pull/210)).

| Area | Change | Kind |
|:--|:--|:--:|
| 🔤 Fonts | `advmenu.rc` uses fonts bundled in `BIOS/.advance` instead of Onion core fonts | UX |
| 🔆 Backlight | PWM restored only on `quick_switch` return-from-game, not on every exit | power |
| 🚦 Single instance | `launch.sh` skips launch (with a log message) if AdvanceMENU is already running | robustness |
| 🐎 PowerShell tooling | `move_Roms_Without_Preview.ps1` / `move_incompatible_Roms.ps1` use lookup tables | O(n²)→O(n) |
| 📝 Romscripts | temp files next to `advmenu.rc` with a PID suffix; committed only on full success | atomicity |
| 🎬 Media tooling | `mp4_to_mng.ps1` ffmpeg download hardened (HTTPS, TLS 1.2, scoped search) | reliability |

## 🧪 8 · Testing

> 🟩 There were no host unit tests at the OnionPlus base commit. Everything below was added on
> this branch.

| Metric | Value |
|:--|--:|
| 🧪 Test suites | **70** |
| ✅ Tests | **1,459** |
| ✅ Assertions | **71,522** |
| ❌ Failures | **0** |
| 🧬 Sanitizer subset (ASan + UBSan) | **6 suites** — hash, file, str, json, neon, cache_db |
| 🔐 Security-focused suites | 10 suites · 222 tests · 969 assertions (15% of all tests) |

- 🏗️ **Host only** — no cross-toolchain, no SDL, no device — via `make unit-test`, so it works as a
  fast CI gate. A `neon-arm` CI job cross-compiles the NEON assembly and runs it under
  `qemu-user`; `unit-test-san` runs the sanitizer subset.
- 🚫 **Fails on implicit function declarations**, as clang and the device toolchain do, so a header
  that uses a function before declaring it cannot pass on the host and break the console build.
- 🐚 **`test_scripts.sh`** extracts functions from the shipped shell scripts (`disable_flag`,
  `detect_device_model`) and runs them against stubs — the real text, not a copy.
- 🗃️ **`test_cache_db`** now includes the production `cacheDB.h` (with a small sqlite3 link stub)
  and runs under ASan, including the long-folder overflow case.
- 🧯 `test_hash` locks in the hash bit-identity guarantee (15 tests / 350 assertions);
  `test_device_model` covers `MIYOO285` and the `HAS_AXP()` / `HAS_WIFI()` /
  `IS_MIYOO_PLUS_OR_FLIP()` macros.
- 🔬 Earlier passes were also checked with **out-of-tree** host programs (marked 🧪*): selective
  save and atomic writes on a mock `/mnt/SDCARD`, playActivity against real SQLite, a
  ThreadSanitizer stress of the romscreen worker, old-vs-new equivalence of every rewritten
  `runtime.sh` helper under dash and bash `--posix`, and the shell changes on a Miyoo Mini+.

> [!WARNING]
> **Coverage is narrower than the test count suggests.** 39 suites exercise production code;
> **31 still test a local copy** of the function they cover, so a change to the real code can
> pass them unnoticed. Moving them onto the production headers is ongoing work.

### ⏱️ Measuring on the device

[`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e) adds timing marks to
`runtime.sh`, active **only with logging on** (Tweaks → Advanced → Diagnostics → Enable
logging). They use `/proc/uptime` and shell builtins, so they are immune to the clock jump when
the time is restored at boot, and cost one file test per mark when logging is off. Results go
to `.tmp_update/logs/timing.log` (the previous session is kept as `timing.prev.log`).

| Mark | Measures |
|:--|:--|
| `boot: runtime.sh started N s after kernel start` | kernel + init before Onion |
| `boot_init` / `boot_network` / `boot` | `init_system`, `start_networking`, runtime start → first menu/game |
| `boot_swap` / `boot_audio` / `boot_display` | inside `boot_init`: swap, audio server, screen detection |
| `game_prepare` / `game_run` / `game_exit` | launch preparation, the session itself, post-processing |
| `mainui_prepare` / `mainui_session` / `mainui_return` | before, during and after MainUI |
| `switcher_prepare` / `switcher_session` | the same for the GameSwitcher |

## 🏗️ 9 · Build, CI & release

| Change | Detail |
|:--|:--|
| 📦 Release flags | `-O2 -ffunction-sections -fdata-sections -Wl,--gc-sections` → **−5–15% binary size** 📏 |
| 🎯 Host tests | `make unit-test`, host-only, zero device dependency; CI runs on push to `onionplus-compact` |
| ⚙️ Parallel `make` | `core` builds `bootScreen` + `gameSwitcher` first, then the other modules as independent targets; `make -j` works without two sub-makes racing on one `.o` |
| ♻️ Setup stamp | `cache/.setup` re-runs when `static/`, `lib/` or any `src/*/res`/`script` file is newer |
| 🗜️ RetroArch package | `retroarch.pak` reused from `cache/` while its content hash is unchanged |
| 📊 Opt-in profiling | `src/common/utils/perf.h` — `PERF_START`/`PERF_END` compile to nothing unless `-DPERF_ENABLED` |
| 🏷️ Release naming | `OnionPlus V4.4.0-beta-YYYYMMDD`, zip `OnionPlus-v…-<sha>.zip` — dated GitHub Releases. Base remains **4.4.0-beta** |
| 📡 OTA | `ota_update.sh` points at `Amiga500/Onion` and filters `OnionPlus-v` assets. Stable = `/releases/latest`; beta installs **only GitHub prereleases** |
| 📱 Mini Flip | Device id `285`, MainUI-285 binaries, lid-close Tweaks. Runtime and installer use the same order: `hall-mh248` sysfs node → **285 Mini Flip**, else AXP → **354 Mini+**, else **283 Mini**. `/dev/input/event*` is **not** a Mini Flip signal. Ported from `OnionUI/Onion:v4.5-dev` without merging that branch; lid/Hall **untested** on a physical Mini Flip |
| 🖥️ Boot framebuffer | Plus/Mini Flip keep polling `mi_fb0` when dmesg says 640; `commit_mainui_fbmode()` waits for the FB driver; installer preclears the framebuffer before device detection |
| 🧵 Signal handling | shared `signal_handler_quit()` across 6 apps; `volatile sig_atomic_t` for signal-shared state |

## 📊 10 · Grand totals

| Metric | Value |
|:--|--:|
| 🔧 Commits `07505ea5..41569da0` | **59** *(56 without merges; the long `OnionPlus` branch was 97)* |
| 📁 Files changed | **191** |
| ➕➖ Lines | **+31,786 / −1,494** |
| 🧩 Production (`src/` + `static/` + CI/Makefile) | **111 files · +6,767 / −1,468** |
| 🧪 Tests (`test/`) | **78 files · +24,004 / −10** |
| ⚡ NEON kernels | **8** (7 asm + 1 intrinsics) |
| 🧪 Test suites / tests / assertions | **70 / 1,459 / 71,522** — all passing |
| 🛡️ Unsafe `sprintf` / `strcpy`+`strcat` / `strtok` left (hardened set) | **0 / 0 / 0** |
| 🛡️ NULL guards / closed descriptors added | **+57 / +18** *(25-file set)* |
| 🔐 Defects fixed in code shared with Onion | **29** |
| 🕹️ AdvanceMENU scripts hardened/optimized | **7 files** |

Reproducible from git: `git rev-list --count 07505ea5..HEAD`, `git diff --shortstat 07505ea5..HEAD`,
`git show --stat <sha>`.

## 🔀 11 · Commit timeline

<details open>
<summary><b>🗓️ Oldest first — 34 steps</b></summary>

1. 🖼️ **NEON foundation** — vector kernels vs OnionUI scalar pixel loops.
2. 🛡️ **Hardening wave** — crash/memory-safety port across the common layer.
3. 🧪 **Test harness** — host unit-test scaffold added from scratch.
4. 📚 **Docs** — first optimization report published.
5. 🔋 **Power/CPU batch** — OSD busy-wait, brightness cache, battery cache, batmon, SQLite, config, GS overlay fork+exec, infoPanel hardening.
6. 🐛 **Defect fixes** — hash over-read, save-state uninitialised buffer, `const` cast, plus two external PRs (#206, #207).
7. 🔎 **Review pass 1** — `currentGame()` NULL derefs, async `playActivity` restored, dead slot check, OSD overlay throttle.
8. 🏗️ **Release/OTA** — dated GitHub Releases, `Amiga500/Onion` OTA wiring, `TARGET=OnionPlus`.
9. 🎯 **GameSwitcher fixes** — framebuffer stride and romscreen stretch corrections.
10. 🔎 **Review pass 2** — rumble caching, infoPanel image cache, GS battery-poll throttle, playActivityUI page cache, randomGamePicker dedup.
11. 🕹️ **AdvanceMENU pass** — fonts, PWM handling, script speedups, race-condition and false-positive fixes ([PR #210](https://github.com/Amiga500/Onion/pull/210)).
12. 🔎 **Review pass 3** — randomGamePicker division-by-zero guard, batteryMonitorUI/themeSwitcher NULL-asset & bounds hardening, packageManager NULL guard, gs_romscreen format-string fix.
13. 📱 **Mini Flip port** — surgical carry of Miyoo Mini Flip + MainUI-285 from upstream `v4.5-dev` (`921155e8`).
14. 🔎 **OnionUI-parity review** — `battery_hasChanged` early return while charging; `process_killall` for RetroArch; `file_read("")` parity; rumble GPIO retry; remaining `sprintf` bounds; TTF cache cleanup on exit.
15. 🩹 **2026-09-01 review (A–G)** — empty-file test contract; Mini Flip `suspend_exec` lid-already-closed; OTA beta without stable fallback; brightness write-through; infoPanel scale identity; theme TTF cleanup-before-free.
16. 📦 **Compact history** — long `OnionPlus` (97 commits to `fa5bb007`) squashed onto `onionplus-compact`.
17. 🔀 **@robcodedev ports** — `OnionUI/Onion` PRs #1936–#1946 via [#217](https://github.com/Amiga500/Onion/pull/217): keymon SELECT refresh, `lt.lang` JSON, ThemeSwitcher on-demand previews, GameSwitcher favorites + crash fixes, `fbmode` framebuffer transitions, `.forceKillRetroarch`, `romwinidx` on SD, theme per serial number, recents cap 200, skip RA cfg patch, overlap launch.
18. 🔤 **List cache + installer Mini Flip** — `fbd26d06`: dim a copy of cached TTF labels; installer hall-first; framebuffer preclear before device detection.
19. 🖥️ **Boot FB + AXP percent** — `bf3deb8e`: keep polling `mi_fb0` when dmesg says 640; `commit_mainui_fbmode` waits for the driver; `getBatPercMMP` never writes garbage.
20. ⚡ **Performance pass** — [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804): atomic settings/JSON/config I/O and selective save; in-process blue-light schedule; keymon without `sync`/shell on key paths; single `/proc` pass; playActivity index + `TRUNCATE` journal; GameSwitcher romscreen preload worker; idle sleep in every SDL UI loop; `runtime.sh` sync diet; parallel `make`; 7 upstream defects.
21. 📚 **Docs retired** — `docs/` removed; this README is the single reference.
22. 🗃️ **Performance pass 2** — [`85bc9f21`](https://github.com/Amiga500/Onion/commit/85bc9f21): MainUI cache connection leak, `cache_db_find` leak, recent-list line numbering, `readHistory` duplicates in one rewrite, `stop_all` index, GameSwitcher play time on the worker, fork-free launch parsing.
23. 🧹 **Artifact clean-up** — [#220](https://github.com/Amiga500/Onion/pull/220).
24. 🔋 **Performance pass 3** — [`747d102a`](https://github.com/Amiga500/Onion/commit/747d102a53affd8cd2c5bf250d8b7d54ac7982e8): battery icon only on change, one `sync` per MainUI cycle, `fb_probe_fields`, `mount_main_ui` via `read`, `file_move_line_to_top`, Wi-Fi read once in `update_networking.sh`.
25. ⏱️ **Performance pass 4** — [`1592866e`](https://github.com/Amiga500/Onion/commit/1592866e): `timing.log` marks, fork-free `get_full_resolution_path` / `get_info_value` / `sysjson_get`, crash-safe `system.json` at boot, single-pass `play_activity_find_all`, Package Manager `cJSON_Delete`.
26. 📚 **README refresh** — passes 2–4 folded into the categories.
27. 🧅 **README front page** — at-a-glance comparison, before/after tables, first on-device timings.
28. 🩹 **On-device fixes 1** — [`874ea325`](https://github.com/Amiga500/Onion/releases/tag/OnionPlus-v4.4.0-beta-20260925-874ea325): Wi-Fi off the boot path, play time after clock jumps, GameSwitcher capture scaling.
29. 🕒 **On-device fixes 2** — [`10f2369e`](https://github.com/Amiga500/Onion/commit/10f2369e): boot no longer waits with temporary Wi-Fi set, time zone kept on failed lookups, `ntpdate` sync marked. Boot 5.51 s → **2.28 s**.
30. 🔌 **Faster `boot_init`** — [`29791efc`](https://github.com/Amiga500/Onion/commit/29791efc): audio server polled every 50 ms, swap in the background. Boot → **1.74 s**.
31. 📚 **README polish** — Mini Flip naming, grouped tables, OnionPlus presented as an independent build.
32. 📴 **Temporary Wi-Fi at boot** — [`9768ae02`](https://github.com/Amiga500/Onion/commit/9768ae02): on → sync → update check → off in the background. Boot with Wi-Fi off 5.16 s → **1.78 s**, time synced.
33. 🩺 **Pass 1 correctness review** — [PR #221](https://github.com/Amiga500/Onion/pull/221), 14 commits: F1–F14 (Load preview cache, cache-DB stack overflow, atomic `retroarch.cfg` edits, `disable_flag`, Hall-first detection, Mini Flip suspend timeout, top-entry romscreen, overlay autosave pin, CJK ranges, framebuffer pitch, `readLastLine`, `spawn_detached`), stricter host build, `test_scripts.sh`, `test_romscreen_window`, production `test_cache_db` under ASan.
34. 🎨 **README redesign** — this page: centered header, highlight cards, collapsible reference sections, GitHub alerts, numbers refreshed.

Per-commit detail: `git log --stat 07505ea5..HEAD` on `onionplus-compact`.

</details>

---

<div align="center">
<sub>

[Amiga500/Onion](https://github.com/Amiga500/Onion) · branch `onionplus-compact` ·
base [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) (`OnionUI/Onion:main`, 4.4.0-beta) →
[`41569da0`](https://github.com/Amiga500/Onion/commit/41569da0) · on-device figures: Miyoo Mini+ ·
refreshed 2026-09-26

</sub>
</div>
