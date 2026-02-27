# Porting Analysis: Onion OS → Miyoo Flip (Rockchip RK3566)

## Executive Summary

This document provides a detailed analysis for porting Onion OS from the current
Miyoo Mini / Mini+ platform (Allwinner/SigmaStar ARMv7 Cortex-A7) to the
**Miyoo Flip** handheld, powered by a **Rockchip RK3566** (quad-core ARM
Cortex-A55, ARMv8-A/AArch64) with **1 GB RAM**.

The porting effort is **medium-high complexity**. Approximately 80 % of the C
source code is platform-agnostic (UI logic, game-list management, JSON parsing,
utilities). The remaining ~20 % contains hardware-specific paths, proprietary
ioctl calls, inline ARM assembly, and pre-built 32-bit binaries that require
modification or replacement.

---

## 1. Hardware Comparison

| Feature | Miyoo Mini / Mini+ | Miyoo Flip (RK3566) |
|---|---|---|
| **SoC** | SigmaStar SSD202D / SSD212 | Rockchip RK3566 |
| **CPU** | Single/Dual Cortex-A7 @ 1.2 GHz | Quad Cortex-A55 @ 1.8 GHz |
| **Architecture** | ARMv7-A (32-bit) | ARMv8-A (64-bit / AArch64) |
| **SIMD** | NEON-VFPv4 | NEON (ARMv8 Advanced SIMD) |
| **GPU** | None (software rendering) | Mali-G52 2EE |
| **RAM** | 128 MB DDR2 | 1 GB DDR4 |
| **Display** | 640×480 IPS (landscape) | ~640×480 or 720×720 (TBD) |
| **Audio** | Proprietary MI_AO driver | ALSA / PulseAudio (standard) |
| **Power IC** | AXP Power IC (I2C 0x34) | RK817 PMIC (standard Linux) |
| **OS Kernel** | Linux 3.x / 4.x (vendor) | Linux 4.19+ / 5.x (Rockchip BSP) |

**Key advantages of RK3566:**
- 4× CPU cores, ~3× single-thread performance (A55 vs A7)
- 8× RAM (1 GB vs 128 MB) — eliminates memory constraints
- Mali-G52 GPU available for OpenGL ES 3.2 / Vulkan
- Standard Linux driver stack (ALSA, DRM/KMS, evdev)
- Active mainline Linux support (5.x+)

---

## 2. Architecture Analysis: What Needs to Change

### 2.1 Build System (`src/common/config.mk`, top-level `Makefile`)

**Current state:**
```makefile
# Platform-specific flags (miyoomini)
CFLAGS := -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
```

**Required changes:**
- Add a new `PLATFORM=miyooflip` target in `config.mk`
- AArch64 compiler flags: `-march=armv8-a -mtune=cortex-a55`
- New Docker toolchain image or Buildroot SDK for RK3566 (AArch64)
- Update `CROSS_COMPILE` prefix for aarch64 toolchain (e.g. `aarch64-linux-gnu-`)
- Library path may change from `/mnt/SDCARD/.tmp_update/lib`

**Effort: Low** — Only `config.mk` and `Makefile` need changes.

### 2.2 Display Subsystem (`src/common/system/display.h`)

**Current state:**
- Direct `/dev/fb0` framebuffer access via `mmap` (32-bit ARGB)
- Hardcoded default 640×480 for `PLATFORM_MIYOOMINI`
- Screen on/off via GPIO4 (`/sys/class/gpio/`)
- Brightness via PWM (`/sys/devices/soc0/soc/1f003400.pwm/pwm/pwmchip0/`)
- `display_drawFrame()` hardcoded for 640-pixel stride

**Required changes:**
- RK3566 typically uses **DRM/KMS** instead of legacy fbdev
  - Option A: Use DRM/KMS directly (modern, recommended)
  - Option B: Enable `CONFIG_FB_SIMPLE` or `simplefb` in kernel for fbdev compat
- Update GPIO path for display backlight (RK3566 uses different GPIO controller)
- Update PWM path for brightness (RK3566 PWM is at different sysfs path)
- Detect and handle display resolution dynamically (may differ)
- Remove hardcoded 640-pixel stride in `display_drawFrame()`

**Effort: Medium** — Framebuffer access is abstracted, but GPIO/PWM paths are
hardcoded. DRM/KMS migration would be ideal but optional if fbdev compat layer
is available.

