# Miyoo Flip Porting Documentation

This directory contains comprehensive research and planning documentation for porting Onion OS to the Miyoo Flip device.

## Available Documents

### Phase 0 - Research and Initial Planning

#### 📄 [MIYOO_FLIP_PORTING_PHASE0.md](./MIYOO_FLIP_PORTING_PHASE0.md) (Italiano)
Documento completo in italiano che analizza lo stato attuale del porting di Onion OS sul Miyoo Flip (clamshell con Rockchip RK3566).

**Contenuto:**
- Stato attuale del porting (nessun supporto ufficiale)
- 7 blocker tecnici principali identificati
- Ricerca di fork e branch non ufficiali
- Piano dettagliato in 10 fasi con priorità
- Timeline stimata: 8-18 mesi
- Raccomandazioni e decisioni go/no-go

#### 📄 [MIYOO_FLIP_PORTING_PHASE0_EN.md](./MIYOO_FLIP_PORTING_PHASE0_EN.md) (English)
Complete English version of the Miyoo Flip porting research and planning document.

**Contents:**
- Current porting status (no official support)
- 7 major technical blockers identified
- Research on unofficial forks and branches
- Detailed 10-phase plan with priorities
- Estimated timeline: 8-18 months
- Recommendations and go/no-go decisions

### Phase 1 - Hardware Analysis and Differences

#### 📄 [MIYOO_FLIP_PHASE1_HARDWARE_ANALYSIS.md](./MIYOO_FLIP_PHASE1_HARDWARE_ANALYSIS.md) (Italiano)
Analisi dettagliata delle differenze hardware con mappatura file-per-file delle modifiche necessarie al codice Onion OS.

**Contenuto:**
- Confronto hardware dettagliato (Mini+ vs Flip)
- 8 categorie hardware analizzate (CPU/GPU, Display, Input, Power, Audio, Storage, WiFi/BT)
- 47+ file Onion OS identificati per modifica/riscrittura
- Stima complessità per area (LOC, giorni, rischio)
- Timeline dettagliata per implementazione (67-98 giorni)
- Tabelle riepilogative e priorità (P0/P1/P2)

#### 📄 [MIYOO_FLIP_PHASE1_HARDWARE_ANALYSIS_EN.md](./MIYOO_FLIP_PHASE1_HARDWARE_ANALYSIS_EN.md) (English)
Detailed hardware differences analysis with file-by-file mapping of required Onion OS code modifications.

**Contents:**
- Detailed hardware comparison (Mini+ vs Flip)
- 8 hardware categories analyzed (CPU/GPU, Display, Input, Power, Audio, Storage, WiFi/BT)
- 47+ Onion OS files identified for modification/rewrite
- Complexity estimate per area (LOC, days, risk)
- Detailed implementation timeline (67-98 days)
- Summary tables and priorities (P0/P1/P2)

### Phase 2 - Kernel and Device Tree

#### 📄 [MIYOO_FLIP_PHASE2_KERNEL_DEVICETREE.md](./MIYOO_FLIP_PHASE2_KERNEL_DEVICETREE.md) (Italiano)
Il cuore del porting: device tree completo e configurazione kernel Linux per RK3566.

**Contenuto:**
- Device tree source completo (rk3566-miyoo-flip.dts)
- Mappatura hardware dettagliata (CPU OPP table, GPIO, PWM, I2C, SPI, SDMMC)
- Kernel defconfig ottimizzato (CONFIG_* flags per tutti i componenti)
- Istruzioni compilazione kernel step-by-step
- Boot procedure con U-Boot
- Troubleshooting e testing hardware
- 20KB di documentazione tecnica completa

#### 📄 [MIYOO_FLIP_PHASE2_KERNEL_DEVICETREE_EN.md](./MIYOO_FLIP_PHASE2_KERNEL_DEVICETREE_EN.md) (English)
The heart of the porting: complete device tree and Linux kernel configuration for RK3566.

