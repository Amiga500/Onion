# 🧅 OnionPlus — Optimization & Hardening Report

> **5 commits** · **97 files** · **+24,094 / −284 lines** · **66 test suites** · **1,373 unit tests** · **69,673 assertions** · **ALL PASSED**

> **Scope:** this report covers *only* the 5 commits that make up the OnionPlus port
> on top of upstream `OnionUI/Onion:main`. It is **not** a report on the whole
> OniOpus46 branch. Every number below was extracted from `git` and from a real
> `make unit-test` run — see [§7 Methodology & Limits](#-7-methodology--limits)
> for what is measured and what is not.

---

## 📋 Table of Contents

1. [Summary Overview](#-summary-overview)
2. [Commit Breakdown](#-1-commit-breakdown)
3. [Security & Hardening](#%EF%B8%8F-2-security--hardening)
4. [Performance — NEON Pixel Paths](#-3-performance--neon-pixel-paths)
5. [Testing](#-4-testing)
6. [Build & Tooling](#%EF%B8%8F-5-build--tooling)
7. [Overall Statistics](#-6-overall-statistics)
8. [Methodology & Limits](#-7-methodology--limits)
9. [Known Residuals](#%EF%B8%8F-8-known-residuals)
10. [Final Status](#-final-status)

---

## 📊 Summary Overview

> Port of the OniOpus46 crash/memory hardening, NEON pixel conversions and host
> unit-test suite onto the OnionPlus branch. Baseline is `07505ea5`, the tip of
> `OnionUI/Onion:main` at the time of the port; the resulting tip is `300390a7`.

| Category | Before | After | Δ |
|:---------|:------:|:-----:|--:|
| 🛡️ `sprintf` call sites *(18 ported files)* | 23 | 1 | **−95.7 %** |
| 🛡️ `strcpy` + `strcat` call sites *(18 ported files)* | 37 | 9 | **−75.7 %** |
| 🛡️ `strtok` (non-reentrant) *(18 ported files)* | 4 | 0 | **−100 %** ✅ |
| 🛡️ `system()` call sites *(18 ported files)* | 3 | 1 | **−66.7 %** |
| 🛡️ NULL-check predicates added *(all of `src/`)* | — | 56 | **+56 lines** |
| 🛡️ `fclose`/`close` calls added *(all of `src/`)* | — | 18 | **+18 lines** |
| ⚡ NEON pixel kernels | 0 | 7 | **+7 kernels** |
| 🧪 Active unit-test suites | 0 | 66 | **+66** ✅ |
| 🧪 Unit tests / assertions | 0 / 0 | 1,373 / 69,673 | **ALL PASSED** ✅ |
| 🏗️ Host test entry point | none | `make unit-test` | **added** ✅ |

*"18 ported files" = the C/H files under `src/` actually touched by these 5 commits.
Counts are per call site, obtained by grepping the file contents at `07505ea5`
vs. at `300390a7`. They are **not** codebase-wide claims.*

---

## 🔀 1. Commit Breakdown

| # | SHA | Subject | Files | +/− |
|:-:|:----|:--------|------:|----:|
| 1 | [`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1b40f6d021a58a41589d6371b30a81142) | NEON pixel conversions ported from OniOpus46 | 7 | +483 / −80 |
| 2 | [`6da7f28b`](https://github.com/Amiga500/Onion/commit/6da7f28b3a7503a46e0caa799cd85ab9117a0785) | 🎨 apply clang-format changes *(CI-generated)* | 2 | +2 / −2 |
| 3 | [`ad402fa2`](https://github.com/Amiga500/Onion/commit/ad402fa2e400af6538f971f89979ac0647daa98f) | Port common crash/memory hardening from OniOpus46 | 33 | +9,724 / −193 |
| 4 | [`1a1e3f84`](https://github.com/Amiga500/Onion/commit/1a1e3f84c7e8651e9b5a9f4d0954033b6c6cb3db) | Limit unit-test suite list to ported hardening tests | 1 | +2 / −2 |
| 5 | [`300390a7`](https://github.com/Amiga500/Onion/commit/300390a7bfec1887acaa261cfd711b57c70c23fc) | Expand host unit-test suite from OniOpus46 | 58 | +13,894 / −18 |
| | | **Aggregate `07505ea5..300390a7`** | **97** | **+24,094 / −284** |

> ℹ️ **5 commits in the delta, of which 4 are hand-written.** Commit
> [`6da7f28b`](https://github.com/Amiga500/Onion/commit/6da7f28b3a7503a46e0caa799cd85ab9117a0785)
> was **generated automatically by the CI formatting workflow on PR #197**, not
> authored by hand. Its committer is `GitHub Actions <actions@github.com>`
> (verifiable with `git show -s --format='%cn <%ce>' 6da7f28b`), and its entire
> content is a 2-line `clang-format` whitespace pass across 2 files with no
> semantic change. It is counted in every total in this report — the aggregate
> is the full `07505ea5..300390a7` range — but it should not be read as
> engineering work.

Split by area:

| Area | Files | +/− |
|:-----|------:|----:|
| `src/` + root `Makefile` | 24 | +1,120 / −274 |
| `test/` | 73 | +22,974 / −10 |

> 📈 **Result:** 95 % of the added lines are test code. The production-code
> surface of the port is deliberately small (**+1,120 / −274** across 24 files).

---

## 🛡️ 2. Security & Hardening

### 2.1 `sprintf` → `snprintf` (23 → 1 call sites)

**Problem:** unbounded `sprintf` into fixed-size stack buffers.
**Solution:** bounded `snprintf` with an explicit `sizeof(dst)` argument.

| File | `sprintf` removed | `snprintf` added |
|:-----|------------------:|-----------------:|
| `src/common/utils/process.h` | 7 | 7 |
| `src/common/system/state.h` | 6 | 6 |
| `src/common/utils/file.c` | 3 | 4 |
| `src/common/utils/str.c` | 3 | 3 |
| `src/common/utils/log.c` | 2 | 2 |
| `src/common/components/list.h` | 2 | 2 |
| `src/common/utils/str.h` | 0 | 1 |
| **Total** | **23** | **25** |

Across the 18 ported files, `snprintf` call sites went **4 → 27** while `sprintf`
went **23 → 1**.

Notable: `log.c` also fixed an off-by-one (`snprintf(_log_path, 63, …)` →
`sizeof(_log_path)`) and replaced an unbounded `vsprintf` with a length-checked
`vsnprintf`.

> 📈 **Result:** 22 of 23 unbounded format calls in the ported files eliminated.
> The single remaining site is documented in [§8](#%EF%B8%8F-8-known-residuals).

---

### 2.2 `strcpy` / `strcat` → bounded copies (37 → 9 call sites)

**Problem:** unbounded string copies into fixed-size struct fields and buffers.
**Solution:** `strncpy`/`memcpy`/`memmove` with an explicit size and a forced
null terminator on the destination.

| File | `strcpy`/`strcat` removed | bounded added |
|:-----|--------------------------:|--------------:|
| `src/common/utils/file.c` | 7 | 1 |
| `src/common/system/state.h` | 7 | 5 |
| `src/common/system/settings.h` | 6 | 8 |
| `src/common/utils/str.c` | 2 | 0 *(→ `memcpy`)* |
| `src/common/utils/str.h` | 2 | 0 |
| `src/common/utils/process.h` | 2 | 1 |
| `src/common/components/list.h` | 2 | 2 |
| **Total** | **28** | **17** |

Every `strncpy` introduced is paired with an explicit
`dst[sizeof(dst) - 1] = '\0'`, so the classic "`strncpy` does not null-terminate
when `strlen(src) >= n`" trap is closed at each new site.

Notable rewrites:
- `str.h` — the widely-used `concat(ptr, str1, str2)` macro expanded to
  `strcpy(ptr, str1); strcat(ptr, str2);` with no bound at all. It is now a
  single `snprintf(ptr, STR_MAX, "%s%s", str1, str2)`. Because this is a macro,
  the fix bounds **every** `concat()` call site in the codebase, not just the
  ones in the files listed above — those call sites are not counted in the
  table, so the real reduction is larger than the −75.7 % figure suggests.
- `settings.h` — 5 struct-field copies in `settings_copy()` converted to
  `strncpy` + explicit terminator.
- `state.h` — a VLA-based `strcpy(secondPart, colonPosition + 1)` /
  `strcpy(romPathSearch, secondPart)` pair (stack VLA sized from untrusted input)
  replaced with a single in-place `memmove`.
- `str.c` — `strcpy(tmp, orig)` in `str_replace` replaced with
  `memcpy(tmp, orig, strlen(orig) + 1)`.

> 📈 **Result:** 28 of 37 unbounded copies in the ported files eliminated
> (**−75.7 %**). Remaining sites listed in [§8](#%EF%B8%8F-8-known-residuals).

---

### 2.3 `strtok` → `strtok_r` (4 → 0)

All 4 non-reentrant `strtok` call sites in the ported files were converted to
`strtok_r` with a caller-owned save pointer.

> 📈 **Result:** **−100 %** non-reentrant tokenizer use in the ported files. ✅

---

### 2.4 `system()` → POSIX calls (3 → 1)

Two shell-out call sites in `src/common/utils/file.c` were replaced with
direct POSIX code:

| Before | After |
|:-------|:------|
| `sprintf(dir_cmd, "mkdir -p \"%s\"", dir_path); system(dir_cmd);` | iterative `mkdir(tmp, 0755)` walk, returning `true` on `0` or `EEXIST` |
| `snprintf(system_cmd, …, "cp -f \"%s\" \"%s\"", …); system(system_cmd);` | `open`/`read`/`write`/`close` loop preserving `st.st_mode`, with `close()` on every error path |

Both removals eliminate a path-injection vector (a filename containing a quote
or `;` previously reached `/bin/sh`) and remove a `fork`+`exec` of a shell from
hot file operations.

> 📈 **Result:** 2 of 3 shell-out sites removed. The `mkdirs()` and `file_copy()`
> rewrites are covered by `test_file` (88 tests) and `test_file_security` (40 tests).

---

### 2.5 NULL-pointer and I/O return-value guards

**56** NULL-check predicate lines were added across `src/`:

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
| `src/common/utils/perf.h` *(new file)* | 1 |
| **Total** | **56** |

Plus **18** added `fclose`/`close` calls, closing descriptor leaks on error paths.

Concrete fixes:

| File | Fix |
|:-----|:----|
| `axp.h` | `open(AXPDEV, O_RDWR)` result unchecked → `if (fd < 0) return -1`. Previously `ioctl(-1, …)` was reachable (2 sites: `axp_read`, `axp_write`). |
| `clock.h` | `open("/dev/rtc0") > 0` → `>= 0`. fd `0` is a valid descriptor. |
| `clock.h` | `getMilliseconds()` returned `int` computed from `te.tv_sec * 1000` — overflow after ~24.8 days of uptime. Now returns `long` with `(long)te.tv_sec * 1000L`. |
| `flags.h` | `close(creat(filename, 777))` → `creat(filename, 0644)` with `fd >= 0` checked before `close()`. `777` decimal is mode `01411`, not `0777`. |
| `file.c` | `file_isLocked()` used `O_RDONLY \| O_CREAT, 0666` — the lock *check* created the file as a side effect. Now `O_RDONLY` only. |
| `file.c` | `if ((fd = open(path, O_WRONLY)) == 0)` → `< 0`, plus `close(fd)`. |
| `file.c` | 100 MB upper bound on `st.st_size` before `malloc`, preventing a huge/corrupt file from driving an unbounded allocation. |
| `file.c` | `sscanf` format built with a bounded field width (`%255[^%c]`) instead of an unbounded `%[^…]`. |
| `file.c` | 2 write paths made atomic via a `.tmp` file + `rename()`. |
| `json.h` | `cJSON_GetStringValue()` result checked for NULL before `strncpy`; `json_load` checks file contents; `json_save` checks the `cJSON_Print` result. |
| `process.h` | `opendir("/proc")` NULL-guarded; `atoi` → `strtol`; `fscanf` return value checked; `process_start_read_return` return type `bool` → `int`, and an unbounded `strcpy(out_str, result)` replaced with a bounded copy. |
| `state.h` | Hardcoded `str += 19` pointer arithmetic to skip `HOME=<path> ./` replaced with a `strstr(str, " ./")` search — the old code mis-parsed whenever `HOME` was not exactly `/mnt/SDCARD`. |
| `state.h` | `main_total - 4` / `total - page_size` could go negative → clamped to `0`. |
| `state.h` | `strstr(...) + 7` / `+ 11` results dereferenced without a NULL check; each now `continue`s instead of crashing, and the per-line allocation is freed on every path (previously an early `return NULL` leaked it). |
| `list.h` | `list_getVisibleItemAt` walked past `item_count`; loop bound corrected. `list_addItem` now checks `items != NULL` and `item_count < max_items` before writing. `list_free` NULL-guards `items`. |
| `str.c` | `str_replace` allocation size computed in `size_t` with explicit multiplication- and addition-overflow guards (previously signed `int` arithmetic). |
| `str.c` | `str_trim` skip loop could read past the terminator — now checks `*str != '\0'` first. The all-whitespace case also returned length `1` while writing an empty string; now returns `0`, so callers using the return value as a length no longer over-read by one. |
| `str.c` | `str_count_char` iterated `i <= strlen(str)` (re-evaluating `strlen` each step, and reading one byte past the end) → single-pass pointer walk. |
| `str.c` | CJK detection tested `c >= 0x80 && c <= 0x9FFF` on an `unsigned char` — the upper bound was unreachable, so *every* byte ≥ 0x80 matched. Rewritten as a real 3-byte UTF-8 sequence check. |
| `IMG_Save.h` | Line buffer allocated from `image->pitch` but indexed by `width`; now allocated as `width * sizeof(Uint32)`, NULL-checked, and the row pointer advances by `pitch` so non-contiguous surfaces are handled correctly. |
| `screenshot.h` | `png_create_write_struct` / `png_create_info_struct` results NULL-checked, with `fclose(fp)` on each failure path. `screenshot_save` NULL-guards its input buffer. |

---

### 2.6 Code deduplication — `signal_handler.h`

New shared header `src/common/utils/signal_handler.h` (47 lines) providing
`signal_handler_quit(volatile bool *quit_flag, int sig)` for SIGINT/SIGTERM.
Note the `volatile` qualifier — the previous per-app copies wrote to a plain
`bool` from signal context.

> ⚠️ In this port the header is **added and unit-tested** (`test_signal_handler`,
> 10 tests) but the individual applications have **not yet been migrated** to it.
> The deduplication benefit is pending; no call sites were removed by these 5 commits.

---

## ⚡ 3. Performance — NEON Pixel Paths

Commit [`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1b40f6d021a58a41589d6371b30a81142)
adds `src/common/utils/neon_pixel.h` (301 lines): 7 inline pixel kernels written
in ARM NEON inline assembly, each guarded by `#ifdef __ARM_NEON` with a **scalar
C fallback** and a scalar tail loop for the remainder.

### 3.1 Kernels

| Function | Operation | Vector width | Instructions |
|:---------|:----------|:------------:|:-------------|
| `neon_swap_rb_inplace` | ARGB ↔ ABGR in place | **16 px/iter** | `vld4.8` ×2 / `vst4.8` ×2 |
| `neon_argb_to_rgba` | ARGB → RGBA | **16 px/iter** | `vld4.8` ×2 / `vst4.8` ×2 |
| `neon_argb_to_rgba_alpha` | ARGB → RGBA, zero pixel if `A == 0` | **8 px/iter** | `vld4.8` / `vst4.8` |
| `neon_rgb888_to_argb` | RGB888 → ARGB8888 | **16 px/iter** | `vld3.8` ×2 / `vst4.8` ×2 |
| `neon_gray8_to_argb` | Gray8 → ARGB8888 | **16 px/iter** | `vld1.8` ×2 / `vst4.8` ×2 |
| `neon_gray8a_to_argb` | Gray8+Alpha → ARGB8888 | **16 px/iter** | `vld2.8` ×2 / `vst4.8` ×2 |
| `neon_rotate180_inplace` | 180° rotation in place | **16 px/iter** (8 low ↔ 8 high) | `vld1.32` ×2 / `vrev64.32` ×4 / `vst1.32` ×2 |

Each kernel processes `count & ~15` (or `& ~7`) pixels vectorised and finishes
the remainder with a scalar loop, so behaviour is identical for any `count`.

### 3.2 Call sites converted

| File | Change |
|:-----|:-------|
| `src/jpg2png/jpg2png.c` | Two scalar per-pixel loops (RGB888→ARGB decode, ARGB→RGBA encode) replaced with `neon_rgb888_to_argb` / `neon_argb_to_rgba`. |
| `src/pngScale/pngScale.c` | All 4 channel cases (gray8, gray8+alpha, RGB888, RGBA) plus the output R/B swap replaced with NEON kernels via thin `static inline` wrappers. |
| `src/common/system/screenshot.h` | Per-pixel ARGB→RGBA conversion replaced with `neon_argb_to_rgba` (normal path) and `neon_swap_rb_inplace` (rotated path). |
| `src/common/utils/IMG_Save.h` | Per-pixel alpha-conditional conversion replaced with `neon_argb_to_rgba_alpha`. |
| `src/common/utils/rotate180.h` | For 32 bpp surfaces with contiguous pitch (`pitch == w * 4`), the SDL rotozoom blit is replaced with in-place `neon_rotate180_inplace`. Other formats keep the stock rotozoom path. |
| `src/common/utils/surfaceSetAlpha.h` | Per-pixel float alpha scaling (`alpha / 255.0f`) replaced with fixed-point integer math: `alpha_scale = (alpha * 257 + 1) >> 8`, then `(a * alpha_scale) >> 8`. Exact identity at `alpha == 255`. |

### 3.3 What this does and does not claim

- ✅ **Verified:** the **scalar fallback** of every kernel produces the expected
  output. `test_neon` (36 tests / 248 assertions) and `test_neon_pixel`
  (37 / 71) exercise each function's portable C path against hand-computed
  expected values, including single-pixel, zero-alpha and non-multiple-of-16
  counts. `test_alpha_scale` (20 / 65,827) checks the fixed-point alpha formula
  for rounding, boundary values, identity at `alpha == 255`, and commutativity
  of consecutive blends.
- ✅ **Verified:** `rotate180` avoids one full surface allocation + blit for the
  32 bpp contiguous case, because it now operates in place.
- ⚠️ **The NEON assembly itself is NOT covered by any test.** The host test
  runner is x86-64, so `__ARM_NEON` is undefined and only the `#else` scalar
  branches compile and run. Equivalence between the assembly path and the scalar
  path is therefore **assumed, not demonstrated** — the assembly is validated
  only by cross-compilation. This is the single biggest verification gap in the
  port.
- ⚠️ **Theoretical / expected, NOT measured:** the 16 px/iteration throughput is
  a property of the instruction encoding, not a benchmark result. **No timing
  measurement was taken on a Miyoo Mini or Mini+.** No speedup percentage is
  claimed anywhere in this document.

---

## 🧪 4. Testing

Commits [`ad402fa2`](https://github.com/Amiga500/Onion/commit/ad402fa2e400af6538f971f89979ac0647daa98f)
and [`300390a7`](https://github.com/Amiga500/Onion/commit/300390a7bfec1887acaa261cfd711b57c70c23fc)
add a self-contained host test harness: `test/onion_test.h` (162 lines),
`test/Makefile.unit` (606 lines), `test/Makefile.gtest` (25 lines),
`test/README.md` (105 lines) and **67 new test source files**.

| Metric | Value |
|:-------|------:|
| Active suites | **66** |
| Tests | **1,373** |
| Assertions | **69,673** |
| Result | **ALL PASSED** ✅ |
| Wall-clock runtime | **~15 s** |

Suite growth across the port:

| Stage | Active suites |
|:------|--------------:|
| Baseline `07505ea5` | 0 |
| After `ad402fa2` + `1a1e3f84` | 17 |
| After `300390a7` | **66** |

Commit `ad402fa2` imported a `Makefile.unit` whose `TESTS` list still named all
**67** OniOpus46 suites while only **18** test sources had been copied over, so
`make unit-test` did not build at that commit. Commit `1a1e3f84` is the fix: it
narrows `TESTS` to the **17** suites whose production code had actually been
hardened and whose sources were present. Commit `300390a7` then restores the
list to **66** once the remaining sources land.

### 4.1 Security-focused suites

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

### 4.2 Largest suites

| Suite | Tests | Assertions |
|:------|------:|-----------:|
| `test_list` | 154 | 260 |
| `test_file` | 88 | 183 |
| `test_str` | 72 | 357 |
| `test_formatters` | 45 | 140 |
| `test_neon_pixel` | 37 | 71 |
| `test_neon` | 36 | 248 |
| `test_json` | 33 | 64 |
| `test_theme_config` | 27 | 145 |
| `test_settings` | 26 | 56 |
| `test_alpha_scale` | 20 | 65,827 |

### 4.3 Deferred suite

`test_images_browser` (97 lines) is present in the tree and has a build rule in
`test/Makefile.unit`, but is **intentionally excluded** from the `TESTS` list:
it depends on `src/infoPanel/imagesBrowser.c` hardening that has **not** been
ported yet. It can be run manually with `make -f Makefile.unit test_images_browser`
once that port lands.

---

## 🏗️ 5. Build & Tooling

**Root `Makefile`** — new `unit-test` target (added to `.PHONY`):

```make
unit-test:
	@cd $(ROOT_DIR)/test && $(MAKE) -f Makefile.unit all
```

This runs entirely on the host with the system compiler. It requires **no**
cross-toolchain, no SDL, and no device, which makes it usable as a fast
pre-commit and CI gate. The pre-existing `test` target (device/GTest oriented,
requires `external-libs`) is untouched.

**`src/common/utils/perf.h`** (new, 92 lines) — opt-in lightweight profiling:
`PERF_START(label)` / `PERF_END(label)` compile to `do {} while (0)` unless
`-DPERF_ENABLED` is passed, so there is zero cost in release builds. Logs to
`/mnt/SDCARD/.tmp_update/logs/perf.log`. Covered by `test_perf` (5 tests),
which is compiled with `-DPERF_ENABLED`.

**`src/common/utils/msleep.h`** — signal-safety fix: the `msleep_interrupt` flag
is written from a signal handler but was declared `static int`. It is now
`static volatile sig_atomic_t`, which is the only type the C standard guarantees
is safe to access from a signal handler. Covered by `test_msleep` (5 tests).

**Formatting** — commit [`6da7f28b`](https://github.com/Amiga500/Onion/commit/6da7f28b3a7503a46e0caa799cd85ab9117a0785)
is a pure `clang-format` pass (2 lines in 2 files) with no semantic change.

---

## 📈 6. Overall Statistics

| Metric | Value |
|:-------|------:|
| 🔧 **Commits** | **5** |
| 📁 **Files changed** | **97** |
| ➕ **Lines added / removed** | **+24,094 / −284** |
| 🧩 **Production code (`src/` + `Makefile`)** | **24 files · +1,120 / −274** |
| 🧪 **Test code (`test/`)** | **73 files · +22,974 / −10** |
| 🆕 **New test source files** | **67** |
| 🧪 **Active suites / tests / assertions** | **66 / 1,373 / 69,673** |
| ✅ **Test result** | **ALL PASSED** |
| ⚡ **NEON kernels added** | **7** (all with scalar fallback) |
| ⚡ **NEON call sites converted** | **6 files** |
| 🛡️ **`sprintf` call sites** | **23 → 1** (−95.7 %) |
| 🛡️ **`strcpy`+`strcat` call sites** | **37 → 9** (−75.7 %) |
| 🛡️ **`strtok` call sites** | **4 → 0** (−100 %) |
| 🛡️ **`system()` call sites** | **3 → 1** (−66.7 %) |
| 🛡️ **NULL-check predicates added** | **56** |
| 🛡️ **`fclose`/`close` added** | **18** |
| 🏗️ **New build target** | **`make unit-test`** |
| 📉 **Failing tests at tip** | **0 / 1,373** |

Reproduce the aggregate figures with:

```bash
git fetch amiga OnionPlus
git diff --stat 07505ea5 amiga/OnionPlus
git diff --stat 07505ea5 amiga/OnionPlus -- src/ Makefile
make unit-test
```

---

## 🔬 7. Methodology & Limits

### What is measured

- **Line and file counts** — from `git diff --stat 07505ea5..300390a7`. Exact.
- **Call-site counts** — obtained by counting occurrences of each pattern in the
  contents of the 18 ported `src/` files at `07505ea5` versus at `300390a7`.
  Both endpoints are stated so the delta is checkable. These counts are scoped
  to the ported files only.
- **Test results** — a real `make unit-test` run: 66 suites, 1,373 tests,
  69,673 assertions, all passing, in roughly 15 seconds.
- **Correctness of the NEON kernels' scalar fallback paths** — verified by unit
  tests against hand-computed expected values. Note this is the fallback, not
  the NEON assembly; see below.

### What is NOT measured

- ❌ **No on-device benchmarks.** Nothing in this report was timed on a Miyoo
  Mini (283) or Mini+ (354). No frame-rate, latency, CPU-utilisation, battery
  or throughput figure is claimed.
- ❌ **No speedup percentages.** The `px/iter` figures in §3.1 describe the
  instruction encoding, not observed performance. Real-world gain depends on
  memory bandwidth, cache behaviour and image size, and is unknown.
- ❌ **NEON assembly is not exercised by the host test run.** The host is x86-64,
  so `__ARM_NEON` is undefined and the scalar fallbacks are what execute during
  `make unit-test`.
- ❌ **No runtime testing on target hardware.** The ported code compiles and
  passes host tests; end-to-end behaviour on device has not been validated as
  part of these 5 commits.
- ❌ **No static-analysis or sanitizer run** (ASan/UBSan/Valgrind/coverity)
  is included in these figures.
- ❌ **Bug counts are not claimed.** This report counts *code changes*
  (call sites, guards, closed descriptors), not "bugs fixed", because mapping a
  hardening edit to a user-visible bug requires reproduction evidence that was
  not collected. Several changes in §2.5 are clearly latent-defect fixes, but
  they are described individually rather than aggregated into a headline number.

### Counting caveats

- Pattern counts are **textual**. `sprintf` counts exclude `snprintf`/`vsnprintf`
  via a preceding-character exclusion; `strcpy`/`strcat` counts exclude
  `strncpy`/`strncat` the same way. Occurrences inside comments or strings, if
  any, are included.
- "NULL-check predicates added" counts added lines matching `== NULL`, `!= NULL`
  or `if (!`. This is a **lower bound proxy** for defensive guards, not an exact
  count of distinct guarded conditions — one guard can span multiple lines, and
  guards written in other styles are not counted.
- The `+/−` totals for a squashed range can differ slightly from the sum of the
  per-commit stats when later commits modify lines introduced by earlier ones.
- **Expect a "5 vs 4" discrepancy against the GitHub release notes.** The
  generated notes for the `latest` tag list only **4** commits, because the
  first one ([`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1b40f6d021a58a41589d6371b30a81142),
  NEON) had already shipped in the **previous** release and is therefore outside
  that tag's comparison range. This report intentionally scopes to the full
  `07505ea5..300390a7` delta, so it counts **5**. The difference is a release-
  boundary artefact, **not** a missing or unaccounted commit.
- Of those 5 commits, **4 are hand-written**; `6da7f28b` is a CI-generated
  `clang-format` pass (see [§1](#-1-commit-breakdown)). Counts of "commits" in
  this report always mean all 5 unless stated otherwise.

---

## ⚠️ 8. Known Residuals

For transparency, these unsafe patterns **remain** in the ported files at
`300390a7`. They are outside the scope of these 5 commits, not oversights that
the numbers above conceal.

| File:line | Pattern | Note |
|:----------|:--------|:-----|
| `src/common/system/screenshot.h:23` | `strcpy` | Fixed literal prefix `/mnt/SDCARD/Screenshots/` into `path_out`. |
| `src/common/system/screenshot.h:32,37,39,57,61` | `strcat` (×5) | Screenshot path assembly. |
| `src/common/system/screenshot.h:52` | `strcpy` | `app_name` from `no_extension`. |
| `src/common/system/screenshot.h:65` | `sprintf` | `"_%03d.png"` suffix — bounded in practice by the `%03d` format, but not by construction. |
| `src/jpg2png/jpg2png.c:137,141` | `strcpy` + `strcat` | `argv[1]` copied into `char filename[256]` then `".png"` appended, with no length check. Reachable with a long CLI argument. |
| `src/common/utils/process.h:101` | `system(cmd)` | `process_start_*` still shells out to run applications; replacing it needs a `fork`/`execv` redesign. |

Recommended follow-ups, in rough priority order:

1. Bound the `jpg2png.c` argument copy (`snprintf(filename, sizeof(filename), "%s.png", argv[1])`) — this is the most directly attacker-influenced of the residuals.
2. Convert `screenshot.h` path assembly to a single `snprintf`.
3. Migrate the applications to `signal_handler.h` to realise the deduplication in §2.6.
4. Port the `infoPanel` hardening so `test_images_browser` can be re-enabled.
5. Add a cross-compiled or QEMU-based run so the NEON assembly paths are actually executed under test.
6. Take baseline timings on device for `jpg2png`, `pngScale` and screenshot capture, so §3 can be backed by measurements.

---

## ✅ Final Status

The OnionPlus branch is **5 commits ahead** of upstream `OnionUI/Onion:main`
(`07505ea5`), adding NEON pixel conversion paths, crash/memory hardening of the
`src/common` layer, and a 66-suite host unit-test harness runnable with a single
`make unit-test`.

> **66 suites · 1,373 tests · 69,673 assertions · 0 failures.** ✅
>
> Note: the baseline `07505ea5` had **no** host test suite, so this is a new
> quality floor rather than a "no regressions" comparison — there is nothing to
> compare against on the upstream side.

The hardening work is **partial by design**: it covers the shared `src/common`
utilities plus the two image tools, with the remaining unsafe call sites
explicitly enumerated in [§8](#%EF%B8%8F-8-known-residuals) rather than rounded away.
Performance claims are limited to what the implementation guarantees
structurally; **no on-device measurement has been taken**, and any speedup
figure should be treated as unverified until benchmarked on real hardware.

---

Repository: [Amiga500/Onion](https://github.com/Amiga500/Onion) · Branch: `OnionPlus` ·
Base: [`07505ea5`](https://github.com/Amiga500/Onion/commit/07505ea5) →
Tip: [`300390a7`](https://github.com/Amiga500/Onion/commit/300390a7bfec1887acaa261cfd711b57c70c23fc) ·
Commits analyzed: **5** · Date: 2026-08-20
