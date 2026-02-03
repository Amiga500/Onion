# Phase 0 – Research and Initial Planning: Onion OS Porting to Miyoo Flip

**Analysis Date:** February 2026  
**Current Onion Version:** 4.2+  
**Target Device:** Miyoo Flip (clamshell with Rockchip RK3566)

---

## 1. Current Porting Status

### 1.1 Official Support
**Status:** ❌ **No Official Support**

Onion OS is currently developed exclusively for:
- **Miyoo Mini** (MIYOO283) - Allwinner F1C100s/F1C200s
- **Miyoo Mini Plus** (MIYOO354) - Allwinner F1C500s

**Evidence from codebase:**
- `src/common/system/device_model.h`: only defines `MIYOO283` and `MIYOO354`
- `src/common/system/keymap_hw.h`: input mapping limited to D-pad and basic buttons (no dual analog)
- No references to RK3566, Rockchip, or Flip in the entire codebase

### 1.2 GitHub/Community Research
**Search Results (as of February 2026):**

- ✅ OnionUI/Onion repository: 0 issues/PRs related to "Miyoo Flip", "RK3566", or "Rockchip"
- ✅ OnionUI organization: 0 discussions about Flip porting found
- ❓ Reddit r/MiyooMini: not accessible via GitHub search
- ❓ Retro Game Corps/Joey's Retro Handhelds: not accessible via GitHub search

**Conclusion:** As of the research date (2026), no official forks or public branches attempting Miyoo Flip porting were found in the OnionUI repository.

---

## 2. Major Technical Blockers Identified

### 2.1 Completely Different Hardware Architecture

| Component | Miyoo Mini/Mini+ | Miyoo Flip | Impact |
|-----------|------------------|------------|--------|
| **CPU/SoC** | Allwinner F1C100s/F1C500s (ARM926EJ-S) | Rockchip RK3566 (quad-core Cortex-A55) | ⚠️ **CRITICAL** |
| **GPU** | No dedicated GPU | Mali-G52 2EE | ⚠️ **HIGH** |
| **RAM** | 64-128 MB | 1-2 GB | ✅ Advantage |
| **Display** | Single LCD 240x320 | Dual LCD screens (internal + external) | ⚠️ **HIGH** |
| **Input** | D-pad + buttons | D-pad + **dual analog sticks** | ⚠️ **MEDIUM-HIGH** |

### 2.2 Drivers and Kernel

**Issue #1: GPU/Display Drivers**
- Miyoo Mini/Mini+: Allwinner sunxi framebuffer drivers
- Miyoo Flip: requires Rockchip + Mali GPU drivers
- Impact: entire rendering system needs rewrite

**Issue #2: Linux Kernel**
- Current: custom kernel for Allwinner (3.4.x-4.14.x)
- Required: kernel for RK3566 (5.10+ or mainline 6.x)
- Impact: complete recompilation, potential binary incompatibility

**Issue #3: Bootloader**
- Current: U-Boot for Allwinner
- Required: U-Boot or RKBOOT for Rockchip
- Impact: completely different partition management and boot process

### 2.3 Input System - Dual Analog Sticks

**Current situation:**
```c
// src/common/system/keymap_hw.h
#define HW_BTN_UP KEY_UP
#define HW_BTN_DOWN KEY_DOWN
// ... only digital buttons, NO analog stick support
```

**Required for Flip:**
- Support for 2 analog sticks (events ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_RZ)
- Analog stick calibration
- Dead zone management
- Hybrid input handling (analog + digital)

**Complexity:** ⚠️ MEDIUM-HIGH (requires modifications to keymon, input handling, RetroArch config)

### 2.4 Lid Sensor (Clamshell Closure Detection)

**Clamshell-specific issue:**
- Miyoo Flip has a magnetic/mechanical sensor to detect opening/closing
- Required:
  - Driver for lid sensor (GPIO or hall effect sensor)
  - ACPI-like or custom event handling
  - Trigger for suspend/resume
  - State saving before closure

**Impact:** ⚠️ HIGH (critical for clamshell user experience)

### 2.5 Vibration Motor

**Current situation:**
```c
// src/common/system/rumble.h
// Existing rumble support but limited
```