### 2.3 Audio Subsystem (`src/common/system/volume.h`)

**Current state:**
- **Completely proprietary**: Uses `/dev/mi_ao` device with custom ioctl codes
  (`MI_AO_SETVOLUME = 0x4008690b`, `MI_AO_GETVOLUME = 0xc008690c`)
- Volume range: -60 to +30 dB mapped to 0-20 levels
- SDL_mixer used for game audio (48 kHz, stereo)

**Required changes:**
- Replace `/dev/mi_ao` with **ALSA** mixer control (`amixer` / `libasound`)
- RK3566 uses standard ALSA audio drivers
- Rewrite `setVolumeRaw()` and `setVolume()` to use ALSA API:
  ```c
  // Example: ALSA mixer control
  snd_mixer_t *handle;
  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, "default");
  snd_mixer_selem_register(handle, NULL, NULL);
  snd_mixer_load(handle);
  // ... set volume via snd_mixer_selem_set_playback_volume_all()
  ```
- Alternative: Use `amixer` command-line for simplicity

**Effort: Medium** — Complete rewrite of volume control, but standard ALSA
patterns are well-documented.

### 2.4 Power Management (`src/common/system/axp.h`, `battery.h`)

**Current state:**
- AXP PMIC accessed via I2C (`/dev/i2c-1`, address `0x34`)
- Battery percentage read from `/tmp/percBat` (written by `batmon` daemon)
- Charging state detected via GPIO59 (Miyoo283) or `axp_test` subprocess (Miyoo354)
- CPU frequency scaling via `/sys/devices/system/cpu/cpufreq/policy0/`

**Required changes:**
- RK3566 uses **RK817 PMIC** — different I2C address and registers
- Battery info available via standard Linux `power_supply` sysfs:
  ```
  /sys/class/power_supply/battery/capacity        → percentage
  /sys/class/power_supply/battery/status           → Charging/Discharging
  /sys/class/power_supply/battery/voltage_now      → voltage in µV
  ```
- CPU frequency scaling paths are compatible (`cpufreq/policy0/`)
  but RK3566 has 4 policies (one per core or cluster)
- The `system.h` PWM path (`/sys/devices/soc0/soc/1f003400.pwm/...`) needs
  updating to RK3566's PWM controller path

**Effort: Medium** — Standard Linux power_supply interface is simpler than the
current AXP I2C driver. CPU scaling is mostly compatible.

### 2.5 Input Subsystem (`src/common/system/keymap_hw.h`)

**Current state:**
- Linux evdev input via `/dev/input/event0`
- 26 buttons mapped to Linux `KEY_*` constants
- Button layout: D-pad, A/B/X/Y, L1/R1/L2/R2, Select/Start/Menu/Power, Volume

**Required changes:**
- **Input device path may differ** — enumerate `/dev/input/event*` instead of
  hardcoding `event0`
- **Key code mapping** — Miyoo Flip's button-to-keycode mapping will be
  different; update the `hw_*` constants in `keymap_hw.h`
- The evdev API itself is standard Linux and remains compatible

**Effort: Low** — Only key code values need updating; the evdev infrastructure
is identical.

### 2.6 NEON/SIMD Code (`src/common/utils/neon_pixel.h`)

**Current state:**
- ARM NEON inline assembly using ARMv7 (A32) syntax:
  ```asm
  "vld4.8  {d0-d3}, [%[src]]!"    // Load 16 ARGB pixels
  "vst4.8  {d0-d3}, [%[src]]!"    // Store 16 ARGB pixels
  "pld     [%[src], #192]"         // Prefetch
  ```
- Guarded by `#ifdef __ARM_NEON`
- Functions: `neon_swap_rb_inplace()`, `neon_argb_to_rgba()`, `neon_rotate180_inplace()`,
  etc.

**Required changes:**
- **AArch64 uses different NEON assembly syntax** — A32 instructions (`vld4.8`,
  `vst4.8`, `vswp`, `vrev64`) are invalid in AArch64 mode
- Options:
  1. **Recommended**: Rewrite using **ARM NEON intrinsics** (`arm_neon.h`),
     which are portable across ARMv7 and AArch64:
     ```c
     #include <arm_neon.h>
     uint8x16x4_t pixels = vld4q_u8(src);  // Load 16 ARGB pixels
     uint8x16_t tmp = pixels.val[0];        // Swap R and B
     pixels.val[0] = pixels.val[2];
     pixels.val[2] = tmp;
     vst4q_u8(src, pixels);                 // Store back
     ```
  2. Write AArch64 assembly equivalents (more effort, less portable)
  3. Use scalar fallback only (slower but functional)

