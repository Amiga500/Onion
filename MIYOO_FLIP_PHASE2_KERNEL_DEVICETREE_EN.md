# Phase 2 – Kernel and Device Tree: The Heart of Miyoo Flip Porting

**Date:** February 2026  
**Onion Version:** 4.4.0-beta  
**Target:** Miyoo Flip with Rockchip RK3566  
**Kernel Version:** Linux 5.10+ (recommended 5.10.160 or 6.1 LTS)

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Device Tree Source](#2-device-tree-source)
3. [Hardware Components Mapped](#3-hardware-components-mapped)
4. [Kernel Configuration (defconfig)](#4-kernel-configuration-defconfig)
5. [Kernel Compilation](#5-kernel-compilation)
6. [Boot and Testing](#6-boot-and-testing)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Introduction

This phase covers the **heart of the porting**: creating the device tree and configuring the Linux kernel for Miyoo Flip. The device tree describes all hardware present on the device to the kernel, while defconfig enables necessary drivers.

### 1.1 Phase 2 Objectives

- ✅ Complete device tree for RK3566
- ✅ Hardware mapping (CPU, GPU, display, input, power, storage)
- ✅ Optimized kernel defconfig
- ✅ Bootable kernel compilation instructions

### 1.2 Prerequisites

- **Hardware:** Miyoo Flip with UART access
- **Software:** 
  - Linux kernel 5.10+ sources
  - ARM64 toolchain (aarch64-linux-gnu-gcc 11+)
  - Device tree compiler (dtc)
  - U-Boot for RK3566
- **Knowledge:** Linux kernel, device tree, Rockchip hardware

---

## 2. Device Tree Source

### 2.1 File Created

**Location:** `device-tree/rk3566-miyoo-flip.dts`

This device tree was created based on:
- **rk3566-quartz64-a.dts** - Pine64 Quartz64 Model A (RK3566 reference board)
- **rk3566-anbernic-rgxx3.dts** - Anbernic RG353 series (similar handheld)
- **rk3326-odroid-go2.dts** - ODROID-GO Advance (gaming handheld reference)

### 2.2 Device Tree Structure

The device tree includes complete mapping for:
- CPU cores (4× Cortex-A55) with OPP table
- GPU (Mali-G52)
- Display panel (640×480 MIPI DSI)
- GPIO buttons (D-pad, face, shoulder, system)
- Analog sticks (ADC-based via SARADC)
- Lid sensor (hall effect switch)
- PWM vibration motor
- RK809/RK817 PMIC with regulators
- Dual SD/MMC controllers
- WiFi SDIO interface
- Peripherals (UART, I2C, SPI, ADC)

### 2.3 Compiling Device Tree

```bash
# From kernel source directory
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs

# Or compile single DTS
dtc -I dts -O dtb -o rk3566-miyoo-flip.dtb device-tree/rk3566-miyoo-flip.dts

# Verify compiled DTB
dtc -I dtb -O dts rk3566-miyoo-flip.dtb | less
```

---

## 3. Hardware Components Mapped

### 3.1 CPU and Performance

#### CPU Cores (Cortex-A55 × 4)

Operating performance points for frequency scaling:

| Frequency | Voltage | Typical Use |
|-----------|---------|-------------|
| 408 MHz | 900 mV | Idle, menu navigation |
| 816 MHz | 900 mV | Retro emulation (8/16-bit) |
| 1104 MHz | 950 mV | GBA, SNES emulation |
| 1416 MHz | 1000 mV | PSX, N64 emulation |
| 1608 MHz | 1050 mV | PSP, DC emulation |
| 1800 MHz | 1150 mV | Max performance |

**CPU Governor:** `schedutil` recommended for gaming (dynamic scaling)

### 3.2 GPU Mali-G52

**Driver:** `panfrost` (mainline Linux 5.10+)

**Capabilities:**
- OpenGL ES 3.2
- Vulkan 1.1
- 2× execution engines
- Frequencies: 200-800 MHz

### 3.3 Input System

#### GPIO Buttons

Complete button mapping with GPIO assignments for all digital inputs (D-pad, face buttons, shoulder buttons, L3/R3).

#### Analog Sticks (ADC-based)

**ADC Channels:**
- SARADC CH0 → Left stick X-axis
- SARADC CH1 → Left stick Y-axis
- SARADC CH2 → Right stick X-axis
- SARADC CH3 → Right stick Y-axis

**Driver:** `adc-joystick` (CONFIG_INPUT_ADC_JOYSTICK)

**Calibration parameters:**
- `abs-fuzz`: 10 (noise filtering)
- `abs-flat`: 50 (deadzone radius)
- `poll-interval`: 10ms (100Hz polling)

#### Lid Sensor (Clamshell Detection)

**Type:** Hall effect sensor (magnetic switch)  
**Event:** SW_LID (triggers suspend/resume)  
**Driver:** `gpio-keys` (switch variant)

### 3.4 Power Management - RK809/RK817 PMIC

**Features:**
- 5× DCDC regulators (CPU, GPU, DDR, peripherals)
- 9× LDO regulators
- Battery charger (Li-ion 3.7V)
- Fuel gauge (capacity estimation)
- Power button handling
- RTC (real-time clock)
- Audio codec (stereo output)

**Driver:** `rk808-regulator` (CONFIG_REGULATOR_RK808)

### 3.5 Storage (Dual SD/MMC)

- **SDMMC0:** Internal eMMC or system SD (8-lane, HS200)
- **SDMMC1:** External user SD (4-lane, removable)
- **SDMMC2:** WiFi SDIO (4-lane, non-removable)

---

## 4. Kernel Configuration (defconfig)

### 4.1 Essential Configs

#### Platform and SoC

```kconfig
CONFIG_ARCH_ROCKCHIP=y
CONFIG_ARM64=y
CONFIG_ROCKCHIP_PM_DOMAINS=y
CONFIG_ROCKCHIP_SARADC=y
```

#### CPU and Scheduling

```kconfig
CONFIG_SMP=y
CONFIG_NR_CPUS=4
CONFIG_CPU_FREQ=y
CONFIG_CPU_FREQ_DEFAULT_GOV_SCHEDUTIL=y
CONFIG_CPUFREQ_DT=y
```

#### Display and Graphics

```kconfig
CONFIG_DRM=y
CONFIG_DRM_ROCKCHIP=y
CONFIG_ROCKCHIP_VOP2=y
CONFIG_ROCKCHIP_DW_MIPI_DSI=y
CONFIG_DRM_PANEL_SIMPLE=y
CONFIG_DRM_PANFROST=y
CONFIG_BACKLIGHT_PWM=y
```

#### Input Devices (CRITICAL)

```kconfig
CONFIG_INPUT_EVDEV=y
CONFIG_KEYBOARD_GPIO=y
CONFIG_INPUT_ADC_JOYSTICK=y  # Analog sticks
CONFIG_INPUT_FF_MEMLESS=y
CONFIG_INPUT_PWM_VIBRA=y     # Vibration
```

#### Power Management

```kconfig
CONFIG_PM=y
CONFIG_PM_SLEEP=y
CONFIG_MFD_RK808=y
CONFIG_REGULATOR_RK808=y
CONFIG_CHARGER_RK817=y
CONFIG_BATTERY_RK817=y
```

#### Storage and WiFi

```kconfig
CONFIG_MMC_DW_ROCKCHIP=y
CONFIG_RTW88=m
CONFIG_RTW88_8821CS=m
CONFIG_RTW88_SDIO=m
```

### 4.2 Gaming Optimizations

```kconfig
CONFIG_HZ_1000=y
CONFIG_PREEMPT=y
CONFIG_NO_HZ_FULL=y
CONFIG_THERMAL=y
CONFIG_CPU_THERMAL=y
```

---

## 5. Kernel Compilation

### 5.1 Build Process

```bash
# Setup toolchain
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

# Configure
make rockchip_defconfig
make menuconfig  # Apply custom changes

# Add device tree
cp device-tree/rk3566-miyoo-flip.dts \
   arch/arm64/boot/dts/rockchip/

# Build
make -j$(nproc) Image dtbs modules

# Results:
# - arch/arm64/boot/Image
# - arch/arm64/boot/dts/rockchip/rk3566-miyoo-flip.dtb
```

### 5.2 Install Modules

```bash
export INSTALL_MOD_PATH=/tmp/miyoo-flip-modules
make modules_install
cd /tmp/miyoo-flip-modules
tar czf /tmp/modules.tar.gz lib/
```

---

## 6. Boot and Testing

### 6.1 U-Boot Boot Command

```bash
load mmc 1:1 ${kernel_addr_r} Image
load mmc 1:1 ${fdt_addr_r} rk3566-miyoo-flip.dtb
setenv bootargs "console=ttyS2,1500000 root=/dev/mmcblk1p2 rw rootwait"
booti ${kernel_addr_r} - ${fdt_addr_r}
```

### 6.2 Verify Hardware

```bash
# Input devices
ls /dev/input/
evtest /dev/input/event0  # buttons
evtest /dev/input/event1  # joystick

# Display
cat /sys/class/graphics/fb0/modes

# ADC
cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw

# Backlight
echo 50 > /sys/class/backlight/backlight/brightness
```

---

## 7. Troubleshooting

### 7.1 Common Issues

**Kernel Panic - No Root Device:**
```bash
setenv bootargs "root=/dev/mmcblk1p2 rootwait rw debug"
```

**Display Not Working:**
- Check: `CONFIG_ROCKCHIP_DW_MIPI_DSI=y`
- Check: `CONFIG_DRM_PANEL_SIMPLE=y`
- Enable DRM debug: `drm.debug=0x1e`

**Analog Sticks Not Detected:**
```bash
# Check ADC channels
cat /sys/bus/iio/devices/iio:device0/in_voltage*_raw

# Verify driver
dmesg | grep adc-joystick

# Enable in defconfig
CONFIG_INPUT_ADC_JOYSTICK=y
```

**WiFi Not Working:**
```bash
modprobe rtw88_8821cs
dmesg | grep mmc2
ls /lib/firmware/rtw88/
```

---

## 8. References

- [Linux Device Tree Specification](https://devicetree-specification.readthedocs.io/)
- [Rockchip Linux Kernel](https://github.com/rockchip-linux/kernel)
- [Panfrost GPU Driver](https://docs.kernel.org/gpu/panfrost.html)

---

## 9. Phase 2 Conclusions

### 9.1 Deliverables Completed

✅ Complete device tree source (`rk3566-miyoo-flip.dts`)  
✅ Full hardware mapping (CPU, GPU, display, input, power, storage)  
✅ Optimized kernel defconfig (CONFIG_* flags)  
✅ Step-by-step kernel compilation guide  
✅ Boot procedure and troubleshooting  

### 9.2 Next Steps

**Phase 3 (Recommended):**
- Port U-Boot bootloader for RK3566
- Create boot firmware image
- Flash procedure and recovery

**Phase 4:**
- Userspace integration (Onion OS Phase 1 modifications)
- Driver testing and tuning
- Performance optimization

### 9.3 Important Notes

⚠️ **WARNING:**
- The provided device tree is a **starting template**
- GPIO pin numbers must be verified with real hardware
- Some vendors may use different chips (WiFi, display panel)
- Extensive UART debug testing is **essential**

🔧 **RECOMMENDATIONS:**
- Start with minimal config (CPU, UART, SD card)
- Add components incrementally
- Keep UART always connected for debugging
- Maintain frequent backups of working configurations

---

**Document Prepared By:** Onion OS porting technical analysis  
**Version:** 1.0  
**Status:** Phase 2 Complete - Ready for hardware testing  
**Next Phase:** Phase 3 - Bootloader and Firmware
