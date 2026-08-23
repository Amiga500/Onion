# 📐 OnionPlus vs. base release — Diff Statistics

> **12 commits + infra** · **124 files** · **+26,154 / −535 lines** · **79 added** / **45 modified** · **0 deleted** · **67 test suites** · **1,407 tests** ✅

> **What this document is:** the raw, reproducible *diff arithmetic* between the OnionPlus
> working tree and the upstream base release. Every number here comes from `git` on this
> workspace (PR#206-207 critical/medium fixes integrated).
> **What it is not:** a narrative of what the changes do — for optimizations, hardening and
> performance figures see **[ONIONPLUS_OPTIMIZATION.md](./ONIONPLUS_OPTIMIZATION.md)**.

| 🔖 Reference | Value |
|:---|:---|
| 🌿 Branch tip | `origin/OnionPlus` → `HEAD` [`4f7841e0`](https://github.com/Amiga500/Onion/commit/4f7841e0) *(2026-08-22)* |
| 🏁 Base / merge-base | [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) — `OnionUI/Onion:main` *(2026-01-21, Aemiii91)* |
| ⏩ Commits ahead | **12** net *(9 original + `eb3f0aec` WIP-landing + PR#206 + PR#207)* + 3 infra *(docs, version bump, SDL fix)* + 4 reverted *(net zero, see note)* |
| 📦 Aggregate delta | **124 files** · **+26,154** / **−535** |
| 🧩 Code-only delta *(excl. `docs/`)* | **122 files** · **+24,886** / **−535** |
| 🧪 Unit tests at tip | **67 suites** · **1,407 tests** · **71,363 assertions** · **0 failures** ✅ *(re-run 2026-08-22)* |
| 🔀 Net line growth | **+25,619** |

> 🔁 **Self-reference.** The two files in `docs/` are part of the range they measure, so every
> *aggregate* figure below includes them. Wherever that matters, the **code-only** subset
> (everything except `docs/`) is given alongside it. Each table states which of the two it uses.

---

## 📋 Table of Contents

1. [Headline Ratios](#-1-headline-ratios)
2. [The 9 Commits + WIP](#-2-the-9-commits--wip)
3. [Breakdown by Directory](#️-3-breakdown-by-directory)
4. [Breakdown by Functional Category](#️-4-breakdown-by-functional-category)
5. [Key Files](#-5-key-files)
6. [Test Suite Verification](#-6-test-suite-verification)
7. [Reproduction Commands](#-7-reproduction-commands)

---

## 📊 1. Headline Ratios

*Shares are of the **aggregate** 26,154 insertions, `docs/` included.*

| Metric | Value | Share |
|:---|---:|---:|
| 🧪 Insertions that are **test code** | **23,233** | **88.8 %** |
| 🧩 Insertions that are **production code** (`src/`) | **1,601** | **6.1 %** |
| 📚 Insertions that are **documentation** | **1,268** | **4.8 %** |
| 🏗️ Insertions that are **build/CI wiring** | **52** | **0.2 %** |
| ➕ Files **added** (`A`) | **79** | **63.7 %** |
| ✏️ Files **modified** (`M`) | **45** | **36.3 %** |
| 🗑️ Files **deleted** (`D`) | **0** | **0 %** |
| 🔁 Insertions per deletion | **≈ 49 : 1** | — |
| 🧪 Test lines per production line | **≈ 15 : 1** | — |

> 📈 **Read this as:** OnionPlus is still a **test-heavy, low-blast-radius** port. Roughly
> **15 lines of test** landed for every **1 line of production code**. Nothing was deleted
> outright — the 535 removed lines are in-place rewrites inside modified files.

---

## 🔀 2. The 12 Net Commits + Infra

| # | Hash | Subject | Files | +/− | Category |
|:-:|:-----|:--------|------:|----:|:---------|
| 1 | [`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1b40f6d021a58a41589d6371b30a81142) | OnionPlus: NEON pixel conversions ported from OniOpus46 | 7 | +483 / −80 | ⚡ NEON / perf |
| 2 | [`6da7f28b`](https://github.com/Amiga500/Onion/commit/6da7f28b3a7503a46e0caa799cd85ab9117a0785) | 🎨 apply clang-format changes | 2 | +2 / −2 | 🎨 Formatting *(CI-generated)* |
| 3 | [`ad402fa2`](https://github.com/Amiga500/Onion/commit/ad402fa2e400af6538f971f89979ac0647daa98f) | OnionPlus: port common crash/memory hardening from OniOpus46 | 33 | +9,724 / −193 | 🛡️ Hardening + 🧪 test |
| 4 | [`1a1e3f84`](https://github.com/Amiga500/Onion/commit/1a1e3f84c7e8651e9b5a9f4d0954033b6c6cb3db) | Limit unit-test suite list to ported hardening tests | 1 | +2 / −2 | 🧪 Test fix |
| 5 | [`300390a7`](https://github.com/Amiga500/Onion/commit/300390a7bfec1887acaa261cfd711b57c70c23fc) | OnionPlus: expand host unit-test suite from OniOpus46 | 58 | +13,894 / −18 | 🧪 Test + build |
| 6 | [`971d6169`](https://github.com/Amiga500/Onion/commit/971d6169aa4a0fed355ea89e3a9b35b223598270) | Add OnionPlus optimization and diff-stat documentation | 2 | +683 / −0 | 📚 Docs |
| 7 | [`deb8b6ad`](https://github.com/Amiga500/Onion/commit/deb8b6ad) | Fix pre-existing hash, save-state and `const` defects; refresh docs | 6 | +1,171 / −476 | 🛡️ Fix + 🧪 test + 📚 docs |
| 8 | [`c9e052d4`](https://github.com/Amiga500/Onion/commit/c9e052d4) | Harden host unit-test CI with sanitizers and production contracts | 17 | +527 / −646 | 🧪 Test + CI |
| 9 | [`47fc5289`](https://github.com/Amiga500/Onion/commit/47fc5289) | Unify NEON ifdefs and keep jpg2png out of core | 11 | +487 / −103 | ⚡ NEON + 🏗️ build |
| 10 | [`eb3f0aec`](https://github.com/Amiga500/Onion/commit/eb3f0aec) | Port TTF label cache, shared signal handlers, and leftover hardening | 18 | *landed WIP* | 🎨 TTF + 🛡️ |
| 11 | [PR#206](https://github.com/Amiga500/Onion/pull/206) | Critical fixes: `file_isLocked` O_CREAT, `RUN_TEST` [FAIL], dialog cleanup | 4 | +58 / −15 | 🛡️ Critical bugfix |
| 12 | [PR#207](https://github.com/Amiga500/Onion/pull/207) | Medium fixes: MULTIVALUE cache color, screenshot VLA guard, UTF-8 validation, fsync | 6 | +42 / −28 | 🛡️ Medium bugfix |
| — | [`2537c94d`](https://github.com/Amiga500/Onion/commit/2537c94d) | fix: no `SDL_Color.a` on Miyoo toolchain (ARM build fix) | 1 | +4 / −4 | 🏗️ Build fix |
| — | `bef883c2`+`00eedbde` | Version date bump → `20260822`; this docs refresh | 3 | — | 📚 Infra |
| | | **Aggregate `07505ea5` → HEAD** | **124** | **+26,154 / −535** | |

*Ordered oldest → tip (`git log --reverse 07505ea5..HEAD`).*

> ℹ️ **Reverted commits.** Four commits (`eea25f88`, `10ec2387`, `840e2f2a`, `1e359571` —
> `process_start` fork+execv rewrite and OSD changes) were reverted in `4f7841e0` on the same
> day: the `execv` rewrite broke multi-word argument tokenization. They are in history but
> contribute **net zero** to every figure above.

> ℹ️ **12 net commits: 9 original engineering + WIP landing + 2 fix PRs.** `6da7f28b` and two
> more are CI `clang-format` passes (whitespace only). PR#206 and PR#207 are targeted critical
> and medium severity bugfixes (7 bugs total). All are included in every total.

### 📝 Notes per commit

| Commit | What landed |
|:---|:---|
| ⚡ `d7aed5a1` | New `neon_pixel.h` with 7 assembly kernels, plus NEON wiring in `rotate180.h`, `surfaceSetAlpha.h`, `IMG_Save.h`, `screenshot.h`, `jpg2png.c`, `pngScale.c`. |
| 🎨 `6da7f28b` | `clang-format` touch-ups on two headers already modified by the NEON port. |
| 🛡️ `ad402fa2` | Common-layer hardening and the first host suites. Introduces `signal_handler.h`, hardens `file.c` / `str.c` / `json.h`, adds `test/onion_test.h` and `test/Makefile.unit`. |
| 🧪 `1a1e3f84` | Shrinks `TESTS` from an aspirational list (build was broken) to the **17** suites actually present. |
| 🧪 `300390a7` | Ports the remaining host suites; adds `perf.h` and root `Makefile` / `test/Makefile*` wiring. |
| 📚 `971d6169` | `ONIONPLUS_OPTIMIZATION.md` + this file, first revision. No code. |
| 🛡️ `deb8b6ad` | Three **pre-existing** defects — `FNV1A_Pippip_Yurii` 8-byte over-read and unaligned loads, uninitialised `stateFilePath`, `const`-discarding `file_basename` — plus hash regression tests. |
| 🧪 `c9e052d4` | CI `make unit-test` + sanitizer job; production-header includes; `test_history_recent` contract (`continue` skip of non-game entries **locked**). |
| ⚡ `47fc5289` | Unified `__ARM_NEON` ifdefs, scalar oracles, `count <= 0` rotate, `neon-arm` qemu job, `jpg2png` Makefile sibling **kept out of `core`**. |
| 🎨 `eb3f0aec` | Lands the former WIP: OniOpus46 TTF caches (`list`/`footer`/`header`/`dialog`), signal-handler call sites, `reset.h` `file_remove_recursive`, bounded screenshot/jpg2png paths, `config.mk` `--gc-sections`. |
| 🛡️ PR#206 | **Critical fixes**: `file_isLocked()` restores O_CREAT flag (save-state regression), `RUN_TEST` macro now prints [FAIL] for failures (test reporting), `dialog` cache cleanup (memory leak). |
| 🛡️ PR#207 | **Medium fixes**: MULTIVALUE cache now keyed on color (UX regression), `screenshot_save()` guarded against w=0/h=0 (VLA UB), `includeCJK()` validates all 3 UTF-8 bytes, `file_remove_recursive()` logs errors, `file_changeKeyValue()` fflush+fsync before rename (data consistency). |
| 🏗️ `2537c94d` | Miyoo toolchain `SDL_Color` is RGB-only — the PR#207 color key must not read `.a`. ARM pre-release build restored. |

---

## 🗂️ 3. Breakdown by Directory

*Aggregate range `07505ea5` → working tree, `docs/` included.*

| 📁 Area | Files | ➕ Insertions | ➖ Deletions | Share of + |
|:---|---:|---:|---:|---:|
| 🧪 `test/` *(incl. `test/Makefile*`)* | **75** | **+23,233** | **−10** | 88.8 % |
| 🧩 `src/` | **42** | **+1,601** | **−520** | 6.1 % |
| 📚 `docs/` | **2** | **+1,268** | **0** | 4.8 % |
| 🏗️ `Makefile` + `.github/` + `.gitignore` | **5** | **+52** | **−5** | 0.2 % |
| | **124** | **+26,154** | **−535** | 100 % |

### 🧪 Inside `test/`

| Item | Files | +/− |
|:---|---:|---:|
| `test_*.c` suites *(all new)* | **68** | *(included in test/ total)* |
| `Makefile.unit` *(new)* | 1 | +684 / −0 |
| `onion_test.h` — `TEST` / `RUN_TEST` framework *(new)* | 1 | +162 / −0 |
| **Total `test/`** | **75** | **+23,233 / −10** |

### 🧩 Inside `src/`

**38 modified**, **4 added** (`neon_pixel.h`, `perf.h`, `signal_handler.h`, `gs_savestate_path.h`), **0 deleted**.

The extra files beyond the original 25-file NEON/hardening set are theme-render caches,
signal-handler call sites, `reset.h`, `config.mk`, `jpg2png/Makefile` and
`gs_savestate_path.h`.

---

## 🏷️ 4. Breakdown by Functional Category

Categories below are approximate file-level labels for the same 124 files. Prefer the
directory table in [§3](#️-3-breakdown-by-directory) when checking `git diff --stat`.

| Category | Role |
|:---|:---|
| 🧪 Unit test suites + harness | 75 files under `test/` |
| 📚 Documentation | 2 files under `docs/` |
| ⚡ NEON / graphics | `neon_pixel.h`, `surfaceSetAlpha.h`, `rotate180.h`, `IMG_Save.h`, `screenshot.h`, `pngScale.c`, `jpg2png.c` |
| 🛡️ Hardening & correctness | `file.c`, `str.c`, `state.h`, `list.h`, `hash.h`, `gs_popMenu.h`, `reset.h`, … |
| 🎨 TTF / list caches | `theme/render/{list,footer,header,dialog}.h` |
| 🔧 Shared infra | `perf.h`, `signal_handler.h`, `config.mk` `--gc-sections` |
| 🏗️ Makefile / CI | root `Makefile`, `.github/workflows/*`, `jpg2png/Makefile` |

### ⚡ NEON / graphics detail *(from `git diff --numstat`)*

| File | Status | +/− |
|:---|:---:|---:|
| `src/common/utils/neon_pixel.h` | 🆕 A | +343 / −0 |
| `src/common/system/screenshot.h` | ✏️ M | +64 / −29 |
| `src/pngScale/pngScale.c` | ✏️ M | *(in src/ total)* |
| `src/jpg2png/jpg2png.c` | ✏️ M | +20 / −17 |
| `src/common/utils/rotate180.h` | ✏️ M | *(in src/ total)* |
| `src/common/utils/IMG_Save.h` | ✏️ M | *(in src/ total)* |
| `src/common/utils/surfaceSetAlpha.h` | ✏️ M | *(in src/ total)* |

### 🎨 TTF cache detail

| File | Status | +/− |
|:---|:---:|---:|
| `src/common/theme/render/list.h` | ✏️ M | +105 / −39 |
| `src/common/theme/render/footer.h` | ✏️ M | +70 / −35 |
| `src/common/theme/render/dialog.h` | ✏️ M | +28 / −16 |
| `src/common/theme/render/header.h` | ✏️ M | +26 / −6 |

---

## 🔑 5. Key Files

| File | Status | Delta | Role |
|:---|:---:|---:|:---|
| 🧪 `test/test_list.c` | 🆕 A | +2,100 | Largest single suite — 156 tests |
| 🧪 `test/test_file.c` | 🆕 A | +1,161 | File I/O, paths, `mkdirs`, `file_copy` — 89 tests |
| 🏗️ `test/Makefile.unit` | 🆕 A | +684 | Build + run + summary for 67 suites |
| 🧪 `test/test_neon.c` | 🆕 A | +608 | Scalar NEON fallbacks + oracles — 44 tests / 1,402 assertions |
| ⚡ `src/common/utils/neon_pixel.h` | 🆕 A | +343 | 7 ARM NEON pixel kernels + scalar fallbacks |
| 🧪 `test/test_hash.c` | 🆕 A | +281 | Hash regression vectors — 15 tests / 350 assertions |
| 🧪 `test/test_history_recent.c` | 🆕 A | +226 | Production `history_getRecentPath` contract — 10 tests |
| 🛡️ `src/common/utils/file.c` | ✏️ M | +207 / −71 | Path/IO hardening, `system()` removal, `file_remove_recursive` |
| 🎨 `src/common/theme/render/list.h` | ✏️ M | +105 / −39 | TTF/preview cache populate path |
| 🛡️ `src/common/system/screenshot.h` | ✏️ M | +64 / −29 | NEON convert + bounded `snprintf` path |
| 🔧 `src/gameSwitcher/gs_savestate_path.h` | 🆕 A | +45 / −0 | Save-state path helper extracted for tests |
| 🛡️ `src/common/utils/hash.h` | ✏️ M | +20 / −10 | Bounded, alignment-safe 64-bit load; hashes bit-identical |
| 🏗️ `src/common/config.mk` | ✏️ M | +7 / −0 | `-O2 -ffunction-sections -Wl,--gc-sections` |

---

## ✅ 6. Test Suite Verification

Numbers below come from an **actual `make unit-test` run** on this workspace (x86-64 host,
exit code `0`, **re-verified 2026-08-22** at tip `4f7841e0`), not from a static count.

| Metric | Value |
|:---|---:|
| 🧪 Suites listed in `TESTS` | **67** |
| 📄 `test_*.c` files present in the tree | **68** *(one deferred)* |
| ✅ Tests executed | **1,407** |
| ✅ Assertions executed | **71,363** |
| ❌ Failures | **0** |
| 🎯 Result | **ALL PASSED** ✅ |
| ⏱️ Run only *(binaries prebuilt)* | **~3.3 s** |

### 📈 Suite count across the port

| Checkpoint | Suites in `TESTS` | Tests | Note |
|:---|---:|---:|:---|
| Base `07505ea5` | **0** | 0 | No host suite existed upstream |
| After `ad402fa2` | 67 *(aspirational)* | — | ⚠️ Build broken — most `.c` files absent |
| After `1a1e3f84` | **17** | 583 | Narrowed to the sources actually present |
| After `300390a7` | **66** | 1,373 | ✅ All passing |
| After `deb8b6ad` | **66** | 1,376 | +3 hash regression tests |
| After `c9e052d4` + `47fc5289` + PR#206-207 | **67** | **1,407** | ✅ All critical + medium fixes validated |
| At `4f7841e0` (after revert of 4 bad commits) | **67** | **1,407** | ✅ Re-verified 2026-08-22 — revert changed nothing under test |

The 17 intermediate suites:

```
test_str            test_str_security   test_file           test_file_security
test_json           test_json_security  test_json_null_guards  test_list
test_signal_handler test_state          test_state_security test_flags
test_process        test_clock          test_critical_fixes test_null_safety
test_system_utils
```

### ⏸️ The deferred suite

`test_images_browser.c` exists in the tree with a build rule in `test/Makefile.unit`, but is
**intentionally left out** of `TESTS`: it needs `src/infoPanel/imagesBrowser.c` hardening that
was **not** ported. That is the whole difference between **68** files present and **67** active.

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

---

## 🔁 7. Reproduction Commands

```bash
cd /path/to/Onion

# Commit list
git log --oneline --reverse 07505ea5..HEAD

# Aggregate delta
git diff --shortstat 07505ea5                     # 124 files, +26,154 / −535
git diff --shortstat 07505ea5 -- src/             #  42 files,  +1,601 / −520
git diff --shortstat 07505ea5 -- test/            #  75 files, +23,233 / −10
git diff --shortstat 07505ea5 -- docs/            #   2 files,  (docs line count)
git diff --shortstat 07505ea5 -- . ':!docs'       # 122 files, +24,886 / −535  (code only)

# Added vs modified
git diff --name-status 07505ea5 | awk '{print $1}' | sort | uniq -c

# Per-file numbers
git diff --numstat 07505ea5 | sort -k1 -rn

# Test suite (real run, prints the summary table)
make unit-test
```

---

⚡ See also: **[ONIONPLUS_OPTIMIZATION.md](./ONIONPLUS_OPTIMIZATION.md)** — what these changes
actually do, with before/after code and performance figures.

<sub>Repository: [Amiga500/Onion](https://github.com/Amiga500/Onion) · Branch: `OnionPlus` ·
Base [`07505ea5`](https://github.com/OnionUI/Onion/commit/07505ea5) → Tip `HEAD` (`4f7841e0`) ·
Commits analyzed: **12 net + infra** *(4 reverted, net zero)* · All figures regenerated from `git`
and a real `make unit-test` run on 2026-08-22.</sub>