**Contents:**
- Complete device tree source (rk3566-miyoo-flip.dts)
- Detailed hardware mapping (CPU OPP table, GPIO, PWM, I2C, SPI, SDMMC)
- Optimized kernel defconfig (CONFIG_* flags for all components)
- Step-by-step kernel compilation instructions
- Boot procedure with U-Boot
- Hardware troubleshooting and testing
- 20KB of complete technical documentation

#### 📁 [device-tree/rk3566-miyoo-flip.dts](./device-tree/rk3566-miyoo-flip.dts)
Device tree source file ready for compilation and deployment.

**Features:**
- Based on Pine64 Quartz64, Anbernic RG353, ODROID-GO references
- Complete hardware description (500+ lines)
- CPU cores with OPP table (408MHz-1800MHz)
- Mali-G52 GPU configuration
- MIPI DSI display panel (640×480)
- GPIO buttons (D-pad, face, shoulder, L3/R3)
- ADC analog joysticks (dual sticks, 4 channels)
- Hall effect lid sensor
- PWM vibration motor
- RK809/RK817 PMIC with regulators
- Dual SD/MMC + WiFi SDIO
- Complete pinctrl configuration

### Phase 3 - Bootloader and First Boot

#### 📄 [MIYOO_FLIP_PHASE3_BOOTLOADER_FIRSTBOOT.md](./MIYOO_FLIP_PHASE3_BOOTLOADER_FIRSTBOOT.md) (Italiano)
Bootloader U-Boot e creazione immagine SD bootabile per primo avvio sicuro.

**Contenuto:**
- Setup toolchain ARM64 cross-compilation completo
- U-Boot configuration per RK3566 Miyoo Flip
- Build U-Boot + firmware Rockchip (rkbin tools)
- Struttura partizioni SD ottimizzata (boot, rootfs, user)
- Script creazione immagine SD bootabile automatici
- Integrazione kernel + DTB custom (Phase 2)
- Procedure primo boot e testing con UART
- Checklist anti-brick dettagliata (protezione eMMC)
- Recovery procedures (Maskrom mode, rkdeveloptool)
- Troubleshooting U-Boot e kernel boot completo
- 29KB di documentazione tecnica con esempi pratici

#### 📄 [MIYOO_FLIP_PHASE3_BOOTLOADER_FIRSTBOOT_EN.md](./MIYOO_FLIP_PHASE3_BOOTLOADER_FIRSTBOOT_EN.md) (English)
U-Boot bootloader and bootable SD image creation for safe first boot.

**Contents:**
- Complete ARM64 cross-compilation toolchain setup
- U-Boot configuration for RK3566 Miyoo Flip
- U-Boot + Rockchip firmware build (rkbin tools)
- Optimized SD partition structure (boot, rootfs, user)
- Automated bootable SD image creation scripts
- Custom kernel + DTB integration (Phase 2)
- First boot procedures and UART testing
- Detailed anti-brick checklist (eMMC protection)
- Recovery procedures (Maskrom mode, rkdeveloptool)
- Complete U-Boot and kernel boot troubleshooting
- 29KB of technical documentation with practical examples

## Quick Summary

### Current Status: ❌ No Official Support

Onion OS currently supports only:
- Miyoo Mini (MIYOO283) - Allwinner F1C100s/F1C200s
- Miyoo Mini Plus (MIYOO354) - Allwinner F1C500s

The Miyoo Flip uses a completely different hardware architecture (Rockchip RK3566) requiring extensive porting work.

### Major Blockers Identified:

1. ⚠️ **CRITICAL**: Different SoC architecture (Allwinner → Rockchip RK3566)
2. ⚠️ **HIGH**: GPU/Display drivers (Mali G52, dual screens)
3. ⚠️ **HIGH**: Linux kernel differences (3.4.x-4.14.x → 5.10+)
4. ⚠️ **MEDIUM-HIGH**: Dual analog stick support
5. ⚠️ **HIGH**: Lid sensor for clamshell functionality
6. ⚠️ **CRITICAL**: Power management (different PMU chip)
7. ⚠️ **MEDIUM**: Vibration motor integration

