# Phase 3 – Bootloader and First Boot: U-Boot for Miyoo Flip

**Date:** February 2026  
**Onion Version:** 4.4.0-beta  
**Target:** Miyoo Flip with Rockchip RK3566  
**U-Boot Version:** 2023.10+ or Rockchip vendor fork

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Toolchain and Build Environment](#2-toolchain-and-build-environment)
3. [U-Boot for RK3566](#3-u-boot-for-rk3566)
4. [SD Card Partition Structure](#4-sd-card-partition-structure)
5. [Building U-Boot and Firmware](#5-building-u-boot-and-firmware)
6. [Creating Bootable SD Image](#6-creating-bootable-sd-image)
7. [First Boot and Testing](#7-first-boot-and-testing)
8. [Anti-Brick Checklist](#8-anti-brick-checklist)
9. [Recovery Procedures](#9-recovery-procedures)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Introduction

This phase covers the **U-Boot bootloader** and creation of a bootable SD image for Miyoo Flip. U-Boot is the standard bootloader for embedded Linux devices, including Rockchip SoCs like the RK3566.

### 1.1 Phase 3 Objectives

- ✅ ARM64 cross-compilation toolchain setup
- ✅ Build U-Boot for RK3566 Miyoo Flip
- ✅ Optimized SD partition structure
- ✅ Kernel + DTB integration (from Phase 2)
- ✅ Complete bootable SD image
- ✅ Safe first boot procedures (anti-brick)

### 1.2 Boot Process Overview

```
Power On → BootROM → TPL/SPL → U-Boot → Kernel → Init System
```

### 1.3 Rockchip Boot Components

**Required binaries (rkbin):**
- `rk3568_ddr_1560MHz_v1.16.bin` - DRAM init
- `rk3568_bl31_v1.43.elf` - ARM Trusted Firmware
- `rk3568_spl_loader_v1.08.111.bin` - SPL loader

---

## 2. Toolchain and Build Environment

### 2.1 ARM64 Toolchain

**Installation on Ubuntu/Debian:**

```bash
sudo apt-get update
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    device-tree-compiler \
    u-boot-tools

# Verify
aarch64-linux-gnu-gcc --version
dtc --version
mkimage -V
```

### 2.2 Rockchip Tools (rkbin)

```bash
git clone https://github.com/rockchip-linux/rkbin.git
cd rkbin
ls bin/rk35/  # Contains RK3566/RK3568 binaries
```

### 2.3 Environment Variables

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-
export RKBIN_PATH=/path/to/rkbin
```

---

## 3. U-Boot for RK3566

### 3.1 U-Boot Sources

**Mainline U-Boot (recommended):**
```bash
git clone https://source.denx.de/u-boot/u-boot.git
cd u-boot
git checkout v2023.10
```

**Rockchip Vendor U-Boot:**
```bash
git clone https://github.com/rockchip-linux/u-boot.git
```

### 3.2 U-Boot Configuration

```bash
cd u-boot
make rk3568_defconfig
make menuconfig
```

**Essential configs:**
```kconfig
CONFIG_ROCKCHIP_RK3568=y
CONFIG_MMC_DW_ROCKCHIP=y
CONFIG_DM_PMIC=y
CONFIG_PMIC_RK8XX=y
```

### 3.3 Boot Script

**File:** `boot.cmd`

```bash
echo "=== Miyoo Flip Boot ==="
setenv bootargs "console=ttyS2,1500000 root=/dev/mmcblk1p2 rw rootwait"

if load mmc 1:1 ${kernel_addr_r} Image; then
    if load mmc 1:1 ${fdt_addr_r} rk3566-miyoo-flip.dtb; then
        booti ${kernel_addr_r} - ${fdt_addr_r}
    fi
fi
```

Compile: `mkimage -C none -A arm64 -T script -d boot.cmd boot.scr`

---

## 4. SD Card Partition Structure

### 4.1 Partition Layout

```
Offset      Size        Partition       Filesystem
────────────────────────────────────────────────────
0           32KB        Reserved        -
32KB        16MB        U-Boot          raw
16MB+32KB   256MB       Boot            FAT32
~272MB      8GB         RootFS          ext4
~8.3GB      Remaining   User/ROMs       exFAT
```

### 4.2 Creating Partitions

```bash
#!/bin/bash
SD_DEVICE=$1

# Clean and create GPT
sudo dd if=/dev/zero of=$SD_DEVICE bs=1M count=100
sudo parted -s $SD_DEVICE mklabel gpt

# Partitions
sudo parted -s $SD_DEVICE mkpart primary fat32 16.5M 272M
sudo parted -s $SD_DEVICE mkpart primary ext4 272M 8464M
sudo parted -s $SD_DEVICE mkpart primary 8464M 100%

# Format
sudo mkfs.vfat -F 32 -n BOOT ${SD_DEVICE}1
sudo mkfs.ext4 -L rootfs ${SD_DEVICE}2
sudo mkfs.exfat -n USERDATA ${SD_DEVICE}3
```

---

## 5. Building U-Boot and Firmware

### 5.1 Build U-Boot

```bash
cd u-boot
export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-

make mrproper
make rk3568_defconfig
make -j$(nproc)

# Output:
# - u-boot.bin
# - u-boot.dtb
# - spl/u-boot-spl.bin
```

### 5.2 Firmware Packaging with rkbin

```bash
# Copy required blobs
cp $RKBIN_PATH/bin/rk35/rk3568_ddr_1560MHz_v1.16.bin ./
cp $RKBIN_PATH/bin/rk35/rk3568_bl31_v1.43.elf ./bl31.elf

# Build with rkbin integration
make BL31=bl31.elf ROCKCHIP_TPL=rk3568_ddr_1560MHz_v1.16.bin

# Output:
# - u-boot-rockchip.bin (combined TPL+SPL+U-Boot)
# - idbloader.img
```

---

## 6. Creating Bootable SD Image

### 6.1 Flash U-Boot to SD

```bash
# Method 1: Combined image
sudo dd if=u-boot-rockchip.bin of=$SD_DEVICE seek=64 bs=512 conv=fsync

# Method 2: Separate idbloader + u-boot.itb
sudo dd if=idbloader.img of=$SD_DEVICE seek=64 bs=512 conv=fsync
sudo dd if=u-boot.itb of=$SD_DEVICE seek=16384 bs=512 conv=fsync

sudo sync
```

**Offset explanation:**
- `seek=64` → 32KB (BootROM standard offset)
- `seek=16384` → 8MB (U-Boot proper offset)

### 6.2 Populate Boot Partition

```bash
sudo mount ${SD_DEVICE}1 /mnt/boot

# Copy kernel and DTB (from Phase 2)
sudo cp Image /mnt/boot/
sudo cp rk3566-miyoo-flip.dtb /mnt/boot/
sudo cp boot.scr /mnt/boot/

sudo sync && sudo umount /mnt/boot
```

### 6.3 Populate RootFS

```bash
sudo mount ${SD_DEVICE}2 /mnt/root

# Extract Onion OS rootfs
sudo tar xzf onion-rootfs.tar.gz -C /mnt/root

# Configure fstab
sudo tee /mnt/root/etc/fstab << EOF
/dev/mmcblk1p2  /           ext4    defaults,noatime  0 1
/dev/mmcblk1p1  /boot       vfat    defaults          0 2
/dev/mmcblk1p3  /mnt/SDCARD exfat   defaults,nofail   0 0
EOF

sudo sync && sudo umount /mnt/root
```

---

## 7. First Boot and Testing

### 7.1 Hardware Preparation

**Pre-boot checklist:**
- [ ] UART adapter connected (TX, RX, GND)
- [ ] SD card inserted
- [ ] Battery charged or USB power
- [ ] Serial console configured (1500000 baud)

**Serial console:**
```bash
screen /dev/ttyUSB0 1500000
```

### 7.2 Expected Boot Sequence

```
DDR V1.16 ...
LPDDR4X, 1560MHz
...
U-Boot SPL 2023.10
U-Boot 2023.10
Model: Miyoo Flip Handheld Console
...
Booting kernel...
[    0.000000] Booting Linux on physical CPU 0x0
[    0.000000] Machine model: Miyoo Flip Handheld Console
...
```

### 7.3 Useful U-Boot Commands

```bash
# U-Boot shell
=> mmc list
=> ls mmc 1:1
=> load mmc 1:1 ${kernel_addr_r} Image
=> booti ${kernel_addr_r} - ${fdt_addr_r}
```

---

## 8. Anti-Brick Checklist

### 8.1 Pre-Flash Verifications

- [ ] **Backup stock firmware**
  ```bash
  dd if=/dev/mmcblk0 of=stock-backup.img bs=4M
  ```
- [ ] **Verify binary integrity** (checksums)
- [ ] **Test on sacrificial SD card** (NOT internal eMMC)
- [ ] **UART connected and tested**
- [ ] **Battery fully charged** (>80%)
- [ ] **Recovery SD card prepared**

### 8.2 Safe Flashing Procedure

```bash
# ALWAYS test on SD first, NEVER on eMMC
SD=/dev/sdb  # Verify with lsblk!

# Backup before flash
sudo dd if=$SD of=sd-backup.img bs=4M

# Flash U-Boot
sudo dd if=u-boot-rockchip.bin of=$SD seek=64 bs=512 conv=fsync

# Verify checksum after flash
sudo dd if=$SD skip=64 bs=512 count=32768 | md5sum
```

### 8.3 eMMC Protection

**IMPORTANT:** Never flash to eMMC until SD boot is fully tested.

```bash
# Identify devices correctly
lsblk
# mmcblk0 - internal eMMC (DO NOT TOUCH)
# mmcblk1 - external SD (SAFE for testing)

SD_DEVICE=/dev/mmcblk1  # SD card
# NEVER: /dev/mmcblk0  # eMMC (brick risk)
```

---

## 9. Recovery Procedures

### 9.1 Recovery SD Card

**Prepare recovery SD before any modifications:**

```bash
# Flash stock firmware to dedicated "Recovery" SD
sudo dd if=stock-firmware.img of=/dev/sdb bs=4M

# Label physically as "RECOVERY - DO NOT ERASE"
```

### 9.2 Unbrick via Maskrom Mode

**If device completely bricked:**

**Enter Maskrom Mode:**
1. Remove SD card and eMMC (if socketed)
2. Short Maskrom pins on PCB
3. Connect USB-C to PC
4. Power on while pins shorted

**Verify Maskrom:**
```bash
lsusb | grep Rockchip
# Output: ID 2207:330c Rockchip RK3566 in Maskrom mode
```

**Flash via rkdeveloptool:**
```bash
# Install rkdeveloptool
git clone https://github.com/rockchip-linux/rkdeveloptool
cd rkdeveloptool
autoreconf -i && ./configure && make && sudo make install

# Flash bootloader
sudo rkdeveloptool db rk3568_loader_v1.08.111.bin
sudo rkdeveloptool wl 0x40 u-boot-rockchip.bin

# Reboot
sudo rkdeveloptool rd
```

---

## 10. Troubleshooting

### 10.1 U-Boot Doesn't Start

**Symptom:** No UART output, black screen

**Solutions:**
```bash
# Re-flash U-Boot with correct offset
sudo dd if=u-boot-rockchip.bin of=$SD_DEVICE seek=64 bs=512 conv=fsync

# Verify binary contains all components (should be ~2-4MB)
ls -lh u-boot-rockchip.bin

# Try separate idbloader + u-boot.itb
sudo dd if=idbloader.img of=$SD_DEVICE seek=64 bs=512
sudo dd if=u-boot.itb of=$SD_DEVICE seek=16384 bs=512
```

### 10.2 U-Boot Starts But Kernel Doesn't Boot

**Symptom:** U-Boot works, kernel boot fails

**Solutions:**
```bash
# Verify kernel architecture
file Image
# Expected: "Linux kernel ARM64 boot executable Image"

# Test manual boot with debug
=> setenv bootargs "console=ttyS2,1500000 root=/dev/mmcblk1p2 rw rootwait debug"
=> load mmc 1:1 ${kernel_addr_r} Image
=> load mmc 1:1 ${fdt_addr_r} rk3566-miyoo-flip.dtb
=> booti ${kernel_addr_r} - ${fdt_addr_r}
```

### 10.3 Kernel Boots But Userspace Fails

**Symptom:** Kernel starts, then crash in init

**Solutions:**
```bash
# Check rootfs integrity
sudo fsck.ext4 -f ${SD_DEVICE}2

# Mount and verify rootfs
sudo mount ${SD_DEVICE}2 /mnt
ls -la /mnt/  # Should contain: bin/ sbin/ usr/ etc/

# Verify init binary
ls -la /mnt/sbin/init

# Boot to rescue shell
=> setenv bootargs "... init=/bin/sh"
```

---

## Phase 3 Conclusions

### Deliverables Completed

✅ ARM64 toolchain setup guide  
✅ U-Boot configured for RK3566  
✅ Optimized SD partition structure  
✅ U-Boot + firmware build procedure  
✅ Bootable SD image creation scripts  
✅ First boot procedures and testing  
✅ Complete anti-brick checklist  
✅ Detailed recovery procedures  
✅ Comprehensive troubleshooting guide  

### Next Steps

**Phase 4 (Integration):**
- Port Onion OS userspace to RK3566
- Apply Phase 1 modifications (47+ files)
- Rebuild RetroArch + emulator cores
- Complete hardware testing

---

**Document Prepared By:** Onion OS porting technical analysis  
**Version:** 1.0  
**Status:** Phase 3 Complete - Bootable SD creation ready  
**Next Phase:** Phase 4 - Onion OS Userspace Integration
