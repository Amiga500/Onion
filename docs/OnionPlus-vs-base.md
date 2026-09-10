# 📐 OnionPlus vs. base release — Diff Statistics

> **21 commits** · **182 files** · **+30,487 / −1,109 lines** · **0 deleted** · **68 test suites** · **1,419 tests** ✅

> **What this document is:** the raw, reproducible *diff arithmetic* between the current
> integration branch **`onionplus-compact`** and the upstream base release. Every number
> here comes from `git` on this workspace.
> **What it is not:** a narrative of what the changes do — for optimizations, hardening and
> performance figures see **[ONIONPLUS_OPTIMIZATION.md](./ONIONPLUS_OPTIMIZATION.md)**.

| 🔖 Reference | Value |
|:---|:---|
| 🌿 Branch tip | `onionplus-compact` last code [`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e) vs [`OnionUI/Onion:main`](https://github.com/OnionUI/Onion/tree/main) — compact history + @robcodedev ports + 2026-09-09 review |
| 🏁 Base / merge-base | [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) — `OnionUI/Onion:main` *(2026-01-21, Aemiii91)* |
| ⏩ Commits ahead | **21** *(`git rev-list --count 07505ea5..HEAD` after this number audit. 18 through last code `bf3deb8e`. The long `OnionPlus` branch was **97** to `fa5bb007`.)* |
| 📦 Aggregate delta | **182 files** · **+30,482** / **−1,106** |
| 🔀 @robcodedev ports | `c7a1a7e9` + `587c35ec` + merge `f87e7781` — `OnionUI/Onion` PRs **#1936–#1946** via [Amiga500 #217](https://github.com/Amiga500/Onion/pull/217) |
| 🩹 2026-09-09 review | [`fbd26d06`](https://github.com/Amiga500/Onion/commit/fbd26d06) (3 · +58 / −17) · [`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e) (3 · +79 / −23) |
| 🧪 Unit tests at tip | **68 suites** · **1,419 tests** · **71,410 assertions** · **0 failures** ✅ |
| 🔀 Net line growth | **+29,376** |

