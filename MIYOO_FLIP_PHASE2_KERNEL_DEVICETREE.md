# Fase 2 – Kernel e Device Tree: Il Cuore del Porting Miyoo Flip

**Data:** Febbraio 2026  
**Versione Onion:** 4.4.0-beta  
**Target:** Miyoo Flip con Rockchip RK3566  
**Kernel Version:** Linux 5.10+ (raccomandato 5.10.160 o 6.1 LTS)

---

## Indice

1. [Introduzione](#1-introduzione)
2. [Device Tree Source](#2-device-tree-source)
3. [Componenti Hardware Mappati](#3-componenti-hardware-mappati)
4. [Kernel Configuration (defconfig)](#4-kernel-configuration-defconfig)
5. [Compilazione Kernel](#5-compilazione-kernel)
6. [Boot e Testing](#6-boot-e-testing)
7. [Troubleshooting](#7-troubleshooting)

---

## 1. Introduzione

Questa fase copre il **cuore del porting**: la creazione del device tree e la configurazione del kernel Linux per il Miyoo Flip. Il device tree descrive tutto l'hardware presente sul dispositivo al kernel, mentre il defconfig abilita i driver necessari.

### 1.1 Obiettivi Fase 2

- ✅ Device tree completo per RK3566
- ✅ Mappatura hardware (CPU, GPU, display, input, power, storage)
- ✅ Kernel defconfig ottimizzato
- ✅ Istruzioni compilazione kernel bootabile

### 1.2 Prerequisiti

- **Hardware:** Miyoo Flip con UART access
- **Software:** 
  - Kernel Linux 5.10+ sources
  - ARM64 toolchain (aarch64-linux-gnu-gcc 11+)
  - Device tree compiler (dtc)
  - U-Boot per RK3566
- **Conoscenze:** Linux kernel, device tree, Rockchip hardware

---

## 2. Device Tree Source

### 2.1 File Creato

**Location:** `device-tree/rk3566-miyoo-flip.dts`

Questo device tree è stato creato basandosi su:
- **rk3566-quartz64-a.dts** - Pine64 Quartz64 Model A (board di riferimento RK3566)
- **rk3566-anbernic-rgxx3.dts** - Anbernic RG353 series (handheld simile)
- **rk3326-odroid-go2.dts** - ODROID-GO Advance (reference per gaming handheld)

### 2.2 Struttura Device Tree

```
rk3566-miyoo-flip.dts
├── Model & Compatible
├── Aliases (mmc0, mmc1, mmc2)
├── Power Supplies
│   ├── vcc_sys (3.8V battery)
│   └── vcc_3v3 (3.3V regulator)
├── Display
│   ├── panel_internal (640×480 MIPI DSI)
│   └── backlight_internal (PWM4)
├── Input Devices
│   ├── gpio_keys (D-pad, buttons, L3/R3)
│   ├── gpio_switches (lid sensor)
│   └── adc_joystick (dual analog sticks)
├── Vibration Motor (PWM3)
├── CPU Configuration (4× Cortex-A55)
├── GPU (Mali-G52)
├── PMIC (RK809/RK817)
├── Storage
│   ├── sdmmc0 (eMMC/System SD)
│   ├── sdmmc1 (User SD)
│   └── sdmmc2 (WiFi SDIO)
└── Peripherals (UART, I2C, SPI, SARADC)
```

### 2.3 Compilazione Device Tree

```bash
# From kernel source directory
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs

# Or compile single DTS
dtc -I dts -O dtb -o rk3566-miyoo-flip.dtb device-tree/rk3566-miyoo-flip.dts

# Verify compiled DTB
dtc -I dtb -O dts rk3566-miyoo-flip.dtb | less
```

---

## 3. Componenti Hardware Mappati

### 3.1 CPU e Performance

#### CPU Cores (Cortex-A55 × 4)

```dts
&cpu0 {
    cpu-supply = <&vdd_cpu>;
    operating-points-v2 = <&cpu0_opp_table>;
};
```

#### OPP Table (Operating Performance Points)

| Frequenza | Voltage | Uso Tipico |
|-----------|---------|------------|
| 408 MHz | 900 mV | Idle, menu navigation |
| 816 MHz | 900 mV | Retro emulation (8/16-bit) |
| 1104 MHz | 950 mV | GBA, SNES emulation |
| 1416 MHz | 1000 mV | PSX, N64 emulation |
| 1608 MHz | 1050 mV | PSP, DC emulation |
| 1800 MHz | 1150 mV | Max performance |

**CPU Governor:** `schedutil` raccomandato per gaming (dynamic scaling)

### 3.2 GPU Mali-G52

```dts
&gpu {
    mali-supply = <&vdd_gpu>;
    status = "okay";
};
```

**Driver necessario:** `panfrost` (mainline Linux 5.10+)

**Capabilities:**
- OpenGL ES 3.2
- Vulkan 1.1
- 2× execution engines
- Frequenze: 200-800 MHz

### 3.3 Display Panel

#### Internal Display (640×480)

```dts
panel_internal: panel-internal {
    compatible = "innolux,at043tn24", "simple-panel";
    backlight = <&backlight_internal>;
    power-supply = <&vcc_3v3>;
    
    port {
        panel_internal_in: endpoint {
            remote-endpoint = <&dsi0_out>;
        };
    };
};
```

**Interface:** MIPI DSI (Display Serial Interface)  
**Controller:** Rockchip DSI0  
**Resolution:** 640×480 @ 60Hz  
**Backlight:** PWM4 (10 brightness levels)

**Driver:** `panel-simple` o driver specifico panel

### 3.4 Input System

#### GPIO Buttons

**Mappatura completa:**

| Button | GPIO | Linux Code | Descrizione |
|--------|------|------------|-------------|
| D-Up | GPIO3_A0 | BTN_DPAD_UP | D-pad su |
| D-Down | GPIO3_A1 | BTN_DPAD_DOWN | D-pad giù |
| D-Left | GPIO3_A2 | BTN_DPAD_LEFT | D-pad sinistra |
| D-Right | GPIO3_A3 | BTN_DPAD_RIGHT | D-pad destra |
| A | GPIO3_A4 | BTN_EAST | Face button A |
| B | GPIO3_A5 | BTN_SOUTH | Face button B |
| X | GPIO3_A6 | BTN_NORTH | Face button X |
| Y | GPIO3_A7 | BTN_WEST | Face button Y |
| L1 | GPIO3_B0 | BTN_TL | Shoulder left 1 |
| R1 | GPIO3_B1 | BTN_TR | Shoulder right 1 |
| L2 | GPIO3_B2 | BTN_TL2 | Shoulder left 2 |
| R2 | GPIO3_B3 | BTN_TR2 | Shoulder right 2 |
| Select | GPIO3_B4 | BTN_SELECT | Select button |
| Start | GPIO3_B5 | BTN_START | Start button |
| Menu | GPIO3_B6 | KEY_MENU | Menu/Guide button |
| L3 | GPIO3_B7 | BTN_THUMBL | Left stick click |
| R3 | GPIO3_C0 | BTN_THUMBR | Right stick click |

**Driver:** `gpio-keys` (built-in)

#### Analog Sticks (ADC-based)

```dts
adc_joystick_left: adc-joystick-left {
    compatible = "adc-joystick";
    io-channels = <&saradc 0>, <&saradc 1>;
    
    axis@0 {
        linux,code = <ABS_X>;
        abs-range = <0 1023>;
        abs-fuzz = <10>;
        abs-flat = <50>;  /* Deadzone */
    };
};
```

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

```dts
lid {
    label = "Lid Switch";
    gpios = <&gpio0 RK_PA5 GPIO_ACTIVE_LOW>;
    linux,input-type = <EV_SW>;
    linux,code = <SW_LID>;
    wakeup-source;
    debounce-interval = <10>;
};
```

**Type:** Hall effect sensor (magnetic switch)  
**GPIO:** GPIO0_A5  
**Event:** SW_LID (triggers suspend/resume)  
**Driver:** `gpio-keys` (switch variant)

### 3.5 Power Management

#### RK809/RK817 PMIC

```dts
rk809: pmic@20 {
    compatible = "rockchip,rk809";
    reg = <0x20>;
    interrupt-parent = <&gpio0>;
    interrupts = <RK_PA3 IRQ_TYPE_LEVEL_LOW>;
    rockchip,system-power-controller;
    wakeup-source;
    
    regulators {
        vdd_cpu: DCDC_REG1 { ... };
        vdd_gpu: DCDC_REG2 { ... };
        vcc_ddr: DCDC_REG3 { ... };
        /* Additional regulators */
    };
    
    rk809_codec: codec {
        compatible = "rockchip,rk809-codec";
    };
};
```

**Features:**
- 5× DCDC regulators (CPU, GPU, DDR, peripherals)
- 9× LDO regulators
- Battery charger (Li-ion 3.7V)
- Fuel gauge (capacity estimation)
- Power button handling
- RTC (real-time clock)
- Audio codec (stereo output)

**Driver:** `rk808-regulator` (CONFIG_REGULATOR_RK808)

### 3.6 Vibration Motor

```dts
vibrator {
    compatible = "pwm-vibrator";
    pwm-names = "enable";
    pwms = <&pwm3 0 1000000 0>;  /* 1kHz, 1ms period */
    vcc-supply = <&vcc_3v3>;
};
```

**Type:** Linear Resonant Actuator (LRA) o ERM motor  
**Control:** PWM3 (1kHz frequency)  
**Intensity:** Controllabile via duty cycle (0-100%)  
**Driver:** `pwm-vibrator` (CONFIG_INPUT_PWM_VIBRA)

### 3.7 Storage (Dual SD/MMC)

#### SDMMC0 - Internal Storage

```dts
&sdmmc0 {
    status = "okay";
    bus-width = <8>;
    cap-mmc-highspeed;
    mmc-hs200-1_8v;
    non-removable;
};
```

**Usage:** eMMC interno (system partition) o SD card di sistema  
**Speed:** HS200 (up to 200 MB/s)  
**Capacity:** 8-32 GB tipico

#### SDMMC1 - External User SD

```dts
&sdmmc1 {
    status = "okay";
    bus-width = <4>;
    cap-sd-highspeed;
    cd-gpios = <&gpio0 RK_PA6 GPIO_ACTIVE_LOW>;
    disable-wp;
};
```

**Usage:** SD card user removibile (ROMs, saves)  
**Speed:** High-speed (up to 50 MB/s)  
**Detection:** Card detect via GPIO0_A6

#### SDMMC2 - WiFi SDIO

```dts
&sdmmc2 {
    status = "okay";
    bus-width = <4>;
    cap-sdio-irq;
    keep-power-in-suspend;
    non-removable;
    
    wifi: wifi@1 {
        compatible = "realtek,rtl8821cs";
        interrupt-names = "host-wake";
    };
};
```

**WiFi Chip Options:**
- Realtek RTL8821CS (common)
- Broadcom BCM43455 (alternative)

**Driver:** `rtw88` o `brcmfmac`

### 3.8 Peripherals

#### UART2 - Serial Console

```dts
&uart2 {
    status = "okay";
    pinctrl-0 = <&uart2m0_xfer>;
};
```

**Baudrate:** 1500000 (1.5 Mbaud)  
**Usage:** Kernel debug console, bootloader interaction

#### SARADC - ADC Converter

```dts
&saradc {
    status = "okay";
    vref-supply = <&vcc_1v8>;
};
```

**Channels:** 6× 10-bit ADC  
**Reference:** 1.8V  
**Usage:** Analog stick input, battery voltage monitoring

---

## 4. Kernel Configuration (defconfig)

### 4.1 Base Configuration

**File:** `arch/arm64/configs/miyoo_flip_defconfig`

```bash
# Create new defconfig based on rockchip
make ARCH=arm64 rockchip_defconfig
make ARCH=arm64 menuconfig  # Apply changes below
make ARCH=arm64 savedefconfig
mv defconfig arch/arm64/configs/miyoo_flip_defconfig
```

### 4.2 Essential Configs

#### Platform and SoC

```kconfig
CONFIG_ARCH_ROCKCHIP=y
CONFIG_ARM64=y
CONFIG_ARM64_PAGE_SIZE_4KB=y
CONFIG_ARM64_VA_BITS_39=y

# Rockchip specific
CONFIG_ROCKCHIP_PM_DOMAINS=y
CONFIG_ROCKCHIP_IODOMAIN=y
CONFIG_ROCKCHIP_IOMMU=y
CONFIG_ROCKCHIP_SARADC=y
```

#### CPU and Scheduling

```kconfig
CONFIG_SMP=y
CONFIG_NR_CPUS=4
CONFIG_SCHED_MC=y
CONFIG_CPU_FREQ=y
CONFIG_CPU_FREQ_DEFAULT_GOV_SCHEDUTIL=y
CONFIG_CPU_FREQ_GOV_PERFORMANCE=y
CONFIG_CPU_FREQ_GOV_POWERSAVE=y
CONFIG_CPU_FREQ_GOV_ONDEMAND=y
CONFIG_CPUFREQ_DT=y
CONFIG_ARM_ROCKCHIP_CPUFREQ=y
```

#### Power Management

```kconfig
CONFIG_PM=y
CONFIG_PM_SLEEP=y
CONFIG_SUSPEND=y
CONFIG_PM_AUTOSLEEP=y
CONFIG_PM_WAKELOCKS=y
CONFIG_PM_DEBUG=y

# PMIC and Regulators
CONFIG_REGULATOR=y
CONFIG_REGULATOR_FIXED_VOLTAGE=y
CONFIG_MFD_RK808=y
CONFIG_REGULATOR_RK808=y
CONFIG_RTC_DRV_RK808=y
CONFIG_CHARGER_RK817=y
CONFIG_BATTERY_RK817=y
```

#### Display and Graphics

```kconfig
# DRM and KMS
CONFIG_DRM=y
CONFIG_DRM_ROCKCHIP=y
CONFIG_ROCKCHIP_VOP2=y
CONFIG_ROCKCHIP_DW_MIPI_DSI=y
CONFIG_DRM_PANEL_SIMPLE=y

# Mali GPU (Panfrost)
CONFIG_DRM_PANFROST=y
CONFIG_DRM_LIMA=y

# Framebuffer Console
CONFIG_FRAMEBUFFER_CONSOLE=y
CONFIG_LOGO=y
CONFIG_LOGO_LINUX_CLUT224=y

# Backlight
CONFIG_BACKLIGHT_CLASS_DEVICE=y
CONFIG_BACKLIGHT_PWM=y
```

#### Input Devices

```kconfig
# Input core
CONFIG_INPUT=y
CONFIG_INPUT_EVDEV=y
CONFIG_INPUT_JOYSTICK=y

# GPIO Keys
CONFIG_KEYBOARD_GPIO=y
CONFIG_INPUT_KEYBOARD=y
CONFIG_KEYBOARD_GPIO=y

# ADC Joystick (CRITICAL for analog sticks)
CONFIG_INPUT_ADC_JOYSTICK=y

# Force feedback (vibration)
CONFIG_INPUT_FF_MEMLESS=y
CONFIG_INPUT_PWM_VIBRA=y
```

#### MMC/SD Card

```kconfig
CONFIG_MMC=y
CONFIG_MMC_BLOCK=y
CONFIG_MMC_DW=y
CONFIG_MMC_DW_ROCKCHIP=y
CONFIG_MMC_SDHCI=y
CONFIG_MMC_SDHCI_OF_DWCMSHC=y
```

#### WiFi and Networking

```kconfig
CONFIG_WIRELESS=y
CONFIG_CFG80211=y
CONFIG_MAC80211=y

# Realtek WiFi
CONFIG_RTW88=m
CONFIG_RTW88_8821CS=m
CONFIG_RTW88_SDIO=m

# Broadcom WiFi (alternative)
CONFIG_BRCMFMAC=m
CONFIG_BRCMFMAC_SDIO=y
```

#### Audio

```kconfig
CONFIG_SOUND=y
CONFIG_SND=y
CONFIG_SND_SOC=y
CONFIG_SND_SOC_ROCKCHIP=y
CONFIG_SND_SOC_ROCKCHIP_I2S=y
CONFIG_SND_SOC_RK817=y
```

#### USB

```kconfig
CONFIG_USB=y
CONFIG_USB_XHCI_HCD=y
CONFIG_USB_DWC3=y
CONFIG_USB_DWC3_OF_SIMPLE=y
CONFIG_PHY_ROCKCHIP_INNO_USB2=y
CONFIG_PHY_ROCKCHIP_NANENG_COMBO_PHY=y
```

#### Filesystems

```kconfig
CONFIG_EXT4_FS=y
CONFIG_F2FS_FS=y
CONFIG_VFAT_FS=y
CONFIG_EXFAT_FS=y
CONFIG_OVERLAY_FS=y
CONFIG_SQUASHFS=y
CONFIG_SQUASHFS_XZ=y
```

### 4.3 Gaming-Specific Optimizations

```kconfig
# Low latency
CONFIG_HZ_1000=y
CONFIG_PREEMPT=y

# Performance
CONFIG_NO_HZ_FULL=y
CONFIG_RCU_FAST_NO_HZ=y

# Thermal management
CONFIG_THERMAL=y
CONFIG_CPU_THERMAL=y
CONFIG_ROCKCHIP_THERMAL=y
```

### 4.4 Debugging (Development Only)

```kconfig
# Remove for production
CONFIG_DEBUG_KERNEL=y
CONFIG_DEBUG_INFO=y
CONFIG_DYNAMIC_DEBUG=y
CONFIG_MAGIC_SYSRQ=y
```

### 4.5 Complete Defconfig File

**Location:** Create `arch/arm64/configs/miyoo_flip_defconfig`

```bash
# Generate from current config
make ARCH=arm64 savedefconfig
cp defconfig arch/arm64/configs/miyoo_flip_defconfig
```

---

## 5. Compilazione Kernel

### 5.1 Setup Toolchain

```bash
# Install ARM64 cross-compiler
sudo apt-get install gcc-aarch64-linux-gnu

# Verify
aarch64-linux-gnu-gcc --version
# Should show version 11+ recommended
```

### 5.2 Kernel Build Process

#### Step 1: Clone Kernel Source

```bash
# Use Rockchip vendor kernel or mainline
git clone --depth=1 --branch linux-5.10.y \
    https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git

cd linux

# Or use Rockchip's kernel
git clone https://github.com/rockchip-linux/kernel.git -b stable-5.10-rock5
```

#### Step 2: Configure Kernel

```bash
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu-

# Load base config
make rockchip_defconfig

# Or use custom config
make miyoo_flip_defconfig

# Fine-tune config (optional)
make menuconfig
```

#### Step 3: Add Device Tree

```bash
# Copy DTS to kernel tree
cp device-tree/rk3566-miyoo-flip.dts \
   arch/arm64/boot/dts/rockchip/

# Update Makefile
echo "dtb-\$(CONFIG_ARCH_ROCKCHIP) += rk3566-miyoo-flip.dtb" >> \
   arch/arm64/boot/dts/rockchip/Makefile
```

#### Step 4: Build Kernel and DTB

```bash
# Build kernel image
make -j$(nproc) Image

# Build device tree
make dtbs

# Build modules
make modules

# Build everything
make -j$(nproc) Image dtbs modules

# Results:
# - arch/arm64/boot/Image (kernel image)
# - arch/arm64/boot/dts/rockchip/rk3566-miyoo-flip.dtb
```

### 5.3 Install Modules

```bash
# Install to staging directory
export INSTALL_MOD_PATH=/tmp/miyoo-flip-modules
make modules_install

# Create tarball
cd /tmp/miyoo-flip-modules
tar czf /tmp/modules.tar.gz lib/
```

### 5.4 Create Boot Image

```bash
# Install mkbootimg tool
sudo apt-get install android-tools-mkbootimg

# Create boot.img for Android-style boot
mkbootimg \
    --kernel arch/arm64/boot/Image \
    --dtb arch/arm64/boot/dts/rockchip/rk3566-miyoo-flip.dtb \
    --cmdline "console=ttyS2,1500000n8 root=/dev/mmcblk0p2 rw rootwait" \
    --base 0x00200000 \
    --pagesize 2048 \
    --output boot.img
```

---

## 6. Boot e Testing

### 6.1 U-Boot Configuration

**U-Boot command:**

```bash
# Load kernel from SD card
load mmc 1:1 ${kernel_addr_r} Image
load mmc 1:1 ${fdt_addr_r} rk3566-miyoo-flip.dtb

# Set bootargs
setenv bootargs "console=ttyS2,1500000 root=/dev/mmcblk1p2 rw rootwait"

# Boot kernel
booti ${kernel_addr_r} - ${fdt_addr_r}
```

### 6.2 Partition Layout

**Recommended SD card layout:**

```
/dev/mmcblk1p1  - Boot partition (FAT32, 256MB)
                  Contains: Image, DTB, boot script
/dev/mmcblk1p2  - Root filesystem (ext4, remaining space)
                  Contains: Onion OS userspace
```

### 6.3 First Boot Checklist

**UART output should show:**

```
[    0.000000] Booting Linux on physical CPU 0x0
[    0.000000] Linux version 5.10.160
[    0.000000] Machine model: Miyoo Flip Handheld Console
[    1.234567] rockchip-pinctrl pinctrl: probed
[    2.345678] dw-apb-uart ff660000.serial: ttyS2 at MMIO 0xff660000
[    3.456789] mmc0: new HS200 MMC card at address 0001
[    4.567890] input: gpio-keys as /devices/platform/gpio-keys/input0
[    5.678901] adc-joystick: probed
```

**Verify devices:**

```bash
# Check input devices
ls /dev/input/  # Should see event0 (keys), event1 (joystick), event2 (lid)

# Check display
cat /sys/class/graphics/fb0/modes

# Check ADC
cat /sys/bus/iio/devices/iio:device0/in_voltage0_raw

# Check GPIO
cat /sys/kernel/debug/gpio

# Check regulators
cat /sys/kernel/debug/regulator/regulator_summary
```

### 6.4 Testing Hardware

```bash
# Test buttons
evtest /dev/input/event0

# Test analog sticks
evtest /dev/input/event1

# Test lid sensor
evtest /dev/input/event2

# Test PWM vibrator
echo 128 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle

# Test backlight
echo 50 > /sys/class/backlight/backlight/brightness
```

---

## 7. Troubleshooting

### 7.1 Common Issues

#### Issue: Kernel Panic - No Root Device

**Error:** `VFS: Cannot open root device`

**Solution:**
```bash
# Check bootargs in U-Boot
setenv bootargs "root=/dev/mmcblk1p2 rootwait rw"

# Verify partition exists
ls mmc 1:2 /

# Enable more debug
setenv bootargs "${bootargs} debug initcall_debug"
```

#### Issue: Display Not Working

**Error:** Black screen after boot

**Checklist:**
1. Verify MIPI DSI is enabled: `CONFIG_ROCKCHIP_DW_MIPI_DSI=y`
2. Check panel driver: `CONFIG_DRM_PANEL_SIMPLE=y`
3. Verify DTS panel compatible string matches driver
4. Check backlight: `echo 100 > /sys/class/backlight/*/brightness`
5. Enable DRM debug: `drm.debug=0x1e`

#### Issue: Analog Sticks Not Detected

**Error:** No `/dev/input/js0` device

**Solution:**
```bash
# 1. Check ADC channels
cat /sys/bus/iio/devices/iio:device0/in_voltage*_raw

# 2. Verify driver
dmesg | grep adc-joystick

# 3. Enable in defconfig
CONFIG_INPUT_ADC_JOYSTICK=y

# 4. Check device tree node
cat /proc/device-tree/adc-joystick-left/compatible
```

#### Issue: WiFi Not Working

**Error:** `wlan0` interface missing

**Solution:**
```bash
# 1. Check SDIO
dmesg | grep mmc2

# 2. Load driver manually
modprobe rtw88_8821cs  # or brcmfmac

# 3. Verify device tree
cat /sys/firmware/devicetree/base/sdmmc@fe2c0000/wifi@1/compatible

# 4. Check firmware
ls /lib/firmware/rtw88/  # Should contain .bin files
```

### 7.2 Debug Tools

```bash
# Enable dynamic debug
echo "file drivers/mmc/* +p" > /sys/kernel/debug/dynamic_debug/control

# Monitor interrupts
watch -n1 cat /proc/interrupts

# Check clocks
cat /sys/kernel/debug/clk/clk_summary

# Monitor power
cat /sys/kernel/debug/regulator/regulator_summary
```

### 7.3 Performance Monitoring

```bash
# CPU frequency
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq

# GPU frequency
cat /sys/class/devfreq/fb000000.gpu/cur_freq

# Temperature
cat /sys/class/thermal/thermal_zone0/temp

# Memory
free -h
cat /proc/meminfo
```

---

## 8. Riferimenti e Risorse

### 8.1 Upstream Documentation

- [Linux Device Tree Specification](https://devicetree-specification.readthedocs.io/)
- [Rockchip Linux Kernel](https://github.com/rockchip-linux/kernel)
- [ARM Mali GPU Driver (Panfrost)](https://docs.kernel.org/gpu/panfrost.html)
- [Input Subsystem](https://www.kernel.org/doc/html/latest/input/index.html)

### 8.2 Similar Device Trees

- `arch/arm64/boot/dts/rockchip/rk3566-quartz64-a.dts`
- `arch/arm64/boot/dts/rockchip/rk3566-anbernic-rgxx3.dts`
- `arch/arm/boot/dts/rockchip/rk3326-odroid-go2.dts`

### 8.3 Driver Documentation

- GPIO Keys: `Documentation/devicetree/bindings/input/gpio-keys.yaml`
- ADC Joystick: `Documentation/devicetree/bindings/input/adc-joystick.yaml`
- PWM Vibrator: `Documentation/devicetree/bindings/input/pwm-vibrator.yaml`
- RK809 PMIC: `Documentation/devicetree/bindings/mfd/rockchip,rk808.yaml`

---

## 9. Conclusioni Fase 2

### 9.1 Deliverable Completati

✅ Device tree source completo (`rk3566-miyoo-flip.dts`)  
✅ Mappatura completa hardware (CPU, GPU, display, input, power, storage)  
✅ Kernel defconfig ottimizzato (CONFIG_* flags)  
✅ Guida compilazione kernel step-by-step  
✅ Boot procedure e troubleshooting  

### 9.2 Prossimi Passi

**Fase 3 (Raccomandato):**
- Port U-Boot bootloader per RK3566
- Creazione firmware boot image
- Flash procedure e recovery

**Fase 4:**
- Userspace integration (Onion OS modifiche Phase 1)
- Driver testing e tuning
- Performance optimization

### 9.3 Note Importanti

⚠️ **ATTENZIONE:**
- Il device tree fornito è un **template di partenza**
- GPIO pin numbers devono essere verificati con hardware reale
- Alcuni vendor potrebbero usare chip diversi (WiFi, display panel)
- Testing estensivo con UART debug è **essenziale**

🔧 **RACCOMANDAZIONI:**
- Iniziare con configurazione minima (CPU, UART, SD card)
- Aggiungere componenti incrementalmente
- Mantenere UART sempre connesso per debug
- Fare backup frequenti di configurazioni funzionanti

---

**Documento preparato da:** Analisi tecnica Onion OS porting  
**Versione:** 1.0  
**Status:** Phase 2 Complete - Ready for hardware testing  
**Next Phase:** Phase 3 - Bootloader and Firmware