**Effort: Medium** — Intrinsics rewrite is straightforward; the current code
already has scalar fallbacks.

### 2.7 Platform-Specific Libraries

#### 2.7.1 `libshmvar.so` (Shared Memory Variables)
- **Miyoo-specific** IPC for settings sync (volume, brightness, hue, etc.)
- Used only when `PLATFORM_MIYOOMINI` is defined
- **For RK3566**: Can be replaced with standard Linux IPC (POSIX shm, dbus),
  or omitted if settings are managed differently

#### 2.7.2 `libgfx.so` (Graphics Framework)
- Wraps Miyoo's **proprietary MI-SYS/MI-GFX** hardware blitter (`<mi_sys.h>`,
  `<mi_gfx.h>`)
- Used for hardware-accelerated 2D blitting to framebuffer
- **For RK3566**: Replace with:
  - SDL2 rendering (recommended — also enables GPU acceleration via Mali-G52)
  - DRM/KMS dumb buffer blitting
  - Or simply software blitting (the 4× CPU performance makes this viable)

#### 2.7.3 `/dev/mi_ao` (Audio Output)
- Proprietary audio device — see §2.3 above

#### 2.7.4 `/dev/sar` (ADC for Battery)
- SigmaStar-specific SAR ADC — replace with `power_supply` sysfs (§2.4)

**Effort: Medium-High** — `libgfx` and `libshmvar` need complete replacement.

### 2.8 Pre-Built Binaries (`static/build/.tmp_update/bin/`)

**Current state:** 30+ pre-compiled ARMv7 (32-bit) ELF binaries:
- `MainUI-283-expert`, `MainUI-354-expert` (main UI variants)
- `retroarch`, `retroarch_miyoo354` (emulator frontend)
- System utilities: `7z`, `curl`, `wget`, `ffmpeg`, `jq`, `sqlite3`
- Standalone emulators: `scummvm`, `drastic`, etc.

**Required changes:**
- **All binaries must be recompiled for AArch64**
- RetroArch and LibRetro cores: Rebuild from source with RK3566 toolchain
  (RetroArch officially supports AArch64)
- System utilities: Cross-compile or use Buildroot/Yocto packages
- MainUI: This is a closed-source Miyoo firmware component — needs replacement
  or reimplementation

**Effort: High** — This is the single largest effort item.

### 2.9 Shell Scripts and Runtime System

**Current state:**
- `runtime.sh` orchestrates boot, game launching, and services
- References Miyoo-specific paths and device detection
- Library paths: `LD_LIBRARY_PATH` and `LD_PRELOAD` for `libpadsp.so`

**Required changes:**
- Update device detection logic (new DEVICE_ID for Miyoo Flip)
- Update library paths for AArch64
- Remove `LD_PRELOAD` of `libpadsp.so` (Miyoo-specific audio routing)
- Update RetroArch binary selection logic

**Effort: Low-Medium** — Scripts are straightforward to adapt.

### 2.10 SDL Version

**Current state:** SDL 1.2 (legacy)

**Consideration for RK3566:**
- SDL 1.2 works but does not leverage the Mali-G52 GPU
- **Recommended**: Migrate to **SDL2**, which supports:
  - DRM/KMS display backend
  - OpenGL ES hardware acceleration
  - Modern audio backends (ALSA, PulseAudio)
  - Better gamepad support
- This is a significant effort but unlocks GPU performance

**Effort: High** (if upgrading to SDL2) / **Low** (if keeping SDL 1.2)

---

## 3. Effort Estimation