> 🔁 **Self-reference.** The two files in `docs/` are part of the range they measure, so every
> *aggregate* figure below includes them. Wherever that matters, the **code-only** subset
> (everything except `docs/`) is given alongside it. Each table states which of the two it uses.
> Headline counts are **`onionplus-compact`**, not the long `OnionPlus` branch.
> Section 2 still lists the original 56 SHAs from that long history (those hashes are **not**
> on compact). Compact SHAs are in [§2b](#-2b-onionplus-compact-shas).

---

## 📋 Table of Contents

1. [Headline Ratios](#-1-headline-ratios)
2. [The original 56-commit window — plus Flip](#-2-the-original-56-commit-window--plus-flip)
2b. [`onionplus-compact` SHAs](#-2b-onionplus-compact-shas)
3. [Breakdown by Directory](#️-3-breakdown-by-directory)
4. [Breakdown by Functional Category](#️-4-breakdown-by-functional-category)
5. [Key Files](#-5-key-files)
6. [Test Suite Verification](#-6-test-suite-verification)
7. [Reproduction Commands](#-7-reproduction-commands)

---

## 📊 1. Headline Ratios

*Shares from `git diff --numstat 07505ea5 HEAD` on `onionplus-compact` (`a224508d`).*

| Metric | Value | Share |
|:---|---:|---:|
| 🧪 Insertions that are **test code** (`test/`) | **23,327** | **76.5 %** |
| 🧩 Insertions that are **production code** (`src/`) | **3,985** | **13.1 %** |
| 📚 Insertions that are **documentation** (`docs/` + README) | **2,548** | **8.4 %** |
| 🏗️ Insertions that are **build/CI/static/Makefile** | **622** | **2.0 %** |
| ➕ Files **added** (`A`) | **88** | **48.4 %** |
| ✏️ Files **modified** (`M`) | **94** | **51.6 %** |
| 🗑️ Files **deleted** (`D`) | **0** | **0 %** |
| 🔁 Insertions per deletion | **≈ 28 : 1** | — |
| 🧪 Test lines per production `src/` line | **≈ 6 : 1** | — |

> 📈 **Read this as:** OnionPlus is still a **test-heavy, low-blast-radius** port. Roughly
> **6 lines of test** landed for every **1 line of `src/`**. Nothing was deleted
> outright — removed lines are in-place rewrites inside modified files.
> Compact production (`src/` + `static/` + CI/Makefile, excluding `.gitignore` and `SDL.h`) is
> **101 files · +4,572 / −1,080**.

---

## 🔀 2. The original 56-commit window — plus Flip

*Historical: rows 1–56 are the original port window on the long `OnionPlus` branch
(`07505ea5` → `82fab865`, authored 2026-08-20–24). Those SHAs are **not** on
`onionplus-compact`. File/+− columns are that commit's own `git show --shortstat`.
Compact SHAs: [§2b](#-2b-onionplus-compact-shas).*

| # | Hash | Subject | Files | +/− | Category |
|:-:|:-----|:--------|------:|----:|:---------|
| 1 | [`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1) | NEON pixel conversions vs OnionUI scalar loops | 7 | +483 / −80 | ⚡ NEON / perf |
| 2 | [`6da7f28b`](https://github.com/Amiga500/Onion/commit/6da7f28b) | 🎨 apply clang-format changes | 2 | +2 / −2 | 🎨 Formatting *(CI-generated)* |
| 3 | [`ad402fa2`](https://github.com/Amiga500/Onion/commit/ad402fa2) | Crash/memory hardening vs OnionUI common layer | 33 | +9,724 / −193 | 🛡️ Hardening + 🧪 test |
| 4 | [`1a1e3f84`](https://github.com/Amiga500/Onion/commit/1a1e3f84) | Limit unit-test suite list to ported hardening tests | 1 | +2 / −2 | 🧪 Test fix |
| 5 | [`300390a7`](https://github.com/Amiga500/Onion/commit/300390a7) | Host unit-test suite (absent on OnionUI/Onion:main) | 58 | +13,894 / −18 | 🧪 Test + build |
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
| 31 | [`4b851203`](https://github.com/Amiga500/Onion/commit/4b851203) | OSD busy-wait fix vs OnionUI usleep(100) | 1 | +51 / −30 | ⚡ Perf |
| 32 | [`e8143d09`](https://github.com/Amiga500/Onion/commit/e8143d09) | Brightness sysfs cache vs OnionUI uncached PWM | 1 | +40 / −13 | ⚡ Perf |
| 33 | [`0121f943`](https://github.com/Amiga500/Onion/commit/0121f943) | Battery charging cache vs OnionUI uncached axp_test | 3 | +96 / −30 | ⚡ Perf + 🛡️ fix |
| 34 | [`0d1ce423`](https://github.com/Amiga500/Onion/commit/0d1ce423) | SQLite open/close 2→1 vs OnionUI | 6 | +250 / −81 | ⚡ Perf + 🛡️ hardening |
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
| 49 | [`46f25987`](https://github.com/Amiga500/Onion/commit/46f25987) | docs: refresh OnionPlus stats to tip 74f0a0af (review-pass fixes) | 2 | +259 / −145 | 📚 Docs |
| 50 | [`a4793fab`](https://github.com/Amiga500/Onion/commit/a4793fab) | ci: publish unique dated OnionPlus GitHub Releases | 1 | +32 / −21 | 🏗️ Release + CI |
| 51 | [`201bae3d`](https://github.com/Amiga500/Onion/commit/201bae3d) | fix: point Miyoo OTA at Amiga500 OnionPlus releases | 1 | +9 / −5 | 🏗️ OTA |
| 52 | [`ddbb7e14`](https://github.com/Amiga500/Onion/commit/ddbb7e14) | fix: name release zip OnionPlus-v so gh-release finds it | 3 | +21 / −6 | 🏗️ Release + CI |
| 53 | [`69af9a21`](https://github.com/Amiga500/Onion/commit/69af9a21) | docs: refresh OnionPlus stats to tip ddbb7e14 (release/OTA) | 2 | +83 / −52 | 📚 Docs |
| 54 | [`e17c6a9b`](https://github.com/Amiga500/Onion/commit/e17c6a9b) | fix: correct GameSwitcher romscreen capture stride and blit | 3 | +70 / −17 | 🛡️ Fix |
| 55 | [`82fab865`](https://github.com/Amiga500/Onion/commit/82fab865) | fix: GameSwitcher preview FB stride and stretch romscreens | 3 | +6 / −4 | 🛡️ Fix |
| 56 | [`e0b6893c`](https://github.com/Amiga500/Onion/commit/e0b6893c) | docs: keep action_loadGame table row from splitting on GitHub | 1 | +1 / −1 | 📚 Docs |
| 88 | [`921155e8`](https://github.com/Amiga500/Onion/commit/921155e8) | feat: port Miyoo Mini Flip + MainUI-285 from Onion v4.5-dev | 19 | +362 / −37 | 📱 Flip port |
| 89 | [`7a9b0a21`](https://github.com/Amiga500/Onion/commit/7a9b0a21) | docs: refresh OnionPlus stats for Flip port 921155e8 | 3 | docs | 📚 Docs |
| 90 | [`64a0e42`](https://github.com/Amiga500/Onion/commit/64a0e42) | fix: charging icon sentinel, retroarch killall, bound paths | 14 | +80 / −36 | 🛡️ Parity vs OnionUI/Onion |
| 94 | [`c8445ae`](https://github.com/Amiga500/Onion/commit/c8445ae) | test: align file_read empty-file contract with production | — | A | 🧪 Finding A |
| 95 | [`5659de2`](https://github.com/Amiga500/Onion/commit/5659de2) | fix: Flip suspend lid, runtime device detect, OTA beta channel | — | B C D | 🛡️ Finding B–D |
| 96 | [`9ab47af`](https://github.com/Amiga500/Onion/commit/9ab47af) / [`2f90bbe`](https://github.com/Amiga500/Onion/commit/2f90bbe) | fix: brightness cache, infoPanel scale identity, theme cleanup | — | E F G | 🛡️ Finding E–G *(duplicate tree)* |
| 97 | [`fa5bb007`](https://github.com/Amiga500/Onion/commit/fa5bb007) | ci: Add push trigger for OnionPlus branch | 1 | CI | 🏗️ CI |
| | | **Aggregate original window `07505ea5` → `e0b6893c`** | **144** | **+27,234 / −811** | |
| | | **Headline at long-branch tip `fa5bb007` (2026-09-03)** | **172** | **+28,786 / −977** | |
| | | **Headline at `onionplus-compact` after this docs refresh (2026-09-09)** | **182** | **+30,487 / −1,109** | |

> ℹ️ A previous revision of this table had **52 rows** and tip `ddbb7e14`.
> Rows 25–29 are a remote experiment that was fully reverted — net zero in the tree.
> Rows 45–48 are the 2026-08-23 review pass. Rows 50–52 are release/OTA wiring (2026-08-24).
> Rows 54–55 are GameSwitcher framebuffer stride / romscreen stretch fixes (2026-08-24).

### 📝 Notes per later commit

| Commit | What landed |
|:---|:---|
| ⚡ `d7aed5a1` | New `neon_pixel.h` with 7 assembly kernels, plus NEON wiring in `rotate180.h`, `surfaceSetAlpha.h`, `IMG_Save.h`, `screenshot.h`, `jpg2png.c`, `pngScale.c`. |
| 🛡️ `ad402fa2` | Common-layer hardening and the first host suites. Introduces `signal_handler.h`, hardens `file.c` / `str.c` / `json.h`, adds `test/onion_test.h` and `test/Makefile.unit`. |
| 🧪 `300390a7` | Ports the remaining host suites; adds `perf.h` and root `Makefile` / `test/Makefile*` wiring. |
| 🛡️ `deb8b6ad` | Three **pre-existing** defects — `FNV1A_Pippip_Yurii` 8-byte over-read and unaligned loads, uninitialised `stateFilePath`, `const`-discarding `file_basename` — plus hash regression tests. |
| ⚡ `eb3f0aec` | TTF caches vs OnionUI per-frame render (`list`/`footer`/`header`/`dialog`), signal-handler call sites, `reset.h` `file_remove_recursive`, bounded screenshot/jpg2png paths, `config.mk` `--gc-sections`. |
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
| 🏗️ `a4793fab` | Pre-release workflow: unique tag + zip per build (`softprops/action-gh-release`), no `latest` overwrite. |
| 🏗️ `201bae3d` | `ota_update.sh`: repo `Amiga500/Onion`, asset `OnionPlus-v`, bootstrap branch `OnionPlus`. |
| 🏗️ `ddbb7e14` | `Makefile` `TARGET=OnionPlus`; workflow renames zip if name drifts. |
| 🛡️ `e17c6a9b` | Romlist / romscreen capture: correct framebuffer stride and blit. |
| 🛡️ `82fab865` | GameSwitcher preview FB stride + stretch romscreens to fit. |

---

## 🌿 2b. `onionplus-compact` SHAs

`git log --reverse --pretty=format:'%h %s' --shortstat 07505ea5..HEAD` on this branch.
Merge `f87e7781` has no own tree delta.

| # | Hash | Subject | Files | +/− | Category |
|:-:|:-----|:--------|------:|----:|:---------|
| 1 | [`2d2cc64a`](https://github.com/Amiga500/Onion/commit/2d2cc64a) | build: CI, release packaging and OTA wiring | 10 | +143 / −58 | 🏗️ CI |
| 2 | [`2deac7a2`](https://github.com/Amiga500/Onion/commit/2deac7a2) | fix: harden common file/string/hash/process helpers | 14 | +509 / −153 | 🛡️ Hardening |
| 3 | [`9c89bd50`](https://github.com/Amiga500/Onion/commit/9c89bd50) | perf: add NEON pixel kernels with scalar oracles | 6 | +591 / −98 | ⚡ NEON |
| 4 | [`98242ab5`](https://github.com/Amiga500/Onion/commit/98242ab5) | perf: cut OSD/battery/brightness hot-path overhead | 10 | +240 / −107 | ⚡ Power |
| 5 | [`7bbdb332`](https://github.com/Amiga500/Onion/commit/7bbdb332) | fix: cache theme/infoPanel surfaces and harden image paths | 12 | +587 / −197 | ⚡ + 🛡️ |
| 6 | [`154c6fb0`](https://github.com/Amiga500/Onion/commit/154c6fb0) | fix: GameSwitcher overlay, savestate paths and playActivity I/O | 14 | +539 / −206 | 🛡️ GS |
| 7 | [`157ae659`](https://github.com/Amiga500/Onion/commit/157ae659) | fix: bounds and NULL guards in Tweaks and companion apps | 10 | +111 / −69 | 🛡️ |
| 8 | [`e6bb63a7`](https://github.com/Amiga500/Onion/commit/e6bb63a7) | fix: AdvanceMENU launch, fonts, PWM and ROM scripts | 8 | +56 / −32 | 🕹️ AdvanceMENU |
| 9 | [`fd18bdd1`](https://github.com/Amiga500/Onion/commit/fd18bdd1) | feat: Miyoo Mini Flip detection and MainUI-285 | 8 | +327 / −31 | 📱 Flip |
| 10 | [`4c1b65aa`](https://github.com/Amiga500/Onion/commit/4c1b65aa) | test: host unit-test harness (68 suites, 1414 tests) | 76 | +23,318 / −10 | 🧪 Test |
| 11 | [`3dca44b0`](https://github.com/Amiga500/Onion/commit/3dca44b0) | docs: OnionPlus vs OnionUI/Onion main at 07505ea5 | 4 | +2,412 / −16 | 📚 Docs |
| 12 | [`22004cce`](https://github.com/Amiga500/Onion/commit/22004cce) | ci: run host tests on onionplus-compact | 1 | +1 / −1 | 🏗️ CI |
| 13 | [`c7a1a7e9`](https://github.com/Amiga500/Onion/commit/c7a1a7e9) | Port PRs #1936 #1937 #1941 #1942 #1943 #1944 #1945 #1946 | 3 | +88 / −16 | 🔀 @robcodedev |
| 14 | [`587c35ec`](https://github.com/Amiga500/Onion/commit/587c35ec) | Port PRs #1938 #1939 #1940 (ThemeSwitcher, GS favorites, fbmode) | 15 | +1,326 / −112 | 🔀 @robcodedev |
| 15 | [`db9b3e81`](https://github.com/Amiga500/Onion/commit/db9b3e81) | 🎨 apply clang-format changes | 2 | +4 / −3 | 🎨 Format |
| 16 | [`f87e7781`](https://github.com/Amiga500/Onion/commit/f87e7781) | Merge pull request #217 | — | — | 🔀 Merge |
| 17 | [`fbd26d06`](https://github.com/Amiga500/Onion/commit/fbd26d06) | fix: list label cache dimming and installer Flip detection | 3 | +58 / −17 | 🛡️ Review |
| 18 | [`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e) | fix: Flip 640 lock from early dmesg, fbmode-before-driver, AXP percBat | 3 | +79 / −23 | 🛡️ Review |
| 19 | [`a224508d`](https://github.com/Amiga500/Onion/commit/a224508d) | docs: compact branch, robcodedev #1936-1946, and 2026-09-09 review | 4 | +284 / −151 | 📚 Docs |

@robcodedev PRs (still open on `OnionUI/Onion` when ported): **#1936** keymon SELECT refresh ·
**#1937** `lt.lang` JSON · **#1938** ThemeSwitcher on-demand previews · **#1939** GS favorites
+ crash fixes · **#1940** `fbmode` + FB transitions · **#1941** `.forceKillRetroarch` ·
**#1942** `romwinidx` on SD · **#1943** theme per `SERIAL_NUMBER` · **#1944** recents cap 200 ·
**#1945** skip RA cfg patch · **#1946** overlap launch.

`fbd26d06` / `bf3deb8e` are **not** from those PRs. List-cache dimming, installer hall-first
+ FB preclear, Plus/Flip `mi_fb0` poll, `commit_mainui_fbmode` driver wait, AXP `percBat` clamp.

---

## 🗂️ 3. Breakdown by Directory

*Aggregate range `07505ea5` → `onionplus-compact` HEAD (`bf3deb8e`), `docs/` included.*

| 📁 Area | Files | ➕ Insertions | ➖ Deletions | Share of + |
|:---|---:|---:|---:|---:|
| 🧪 `test/` *(incl. `test/Makefile*`)* | **75** | **+23,327** | **−10** | 76.5 % |
| 🧩 `src/` | **78** | **+3,985** | **−962** | 13.1 % |
| 📚 `docs/` + README | **4** | **+2,548** | **−16** | 8.4 % |
| 🏗️ `static/` | **18** | **+483** | **−88** | 1.6 % |
| 🏗️ `.github/` + Makefile | **5** | **+104** | **−30** | 0.3 % |
| 📎 `.gitignore` + `SDL.h` | **2** | **+35** | **−0** | 0.1 % |
| | **182** | **+30,482** | **−1,106** | 100 % |

### 🧪 Inside `test/`

| Item | Files | +/− |
|:---|---:|---:|
| `test_*.c` suites *(all new)* | **68** | *(included in test/ total)* |
| `Makefile.unit` *(new)* | 1 | +684 / −0 |
| `onion_test.h` — `TEST` / `RUN_TEST` framework *(new)* | 1 | +166 / −0 |
| **Total `test/`** | **75** | **+23,327 / −10** |

### 🧩 Inside `src/`

**78 files** under `src/` at compact HEAD (**7 added**, **71 modified**, **0 deleted**):
`neon_pixel.h`, `perf.h`, `signal_handler.h`, `gs_savestate_path.h`, `gs_favorites.h`,
`fbmode.c`, `fbmode/Makefile`.

The extra files beyond the original 25-file NEON/hardening set are theme-render caches,
signal-handler call sites, `reset.h`, `config.mk`, `jpg2png/Makefile`,
`gs_savestate_path.h`, the 2026-08-23 power/hardening batch (`osd.h`, `display.h`,
`battery.h`, `batmon.c/h`, `config.h`, `gs_overlay.h`, `playActivity/*`,
`playActivityUI/playActivityUI.c`, `infoPanel/*`), the review-pass touch-ups in
`gameSwitcher.c` / `gs_popMenu.h` / `gs_overlay.h` / `osd.h`, and the GameSwitcher
framebuffer stride / romscreen stretch fixes (`screenshot.h`, `gs_overlay.h`,
`gs_render.h`, `gs_romscreen.h`, `display.h`).

---

## 🏷️ 4. Breakdown by Functional Category

Categories below are approximate file-level labels. Prefer the
directory table in [§3](#️-3-breakdown-by-directory) when checking `git diff --stat`.

| Category | Role |
|:---|:---|
| 🧪 Unit test suites + harness | 75 files under `test/` |
| 📚 Documentation | 3 files under `docs/` + root `README.md` |
| ⚡ NEON / graphics | `neon_pixel.h`, `surfaceSetAlpha.h`, `rotate180.h`, `IMG_Save.h`, `screenshot.h`, `pngScale.c`, `jpg2png.c` |
| 🛡️ Hardening & correctness | `file.c`, `str.c`, `state.h`, `list.h`, `hash.h`, `gs_popMenu.h`, `reset.h`, `infoPanel/*`, `gameSwitcher.c`, … |
| 🔋 Power / CPU | `osd.h` (bar busy-wait + overlay-loop throttle), `display.h` (brightness cache), `battery.h` + `batmon/*` (charging cache), `gs_overlay.h` (double-fork+exec) |
| 💾 Database | `playActivity/*` (open/close 2 → 1 + hardening), `playActivityUI/playActivityUI.c` |
| 🎨 TTF / list caches | `theme/render/{list,footer,header,dialog}.h` |
| 🔧 Shared infra | `perf.h`, `signal_handler.h`, `config.mk` `--gc-sections`, `config.h` |
| 🏗️ Makefile / CI / OTA | root `Makefile` (`TARGET=OnionPlus`), `.github/workflows/*` (dated releases, `HEAD` SHA), `ota_update.sh`, `jpg2png/Makefile` |

### ⚡ NEON / graphics detail *(from `git diff --numstat`)*

| File | Status | +/− |
|:---|:---:|---:|
| `src/common/utils/neon_pixel.h` | 🆕 A | +343 / −0 |
| `src/common/system/screenshot.h` | ✏️ M | +103 / −38 |
| `src/pngScale/pngScale.c` | ✏️ M | +44 / −29 |
| `src/jpg2png/jpg2png.c` | ✏️ M | +20 / −17 |
| `src/common/utils/rotate180.h` | ✏️ M | *(in src/ total)* |
| `src/common/utils/IMG_Save.h` | ✏️ M | *(in src/ total)* |
| `src/common/utils/surfaceSetAlpha.h` | ✏️ M | *(in src/ total)* |

### 🎨 TTF cache detail

| File | Status | +/− |
|:---|:---:|---:|
| `src/common/theme/render/list.h` | ✏️ M | +135 / −42 |
| `src/common/theme/render/footer.h` | ✏️ M | +70 / −35 |
| `src/common/theme/render/dialog.h` | ✏️ M | +45 / −16 |
| `src/common/theme/render/header.h` | ✏️ M | +26 / −6 |

---

## 🔑 5. Key Files

| File | Status | Delta | Role |
|:---|:---:|---:|:---|
| 🧪 `test/test_list.c` | 🆕 A | +2,100 | Largest single suite — 156 tests |
| 🧪 `test/test_file.c` | 🆕 A | +1,169 | File I/O, paths, `mkdirs`, `file_copy` — 89 tests |
| 🏗️ `test/Makefile.unit` | 🆕 A | +684 | Build + run + summary for 68 suites |
| 🧪 `test/test_neon.c` | 🆕 A | +608 | Scalar NEON fallbacks + oracles — 44 tests / 1,402 assertions |
| ⚡ `src/common/utils/neon_pixel.h` | 🆕 A | +343 | 7 ARM NEON pixel kernels + scalar fallbacks |
| 🧪 `test/test_hash.c` | 🆕 A | +281 | Hash regression vectors — 15 tests / 350 assertions |
| 🧪 `test/test_history_recent.c` | 🆕 A | +226 | Production `history_getRecentPath` contract — 10 tests |
| 🛡️ `src/common/utils/file.c` | ✏️ M | +220 / −71 | Path/IO hardening, `system()` removal, `file_remove_recursive` |
| 🎨 `src/common/theme/render/list.h` | ✏️ M | +135 / −42 | TTF/preview cache populate path; dimming uses a surface copy |
| 🛡️ `src/common/system/screenshot.h` | ✏️ M | +103 / −38 | NEON convert + bounded `snprintf` + romscreen stride |
| 🔧 `src/gameSwitcher/gs_savestate_path.h` | 🆕 A | +45 / −0 | Save-state path helper extracted for tests |
| 🛡️ `src/common/utils/hash.h` | ✏️ M | +20 / −10 | Bounded, alignment-safe 64-bit load; hashes bit-identical |
| 🏗️ `src/common/config.mk` | ✏️ M | +7 / −0 | `-O2 -ffunction-sections -Wl,--gc-sections` |
| ⚡ `src/playActivity/playActivityDB.h` | ✏️ M | +143 / −42 | SQLite open/close 2 → 1 + stmt guards |
| 🛡️ `src/infoPanel/infoPanel.c` | ✏️ M | +83 / −34 | JSON/argv hardening, `-r` flag fix |
| ⚡ `src/common/system/battery.h` | ✏️ M | +57 / −9 | 2 s `battery_isCharging()` cache |
| ⚡ `src/common/system/osd.h` | ✏️ M | +50 / −33 | Bar busy-wait 100 µs → 16 ms + overlay-loop `msleep(2)` |
| ⚡ `src/common/system/display.h` | ✏️ M | +47 / −14 | Brightness sysfs cache + memcpy fast path + stride |
| ⚡ `src/gameSwitcher/gs_overlay.h` | ✏️ M | +106 / −19 | Double-fork playActivity + content-match OOB + FB stride |

---

## ✅ 6. Test Suite Verification

Numbers below come from an **actual `make unit-test` run** on this workspace (x86-64 host,
exit code `0`, 2026-09-09), not from a static count.

| Metric | Value |
|:---|---:|
| 🧪 Suites listed in `TESTS` | **68** |
| 📄 `test_*.c` files present in the tree | **68** *(all active)* |
| ✅ Tests executed | **1,419** |
| ✅ Assertions executed | **71,410** |
| ❌ Failures | **0** |
| 🎯 Result | **ALL PASSED** ✅ |
| ⏱️ Run only *(this host)* | **~2.5 s** |

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
| After `ddbb7e14` | **68** | **1,410** | Release/OTA wiring; suite counts unchanged |
| After `82fab865` | **68** | **1,410** | GameSwitcher stride/romscreen fixes; suite counts unchanged |
| After `921155e8` | **68** | **1,412** | Flip: `test_device_model` +2 tests / +8 assertions. `test_settings` field only. Those two suites re-run green; full 68-suite harness not re-executed for this row. |
| After 2026-09-01 A–G | **68** | **1,414** | empty-file contract + Flip macros (long `OnionPlus`) |
| After `bf3deb8e` | **68** | **1,419** | `test_alpha_scale` +1; `test_battery` +4. Full harness green. |

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
| `test_file` | 89 | 185 |
| `test_str` | 74 | 361 |
| `test_formatters` | 45 | 140 |
| `test_neon` | 44 | 1,402 |
| `test_str_security` | 41 | 659 |
| `test_file_security` | 40 | 59 |
| `test_neon_pixel` | 38 | 72 |

> 🔍 `test_alpha_scale` has 27 tests but **65,879 assertions** — most of the suite total —
> because it sweeps the alpha range exhaustively (includes `scale_alpha_255_does_not_undo_dim`).
> `test_hash` remains dense on purpose: **15 tests / 350 assertions**, 264 bit-identity vectors.
> `test_gs_popmenu` is **24 tests / 155 assertions** at the tip.
> `test_battery` adds the AXP clamp contract (`axp_percent_*`).

---

## 🔁 7. Reproduction Commands

```bash
cd /path/to/Onion

git rev-list --count 07505ea5..HEAD               # 19 at a224508d; 20 after this number audit
git log --oneline --reverse 07505ea5..HEAD
git diff --shortstat 07505ea5 HEAD                # 182 files, +30,487 / −1,109

# @robcodedev ports + 2026-09-09 review
git show --shortstat c7a1a7e9 587c35ec fbd26d06 bf3deb8e

git diff --shortstat 07505ea5 HEAD -- . ':!docs' ':!README.md'  # 178 files, +27,934 / −1,090

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

<sub>Repository: [Amiga500/Onion](https://github.com/Amiga500/Onion) · Branch: `onionplus-compact` ·
Base [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) (`OnionUI/Onion:main`) → last code [`bf3deb8e`](https://github.com/Amiga500/Onion/commit/bf3deb8e) (**20** including this number audit) ·
Headline figures refreshed **2026-09-09** · Section 2 rows 1–56 still describe the original long-branch port window · §2b is the compact SHA list.</sub>