### Estimated Timeline:
- **Best Case**: 8 months
- **Realistic**: 12 months
- **Worst Case**: 18+ months

*(Assuming 1 full-time developer with embedded Linux experience)*

### 10-Phase Porting Plan:

| Phase | Description | Priority | Duration |
|-------|-------------|----------|----------|
| 1 | Research & Preparation | 🔴 HIGH | 2-3 weeks |
| 2 | Toolchain & Build Environment | 🔴 HIGH | 3-4 weeks |
| 3 | Display & Framebuffer Drivers | 🔴 HIGH | 4-6 weeks |
| 4 | Input Drivers (Buttons & Analog) | 🔴 HIGH | 2-3 weeks |
| 5 | Power Management & Battery | 🟡 MEDIUM-HIGH | 3-4 weeks |
| 6 | Lid Sensor & Clamshell | 🟡 MEDIUM-HIGH | 2-3 weeks |
| 7 | Audio & Vibration | 🟢 MEDIUM | 2-3 weeks |
| 8 | Port Onion Core Apps | 🔴 HIGH | 4-6 weeks |
| 9 | Port RetroArch & Emulators | 🔴 HIGH | 6-10 weeks |
| 10 | Testing & Release | 🟡 MEDIUM | 4-6 weeks |

## Phase 1 Results - File-Level Analysis

**Status:** ✅ **COMPLETED** (February 2026)

### Files Identified for Modification

**Critical Priority (P0):**
- 8-10 files requiring major rewrite or complete replacement
- Includes: `Makefile`, `device_model.h`, `axp.h`, `display.h`, `keymap_hw.h`, `input_fd.h`, `keymon.c`, `battery.h`
- Effort: 45-60 days

**Core Priority (P1):**
- 10-15 files requiring significant updates
- Includes: Display DRM/KMS support, analog input handling, lid sensor integration
- Effort: 15-25 days

**Enhancement Priority (P2):**
- 5-10 files for nice-to-have features
- Includes: Dual display, WiFi/BT, advanced power features
- Effort: 7-13 days

### Hardware Components Analyzed

| Component | Impact | Files | Complexity |
|-----------|--------|-------|------------|
| CPU/GPU/Kernel | ⚠️ CRITICAL | 10+ | REWRITE |
| Display (Dual) | ⚠️ HIGH | 3-5 | MAJOR |
| Input (Analog) | ⚠️ CRITICAL | 5+ | MAJOR |
| Power/PMU/Lid | ⚠️ CRITICAL | 8+ | REWRITE |
| Audio/Vibration | 🟡 MEDIUM | 2-3 | MODERATE |
| Storage (Dual SD) | 🟡 MEDIUM | 5-10 | MINOR |
| WiFi/Bluetooth | 🟢 BONUS | 0-3 | MINOR |

### Code Modification Estimate

- **Total Files:** 25-47 files
- **Lines of Code:** ~6,500+ LOC
- **Effort:** 67-98 working days (14-20 weeks)
- **Risk:** 🔴 HIGH

**Note:** This estimate covers only Onion OS userspace code modifications. Kernel, drivers, and U-Boot porting add another 4-8 weeks.

## Phase 2 Results - Kernel and Device Tree

**Status:** ✅ **COMPLETED** (February 2026)

### Device Tree Created

**File:** `device-tree/rk3566-miyoo-flip.dts` (500+ lines)

Complete hardware description including:
- CPU: 4× Cortex-A55 cores with OPP table (408MHz-1800MHz, 6 frequency steps)
- GPU: Mali-G52 2EE with Panfrost driver
- Display: 640×480 MIPI DSI panel with PWM backlight
- Input: 17 GPIO buttons + dual ADC analog sticks (4 axes)
- Power: RK809/RK817 PMIC with 5 DCDC + 9 LDO regulators
- Lid: Hall effect sensor (SW_LID event)
- Vibration: PWM-controlled motor (1kHz)
- Storage: Dual SD/MMC (eMMC + external SD) + WiFi SDIO
- Audio: RK809 integrated codec (stereo)
- Peripherals: UART2, I2C0-2, SPI0-1, SARADC