**Required for Flip:**
- Driver for vibration motor (likely GPIO-controlled PWM)
- Integration with RetroArch rumble API
- Intensity calibration

**Complexity:** ⚠️ LOW-MEDIUM (non-critical feature, architecture already present)

### 2.6 Advanced Power Management

**Critical differences:**
- Mini/Mini+: AXP173/AXP209 PMU (dedicated Allwinner chip)
- Flip: Rockchip RK809/RK817 PMU (completely different)

**Functionality to reimplement:**
- Battery reading
- Charging management
- Suspend/resume
- CPU frequency scaling (cpufreq)
- **Automatic shutdown on lid closure**

**Complexity:** ⚠️ CRITICAL

### 2.7 Storage and Filesystem

**Considerations:**
- Mini/Mini+: Single SD card, ext4/FAT32 filesystem
- Flip: may have internal eMMC + SD card

**Impact:** ⚠️ MEDIUM (partition layout needs verification)

---

## 3. Unofficial Forks and Branches

### 3.1 Search Conducted
- ✅ GitHub OnionUI org: no public branch or fork found
- ❌ GitLab/Codeberg: not verified (not accessible)
- ❌ Chinese forums (Baidu, Tieba): not verified

### 3.2 Related Projects to Investigate
1. **MinUI** - Alternative OS for Miyoo, possible Flip support?
2. **MuOS** - Another custom OS, might have Flip branch
3. **Koriki** - Fork for other Miyoo devices
4. **Standalone RetroArch porting** - Common base for emulators

**Recommendation:** Check if other projects have already tackled RK3566 porting.

---

## 4. 10-Phase Porting Plan

### Phase 1: Research and Preparation (Priority: 🔴 HIGH)
**Objective:** Gather all technical information about Miyoo Flip

**Tasks:**
1. Obtain complete hardware specifications for Miyoo Flip
2. Dump stock firmware and analyze kernel/drivers
3. Identify kernel version and hardware configuration
4. Map GPIO, device tree, partition layout
5. Contact community (Miyoo Discord, Reddit) for information
6. Verify if suitable Rockchip RK3566 SDK/toolchain exists

**Deliverables:**
- Document with complete hardware specifications
- Analyzed stock firmware dump
- Flip device tree

**Time Estimate:** 2-3 weeks  
**Dependencies:** Physical access to Miyoo Flip

---

### Phase 2: Toolchain and Build Environment (Priority: 🔴 HIGH)
**Objective:** Setup cross-compilation development environment for RK3566

**Tasks:**
1. Setup ARM64 cross-compiler (aarch64-linux-gnu-gcc 11+)
2. Build Linux kernel 5.10+ for RK3566
3. Configure device tree for Miyoo Flip
4. Setup U-Boot/bootloader for RK3566
5. Create minimal bootable image (Linux + busybox)
6. Test boot on real Flip

**Deliverables:**
- Working toolchain
- Custom bootable Linux kernel
- Automated build scripts

**Time Estimate:** 3-4 weeks  
**Dependencies:** Phase 1 completed

---

### Phase 3: Base Drivers - Display and Framebuffer (Priority: 🔴 HIGH)
**Objective:** Get video output working on main screen

**Tasks:**
1. Identify display panel driver (MIPI DSI, eDP, or LVDS?)
2. Configure Mali G52 driver or simple framebuffer
3. Port SDL2 for new framebuffer device
4. Test basic rendering (draw pixel, blit image)
5. Implement double-buffering
6. Handle resolution and aspect ratio

**Deliverables:**
- Working display with graphical output
- Compatible SDL2
- Test app that draws to screen

**Time Estimate:** 4-6 weeks  
**Dependencies:** Phase 2 completed  
**Critical blocker:** Without working display, development is blocked

---

### Phase 4: Input Drivers - Buttons and Analog Sticks (Priority: 🔴 HIGH)
**Objective:** Full working input (D-pad, buttons, analog sticks)

**Tasks:**
1. Identify input devices (`/dev/input/eventX`)
2. Map all physical buttons to Linux input events
3. Implement dual analog stick support (ABS_X/Y/RX/RY)
4. Add analog stick calibration
5. Handle dead zone and sensitivity
6. Update `keymap_hw.h` with new definitions
7. Test with SDL_GetJoystickAxis()

