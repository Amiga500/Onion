# 📐 OnionPlus vs. base release — Diff Statistics

> **48 commits** · **140 files** · **+27,059 / −766 lines** · **79 added** / **61 modified** · **0 deleted** · **68 test suites** · **1,410 tests** ✅

> **What this document is:** the raw, reproducible *diff arithmetic* between the OnionPlus
> branch tip and the upstream base release. Every number here comes from `git` on this
> workspace.
> **What it is not:** a narrative of what the changes do — for optimizations, hardening and
> performance figures see **[ONIONPLUS_OPTIMIZATION.md](./ONIONPLUS_OPTIMIZATION.md)**.

| 🔖 Reference | Value |
|:---|:---|
| 🌿 Branch tip | `OnionPlus` → code tip [`74f0a0af`](https://github.com/Amiga500/Onion/commit/74f0a0af) *(four review-pass fixes, 2026-08-23)* on top of CI fix `2ae2e79f`, version bump `6b7f6357`, merge `e44421e1` (PR #206–207) |
| 🏁 Base / merge-base | [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) — `OnionUI/Onion:main` *(2026-01-21, Aemiii91)* |
| ⏩ Commits ahead | **48** *(`git rev-list --count 07505ea5..HEAD`; authored 2026-08-20–23)* |
| 📦 Aggregate delta | **140 files** · **+27,059** / **−766** |
| 🧩 Code-only delta *(excl. `docs/`)* | **138 files** · **+25,538** / **−766** |
| 🧪 Unit tests at tip | **68 suites** · **1,410 tests** · **71,385 assertions** · **0 failures** ✅ |
| 🔀 Net line growth | **+26,293** |

> 🔁 **Self-reference.** The two files in `docs/` are part of the range they measure, so every
> *aggregate* figure below includes them. Wherever that matters, the **code-only** subset
> (everything except `docs/`) is given alongside it. Each table states which of the two it uses.
> A previous revision advertised **23** commits and tip `6b7f6357` / `2ae2e79f`; those were
> stale. The headline is `git rev-list --count`.

---

## 📋 Table of Contents

1. [Headline Ratios](#-1-headline-ratios)
2. [The 48 Commits](#-2-the-48-commits)
3. [Breakdown by Directory](#️-3-breakdown-by-directory)
4. [Breakdown by Functional Category](#️-4-breakdown-by-functional-category)
5. [Key Files](#-5-key-files)
6. [Test Suite Verification](#-6-test-suite-verification)
7. [Reproduction Commands](#-7-reproduction-commands)

---

## 📊 1. Headline Ratios

*Shares are of the **aggregate** 27,059 insertions, `docs/` included (this refresh).*

| Metric | Value | Share |
|:---|---:|---:|
| 🧪 Insertions that are **test code** | **23,233** | **85.9 %** |
| 🧩 Insertions that are **production code** (`src/`) | **2,252** | **8.3 %** |
| 📚 Insertions that are **documentation** | **1,521** | **5.6 %** |
| 🏗️ Insertions that are **build/CI wiring** | **53** | **0.2 %** |
| ➕ Files **added** (`A`) | **79** | **56.4 %** |
| ✏️ Files **modified** (`M`) | **61** | **43.6 %** |
| 🗑️ Files **deleted** (`D`) | **0** | **0 %** |
| 🔁 Insertions per deletion | **≈ 35 : 1** | — |
| 🧪 Test lines per production line | **≈ 10 : 1** | — |

> 📈 **Read this as:** OnionPlus is still a **test-heavy, low-blast-radius** port. Roughly
> **10 lines of test** landed for every **1 line of production code**. Nothing was deleted
> outright — the 766 removed lines are in-place rewrites inside modified files.

---

## 🔀 2. The 48 Commits

*Ordered oldest → tip (`git log --reverse --oneline 07505ea5..HEAD`). File/+− columns are
that commit's own `git show --shortstat`, not the running aggregate. Merge commits have
no tree delta of their own.*

| # | Hash | Subject | Files | +/− | Category |
|:-:|:-----|:--------|------:|----:|:---------|
| 1 | [`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1) | OnionPlus: NEON pixel conversions ported from OniOpus46 | 7 | +483 / −80 | ⚡ NEON / perf |
| 2 | [`6da7f28b`](https://github.com/Amiga500/Onion/commit/6da7f28b) | 🎨 apply clang-format changes | 2 | +2 / −2 | 🎨 Formatting *(CI-generated)* |
| 3 | [`ad402fa2`](https://github.com/Amiga500/Onion/commit/ad402fa2) | OnionPlus: port common crash/memory hardening from OniOpus46 | 33 | +9,724 / −193 | 🛡️ Hardening + 🧪 test |
| 4 | [`1a1e3f84`](https://github.com/Amiga500/Onion/commit/1a1e3f84) | Limit unit-test suite list to ported hardening tests | 1 | +2 / −2 | 🧪 Test fix |
| 5 | [`300390a7`](https://github.com/Amiga500/Onion/commit/300390a7) | OnionPlus: expand host unit-test suite from OniOpus46 | 58 | +13,894 / −18 | 🧪 Test + build |
| 6 | [`971d6169`](https://github.com/Amiga500/Onion/commit/971d6169) | Add OnionPlus optimization and diff-stat documentation | 2 | +683 / −0 | 📚 Docs |
| 7 | [`deb8b6ad`](https://github.com/Amiga500/Onion/commit/deb8b6ad) | Fix pre-existing hash, save-state and `const` defects; refresh docs | 6 | +1,171 / −476 | 🛡️ Fix + 🧪 test + 📚 docs |
| 8 | [`c9e052d4`](https://github.com/Amiga500/Onion/commit/c9e052d4) | Harden host unit-test CI with sanitizers and production contracts | 17 | +527 / −646 | 🧪 Test + CI |
| 9 | [`47fc5289`](https://github.com/Amiga500/Onion/commit/47fc5289) | Unify NEON ifdefs and keep jpg2png out of core | 11 | +487 / −103 | ⚡ NEON + 🏗️ build |
| 10 | [`eb3f0aec`](https://github.com/Amiga500/Onion/commit/eb3f0aec) | Port TTF label cache, shared signal handlers, and leftover hardening | 20 | +633 / −529 | ⚡ + 🛡️ |
| 11 | [`198c74af`](https://github.com/Amiga500/Onion/commit/198c74af) | fix: restore O_CREAT in file_isLocked() wait semantics | 2 | +13 / −7 | 🛡️ Critical fix |
| 12 | [`7f4c3827`](https://github.com/Amiga500/Onion/commit/7f4c3827) | test: report [FAIL] instead of [ OK ] for failed tests in RUN_TEST | 1 | +5 / −1 | 🧪 Test fix |
| 13 | [`b77d5163`](https://github.com/Amiga500/Onion/commit/b77d5163) | fix: add theme_renderDialog_cleanup() for cached dialog surfaces | 1 | +8 / −0 | 🛡️ Critical fix |
| 14 | [`866c13fc`](https://github.com/Amiga500/Onion/commit/866c13fc) | 🎨 apply clang-format changes | 1 | +12 / −3 | 🎨 Formatting |
| 15 | [`333f8927`](https://github.com/Amiga500/Onion/commit/333f8927) | fix: include theme color in MULTIVALUE label cache key | 2 | +9 / −2 | 🛡️ Medium fix |
| 16 | [`83933877`](https://github.com/Amiga500/Onion/commit/83933877) | fix: guard screenshot_save against zero display dimensions | 1 | +7 / −3 | 🛡️ Medium fix |
| 17 | [`713a3eca`](https://github.com/Amiga500/Onion/commit/713a3eca) | fix: require complete 3-byte UTF-8 sequence in includeCJK | 1 | +6 / −2 | 🛡️ Medium fix |
| 18 | [`534c4268`](https://github.com/Amiga500/Onion/commit/534c4268) | fix: report partial failures in file_remove_recursive, tolerate ENOENT | 1 | +7 / −3 | 🛡️ Medium fix |
| 19 | [`80b03838`](https://github.com/Amiga500/Onion/commit/80b03838) | 🎨 apply clang-format changes | 2 | +17 / −12 | 🎨 Formatting |
| 20 | [`f9bac9fc`](https://github.com/Amiga500/Onion/commit/f9bac9fc) | Merge pull request #206 | — | — | 🔀 Merge |
| 21 | [`55de00a9`](https://github.com/Amiga500/Onion/commit/55de00a9) | Merge pull request #207 | — | — | 🔀 Merge |
| 22 | [`00eedbde`](https://github.com/Amiga500/Onion/commit/00eedbde) | Update OnionPlus docs: integrate PR#206-207 | 2 | +47 / −36 | 📚 Docs |
| 23 | [`bef883c2`](https://github.com/Amiga500/Onion/commit/bef883c2) | build: update version date to 2026-08-22 | 1 | +1 / −1 | 🏗️ Release |
| 24 | [`2537c94d`](https://github.com/Amiga500/Onion/commit/2537c94d) | fix: do not use SDL_Color.a on Miyoo toolchain | 1 | +4 / −4 | 🏗️ Build fix |
| 25 | [`eea25f88`](https://github.com/Amiga500/Onion/commit/eea25f88) | fix: replace system() with fork+execv in process_start | 1 | +102 / −18 | ⚡ *(reverted)* |
| 26 | [`10ec2387`](https://github.com/Amiga500/Onion/commit/10ec2387) | perf: reduce OSD bar busy-wait and memory usage | 1 | +13 / −15 | ⚡ *(reverted)* |
| 27 | [`840e2f2a`](https://github.com/Amiga500/Onion/commit/840e2f2a) | hardening: add string.h and clamp meterWidth in OSD | 1 | +4 / −0 | 🛡️ *(reverted)* |
| 28 | [`1e359571`](https://github.com/Amiga500/Onion/commit/1e359571) | perf+hardening: cache meterWidth and harden process_start_read_return | 2 | +15 / −7 | ⚡ *(reverted)* |
| 29 | [`4f7841e0`](https://github.com/Amiga500/Onion/commit/4f7841e0) | Revert the four commits above | 2 | +35 / −129 | ↩️ Revert *(net zero)* |
| 30 | [`5992c8b2`](https://github.com/Amiga500/Onion/commit/5992c8b2) | docs: refresh OnionPlus stats to tip 4f7841e0 (post-revert) | 2 | +80 / −59 | 📚 Docs |
| 31 | [`4b851203`](https://github.com/Amiga500/Onion/commit/4b851203) | Port OSD busy-wait fix and thread hardening from OniOpus46 | 1 | +51 / −30 | ⚡ Perf |
| 32 | [`e8143d09`](https://github.com/Amiga500/Onion/commit/e8143d09) | Port brightness sysfs caching and display hardening from OniOpus46 | 1 | +40 / −13 | ⚡ Perf |
| 33 | [`0121f943`](https://github.com/Amiga500/Onion/commit/0121f943) | Port battery charging cache and batmon fixes from OniOpus46 | 3 | +96 / −30 | ⚡ Perf + 🛡️ fix |
| 34 | [`0d1ce423`](https://github.com/Amiga500/Onion/commit/0d1ce423) | Port SQLite open/close optimization and DB hardening from OniOpus46 | 6 | +250 / −81 | ⚡ Perf + 🛡️ hardening |
| 35 | [`2c5b028a`](https://github.com/Amiga500/Onion/commit/2c5b028a) | Harden config.h: direct mkdirs and bounded copies | 1 | +4 / −7 | 🛡️ Hardening |
| 36 | [`45d4eec4`](https://github.com/Amiga500/Onion/commit/45d4eec4) | Replace GameSwitcher overlay shell-outs with fork+exec and syscalls | 1 | +26 / −6 | ⚡ Perf |
| 37 | [`bda89b2d`](https://github.com/Amiga500/Onion/commit/bda89b2d) | Port infoPanel hardening and enable test_images_browser | 4 | +163 / −54 | 🛡️ Hardening + 🧪 test |
| 38 | [`98708a98`](https://github.com/Amiga500/Onion/commit/98708a98) | Refresh OnionPlus docs for the power/CPU port batch | 2 | +247 / −125 | 📚 Docs |
| 39 | [`e44421e1`](https://github.com/Amiga500/Onion/commit/e44421e1) | Merge remote OnionPlus (PR #206–207) into local power/CPU port batch | — | — | 🔀 Reconciliation |
| 40 | [`927685e8`](https://github.com/Amiga500/Onion/commit/927685e8) | fix: drop meterWidth config cache in OSD bar | 1 | +3 / −7 | 🛡️ Fix |
| 41 | [`55998284`](https://github.com/Amiga500/Onion/commit/55998284) | docs: refresh stats post-merge with remote OnionPlus (PR #206-207) | 2 | +59 / −55 | 📚 Docs |
| 42 | [`6b7f6357`](https://github.com/Amiga500/Onion/commit/6b7f6357) | build: update version date to 2026-08-23 | 1 | +1 / −1 | 🏗️ Release |
| 43 | [`03080200`](https://github.com/Amiga500/Onion/commit/03080200) | docs: updated with final 3 commits (merge, meterWidth, version) | 2 | +50 / −44 | 📚 Docs |
| 44 | [`2ae2e79f`](https://github.com/Amiga500/Onion/commit/2ae2e79f) | ci: fix pre-release workflow to use HEAD SHA instead of origin/main | 1 | +1 / −1 | 🏗️ CI fix |
| 45 | [`fa888f22`](https://github.com/Amiga500/Onion/commit/fa888f22) | fix: guard currentGame() NULL returns at all call sites | 2 | +16 / −6 | 🛡️ Fix |
| 46 | [`d05267ca`](https://github.com/Amiga500/Onion/commit/d05267ca) | fix: restore async semantics for playActivity fork+exec | 1 | +24 / −20 | 🛡️ Fix |
| 47 | [`ff012faa`](https://github.com/Amiga500/Onion/commit/ff012faa) | fix: correct dead-code slot check and OOB read in content match | 2 | +4 / −2 | 🛡️ Fix |
| 48 | [`74f0a0af`](https://github.com/Amiga500/Onion/commit/74f0a0af) | perf: throttle OSD overlay draw loop and demote stats logging | 1 | +4 / −4 | ⚡ Perf |
| | | **Aggregate `07505ea5` → this docs refresh** | **140** | **+27,059 / −766** | |

> ℹ️ A previous revision of this table had **23 rows** and listed the meterWidth drop as
> `6b7f6357`. That SHA is the **version bump** (row 42). The meterWidth drop is
> [`927685e8`](https://github.com/Amiga500/Onion/commit/927685e8) (row 40).
> Rows 25–29 are a remote experiment that was fully reverted — net zero in the tree.
> The four commits at the tip (45–48) are the 2026-08-23 review pass.

### 📝 Notes per later commit

| Commit | What landed |
|:---|:---|
| ⚡ `d7aed5a1` | New `neon_pixel.h` with 7 assembly kernels, plus NEON wiring in `rotate180.h`, `surfaceSetAlpha.h`, `IMG_Save.h`, `screenshot.h`, `jpg2png.c`, `pngScale.c`. |
| 🛡️ `ad402fa2` | Common-layer hardening and the first host suites. Introduces `signal_handler.h`, hardens `file.c` / `str.c` / `json.h`, adds `test/onion_test.h` and `test/Makefile.unit`. |
| 🧪 `300390a7` | Ports the remaining host suites; adds `perf.h` and root `Makefile` / `test/Makefile*` wiring. |
| 🛡️ `deb8b6ad` | Three **pre-existing** defects — `FNV1A_Pippip_Yurii` 8-byte over-read and unaligned loads, uninitialised `stateFilePath`, `const`-discarding `file_basename` — plus hash regression tests. |
| ⚡ `eb3f0aec` | OniOpus46 TTF caches (`list`/`footer`/`header`/`dialog`), signal-handler call sites, `reset.h` `file_remove_recursive`, bounded screenshot/jpg2png paths, `config.mk` `--gc-sections`. |
| 🛡️ PR #206 | `file_isLocked` O_CREAT restored, `RUN_TEST` prints `[FAIL]`, `theme_renderDialog_cleanup()`. |
| 🛡️ PR #207 | MULTIVALUE cache keyed on color+value, screenshot VLA guard, complete UTF-8 in `includeCJK`, `file_remove_recursive` errors + fsync. |
| ⚡ `4b851203` | `osd.h`: bar thread poll 100 µs → 16 ms, `volatile` thread state, 160× smaller bar save buffer with `memcpy` rows, `yres` division guards, 2 overlay leak fixes. `meterWidth` is **not** cached (dropped later in `927685e8`). |
| ⚡ `e8143d09` | `display.h`: brightness duty-cycle cache with PWM re-export invalidation, `log()` guard, `memcpy` fast path in `display_readOrWriteBuffer`. |
| ⚡ `0121f943` | `battery.h` + `batmon`: 2 s `battery_isCharging()` cache, `warnAt` at check timeout, 500 ms low-battery thread, `popen` for `getBatPercMMP`. |
| ⚡ `0d1ce423` | `playActivity`/`playActivityUI`: single open/exec/close per DB operation + stmt/migrate/leak hardening. |
| 🛡️ `2c5b028a` | `config.h`: `system("mkdir -p")` → hardened `mkdirs()`, bounded `dir_path` copy. |
| ⚡ `45d4eec4` | `gs_overlay.h`: `playActivity` via `fork`+`execl`; RetroArch `killall`/`pidof` → process helpers. Blocking `waitpid` later fixed in `d05267ca`. |
| 🛡️ `bda89b2d` | `infoPanel` hardening. Re-enables `test_images_browser` (67 → 68 suites). |
| 🛡️ `927685e8` | Drops the `meterWidth` config cache (no theme-change invalidation path). |
| 🏗️ `2ae2e79f` | Pre-release workflow tags the built `HEAD`, not `origin/main`. |
| 🛡️ `fa888f22` | NULL-guard `currentGame()` at three call sites (empty `game_list`). |
| 🛡️ `d05267ca` | Double-fork playActivity helper — async again, no zombies. |
| 🛡️ `ff012faa` | Slot check `&&` → `||`; content-match no longer reads before the string. |
| ⚡ `74f0a0af` | `overlay_surface()` draw loop: `msleep(2)` per iter; stats → `printf_debug`. |

---

## 🗂️ 3. Breakdown by Directory

*Aggregate range `07505ea5` → working tree, `docs/` included.*

| 📁 Area | Files | ➕ Insertions | ➖ Deletions | Share of + |
|:---|---:|---:|---:|---:|
| 🧪 `test/` *(incl. `test/Makefile*`)* | **75** | **+23,233** | **−10** | 85.9 % |
| 🧩 `src/` | **58** | **+2,252** | **−750** | 8.3 % |
| 📚 `docs/` | **2** | **+1,521** | **0** | 5.6 % |
| 🏗️ `Makefile` + `.github/` + `.gitignore` | **5** | **+53** | **−6** | 0.2 % |
| | **140** | **+27,059** | **−766** | 100 % |

### 🧪 Inside `test/`

| Item | Files | +/− |
|:---|---:|---:|
| `test_*.c` suites *(all new)* | **68** | *(included in test/ total)* |
| `Makefile.unit` *(new)* | 1 | +684 / −0 |
| `onion_test.h` — `TEST` / `RUN_TEST` framework *(new)* | 1 | +162 / −0 |
| **Total `test/`** | **75** | **+23,233 / −10** |

### 🧩 Inside `src/`

**54 modified**, **4 added** (`neon_pixel.h`, `perf.h`, `signal_handler.h`, `gs_savestate_path.h`), **0 deleted**.

The extra files beyond the original 25-file NEON/hardening set are theme-render caches,
signal-handler call sites, `reset.h`, `config.mk`, `jpg2png/Makefile`,
`gs_savestate_path.h`, the 2026-08-23 power/hardening batch (`osd.h`, `display.h`,
`battery.h`, `batmon.c/h`, `config.h`, `gs_overlay.h`, `playActivity/*`,
`playActivityUI/playActivityUI.c`, `infoPanel/*`), and the review-pass touch-ups in
`gameSwitcher.c` / `gs_popMenu.h` / `gs_overlay.h` / `osd.h`.

---

## 🏷️ 4. Breakdown by Functional Category

Categories below are approximate file-level labels for the same 140 files. Prefer the
directory table in [§3](#️-3-breakdown-by-directory) when checking `git diff --stat`.

| Category | Role |
|:---|:---|
| 🧪 Unit test suites + harness | 75 files under `test/` |
| 📚 Documentation | 2 files under `docs/` |
| ⚡ NEON / graphics | `neon_pixel.h`, `surfaceSetAlpha.h`, `rotate180.h`, `IMG_Save.h`, `screenshot.h`, `pngScale.c`, `jpg2png.c` |
| 🛡️ Hardening & correctness | `file.c`, `str.c`, `state.h`, `list.h`, `hash.h`, `gs_popMenu.h`, `reset.h`, `infoPanel/*`, `gameSwitcher.c`, … |
| 🔋 Power / CPU | `osd.h` (bar busy-wait + overlay-loop throttle), `display.h` (brightness cache), `battery.h` + `batmon/*` (charging cache), `gs_overlay.h` (double-fork+exec) |
| 💾 Database | `playActivity/*` (open/close 2 → 1 + hardening), `playActivityUI/playActivityUI.c` |
| 🎨 TTF / list caches | `theme/render/{list,footer,header,dialog}.h` |
| 🔧 Shared infra | `perf.h`, `signal_handler.h`, `config.mk` `--gc-sections`, `config.h` |
| 🏗️ Makefile / CI | root `Makefile`, `.github/workflows/*` (incl. pre-release `HEAD` SHA), `jpg2png/Makefile` |

### ⚡ NEON / graphics detail *(from `git diff --numstat`)*

| File | Status | +/− |
|:---|:---:|---:|
| `src/common/utils/neon_pixel.h` | 🆕 A | +343 / −0 |
| `src/common/system/screenshot.h` | ✏️ M | +71 / −32 |
| `src/pngScale/pngScale.c` | ✏️ M | *(in src/ total)* |
| `src/jpg2png/jpg2png.c` | ✏️ M | +20 / −17 |
| `src/common/utils/rotate180.h` | ✏️ M | *(in src/ total)* |
| `src/common/utils/IMG_Save.h` | ✏️ M | *(in src/ total)* |
| `src/common/utils/surfaceSetAlpha.h` | ✏️ M | *(in src/ total)* |

### 🎨 TTF cache detail

| File | Status | +/− |
|:---|:---:|---:|
| `src/common/theme/render/list.h` | ✏️ M | +116 / −39 |
| `src/common/theme/render/footer.h` | ✏️ M | +70 / −35 |
| `src/common/theme/render/dialog.h` | ✏️ M | +45 / −16 |
| `src/common/theme/render/header.h` | ✏️ M | +26 / −6 |

---

## 🔑 5. Key Files

| File | Status | Delta | Role |
|:---|:---:|---:|:---|
| 🧪 `test/test_list.c` | 🆕 A | +2,100 | Largest single suite — 156 tests |
| 🧪 `test/test_file.c` | 🆕 A | +1,161 | File I/O, paths, `mkdirs`, `file_copy` — 89 tests |
| 🏗️ `test/Makefile.unit` | 🆕 A | +684 | Build + run + summary for 68 suites |
| 🧪 `test/test_neon.c` | 🆕 A | +608 | Scalar NEON fallbacks + oracles — 44 tests / 1,402 assertions |
| ⚡ `src/common/utils/neon_pixel.h` | 🆕 A | +343 | 7 ARM NEON pixel kernels + scalar fallbacks |
| 🧪 `test/test_hash.c` | 🆕 A | +281 | Hash regression vectors — 15 tests / 350 assertions |
| 🧪 `test/test_history_recent.c` | 🆕 A | +226 | Production `history_getRecentPath` contract — 10 tests |
| 🛡️ `src/common/utils/file.c` | ✏️ M | +210 / −70 | Path/IO hardening, `system()` removal, `file_remove_recursive` |
| 🎨 `src/common/theme/render/list.h` | ✏️ M | +116 / −39 | TTF/preview cache populate path |
| 🛡️ `src/common/system/screenshot.h` | ✏️ M | +71 / −32 | NEON convert + bounded `snprintf` path |
| 🔧 `src/gameSwitcher/gs_savestate_path.h` | 🆕 A | +45 / −0 | Save-state path helper extracted for tests |
| 🛡️ `src/common/utils/hash.h` | ✏️ M | +20 / −10 | Bounded, alignment-safe 64-bit load; hashes bit-identical |
| 🏗️ `src/common/config.mk` | ✏️ M | +7 / −0 | `-O2 -ffunction-sections -Wl,--gc-sections` |
| ⚡ `src/playActivity/playActivityDB.h` | ✏️ M | +143 / −42 | SQLite open/close 2 → 1 + stmt guards |
| 🛡️ `src/infoPanel/infoPanel.c` | ✏️ M | +80 / −34 | JSON/argv hardening, `-r` flag fix |
| ⚡ `src/common/system/battery.h` | ✏️ M | +55 / −10 | 2 s `battery_isCharging()` cache |
| ⚡ `src/common/system/osd.h` | ✏️ M | +50 / −33 | Bar busy-wait 100 µs → 16 ms + overlay-loop `msleep(2)` |
| ⚡ `src/common/system/display.h` | ✏️ M | +40 / −13 | Brightness sysfs cache + memcpy fast path |
| ⚡ `src/gameSwitcher/gs_overlay.h` | ✏️ M | +33 / −7 | Double-fork playActivity + content-match OOB fix |

---

## ✅ 6. Test Suite Verification

Numbers below come from an **actual `make unit-test` run** on this workspace (x86-64 host,
exit code `0`, 2026-08-23), not from a static count.

| Metric | Value |
|:---|---:|
| 🧪 Suites listed in `TESTS` | **68** |
| 📄 `test_*.c` files present in the tree | **68** *(all active)* |
| ✅ Tests executed | **1,410** |
| ✅ Assertions executed | **71,385** |
| ❌ Failures | **0** |
| 🎯 Result | **ALL PASSED** ✅ |
| ⏱️ Run only *(binaries prebuilt)* | **~2.7 s** |

### 📈 Suite count across the port

| Checkpoint | Suites in `TESTS` | Tests | Note |
|:---|---:|---:|:---|
| Base `07505ea5` | **0** | 0 | No host suite existed upstream |
| After `ad402fa2` | 67 *(aspirational)* | — | ⚠️ Build broken — most `.c` files absent |
| After `1a1e3f84` | **17** | 583 | Narrowed to the sources actually present |
| After `300390a7` | **66** | 1,373 | ✅ All passing |
| After `deb8b6ad` | **66** | 1,376 | +3 hash regression tests |
| After `c9e052d4` + `47fc5289` | **67** | 1,407 | ✅ `test_history_recent` + NEON oracles |
| After `bda89b2d` | **68** | **1,410** | ✅ `test_images_browser` re-enabled (3 tests / 22 assertions) |
| After `74f0a0af` | **68** | **1,410** | Review-pass fixes; suite counts unchanged |

The 17 intermediate suites:

```
test_str            test_str_security   test_file           test_file_security
test_json           test_json_security  test_json_null_guards  test_list
test_signal_handler test_state          test_state_security test_flags
test_process        test_clock          test_critical_fixes test_null_safety
test_system_utils
```

### ✅ The formerly deferred suite

`test_images_browser.c` was excluded from `TESTS` pending the `src/infoPanel/imagesBrowser.c`
hardening. That port landed in `bda89b2d`; the suite is now in `TESTS` and passes
(**3 tests / 22 assertions**). All 68 `test_*.c` files in the tree are active.

### 🏆 Top suites by test count

| Suite | Tests | Assertions |
|:---|---:|---:|
| `test_list` | 156 | 265 |
| `test_file` | 89 | 184 |
| `test_str` | 74 | 361 |
| `test_formatters` | 45 | 140 |
| `test_neon` | 44 | 1,402 |
| `test_str_security` | 41 | 659 |
| `test_file_security` | 40 | 58 |
| `test_neon_pixel` | 38 | 72 |

> 🔍 `test_alpha_scale` has 26 tests but **65,877 assertions** — most of the suite total —
> because it sweeps the alpha range exhaustively.
> `test_hash` remains dense on purpose: **15 tests / 350 assertions**, 264 bit-identity vectors.
> `test_gs_popmenu` is **24 tests / 155 assertions** at the tip.

---

## 🔁 7. Reproduction Commands

```bash
cd /path/to/Onion

# Commit list and count (must be 48 at code tip 74f0a0af)
git rev-list --count 07505ea5..HEAD
git log --oneline --reverse 07505ea5..HEAD

# Aggregate delta (docs refresh on top of 74f0a0af adds only docs/ lines)
git diff --shortstat 07505ea5 HEAD            # 140 files, +27,059 / −766
git diff --shortstat 07505ea5 HEAD -- src/    #  58 files,  +2,252 / −750
git diff --shortstat 07505ea5 HEAD -- test/   #  75 files, +23,233 / −10
git diff --shortstat 07505ea5 HEAD -- docs/   #   2 files,  (docs line count)
git diff --shortstat 07505ea5 HEAD -- . ':!docs'  # 138 files, +25,538 / −766  (code only)

# Added vs modified
git diff --name-status 07505ea5 HEAD | awk '{print $1}' | sort | uniq -c

# Per-file numbers
git diff --numstat 07505ea5 HEAD | sort -k1 -rn

# Test suite (real run, prints the summary table)
make unit-test
```

---

⚡ See also: **[ONIONPLUS_OPTIMIZATION.md](./ONIONPLUS_OPTIMIZATION.md)** — what these changes
actually do, with before/after code and performance figures.

<sub>Repository: [Amiga500/Onion](https://github.com/Amiga500/Onion) · Branch: `OnionPlus` ·
Base [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) → Code tip [`74f0a0af`](https://github.com/Amiga500/Onion/commit/74f0a0af) (**48** commits) ·
Commits analyzed: **48** · All figures regenerated from `git` and a real `make unit-test` run
on 2026-08-23.</sub>
