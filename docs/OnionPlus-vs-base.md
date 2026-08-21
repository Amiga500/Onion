# 📐 OnionPlus vs. base release — Diff Statistics

> **7 commits** · **101 files** · **+25,489 / −301 lines** · **76 added** / **25 modified** · **0 deleted** · **66 test suites** · **1,376 tests** ✅

> **What this document is:** the raw, reproducible *diff arithmetic* between the OnionPlus tip
> and the upstream base release. Every number here comes from `git` on this workspace.
> **What it is not:** a narrative of what the changes do — for optimizations, hardening and
> performance figures see **[ONIONPLUS_OPTIMIZATION.md](./ONIONPLUS_OPTIMIZATION.md)**.

| 🔖 Reference | Value |
|:---|:---|
| 🌿 Branch tip | `OnionPlus` → `HEAD`, the 7th commit *(2026-08-20, Amiga500 — the commit that also carries this file, so its own SHA cannot appear here)* |
| 🏁 Base / merge-base | [`07505ea5`](https://github.com/Amiga500/Onion/commit/07505ea5) — `OnionUI/Onion:main` *(2026-01-21, Aemiii91)* |
| ⏩ Commits ahead | **7** *(all authored 2026-08-20)* |
| 📦 Aggregate delta | **101 files** · **+25,489** / **−301** |
| 🧩 Code-only delta *(excl. `docs/`)* | **99 files** · **+24,222** / **−301** |
| 🧪 Unit tests at tip | **66 suites** · **1,376 tests** · **70,002 assertions** · **0 failures** ✅ |
| 🔀 Net line growth | **+25,188** |

> 🔁 **Self-reference.** The two files in `docs/` are part of the range they measure, so every
> *aggregate* figure below includes them. Wherever that matters, the **code-only** subset
> (everything except `docs/`) is given alongside it. Each table states which of the two it uses.

---

## 📋 Table of Contents

1. [Headline Ratios](#-1-headline-ratios)
2. [The 7 Commits](#-2-the-7-commits)
3. [Breakdown by Directory](#️-3-breakdown-by-directory)
4. [Breakdown by Functional Category](#️-4-breakdown-by-functional-category)
5. [Key Files](#-5-key-files)
6. [Test Suite Verification](#-6-test-suite-verification)
7. [Reproduction Commands](#-7-reproduction-commands)

---

## 📊 1. Headline Ratios

*Shares are of the **aggregate** 25,489 insertions, `docs/` included.*

| Metric | Value | Share |
|:---|---:|---:|
| 🧪 Insertions that are **test code** | **23,070** | **90.5 %** |
| 🧩 Insertions that are **production code** (`src/`) | **1,148** | **4.5 %** |
| 📚 Insertions that are **documentation** | **1,267** | **5.0 %** |
| 🏗️ Insertions that are **build wiring** (root `Makefile`) | **4** | **<0.1 %** |
| ➕ Files **added** (`A`) | **76** | **75.2 %** |
| ✏️ Files **modified** (`M`) | **25** | **24.8 %** |
| 🗑️ Files **deleted** (`D`) | **0** | **0 %** |
| 🔁 Insertions per deletion | **85 : 1** | — |
| 🧪 Test lines per production line | **≈ 20 : 1** | — |

> 📈 **Read this as:** OnionPlus is a **test-heavy, low-blast-radius** port. Roughly
> **20 lines of test** landed for every **1 line of production code**, and nothing was deleted
> outright — the 301 removed lines are all in-place rewrites inside 25 modified files.

---

## 🔀 2. The 7 Commits

| # | Hash | Subject | Files | +/− | Category |
|:-:|:-----|:--------|------:|----:|:---------|
| 1 | [`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1b40f6d021a58a41589d6371b30a81142) | OnionPlus: NEON pixel conversions ported from OniOpus46 | 7 | +483 / −80 | ⚡ NEON / perf |
| 2 | [`6da7f28b`](https://github.com/Amiga500/Onion/commit/6da7f28b3a7503a46e0caa799cd85ab9117a0785) | 🎨 apply clang-format changes | 2 | +2 / −2 | 🎨 Formatting *(CI-generated)* |
| 3 | [`ad402fa2`](https://github.com/Amiga500/Onion/commit/ad402fa2e400af6538f971f89979ac0647daa98f) | OnionPlus: port common crash/memory hardening from OniOpus46 | 33 | +9,724 / −193 | 🛡️ Hardening + 🧪 test |
| 4 | [`1a1e3f84`](https://github.com/Amiga500/Onion/commit/1a1e3f84c7e8651e9b5a9f4d0954033b6c6cb3db) | Limit unit-test suite list to ported hardening tests | 1 | +2 / −2 | 🧪 Test fix |
| 5 | [`300390a7`](https://github.com/Amiga500/Onion/commit/300390a7bfec1887acaa261cfd711b57c70c23fc) | OnionPlus: expand host unit-test suite from OniOpus46 | 58 | +13,894 / −18 | 🧪 Test + build |
| 6 | [`971d6169`](https://github.com/Amiga500/Onion/commit/971d6169aa4a0fed355ea89e3a9b35b223598270) | Add OnionPlus optimization and diff-stat documentation | 2 | +683 / −0 | 📚 Docs |
| 7 | `HEAD` *(this commit)* | Fix pre-existing hash, save-state and `const` defects; refresh docs | 6 | +1,171 / −476 | 🛡️ Fix + 🧪 test + 📚 docs |
| | | **Aggregate `07505ea5..HEAD`** | **101** | **+25,489 / −301** | |

*Ordered oldest → tip (`git log --reverse 07505ea5..HEAD`).*

> ℹ️ **5 of the 7 commits are hand-written engineering work.** `6da7f28b` is a CI
> `clang-format` pass (2 whitespace lines, zero semantic change) and `971d6169` is the first
> revision of this documentation pair. Both are included in every total.

> 🔁 Commit 7 rewrites the two `docs/` files in place, so **459 of its 476 deletions and 1,043
> of its 1,171 insertions are documentation churn**. Its code-only footprint is **4 files,
> +128 / −17**: `hash.h` (+20 / −10), `gs_popMenu.h` (+10 / −5), `file.c` (+2 / −2) and
> `test/test_hash.c` (+96 / −0).

### 📝 Notes per commit

| Commit | What landed |
|:---|:---|
| ⚡ `d7aed5a1` | New `neon_pixel.h` (**+301**) with 7 assembly kernels, plus NEON wiring in `rotate180.h`, `surfaceSetAlpha.h`, `IMG_Save.h`, `screenshot.h`, `jpg2png.c`, `pngScale.c`. |
| 🎨 `6da7f28b` | `clang-format` touch-ups on two headers already modified by the NEON port. |
| 🛡️ `ad402fa2` | **14** `src/common` files (**+540 / −193**) and the first **19** test files (**+9,184**). Introduces `signal_handler.h`, hardens `file.c` / `str.c` / `json.h`, adds `test/onion_test.h` and `test/Makefile.unit`. |
| 🧪 `1a1e3f84` | Shrinks `TESTS` from an aspirational list (67 names, most `.c` files still missing → the build was broken) to the **17** suites actually present. |
| 🧪 `300390a7` | Ports the remaining host suites: tip reaches **66 / 1,373**, plus `perf.h` and the root `Makefile` / `test/Makefile*` wiring. |
| 📚 `971d6169` | `ONIONPLUS_OPTIMIZATION.md` + this file, first revision. No code. |
| 🛡️ `HEAD` | Three **pre-existing** defects fixed — the `FNV1A_Pippip_Yurii` 8-byte over-read and unaligned loads, the uninitialised `stateFilePath` in the Game Switcher save thread, and a `const`-discarding cast in `file_basename` — plus **3 new hash regression tests** (tip reaches **66 / 1,376**) and this documentation refresh. |

---

## 🗂️ 3. Breakdown by Directory

*Aggregate range `07505ea5..HEAD`, `docs/` included.*

| 📁 Area | Files | ➕ Insertions | ➖ Deletions | Share of + |
|:---|---:|---:|---:|---:|
| 🧪 `test/` *(incl. `test/Makefile*`)* | **73** | **+23,070** | **−10** | 90.5 % |
| 🧩 `src/` | **25** | **+1,148** | **−290** | 4.5 % |
| 📚 `docs/` | **2** | **+1,267** | **0** | 5.0 % |
| 🏗️ `Makefile` *(root)* | **1** | **+4** | **−1** | <0.1 % |
| | **101** | **+25,489** | **−301** | 100 % |

### 🧪 Inside `test/`

| Item | Files | +/− |
|:---|---:|---:|
| `test_*.c` suites *(all new)* | **67** | **+22,101 / −0** |
| `Makefile.unit` *(new)* | 1 | +606 / −0 |
| `onion_test.h` — `TEST` / `RUN_TEST` framework *(new)* | 1 | +162 / −0 |
| `README.md` *(new)* | 1 | +105 / −0 |
| `Makefile` *(modified)* | 1 | +33 / −8 |
| `test_infoPanel.cpp` *(modified)* | 1 | +38 / −2 |
| `Makefile.gtest` *(new)* | 1 | +25 / −0 |
| **Total** | **73** | **+23,070 / −10** |

### 🧩 Inside `src/`

**22 modified**, **3 added** (`neon_pixel.h`, `perf.h`, `signal_handler.h`), **0 deleted**.

The 22 modified files are 20 from the port proper plus the two touched only by the defect
fixes in commit 7: `src/common/utils/hash.h` and `src/gameSwitcher/gs_popMenu.h`.
`src/common/utils/file.c` appears in both groups.

---

## 🏷️ 4. Breakdown by Functional Category

| Category | Files | ➕ | ➖ | Net |
|:---|---:|---:|---:|---:|
| 🧪 Unit test suites | 68 | +22,139 | −2 | **+22,137** |
| 📚 Documentation | 3 | +1,372 | 0 | **+1,372** |
| 🏗️ Makefile / build wiring | 4 | +668 | −9 | **+659** |
| 🛡️ Hardening & correctness (`src/`) | 16 | +527 | −211 | **+316** |
| ⚡ NEON / graphics (`src/`) | 7 | +482 | −79 | **+403** |
| 🧰 Test framework (`onion_test.h`) | 1 | +162 | 0 | **+162** |
| 🔧 New shared infra (`perf.h`, `signal_handler.h`) | 2 | +139 | 0 | **+139** |
| | **101** | **+25,489** | **−301** | **+25,188** |

*Categories are file-level classifications, so `test/README.md` counts under 📚 Documentation
and `test/Makefile*` under 🏗️ build wiring — which is why the per-directory table in
[§3](#️-3-breakdown-by-directory) and this one slice the same 101 files differently. Both
tables sum to the same **101 files / +25,489 / −301**.*

### ⚡ NEON / graphics detail

| File | Status | +/− |
|:---|:---:|---:|
| `src/common/utils/neon_pixel.h` | 🆕 A | +301 / −0 |
| `src/common/utils/surfaceSetAlpha.h` | ✏️ M | +62 / −14 |
| `src/pngScale/pngScale.c` | ✏️ M | +44 / −29 |
| `src/common/system/screenshot.h` | ✏️ M | +38 / −11 |
| `src/common/utils/rotate180.h` | ✏️ M | +19 / −0 |
| `src/common/utils/IMG_Save.h` | ✏️ M | +11 / −14 |
| `src/jpg2png/jpg2png.c` | ✏️ M | +7 / −11 |
| **Total** | | **+482 / −79** |

### 🛡️ Hardening & correctness detail

| File | Status | +/− | Landed in |
|:---|:---:|---:|:---|
| `src/common/utils/file.c` | ✏️ M | +207 / −71 | `ad402fa2` + `HEAD` |
| `src/common/system/state.h` | ✏️ M | +102 / −48 | `ad402fa2` |
| `src/common/utils/str.c` | ✏️ M | +41 / −20 | `ad402fa2` |
| `src/common/components/list.h` | ✏️ M | +40 / −14 | `ad402fa2` |
| `src/common/system/settings.h` | ✏️ M | +37 / −10 | `ad402fa2` |
| `src/common/utils/process.h` | ✏️ M | +29 / −14 | `ad402fa2` |
| `src/common/utils/hash.h` | ✏️ M | +20 / −10 | `HEAD` |
| `src/common/utils/json.h` | ✏️ M | +13 / −4 | `ad402fa2` |
| `src/gameSwitcher/gs_popMenu.h` | ✏️ M | +10 / −5 | `HEAD` |
| `src/common/utils/file.h` | ✏️ M | +8 / −0 | `ad402fa2` |
| `src/common/utils/flags.h` | ✏️ M | +5 / −2 | `ad402fa2` |
| `src/common/system/clock.h` | ✏️ M | +4 / −5 | `ad402fa2` |
| `src/common/utils/log.c` | ✏️ M | +4 / −3 | `ad402fa2` |
| `src/common/system/axp.h` | ✏️ M | +4 / −0 | `ad402fa2` |
| `src/common/utils/msleep.h` | ✏️ M | +2 / −1 | `ad402fa2` |
| `src/common/utils/str.h` | ✏️ M | +1 / −4 | `ad402fa2` |
| **Total** | | **+527 / −211** | |

*`file.c` totals **+207 / −71**: the `ad402fa2` hardening (+205 / −69) plus the two-line
`const`-correctness fix to `file_basename` in commit 7.*

---

## 🔑 5. Key Files

| File | Status | Delta | Role |
|:---|:---:|---:|:---|
| 🧪 `test/test_list.c` | 🆕 A | +2,514 | Largest single suite — 154 tests |
| 🧪 `test/test_file.c` | 🆕 A | +1,149 | File I/O, paths, `mkdirs`, `file_copy` — 88 tests |
| 🧪 `test/test_theme_config.c` | 🆕 A | +788 | Theme config read/write — 27 tests |
| 🧪 `test/test_str.c` | 🆕 A | +625 | String ops, CJK, `str_count_char` — 72 tests |
| 🏗️ `test/Makefile.unit` | 🆕 A | +606 | Build + run + summary for 66 suites |
| ⚡ `src/common/utils/neon_pixel.h` | 🆕 A | +301 | 7 ARM NEON pixel kernels + scalar fallbacks |
| 🧪 `test/test_hash.c` | 🆕 A | +284 | Hash regression vectors and alignment coverage — 15 tests / 350 assertions |
| 🛡️ `src/common/utils/file.c` | ✏️ M | +207 / −71 | Path/IO hardening, `system()` removal, direct `read()`, `const`-correct `file_basename` |
| 🛡️ `src/common/system/state.h` | ✏️ M | +102 / −48 | App-state parsing hardening |
| 🔧 `src/common/utils/perf.h` | 🆕 A | +92 | Opt-in host profiling helpers |
| 🧪 `test/onion_test.h` | 🆕 A | +162 | `TEST` / `RUN_TEST` / `ASSERT_*` framework |
| 🔧 `src/common/utils/signal_handler.h` | 🆕 A | +47 | Shared SIGINT/SIGTERM handler |
| 🛡️ `src/common/components/list.h` | ✏️ M | +40 / −14 | Bounds checks + cache slots |
| 🛡️ `src/common/utils/hash.h` | ✏️ M | +20 / −10 | Bounded, alignment-safe 64-bit load; hashes bit-identical |
| 🛡️ `src/gameSwitcher/gs_popMenu.h` | ✏️ M | +10 / −5 | Bail out instead of using an uninitialised save-state path |
| 🏗️ `Makefile` *(root)* | ✏️ M | +4 / −1 | `make unit-test` target |

---

## ✅ 6. Test Suite Verification

Numbers below come from an **actual `make unit-test` run** on this workspace (x86-64 host,
exit code `0`), not from a static count.

| Metric | Value |
|:---|---:|
| 🧪 Suites listed in `TESTS` | **66** |
| 📄 `test_*.c` files present in the tree | **67** *(one deferred)* |
| ✅ Tests executed | **1,376** |
| ✅ Assertions executed | **70,002** |
| ❌ Failures | **0** |
| 🎯 Result | **ALL PASSED** ✅ |
| ⏱️ Clean build + run | **~14.3 s** |
| ⏱️ Run only *(binaries prebuilt)* | **~2.8 s** |

### 📈 Suite count across the port

| Checkpoint | Suites in `TESTS` | Tests | Note |
|:---|---:|---:|:---|
| Base `07505ea5` | **0** | 0 | No host suite existed upstream |
| After `ad402fa2` | 67 *(aspirational)* | — | ⚠️ Build broken — most `.c` files absent |
| After `1a1e3f84` | **17** | 583 | Narrowed to the sources actually present |
| After `300390a7` | **66** | 1,373 | ✅ All passing |
| Tip `HEAD` | **66** | **1,376** | ✅ All passing — **+3** hash regression tests, **+329** assertions |

The suite *count* is unchanged at the tip: the three new tests are additional `RUN_TEST`
entries inside the existing `test_hash` suite, which goes from **12 / 21** to **15 / 350**.

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
was **not** ported. That is the whole difference between **67** files present and **66** active.

### 🏆 Top 10 suites by test count

| Suite | Tests | Assertions |
|:---|---:|---:|
| `test_list` | 154 | 260 |
| `test_file` | 88 | 183 |
| `test_str` | 72 | 357 |
| `test_formatters` | 45 | 140 |
| `test_str_security` | 41 | 659 |
| `test_file_security` | 40 | 58 |
| `test_neon_pixel` | 37 | 71 |
| `test_neon` | 36 | 248 |
| `test_json` | 33 | 64 |
| `test_theme_config` | 27 | 145 |

> 🔍 `test_alpha_scale` has only 20 tests but **65,827 assertions** — **94.0 %** of all
> assertions in the suite — because it sweeps the full alpha range exhaustively.
> `test_hash` is the runner-up on assertion density: **15 tests / 350 assertions**, because
> two of them replay 33 reference vectors across 8 start offsets each.

---

## 🔁 7. Reproduction Commands

```bash
cd /path/to/Onion
git fetch amiga OnionPlus

# Commit list
git log --oneline --reverse 07505ea5..HEAD

# Aggregate delta
git diff --shortstat 07505ea5..HEAD                     # 101 files, +25,489 / −301
git diff --shortstat 07505ea5..HEAD -- src/             #  25 files,  +1,148 / −290
git diff --shortstat 07505ea5..HEAD -- test/            #  73 files, +23,070 / −10
git diff --shortstat 07505ea5..HEAD -- docs/            #   2 files,  +1,267 / −0
git diff --shortstat 07505ea5..HEAD -- . ':!docs'       #  99 files, +24,222 / −301  (code only)

# Added vs modified
git diff --name-status 07505ea5..HEAD | awk '{print $1}' | sort | uniq -c

# Per-file numbers
git diff --numstat 07505ea5..HEAD | sort -k1 -rn

# Confirm the base is really upstream main
git merge-base HEAD origin/main                         # → 07505ea5

# Test suite (real run, prints the summary table)
make unit-test
```

---

⚡ See also: **[ONIONPLUS_OPTIMIZATION.md](./ONIONPLUS_OPTIMIZATION.md)** — what these changes
actually do, with before/after code and performance figures.

<sub>Repository: [Amiga500/Onion](https://github.com/Amiga500/Onion) · Branch: `OnionPlus` ·
Base [`07505ea5`](https://github.com/Amiga500/Onion/commit/07505ea5) → Tip `HEAD` *(7th commit)* ·
Commits analyzed: **7** · All figures regenerated from `git` and a real `make unit-test` run
on 2026-08-20.</sub>
