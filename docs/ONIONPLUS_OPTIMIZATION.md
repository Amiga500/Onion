# 🧅 OnionPlus — Optimization & Hardening Report

> **7 commits** · **101 files** · **+25,489 / −301 lines** · **8 NEON kernels** · **66 test suites** · **1,376 tests** · **70,002 assertions** · **ALL PASSED** ✅

> **Scope:** this report covers *only* the commits that make up the OnionPlus port on top of
> upstream `OnionUI/Onion:main` (`07505ea5` → `HEAD`). It is **not** a report on the whole
> [OniOpus46](https://github.com/Amiga500/Onion/blob/OniOpus46/docs/OPTIMIZATION.md) branch.
> Percentages quoted from OniOpus46 are **explicitly labelled as such** — see the
> [legend](#-legend--how-to-read-the-numbers) below.

📊 Raw diff statistics vs. the base release live in a companion document:
**[OnionPlus-vs-base.md](./OnionPlus-vs-base.md)**.

> 🔁 **Self-reference.** The tip commit carries both this document and the defect fixes it
> describes, so its own SHA cannot appear here — it is referred to as `HEAD` throughout. The
> two `docs/` files are inside the range they measure; wherever it matters, the **code-only**
> subset is given next to the aggregate.

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

> Port of the OniOpus46 NEON pixel conversions, crash/memory hardening and host unit-test
> suite onto the OnionPlus branch. Baseline is `07505ea5`, the tip of `OnionUI/Onion:main`
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
| 🛡️ Unsafe `sprintf` call sites | 23 | 1 | **−95.7 %** | 🛡️ |
| 🛡️ Unsafe `strcpy`+`strcat` call sites | 37 | 9 | **−75.7 %** | 🛡️ |
| 🛡️ Non-reentrant `strtok` call sites | 4 | 0 | **−100 %** ✅ | 🛡️ |
| 🛡️ `system()` call sites | 3 | 1 | **−66.7 %** | 🛡️ |
| 🛡️ NULL-check predicates | — | +57 | **+57** | 🛡️ |
| 🛡️ `fclose`/`close` on error paths | — | +18 | **+18** | 🛡️ |
| 🛡️ `FNV1A_Pippip_Yurii` hash load | 8-byte read regardless of `wrdlen`, unaligned | `memcpy` of exactly `wrdlen` bytes into an aligned local | **over-read and unaligned access removed, hashes bit-identical** | 🛡️ 🧪 |
| 🛡️ Game Switcher save thread | ran with an uninitialised `stateFilePath` when the path could not be built | returns before touching RetroArch | **up to 60 s of polling on a garbage path removed** | 🛡️ 🧪 |
| 🛡️ `file_basename` | discarded `const` via a cast | `const char *` throughout | **`-Wcast-qual` clean** | 🛡️ |
| 🧪 Active unit-test suites | 0 | 66 | **+66** ✅ | 🧪 |
| 🧪 Unit tests / assertions | 0 / 0 | 1,376 / 70,002 | **ALL PASSED** ✅ | 🧪 |
| 🏗️ Host test entry point | none | `make unit-test` | **added** ✅ | 🧪 |

*Call-site counts are scoped to the **25 files under `src/` touched by this port**, obtained by
grepping their contents at `07505ea5` vs. at `HEAD`. They are **not** codebase-wide claims.*

*The three 🛡️ rows at the bottom of the hardening block fix **pre-existing upstream defects**
rather than porting anything from OniOpus46 — see [§4.7](#47-pre-existing-defects-fixed-in-this-branch).
No performance figure is attached to any of them.*

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
| 7 | `HEAD` *(this commit)* | 🛡️ Fix pre-existing hash, save-state and `const` defects; refresh docs | 6 | +1,171 / −476 | fix + test + docs |
| | | **Aggregate `07505ea5..HEAD`** | **101** | **+25,489 / −301** | |
| | | *Code only, excluding `docs/`* | *99* | *+24,222 / −301* | |

> ℹ️ **Of the 7 commits, 5 are hand-written engineering work.**
> [`6da7f28b`](https://github.com/Amiga500/Onion/commit/6da7f28b3a7503a46e0caa799cd85ab9117a0785)
> was generated by the CI formatting workflow on PR #197 (committer
> `GitHub Actions <actions@github.com>`, 2 whitespace lines, zero semantic change), and
> [`971d6169`](https://github.com/Amiga500/Onion/commit/971d6169aa4a0fed355ea89e3a9b35b223598270)
> is the first revision of this documentation. Both are counted in the aggregate but neither
> changes behaviour.

> 🔁 Commit 7 rewrites the two `docs/` files in place, so **1,043 of its 1,171 insertions and
> 459 of its 476 deletions are documentation churn**. Its code-only footprint is **4 files,
> +128 / −17** — `hash.h` (+20 / −10), `gs_popMenu.h` (+10 / −5), `file.c` (+2 / −2) and
> `test/test_hash.c` (+96 / −0).

📈 Split by area:

| Area | Files | +/− | Share of insertions |
|:-----|------:|----:|--------------------:|
| 🧪 `test/` | 73 | +23,070 / −10 | **90.5 %** |
| 🧩 `src/` | 25 | +1,148 / −290 | **4.5 %** |
| 📚 `docs/` | 2 | +1,267 / −0 | **5.0 %** |
| 🏗️ root `Makefile` | 1 | +4 / −1 | **<0.1 %** |

> 📈 **Result:** the production-code surface of this port is deliberately tiny —
> **+1,148 / −290 across 25 files**, i.e. **4.5 %** of the added lines. Everything else
> is test scaffolding and documentation.

---

## ⚡ 2. Performance — NEON Pixel Paths

Commit [`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1b40f6d021a58a41589d6371b30a81142)
adds `src/common/utils/neon_pixel.h` (**301 lines**): 7 inline pixel kernels in ARM NEON
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

### 2.5 ⚠️ What is *not* proven

- ✅ 🧪 The **scalar fallback** of every kernel is verified. `test_neon` (36 tests / 248
  assertions) and `test_neon_pixel` (37 / 71) exercise each function's portable C path against
  hand-computed values, including single-pixel, zero-alpha and non-multiple-of-16 counts.
- ❌ **The NEON assembly itself is not covered by any test.** The host runner is x86-64, so
  `__ARM_NEON` is undefined and only the `#else` scalar branches execute during `make unit-test`.
  Equivalence between the assembly and the scalar path is **assumed**, validated only by
  cross-compilation. This is the single biggest verification gap in the port.
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

### 3.6 `list.h` cache slots — infrastructure only ⚠️

`ListItem` gained six fields for TTF/preview surface caching (`_label_cache`, `_label_hash`,
`_value_cache`, `_cached_value`, `_scaled_preview`, `_scaled_preview_w`), and `list_free()`
now releases them.

> ❌ **No performance claim.** Nothing in `src/` currently reads or writes these fields — the
> OniOpus46 render-side caching that would populate them is **not ported**. The slots and the
> teardown path are in place so the render-side change can land without an ABI churn.
> See [§8](#-8-not-ported-from-oniopus46).

---

## 🛡️ 4. Security & Hardening

Counts below are over the **25 `src/` files touched by this port**, at `07505ea5` vs `HEAD`.

> ⚠️ The scope grew from 23 to 25 files in commit 7, which added `hash.h` and `gs_popMenu.h`.
> Neither file contains `sprintf`, `strcpy`, `strcat`, `strtok` or `system()`, so every
> *reduction* percentage below is unaffected. The **adoption** counts in
> [§4.2](#42-strcpy--strcat--bounded-copies--37--9-757-) do move, because `gs_popMenu.h`
> already used `snprintf` 7 times before the port and `hash.h` gains one `memcpy`.

### 4.1 `sprintf` → `snprintf` — 23 → 1 call sites (**−95.7 %**)

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

Across the ported files `snprintf` went **11 → 34** (net **+23**) while `sprintf` went
**23 → 1**. Of the 11 pre-existing sites, 4 are in the 7 files listed above and 7 are in
`gs_popMenu.h`, which the port never touched — the per-file column above counts *added*
`snprintf` calls, which is why it sums to 25 rather than to the net 23.
`log.c` also fixed an off-by-one (`snprintf(_log_path, 63, …)` → `sizeof(_log_path)`) and
replaced an unbounded `vsprintf` with a length-checked `vsnprintf`.

> 📈 **Result:** **22 of 23** unbounded format calls eliminated (**−95.7 %**). The single
> remaining site is documented in [§10](#️-10-known-residuals).

### 4.2 `strcpy` / `strcat` → bounded copies — 37 → 9 (**−75.7 %**)

| File | removed | bounded added |
|:-----|--------:|--------------:|
| `src/common/utils/file.c` | 7 | 1 |
| `src/common/system/state.h` | 7 | 5 |
| `src/common/system/settings.h` | 6 | 8 |
| `src/common/utils/str.c` | 2 | 0 *(→ `memcpy`)* |
| `src/common/utils/str.h` | 2 | 0 *(→ `snprintf` macro)* |
| `src/common/utils/process.h` | 2 | 1 |
| `src/common/components/list.h` | 2 | 2 |

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

> 📈 **Result:** **28 of 37** unbounded copies eliminated (**−75.7 %**). Because `concat()` is
> a macro, the *real* reduction across the whole codebase is larger than this figure suggests.

### 4.3 `strtok` → `strtok_r` — 4 → 0 (**−100 %**) ✅

All 4 non-reentrant tokenizer call sites — every one of them in `src/common/utils/file.c`,
across `file_readLastLine()` and `file_resolvePath()` — converted to `strtok_r` with a
caller-owned save pointer.

> 📈 **Result:** **−100 %** non-reentrant tokenizer use in the ported files. ✅
> 🛡️ `strtok` keeps hidden global state, so any two callers interleaving (or a signal handler
> tokenising) silently corrupt each other's iteration.

### 4.4 `system()` → POSIX — 3 → 1 (**−66.7 %**)

Covered in [§3.4](#34-system--direct-syscalls--100--process-spawns) — 2 of 3 shell-out sites
removed, eliminating both the process overhead and the path-injection vector. The surviving
site (`process.h:101`) needs a `fork`/`execv` redesign; see [§10](#️-10-known-residuals).

Additionally, `file_remove_recursive()` was added using `nftw(FTW_DEPTH | FTW_PHYS)` as a
shell-free replacement for `rm -rf`.

> ⚠️ It is currently **only consumed by the test suite** — no `src/` call site uses it yet.

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

> ⚠️ The header is **added and unit-tested** (`test_signal_handler`, 10 tests) but the
> applications have **not yet been migrated** to it. OniOpus46 reports **−100 %** duplicated
> signal code across 9 apps; **OnionPlus realises 0 % of that** so far — no call sites were
> removed by this port. See [§8](#-8-not-ported-from-oniopus46).

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
> 🧪 `test_gs_popmenu` (23 tests / 26 assertions) and `test_savestate_path`
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

---

## 🧪 5. Testing

A self-contained host test harness: `test/onion_test.h` (162 lines), `test/Makefile.unit`
(606 lines), `test/Makefile.gtest` (25 lines), `test/README.md` (105 lines) and
**67 new test source files**.

| Metric | Value |
|:-------|------:|
| 🧪 Active suites | **66** |
| ✅ Tests | **1,376** |
| ✅ Assertions | **70,002** |
| ❌ Failures | **0** |
| 🎯 Result | **ALL PASSED** ✅ |
| ⏱️ Clean build + run | **~14.3 s** |
| ⏱️ Run only (prebuilt) | **~2.8 s** |

*Verified by an actual `make unit-test` run on this workspace (x86-64 host, exit code `0`).*

📈 Suite growth across the port:

| Stage | Active suites | Tests | Δ |
|:------|--------------:|------:|--:|
| Baseline `07505ea5` | 0 | 0 | — |
| After `ad402fa2` + `1a1e3f84` | 17 | 583 | **+17 suites** |
| After `300390a7` | **66** | 1,373 | **+288 % suites** |
| Tip `HEAD` | **66** | **1,376** | **+3 tests, +329 assertions** |

The suite count is flat at the tip: commit 7 adds three `RUN_TEST` entries to the existing
`test_hash` suite ([§4.7.1](#471-hashh--bounded-alignment-safe-64-bit-load)) rather than a
new suite file.

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
| `test_alpha_scale` | 20 | 65,827 | §2.3 fixed-point alpha |
| `test_neon_pixel` | 37 | 71 | §2.1 scalar fallbacks |
| `test_neon` | 36 | 248 | §2.1 scalar fallbacks |
| `test_str` | 72 | 357 | §3.1 `str_count_char` |
| `test_file` | 88 | 183 | §3.2 · §3.3 · §3.4 · §3.5 |
| `test_file_security` | 40 | 58 | §3.3 · §3.4 · §3.5 |
| `test_list` | 154 | 260 | §3.6 list bounds |
| `test_perf` | 5 | 5 | §6 `perf.h` |
| `test_hash` | 15 | 350 | §4.7.1 hash bit-identity *(🛡️ not a perf claim)* |

### 5.3 🏆 Largest suites

| Suite | Tests | Assertions |
|:------|------:|-----------:|
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

### 5.4 ⏸️ Deferred suite

`test_images_browser` is present in the tree (67 `test_*.c` files exist, 66 are listed in
`TESTS`) and has a build rule, but is **intentionally excluded**: it depends on
`src/infoPanel/imagesBrowser.c` hardening that has **not** been ported. Run it manually with
`make -f Makefile.unit test_images_browser` once that port lands.

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

> ❌ **Compiler-flag optimizations are not ported.** OniOpus46 reports **−5–15 %** binary size
> from `-ffunction-sections -fdata-sections -Wl,--gc-sections`; the OnionPlus root `Makefile`
> delta is **+4 / −1 lines** and contains only the `unit-test` target. No flag change landed.

---

## 📈 7. Overall Statistics

| Metric | Value |
|:-------|------:|
| 🔧 **Commits** | **7** *(5 hand-written)* |
| 📁 **Files changed** | **101** *(99 excluding `docs/`)* |
| ➕ **Lines added / removed** | **+25,489 / −301** *(+24,222 / −301 excluding `docs/`)* |
| 🧩 **Production code (`src/`)** | **25 files · +1,148 / −290** |
| 🧪 **Test code (`test/`)** | **73 files · +23,070 / −10** |
| 📚 **Documentation (`docs/`)** | **2 files · +1,267** |
| 🆕 **New test source files** | **67** |
| 🧪 **Active suites / tests / assertions** | **66 / 1,376 / 70,002** |
| ✅ **Test result** | **ALL PASSED** *(0 failures)* |
| ⏱️ **Suite runtime** | **~14.3 s** clean · **~2.8 s** prebuilt |
| ⚡ **NEON kernels added** | **8** *(7 asm + 1 intrinsics, all with scalar fallback)* |
| ⚡ **Scalar pixel loops vectorised** | **11** across **6 files** |
| 🚀 **Max single-op speedup** | 📏 **+5000 %** *(NEON 180° rotation, measured on OniOpus46)* |
| 🚀 **`str_count_char`** | 📏 **−90 %** *(O(n²) → O(n))* |
| 🚀 **Shell processes per `mkdirs`/`file_copy`** | 📐 **2 → 0** *(−100 %)* |
| 🛡️ **`sprintf` call sites** | **23 → 1** *(−95.7 %)* |
| 🛡️ **`strcpy`+`strcat` call sites** | **37 → 9** *(−75.7 %)* |
| 🛡️ **`strtok` call sites** | **4 → 0** *(−100 %)* |
| 🛡️ **`system()` call sites** | **3 → 1** *(−66.7 %)* |
| 🛡️ **NULL-check predicates added** | **57** |
| 🛡️ **`fclose`/`close` added** | **18** |
| 🛡️ **Pre-existing upstream defects fixed** | **3** *(hash over-read + unaligned load, uninitialised save-state path, `const` cast — [§4.7](#47-pre-existing-defects-fixed-in-this-branch))* |
| 🔐 **Hash values changed** | **0** *(bit-identical, 264 reference vectors at 5 optimization levels)* |
| 🏗️ **New build target** | **`make unit-test`** |
| 📉 **Failing tests at tip** | **0 / 1,376** |

Reproduce every figure above with:

```bash
git diff --shortstat 07505ea5..HEAD
git diff --numstat  07505ea5..HEAD
git diff --shortstat 07505ea5..HEAD -- src/ test/ docs/ Makefile
make unit-test
```

---

## 🚫 8. Not Ported from OniOpus46

For honesty, these OniOpus46 optimizations are **absent** from OnionPlus today. Their
percentages appear **nowhere** in the tables above.

| OniOpus46 optimization | OniOpus46 claim | OnionPlus status |
|:---|:---|:---|
| TTF surface caching (footer, header, dialog, labels) | 📏 5–15 ms/frame saved | ❌ Only the `ListItem` cache **slots** exist ([§3.6](#36-listh-cache-slots--infrastructure-only-️)); nothing populates them |
| SQLite open/close 2 → 1 per operation | 📏 −50 % DB I/O | ❌ `playActivityDB` not touched |
| OSD busy-wait 100 µs → 16 ms | 📏 idle CPU ~10 % → <1 % | ❌ `osd.h` not touched |
| Display brightness sysfs caching | 📏 −100 % duplicate writes | ❌ `display.h` not touched |
| Build flags `--gc-sections` etc. | 📏 −5–15 % binary size | ❌ Root `Makefile` delta is the `unit-test` target only |
| `system()` → `fork`+`exec` (dialog bg, GS overlay) | 📏 −80 % process overhead | ❌ Not ported *(the unrelated `mkdirs`/`file_copy` syscall rewrite **is** — [§3.4](#34-system--direct-syscalls--100--process-spawns))* |
| Volume logarithmic curve | perceptual mapping | ❌ Not ported *(`test_volume` runs against unported reference logic)* |
| Signal-handler dedup across 9 apps | 📏 −100 % duplication | ⚠️ Header ported, **call sites not migrated** ([§4.6](#46-code-deduplication--signal_handlerh)) |
| `str_replace` `strlen` caching | 📏 −50 % scan | ⚠️ Not applicable — the OnionPlus rewrite is 🛡️ overflow hardening; the scan count is unchanged |
| `file_remove_recursive` replacing `rm -rf` call sites | 📏 15+ injection points | ⚠️ Function ported, **no `src/` call site uses it yet** |
| `infoPanel` / `imagesBrowser` hardening | — | ❌ Not ported; `test_images_browser` stays disabled |

---

## 🔬 9. Methodology & Limits

### ✅ What is measured *here*

| Figure | Method |
|:---|:---|
| Line / file counts | `git diff --stat 07505ea5..HEAD`, working tree included. **Exact.** |
| Call-site counts | Pattern occurrences in the 25 ported `src/` files at `07505ea5` vs `HEAD`. Both endpoints stated so the delta is checkable. Scoped to ported files only. |
| Test results | A real `make unit-test` run on this workspace: 66 suites, 1,376 tests, 70,002 assertions, 0 failures, exit `0`. |
| Suite runtime | `time make unit-test` — 14.3 s from clean, 2.8 s with binaries prebuilt. Host is x86-64. |
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
- ❌ **NEON assembly is not exercised by the host test run.** The host is x86-64, so
  `__ARM_NEON` is undefined and the scalar fallbacks are what execute.
- ❌ **No runtime testing on target hardware.** The code compiles and passes host tests;
  end-to-end device behaviour has not been validated.
- ⚠️ **Sanitizer coverage is partial, not systematic.** ASan + UBSan were run over a subset of
  the suite (11 suites, plus `test_hash` against both revisions of `hash.h`) while
  investigating [§4.7](#47-pre-existing-defects-fixed-in-this-branch). The default
  `make unit-test` build carries **no** sanitizer flags, and no Valgrind or static-analysis
  tool (coverity, clang-tidy, cppcheck) has been run over the branch.
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
- **Expect a "7 vs 4" discrepancy against the GitHub release notes.** The generated notes for
  the `latest` tag list only 4 commits, because
  [`d7aed5a1`](https://github.com/Amiga500/Onion/commit/d7aed5a1b40f6d021a58a41589d6371b30a81142)
  had already shipped in the previous release and is outside that tag's comparison range, and
  the docs commits landed after. This report scopes to the full `07505ea5..HEAD` delta.
- **The tip commit is inside the range it measures.** It carries the defect fixes, the three
  new hash tests *and* this document, so every aggregate figure includes 1,043 insertions of
  documentation churn. The code-only subset is stated next to each aggregate.

---

## ⚠️ 10. Known Residuals

For transparency, these unsafe patterns **remain** at `HEAD`. They are outside the scope
of this port, not oversights the numbers above conceal.

| File:line | Pattern | Note |
|:----------|:--------|:-----|
| `src/common/system/screenshot.h:23` | `strcpy` | Fixed literal prefix `/mnt/SDCARD/Screenshots/` into `path_out`. |
| `src/common/system/screenshot.h:32,37,39,57,61` | `strcat` ×5 | Screenshot path assembly — also still O(n²). |
| `src/common/system/screenshot.h:52` | `strcpy` | `app_name` from `no_extension`. |
| `src/common/system/screenshot.h:65` | `sprintf` | `"_%03d.png"` suffix — bounded in practice by the format, not by construction. |
| `src/jpg2png/jpg2png.c:137,141` | `strcpy` + `strcat` | `argv[1]` copied into `char filename[256]` then `".png"` appended, no length check. **Reachable with a long CLI argument.** |
| `src/common/utils/process.h:101` | `system(cmd)` | `process_start_*` still shells out; replacing it needs a `fork`/`execv` redesign. |

🎯 Recommended follow-ups, in priority order:

1. 🔴 Bound the `jpg2png.c` argument copy (`snprintf(filename, sizeof(filename), "%s.png", argv[1])`) — the most directly attacker-influenced residual.
2. 🟠 Convert `screenshot.h` path assembly to a single `snprintf` (fixes 7 residuals + the quadratic scan at once).
3. 🟡 Migrate the applications to `signal_handler.h` to realise the dedup in [§4.6](#46-code-deduplication--signal_handlerh).
4. 🟡 Wire `file_remove_recursive()` into the `src/` call sites that still shell out.
5. 🟢 Port the render-side TTF/preview caching so the `ListItem` slots in [§3.6](#36-listh-cache-slots--infrastructure-only-️) do something.
6. 🟢 Port the `infoPanel` hardening so `test_images_browser` can be re-enabled (66 → 67 active suites).
7. 🔵 Add a cross-compiled or QEMU-based run so the NEON assembly is actually executed under test.
8. 🔵 **Take baseline timings on device** for `jpg2png`, `pngScale`, `rotate180` and screenshot capture, so every 📏 in this document can be upgraded to a real OnionPlus measurement.

---

## ✅ Final Status

OnionPlus is **7 commits ahead** of upstream `OnionUI/Onion:main` (`07505ea5`), adding
**8 NEON pixel kernels**, crash/memory hardening of the `src/common` layer, four algorithmic
wins in `str`/`file`, fixes for **3 pre-existing upstream defects**, and a **66-suite host
unit-test harness** runnable with a single `make unit-test`.

> 🧪 **66 suites · 1,376 tests · 70,002 assertions · 0 failures.** ✅
>
> Note: the baseline `07505ea5` had **no** host test suite, so this is a new quality floor
> rather than a "no regressions" comparison — there is nothing to compare against upstream.

The hardening is **partial by design**: it covers the shared `src/common` utilities plus the
two image tools, with the remaining unsafe call sites enumerated in
[§10](#️-10-known-residuals) rather than rounded away. Performance figures marked **📏** are
**OniOpus46 measurements on identical code**, reproduced here for context and clearly
separated from **📐 analytical** figures. **No OnionPlus on-device measurement has been taken** —
treat every speedup as inherited evidence until benchmarked on real hardware.

The three defect fixes in [§4.7](#47-pre-existing-defects-fixed-in-this-branch) carry **no
performance claim at all**. The `hash.h` rewrite in particular is valuable precisely because it
changes **nothing** observable: same hash values, same generated load on the fast path, minus a
7-byte over-read, minus three classes of undefined behaviour, and minus an allocation
convention that callers had to remember on their own.

---

📊 See also: **[OnionPlus-vs-base.md](./OnionPlus-vs-base.md)** — full diff statistics vs. the base release.

<sub>Repository: [Amiga500/Onion](https://github.com/Amiga500/Onion) · Branch: `OnionPlus` ·
Base: [`07505ea5`](https://github.com/Amiga500/Onion/commit/07505ea5) → Tip: `HEAD` *(7th commit)* ·
Style adapted from the OniOpus46 [`OPTIMIZATION.md`](https://github.com/Amiga500/Onion/blob/OniOpus46/docs/OPTIMIZATION.md) ·
Commits analyzed: **7** · Date: 2026-08-20</sub>