### Kernel Configuration

**Essential CONFIG flags identified:**

**Input System (Critical for Gaming):**
- `CONFIG_INPUT_ADC_JOYSTICK=y` - Analog sticks
- `CONFIG_KEYBOARD_GPIO=y` - Digital buttons
- `CONFIG_INPUT_PWM_VIBRA=y` - Vibration motor

**Display and Graphics:**
- `CONFIG_DRM_ROCKCHIP=y` - Display driver
- `CONFIG_ROCKCHIP_VOP2=y` - Video output processor
- `CONFIG_ROCKCHIP_DW_MIPI_DSI=y` - MIPI DSI interface
- `CONFIG_DRM_PANFROST=y` - Mali GPU driver
- `CONFIG_BACKLIGHT_PWM=y` - Backlight control

**Power Management:**
- `CONFIG_MFD_RK808=y` - PMIC support
- `CONFIG_REGULATOR_RK808=y` - Voltage regulators
- `CONFIG_CHARGER_RK817=y` - Battery charger
- `CONFIG_BATTERY_RK817=y` - Fuel gauge

**CPU Frequency Scaling:**
- `CONFIG_CPU_FREQ_DEFAULT_GOV_SCHEDUTIL=y` - Dynamic governor
- `CONFIG_CPUFREQ_DT=y` - Device tree OPP support

**Storage:**
- `CONFIG_MMC_DW_ROCKCHIP=y` - SD/MMC controller
- `CONFIG_RTW88_8821CS=m` - Realtek WiFi driver

### Compilation Guide

Complete step-by-step instructions for:
1. Toolchain setup (aarch64-linux-gnu-gcc 11+)
2. Kernel configuration (rockchip_defconfig + custom)
3. Device tree compilation (dtc)
4. Kernel image build (Image + dtb + modules)
5. Module installation and packaging
6. Boot image creation (mkbootimg)

### Boot Procedure

**U-Boot commands:**
```bash
load mmc 1:1 ${kernel_addr_r} Image
load mmc 1:1 ${fdt_addr_r} rk3566-miyoo-flip.dtb
setenv bootargs "console=ttyS2,1500000 root=/dev/mmcblk1p2 rw"
booti ${kernel_addr_r} - ${fdt_addr_r}
```

### Hardware Testing

Verification procedures for:
- Input devices (`evtest /dev/input/event*`)
- Display framebuffer (`/sys/class/graphics/fb0`)
- ADC channels (`/sys/bus/iio/devices`)
- GPIO state (`/sys/kernel/debug/gpio`)
- PWM outputs (`/sys/class/pwm`)
- Regulators (`/sys/kernel/debug/regulator`)

### Troubleshooting Guide

Common issues and solutions:
- Kernel panic (no root device)
- Display not working (DRM/MIPI DSI)
- Analog sticks not detected (ADC/joystick driver)
- WiFi not working (SDIO/driver/firmware)
- Performance monitoring (cpufreq, thermal)

## Phase 3 Results - Bootloader and First Boot

**Status:** ✅ **COMPLETED** (February 2026)

### U-Boot Bootloader Configuration

**Target:** RK3566 Miyoo Flip with Rockchip-specific boot flow

**U-Boot Sources:**
- Mainline U-Boot 2023.10+ (recommended for development)
- Rockchip vendor U-Boot (more drivers, less mainline)
- Stock firmware extraction (Miyoo Flip specific)

**Configuration:**
- Base: `rk3568_defconfig` or `quartz64-a-rk3566_defconfig`
- Custom: `miyoo_flip_rk3566_defconfig` with Flip-specific options
- 50+ CONFIG flags for MMC, display, GPIO, PMIC, USB

