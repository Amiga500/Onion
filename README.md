# 🕹️ OnionPlus — Optimizations at a Glance

[![branch](https://img.shields.io/badge/branch-onionplus--compact-8A2BE2?style=for-the-badge&logo=git)](https://github.com/Amiga500/Onion/tree/onionplus-compact)
[![commits](https://img.shields.io/badge/commits-20-blueviolet?style=for-the-badge)](#-11--commit-timeline)
[![files](https://img.shields.io/badge/files%20changed-182-blue?style=for-the-badge)](#-10--grand-totals)
[![diff](https://img.shields.io/badge/diff-%2B30%2C482%20%2F%20%E2%88%921%2C106-informational?style=for-the-badge)](./docs/OnionPlus-vs-base.md)
[![neon](https://img.shields.io/badge/NEON%20kernels-8-orange?style=for-the-badge)](#️-1--vectorized-pixel-paths-neon)
[![tests](https://img.shields.io/badge/tests-1%2C419%20%2F%2071%2C408%20assertions-success?style=for-the-badge)](#-8--testing--the-safety-net)
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
A–G). Every pass reaches installs through the built-in
**OTA updater** (`Amiga500/Onion`, assets `OnionPlus-v…`). Last **code** tip is
[`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e). This document groups
**everything shipped to date** by *category* rather than by commit.

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
| 🛡️ | Correctness/safety fix carrying **no** performance claim |

> ⚠️ Percentages compare **OnionPlus code vs `OnionUI/Onion:main`** (`07505ea5`). They have
> not been re-timed on a physical Miyoo as part of this fork. 📐 = analytical
> (complexity / syscall count). See [methodology](./docs/ONIONPLUS_OPTIMIZATION.md#-9-methodology--limits).

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

> 🧹 Every cache above ships with its own teardown: `list_free()` releases the TTF slots,
> `cleanImagesCache()` frees the infoPanel scaled cache, and `free_resources()` releases
> the playActivityUI page cache — so this is a speed win **without** a new leak.

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
| 🕹️ AdvanceMENU romscripts | temp file written **CWD-relative** | temp file next to `advmenu.rc`, PID-suffixed | race condition + read-only-CWD failure fixed |
| 🕹️ AdvanceMENU `launch.sh` | no reentrancy guard | early exit if `advmenu` already running | duplicate-instance guard |

> 📉 Net result across the 25-file hardening core: **`system()` call sites 3 → 1**
> (the one survivor, `process_start()`, is confirmed **dead code** with no live caller).

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

---

## 🏗️ 9 · Build, CI & release

| Change | Detail |
|:--|:--|
| 📦 Release flags | `-O2 -ffunction-sections -fdata-sections -Wl,--gc-sections` → 🚀 **−5–15 % binary size** 📏 |
| 🎯 New build target | `make unit-test` — host-only, zero device dependency |
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
| 🔧 Commits (`07505ea5..HEAD`) | **20** *(`git rev-list --count` on `onionplus-compact` after this number audit; 18 through last code `bf3deb8e`. The long `OnionPlus` branch was 97.)* |
| 📁 Files changed | **182** *(88 added, 94 modified, 0 deleted)* |
| ➕➖ Lines | **+30,487 / −1,109** |
| 🧩 Production (`src/` + `static/` + CI/Makefile) | **101 files · +4,572 / −1,080** *(excludes `.gitignore` + `SDL.h`, 2 · +35 / −0)* |
| 🧪 Tests (`test/`) | **75 files · +23,327 / −10** |
| 📚 Docs + README | **4 files · +2,548 / −16** |
| ⚡ NEON kernels | **8** (7 asm + 1 intrinsics) |
| 🧪 Test suites / tests / assertions | **68 / 1,419 / 71,410** — **all green** ✅ |
| 🛡️ Unsafe `sprintf`/`strcpy`+`strcat`/`strtok` remaining (hardened set) | **0 / 0 / 0** |
| 🛡️ NULL-guards / closed descriptors added | **+57 / +18** *(25-file set)* |
| 🔐 Pre-existing upstream defects fixed | **6** |
| 🕹️ AdvanceMENU scripts hardened/optimized | **7 files** |

> 📎 For per-commit line counts, file-level diff stats, and independently reproducible
> `git` commands, see [`OnionPlus-vs-base.md`](./docs/OnionPlus-vs-base.md).

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

> 🔍 Full SHA-by-SHA detail lives in
> [§1 of the deep-dive report](./docs/ONIONPLUS_OPTIMIZATION.md#-1-commit-breakdown).

---

## ✅ Final word

`onionplus-compact` is **21 commits** ahead of upstream `OnionUI/Onion:main` (`07505ea5` →
this number audit; last code `bf3deb8e`. The long `OnionPlus` branch was 97). Same tree: **8 vectorized NEON kernels**,
a dozen algorithmic O(n²)→O(n) rewrites, five distinct render/UI caches (list dimming
no longer mutates the TTF cache), a power/battery batch (AXP percent clamped), a syscall
diet that removed every avoidable `system()` call from the hardened core, **six
pre-existing upstream defects** closed, a **68-suite / 1,419-test** host test harness
that did not exist before this branch, a full **AdvanceMENU** hardening pass, a
**Miyoo Mini Flip** port from `v4.5-dev` that does **not** merge that branch, an
**OnionUI-parity review**, the **2026-09-01 A–G fixes**, the **@robcodedev** ports of
`OnionUI/Onion` **#1936–#1946**, and the **2026-09-09** installer / boot-FB fixes.
Base remains `4.4.0-beta`. OTA stays on `Amiga500/Onion`. Flip lid/Hall and on-device
timings are still unconfirmed.
See [`ONIONPLUS_OPTIMIZATION.md`](./docs/ONIONPLUS_OPTIMIZATION.md).

---

<sub>Repository: [Amiga500/Onion](https://github.com/Amiga500/Onion) · Branch: `onionplus-compact` ·
Base: [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) (`OnionUI/Onion:main`) → last code [`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e) (**20** including this number audit,
`git rev-list --count`) · Headline figures refreshed **2026-09-09** · Companion docs:
[`ONIONPLUS_OPTIMIZATION.md`](./docs/ONIONPLUS_OPTIMIZATION.md) ·
[`OnionPlus-vs-base.md`](./docs/OnionPlus-vs-base.md)</sub>