**Deliverables:**
- Complete input driver
- All buttons and sticks mapped
- Calibration tool

**Time Estimate:** 2-3 weeks  
**Dependencies:** Phase 3 completed

---

### Phase 5: Power Management and Battery (Priority: 🟡 MEDIUM-HIGH)
**Objective:** Battery reading, suspend/resume, CPU scaling

**Tasks:**
1. Identify PMU chip (RK809/RK817)
2. Driver for battery percentage reading
3. Driver for charging status
4. Implement suspend/resume (rtcwake or similar)
5. CPU frequency scaling (cpufreq-dt)
6. Temperature monitoring (thermal zones)
7. Port `src/common/system/battery.h` and `thermal.h`

**Deliverables:**
- Accurate battery reading
- Working suspend/resume
- CPU scaling for power saving

**Time Estimate:** 3-4 weeks  
**Dependencies:** Phases 2-4 completed

---

### Phase 6: Lid Sensor and Clamshell Management (Priority: 🟡 MEDIUM-HIGH)
**Objective:** Auto-suspend/resume on lid close/open

**Tasks:**
1. Identify lid sensor type (GPIO, hall effect, mechanical switch)
2. Create driver/monitor for lid events
3. Integrate with power management (auto-suspend on close)
4. Save emulation state before suspend
5. Recover state after resume
6. Handle edge cases (closing during gameplay)

**Deliverables:**
- Working lid sensor
- Smooth auto-suspend/resume
- Automatic state saving

**Time Estimate:** 2-3 weeks  
**Dependencies:** Phase 5 completed  
**Note:** Distinctive clamshell feature, high UX priority

---

### Phase 7: Audio and Vibration (Priority: 🟢 MEDIUM)
**Objective:** Audio output and rumble/vibration

**Tasks:**
1. Identify audio codec (likely integrated in RK3566)
2. Configure ALSA/audio driver
3. Port SDL_mixer/audio output
4. Test audio playback with various sample rates
5. Identify GPIO for vibration motor
6. Implement PWM driver for vibration intensity
7. Integrate with RetroArch rumble API

**Deliverables:**
- Working audio
- Controllable vibration
- Integration with emulators

**Time Estimate:** 2-3 weeks  
**Dependencies:** Phase 4 completed

---

### Phase 8: Port Onion Core Applications (Priority: 🔴 HIGH)
**Objective:** Adapt all Onion apps for new hardware

**Tasks:**
1. Recompile all binaries in `src/` for ARM64
2. Update device detection in `device_model.h` (add MIYOO_FLIP)
3. Port MainUI for new resolution
4. Adapt `gameSwitcher` for dual analog
5. Update `batteryMonitorUI` for new drivers
6. Test all Onion apps (tweaks, package manager, etc.)
7. Fix ARM64/RK3566 specific bugs

**Deliverables:**
- All Onion tools working
- Adapted MainUI
- System apps (batmon, tweaks, etc.) functional

**Time Estimate:** 4-6 weeks  
**Dependencies:** Phases 3-7 completed

---

### Phase 9: Port RetroArch and Emulator Cores (Priority: 🔴 HIGH)
**Objective:** Working emulators with new architecture

**Tasks:**
1. Build RetroArch for RK3566 with new drivers
2. Configure input mapping for dual analog
3. Port all emulator cores (NES, SNES, GBA, PSX, N64, etc.)
4. Optimizations for Mali GPU (use OpenGL ES 3.x)
5. Performance testing and tweaking
6. Fix shaders/graphic filters
7. Configure audio latency

**Deliverables:**
- Working RetroArch
- At least 20+ stable emulator cores
- Acceptable performance (60fps for SNES/GBA)

**Time Estimate:** 6-10 weeks  
**Dependencies:** Phase 8 completed  
**Note:** Most critical phase for final usability

---

### Phase 10: Testing, Optimization and Release (Priority: 🟡 MEDIUM)
**Objective:** Stabilize, optimize, document, release