| Component | Files Affected | Effort | Priority |
|---|---|---|---|
| Build system / toolchain | `config.mk`, `Makefile` | Low | P0 — First |
| Device model detection | `device_model.h` | Low | P0 |
| Compiler flags (AArch64) | `config.mk` | Low | P0 |
| Input keymapping | `keymap_hw.h` | Low | P1 |
| CPU frequency scaling | `system.h` | Low | P1 |
| Display (fbdev/DRM) | `display.h`, `sdl_direct_fb.h` | Medium | P1 |
| Battery / Power | `battery.h`, `axp.h` | Medium | P1 |
| NEON SIMD intrinsics | `neon_pixel.h` | Medium | P2 |
| Audio (ALSA replacement) | `volume.h` | Medium | P1 |
| Display brightness/GPIO | `display.h`, `system.h` | Medium | P1 |
| Shell scripts / runtime | `runtime.sh`, launch scripts | Medium | P2 |
| `libshmvar` replacement | `settings_sync.h`, `osd.h` | Medium | P2 |
| `libgfx` replacement | Graphics pipeline | Medium-High | P2 |
| Pre-built binaries | `static/` | High | P3 |
| RetroArch + cores rebuild | `third-party/` | High | P3 |
| SDL2 migration (optional) | All UI code | High | P4 — Optional |

**Total estimated effort:** 4-8 developer-weeks for a functional port,
depending on familiarity with the RK3566 BSP.

---

## 4. Recommended Porting Strategy

### Phase 1: Foundation (Week 1-2)
1. **Set up RK3566 cross-compilation toolchain** (Buildroot or vendor SDK)
2. **Add `PLATFORM=miyooflip` to build system** (config.mk)
3. **Add `MIYOOFLIP` device model** (device_model.h)
4. **Compile all C sources** — fix AArch64 compilation errors
5. **Replace NEON assembly** with portable intrinsics
6. **Stub out hardware-specific code** behind `#ifdef PLATFORM_MIYOOFLIP`

### Phase 2: Hardware Abstraction (Week 2-4)
1. **Display**: Implement DRM/KMS or fbdev framebuffer access
2. **Audio**: Implement ALSA volume control
3. **Battery**: Implement `power_supply` sysfs reading
4. **Input**: Map Miyoo Flip button codes
5. **Brightness**: Identify and configure PWM/backlight sysfs path

### Phase 3: Runtime (Week 4-6)
1. **Cross-compile RetroArch** for AArch64 RK3566
2. **Cross-compile LibRetro cores** (priority systems first)
3. **Adapt shell scripts** for new platform
4. **Cross-compile system utilities** (curl, 7z, ffmpeg, etc.)

### Phase 4: Optimization (Week 6-8)
1. **GPU acceleration** (Mali-G52 via OpenGL ES or Vulkan)
2. **SDL2 migration** for modern rendering pipeline
3. **Performance tuning** (CPU governor, thermal management)
4. **Testing** across emulator cores

---

## 5. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| MainUI is closed-source, cannot port | High | High | Reimplement UI or use RetroArch RGUI |
| Miyoo Flip kernel lacks fbdev support | Medium | Medium | Use DRM/KMS or request vendor kernel config |
| LibRetro cores fail on AArch64 | Low | Medium | Most cores support AArch64; test individually |
| `libshmvar` deeply coupled | Low | Low | Only used with `#ifdef PLATFORM_MIYOOMINI` |
| Display rotation differs | Medium | Low | Already has rotation support in display code |

---

## 6. Build System Changes (Implemented)

The following scaffolding has been added to the codebase to support the new
platform:

### `src/common/config.mk`
New `miyooflip` platform block with AArch64 compiler flags.

### `src/common/system/device_model.h`
New `MIYOOFLIP` device constant (ID: 566, referencing the RK3566 SoC).

### `src/common/system/display.h`
Platform-aware default resolution for Miyoo Flip.

### `src/common/system/volume.h`
Stub for ALSA-based volume control on RK3566.

### `src/common/system/battery.h`
Stub for `power_supply` sysfs battery reading on RK3566.

### `src/common/system/system.h`
Platform-aware sysfs paths for RK3566 GPIO/PWM/CPU.

---

## 7. Conclusion

Porting Onion OS to the Miyoo Flip (RK3566) is **feasible and worthwhile**.
The RK3566's significantly higher performance (4× cores, 8× RAM, GPU) would
enable running more demanding emulators and a smoother user experience.

The codebase is **moderately well-abstracted** — most hardware access is
concentrated in `src/common/system/` headers, making the porting surface
manageable. The main challenges are:

1. **AArch64 migration** (32-bit → 64-bit)
2. **Proprietary driver replacement** (MI_AO, MI_GFX, AXP → ALSA, DRM, power_supply)
3. **Pre-built binary rebuilding** (RetroArch, cores, utilities)

The recommended approach is an **incremental port**: get the build compiling,
then replace hardware interfaces one at a time, testing on real hardware at
each stage.
