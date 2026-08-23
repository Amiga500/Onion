# 🧅 OnionPlus — Optimization & Hardening Report

> **48 commits** · **140 files** · **+27,059 / −766 lines** · **8 NEON kernels** · **68 test suites** · **1,410 tests** · **71,385 assertions** · **ALL PASSED** ✅

> **Scope:** this report covers *only* the commits that make up the OnionPlus port on top of
> upstream `OnionUI/Onion:main` (`07505ea5` → `HEAD`). It is **not** a report on the whole
> [OniOpus46](https://github.com/Amiga500/Onion/blob/OniOpus46/docs/OPTIMIZATION.md) branch.
> Percentages quoted from OniOpus46 are **explicitly labelled as such** — see the
> [legend](#-legend--how-to-read-the-numbers) below.

📊 Raw diff statistics vs. the base release live in a companion document:
**[OnionPlus-vs-base.md](./OnionPlus-vs-base.md)**.

> 🔁 **Self-reference.** The docs-update commit carries both this document and the statistics
> it describes, so its own SHA cannot appear here — the code tip is `74f0a0af` (four
> review-pass fixes, 2026-08-23) on top of the CI fix `2ae2e79f`, the docs refresh
> `03080200`, the version bump `6b7f6357`, merge `e44421e1` (reconciliation with remote
> PR #206–207) and the docs refresh `55998284`. The two `docs/` files are inside the range
> they measure; wherever it matters, the **code-only** subset is given next to the aggregate.

---

## 🔖 Legend — how to read the numbers

Every performance figure in this document carries one of these markers. **Nothing here was
timed on a Miyoo Mini as part of the OnionPlus port.**

| Icon | Meaning |
|:---:|:---|
| 📏 | **measured on OniOpus46** — benchmark figure published in the [OniOpus46 `OPTIMIZATION.md`](https://github.com/Amiga500/Onion/blob/OniOpus46/docs/OPTIMIZATION.md) for the **same code path**, which OnionPlus ports byte-for-byte. Not re-measured here. |
| 📐 | **estimated** — analytical result derived from algorithmic complexity, instruction encoding or syscall count. No timing involved. |
| 🧪 | **verified by test** — behaviour is exercised by the host unit-test suite in this repo (`make unit-test`). |
| 🛡️ | **robustness only** — a correctness/safety change with **no** performance claim attached. |
| ❌ | **not ported** — the optimization exists in OniOpus46 but is **absent** from OnionPlus today. Listed for honesty, never counted in the totals. |

> ⚠️ A 📏 figure means *"this same code was measured to do this on OniOpus46"*, **not**
> *"we measured this on OnionPlus"*. Treat every 📏 as inherited evidence, not fresh evidence.

---

## 📋 Table of Contents

1. [Summary Overview](#-summary-overview)
2. [Commit Breakdown](#-1-commit-breakdown)
3. [Performance — NEON Pixel Paths](#-2-performance--neon-pixel-paths)
4. [Performance — Algorithmic & Syscall Wins](#-3-performance--algorithmic--syscall-wins)
5. [Security & Hardening](#️-4-security--hardening)
6. [Testing](#-5-testing)
7. [Build & Tooling](#️-6-build--tooling)
8. [Overall Statistics](#-7-overall-statistics)
9. [Not Ported from OniOpus46](#-8-not-ported-from-oniopus46)
10. [Methodology & Limits](#-9-methodology--limits)
11. [Known Residuals](#️-10-known-residuals)
12. [Final Status](#-final-status)

---

## 📊 Summary Overview

> Port of the OniOpus46 NEON pixel conversions, crash/memory hardening, host unit-test
> suite, and the power/CPU-relevant optimizations (OSD busy-wait, brightness caching,
> battery-charging cache, batmon fixes, SQLite open/close, fork+exec overlay) onto the
> OnionPlus branch. Baseline is `07505ea5`, the tip of `OnionUI/Onion:main`
> at the time of the port.

| Category | Before | After | Δ | Evidence |
|:---------|:------:|:-----:|--:|:--------:|
| ⚡ Image 180° rotation (32bpp) | software rotozoom | in-place NEON `VREV64` | **+5000 %** 🚀 | 📏 |
| ⚡ ARGB↔RGBA conversion | scalar per-pixel loop | NEON `VLD4`/`VST4` | **~+800 %** 🚀 | 📏 |
| ⚡ RGB888 → ARGB decode | scalar per-pixel loop | NEON `VLD3`/`VST4` | **~+800 %** 🚀 | 📏 |
| ⚡ Gray8 → ARGB decode | scalar per-pixel loop | NEON `VLD1`/`VST4` | **~+600 %** 🚀 | 📏 |
| ⚡ `surfaceSetAlpha` | float mul + `SDL_GetRGBA` | fixed-point + NEON `VMULL` | **~+400 %** 🚀 | 📏 |
| ⚡ `str_count_char` | O(n²) | O(n) | **−90 %** 🚀 | 📏 🧪 |
| ⚡ `file_removeExtension` | `strlen` ×2 | `strlen` ×1 + `memcpy` | **−50 % scans** | 📏 🧪 |
| ⚡ `file_path_relative_to` | O(n²) (`strcat` loop) | O(n) offset walk | **−50 % scans** | 📏 🧪 |
| ⚡ `mkdirs()` | `system("mkdir -p")` | direct `mkdir()` walk | **−100 % proc spawns** | 📐 🧪 |
| ⚡ `file_copy()` | `system("cp -f")` | `open`/`read`/`write` loop | **−100 % proc spawns** | 📐 🧪 |
| ⚡ `file_read()` | `fopen`+`fseek`×2+`ftell` | `stat64`+`read()` loop | **−2 seeks, −1 copy** | 📐 🧪 |
| ⚡ `file_resolvePath` | O(n²) (`strcat` loop) | O(n) offset walk | **−50 % scans** | 📐 🧪 |
| ⚡ TTF/list/footer/header/dialog surface cache | `TTF_RenderUTF8_Blended` every frame | hash-invalidated SDL surfaces | **5–15 ms/frame saved** | 📏 |
| ⚡ Release binary size (`--gc-sections`) | default `-O0` sections kept | `-O2 -ffunction-sections -Wl,--gc-sections` | **−5–15 %** | 📏 |
| ⚡ OSD bar thread poll | `usleep(100)` busy-wait (~10,000 loops/s) | `usleep(16000)` (~60 fps) | **idle CPU ~10 % → <1 %** | 📏 |
| ⚡ `display_setBrightnessRaw` | sysfs write on every call | cached, duplicate writes skipped | **−100 % duplicate PWM writes** | 📏 |
| ⚡ `battery_isCharging` (MIYOO354) | `fork`+`exec` of `axp_test` per call (~5–10 ms) | 2 s cached wrapper | **~−99 % subprocess spawns** | 📐 |
| ⚡ batmon main loop | `config_get("battery/warnAt")` every tick | read only at check timeout | **−100 % hot-loop config reads** | 📐 |
| ⚡ batmon low-battery thread | `usleep(0x4000)` (~16 ms) | `usleep(500000)` (500 ms) | **~−97 % wake-ups on that thread** | 📐 |
| ⚡ playActivity DB operations | 2 × open/close per operation | 1 × open/exec/close | **−50 % DB I/O** | 📏 |
| ⚡ GS overlay `playActivity` calls | `system("playActivity … &")` | double-fork + `execl` (async, no zombies) | **−80 % process overhead** | 📏 |
| ⚡ GS overlay RetroArch kill/poll | `killall` / `pidof` shell-outs | `process_kill_signal` / `process_isRunning` | **−100 % shell spawns** | 📐 |
| ⚡ `config.h` `_config_prepare` | `system("mkdir -p …")` | direct `mkdirs()` walk | **−100 % proc spawns** | 📐 🧪 |
| ⚡ `display_readOrWriteBuffer` | per-pixel loop on every row | `memcpy` fast path for contiguous rows | **row copy vectorised** | 📐 |
| 🛡️ Unsafe `sprintf` call sites | 23 | 0 | **−100 %** ✅ | 🛡️ |
| 🛡️ Unsafe `strcpy`+`strcat` call sites | 37 | 0 | **−100 %** ✅ | 🛡️ |
| 🛡️ Non-reentrant `strtok` call sites | 4 | 0 | **−100 %** ✅ | 🛡️ |
| 🛡️ `system()` call sites | 3 | 1 | **−66.7 %** | 🛡️ |
| 🛡️ NULL-check predicates | — | +57 | **+57** | 🛡️ |
| 🛡️ `fclose`/`close` on error paths | — | +18 | **+18** | 🛡️ |
| 🛡️ `FNV1A_Pippip_Yurii` hash load | 8-byte read regardless of `wrdlen`, unaligned | `memcpy` of exactly `wrdlen` bytes into an aligned local | **over-read and unaligned access removed, hashes bit-identical** | 🛡️ 🧪 |
| 🛡️ Game Switcher save thread | ran with an uninitialised `stateFilePath` when the path could not be built | returns before touching RetroArch | **up to 60 s of polling on a garbage path removed** | 🛡️ 🧪 |
| 🛡️ `file_basename` | discarded `const` via a cast | `const char *` throughout | **`-Wcast-qual` clean** | 🛡️ |
| 🛡️ `RUN_TEST` macro test result reporting | Printed `[ OK ]` for both pass **and** fail | Now correctly prints `[FAIL]` for failures ([PR#206](https://github.com/Amiga500/Onion/pull/206)) | **silently failing tests now visible** | 🛡️ 🧪 |
| 🛡️ `file_isLocked()` O_CREAT flag | Removed in port, breaking save-state loop in `gs_popMenu.h` | Restored flag ([PR#206](https://github.com/Amiga500/Onion/pull/206)) | **save-state wait loop works again** | 🛡️ 🧪 |
| 🛡️ Dialog theme-render cache | Surfaces never released on theme change; memory accumulation | Added `theme_renderDialog_cleanup()` ([PR#206](https://github.com/Amiga500/Onion/pull/206)) | **memory leak closed** | 🛡️ 🧪 |
| 🛡️ MULTIVALUE label cache color | Cache keyed only on value, not color; disabled-state feedback broken | Cache now keyed on `color+value` pair ([PR#207](https://github.com/Amiga500/Onion/pull/207)) | **UX regression fixed** | 🛡️ 🧪 |
| 🛡️ `screenshot_save()` VLA undefined behavior | No guard on `g_display.width/height == 0`; VLA with 0-size is UB | Added dimension checks before VLA declaration ([PR#207](https://github.com/Amiga500/Onion/pull/207)) | **undefined behavior removed** | 🛡️ 🧪 |
| 🛡️ `includeCJK()` UTF-8 validation | Only checked first continuation byte; multi-byte validation incomplete | Validation now checks all 3 bytes of valid sequence ([PR#207](https://github.com/Amiga500/Onion/pull/207)) | **complete UTF-8 validation** | 🛡️ 🧪 |
| 🛡️ File I/O error reporting & consistency | `file_remove_recursive()` silently ignores `nftw()` errors; `file_changeKeyValue()` missing fsync | Added error logging & fsync before rename ([PR#207](https://github.com/Amiga500/Onion/pull/207)) | **data consistency + error visibility** | 🛡️ 🧪 |
| 🛡️ `currentGame()` NULL dereference sites | 3 callers dereferenced unchecked (`game_list_len == 0` → NULL) | NULL guards at all 3 sites ([`fa888f22`](https://github.com/Amiga500/Onion/commit/fa888f22)) | **crash on empty game list removed** | 🛡️ |
| 🛡️ `action_loadGame` slot bounds check | `< 0 && >= slot_count` — untriggerable dead code | `||` — out-of-range slots rejected ([`ff012faa`](https://github.com/Amiga500/Onion/commit/ff012faa)) | **dead check revived** | 🛡️ |
| 🛡️ `_isContentNameInInfo` left-boundary read | `*(found - 1)` read 1 byte before the buffer at position 0 | string start treated as valid left boundary ([`ff012faa`](https://github.com/Amiga500/Onion/commit/ff012faa)) | **OOB read removed** | 🛡️ |
| 🛡️ playActivity fork+exec | `waitpid()` on the child blocked the UI thread | double-fork, detached grandchild reparented to init ([`d05267ca`](https://github.com/Amiga500/Onion/commit/d05267ca)) | **async semantics restored, no zombies** | 🛡️ |
| ⚡ OSD overlay draw loop | full-throttle redraw for the overlay's entire duration | 2 ms `msleep` per iteration + stats demoted to `printf_debug` ([`74f0a0af`](https://github.com/Amiga500/Onion/commit/74f0a0af)) | **overlay CPU burn capped** | 📐 |
| 🧪 Active unit-test suites | 0 | 68 | **+68** ✅ | 🧪 |
| 🧪 Unit tests / assertions | 0 / 0 | 1,410 / 71,385 | **ALL PASSED** ✅ | 🧪 |
| 🏗️ Host test entry point | none | `make unit-test` | **added** ✅ | 🧪 |

*Call-site counts (`sprintf` / `strcpy` / `strcat` / `strtok` / `system`) are scoped to the
**original 25** common-layer + image-tool files of the NEON/hardening port, grepped at
`07505ea5` vs. the working tree. Closing the screenshot/jpg2png residuals brings that set
to **0** unbounded `sprintf`/`strcpy`/`strcat`. They are **not** codebase-wide claims.
The full `src/` delta is now **58 files** (TTF cache, signal-handler call sites, `reset.h`,
`config.mk`, Game Switcher review-pass call sites, …).*

*The three 🛡️ rows fixing the hash over-read, the uninitialised save-state path and the
`const` cast address **pre-existing upstream defects** rather than porting anything from
OniOpus46 — see [§4.7](#47-pre-existing-defects-fixed-in-this-branch). The final five rows
are the 2026-08-23 review pass at the tip — three more pre-existing upstream defects
(`currentGame()` NULL derefs, dead slot check, content-match OOB read, all present at
`07505ea5`), one port-regression fix (blocking `waitpid` from `45d4eec4`) and one perf
throttle — see [§4.9](#49-game-switcher-review-pass-fixes-commits-fa888f22-d05267ca-ff012faa).
No performance figure is attached to any of the 🛡️ rows.*

---

## 🔀 1. Commit Breakdown

| # | SHA | Subject | Files | +/− | Kind |
|:-:|:----|:--------|------:|----:|:-----|
| 1 | [`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1b40f6d021a58a41589d6371b30a81142) | ⚡ NEON pixel conversions ported from OniOpus46 | 7 | +483 / −80 | perf |
| 2 | [`6da7f28b`](https://github.com/Amiga500/Onion/commit/6da7f28b3a7503a46e0caa799cd85ab9117a0785) | 🎨 apply clang-format changes *(CI-generated)* | 2 | +2 / −2 | format |
| 3 | [`ad402fa2`](https://github.com/Amiga500/Onion/commit/ad402fa2e400af6538f971f89979ac0647daa98f) | 🛡️ Port common crash/memory hardening from OniOpus46 | 33 | +9,724 / −193 | hardening |
| 4 | [`1a1e3f84`](https://github.com/Amiga500/Onion/commit/1a1e3f84c7e8651e9b5a9f4d0954033b6c6cb3db) | 🧪 Limit unit-test suite list to ported hardening tests | 1 | +2 / −2 | test fix |
| 5 | [`300390a7`](https://github.com/Amiga500/Onion/commit/300390a7bfec1887acaa261cfd711b57c70c23fc) | 🧪 Expand host unit-test suite from OniOpus46 | 58 | +13,894 / −18 | test |
| 6 | [`971d6169`](https://github.com/Amiga500/Onion/commit/971d6169aa4a0fed355ea89e3a9b35b223598270) | 📚 Add OnionPlus optimization and diff-stat documentation | 2 | +683 / −0 | docs |
| 7 | [`deb8b6ad`](https://github.com/Amiga500/Onion/commit/deb8b6ad) | 🛡️ Fix pre-existing hash, save-state and `const` defects; refresh docs | 6 | +1,171 / −476 | fix + test + docs |
| 8 | [`c9e052d4`](https://github.com/Amiga500/Onion/commit/c9e052d4) | 🧪 Harden host unit-test CI with sanitizers and production contracts | 17 | +527 / −646 | test + CI |
| 9 | [`47fc5289`](https://github.com/Amiga500/Onion/commit/47fc5289) | ⚡ Unify NEON ifdefs; keep `jpg2png` out of `core` | 11 | +487 / −103 | perf + build |
| 10 | [`eb3f0aec`](https://github.com/Amiga500/Onion/commit/eb3f0aec) | ⚡ Port TTF label cache, shared signal handlers, and leftover hardening | 18 | *see aggregate* | perf + hardening |
| 11 | [`4b851203`](https://github.com/Amiga500/Onion/commit/4b851203) | ⚡ Port OSD busy-wait fix and thread hardening from OniOpus46 | 1 | +51 / −30 | perf |
| 12 | [`e8143d09`](https://github.com/Amiga500/Onion/commit/e8143d09) | ⚡ Port brightness sysfs caching and display hardening from OniOpus46 | 1 | +40 / −13 | perf |
| 13 | [`0121f943`](https://github.com/Amiga500/Onion/commit/0121f943) | ⚡ Port battery charging cache and batmon fixes from OniOpus46 | 3 | +96 / −30 | perf + fix |
| 14 | [`0d1ce423`](https://github.com/Amiga500/Onion/commit/0d1ce423) | ⚡ Port SQLite open/close optimization and DB hardening from OniOpus46 | 6 | +250 / −81 | perf + hardening |
| 15 | [`2c5b028a`](https://github.com/Amiga500/Onion/commit/2c5b028a) | 🛡️ Harden `config.h`: direct `mkdirs` and bounded copies | 1 | +4 / −7 | hardening |
| 16 | [`45d4eec4`](https://github.com/Amiga500/Onion/commit/45d4eec4) | ⚡ Replace GameSwitcher overlay shell-outs with fork+exec and syscalls | 1 | +26 / −6 | perf |
| 17 | [`bda89b2d`](https://github.com/Amiga500/Onion/commit/bda89b2d) | 🛡️ Port infoPanel hardening and enable `test_images_browser` | 4 | +163 / −54 | hardening + test |
| 18 | [PR#206](https://github.com/Amiga500/Onion/pull/206) | 🛡️ **CRITICAL**: restore `file_isLocked` O_CREAT, fix `RUN_TEST` [FAIL] reporting, add dialog cleanup | 4 | +58 / −15 | critical fix |
| 19 | [PR#207](https://github.com/Amiga500/Onion/pull/207) | 🛡️ **MEDIUM**: MULTIVALUE cache color, screenshot VLA guard, UTF-8 validation, fsync consistency | 6 | +42 / −28 | medium fix |
| 20 | [`2537c94d`](https://github.com/Amiga500/Onion/commit/2537c94d) | 🏗️ No `SDL_Color.a` on Miyoo toolchain (ARM build fix for PR#207 color key) | 1 | +4 / −4 | build fix |
| 21 | [`e44421e1`](https://github.com/Amiga500/Onion/commit/e44421e1) | 🔀 Merge remote OnionPlus (PR #206–207 fixes) into local power/CPU port batch | — | — | reconciliation |
| 22 | [`927685e8`](https://github.com/Amiga500/Onion/commit/927685e8) | 🛡️ Fix: drop `meterWidth` config cache in OSD bar (invalidation hazard) | 1 | +3 / −7 | fix |
| 23 | [`6b7f6357`](https://github.com/Amiga500/Onion/commit/6b7f6357) | 🏗️ build: update version date to 2026-08-23 | 1 | +1 / −1 | release |
| 24 | [`2ae2e79f`](https://github.com/Amiga500/Onion/commit/2ae2e79f) | 🏗️ CI: pre-release workflow uses `HEAD` SHA instead of `origin/main` | 1 | +1 / −1 | CI fix |
| 25 | [`fa888f22`](https://github.com/Amiga500/Onion/commit/fa888f22) | 🛡️ Guard `currentGame()` NULL returns at all call sites | 2 | +16 / −6 | fix |
| 26 | [`d05267ca`](https://github.com/Amiga500/Onion/commit/d05267ca) | 🛡️ Restore async semantics for playActivity fork+exec | 1 | +24 / −20 | fix |
| 27 | [`ff012faa`](https://github.com/Amiga500/Onion/commit/ff012faa) | 🛡️ Correct dead-code slot check and OOB read in content match | 2 | +4 / −2 | fix |
| 28 | [`74f0a0af`](https://github.com/Amiga500/Onion/commit/74f0a0af) | ⚡ Throttle OSD overlay draw loop and demote stats logging | 1 | +4 / −4 | perf |
| | | **Aggregate `07505ea5` → `74f0a0af` (code tip, excl. `docs/`)** | **138** | **+25,538 / −766** | |
| | | **Full range `07505ea5` → this docs refresh** | **140** | **+27,059 / −766** | |

> ℹ️ **`git rev-list --count 07505ea5..HEAD` is 48**, not the 28 milestone rows above.
> The extra 20 are: 3 merge commits (`f9bac9fc`, `55de00a9`, `e44421e1` is listed), 3 CI
> `clang-format` passes, 6 docs-only refreshes (including `971d6169` listed as #6), a
> 5-commit remote experiment (`eea25f88`…`4f7841e0`) that is **net zero** in the tree, the
> individual PR #206/#207 commits grouped as rows 18–19, and the 2026-08-22 version bump.
> A previous revision of this document advertised **23** commits; that was a curated
> milestone count, not `rev-list`.
>
> [`6da7f28b`](https://github.com/Amiga500/Onion/commit/6da7f28b3a7503a46e0caa799cd85ab9117a0785)
> was generated by the CI formatting workflow on PR #197 (committer
> `GitHub Actions <actions@github.com>`, 2 whitespace lines, zero semantic change).
>
> **Timeline:**
> - **Commits 1–10**: OniOpus46 port foundation (NEON + hardening + test suite).
> - **Commits 11–17**: Power/CPU optimization batch ported on **2026-08-22** (OSD busy-wait, brightness caching, battery caching, DB optimization, config hardening, GameSwitcher fork+exec, infoPanel hardening).
> - **Commits 18–20**: Defect fixes from parallel remote work (PR #206–207, merged **2026-08-23**).
> - **Commit 21**: Merge of remote `OnionPlus` branch containing PR #206–207 into local main (**2026-08-23**).
> - **Commit 22**: Reconciliation fix — dropped `meterWidth` cache in OSD bar ([`927685e8`](https://github.com/Amiga500/Onion/commit/927685e8); a previous revision mis-attributed this SHA to `6b7f6357`).
> - **Commit 23**: Version bump to today's date for release packaging (**2026-08-23**).
> - **Commit 24**: Pre-release CI tags the built `HEAD`, not `origin/main`.
> - **Commits 25–28**: 2026-08-23 review pass — three upstream NULL/bounds defects, one port-regression (blocking `waitpid`), one overlay-loop throttle. See [§4.9](#49-game-switcher-review-pass-fixes-commits-fa888f22-d05267ca-ff012faa).
>
> A parallel remote experiment (`eea25f88..1e359571`: OSD busy-wait, `meterWidth` cache, `process_start` fork+execv) was **reverted** in
> [`4f7841e0`](https://github.com/Amiga500/Onion/commit/4f7841e0) — broken argument
> tokenization in `process_start` and missing theme-change invalidation in `meterWidth` cache — and is net zero in the tree.

📈 Split by area:

| Area | Files | +/− | Share of insertions |
|:-----|------:|----:|--------------------:|
| 🧪 `test/` | 75 | +23,233 / −10 | **85.9 %** |
| 🧩 `src/` | 58 | +2,252 / −750 | **8.3 %** |
| 📚 `docs/` | 2 | +1,521 / −0 | **5.6 %** |
| 🏗️ `Makefile` + `.github/` + `.gitignore` | 5 | +53 / −6 | **0.2 %** |

> 📈 **Result:** the production-code surface of this port is still small —
> **+2,252 / −750 across 58 `src/` files**, i.e. **8.3 %** of the added lines. Everything else
> is test scaffolding, CI wiring and documentation. The `docs/` insertion count includes
> this refresh.

---

## ⚡ 2. Performance — NEON Pixel Paths

Commit [`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1b40f6d021a58a41589d6371b30a81142)
adds `src/common/utils/neon_pixel.h` (**343 lines**): 7 inline pixel kernels in ARM NEON
assembly, each guarded by `#ifdef __ARM_NEON` with a **scalar C fallback** and a scalar tail
loop for the remainder. An 8th NEON path (intrinsics, not assembly) lives in
`surfaceSetAlpha.h`.

### 2.1 Kernel table

| NEON Function | Instructions | Throughput | Speedup | Evidence |
|:---|:---|:---:|---:|:---:|
| `neon_rotate180_inplace()` | `VLD1` / `VREV64` / `VSWP` | **16 px/iter** *(8 low ↔ 8 high)* | 🚀 **+5000 %** | 📏 |
| `neon_swap_rb_inplace()` | `VLD4.8` / `VST4.8` | **16 px/iter** | 🚀 ~**+800 %** | 📏 |
| `neon_argb_to_rgba()` | `VLD4.8` / `VST4.8` | **16 px/iter** | 🚀 ~**+800 %** | 📏 |
| `neon_rgb888_to_argb()` | `VLD3.8` / `VST4.8` | **16 px/iter** | 🚀 ~**+800 %** | 📏 |
| `neon_argb_to_rgba_alpha()` | `VLD4.8` / `VCEQ` / `VST4.8` | **8 px/iter** | 🚀 ~**+600 %** | 📏 |
| `neon_gray8_to_argb()` | `VLD1.8` / `VST4.8` | **16 px/iter** | 🚀 ~**+600 %** | 📏 |
| `neon_gray8a_to_argb()` | `VLD2.8` / `VST4.8` | **16 px/iter** | 🚀 ~**+500 %** | 📏 |
| `surfaceSetAlpha` NEON | `VMOVL` / `VMUL` / `VSHRN` | **8 px/iter** | 🚀 ~**+400 %** | 📏 |

> 📏 **180° rotation:** OniOpus46 measured **~2 ms → ~40 µs** on the same in-place `VREV64`
> code path = **+5000 %**. OnionPlus ships that exact kernel.
>
> 📐 **Throughput column:** `px/iter` is read off the loop guards in *this* repo
> (`count & ~15` / `count & ~7` / the `hi - lo >= 15` window), so it is exact for OnionPlus.
> ⚠️ It **disagrees with the OniOpus46 table in three rows** — that document lists
> `neon_rotate180_inplace` and `neon_gray8a_to_argb` at 8 px/iter and
> `neon_argb_to_rgba_alpha` at 16. The values above are the ones the OnionPlus code actually
> implements. Throughput is also *not* the same thing as wall-clock speedup: on a
> memory-bound Cortex-A7 the real gain is capped by SD-card and DRAM bandwidth.

### 2.2 Before / after — 180° rotation

```c
// BEFORE — allocate a whole new surface, rotozoom blit, fill, blit back
SDL_Surface *rotated = rotozoomSurface(original, 180.0, 1.0, 0);
SDL_FillRect(original, NULL, SDL_MapRGB(original->format, 255, 0, 0));
SDL_BlitSurface(rotated, NULL, original, &rect);
SDL_FreeSurface(rotated);
```

```c
// AFTER — zero allocations, in-place vector reverse for the common 32bpp case
if (original->format->BytesPerPixel == 4 && original->pitch == original->w * 4) {
    SDL_LockSurface(original);
    neon_rotate180_inplace((uint32_t *)original->pixels, original->w * original->h);
    SDL_UnlockSurface(original);
    return original;
}
/* non-32bpp / padded pitch keeps the stock rotozoom path */
```

> 📈 **Result:** one full surface allocation + two blits eliminated per rotation
> (📐 exact, structural). Speedup magnitude is 📏 inherited from OniOpus46.

### 2.3 Before / after — `surfaceSetAlpha`

```c
// BEFORE — per pixel: SDL_GetRGBA + float multiply + SDL_MapRGBA
float scale = alpha / 255.0f;
for (int y = 0; y < surface->h; ++y)
    for (int x = 0; x < surface->w; ++x) {
        Uint8 r, g, b, a;
        SDL_GetRGBA(*pixel_ptr, fmt, &r, &g, &b, &a);
        *pixel_ptr = SDL_MapRGBA(fmt, r, g, b, scale * a);
    }
```

```c
// AFTER — integer fixed point, no libSDL call per pixel, NEON 8 px/iter when contiguous
const uint32_t alpha_scale = ((uint32_t)alpha * 257 + 1) >> 8;  // 255 → 256 → identity
uint32_t a = (px & a_mask) >> a_shift;
a = (a * alpha_scale) >> 8;
pixels[i] = (px & rgb_mask) | (a << a_shift);
```

> 🐞 **OnionPlus fixes an OniOpus46 bug here.** The OniOpus46 NEON branch multiplied by the
> raw `alpha` and shifted by 8, which breaks the `alpha == 255` identity and the `alpha == 0`
> case. OnionPlus forces the NEON branch to use the same `alpha_scale` as the scalar branch.
> 🧪 Covered by `test_alpha_scale` — **20 tests / 65,827 assertions**, including rounding,
> boundary values, identity at `alpha == 255` and commutativity of consecutive blends.

### 2.4 Call sites converted

| File | Change | Kernels used |
|:-----|:-------|:-------------|
| `src/pngScale/pngScale.c` | All 4 channel cases (gray8, gray8+alpha, RGB888, RGBA) plus the output R/B swap replaced with NEON via thin `static inline` wrappers | 4 |
| `src/jpg2png/jpg2png.c` | Two scalar per-pixel loops (RGB888→ARGB decode, ARGB→RGBA encode) replaced | 2 |
| `src/common/system/screenshot.h` | Per-pixel ARGB→RGBA conversion replaced (normal path) + in-place R/B swap (rotated path) | 2 |
| `src/common/utils/IMG_Save.h` | Per-pixel alpha-conditional conversion replaced | 1 |
| `src/common/utils/rotate180.h` | 32bpp contiguous surfaces rotated in place instead of via rotozoom | 1 |
| `src/common/utils/surfaceSetAlpha.h` | Float alpha scaling → fixed-point + NEON intrinsics | 1 |

> 📈 **Result:** **6 files**, **11 scalar per-pixel loops** replaced by vectorised paths.
> `jpg2png` is built like `pngScale` but is **intentionally not part of `make core`**: the
> Miyoo sysroot used by the project does not currently ship libjpeg. Build it with
> `make -C src/jpg2png` when the headers are available.

### 2.5 ⚠️ What is *not* proven

- ✅ 🧪 The **scalar fallback** of every kernel is verified. `test_neon` (44 tests / 1,402
  assertions) and `test_neon_pixel` (38 / 72) exercise each function's portable C path against
  hand-computed values and against scalar oracles of the same kernels, including single-pixel,
  zero-alpha, `count <= 0` and non-multiple-of-16 counts.
- ✅ 🧪 A separate CI job (`neon-arm`) cross-compiles the NEON tests with
  `gcc-arm-linux-gnueabihf -mfpu=neon` and runs them under `qemu-user-static`, so the
  `__ARM_NEON` assembly is **executed** in CI when those packages are present. That job is
  **not** the host `make unit-test` path.
- ❌ **No OnionPlus on-device benchmark exists.** Every 📏 figure above is OniOpus46's.

---

## 🚀 3. Performance — Algorithmic & Syscall Wins

### 3.1 `str_count_char` — O(n²) → O(n) 📏 −90 %

```c
// BEFORE — strlen() re-evaluated on every iteration ⇒ quadratic, and reads 1 byte past the end
for (i = 0; i <= strlen(str); i++)
    if (str[i] == ch) count++;
```

```c
// AFTER — single pointer walk
for (const char *p = str; *p; p++)
    if (*p == ch) count++;
```

> 📏 **−90 %** comparisons for long strings (OniOpus46 figure for this exact rewrite).
> 📐 For a 1,000-char string: ~1,000,000 → 1,000 character reads.
> 🧪 Covered by `test_str` (72 tests / 357 assertions).
> 🛡️ Also fixes a 1-byte over-read (`i <= strlen`).

### 3.2 `file_removeExtension` — `strlen` ×2 → ×1 📏 −50 % scans

```c
// BEFORE — strlen() for the malloc, then strcpy() rescans the same string
char *retStr = (char *)malloc(strlen(myStr) + 1);
strcpy(retStr, myStr);
```

```c
// AFTER — one scan, then a length-known memcpy
size_t len = strlen(myStr);
char *retStr = (char *)malloc(len + 1);
memcpy(retStr, myStr, len + 1);
```

> 📏 **−50 %** string scans (OniOpus46 figure). 🧪 `test_file` (88 / 183).

### 3.3 Path assembly — quadratic `strcat` → offset walk 📐

`strcat()` in a loop rescans the destination from byte 0 on every append, making path
assembly O(n²). Two functions were rewritten to track an explicit `offset`:

| Function | Before | After | Δ | Evidence |
|:---|:---|:---|--:|:---:|
| `file_path_relative_to()` | `str_count_char` *(itself O(n²))* + `strcat` loop | inline `/` count + `memcpy` at `offset` | **O(n²) → O(n)**, 📏 −50 % scans | 📏 🧪 |
| `file_resolvePath()` | `strcat(resolvedPath, "/")` + `strcat(…, component)` per component | `memcpy` at `offset`, bounds-checked against `PATH_MAX` | **O(n²) → O(n)** | 📐 🧪 |

> 🛡️ Both rewrites also add `PATH_MAX` bounds checks that the `strcat` versions lacked.
> 🧪 `test_file` + `test_file_security` (128 tests combined).

### 3.4 `system()` → direct syscalls 📐 −100 % process spawns

Each `system()` call forks `/bin/sh -c "…"`, which in turn forks the real binary — **2 extra
processes** plus shell parsing, on every invocation.

| Function | Before | After | Processes spawned |
|:---|:---|:---|:---:|
| `mkdirs()` | `sprintf(cmd, "mkdir -p \"%s\"", p); system(cmd);` | iterative `mkdir(tmp, 0755)` walk, `true` on `0` or `EEXIST` | **2 → 0** |
| `file_copy()` | `snprintf(cmd, …, "cp -f \"%s\" \"%s\"", …); system(cmd);` | `open`/`read`/`write`/`close` loop preserving `st.st_mode` | **2 → 0** |

```c
// AFTER — mkdirs(): pure syscalls, no shell, no injection surface
for (char *p = tmp + 1; *p; p++) {
    if (*p == '/') { *p = '\0'; mkdir(tmp, 0755); *p = '/'; }
}
return mkdir(tmp, 0755) == 0 || errno == EEXIST;
```

> 📐 **Result:** **−100 %** process spawns on both paths (2 `fork`+`exec` pairs → 0), and
> **−100 %** shell-injection surface — a filename containing `"` or `;` no longer reaches
> `/bin/sh`. 🧪 Covered by `test_file` (88 tests) and `test_file_security` (40 tests).
> 📏 OniOpus46 quotes **−80 %** process overhead for its broader `system()` → `fork`+`exec`
> substitution; that is a *different, unported* change — the OnionPlus rewrites remove the
> child processes entirely rather than just the intermediate shell.

### 3.5 `file_read()` — stdio → direct `read()` 📐

```c
// BEFORE — fopen, seek to end, ftell, seek back, buffered fread, fclose
fseek(f, 0, SEEK_END); length = ftell(f); fseek(f, 0, SEEK_SET);
buffer = malloc(length + 1);
fread(buffer, sizeof(char), length, f);
```

```c
// AFTER — one stat64, one open, a read() loop straight into the destination
if (stat64(path, &st) != 0 || st.st_size < 0) return NULL;
if (st.st_size > 100 * 1024 * 1024) return NULL;   // 🛡️ allocation cap
int fd = open(path, O_RDONLY);
while (total < st.st_size) { ssize_t n = read(fd, buffer + total, st.st_size - total); … }
```

> 📐 **Result:** 2 `lseek` syscalls replaced by 1 `stat64`, and the stdio intermediate buffer
> copy is eliminated — data lands directly in the caller's allocation. On an SD-card-backed
> filesystem this is a small win per call, **not benchmarked**. 🛡️ Also adds a 100 MB
> allocation cap, a partial-read loop and a NULL return on failure (previously
> `buffer[length] = '\0'` was reachable with `buffer == NULL`). 🧪 `test_file`, `test_file_security`.

### 3.6 `list.h` + theme render — TTF/preview surface cache 📏 5–15 ms/frame

`ListItem` carries six fields for TTF/preview surface caching (`_label_cache`, `_label_hash`,
`_value_cache`, `_cached_value`, `_scaled_preview`, `_scaled_preview_w`). `list_free()`
releases them. The OniOpus46 render-side code that **populates** those fields now lives in
`src/common/theme/render/list.h` (`_theme_renderListLabelCached`, MULTIVALUE value cache,
scaled-preview cache). Matching static caches were ported in `footer.h`, `header.h` and
`dialog.h` (hint labels, footer page numbers, header title, dialog OK/Cancel + dim overlay).

> 📏 **5–15 ms/frame saved** is OniOpus46's figure for this same TTF-cache code path.
> 🧪 `test_list` covers bounds, `list_free` teardown of the slots, and does **not** fake a
> cache hit-rate (no SDL/TTF in the host suite).

### 3.7 Power & CPU — OSD, battery, display, DB, overlay (commits 11–16)

These six commits are the batch with real **battery/thermal relevance**: they cut work in
always-on loops and daemons rather than in burst paths. No on-device power measurement
exists; magnitudes are 📏 inherited or 📐 structural.

**OSD busy-wait** ([`4b851203`](https://github.com/Amiga500/Onion/commit/4b851203)) —
`src/common/system/osd.h`. The volume/brightness bar thread polled at `usleep(100)`
(~10,000 loops/s for the 2 s the bar is visible). Now `usleep(16000)` (~60 fps redraw).
📏 OniOpus46 measured idle CPU **~10 % → <1 %** for this change. The commit also adds
`volatile` to thread-shared state, shrinks the bar save buffer **160×**
(`meterWidth × height` instead of `width × height`) with `memcpy` row copies, guards the
`numBuffers` division against `yres == 0`, and closes
two `free(data)` leaks on `overlay_surface()` error paths. The `meterWidth` config read is
**not** cached: a cached value would go stale on theme change with no invalidation path
(the defect behind the remote revert `4f7841e0`), so [`927685e8`](https://github.com/Amiga500/Onion/commit/927685e8)
dropped the cache — `config_get` runs once per bar activation, which is negligible.

The `overlay_surface()` **draw loop** (a different path from the volume/brightness bar
thread) still spun full-throttle after that port. [`74f0a0af`](https://github.com/Amiga500/Onion/commit/74f0a0af)
caps it with `msleep(2)` per iteration and demotes the draw-count/speed lines from
`printf` to `printf_debug`. 📐 structural — this is an OnionPlus review fix, not an
OniOpus46 port.

**Brightness sysfs caching** ([`e8143d09`](https://github.com/Amiga500/Onion/commit/e8143d09)) —
`src/common/system/display.h`. `display_setBrightnessRaw()` now skips the
`pwm0/duty_cycle` sysfs write when the value is unchanged (📏 OniOpus46: **−100 % duplicate
writes**); the cache is invalidated after the PWM re-export in `display_setScreen()`, and
`display_getBrightnessFromRaw()` guards `log()` against non-positive raw values. Same
commit: `memcpy` fast path for contiguous rows in `display_readOrWriteBuffer()`,
`yres == 0` guards, `display_drawFrame()` sized from `g_display` instead of hardcoded
640×480, overflow-checked `display_save()` allocation, `fb_fd >= 0` close fix. Verified no
other code path writes `duty_cycle`, so the cache cannot go stale.

**Battery charging cache + batmon** ([`0121f943`](https://github.com/Amiga500/Onion/commit/0121f943)) —
`src/common/system/battery.h`, `src/batmon/`. On MIYOO354 every uncached
`battery_isCharging()` call forks `/customer/app/axp_test` (~5–10 ms); the new 2 s cached
wrapper eliminates ~99 % of those spawns in hot loops (📐 structural — keymon, batmon and
chargingState all poll it). In batmon: `warnAt` config is read only at check timeout
instead of every loop tick, the low-battery warning thread sleeps 500 ms instead of ~16 ms,
`getBatPercMMP()` uses `popen` instead of `system()` + temp file, `sar_fd` is initialised
to `-1` with guarded `close`/`ioctl`, signal-shared state is `volatile sig_atomic_t`, and
two `sqlite3_finalize` placement bugs (skipped finalize) are fixed.

**SQLite open/close 2 → 1** ([`0d1ce423`](https://github.com/Amiga500/Onion/commit/0d1ce423)) —
`src/playActivity/`, `src/playActivityUI/`. Each play-activity operation now runs through a
single `open` → `sqlite3_exec` → `close` cycle instead of two (📏 OniOpus46: **−50 % DB
I/O**). The same commit ports the associated DB hardening: `stmt` NULL guards, finalize/free
placement fixes in `migrateDB.h` (including a use-after-free of `sql`), malloc-failure
cleanup in `play_activity_find_all()`, leak fixes in `free_play_activities()` and
`cacheDB.h`, and NULL/zero-dimension guards in `playActivityUI` image loading.

**`config.h` hardening** ([`2c5b028a`](https://github.com/Amiga500/Onion/commit/2c5b028a)) —
`_config_prepare()` drops `system("mkdir -p …")` for the hardened `mkdirs()` walk
(📐 −100 % proc spawns on that path), bounds the `dir_path` copy, and
`config_setString()` takes `const char *`.

**Overlay fork+exec** ([`45d4eec4`](https://github.com/Amiga500/Onion/commit/45d4eec4),
fixed by [`d05267ca`](https://github.com/Amiga500/Onion/commit/d05267ca)) —
`src/gameSwitcher/gs_overlay.h`. Six shell-outs removed: `playActivity stop_all`/`resume`
now run via double-fork + `execl` (📏 OniOpus46: **−80 % process overhead**), and the
RetroArch `killall`/`pidof` calls in `overlay_exit()` use the direct
`process_kill_signal()`/`process_isRunning()` helpers (📐 −100 % shell spawns).
The first replacement called `waitpid()` on the worker and blocked the UI thread;
`d05267ca` restores the original `system("… &")` async semantics (intermediate child
exits immediately, grandchild reparented to init, no zombies). See [§4.9](#49-game-switcher-review-pass-fixes-commits-fa888f22-d05267ca-ff012faa).

---

## 🛡️ 4. Security & Hardening

Counts below are over the **original 25 `src/` files** of the NEON/hardening port
(common utils + `screenshot.h` + `jpg2png.c` + `pngScale.c` + `gs_popMenu.h`), at
`07505ea5` vs the working tree. The full `src/` delta is 58 files; extra files (theme
render, signal-handler call sites, `reset.h`, `config.mk`) are **not** in these percentages.

### 4.1 `sprintf` → `snprintf` — 23 → 0 call sites (**−100 %**) ✅

| File | `sprintf` removed | `snprintf` added |
|:-----|------------------:|-----------------:|
| `src/common/utils/process.h` | 7 | 7 |
| `src/common/system/state.h` | 6 | 6 |
| `src/common/system/screenshot.h` | 1 | 1 |
| `src/common/utils/file.c` | 3 | 4 |
| `src/common/utils/str.c` | 3 | 3 |
| `src/common/utils/log.c` | 1 | 2 |
| `src/common/components/list.h` | 2 | 2 |
| `src/common/utils/str.h` | 0 | 1 |
| **Total** | **23** | **25** |

Across the same 25 files `snprintf` went **11 → 34** (net **+23**) while `sprintf` went
**23 → 0**. `log.c` also fixed an off-by-one (`snprintf(_log_path, 63, …)` → `sizeof(_log_path)`)
and replaced an unbounded `vsprintf` with a length-checked `vsnprintf`. The last remaining
`sprintf` in this set (`screenshot.h` `"_%03d.png"`) is now a bounded `snprintf` of the
full path ([§10](#️-10-known-residuals) previously listed it).

> 📈 **Result:** **23 of 23** unbounded format calls eliminated (**−100 %**). ✅

### 4.2 `strcpy` / `strcat` → bounded copies — 37 → 0 (**−100 %**) ✅

| File | removed | bounded added |
|:-----|--------:|--------------:|
| `src/common/utils/file.c` | 7 | 1 |
| `src/common/system/state.h` | 7 | 5 |
| `src/common/system/settings.h` | 6 | 8 |
| `src/common/utils/str.c` | 2 | 0 *(→ `memcpy`)* |
| `src/common/utils/str.h` | 2 | 0 *(→ `snprintf` macro)* |
| `src/common/utils/process.h` | 2 | 1 |
| `src/common/components/list.h` | 2 | 2 |
| `src/common/system/screenshot.h` | 7 | 0 *(→ one `snprintf` of the full path)* |
| `src/jpg2png/jpg2png.c` | 2 | 0 *(→ `snprintf`)* |

Every introduced `strncpy` is paired with an explicit `dst[sizeof(dst) - 1] = '\0'`, closing
the "`strncpy` does not null-terminate when `strlen(src) >= n`" trap at each new site.

**Notable rewrites:**

```c
// BEFORE — str.h: the codebase-wide concat() macro had no bound at all
#define concat(ptr, str1, str2) { strcpy(ptr, str1); strcat(ptr, str2); }
```

```c
// AFTER — bounds *every* concat() call site in the codebase in one line
#define concat(ptr, str1, str2) snprintf(ptr, STR_MAX, "%s%s", str1, str2)
```

- 🛡️ `state.h` — a VLA-based `strcpy(secondPart, colonPosition + 1)` / `strcpy(romPathSearch, secondPart)`
  pair (stack VLA sized from untrusted input) replaced with a single in-place `memmove`.
- 🛡️ `settings.h` — 5 struct-field copies in `settings_copy()` converted to `strncpy` + terminator.
- 🛡️ `str.c` — `strcpy(tmp, orig)` in `str_replace` replaced with `memcpy(tmp, orig, strlen(orig) + 1)`.

📊 Bounded-primitive adoption across the same 25 files:

| Primitive | Before | After | Δ |
|:---|---:|---:|---:|
| `snprintf` | 11 | 34 | **+209 %** |
| `strncpy` | 13 | 25 | **+92 %** |
| `memcpy` | 3 | 17 | **+467 %** |

*The `snprintf` ratio looks smaller than in the previous revision of this document (**+575 %**)
purely because the scope widened from 23 to 25 files: the 7 pre-existing calls in
`gs_popMenu.h` now sit in the "before" column. The number of call sites converted is
identical.*

> 📈 **Result:** **37 of 37** unbounded copies eliminated in this file set (**−100 %**). ✅
> Because `concat()` is a macro, the *real* reduction across the whole codebase is larger
> than this figure suggests. Files outside the 25 (for example `chargingState.c`) still
> contain `strcpy`; they are not in this percentage.

### 4.3 `strtok` → `strtok_r` — 4 → 0 (**−100 %**) ✅

All 4 non-reentrant tokenizer call sites — every one of them in `src/common/utils/file.c`,
across `file_readLastLine()` and `file_resolvePath()` — converted to `strtok_r` with a
caller-owned save pointer.

> 📈 **Result:** **−100 %** non-reentrant tokenizer use in the ported files. ✅
> 🛡️ `strtok` keeps hidden global state, so any two callers interleaving (or a signal handler
> tokenising) silently corrupt each other's iteration.

### 4.4 `system()` → POSIX — 3 → 1 (**−66.7 %**)

Covered in [§3.4](#34-system--direct-syscalls--100--process-spawns) — 2 of 3 shell-out sites
in the original 25-file set removed, eliminating both the process overhead and the
path-injection vector.

Additionally, `file_remove_recursive()` was added using `nftw(FTW_DEPTH | FTW_PHYS)` as a
shell-free replacement for `rm -rf`. Production call sites in `src/tweaks/reset.h` now use
it for the three `rm -rf` resets (tweaks config, theme overrides, RA core overrides), plus
`remove()`/`file_copy()`/`mkdirs()` instead of `rm -f`/`cp`/`mkdir -p` on the MainUI reset
path. Remaining `system()` calls in that file are `7z` invocations, which are not recursive
deletes.

> 🛡️ `process.h:101` (`process_start_*`) still shells out; replacing it needs a `fork`/`execv`
> redesign — see [§10](#️-10-known-residuals).

### 4.5 NULL-pointer and I/O return-value guards — **+57 predicates, +18 closes**

| File | Guards added |
|:-----|-------------:|
| `src/common/system/state.h` | 13 |
| `src/common/utils/file.c` | 12 |
| `src/common/components/list.h` | 9 |
| `src/common/utils/process.h` | 5 |
| `src/common/system/settings.h` | 5 |
| `src/common/utils/json.h` | 4 |
| `src/common/system/screenshot.h` | 4 |
| `src/common/utils/str.c` | 1 |
| `src/common/utils/rotate180.h` | 1 |
| `src/common/utils/IMG_Save.h` | 1 |
| `src/common/utils/perf.h` *(new)* | 1 |
| `src/gameSwitcher/gs_popMenu.h` | 1 |
| **Total** | **57** |

Concrete latent-defect fixes:

| File | 🐞 Fix |
|:-----|:----|
| `axp.h` | `open(AXPDEV, O_RDWR)` unchecked → `if (fd < 0) return -1`. `ioctl(-1, …)` was reachable (2 sites). |
| `clock.h` | `open("/dev/rtc0") > 0` → `>= 0`. fd `0` is a valid descriptor. |
| `clock.h` | `getMilliseconds()` returned `int` from `te.tv_sec * 1000` — overflow after ~24.8 days uptime. Now `long`. |
| `flags.h` | `close(creat(filename, 777))` → `creat(filename, 0644)` with `fd >= 0` checked. `777` decimal is mode `01411`. |
| `file.c` | `file_isLocked()` used `O_RDONLY \| O_CREAT, 0666` — the lock *check* created the file. Now `O_RDONLY`. |
| `file.c` | `if ((fd = open(path, O_WRONLY)) == 0)` → `< 0`, plus `close(fd)` on the write-failure path. |
| `file.c` | 100 MB cap on `st.st_size` before `malloc`. |
| `file.c` | `sscanf` given a bounded field width (`%255[^%c]`). |
| `file.c` | 2 write paths made atomic via a `.tmp` sibling + `rename()` (previously a shared `temp.txt` in CWD). |
| `json.h` | `cJSON_GetStringValue()` NULL-checked before `strncpy`; `json_load` validates contents; `json_save` checks `cJSON_Print`. |
| `process.h` | `opendir("/proc")` NULL-guarded; `atoi` → `strtol`; `fscanf` return checked; unbounded `strcpy(out_str, result)` bounded. |
| `state.h` | Hardcoded `str += 19` to skip `HOME=<path> ./` replaced with a `strstr(str, " ./")` search — the old code mis-parsed whenever `HOME` was not exactly `/mnt/SDCARD`. |
| `state.h` | `main_total - 4` / `total - page_size` could go negative → clamped to `0`. |
| `state.h` | `strstr(...) + 7` / `+ 11` dereferenced without a NULL check; each now `continue`s, and the per-line allocation is freed on every path (an early `return NULL` leaked it). |
| `list.h` | `list_getVisibleItemAt` walked past `item_count`; `list_addItem` now checks `items != NULL` **and** `item_count < max_items` before writing; `list_free` NULL-guards `items`. |
| `str.c` | `str_replace` allocation size computed in `size_t` with explicit multiplication- **and** addition-overflow guards (previously signed `int`). |
| `str.c` | `str_trim` skip loop could read past the terminator. The all-whitespace case returned length `1` while writing an empty string; now returns `0`. |
| `str.c` | CJK detection tested `c >= 0x80 && c <= 0x9FFF` on an `unsigned char` — the upper bound was unreachable, so *every* byte ≥ 0x80 matched. Rewritten as a real 3-byte UTF-8 sequence check. |
| `IMG_Save.h` | Line buffer allocated from `pitch` but indexed by `width`; now `width * sizeof(Uint32)`, NULL-checked, row pointer advances by `pitch`. |
| `screenshot.h` | `png_create_write_struct` / `png_create_info_struct` NULL-checked with `fclose(fp)` on each failure path. |

> 📈 **Result:** **57** guard predicates and **18** descriptor closes added. **🛡️ robustness
> only — no performance claim is attached to any item in this table.**

### 4.6 Code deduplication — `signal_handler.h`

New shared header `src/common/utils/signal_handler.h` (**47 lines**) providing
`signal_handler_quit(volatile bool *quit_flag, int sig)` for SIGINT/SIGTERM. Note the
`volatile` qualifier — the previous per-app copies wrote to a plain `bool` from signal context.

Call sites now use the helper in **tweaks**, **prompt**, **playActivityUI**, **infoPanel**,
**chargingState** and **batteryMonitorUI**. Game Switcher keeps a two-flag handler (`quit` +
`exit_to_menu`) and includes the header; `batmon` and `keymon` keep custom handlers because
they also catch SIGUSR1 / SIGSTOP / SIGCONT / SIGSEGV, which `signal_handler_quit` does not
map. 🧪 `test_signal_handler` (10 tests) includes the production header.

### 4.7 Pre-existing defects fixed in this branch

The three items below are **not ports**. They are latent defects present in the upstream base
`07505ea5`, surfaced by running the newly ported host suite under **ASan + UBSan**, by reading
the code the suite covers, and by compiling with **`-Wcast-qual`** (which `-Wall` does not
enable). Each is 🛡️ **robustness / correctness only — no performance figure is claimed for any
of them.**

#### 4.7.1 `hash.h` — bounded, alignment-safe 64-bit load

`FNV1A_Pippip_Yurii` read a full `uint64_t` through a cast pointer regardless of how many
bytes the caller actually owned, and did so at arbitrary addresses.

```c
// BEFORE — reads 8 bytes even for a 3-byte string, at whatever alignment `str` happens to have
#define _PADr_KAZE(x, n) (((x) << (n)) >> (n))
…
hash64 = (hash64 ^ (*(uint64_t *)(str))) * PRIME;
hash64 = (hash64 ^ (*(uint64_t *)(str + NDhead))) * PRIME;
…
hash64 = (hash64 ^ _PADr_KAZE(*(uint64_t *)(str + 0), (8 - wrdlen) << 3)) * PRIME;
```

```c
// AFTER — zero-extending little-endian load of exactly the bytes that exist
static inline uint64_t _hash_load64le(const char *str, size_t len)
{
    uint64_t value = 0;
    memcpy(&value, str, len);
    return value;
}
…
hash64 = (hash64 ^ _hash_load64le(str, 8)) * PRIME;
hash64 = (hash64 ^ _hash_load64le(str + NDhead, 8)) * PRIME;
…
hash64 = (hash64 ^ _hash_load64le(str, wrdlen)) * PRIME;
```

Three distinct instances of undefined behaviour are removed:

| 🐞 Defect | Consequence |
|:---|:---|
| **Out-of-bounds read.** For `wrdlen < 8` the old short-string path loaded 8 bytes and masked the surplus away with `_PADr_KAZE`, so up to **7 bytes past the end** of the caller's buffer were read before being discarded. | The upstream header papered over this by *documenting* the hazard: *"CAUTION: Add 8 more bytes to the buffer being hashed, usually `malloc(...+8)`"*. Every caller had to honour a convention the compiler could not check. ASan reports it as `heap-buffer-overflow … READ of size 8`. |
| **Unaligned load.** `*(uint64_t *)str` is undefined behaviour whenever `str` is not 8-byte aligned, and on the Miyoo Mini's ARMv7 Cortex-A7 the `LDRD` instruction GCC emits for a 64-bit load **traps on unaligned addresses**. Path strings come from `strtok_r` results, `strrchr` offsets and struct fields, so odd alignments are routine. | A real crash risk on device, not merely a diagnostic. UBSan reports `load of misaligned address … which requires 8 byte alignment` at all three load sites. |
| **Oversized shift.** For the empty string `_PADr_KAZE(x, (8 - 0) << 3)` evaluates to `x << 64`, and a shift by the full width of the type is undefined in C. | UBSan reports `shift exponent 64 is too large for 64-bit type`. The rewrite has no shift at all, so the case disappears rather than being clamped. |

> 🔐 **Hashes are bit-identical — this was the hard constraint.** The values feed on-device
> file names (`/mnt/SDCARD/Saves/CurrentProfile/romScreens/<hash>.png`) and the play-activity
> cache-database keys, so any change would orphan every cached file already sitting on a
> user's SD card. Zeroing the unavailable bytes in an aligned local is arithmetically the same
> thing the `(x << n) >> n` mask did, and the long-string path is unchanged, so the output is
> the same by construction. It was also checked empirically: **264 reference vectors**
> (33 strings × 8 start offsets) captured from the *old* implementation, replayed against the
> new one at `-O0`, `-O1`, `-O2`, `-O3` and `-Os`.
>
> 🧪 Those vectors are now permanent regression tests. `test_hash` grows from **12 tests /
> 21 assertions** to **15 / 350** via three new cases: `hash_regression_vectors` (fixed
> expected values), `hash_unaligned_start_offsets` (the same 33 inputs at offsets 0–7) and
> `hash_exact_size_buffer_no_overread` (exact-sized `malloc`, so any read past `wrdlen` is a
> heap overflow under ASan). All three also pass **against the old implementation**, which is
> what makes them a real equivalence check rather than a snapshot of the new behaviour.
>
> 🧹 The `CAUTION: add 8 more bytes` note and the `_PADr_KAZE` macro are both gone: callers no
> longer owe the function any over-allocation. GCC folds the `memcpy` back into a single load,
> so the generated code for the 8-byte case is unchanged — but no timing was taken, and none
> is claimed.

**Sanitizer evidence, both directions.** `test_hash` was built twice with
`clang -fsanitize=address,undefined`, once against the current header and once against the
header as it stood at `971d6169`:

| Build | Result |
|:---|:---|
| Current `hash.h` | **15 tests / 350 assertions, 0 failures, no sanitizer diagnostic.** |
| Previous `hash.h` | **Same 350 assertions pass** — the bit-identity claim — but the run emits `shift exponent 64 is too large`, `load of misaligned address` at all three load sites, and finally **aborts** on `AddressSanitizer: heap-buffer-overflow … READ of size 8` in `hash_exact_size_buffer_no_overread`. |

> That pair is the whole argument in one place: the output values did not move, and the
> undefined behaviour that produced them did.

#### 4.7.2 `gs_popMenu.h` — uninitialised save-state path

`_save_thread()` declared `char stateFilePath[4096]` and only filled it inside
`if (createSaveStatePath(...))`. The `exists()` call was guarded, but everything after the
`if` block was not, so when path construction failed the thread carried on using 4 KB of
uninitialised stack.

```c
// AFTER — no valid path means the save cannot be confirmed, so do not start it
if (!createSaveStatePath(game, slot, stateFilePath, sizeof(stateFilePath))) {
    g_save_thread_running = false;
    return NULL;
}
```

> 🐞 **What the old code did on that path:** it still called `retroarch_save(slot)`, then spent
> up to **30 s** — both `while` loops share the same `start` timestamp and the same 30,000 ms
> deadline — calling `exists()`, `file_isModified()` and `file_isLocked()` on that buffer at
> 100 ms intervals. Every one of those treats it as a NUL-terminated string, so the best case
> is a `stat()` on a path made of uninitialised stack bytes and the worst case, if no NUL
> happens to fall inside the 4 KB array, is a read past the end of it. The UI meanwhile waited
> on `g_save_thread_running` for a save that could never be confirmed.
> 🧪 `test_gs_popmenu` (24 tests / 155 assertions) and `test_savestate_path`
> (13 / 76) cover the path-construction contract this fix relies on.

#### 4.7.3 `file.c` — `const`-correct `file_basename`

```c
// BEFORE — const stripped twice: strrchr's result into char *, then a cast on the fallback
char *p = strrchr(filename, '/');
return p ? p + 1 : (char *)filename;
```

```c
// AFTER
const char *p = strrchr(filename, '/');
return p ? p + 1 : filename;
```

> The declaration in `file.h` already returned `const char *`, so no call site changes. The
> cast was invisible under `-Wall` and only appears with **`-Wcast-qual`**. 🛡️ Diagnostic
> hygiene: it removes the one place in this function where the compiler could no longer prove
> the input is not written to.

### 4.8 `infoPanel` hardening (commit [`bda89b2d`](https://github.com/Amiga500/Onion/commit/bda89b2d))

The port that unblocks `test_images_browser` (§5.4). 🛡️ robustness only — no performance
claim.

| File | 🐞 Fix |
|:-----|:----|
| `imagesBrowser.c` | Directory check was `S_ISDIR(ent->d_type & DT_DIR)` — meaningless; now `ent->d_type == DT_DIR`. |
| `imagesBrowser.c` | Image list growth was "found more images than allocated — TODO, just break"; now dynamic `realloc` with a `SIZE_MAX` overflow guard, per-entry malloc checks and full cleanup on failure. New `freeImagePaths()` helper. |
| `imagesBrowser.c` | `strcpy`/`sprintf`/`strncpy` → bounded `snprintf`/`strncpy` + explicit terminators; NULL guards on all `loadImagesPathsFromDir()` arguments. |
| `imagesCache.c` | `cleanImagesCache()` NULLs pointers after `SDL_FreeSurface` (double-free on repeat calls); `drawImageByIndex()` guards NULL `images_paths`/`cache_used`/`screen`. |
| `infoPanel.c` | `loadImagesPathsFromJson()`: NULL checks for `cJSON_Parse`/array items, malloc-failure cleanup, heap `file_dirname()` instead of unchecked stack `strncpy` + `dirname()`, compacting index replaces the buggy `(*count)--` decrement-while-indexing. |
| `infoPanel.c` | `main()`: `i + 1 < argc` bounds check before every `argv[++i]`; **`--romscreen` was unreachable** (its flag duplicated `-a`) — now correctly `-r`; `file_basename()` replaces raw `basename()`. |

### 4.9 Game Switcher review-pass fixes (commits fa888f22, d05267ca, ff012faa)

Four commits at the code tip (`74f0a0af`), found by a 2026-08-23 review of Game Switcher
and the OSD overlay path. Three are **pre-existing upstream defects** present at
`07505ea5`. One is a **port regression** from `45d4eec4`. The overlay-loop throttle
(`74f0a0af`) is documented in [§3.7](#37-power--cpu--osd-battery-display-db-overlay-commits-1116).
All four are 🛡️ / 📐 only — **no on-device measurement**.

#### `currentGame()` NULL dereference — [`fa888f22`](https://github.com/Amiga500/Onion/commit/fa888f22)

`currentGame()` returns `NULL` when `game_list_len == 0`. Three callers dereferenced
the result unchecked: the resume path in `gameSwitcher.c`, `_isSaveEnabled()`, and
`action_loadGame()`. Each now returns / disables the action when the list is empty.

#### Blocking `waitpid` on playActivity — [`d05267ca`](https://github.com/Amiga500/Onion/commit/d05267ca)

`45d4eec4` replaced `system("playActivity … &")` with `fork`+`execl`+`waitpid(child)`.
That waited for playActivity to finish on the UI thread. `_playActivityAsync()` now
double-forks: the intermediate child exits immediately so the parent's `waitpid` returns
at once, and the grandchild is reparented to init (no zombies). Async semantics match
the original `&` backgrounding.

#### Dead slot check + content-match OOB read — [`ff012faa`](https://github.com/Amiga500/Onion/commit/ff012faa)

`action_loadGame` rejected slots with `selected_slot < 0 && selected_slot >= slot_count`
— a conjunction that can never be true. It is now `||`. `_isContentNameInInfo` treated
a match at offset 0 as needing `*(found - 1) == ','`, which reads one byte before the
buffer; the string start is now a valid left boundary (`found == content_info || *(found - 1) == ','`).

---

## 🧪 5. Testing

A self-contained host test harness: `test/onion_test.h` (162 lines), `test/Makefile.unit`
(684 lines), `test/Makefile.gtest` (25 lines), `test/README.md` and
**68 test source files** (67 listed in `TESTS`).

| Metric | Value |
|:-------|------:|
| 🧪 Active suites | **68** |
| ✅ Tests | **1,410** |
| ✅ Assertions | **71,385** |
| ❌ Failures | **0** |
| 🎯 Result | **ALL PASSED** ✅ |
| ⏱️ Run only (prebuilt) | **~3.3 s** |

*Verified by an actual `make unit-test` run on this workspace (x86-64 host, exit code `0`,
2026-08-23).*

📈 Suite growth across the port:

| Stage | Active suites | Tests | Δ |
|:------|--------------:|------:|--:|
| Baseline `07505ea5` | 0 | 0 | — |
| After `ad402fa2` + `1a1e3f84` | 17 | 583 | **+17 suites** |
| After `300390a7` | 66 | 1,373 | **+288 % suites** |
| After `deb8b6ad` | 66 | 1,376 | **+3 hash tests** |
| After `c9e052d4` + `47fc5289` | 67 | 1,407 | **+1 suite** (`test_history_recent`), NEON oracles, CI sanitizer job |
| After `bda89b2d` (infoPanel port) | **68** | **1,410** | **+1 suite** (`test_images_browser` re-enabled, 3 tests / 22 assertions) |

`c9e052d4` adds `test_history_recent` (production `history_getRecentPath` contract — the
`continue` skip of non-game entries is **locked**, not reverted) and rewires several suites
to include production headers. `47fc5289` grows `test_neon` / `test_alpha_scale` with scalar
oracles. Host CI runs `make unit-test` plus `unit-test-san`; the `neon-arm` job is separate.

Commit `ad402fa2` imported a `Makefile.unit` whose `TESTS` list still named all 67 OniOpus46
suites while only 18 test sources had been copied, so `make unit-test` did not build at that
commit. `1a1e3f84` is the fix: it narrows `TESTS` to the 17 suites whose production code had
actually been hardened. `300390a7` then restores the list to **66** once the sources land.

### 5.1 🛡️ Security-focused suites

| Suite | Tests | Assertions |
|:------|------:|-----------:|
| `test_str_security` | 41 | 659 |
| `test_file_security` | 40 | 58 |
| `test_json_security` | 26 | 44 |
| `test_state_security` | 18 | 36 |
| `test_config_security` | 18 | 34 |
| `test_json_null_guards` | 18 | 35 |
| `test_null_safety` | 16 | 25 |
| `test_critical_fixes` | 16 | 26 |
| `test_cjson_null_safety` | 14 | 23 |
| `test_double_call_safety` | 12 | 19 |
| **Subtotal** | **219** | **959** |

> 📈 **16 %** of all tests are dedicated security/robustness regressions.

### 5.2 ⚡ Suites backing the perf claims

| Suite | Tests | Assertions | Backs |
|:------|------:|-----------:|:------|
| `test_alpha_scale` | 26 | 65,877 | §2.3 fixed-point alpha |
| `test_neon_pixel` | 38 | 72 | §2.1 scalar fallbacks + oracles |
| `test_neon` | 44 | 1,402 | §2.1 scalar fallbacks + oracles |
| `test_str` | 74 | 361 | §3.1 `str_count_char` |
| `test_file` | 89 | 184 | §3.2 · §3.3 · §3.4 · §3.5 |
| `test_file_security` | 40 | 58 | §3.3 · §3.4 · §3.5 |
| `test_list` | 156 | 265 | §3.6 list bounds + cache teardown |
| `test_perf` | 5 | 5 | §6 `perf.h` |
| `test_hash` | 15 | 350 | §4.7.1 hash bit-identity *(🛡️ not a perf claim)* |
| `test_history_recent` | 10 | 17 | production `history_getRecentPath` contract |

### 5.3 🏆 Largest suites

| Suite | Tests | Assertions |
|:------|------:|-----------:|
| `test_list` | 156 | 265 |
| `test_file` | 89 | 184 |
| `test_str` | 74 | 361 |
| `test_formatters` | 45 | 140 |
| `test_neon` | 44 | 1,402 |
| `test_str_security` | 41 | 659 |
| `test_file_security` | 40 | 58 |
| `test_neon_pixel` | 38 | 72 |
| `test_json` | 33 | 64 |
| `test_theme_config` | 27 | 145 |

### 5.4 ✅ Previously deferred suite — now enabled

`test_images_browser` was present in the tree with a build rule but excluded from `TESTS`
pending the `src/infoPanel/imagesBrowser.c` hardening. That hardening landed in
[`bda89b2d`](https://github.com/Amiga500/Onion/commit/bda89b2d) (see §4.8), the suite is now
in `TESTS`, and it passes: **3 tests / 22 assertions** (empty dir → NULL paths, filter+sort
with subdir/hidden/non-image rejection, NULL-argument rejection). All 68 `test_*.c` files
in the tree are now active.

---

## 🏗️ 6. Build & Tooling

**Root `Makefile`** — new `unit-test` target (added to `.PHONY`):

```make
unit-test:
	@cd $(ROOT_DIR)/test && $(MAKE) -f Makefile.unit all
```

Runs entirely on the host with the system compiler: **no** cross-toolchain, no SDL, no device.
That makes it usable as a fast pre-commit and CI gate. The pre-existing `test` target
(device/GTest oriented, requires `external-libs`) is untouched.

**`src/common/utils/perf.h`** (new, 92 lines) — opt-in lightweight profiling.
`PERF_START(label)` / `PERF_END(label)` compile to `do {} while (0)` unless `-DPERF_ENABLED`
is passed, so there is **zero cost** in release builds. Logs to
`/mnt/SDCARD/.tmp_update/logs/perf.log`. 🧪 `test_perf` (5 tests), compiled with `-DPERF_ENABLED`.

**`src/common/utils/msleep.h`** — signal-safety fix: `msleep_interrupt` is written from a
signal handler but was `static int`. Now `static volatile sig_atomic_t`, the only type the C
standard guarantees is safe from signal context. 🧪 `test_msleep` (5 tests).

**`src/common/config.mk`** — release builds now pass `-O2 -ffunction-sections -fdata-sections`
and `-Wl,--gc-sections`, matching OniOpus46, only when `DEBUG` is unset. These flags are
supported on the Miyoo GCC 8.3 toolchain. 📏 OniOpus46 reports **−5–15 %** binary size for
this same combination. `jpg2png` stays **out of `core`** (no libjpeg in the sysroot).

Host CI (`.github/workflows/test.yml`): `make unit-test`, a sanitizer subset
(`unit-test-san`), and a `neon-arm` job that runs the NEON tests under qemu. `jpg2png` is
not built in `make core`.

---

## 📈 7. Overall Statistics

| Metric | Value |
|:-------|------:|
| 🔧 **Commits** | **48** *(`git rev-list --count 07505ea5..HEAD`)* |
| 📁 **Files changed** | **140** *(138 excluding `docs/`)* |
| ➕ **Lines added / removed** | **+27,059 / −766** *(+25,538 / −766 excluding `docs/`)* |
| 🧩 **Production code (`src/`)** | **58 files · +2,252 / −750** |
| 🧪 **Test code (`test/`)** | **75 files · +23,233 / −10** |
| 📚 **Documentation (`docs/`)** | **2 files · +1,521** |
| 🆕 **New test source files** | **68** *(all 68 in `TESTS`)* |
| 🧪 **Active suites / tests / assertions** | **68 / 1,410 / 71,385** |
| ✅ **Test result** | **ALL PASSED** *(0 failures)* |
| ⏱️ **Suite runtime** | **~3.3 s** prebuilt |
| ⚡ **NEON kernels added** | **8** *(7 asm + 1 intrinsics, all with scalar fallback)* |
| ⚡ **Scalar pixel loops vectorised** | **11** across **6 files** |
| 🚀 **Max single-op speedup** | 📏 **+5000 %** *(NEON 180° rotation, measured on OniOpus46)* |
| 🚀 **`str_count_char`** | 📏 **−90 %** *(O(n²) → O(n))* |
| 🚀 **TTF/list/footer/header/dialog cache** | 📏 **5–15 ms/frame** *(OniOpus46, same code path)* |
| 🚀 **OSD bar busy-wait** | 📏 **idle CPU ~10 % → <1 %** *(OniOpus46)* |
| 🚀 **Brightness duplicate sysfs writes** | 📏 **−100 %** *(OniOpus46, same code path)* |
| 🚀 **`battery_isCharging` subprocess spawns** | 📐 **~−99 %** *(2 s cache, MIYOO354)* |
| 🚀 **playActivity DB open/close cycles** | 📏 **2 → 1** *(−50 % DB I/O, OniOpus46)* |
| 🚀 **GS overlay shell-outs** | **6 → 0** *(double-fork + `execl` / direct syscalls)* |
| 🚀 **OSD overlay draw loop** | 📐 **2 ms `msleep` / iter** *(was full-throttle; `74f0a0af`)* |
| 🚀 **Shell processes per `mkdirs`/`file_copy`** | 📐 **2 → 0** *(−100 %)* |
| 🚀 **Release `--gc-sections`** | 📏 **−5–15 %** binary size *(OniOpus46, same flags)* |
| 🛡️ **`sprintf` call sites** *(25-file set)* | **23 → 0** *(−100 %)* |
| 🛡️ **`strcpy`+`strcat` call sites** *(25-file set)* | **37 → 0** *(−100 %)* |
| 🛡️ **`strtok` call sites** | **4 → 0** *(−100 %)* |
| 🛡️ **`system()` call sites** *(25-file set)* | **3 → 1** *(−66.7 % — the 1 left is dead code, [§10](#️-10-known-residuals))* |
| 🛡️ **NULL-check predicates added** | **57** |
| 🛡️ **`fclose`/`close` added** | **18** |
| 🛡️ **Pre-existing upstream defects fixed** | **6** *(§4.7 trio + `currentGame()` NULL, dead slot `&&`, content-match OOB — [§4.9](#49-game-switcher-review-pass-fixes-commits-fa888f22-d05267ca-ff012faa))* |
| 🔐 **Hash values changed** | **0** *(bit-identical, 264 reference vectors at 5 optimization levels)* |
| 🏗️ **New build target** | **`make unit-test`** |
| 📉 **Failing tests at tip** | **0 / 1,410** |

Reproduce every figure above with:

```bash
git rev-list --count 07505ea5..HEAD               # 48
git diff --shortstat 07505ea5 HEAD                # 140 files, +27,059 / −766
git diff --shortstat 07505ea5 HEAD -- . ':!docs'  # 138 files, +25,538 / −766
git diff --shortstat 07505ea5 HEAD -- src/ test/ docs/ Makefile
make unit-test
```

---

## 🚫 8. Not Ported from OniOpus46

For honesty, these OniOpus46 optimizations are **absent** from OnionPlus today. Their
percentages appear **nowhere** in the tables above.

| OniOpus46 optimization | OniOpus46 claim | OnionPlus status |
|:---|:---|:---|
| Volume logarithmic curve | perceptual mapping | ❌ Not ported *(`test_volume` runs against unported reference logic)* — deliberately deferred: it changes the perceived UX of the volume keys, so it needs an explicit product decision, not just a port |
| `str_replace` `strlen` caching | 📏 −50 % scan | ⚠️ Not applicable — the OnionPlus rewrite is 🛡️ overflow hardening; OniOpus46 has no different scan algorithm to port |

Ported since the previous revision of this document (and therefore counted above, not here):
OSD busy-wait fix + thread hardening (`4b851203`), display brightness sysfs caching +
display hardening (`e8143d09`), battery charging cache + batmon fixes (`0121f943`), SQLite
open/close 2 → 1 + DB hardening (`0d1ce423`), `config.h` `mkdirs` hardening (`2c5b028a`),
GS overlay `system()` → fork+exec (`45d4eec4`), `infoPanel`/`imagesBrowser` hardening with
`test_images_browser` re-enabled (`bda89b2d`). Earlier: TTF/list/footer/header/dialog
caches, `--gc-sections` in `config.mk`, signal-handler call sites in six apps,
`file_remove_recursive` in `reset.h` (`eb3f0aec`).

---

## 🔬 9. Methodology & Limits

### ✅ What is measured *here*

| Figure | Method |
|:---|:---|
| Line / file counts | `git diff --stat 07505ea5..HEAD`, working tree included. **Exact.** |
| Call-site counts | Pattern occurrences in the 25 ported `src/` files at `07505ea5` vs `HEAD`. Both endpoints stated so the delta is checkable. Scoped to ported files only. |
| Test results | A real `make unit-test` run on this workspace: 68 suites, 1,410 tests, 71,385 assertions, 0 failures, exit `0`. |
| Suite runtime | `time make unit-test` — ~3.3 s with binaries prebuilt. Host is x86-64. |
| Throughput (`px/iter`) | Read off `count & ~15` / `count & ~7` in `neon_pixel.h`. Exact property of the code. |
| Complexity classes (O(n²) → O(n)) | Read off the rewritten loops. Exact. |
| Scalar-fallback correctness | Unit tests against hand-computed expected values. |
| Hash bit-identity | 264 reference vectors (33 strings × 8 start offsets) replayed against both the old and the new implementation, at `-O0` / `-O1` / `-O2` / `-O3` / `-Os`. **Exact match.** |
| Hash undefined behaviour | `test_hash` built with `clang -fsanitize=address,undefined` against both revisions of the header. See [§4.7.1](#471-hashh--bounded-alignment-safe-64-bit-load). |

### 📏 What is inherited, not measured here

Every **📏** figure — the NEON speedup column in [§2.1](#21-kernel-table), the +5000 % rotation,
the −90 % `str_count_char`, the −50 % scan reductions — is an **OniOpus46 benchmark result**
published in its [`OPTIMIZATION.md`](https://github.com/Amiga500/Onion/blob/OniOpus46/docs/OPTIMIZATION.md),
quoted here because OnionPlus ships **the same code for the same operation**. They are
reproduced for context. They are **not** OnionPlus measurements, and the port has not
independently confirmed them.

### ❌ What is NOT measured at all

- ❌ **No OnionPlus on-device benchmark.** Nothing was timed on a Miyoo Mini (283) or Mini+
  (354). No frame-rate, latency, CPU-utilisation, battery or throughput figure was collected
  as part of this port.
- ❌ **NEON assembly is not exercised by the host `make unit-test` run.** The host is x86-64, so
  `__ARM_NEON` is undefined and the scalar fallbacks are what execute. The separate `neon-arm`
  CI job cross-compiles with `-mfpu=neon` and runs under qemu-user when those packages exist.
- ❌ **No runtime testing on target hardware.** The code compiles and passes host tests;
  end-to-end device behaviour has not been validated.
- ⚠️ **Sanitizer coverage is partial.** ASan + UBSan run in CI via `make -C test -f Makefile.unit unit-test-san`
  (a subset of the suite). The default `make unit-test` build carries **no** sanitizer flags.
  No Valgrind or static-analysis tool (coverity, clang-tidy, cppcheck) has been run over the
  branch as part of this port.
- ❌ **Bug counts are not claimed.** This report counts *code changes* (call sites, guards,
  closed descriptors), not "bugs fixed" — mapping a hardening edit to a user-visible bug
  requires reproduction evidence that was not collected.

### ⚠️ Counting caveats

- Pattern counts are **textual**. `sprintf` counts exclude `snprintf`/`vsnprintf` via a
  preceding-character exclusion; `strcpy`/`strcat` exclude `strncpy`/`strncat` the same way.
  Occurrences inside comments or strings are included.
- "NULL-check predicates added" counts added lines matching `== NULL`, `!= NULL` or `if (!`.
  It is a **lower-bound proxy**, not an exact count of distinct guarded conditions.
- `+/−` totals for a squashed range can differ from the sum of per-commit stats when later
  commits modify lines introduced by earlier ones.
- **Expect a "23 vs 48" discrepancy against older revisions of this document.** The
  headline is `git rev-list --count 07505ea5..HEAD` (**48** at the code tip `74f0a0af`).
  Milestone tables group PR #206/#207 and omit docs-only refreshes, format passes, and
  the net-zero reverted experiment.
- **These two `docs/` files are inside the range they measure.** The **code-only** subset
  (everything except `docs/`) is stated next to each aggregate.

---

## ⚠️ 10. Known Residuals

For transparency, these items **remain** at the working tree. They are outside the scope
of this port, not oversights the numbers above conceal.

| Item | Pattern | Note |
|:-----|:--------|:-----|
| `src/common/utils/process.h:101` | `system(cmd)` | `process_start()` still shells out — but it is **dead code**: no caller exists in `src/`, and none of the four shipped MainUI binaries (`MainUI-283/354-clean/expert`) contain its format strings (`cd "%s"; %s %s %s`, `%s/bin/%s`) or `process_searchpid`'s `/proc/%d/comm`. Verified 2026-08-23. Left in place as a documented residual; removal or a `fork`/`execv` rewrite are equally safe. |
| `src/jpg2png` vs `make core` | build | `jpg2png` stays **out of `core`**: Miyoo sysroot has no libjpeg. Makefile sibling exists; `make -C src/jpg2png` is opt-in. |
| Host `make unit-test` | SIMD | Scalar fallbacks only on x86-64. The `neon-arm` CI job runs the assembly under qemu. |
| On-device timings | methodology | Nothing was timed on a Miyoo Mini / Mini+ as part of this port. Every 📏 is inherited. |
| Volume logarithmic curve | not ported | UX-affecting change, deferred pending explicit decision — see [§8](#-8-not-ported-from-oniopus46). |
| `batmon` / `keymon` signals | custom handlers | Extra signals (SIGUSR1, SIGSTOP, …) — not the shared SIGINT/SIGTERM helper. |
| `chargingState.c` `strcpy` | out of 25-file set | Only the signal handler was migrated in that file. |

✅ **Closed this revision:** `overlay_surface()` draw-loop busy-wait — the overlay
thread spun full-throttle for its entire duration. [`74f0a0af`](https://github.com/Amiga500/Onion/commit/74f0a0af)
throttles it with `msleep(2)` per iteration and demotes the draw-count/speed stats to
`printf_debug`. The volume/brightness **bar** thread was already at `usleep(16000)`
(`4b851203`); this residual was the other OSD loop.

🎯 Recommended follow-ups, in priority order:

1. 🔵 **Take baseline timings on device** for `jpg2png`, `pngScale`, `rotate180`, screenshot capture, list scrolling **and the new power paths** (OSD bar CPU, brightness writes, `battery_isCharging` forks), so every 📏/📐 in this document can be upgraded to a real OnionPlus measurement.
2. 🟢 Decide on the volume logarithmic curve (UX decision, then port).
3. 🟢 Remove or rewrite the dead `process_start()` (see table above — zero risk either way).

---

## ✅ Final Status

OnionPlus is **48 commits** ahead of upstream `OnionUI/Onion:main`
(`07505ea5` → `74f0a0af`, `git rev-list --count`), adding **8 NEON pixel kernels**,
crash/memory hardening of the `src/common` layer, TTF/list/footer/header/dialog surface
caches, `--gc-sections` release flags, four algorithmic wins in `str`/`file`, production
`file_remove_recursive` call sites, migrated SIGINT/SIGTERM handlers, fixes for **6
pre-existing upstream defects** (§4.7 + §4.9), the **power/CPU batch** (OSD bar busy-wait,
brightness caching, battery-charging cache, batmon fixes, SQLite open/close 2 → 1, overlay
double-fork+exec), the `infoPanel` hardening, a throttled `overlay_surface()` draw loop,
and a **68-suite host unit-test harness** runnable with a single `make unit-test`.

> 🧪 **68 suites · 1,410 tests · 71,385 assertions · 0 failures.** ✅
>
> Note: the baseline `07505ea5` had **no** host test suite, so this is a new quality floor
> rather than a "no regressions" comparison — there is nothing to compare against upstream.

The hardening of the original 25-file set is now **complete for `sprintf`/`strcpy`/`strcat`/
`strtok`**. The remaining `system()` in `process.h` is enumerated in
[§10](#️-10-known-residuals) rather than rounded away. Performance figures marked **📏** are
**OniOpus46 measurements on identical code**, reproduced here for context and clearly
separated from **📐 analytical** figures. **No OnionPlus on-device measurement has been taken** —
treat every speedup as inherited evidence until benchmarked on real hardware.

The defect fixes in [§4.7](#47-pre-existing-defects-fixed-in-this-branch) and
[§4.9](#49-game-switcher-review-pass-fixes-commits-fa888f22-d05267ca-ff012faa) carry **no
performance claim at all**. The `hash.h` rewrite in particular is valuable precisely because it
changes **nothing** observable: same hash values, same generated load on the fast path, minus a
7-byte over-read, minus three classes of undefined behaviour, and minus an allocation
convention that callers had to remember on their own.

---

📊 See also: **[OnionPlus-vs-base.md](./OnionPlus-vs-base.md)** — full diff statistics vs. the base release.

<sub>Repository: [Amiga500/Onion](https://github.com/Amiga500/Onion) · Branch: `OnionPlus` ·
Base: [`07505ea5`](https://github.com/Amiga500/Onion/commit/07505ea5) → Code tip: [`74f0a0af`](https://github.com/Amiga500/Onion/commit/74f0a0af) (**48** commits, `git rev-list --count`) ·
Style adapted from the OniOpus46 [`OPTIMIZATION.md`](https://github.com/Amiga500/Onion/blob/OniOpus46/docs/OPTIMIZATION.md) ·
Commits analyzed: **48** · Date: 2026-08-23</sub>
