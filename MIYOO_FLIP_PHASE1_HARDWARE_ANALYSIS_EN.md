# Phase 1 – Hardware Analysis and Differences: Miyoo Mini+ vs Miyoo Flip

**Analysis Date:** February 2026  
**Onion OS Version:** 4.4.0-beta  
**Scope:** Detailed hardware comparison with file mapping for modifications

---

## Table of Contents

1. [Hardware Summary](#1-hardware-summary)
2. [CPU/GPU/Kernel Architecture](#2-cpugpukernel-architecture)
3. [Display and Touchscreen](#3-display-and-touchscreen)
4. [Input System](#4-input-system)
5. [Power Management & Lid Sensor](#5-power-management--lid-sensor)
6. [Audio, PWM and Vibration](#6-audio-pwm-and-vibration)
7. [Storage](#7-storage)
8. [WiFi and Bluetooth](#8-wifi-and-bluetooth)
9. [File Summary Table](#9-file-summary-table)
10. [Modification Complexity Estimate](#10-modification-complexity-estimate)

---

## 1. Hardware Summary

### 1.1 Miyoo Mini+ (Current)

| Component | Specification |
|-----------|---------------|
| **SoC** | Allwinner F1C500s (ARM926EJ-S, single-core @ 900 MHz) |
| **CPU** | ARM926EJ-S (ARMv5TE architecture) |
| **GPU** | No dedicated GPU (software rendering) |
| **RAM** | 64-128 MB DDR2 |
| **Display** | 2.8" IPS LCD, 640×480, single screen |
| **Touchscreen** | None |
| **Input** | D-pad, 4 face buttons (A/B/X/Y), 4 shoulder (L1/L2/R1/R2), Start, Select, Menu |
| **Analog Sticks** | None |
| **PMU** | AXP209 or AXP173 (I2C @ 0x34) |
| **Audio** | Integrated audio codec, mono speaker |
| **Vibration** | GPIO48 (motor controller) |
| **Storage** | Single MicroSD |
| **WiFi** | None (some models with USB dongle) |
| **Bluetooth** | None |
| **Form Factor** | Candybar |
| **Battery** | Li-ion 2000 mAh |

### 1.2 Miyoo Flip (Target)

| Component | Specification |
|-----------|---------------|
| **SoC** | Rockchip RK3566 (quad-core Cortex-A55 @ 1.8-2.0 GHz) |
| **CPU** | 4× ARM Cortex-A55 (ARMv8.2-A architecture, 64-bit) |
| **GPU** | Mali-G52 2EE (OpenGL ES 3.2, Vulkan 1.1) |
| **RAM** | 1 GB LPDDR4 |
| **Display** | Dual IPS LCD: Internal 3.5" 640×480 + External 1.5" 240×240 |
| **Touchscreen** | None confirmed |
| **Input** | D-pad, 4 face buttons, 4 shoulder, Start, Select, Menu + **2 analog sticks** |
| **Analog Sticks** | Left stick (L3) + Right stick (R3) with click |
| **PMU** | RK809 or RK817 (I2C, integrated in RK3566) |
| **Audio** | RK809/RK817 codec, stereo speakers |
| **Vibration** | Linear resonant actuator (LRA) or ERM motor |
| **Storage** | **Dual MicroSD** (System + User) or internal eMMC + MicroSD |
| **WiFi** | 802.11 b/g/n (integrated chip or module) |
| **Bluetooth** | BT 4.2/5.0 (possible) |
| **Form Factor** | **Clamshell** (fold-open, lid sensor) |
| **Battery** | Li-ion 3000-4000 mAh |

---

## 2. CPU/GPU/Kernel Architecture

### 2.1 Main Differences

| Aspect | Miyoo Mini+ | Miyoo Flip | Impact |
|--------|-------------|------------|--------|
| **ISA** | ARMv5TE (32-bit) | ARMv8.2-A (64-bit) | ⚠️ CRITICAL |
| **Cores** | 1 core @ 900 MHz | 4 cores @ 1.8-2.0 GHz | ⚠️ HIGH |
| **CPU Arch** | ARM926EJ-S (in-order) | Cortex-A55 (out-of-order) | ⚠️ HIGH |
| **GPU** | None (software FB) | Mali-G52 2EE (HW accel) | ⚠️ CRITICAL |
| **Vendor** | Allwinner | Rockchip | ⚠️ CRITICAL |
| **Kernel** | Linux 3.4.x-4.14.x (sunxi) | Linux 5.10+ (rockchip) | ⚠️ CRITICAL |
| **Bootloader** | U-Boot sunxi | U-Boot rockchip | ⚠️ HIGH |

### 2.2 Onion OS Files to Modify

#### Build System

| File | Required Modification | Priority |
|------|----------------------|----------|
| `Makefile` | Add `PLATFORM_MIYOOFLIP` target, aarch64 toolchain | 🔴 HIGH |
| `src/common/arm_flags.mk` | New CPU flags: `-march=armv8-a -mcpu=cortex-a55` | 🔴 HIGH |
| `src/common/config.mk` | Define `PLATFORM_MIYOOFLIP` | 🔴 HIGH |
| `src/common/system/device_model.h` | Add `#define MIYOOFLIP 640` | 🔴 HIGH |

---

## 3. Display and Touchscreen

### 3.1 Main Differences

| Aspect | Miyoo Mini+ | Miyoo Flip | Impact |
|--------|-------------|------------|--------|
| **Number of screens** | 1 (single internal) | 2 (internal 3.5" + external 1.5") | ⚠️ HIGH |
| **Internal resolution** | 640×480 | 640×480 | ✅ IDENTICAL |
| **Interface** | Framebuffer `/dev/fb0` | MIPI DSI + framebuffer | ⚠️ HIGH |
| **Driver** | sunxi-disp (Allwinner) | rockchip-drm (DRM/KMS) | ⚠️ HIGH |
| **GPU Accel** | None | Mali-G52 (OpenGL ES, Vulkan) | ⚠️ HIGH |

### 3.2 Files to Modify

| File | Required Modification | Priority |
|------|----------------------|----------|
| `src/common/system/display.h` | Dual-display support (`/dev/fb0`, `/dev/fb1`) | 🔴 HIGH |
| `src/common/system/display.h` | DRM/KMS handling | 🔴 HIGH |

---

## 4. Input System

### 4.1 Main Differences

| Aspect | Miyoo Mini+ | Miyoo Flip | Impact |
|--------|-------------|------------|--------|
| **D-pad** | ✅ 4 directions | ✅ 4 directions | ✅ IDENTICAL |
| **Analog sticks** | ❌ None | ✅ **Left + Right stick** | ⚠️ CRITICAL |
| **Event types** | `EV_KEY` (digital) | `EV_KEY` + **`EV_ABS`** (analog) | ⚠️ HIGH |

### 4.2 Files to Modify

| File | Required Modification | Priority |
|------|----------------------|----------|
| `src/common/system/keymap_hw.h` | Add analog axes definitions | 🔴 HIGH |
| `src/keymon/input_fd.h` | Handle `EV_ABS` events | 🔴 HIGH |
| `src/keymon/keymon.c` | Analog event support in main loop | 🔴 HIGH |

---

## 5. Power Management & Lid Sensor

### 5.1 Main Differences

| Aspect | Miyoo Mini+ | Miyoo Flip | Impact |
|--------|-------------|------------|--------|
| **PMU Chip** | AXP209 / AXP173 | RK809 / RK817 | ⚠️ CRITICAL |
| **Lid Sensor** | ❌ None | ✅ Hall effect / GPIO | ⚠️ HIGH |

### 5.2 Files to Modify

| File | Required Modification | Priority |
|------|----------------------|----------|
| `src/common/system/axp.h` | **Complete rewrite** for RK809/RK817 | 🔴 CRITICAL |
| `src/common/system/battery.h` | Adapt for RK809 registers | 🔴 HIGH |
| `src/keymon/keymon.c` | Integrate lid sensor monitoring | 🔴 HIGH |

### 5.3 New Files to Create

| File | Purpose | Priority |
|------|---------|----------|
| `src/common/system/rk809.h` | RK809/817 PMU driver | 🔴 P0 |
| `src/common/system/lid_sensor.h` | Lid sensor handling | 🔴 P0 |

---

## 6. Audio, PWM and Vibration

### 6.1 Differences

| Aspect | Miyoo Mini+ | Miyoo Flip | Impact |
|--------|-------------|------------|--------|
| **Audio Codec** | Allwinner integrated | RK809/RK817 codec | ⚠️ HIGH |
| **Speakers** | Mono | **Stereo** | 🟡 MEDIUM |
| **Vibration** | GPIO48 (on/off) | PWM controller (intensity) | 🟡 MEDIUM |

### 6.2 Files to Modify

| File | Required Modification | Priority |
|------|----------------------|----------|
| `src/common/system/rumble.h` | Rewrite for PWM | 🟡 MEDIUM |

---

## 7. Storage

### 7.1 Differences

| Aspect | Miyoo Mini+ | Miyoo Flip | Impact |
|--------|-------------|------------|--------|
| **MicroSD Slots** | 1 (single) | **2 (dual)** or 1 + eMMC | ⚠️ MEDIUM |

---

## 8. WiFi and Bluetooth

### 8.1 Differences

| Aspect | Miyoo Mini+ | Miyoo Flip | Impact |
|--------|-------------|------------|--------|
| **WiFi** | ❌ None | ✅ 802.11 b/g/n | 🟢 BONUS |
| **Bluetooth** | ❌ None | ✅ BT 4.2/5.0 | 🟢 BONUS |

**Note:** WiFi/BT are extra features, not blocking for base porting.

---

## 9. File Summary Table

### Critical Files to Modify

| File | Modification Type | Complexity | Priority |
|------|-------------------|------------|----------|
| `Makefile` | Major update | ⚠️ HIGH | 🔴 P0 |
| `src/common/system/device_model.h` | Major update | 🟡 MEDIUM | 🔴 P0 |
| `src/common/system/keymap_hw.h` | Major update | ⚠️ HIGH | 🔴 P0 |
| `src/common/system/axp.h` | **REWRITE** | ⚠️ CRITICAL | 🔴 P0 |
| `src/common/system/display.h` | Major update | ⚠️ HIGH | 🔴 P0 |
| `src/common/system/battery.h` | Major update | ⚠️ HIGH | 🔴 P0 |
| `src/keymon/input_fd.h` | Major update | ⚠️ HIGH | 🔴 P0 |
| `src/keymon/keymon.c` | Major update | ⚠️ HIGH | 🔴 P0 |

---

## 10. Modification Complexity Estimate

### Classification by Area

| Area | # Critical Files | LOC Estimate | Effort (days) | Risk |
|------|------------------|--------------|---------------|------|
| **Build System** | 5-10 | 500 | 3-5 | 🟡 MEDIUM |
| **Display/GPU** | 3-5 | 2000+ | 15-20 | 🔴 HIGH |
| **Input (Analog)** | 3-5 | 1000 | 7-10 | 🔴 HIGH |
| **Power/PMU** | 5-8 | 1500 | 10-15 | 🔴 CRITICAL |
| **Lid Sensor** | 2-3 | 500 | 3-5 | 🟡 MEDIUM |
| **Audio/Vibration** | 2-3 | 300 | 2-3 | 🟢 LOW |
| **Storage** | 5-10 | 200 | 2-3 | 🟢 LOW |
| **Testing/Debug** | N/A | N/A | 20-30 | 🔴 HIGH |
| **TOTAL** | **25-47** | **6500+** | **67-98 days** | 🔴 HIGH |

### Detailed Timeline

| Phase | Activity | Duration |
|-------|----------|----------|
| **Week 1-2** | Setup toolchain, build system | 5-10 days |
| **Week 3-6** | Display driver integration | 15-20 days |
| **Week 7-8** | Input system (analog) | 7-10 days |
| **Week 9-11** | Power management (RK809) | 10-15 days |
| **Week 12-13** | Lid sensor | 3-5 days |
| **Week 14** | Audio/vibration | 2-3 days |
| **Week 16-19** | Testing, debugging | 20-30 days |

**TOTAL:** 67-98 working days (14-20 weeks)

---

## Conclusions

### Impact Summary

| Category | Impact | Critical Files | Effort |
|----------|--------|----------------|--------|
| CPU/GPU/Kernel | ⚠️ CRITICAL | 10+ | 🔴 HIGH |
| Display | ⚠️ HIGH | 3-5 | 🔴 HIGH |
| Input | ⚠️ CRITICAL | 5+ | 🔴 HIGH |
| Power/Lid | ⚠️ CRITICAL | 8+ | 🔴 HIGH |
| Audio/Vibration | 🟡 MEDIUM | 2-3 | 🟡 MEDIUM |
| Storage | 🟡 MEDIUM | 5-10 | 🟢 LOW |
| WiFi/BT | 🟢 BONUS | 0-3 | 🟢 LOW |

### Final Estimate

**Onion OS code modification:** 67-98 days (14-20 weeks)  
**Total with kernel/drivers:** 110-154 days (22-31 weeks / 5.5-7.5 months)

**Recommendation:** This document shows that porting requires **substantial rewriting** of core components (PMU, display, input). This is not a simple port, but a significant **re-engineering** of the hardware layer.

---

**Document Prepared By:** Technical analysis based on Onion OS 4.4.0-beta codebase  
**Version:** 1.0  
**Next Phase:** Phase 2 - Kernel and toolchain prototyping
