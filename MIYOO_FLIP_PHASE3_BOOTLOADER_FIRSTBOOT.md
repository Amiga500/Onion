# Fase 3 – Bootloader e Primo Avvio: U-Boot per Miyoo Flip

**Data:** Febbraio 2026  
**Versione Onion:** 4.4.0-beta  
**Target:** Miyoo Flip con Rockchip RK3566  
**U-Boot Version:** 2023.10+ o Rockchip vendor fork

---

## Indice

1. [Introduzione](#1-introduzione)
2. [Toolchain e Ambiente di Build](#2-toolchain-e-ambiente-di-build)
3. [U-Boot per RK3566](#3-u-boot-per-rk3566)
4. [Struttura Partizioni SD Card](#4-struttura-partizioni-sd-card)
5. [Build U-Boot e Firmware](#5-build-u-boot-e-firmware)
6. [Creazione Immagine SD Bootabile](#6-creazione-immagine-sd-bootabile)
7. [Primo Boot e Testing](#7-primo-boot-e-testing)
8. [Checklist Anti-Brick](#8-checklist-anti-brick)
9. [Recovery Procedure](#9-recovery-procedure)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Introduzione

Questa fase copre il **bootloader U-Boot** e la creazione di un'immagine SD bootabile per il Miyoo Flip. U-Boot è il bootloader standard per dispositivi embedded Linux, inclusi i SoC Rockchip come l'RK3566.

### 1.1 Obiettivi Fase 3

- ✅ Setup toolchain ARM64 cross-compilation
- ✅ Build U-Boot per RK3566 Miyoo Flip
- ✅ Struttura partizioni SD ottimizzata
- ✅ Integrazione kernel + DTB (da Phase 2)
- ✅ Immagine SD bootabile completa
- ✅ Procedure primo boot sicure (anti-brick)

### 1.2 Panoramica Boot Process

```
Power On
    ↓
ROM Code (Rockchip BootROM)
    ↓
Carica TPL/SPL da SD/eMMC (offset fisso 32KB)
    ↓
U-Boot SPL (Secondary Program Loader)
    ↓
U-Boot Proper (main bootloader)
    ↓
Carica kernel (Image) + Device Tree (DTB)
    ↓
Linux Kernel Boot
    ↓
Init system (Onion OS userspace)
```

### 1.3 Componenti Rockchip Boot

**Rockchip Boot Flow:**
1. **BootROM** - Hard-coded in SoC, carica TPL/SPL
2. **TPL** (Tertiary Program Loader) - Inizializza DRAM
3. **SPL** (Secondary Program Loader) - Carica U-Boot
4. **U-Boot** - Bootloader principale
5. **Trust Firmware (ATF)** - ARM Trusted Firmware (opzionale)

**Binary necessari (rkbin):**
- `rk3568_ddr_1560MHz_v1.16.bin` - DRAM init
- `rk3568_bl31_v1.43.elf` - ARM Trusted Firmware
- `rk3568_spl_loader_v1.08.111.bin` - SPL loader

---

## 2. Toolchain e Ambiente di Build

### 2.1 ARM64 Toolchain

**Toolchain richiesta:**
- **GCC:** aarch64-linux-gnu-gcc 11+ (recommended 13.x)
- **Binutils:** aarch64-linux-gnu-binutils
- **Device Tree Compiler:** dtc 1.6+
- **Image Tools:** mkimage, mkbootimg

**Installazione su Ubuntu/Debian:**

```bash
# Toolchain base
sudo apt-get update
sudo apt-get install -y \
    gcc-aarch64-linux-gnu \
    g++-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    device-tree-compiler \
    u-boot-tools \
    android-sdk-libsparse-utils

# Python tools per Rockchip
sudo apt-get install -y python3 python3-pip
pip3 install pyelftools

# Verificare installazione
aarch64-linux-gnu-gcc --version
# Output atteso: gcc (Ubuntu/Linaro) 11.x o superiore

dtc --version
# Output atteso: Version: DTC 1.6.x

mkimage -V
# Output atteso: mkimage version 2023.x
```

### 2.2 Rockchip Tools (rkbin)

**Repository rkbin:** Contiene binary blob proprietari per boot Rockchip

```bash
# Clone rkbin repository
git clone https://github.com/rockchip-linux/rkbin.git
cd rkbin

# Binary per RK3566/RK3568 (stesso SoC family)
ls bin/rk35/
# Contiene: rk3568_ddr_*.bin, rk3568_bl31_*.elf, ecc.

# Tools per firmware creation
ls tools/
# Contiene: boot_merger, loaderimage, trust_merger, ecc.
```

### 2.3 Variabili Ambiente

**Setup permanente in ~/.bashrc:**

```bash
# ARM64 Cross-compilation
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

# Rockchip tools path
export RKBIN_PATH=/path/to/rkbin

# U-Boot source path
export UBOOT_SRC=/path/to/u-boot

# Output directory
export BUILD_OUTPUT=/tmp/miyoo-flip-build

# Parallel jobs
export JOBS=$(nproc)

# Source per applicare
source ~/.bashrc
```

---

## 3. U-Boot per RK3566

### 3.1 Sorgenti U-Boot

**Opzioni per sorgenti U-Boot:**

1. **U-Boot Mainline** (Raccomandato per sviluppo)
   ```bash
   git clone https://source.denx.de/u-boot/u-boot.git
   cd u-boot
   git checkout v2023.10  # o latest stable
   ```

2. **Rockchip Vendor U-Boot** (Più driver ma meno mainline)
   ```bash
   git clone https://github.com/rockchip-linux/u-boot.git
   cd u-boot
   git checkout stable-4.19-rock5  # o latest stable branch
   ```

3. **Estrazione da Stock Firmware** (Miyoo Flip specifico)
   ```bash
   # Se si ha accesso al firmware stock
   # Estrarre u-boot.bin da immagine flash
   dd if=/dev/mmcblk0 of=uboot-stock.bin bs=512 skip=64 count=16384
   ```

### 3.2 Configurazione U-Boot per Miyoo Flip

**Defconfig base:** `rk3566_defconfig` o `evb-rk3568_defconfig`

**Creazione custom defconfig:**

```bash
cd $UBOOT_SRC

# Start con config esistente per RK3566
make rk3568_defconfig

# Oppure usa Quartz64 (simile RK3566 board)
make quartz64-a-rk3566_defconfig

# Modifica config per Miyoo Flip
make menuconfig
```

**Opzioni importanti da abilitare:**

```kconfig
# Basic Settings
CONFIG_ARM64=y
CONFIG_ARCH_ROCKCHIP=y
CONFIG_ROCKCHIP_RK3568=y

# Boot Options
CONFIG_BOOTDELAY=3
CONFIG_BOOTCOMMAND="run bootcmd_mmc1; run bootcmd_mmc0"
CONFIG_USE_BOOTCOMMAND=y

# MMC/SD Card Support
CONFIG_MMC=y
CONFIG_MMC_DW=y
CONFIG_MMC_DW_ROCKCHIP=y
CONFIG_MMC_SDHCI=y
CONFIG_MMC_SDHCI_ROCKCHIP=y
CONFIG_SUPPORT_EMMC_BOOT=y

# Display & Video (per splash screen)
CONFIG_VIDEO=y
CONFIG_VIDEO_ROCKCHIP=y
CONFIG_DISPLAY=y
CONFIG_DM_VIDEO=y

# GPIO Support
CONFIG_DM_GPIO=y
CONFIG_ROCKCHIP_GPIO=y

# I2C for PMIC
CONFIG_DM_I2C=y
CONFIG_I2C_ROCKCHIP=y

# PMIC Support (RK809/RK817)
CONFIG_DM_PMIC=y
CONFIG_PMIC_RK8XX=y
CONFIG_DM_REGULATOR=y
CONFIG_DM_REGULATOR_FIXED=y
CONFIG_REGULATOR_RK8XX=y

# USB Support (per fastboot/recovery)
CONFIG_USB=y
CONFIG_USB_XHCI_HCD=y
CONFIG_USB_XHCI_DWC3=y
CONFIG_USB_DWC3=y
CONFIG_USB_GADGET=y
CONFIG_USB_GADGET_DOWNLOAD=y
CONFIG_USB_FUNCTION_FASTBOOT=y

# Filesystem Support
CONFIG_FS_EXT4=y
CONFIG_FS_FAT=y
CONFIG_FAT_WRITE=y

# Command Line Interface
CONFIG_CMD_GPIO=y
CONFIG_CMD_I2C=y
CONFIG_CMD_MMC=y
CONFIG_CMD_USB=y
CONFIG_CMD_EXT4=y
CONFIG_CMD_FAT=y
```

**Salvare configurazione custom:**

```bash
# Salva come defconfig custom
make savedefconfig
cp defconfig configs/miyoo_flip_rk3566_defconfig
```

### 3.3 Device Tree per U-Boot

**U-Boot usa un DTS semplificato (no kernel features):**

**File:** `arch/arm/dts/rk3566-miyoo-flip-u-boot.dtsi`

```dts
// SPDX-License-Identifier: GPL-2.0+
/*
 * U-Boot specific device tree for Miyoo Flip
 */

#include "rk3566-u-boot.dtsi"

/ {
	chosen {
		stdout-path = "serial2:1500000n8";
		u-boot,spl-boot-order = "same-as-spl", &sdmmc0, &sdmmc1;
	};
};

&uart2 {
	u-boot,dm-spl;
	status = "okay";
};

&sdmmc0 {
	u-boot,dm-spl;
	bus-width = <8>;
	cap-mmc-highspeed;
	status = "okay";
};

&sdmmc1 {
	u-boot,dm-spl;
	bus-width = <4>;
	cap-sd-highspeed;
	status = "okay";
};

&sdhci {
	u-boot,dm-spl;
	status = "okay";
};

&i2c0 {
	u-boot,dm-spl;
	status = "okay";
	
	rk809: pmic@20 {
		u-boot,dm-spl;
		compatible = "rockchip,rk809";
		reg = <0x20>;
	};
};

&gpio0 {
	u-boot,dm-spl;
};

&gpio3 {
	u-boot,dm-spl;
};
```

**Aggiungere al Makefile principale:**

```makefile
# In arch/arm/dts/Makefile
dtb-$(CONFIG_ROCKCHIP_RK3568) += \
	rk3566-miyoo-flip.dtb
```

### 3.4 Boot Script U-Boot

**File:** `boot.cmd` (compilato in `boot.scr`)

```bash
# Miyoo Flip U-Boot Boot Script

echo "=== Miyoo Flip Boot Script ==="

# Set environment variables
setenv bootdelay 1
setenv bootargs "console=ttyS2,1500000n8 root=/dev/mmcblk1p2 rw rootwait"

# Kernel and DTB paths
setenv kernel_addr_r 0x02080000
setenv fdt_addr_r 0x08300000
setenv ramdisk_addr_r 0x0a200000

# Try boot from external SD (mmc1) first
if test "${devnum}" = "1"; then
    echo "Booting from external SD card..."
    if load mmc 1:1 ${kernel_addr_r} Image; then
        if load mmc 1:1 ${fdt_addr_r} rk3566-miyoo-flip.dtb; then
            booti ${kernel_addr_r} - ${fdt_addr_r}
        fi
    fi
fi

# Fallback to internal eMMC (mmc0)
echo "Trying internal storage..."
if load mmc 0:1 ${kernel_addr_r} Image; then
    if load mmc 0:1 ${fdt_addr_r} rk3566-miyoo-flip.dtb; then
        booti ${kernel_addr_r} - ${fdt_addr_r}
    fi
fi

# If all fails, drop to U-Boot shell
echo "Boot failed! Dropping to U-Boot shell."
```

**Compilare boot script:**

```bash
mkimage -C none -A arm64 -T script -d boot.cmd boot.scr
```

---

## 4. Struttura Partizioni SD Card

### 4.1 Layout Partizioni

**Schema partizioni ottimizzato per Miyoo Flip:**

```
SD Card Layout (esempio 32GB)

Offset      Size        Partition       Type        Filesystem
─────────────────────────────────────────────────────────────
0           32KB        Reserved        -           -
32KB        16MB        U-Boot          raw         -
16MB+32KB   256MB       Boot            1 (boot)    FAT32
~272MB      8GB         RootFS          2 (root)    ext4
~8.3GB      Remaining   User/Roms       3 (shared)  exFAT

Dettagli:
- Reserved: 0-32KB (BootROM reserved space)
- U-Boot: 32KB-16MB (U-Boot SPL + Proper + ATF + reserve)
- Boot: Kernel, DTB, boot scripts (FAT32 per compatibility)
- RootFS: Onion OS system files (ext4 per permissions)
- User: ROMs, saves, media (exFAT per large files, Windows compat)
```

### 4.2 Creazione Partizioni

**Script automatico `create_sd_partitions.sh`:**

```bash
#!/bin/bash
# create_sd_partitions.sh - Crea partizioni su SD card per Miyoo Flip

SD_DEVICE=$1  # Es: /dev/sdb, /dev/mmcblk0

if [ -z "$SD_DEVICE" ]; then
    echo "Usage: $0 /dev/sdX"
    exit 1
fi

echo "WARNING: This will ERASE ALL DATA on $SD_DEVICE"
echo "Press Ctrl+C to cancel, Enter to continue..."
read

# Unmount tutte le partizioni
sudo umount ${SD_DEVICE}* 2>/dev/null

# Pulire partition table
sudo dd if=/dev/zero of=$SD_DEVICE bs=1M count=100
sudo sync

# Creare nuova partition table GPT
sudo parted -s $SD_DEVICE mklabel gpt

# Partition 1: Boot (FAT32, 256MB, bootable)
sudo parted -s $SD_DEVICE mkpart primary fat32 16.5M 272M
sudo parted -s $SD_DEVICE set 1 boot on

# Partition 2: RootFS (ext4, 8GB)
sudo parted -s $SD_DEVICE mkpart primary ext4 272M 8464M

# Partition 3: User data (exFAT, remaining)
sudo parted -s $SD_DEVICE mkpart primary 8464M 100%

# Sync
sudo sync
sleep 2

# Format partitions
echo "Formatting partitions..."

# Boot partition (FAT32)
sudo mkfs.vfat -F 32 -n BOOT ${SD_DEVICE}1

# RootFS partition (ext4)
sudo mkfs.ext4 -L rootfs ${SD_DEVICE}2

# User partition (exFAT)
sudo mkfs.exfat -n USERDATA ${SD_DEVICE}3

echo "Partitions created successfully!"
sudo parted -s $SD_DEVICE print
```

### 4.3 Montaggio Partizioni

```bash
# Creare mount points
sudo mkdir -p /mnt/miyoo-{boot,root,user}

# Montare partizioni
sudo mount ${SD_DEVICE}1 /mnt/miyoo-boot
sudo mount ${SD_DEVICE}2 /mnt/miyoo-root
sudo mount ${SD_DEVICE}3 /mnt/miyoo-user

# Verificare
df -h | grep miyoo
```

---

## 5. Build U-Boot e Firmware

### 5.1 Build U-Boot

```bash
cd $UBOOT_SRC

# Setup environment
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

# Clean previous builds
make mrproper

# Configure for Miyoo Flip (o usa config esistente come base)
make miyoo_flip_rk3566_defconfig
# Oppure: make evb-rk3568_defconfig

# Build U-Boot
make -j$(nproc)

# Output files:
# - u-boot.bin - U-Boot proper
# - u-boot.dtb - Device tree for U-Boot
# - spl/u-boot-spl.bin - SPL loader
```

### 5.2 Preparazione Firmware con rkbin

**Rockchip richiede packaging speciale dei binary.**

**Step 1: Copia binary rkbin necessari**

```bash
cd $UBOOT_SRC

# Copia DRAM init blob
cp $RKBIN_PATH/bin/rk35/rk3568_ddr_1560MHz_v1.16.bin ./

# Copia ARM Trusted Firmware
cp $RKBIN_PATH/bin/rk35/rk3568_bl31_v1.43.elf ./bl31.elf
```

**Step 2: Build con rkbin integration**

```bash
# U-Boot con Rockchip makefile automatizza questo
make -j$(nproc) BL31=bl31.elf ROCKCHIP_TPL=rk3568_ddr_1560MHz_v1.16.bin

# Output:
# - u-boot-rockchip.bin - Combined TPL + SPL + U-Boot
# - idbloader.img - Boot loader image (alternativa)
```

**Step 3: Generazione idbloader.img (metodo manuale)**

```bash
# Se make non ha generato automaticamente
cd $RKBIN_PATH/tools

# Crea idbloader (TPL + SPL)
./mkimage -n rk3568 -T rksd -d \
    ../bin/rk35/rk3568_ddr_1560MHz_v1.16.bin:$UBOOT_SRC/spl/u-boot-spl.bin \
    idbloader.img

# Verifica dimensione
ls -lh idbloader.img
# Dovrebbe essere ~1-2MB
```

**Step 4: Generazione u-boot.itb (FIT image)**

```bash
cd $UBOOT_SRC

# U-Boot FIT image contiene: U-Boot proper + ATF + DTB
# Generato automaticamente durante make se configurato

ls -lh u-boot.itb
# Output: ~500KB-1MB
```

### 5.3 File Output Richiesti

**Dopo build completo, dovresti avere:**

```
Output Files:
├── idbloader.img        (TPL + SPL, ~1-2MB)
├── u-boot.itb           (U-Boot proper + ATF + DTB, ~500KB)
└── u-boot-rockchip.bin  (Combined all-in-one, opzionale)

Alternative (metodo vecchio):
├── u-boot-spl.bin
├── u-boot.bin
└── u-boot.dtb
```

---

## 6. Creazione Immagine SD Bootabile

### 6.1 Flash U-Boot su SD Card

**Rockchip boot richiede offset specifici:**

```bash
# Metodo 1: Flash combined u-boot-rockchip.bin
sudo dd if=u-boot-rockchip.bin of=$SD_DEVICE seek=64 bs=512 conv=fsync

# Metodo 2: Flash separati idbloader + u-boot.itb
sudo dd if=idbloader.img of=$SD_DEVICE seek=64 bs=512 conv=fsync
sudo dd if=u-boot.itb of=$SD_DEVICE seek=16384 bs=512 conv=fsync

# Sync e verifica
sudo sync
echo "U-Boot flashed to SD card at offset 32KB"
```

**Offset explanation:**
- `seek=64` → 64 × 512 bytes = 32KB (BootROM standard offset)
- `seek=16384` → 16384 × 512 bytes = 8MB (U-Boot proper offset)

### 6.2 Popolamento Boot Partition

```bash
# Montare boot partition
sudo mount ${SD_DEVICE}1 /mnt/miyoo-boot
cd /mnt/miyoo-boot

# Copiare kernel (da Phase 2)
sudo cp $KERNEL_BUILD/arch/arm64/boot/Image ./

# Copiare device tree
sudo cp $KERNEL_BUILD/arch/arm64/boot/dts/rockchip/rk3566-miyoo-flip.dtb ./

# Copiare boot script
sudo cp boot.scr ./

# (Opzionale) Initramfs se necessario
# sudo cp initramfs.cpio.gz ./

# Sync
sudo sync
sudo umount /mnt/miyoo-boot
```

### 6.3 Popolamento RootFS

**Opzioni per rootfs:**

1. **Buildroot rootfs** (minimal)
2. **Debian ARM64** (full-featured)
3. **Onion OS customizzato** (target finale)

**Esempio con Debian ARM64 base:**

```bash
# Download Debian ARM64 rootfs
wget http://http.debian.net/debian/dists/bookworm/main/installer-arm64/current/images/netboot/mini.iso
# Oppure usa debootstrap

# Estrai o installa in partition 2
sudo mount ${SD_DEVICE}2 /mnt/miyoo-root

# Metodo 1: Debootstrap (build from scratch)
sudo debootstrap --arch=arm64 bookworm /mnt/miyoo-root http://deb.debian.org/debian

# Metodo 2: Copia filesystem precompilato Onion OS
sudo tar xzf onion-rootfs.tar.gz -C /mnt/miyoo-root

# Configurazione base
sudo chroot /mnt/miyoo-root /bin/bash
# In chroot:
echo "miyoo-flip" > /etc/hostname
echo "127.0.0.1 localhost miyoo-flip" >> /etc/hosts

# Configura fstab
cat > /etc/fstab << EOF
/dev/mmcblk1p2  /           ext4    defaults,noatime  0 1
/dev/mmcblk1p1  /boot       vfat    defaults          0 2
/dev/mmcblk1p3  /mnt/SDCARD exfat   defaults,nofail   0 0
EOF

# Exit chroot
exit

# Sync e unmount
sudo sync
sudo umount /mnt/miyoo-root
```

### 6.4 Script Completo Creazione Immagine

**File:** `create_bootable_sd.sh`

```bash
#!/bin/bash
# create_bootable_sd.sh - Crea immagine SD completa per Miyoo Flip

set -e

SD_DEVICE=$1
UBOOT_DIR=$2
KERNEL_DIR=$3
ROOTFS_TAR=$4

if [ $# -ne 4 ]; then
    echo "Usage: $0 <sd_device> <uboot_dir> <kernel_dir> <rootfs_tar>"
    echo "Example: $0 /dev/sdb ./u-boot ./linux ./onion-rootfs.tar.gz"
    exit 1
fi

echo "=== Miyoo Flip Bootable SD Creation ==="
echo "SD Device: $SD_DEVICE"
echo "U-Boot: $UBOOT_DIR"
echo "Kernel: $KERNEL_DIR"
echo "RootFS: $ROOTFS_TAR"
echo ""
echo "WARNING: This will ERASE $SD_DEVICE"
read -p "Continue? (yes/no): " confirm
[ "$confirm" != "yes" ] && exit 0

# 1. Creare partizioni
echo "=== Step 1: Creating partitions ==="
./create_sd_partitions.sh $SD_DEVICE

# 2. Flash U-Boot
echo "=== Step 2: Flashing U-Boot ==="
sudo dd if=$UBOOT_DIR/u-boot-rockchip.bin of=$SD_DEVICE seek=64 bs=512 conv=fsync
sudo sync

# 3. Popolare boot partition
echo "=== Step 3: Populating boot partition ==="
sudo mount ${SD_DEVICE}1 /mnt/miyoo-boot
sudo cp $KERNEL_DIR/arch/arm64/boot/Image /mnt/miyoo-boot/
sudo cp $KERNEL_DIR/arch/arm64/boot/dts/rockchip/rk3566-miyoo-flip.dtb /mnt/miyoo-boot/
sudo cp boot.scr /mnt/miyoo-boot/
sudo sync
sudo umount /mnt/miyoo-boot

# 4. Popolare rootfs
echo "=== Step 4: Populating rootfs ==="
sudo mount ${SD_DEVICE}2 /mnt/miyoo-root
sudo tar xzf $ROOTFS_TAR -C /mnt/miyoo-root
sudo sync
sudo umount /mnt/miyoo-root

echo "=== Bootable SD card created successfully! ==="
echo "You can now insert it into Miyoo Flip and power on."
```

---

## 7. Primo Boot e Testing

### 7.1 Preparazione Hardware

**Checklist pre-boot:**

- [ ] UART adapter connesso a GPIO pins (TX, RX, GND)
- [ ] SD card inserita nello slot corretto
- [ ] Batteria carica o alimentazione USB collegata
- [ ] Serial console configurato (115200 o 1500000 baud)

**Connessione UART:**

```
Miyoo Flip      UART Adapter
────────────    ────────────
GPIO_TX2   →    RX
GPIO_RX2   →    TX
GND        →    GND
```

**Software serial console:**

```bash
# Linux
sudo screen /dev/ttyUSB0 1500000

# Oppure minicom
sudo minicom -D /dev/ttyUSB0 -b 1500000

# macOS
screen /dev/cu.usbserial 1500000
```

### 7.2 Sequenza Boot Atteso

**Output UART durante boot normale:**

```
DDR V1.16 a930a f1 20221102
LP4/LP4x derate en, other dram:1x trefi
LPDDR4X, 1560MHz
channel[0] BW=16 Col=10 Bk=8 CS0 Row=16 CS1 Row=16 CS=2 Die BW=8 Size=2048MB
channel[1] BW=16 Col=10 Bk=8 CS0 Row=16 CS1 Row=16 CS=2 Die BW=8 Size=2048MB
change to: 1560MHz
channel[0]: 333MHz
channel[1]: 333MHz
read_mr: mr5=0x50, mr12=0x4d
Channel 0: LPDDR4X, 1560MHz
Channel 1: LPDDR4X, 1560MHz


U-Boot SPL 2023.10-rc4 (Oct 01 2023 - 12:00:00 +0000)
Trying to boot from MMC1
NOTICE:  BL31: v2.9(release):v2.9
NOTICE:  BL31: Built : 10:00:00, Sep 1 2023


U-Boot 2023.10-rc4 (Oct 01 2023 - 12:00:00 +0000)

Model: Miyoo Flip Handheld Console
DRAM:  4 GiB
Core:  311 devices, 28 uclasses, devicetree: separate
MMC:   mmc@fe2b0000: 1, mmc@fe2c0000: 2, sdhci@fe310000: 0
Loading Environment from MMC... OK
In:    serial@fe660000
Out:   serial@fe660000
Err:   serial@fe660000
Model: Miyoo Flip Handheld Console
Net:   No ethernet found.
Hit any key to stop autoboot:  1 
scanning mmc 1:1...
Found boot script /boot.scr
Running bootscript...
=== Miyoo Flip Boot Script ===
Loading kernel...
14520320 bytes read in 620 ms (22.3 MiB/s)
Loading device tree...
65536 bytes read in 5 ms (12.5 MiB/s)
Booting kernel...

[    0.000000] Booting Linux on physical CPU 0x0000000000 [0x412fd050]
[    0.000000] Linux version 5.10.160 (miyoo@builder) #1 SMP PREEMPT
[    0.000000] Machine model: Miyoo Flip Handheld Console
...
```

### 7.3 Comandi U-Boot Utili

**Se boot automatico fallisce, usa U-Boot shell:**

```bash
# U-Boot prompt
=> 

# Lista devices MMC
=> mmc list
mmc@fe2b0000: 1
mmc@fe310000: 0

# Switch a device specifico
=> mmc dev 1
switch to partitions #0, OK

# Lista files in partition
=> ls mmc 1:1
<DIR>       4096 .
<DIR>       4096 ..
        14520320 Image
           65536 rk3566-miyoo-flip.dtb
            4096 boot.scr

# Load manuale kernel + dtb
=> load mmc 1:1 ${kernel_addr_r} Image
14520320 bytes read in 620 ms

=> load mmc 1:1 ${fdt_addr_r} rk3566-miyoo-flip.dtb
65536 bytes read in 5 ms

# Boot
=> booti ${kernel_addr_r} - ${fdt_addr_r}
```

**Comandi diagnostica:**

```bash
# Print environment variables
=> printenv

# Informazioni board
=> bdinfo

# GPIO status
=> gpio status

# I2C scan (per PMIC)
=> i2c dev 0
=> i2c probe

# MMC info dettagliato
=> mmc info

# Test memoria
=> mtest 0x10000000 0x20000000
```

---

## 8. Checklist Anti-Brick

### 8.1 Verifiche Pre-Flash

**Prima di flashare su hardware reale:**

- [ ] **Backup firmware stock** (se possibile)
  ```bash
  dd if=/dev/mmcblk0 of=miyoo-flip-stock-backup.img bs=4M status=progress
  ```

- [ ] **Verificare binary integrity**
  ```bash
  md5sum u-boot-rockchip.bin
  md5sum Image
  md5sum rk3566-miyoo-flip.dtb
  ```

- [ ] **Testare su SD card sacrificabile** (non su eMMC interno)

- [ ] **UART connesso e testato** (essenziale per debug)

- [ ] **Battery fully charged** (>80%)

- [ ] **Recovery SD card preparata** (con firmware funzionante)

### 8.2 Procedura Sicura Flash

**Step-by-step safe flashing:**

```bash
# 1. Prepara SD card di test (NON flashare su eMMC interno)
SD=/dev/sdb  # Verificare con lsblk!

# 2. Backup prima
sudo dd if=$SD of=sd-backup.img bs=4M status=progress

# 3. Flash U-Boot (solo su SD, non eMMC)
sudo dd if=u-boot-rockchip.bin of=$SD seek=64 bs=512 conv=fsync

# 4. Verifica checksum dopo flash
sudo dd if=$SD skip=64 bs=512 count=32768 | md5sum
# Confronta con: md5sum u-boot-rockchip.bin

# 5. Test boot da SD (non rimuovere eMMC ancora)
# Inserisci SD nel Flip e accendi tenendo UART connesso

# 6. Se boot fallisce:
#    - NON rimuovere ancora SD
#    - Analizzare output UART
#    - Ripristinare backup se necessario
```

### 8.3 Safe Boot Options

**U-Boot environment per safe boot:**

```bash
# In U-Boot shell, set safe boot parameters

# Aumenta boot delay per interruzione
=> setenv bootdelay 5

# Set default boot da SD (non eMMC)
=> setenv devnum 1

# Disable auto boot temporaneamente per test
=> setenv bootdelay -1

# Salva ambiente (SOLO se boot funziona)
=> saveenv
```

### 8.4 Protezione eMMC Interno

**IMPORTANTE:** Non flashare mai su eMMC fino a test completo su SD.

```bash
# Identificare correttamente device
lsblk
# Output esempio:
# mmcblk0      - eMMC interno (NON TOCCARE)
# mmcblk1      - SD card esterna (SAFE per testing)

# Usa SEMPRE SD card per primi test
SD_DEVICE=/dev/mmcblk1  # SD card
# MAI: SD_DEVICE=/dev/mmcblk0  # eMMC (rischio brick)
```

---

## 9. Recovery Procedure

### 9.1 Recovery SD Card

**Preparare SD card di recovery prima di qualsiasi modifica:**

```bash
# 1. Download firmware stock (se disponibile)
wget http://example.com/miyoo-flip-stock-firmware.img

# 2. Flash su SD card dedicata "Recovery"
sudo dd if=miyoo-flip-stock-firmware.img of=/dev/sdb bs=4M status=progress

# 3. Etichettare fisicamente SD card come "RECOVERY - DO NOT ERASE"

# 4. Testare che SD boots correttamente
# Inserire nel Flip e verificare boot
```

### 9.2 Unbrick via Maskrom Mode

**Se device completamente brickato (non boot da nessun media):**

**Rockchip Maskrom Mode:** Modalità recovery hardware integrata nel SoC.

**Step 1: Entrare in Maskrom Mode**

```bash
# Metodo 1: Hardware
# 1. Rimuovere SD card e eMMC (se socket presente)
# 2. Cortocircuitare pin Maskrom sul PCB
# 3. Collegare USB-C al PC
# 4. Power on tenendo pin cortocircuitati

# Metodo 2: Software (se U-Boot ancora accessibile)
# In U-Boot shell:
=> rockusb 0 mmc 0  # Entra in USB recovery mode
```

**Step 2: Verificare Maskrom Mode**

```bash
# Linux
lsusb | grep Rockchip
# Output: Bus 001 Device 010: ID 2207:330c Rockchip RK3566 in Maskrom mode

# Windows: Device Manager dovrebbe mostrare "Rockchip Maskrom Device"
```

**Step 3: Flash via rkdeveloptool**

```bash
# Installare rkdeveloptool
git clone https://github.com/rockchip-linux/rkdeveloptool
cd rkdeveloptool
autoreconf -i
./configure
make
sudo make install

# Flash bootloader
sudo rkdeveloptool db rk3568_loader_v1.08.111.bin
sudo rkdeveloptool wl 0x40 u-boot-rockchip.bin

# Flash complete image
sudo rkdeveloptool wl 0x0 miyoo-flip-stock-firmware.img

# Reboot
sudo rkdeveloptool rd
```

### 9.3 Emergency Serial Recovery

**Se UART ancora risponde ma boot fallisce:**

```bash
# Connect via UART
screen /dev/ttyUSB0 1500000

# Interrompere boot (premi qualsiasi tasto durante countdown)
Hit any key to stop autoboot:

# U-Boot prompt attivo
=> 

# Caricare firmware via UART (lento ma funziona)
=> loady ${kernel_addr_r}
# Da host: sz -b Image (xmodem protocol)

=> loady ${fdt_addr_r}
# Da host: sz -b rk3566-miyoo-flip.dtb

# Boot manuale
=> booti ${kernel_addr_r} - ${fdt_addr_r}
```

---

## 10. Troubleshooting

### 10.1 U-Boot Non Si Avvia

**Sintomo:** Nessun output UART, schermo nero

**Possibili cause:**
1. U-Boot flashed a offset sbagliato
2. U-Boot corrotto
3. DRAM init blob mancante/errato
4. Hardware fault

**Soluzioni:**

```bash
# 1. Ri-flash U-Boot con offset corretto
sudo dd if=u-boot-rockchip.bin of=$SD_DEVICE seek=64 bs=512 conv=fsync

# 2. Verificare che binary contenga tutti i componenti
ls -lh u-boot-rockchip.bin
# Dovrebbe essere ~2-4MB (non solo 500KB)

# 3. Usare idbloader + u-boot.itb separati
sudo dd if=idbloader.img of=$SD_DEVICE seek=64 bs=512
sudo dd if=u-boot.itb of=$SD_DEVICE seek=16384 bs=512

# 4. Testare con SD card diversa (possibile incompatibilità)

# 5. Verificare tensione batteria (min 3.7V)
```

### 10.2 U-Boot Avvia Ma Kernel Non Boots

**Sintomo:** U-Boot funziona, ma boot kernel fallisce

**Possibili cause:**
1. Kernel a 32-bit invece di 64-bit
2. DTB mancante o incompatibile
3. Boot arguments errati
4. Kernel panic durante init

**Soluzioni:**

```bash
# Verificare architettura kernel
file Image
# Output corretto: "Linux kernel ARM64 boot executable Image"

# Test boot manuale con debug
=> setenv bootargs "console=ttyS2,1500000n8 root=/dev/mmcblk1p2 rw rootwait debug loglevel=8"
=> load mmc 1:1 ${kernel_addr_r} Image
=> load mmc 1:1 ${fdt_addr_r} rk3566-miyoo-flip.dtb
=> booti ${kernel_addr_r} - ${fdt_addr_r}

# Verificare output kernel panic per identificare problema
```

### 10.3 Kernel Boots Ma Userspace Fallisce

**Sintomo:** Kernel avvia, poi crash o kernel panic in init

**Possibili cause:**
1. Rootfs corrotto o incompatibile (arch mismatch)
2. Init binary mancante o errato
3. Mancanti device nodes
4. Filesystem issues

**Soluzioni:**

```bash
# 1. Verificare rootfs integrity
sudo fsck.ext4 -f ${SD_DEVICE}2

# 2. Montare e controllare rootfs
sudo mount ${SD_DEVICE}2 /mnt
ls -la /mnt/
# Deve contenere: bin/ sbin/ usr/ etc/ lib/

# 3. Verificare init system
ls -la /mnt/sbin/init
# O: ls -la /mnt/usr/lib/systemd/systemd

# 4. Chroot e test
sudo chroot /mnt /bin/bash
file /bin/bash
# Output: ELF 64-bit LSB executable, ARM aarch64

# 5. Aggiungere init debug a kernel bootargs
=> setenv bootargs "... init=/bin/sh"
# Boot in shell rescue
```

### 10.4 Performance Issues

**Sintomo:** Sistema lento, lag, frame drops

**Verifiche:**

```bash
# In Linux shell dopo boot

# 1. CPU frequency scaling
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq
# Dovrebbe mostrare frequenze dinamiche

# 2. CPU governor
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# Dovrebbe essere: schedutil o performance

# 3. Thermal throttling
cat /sys/class/thermal/thermal_zone0/temp
# Temperatura in milliCelsius (es: 45000 = 45°C)

# 4. GPU status
cat /sys/class/devfreq/fb000000.gpu/cur_freq

# 5. Memory usage
free -h

# 6. Process list
top -b -n 1 | head -20
```

---

## 11. Conclusioni Fase 3

### 11.1 Deliverable Completati

✅ Toolchain ARM64 setup completo  
✅ U-Boot configurato per RK3566  
✅ Struttura partizioni SD ottimizzata  
✅ Build procedure U-Boot + firmware  
✅ Script creazione immagine SD bootabile  
✅ Primo boot procedure e testing  
✅ Checklist anti-brick completa  
✅ Recovery procedures dettagliate  
✅ Troubleshooting guide completa  

### 11.2 Files Creati

**Scripts:**
- `create_sd_partitions.sh` - Partition creation
- `create_bootable_sd.sh` - Complete SD image builder
- `boot.cmd` / `boot.scr` - U-Boot boot script

**Config Files:**
- `miyoo_flip_rk3566_defconfig` - U-Boot defconfig
- `rk3566-miyoo-flip-u-boot.dtsi` - U-Boot DTS

**Documentation:**
- Complete step-by-step build guide
- Safe flashing procedures
- Recovery and unbrick guides

### 11.3 Prossimi Passi

**Phase 4 (Integration):**
- Port Onion OS userspace su RK3566
- Apply Phase 1 modifications (47+ files)
- RetroArch + emulator cores rebuild
- Testing completo su hardware reale

**Phase 5 (Optimization):**
- Performance tuning
- Battery life optimization
- Display calibration
- Input latency minimization

### 11.4 Note Importanti

⚠️ **SEMPRE testare prima su SD card sacrificabile**  
⚠️ **Mai flashare su eMMC senza backup**  
⚠️ **UART debug è essenziale per troubleshooting**  
⚠️ **Preparare recovery SD prima di modifiche**  

🔧 **Best Practices:**
- Usare hardware reference (Quartz64) come baseline
- Verificare ogni binary con checksum
- Documentare ogni modifica
- Mantenere backups multipli
- Testare incrementalmente

---

**Documento preparato da:** Analisi tecnica Onion OS porting  
**Versione:** 1.0  
**Status:** Phase 3 Complete - Bootable SD creation ready  
**Next Phase:** Phase 4 - Onion OS Userspace Integration