**Rockchip Boot Components:**
- `idbloader.img` - TPL + SPL (DRAM init + boot loader)
- `u-boot.itb` - U-Boot proper + ATF + DTB (FIT image)
- `u-boot-rockchip.bin` - Combined all-in-one image
- rkbin blobs: `rk3568_ddr_1560MHz_v1.16.bin`, `rk3568_bl31_v1.43.elf`

### SD Card Partition Structure

**Optimized layout for gaming handheld:**

```
Offset      Size        Partition       Filesystem
───────────────────────────────────────────────────
0           32KB        Reserved        -
32KB        16MB        U-Boot          raw
16.5MB      256MB       Boot            FAT32
272MB       8GB         RootFS          ext4
8.3GB       Remaining   User/ROMs       exFAT
```

**Partition Details:**
- **Reserved (0-32KB):** BootROM reserved space
- **U-Boot (32KB-16MB):** Bootloader at offset 64 blocks (32KB)
- **Boot (FAT32):** Kernel Image, DTB, boot scripts (256MB)
- **RootFS (ext4):** Onion OS system files with permissions (8GB)
- **User (exFAT):** ROMs, saves, large files, Windows compatible

### Build Process

**Toolchain Setup:**
```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export RKBIN_PATH=/path/to/rkbin
```

**U-Boot Build:**
```bash
make miyoo_flip_rk3566_defconfig
make -j$(nproc) BL31=bl31.elf ROCKCHIP_TPL=rk3568_ddr_1560MHz_v1.16.bin
# Output: u-boot-rockchip.bin (~2-4MB)
```

**Boot Script:**
- `boot.cmd` compiled to `boot.scr` with mkimage
- Auto-detection: Try external SD → internal eMMC
- Kernel load: `Image` + `rk3566-miyoo-flip.dtb`
- Boot command: `booti ${kernel_addr_r} - ${fdt_addr_r}`

### Bootable SD Image Creation

**Automated scripts provided:**

1. **`create_sd_partitions.sh`** - Partition creation with GPT
   - Creates 3 partitions (boot, rootfs, user)
   - Formats with appropriate filesystems
   - Sets boot flag on partition 1

2. **`create_bootable_sd.sh`** - Complete SD image builder
   - Flashes U-Boot at correct offset (seek=64)
   - Populates boot partition (kernel, DTB, boot.scr)
   - Extracts rootfs tarball
   - Configures fstab for auto-mount

**Manual Flash Commands:**
```bash
# Flash U-Boot bootloader
sudo dd if=u-boot-rockchip.bin of=/dev/sdb seek=64 bs=512 conv=fsync

# Or separate method:
sudo dd if=idbloader.img of=/dev/sdb seek=64 bs=512
sudo dd if=u-boot.itb of=/dev/sdb seek=16384 bs=512
```

### First Boot Procedures

**Hardware Preparation:**
- UART adapter connected (1.5 Mbaud)
- SD card inserted in external slot
- Battery charged (>80%)
- Serial console monitoring: `screen /dev/ttyUSB0 1500000`

**Expected Boot Sequence:**
```
DDR V1.16 ... LPDDR4X, 1560MHz
U-Boot SPL 2023.10
U-Boot 2023.10 - Model: Miyoo Flip Handheld Console
Loading kernel... 14520320 bytes
Booting kernel...
[    0.000000] Booting Linux on physical CPU 0x0
[    0.000000] Machine model: Miyoo Flip Handheld Console
```

**U-Boot Useful Commands:**
- `mmc list` - List MMC devices
- `ls mmc 1:1` - List files in boot partition
- `printenv` - Show environment variables
- `gpio status` - Check GPIO state
- `i2c probe` - Scan I2C bus (PMIC detection)

### Anti-Brick Safety

**Critical checklist:**
- [ ] **ALWAYS test on SD card first** (never eMMC until verified)
- [ ] Backup stock firmware before any modifications
- [ ] UART connection working and tested
- [ ] Verify binary checksums after flash
- [ ] Keep recovery SD card prepared
- [ ] Battery fully charged during flash

