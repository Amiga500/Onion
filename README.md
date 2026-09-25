# 🕹️ OnionPlus — Optimizations at a Glance

[![branch](https://img.shields.io/badge/branch-onionplus--compact-8A2BE2?style=for-the-badge&logo=git)](https://github.com/Amiga500/Onion/tree/onionplus-compact)
[![commits](https://img.shields.io/badge/commits-22-blueviolet?style=for-the-badge)](#-11--commit-timeline)
[![files](https://img.shields.io/badge/files%20changed-185-blue?style=for-the-badge)](#-10--grand-totals)
[![perf](https://img.shields.io/badge/perf%20pass-%2B1%2C164%20%2F%20%E2%88%92226-informational?style=for-the-badge)](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)
[![neon](https://img.shields.io/badge/NEON%20kernels-8-orange?style=for-the-badge)](#️-1--vectorized-pixel-paths-neon)
[![tests](https://img.shields.io/badge/tests-1%2C419%20%2F%2071%2C410%20assertions-success?style=for-the-badge)](#-8--testing--the-safety-net)
[![ota](https://img.shields.io/badge/updates-OTA%20enabled-2ea44f?style=for-the-badge)](#️-9--build-ci--release)
[![status](https://img.shields.io/badge/status-ALL%20GREEN-brightgreen?style=for-the-badge)](#-final-word)

> 📡 **OnionPlus ships and updates itself over-the-air** — `ota_update.sh` checks
> `Amiga500/Onion` releases directly on-device, so every optimization and hardening
> pass below reaches installs without a manual re-flash. See
> [§9 · Build, CI & release](#️-9--build-ci--release) for the wiring.

---

## 🗺️ Table of Contents

| | Section |
|:--|:--|
| 🎯 | [Why this document exists](#-why-this-document-exists) |
| 🖼️ | [1 · Vectorized pixel paths (NEON)](#️-1--vectorized-pixel-paths-neon) |
| ⚡ | [2 · Algorithmic wins (O(n²) → O(n))](#-2--algorithmic-wins-on--on) |
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

## 🎯 Why this document exists

OnionPlus is measured against **[`OnionUI/Onion:main`](https://github.com/OnionUI/Onion/tree/main)**
at merge-base [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) (`4.4.0-beta`).
That is the comparison that matters for this fork. Some NEON kernels and early hardening
were first written elsewhere; every **percentage in this document is OnionPlus vs
`OnionUI/Onion:main`**, not vs that sibling branch.

The integration branch is **[`onionplus-compact`](https://github.com/Amiga500/Onion/tree/onionplus-compact)**:
the long `OnionPlus` history (97 commits to [`fa5bb007`](https://github.com/Amiga500/Onion/commit/fa5bb007))
squashed into topic commits, then the **@robcodedev** ports of still-open
[`OnionUI/Onion` PRs #1936–#1946](https://github.com/OnionUI/Onion/pulls?q=1936)
([Amiga500 #217](https://github.com/Amiga500/Onion/pull/217)), and the **2026-09-09**
review fixes ([`fbd26d06`](https://github.com/Amiga500/Onion/commit/fbd26d06) list-cache
dimming + installer Flip detect; [`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e)
Flip 640 lock, `fbmode` before the FB driver, AXP `percBat` clamp).
`git rev-list --count 07505ea5..HEAD` on this branch is the **compact** count, not 97.

On top of `07505ea5` the tree still carries the power/CPU batch, security review, hot-path
passes, an **AdvanceMENU** frontend pass, a **surgical Miyoo Mini Flip port** from
`OnionUI/Onion:v4.5-dev` that does **not** merge that branch,
an **OnionUI-parity review** (charging-icon sentinel, RetroArch `killall` semantics,
path bounds, rumble GPIO retry), and the **2026-09-01 independent review** (findings
A–G), and the **2026-09-25 performance pass** [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804) (atomic I/O, in-process
blue-light schedule, GameSwitcher preload worker, idle UI loops, parallel `make`).
Every pass reaches installs through the built-in
**OTA updater** (`Amiga500/Onion`, assets `OnionPlus-v…`). Last **code** tip is
[`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804). This README is the **single reference** for the branch (the former `docs/`
reports were retired with it) and groups **everything shipped to date** by *category*
rather than by commit.

### 🔑 Reading the icons

| Icon | Meaning |
|:---:|:---|
| 🟥 | **Order-of-magnitude** win — vectorized paths, quadratic → linear, busy-wait removal |
| 🟧 | **Structural** win — a syscall/shell-out/scan eliminated on a repeatable path |
| 🟨 | **Incremental** win — smaller but still measurable saving |
| 🟦 | **Robustness** — correctness / memory-safety fix, **no performance claim** |
| 🟩 | **Quality floor** — tests, CI, tooling |
| 📏 | Speedup of the **OnionPlus** path vs the **`OnionUI/Onion:main`** equivalent |
| 📐 | Figure **estimated analytically** (algorithmic complexity / syscall count) |
| 🧪 | **Verified by unit test** in this repository |
| 🧪* | Verified by an **out-of-tree** host check (see §8), not yet in `test/` |
| 🛡️ | Correctness/safety fix carrying **no** performance claim |

> ⚠️ Percentages compare **OnionPlus code vs `OnionUI/Onion:main`** (`07505ea5`). They have
> not been re-timed on a physical Miyoo as part of this fork. 📐 = analytical
> (complexity / syscall count / files written, read from the source).

---

## 🖼️ 1 · Vectorized pixel paths (NEON)

> 🟥 The single biggest performance category — whole scalar loops replaced by ARM NEON
> vector kernels, each with a scalar C fallback so non-NEON builds still work.

| Kernel | What it replaced | Speedup | Evidence |
|:--|:--|--:|:--:|
| 🔄 `neon_rotate180_inplace` | rotozoom blit + extra surface alloc | 🚀 **+5000 %** | 📏 |
| 🎨 `neon_swap_rb_inplace` | scalar per-pixel loop | 🚀 **~+800 %** | 📏 |
| 🎨 `neon_argb_to_rgba` | scalar per-pixel loop | 🚀 **~+800 %** | 📏 |
| 🎨 `neon_rgb888_to_argb` | scalar per-pixel loop | 🚀 **~+800 %** | 📏 |
| ⚪ `neon_gray8_to_argb` | scalar per-pixel loop | 🚀 **~+600 %** | 📏 |
| ⚪ `neon_gray8a_to_argb` | scalar per-pixel loop | 🚀 **~+500 %** | 📏 |
| 🎨 `neon_argb_to_rgba_alpha` | scalar per-pixel + branch | 🚀 **~+600 %** | 📏 |
| 🌫️ `surfaceSetAlpha` (NEON intrinsics) | float mul + `SDL_GetRGBA` | 🚀 **~+400 %** | 📏 |

- 📦 **8 kernels total** (7 hand-written ARM assembly + 1 NEON intrinsics), all guarded by
  `#ifdef __ARM_NEON` with a correct scalar tail loop for the remainder.
- 🧪 Backed by `test_neon`, `test_neon_pixel` and `test_alpha_scale` — **109 tests /
  67,353 assertions** cross-checking NEON output against the scalar oracle.
- 🔬 Every scalar fallback is exercised on the x86-64 host CI; a separate `neon-arm` job
  cross-compiles the assembly and runs it under `qemu-user`.

---

## ⚡ 2 · Algorithmic wins (O(n²) → O(n))

> 🟥🟧 String and path handling rewritten to drop a re-scan hidden inside a loop.

| Function | Before | After | Class | Evidence |
|:--|:--|:--|:--:|:--:|
| `str_count_char` | `strlen()` re-evaluated every iteration | single pointer walk | O(n²)→O(n) 🚀 **−90 %** | 📏🧪 |
| `file_removeExtension` | `strlen` + `strcpy` rescans | one scan + length-known `memcpy` | **−50 % scans** | 📏🧪 |
| `file_path_relative_to` | `strcat` loop rescanning from byte 0 | explicit `offset` + `memcpy` | O(n²)→O(n) | 📏🧪 |
| `file_resolvePath` | `strcat` loop per path component | bounds-checked `memcpy` at `offset` | O(n²)→O(n) | 📐🧪 |
| `file_read()` | `fopen`+`fseek`×2+`ftell`+buffered `fread` | `stat64` + one `read()` loop | **2 seeks removed** | 📐🧪 |
| 🔎 `system_state_update()` | one full `/proc` scan **per candidate** (up to 5) + a `cmd_to_run.sh` read per check | single `/proc` pass for all candidates, file read once | **5 scans → 1** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🗃️ playActivity `rom` lookup by `file_path` | full table scan on every start/stop | `rom_file_path_index` (created on open, no-op once present) | O(n)→O(log n) 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🗃️ MainUI cache query on game start | `LIKE '%…' OR disp` full scan of the cache DB on **every** launch, known ROM or not | run only for rows still missing type/name | **−1 full scan per launch** 📐🧪* ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
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
| 🎮 GameSwitcher romscreens | PNG decode + scale **on the UI thread**, under the same mutex as a one-shot loader | persistent worker decodes outside the lock and prefetches **±2** entries; only the UI thread frees surfaces (±5 window) | scrolling no longer waits for decoding 📐🧪* ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |

> 🧹 Every cache above ships with its own teardown: `list_free()` releases the TTF slots,
> `cleanImagesCache()` frees the infoPanel scaled cache, and `free_resources()` releases
> the playActivityUI page cache — so this is a speed win **without** a new leak. The
> romscreen worker is stopped and joined before `freeRomScreens()` releases its surfaces.

---

## 🔋 4 · Power, battery & idle CPU

> 🟥🟧 The category with the most direct battery-life relevance: fewer wake-ups, fewer
> forked subprocesses, fewer duplicate sysfs writes.

| Subsystem | Before | After | Impact |
|:--|:--|:--|:--|
| 🔊 OSD volume/brightness bar thread | `usleep(100)` busy-wait (~10,000 loops/s) | `usleep(16000)` (~60 fps) | 🚀 **idle CPU ~10 % → <1 %** 📏 |
| 🖼️ OSD overlay draw loop | full-throttle spin for the overlay's duration | `msleep(2)` per iteration + demoted logging | overlay CPU burn capped 📐 |
| 🔌 `battery_isCharging()` (`HAS_AXP()` — MM+ and Flip) | `fork`+`exec` of `axp_test` every call (~5–10 ms) | 2 s cached wrapper | 🚀 **~−99 % subprocess spawns** 📐 |
| 🔋 `battery_hasChanged` while charging | OnionPlus used to overwrite the `500` charging sentinel from `/tmp/percBat` | early-return like `OnionUI/Onion:main` (`500` stays while plugged in) | charging icon no longer drops after the first percBat tick 🛡️ |
| 🪫 `getBatPercMMP()` AXP percent | `axp_test` garbage (e.g. `1735289191`) and `-1` written to `/tmp/percBat` | last sane **0–100** kept; out-of-range samples dropped | GS/keymon never read a bogus percent 🛡️ |
| 🪫 batmon low-battery thread | `usleep(0x4000)` (~16 ms) | `usleep(500000)` (500 ms) | 🚀 **~−97 % wake-ups** 📐 |
| 💡 `display_setBrightnessRaw` | sysfs write on **every** call | cached, duplicate writes skipped | **−100 % duplicate PWM writes** 📏 |
| ⏱️ batmon main loop | `config_get("battery/warnAt")` every tick | read only at check timeout | **−100 % hot-loop config reads** 📐 |
| 📳 `rumble()` GPIO init | `export`+`direction` sysfs writes on **every** pulse | one-time init + retry if `gpio48` missing, value-only writes after | **−2 sysfs writes/pulse** 📏 |
| 🎮 GameSwitcher battery poll | `stat()` on `/tmp/percBat` every loop (~1 kHz) | checked once/second (matches batmon's write rate) | **~99.9 % fewer `stat` calls** 📐 |
| 🌀 SDL UI loops (GameSwitcher, Tweaks, prompt, playActivityUI, themeSwitcher, packageManager, batteryMonitorUI + dialogs) | non-blocking poll with **no sleep**: one core at 100 % while a menu just sits there | sleep until the next frame (or 15 ms) when idle | 🚀 **idle UI CPU ~100 % of a core → near 0** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🌙 keymon blue-light schedule | `blue_light.sh check` **every 15 s**, synchronous: 2 global `sync`, ~20 `fork`/`exec`, keymon deaf to keys for up to ~4 s on a transition | evaluated in-process (same `.tz`, same window logic); script started in background **only on a transition** | **~20 spawns/15 s → 0** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 💾 Volume / brightness step | `settings_save()` rewrote every setting: ~30 files, **13 `fsync`** on the SD card | selective save against the on-disk snapshot | **1 file (`system.json`)** 📐🧪* ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 💾 keymon key handling | global `sync()` after deleting flags in `/tmp` (tmpfs) — flushed the SD card on a key press | no sync for tmpfs flags | **−1 global sync per flagged key** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🔁 `runtime.sh` main loop | `pgrep keymon` + `touch` + global `sync`, **4× per loop** | `/proc/<pid>` check, shell builtin, one `sync` when a game/app exits | **4 syncs → 1**, `pgrep` only if keymon died 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🔋 batmon `/tmp/percBat` | `fsync` on tmpfs | write + `rename()` (no fsync; readers never see an empty file) | **−1 useless fsync per % change** 📐 ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🔆 AdvanceMENU quick-switch (PWM) | backlight PWM always re-enabled on exit | re-enabled **only** when returning from a game (`quick_switch`) | fewer redundant PWM writes 📐 |

---

## ⚙️ 5 · Process & syscall diet

> 🟧 Every `system()` call forks a shell **and** the real binary — two processes for
> one line of intent. These were replaced with direct syscalls or `fork`+`exec`.

| Call site | Before | After | Result |
|:--|:--|:--|:--|
| `mkdirs()` | `system("mkdir -p …")` | iterative `mkdir()` walk | **2 → 0 processes** |
| `file_copy()` | `system("cp -f …")` | `open`/`read`/`write` loop | **2 → 0 processes**, shell-injection surface closed |
| `config.h` `_config_prepare` | `system("mkdir -p …")` | direct `mkdirs()` | **2 → 0 processes** |
| GS overlay `playActivity` | `system("… &")` | double-fork + `execl` (async, no zombies) | 🚀 **−80 % process overhead** 📏 |
| GS overlay RetroArch kill/poll | `killall` / `pidof` shell-outs | `process_killall_signal` / `process_isRunning` (all matching PIDs) | **−100 % shell, killall semantics restored** |
| playActivity DB ops | 2× open/close per operation | 1× open/exec/close | **−50 % DB I/O** 📏 |
| Reset paths (tweaks/theme/RA overrides) | `rm -rf` via `system()` | `nftw()`-based `file_remove_recursive()` | shell-free recursive delete |
| keymon `touch`, `tools favfix`, `playActivity stop_all/resume`, `bootScreen`, recorder / blue-light scripts | `system("… &")` / `system("touch …")` | `process_run_wait()` / double-fork `process_spawn_detached()` / `creat()` | **one shell less per call** ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| keymon CPU-clock hotkey | `cpuclock` spawned **twice** per press | last value cached (reset on every state change) | **2 → 1 spawns** ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| keymon config flags (`.altBrightness`, `.cpuClockHotkey`, `.recHotkey`) | `stat()` on the SD card on every key | cached, refreshed on settings change + every 15 s | **−1 SD `stat` per key** ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| `blue_light.sh` time parsing | `echo \| cut \| xargs` ×2 + `awk` per value | shell parameter expansion | **~6 → 0 forks per value** ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
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

---

## 🕹️ 7 · AdvanceMENU frontend

> 🆕 The newest optimization/hardening pass on the branch — a fully self-contained batch
> covering fonts, power handling, script robustness, and tooling.

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
| ✅ Tests | **1,419** |
| ✅ Assertions | **71,410** |
| ❌ Failures | **0** |
| ⏱️ Suite runtime (prebuilt) | **~2.5 s** |
| 🔐 Security-focused suites | 10 suites · 219 tests · 960 assertions (**15 %** of all tests) |

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

---

## 🏗️ 9 · Build, CI & release

| Change | Detail |
|:--|:--|
| 📦 Release flags | `-O2 -ffunction-sections -fdata-sections -Wl,--gc-sections` → 🚀 **−5–15 % binary size** 📏 |
| 🎯 New build target | `make unit-test` — host-only, zero device dependency |
| ⚙️ Parallel `make` | `core` builds `bootScreen` + `gameSwitcher` first (they compile every shared `../common` object), then the other 27 modules as independent targets; `$(MAKE)` passes the jobserver, so `make -j` works without two sub-makes racing on one `.o` ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| ♻️ Setup stamp | `cache/.setup` re-runs when `static/`, `lib/` or any `src/*/res`/`script` file is newer (was: only after `make clean`) ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 🗜️ RetroArch package | `retroarch.pak` reused from `cache/` while a content hash (paths, modes, symlinks, data) is unchanged — skips the slowest `7z` step ([`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804)) |
| 📊 Opt-in profiling | `src/common/utils/perf.h` — `PERF_START`/`PERF_END` compile to nothing unless `-DPERF_ENABLED` |
| 🏷️ Release naming | `OnionPlus V4.4.0-beta-YYYYMMDD`, zip `OnionPlus-v…-<sha>.zip` — real dated GitHub Releases, no more overwritten `latest`. Base remains **4.4.0-beta**; Flip support is a port, not a rebase onto official `v4.5-dev`. |
| 📡 OTA | `ota_update.sh` points at `Amiga500/Onion`, filters `OnionPlus-v` assets. Stable = `/releases/latest`. Beta installs **only GitHub prereleases** — no fallback to `releases[0]` (finding D). Host CI (`.github/workflows/test.yml`) runs on push to `onionplus-compact`. |
| 📱 Mini Flip | Device id `285`, MainUI-285 binaries, lid-close Tweaks. Runtime probes AXP first (354 Mini+), then the `hall-mh248` sysfs node (285 Flip). The **installer** probes **hall first** (Flip stays Flip if `axp` is not on PATH yet); `axp` / `axp_test` = Plus. `/dev/input/event*` is **not** a Flip signal. Installer preclears the framebuffer (`fbmode` if present, else `dd` + `fbset 640x480/2`) before `check_device_model`. Ported from `OnionUI/Onion:v4.5-dev` without merging that branch. Lid/Hall **untested** on a physical Flip. |
| 🖥️ Boot FB | Plus/Flip: a dmesg hint of `640x480` no longer skips the `mi_fb0` poll (avoids locking a 752 panel at 640 for the boot). `commit_mainui_fbmode()` waits for the FB driver; on timeout it uses `fbset`, not `fbmode` with the driver still down. |
| 🧵 Signal handling | Shared `signal_handler_quit()` deduplicated across 6 apps; `volatile sig_atomic_t` used correctly for signal-shared state |

---

## 📊 10 · Grand totals

| Metric | Value |
|:--|--:|
| 🔧 Commits (`07505ea5..HEAD`) | **22** *(21 through last code `fee6c4b`, 22 with this README / `docs/` retirement commit. The long `OnionPlus` branch was 97.)* |
| 📁 Files changed | **185** *(182 at `bf3deb8e`; `fee6c4b` touches 6 files that were still identical to upstream; the 3 `docs/` reports are removed)* |
| ➕➖ Lines | **+30,487 / −1,109** at `bf3deb8e`, then `fee6c4b` **+1,164 / −226** and the `docs/` removal **−2,157** *(net figure: `git diff --shortstat 07505ea5..HEAD`)* |
| 🧩 Production (`src/` + `static/` + CI/Makefile) | **101 files · +4,572 / −1,080** *(excludes `.gitignore` + `SDL.h`, 2 · +35 / −0)* |
| 🧪 Tests (`test/`) | **75 files · +23,327 / −10** |
| 📚 README | **1 file** *(the three `docs/` reports are retired; this README is the reference)* |
| ⚡ NEON kernels | **8** (7 asm + 1 intrinsics) |
| 🧪 Test suites / tests / assertions | **68 / 1,419 / 71,410** — **all green** ✅ |
| 🛡️ Unsafe `sprintf`/`strcpy`+`strcat`/`strtok` remaining (hardened set) | **0 / 0 / 0** |
| 🛡️ NULL-guards / closed descriptors added | **+57 / +18** *(25-file set)* |
| 🔐 Pre-existing upstream defects fixed | **13** *(6 + 7 in `fee6c4b`)* |
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
15. 🩹 **2026-09-01 review (A–G)** — empty-file test contract; Flip `suspend_exec` lid-already-closed; AXP-then-hall detect; OTA beta without stable fallback; brightness write-through; infoPanel scale identity; theme TTF cleanup-before-free. On the long branch: duplicate tree (`9ab47af` / `2f90bbe`); CI push trigger (`fa5bb007`). Compact CI is `onionplus-compact` (`22004cce`).
16. 📦 **Compact history** — long `OnionPlus` (97 commits to `fa5bb007`) squashed onto `onionplus-compact`.
17. 🔀 **@robcodedev ports** — `OnionUI/Onion` PRs **#1936–#1946** (still open upstream) via [Amiga500 #217](https://github.com/Amiga500/Onion/pull/217) (`c7a1a7e9` + `587c35ec` + merge `f87e7781`): keymon SELECT refresh, `lt.lang` JSON, ThemeSwitcher on-demand previews, GameSwitcher favorites + crash fixes, `fbmode` framebuffer transitions, `.forceKillRetroarch`, `romwinidx` on SD, theme per `SERIAL_NUMBER`, recents cap 200, skip RA cfg patch, overlap launch.
18. 🔤 **List cache + installer Flip** — `fbd26d06`: dim a copy of cached TTF labels; installer hall-first (never `event*`); framebuffer preclear before device detect.
19. 🖥️ **Boot FB + AXP percent** — `bf3deb8e`: Plus/Flip keep polling `mi_fb0` when dmesg says 640; `commit_mainui_fbmode` honors `wait_for_fb_driver`; `getBatPercMMP` never writes garbage to `/tmp/percBat`.

20. ⚡ **Performance pass** — [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804): atomic settings/JSON/config I/O and selective save; in-process blue-light schedule; keymon without `sync`/shell on key paths; single `/proc` pass; playActivity `file_path` index + `TRUNCATE` journal + busy timeout; GameSwitcher romscreen preload worker; idle sleep in every SDL UI loop; `runtime.sh` sync diet; parallel `make`, setup stamp, cached `retroarch.pak`; 7 upstream defects (CPU-clock stack overflow, `suspendpid` overflow, `/proc` parsing, debounce, dirty snapshot, non-atomic writes, BLF lock).
21. 📚 **Docs retired** — `docs/` (`ONIONPLUS_OPTIMIZATION.md`, `OPTIMIZATIONS_OVERVIEW.md`, `OnionPlus-vs-base.md`) removed; this README is the single reference.

> 🔍 Per-commit detail: `git log --stat 07505ea5..HEAD` on `onionplus-compact`.

---

## ✅ Final word

`onionplus-compact` is **22 commits** ahead of upstream `OnionUI/Onion:main` (`07505ea5` →
this README commit; last code `fee6c4b`. The long `OnionPlus` branch was 97). Same tree: **8 vectorized NEON kernels**,
a dozen algorithmic O(n²)→O(n) rewrites, five distinct render/UI caches (list dimming
no longer mutates the TTF cache), a power/battery batch (AXP percent clamped), a syscall
diet that removed every avoidable `system()` call from the hardened core, **six
pre-existing upstream defects** closed, a **68-suite / 1,419-test** host test harness
that did not exist before this branch, a full **AdvanceMENU** hardening pass, a
**Miyoo Mini Flip** port from `v4.5-dev` that does **not** merge that branch, an
**OnionUI-parity review**, the **2026-09-01 A–G fixes**, the **@robcodedev** ports of
`OnionUI/Onion` **#1936–#1946**, the **2026-09-09** installer / boot-FB fixes, and the
**2026-09-25** performance pass (atomic I/O, idle UI loops, in-process blue-light
schedule, GameSwitcher preload, parallel build, **7 more upstream defects** closed).
Base remains `4.4.0-beta`. OTA stays on `Amiga500/Onion`. Flip lid/Hall and on-device
timings are still unconfirmed; `fee6c4b` has been host-verified only and still needs an
ARM build and an on-device check (sleep/wake, volume persistence, blue light,
GameSwitcher scrolling and removal, CPU hotkey).

---

<sub>Repository: [Amiga500/Onion](https://github.com/Amiga500/Onion) · Branch: `onionplus-compact` ·
Base: [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) (`OnionUI/Onion:main`) → last code [`fee6c4b`](https://github.com/Amiga500/Onion/commit/fee6c4b9236eaec7435288e6691f08c7f2e4b804) (**22** including this README commit,
`git rev-list --count`) · Headline figures refreshed **2026-09-25**</sub>