**Tasks:**
1. Extensive testing of all functionality
2. Performance optimizations (GPU, CPU scaling)
3. Battery life testing and optimizations
4. Thermal management tuning
5. Create installer/SD image
6. User documentation
7. Setup GitHub repository for Onion-Flip
8. Public beta release

**Deliverables:**
- Stable and tested build
- Ready-to-use SD image
- Complete documentation
- Public repository

**Time Estimate:** 4-6 weeks  
**Dependencies:** All previous phases completed

---

## 5. Priority Summary and Timeline

### HIGH Priority (Critical Blockers) 🔴
1. **Phase 1** - Research (2-3 weeks)
2. **Phase 2** - Toolchain (3-4 weeks)
3. **Phase 3** - Display (4-6 weeks) ⚠️ **BLOCKER**
4. **Phase 4** - Input (2-3 weeks)
5. **Phase 8** - Port Onion Apps (4-6 weeks)
6. **Phase 9** - Port Emulators (6-10 weeks) ⚠️ **BLOCKER**

**Subtotal critical phases:** ~21-32 weeks (5-8 months)

### MEDIUM-HIGH Priority 🟡
1. **Phase 5** - Power Management (3-4 weeks)
2. **Phase 6** - Lid Sensor (2-3 weeks)

**Subtotal:** ~5-7 weeks

### MEDIUM Priority 🟢
1. **Phase 7** - Audio/Vibration (2-3 weeks)
2. **Phase 10** - Testing/Release (4-6 weeks)

**Subtotal:** ~6-9 weeks

### Total Estimated Timeline
**BEST CASE:** 32 weeks (~8 months)  
**REALISTIC:** 48 weeks (~12 months)  
**WORST CASE:** 72+ weeks (18+ months)

*Assuming 1 full-time developer with embedded Linux experience*

---

## 6. Resources and Requirements

### 6.1 Required Hardware
- ✅ Miyoo Flip device (at least 2 units for testing)
- ✅ Multiple SD cards (for testing and brick recovery)
- ✅ UART/Serial adapter for debugging (essential!)
- ✅ Logic analyzer (optional but useful for GPIO)
- ✅ Multimeter to identify GPIO/power rails

### 6.2 Required Skills
1. **Embedded Linux development** (kernel, drivers, device tree) - ⚠️ CRITICAL
2. **ARM64/Rockchip cross-compilation**
3. **SDL2 and multimedia programming**
4. **RetroArch internals and libretro API**
5. **Hardware debugging and reverse engineering**
6. **Advanced C/C++**
7. **Git and open source project management**

### 6.3 Software and Tooling
- Rockchip SDK or Buildroot/Yocto for RK3566
- ARM64 cross-compiler
- U-Boot and Linux kernel sources
- Device tree compiler
- Serial terminal (minicom, screen)
- RetroArch source tree
- OnionUI source tree

---

## 7. Risks and Mitigations

### Risk 1: Insufficient Hardware Documentation 🔴
**Impact:** CRITICAL - Unable to develop drivers  
**Probability:** HIGH  
**Mitigation:**
- Reverse engineer stock firmware
- Contact Miyoo manufacturer for specs
- Community collaboration

### Risk 2: Insufficient Performance 🟡
**Impact:** MEDIUM - Slow emulation or choppy audio  
**Probability:** MEDIUM  
**Mitigation:**
- Leverage Mali G52 GPU for rendering
- CPU overclocking if needed
- ARM64 assembly optimizations

### Risk 3: Device Bricking During Testing 🟡
**Impact:** MEDIUM-HIGH - Loss of test device  
**Probability:** MEDIUM  
**Mitigation:**
- Backup stock firmware via UART/maskrom mode
- Dual-boot with original firmware
- Recovery partition

### Risk 4: Lack of Community Support 🟢
**Impact:** LOW-MEDIUM  
**Probability:** MEDIUM  
**Mitigation:**
- Detailed documentation
- Incremental releases
- Transparent communication

---

## 8. Alternatives to Consider

### 8.1 Fork Other OS
Instead of porting Onion from scratch, evaluate:
1. **JelOS/ROCKNIX** - already support RK3566 on other devices
2. **ArkOS** - experience with Rockchip devices
3. **RetroPie** - solid base, might be simpler

**Pros:** Less initial work, existing community  
**Cons:** Loss of Onion identity/features