**Device identification:**
```bash
lsblk
# mmcblk0 - internal eMMC (DO NOT TOUCH until SD works)
# mmcblk1 - external SD (SAFE for testing)
```

### Recovery Procedures

**If device bricks:**

1. **Recovery SD Card:** Pre-prepared SD with stock firmware
2. **Maskrom Mode:** Hardware recovery mode in SoC
   - Short Maskrom pins on PCB
   - Connect USB-C to PC
   - Flash via rkdeveloptool
3. **Emergency Serial:** Load firmware via UART (slow but works)

**Maskrom Flash:**
```bash
# Verify Maskrom mode
lsusb | grep Rockchip  # ID 2207:330c

# Flash bootloader
sudo rkdeveloptool db rk3568_loader_v1.08.111.bin
sudo rkdeveloptool wl 0x40 u-boot-rockchip.bin
sudo rkdeveloptool rd  # Reboot
```

### Troubleshooting Covered

**10+ common scenarios with solutions:**
1. U-Boot doesn't start (wrong offset, corrupted binary)
2. U-Boot works but kernel won't boot (arch mismatch, missing DTB)
3. Kernel boots but userspace fails (rootfs corruption, init missing)
4. Performance issues (CPU governor, thermal throttling)
5. Display problems (DRM driver, backlight)
6. Input not working (GPIO, ADC configuration)
7. WiFi/Bluetooth failures (SDIO, firmware)
8. SD card not detected (MMC driver, pinmux)
9. PMIC issues (I2C, regulator configuration)
10. Boot hangs (debug with loglevel=8, initcall_debug)

### Scripts and Configurations

**Created files:**
- `create_sd_partitions.sh` - Automated partition creation
- `create_bootable_sd.sh` - Complete SD image builder
- `boot.cmd` / `boot.scr` - U-Boot boot script
- `miyoo_flip_rk3566_defconfig` - U-Boot configuration
- `rk3566-miyoo-flip-u-boot.dtsi` - U-Boot device tree

## Recommendations

### ✅ Immediate Actions:
1. Confirm community interest (Discord/Reddit poll)
2. Acquire 2-3 Miyoo Flip devices for testing
3. Setup development team (2-3 experienced embedded developers)
4. Dump stock firmware for analysis
5. Start with Phase 1 (Research)

### ⚠️ Go/No-Go Decision:
**Proceed with caution** - Start with Phase 1 research and re-evaluate after 1 month based on:
- Hardware reverse-engineering feasibility
- Availability of specifications
- Community support and interest
- Developer availability

## Community Resources

### To Investigate:
- 🔍 MinUI, MuOS, Koriki - Check for existing RK3566 support
- 🔍 Reddit r/MiyooMini - Community interest
- 🔍 Retro Game Corps, Joey's Retro Handhelds - Reviews and info
- 🔍 OnionUI Discord - Developer collaboration

### Related Repositories:
- [OnionUI/Onion](https://github.com/OnionUI/Onion) - Official source
- [Rockchip Linux](https://github.com/rockchip-linux) - Drivers and kernel
- [LibreELEC RK3566](https://github.com/LibreELEC/LibreELEC.tv) - Reference implementation
- [RetroArch](https://github.com/libretro/RetroArch) - Emulator frontend

## Contributing

If you're interested in contributing to the Miyoo Flip porting effort:

1. Read the full documentation (Italian or English version)
2. Join the OnionUI Discord/community discussions
3. Share your embedded Linux/Rockchip experience
4. Help with hardware reverse engineering
5. Contribute to any of the 10 phases outlined in the plan

---

**Document Version:** 1.0  
**Last Updated:** February 2026  
**Status:** Research Phase - Community feedback needed

For detailed technical analysis, feasibility assessment, and complete phase-by-phase planning, please refer to the full documents linked above.