### 8.2 Dual Boot Solution
- Keep stock Miyoo Flip firmware
- Boot Onion from SD card when requested
- Safe fallback to original firmware

**Pros:** Safety, non-destructive  
**Cons:** Bootloader complexity

---

## 9. Conclusions and Recommendations

### 9.1 Technical Feasibility
**VERDICT:** ✅ **TECHNICALLY FEASIBLE** but **HIGHLY COMPLEX**

Porting is possible but requires:
- Team of at least 2-3 experienced developers
- 12-18 months of development
- Budget for hardware and testing
- Community support

### 9.2 Immediate Recommendations
1. ✅ **Confirm community interest** - Poll/survey on Discord/Reddit
2. ✅ **Acquire hardware** - Procure 2-3 Miyoo Flip for testing
3. ✅ **Setup team** - Recruit developers with embedded experience
4. ✅ **Dump stock firmware** - BEFORE modifying anything
5. ❌ **Don't start development without specs** - High failure risk

### 9.3 Go/No-Go Decision
**ADVICE:** 🟡 **PROCEED WITH CAUTION**

Start with **Phase 1 (Research)** and re-evaluate after 1 month:
- If hardware is reverse-engineerable → GO
- If critical specs are missing → NO-GO (too risky)
- If active community support exists → GO
- If nobody is interested/helping → NO-GO

### 9.4 Recommended Next Steps
1. Publish this document on OnionUI GitHub Discussions
2. Create poll on Reddit r/MiyooMini to gauge interest
3. Contact MinUI/MuOS developers for collaboration
4. Acquire Miyoo Flip and start reverse engineering
5. Setup `OnionUI/Onion-Flip` repository for progress tracking

---

## 10. References and Resources

### 10.1 Relevant Repositories
- [OnionUI/Onion](https://github.com/OnionUI/Onion) - Official source
- [Rockchip Linux](https://github.com/rockchip-linux) - Drivers and kernel
- [LibreELEC RK3566](https://github.com/LibreELEC/LibreELEC.tv) - RK3566 reference
- [RetroArch](https://github.com/libretro/RetroArch) - Emulator frontend

### 10.2 Technical Documentation
- [Rockchip RK3566 Datasheet](http://opensource.rock-chips.com/wiki_RK3566) (if available)
- [ARM Mali G52 Documentation](https://developer.arm.com/ip-products/graphics-and-multimedia/mali-gpus)
- [Linux Kernel Device Tree](https://www.kernel.org/doc/html/latest/devicetree/)
- [U-Boot for Rockchip](https://docs.u-boot.org/en/latest/board/rockchip/rockchip.html)

### 10.3 Community and Forums
- OnionUI Discord (check if #porting channel exists)
- Reddit r/MiyooMini
- Retro Game Corps (video reviews/guides)
- Joey's Retro Handhelds (news and discussions)
- Dingoonity forums (handheld console community)

---

**Document Prepared By:** Technical analysis based on OnionUI codebase  
**Version:** 1.0  
**Last Updated:** February 2026  
**Status:** DRAFT - Requires community validation and hardware access

---

## Appendix A: Technical Glossary

- **SoC (System on Chip):** Integrated chip containing CPU, GPU, memory controller, etc.
- **Device Tree:** Data structure describing hardware to Linux kernel
- **PMU (Power Management Unit):** Dedicated chip for power management
- **Framebuffer:** Memory area for display pixels
- **Cross-compilation:** Compile code on one system (x86) for different target (ARM)
- **Bootloader:** Software that loads operating system at boot
- **GPIO (General Purpose Input/Output):** Programmable pins for hardware interface
- **PWM (Pulse Width Modulation):** Technique to control motor/LED intensity
- **ABS_X/Y (Absolute axis):** Input events for analog joysticks
- **Mali GPU:** ARM GPU series for embedded devices
- **UART:** Serial interface for debugging and communication

---

## Appendix B: Changelog

**v1.0 (February 2026)**
- First version of document
- Complete analysis based on GitHub research
- 10-phase plan with time estimates
- Critical blocker identification

---

*This document is a work-in-progress and will be updated as more information becomes available from the community and reverse engineering efforts.*
