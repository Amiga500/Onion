# Fase 1 – Analisi Hardware e Differenze: Miyoo Mini+ vs Miyoo Flip

**Data analisi:** Febbraio 2026  
**Versione Onion OS:** 4.4.0-beta  
**Scope:** Confronto dettagliato hardware con mappatura file da modificare

---

## Indice

1. [Riepilogo Hardware](#1-riepilogo-hardware)
2. [Architettura CPU/GPU/Kernel](#2-architettura-cpugpukernel)
3. [Display e Touchscreen](#3-display-e-touchscreen)
4. [Sistema Input](#4-sistema-input)
5. [Power Management & Lid Sensor](#5-power-management--lid-sensor)
6. [Audio, PWM e Vibrazione](#6-audio-pwm-e-vibrazione)
7. [Storage](#7-storage)
8. [WiFi e Bluetooth](#8-wifi-e-bluetooth)
9. [Tabella Riepilogativa File](#9-tabella-riepilogativa-file)
10. [Stima Complessità Modifiche](#10-stima-complessità-modifiche)

---

## 1. Riepilogo Hardware

### 1.1 Miyoo Mini+ (Attuale)

| Componente | Specifica |
|------------|-----------|
| **SoC** | Allwinner F1C500s (ARM926EJ-S, single-core @ 900 MHz) |
| **CPU** | ARM926EJ-S (ARMv5TE architecture) |
| **GPU** | Nessuna GPU dedicata (software rendering) |
| **RAM** | 64-128 MB DDR2 |
| **Display** | 2.8" IPS LCD, 640×480, singolo schermo |
| **Touchscreen** | Nessuno |
| **Input** | D-pad, 4 pulsanti face (A/B/X/Y), 4 shoulder (L1/L2/R1/R2), Start, Select, Menu |
| **Analog Sticks** | Nessuno |
| **PMU** | AXP209 o AXP173 (I2C @ 0x34) |
| **Audio** | Codec audio integrato, speaker mono |
| **Vibrazione** | GPIO48 (motor controller) |
| **Storage** | MicroSD singola |
| **WiFi** | Nessuno (alcuni modelli con dongle USB) |
| **Bluetooth** | Nessuno |
| **Form Factor** | Candybar |
| **Batteria** | Li-ion 2000 mAh |

### 1.2 Miyoo Flip (Target)

| Componente | Specifica |
|------------|-----------|
| **SoC** | Rockchip RK3566 (quad-core Cortex-A55 @ 1.8-2.0 GHz) |
| **CPU** | 4× ARM Cortex-A55 (ARMv8.2-A architecture, 64-bit) |
| **GPU** | Mali-G52 2EE (OpenGL ES 3.2, Vulkan 1.1) |
| **RAM** | 1 GB LPDDR4 |
| **Display** | Dual IPS LCD: Interno 3.5" 640×480 + Esterno 1.5" 240×240 |
| **Touchscreen** | Nessuno confermato |
| **Input** | D-pad, 4 face buttons, 4 shoulder, Start, Select, Menu + **2 analog sticks** |
| **Analog Sticks** | Left stick (L3) + Right stick (R3) con click |
| **PMU** | RK809 o RK817 (I2C, integrato in RK3566) |
| **Audio** | Codec RK809/RK817, stereo speakers |
| **Vibrazione** | Linear resonant actuator (LRA) o ERM motor |
| **Storage** | **Dual MicroSD** (System + User) o eMMC interno + MicroSD |
| **WiFi** | 802.11 b/g/n (chip integrato o modulo) |
| **Bluetooth** | BT 4.2/5.0 (possibile) |
| **Form Factor** | **Clamshell** (fold-open, lid sensor) |
| **Batteria** | Li-ion 3000-4000 mAh |

---

## 2. Architettura CPU/GPU/Kernel

### 2.1 Differenze Principali

| Aspetto | Miyoo Mini+ | Miyoo Flip | Impatto |
|---------|-------------|------------|---------|
| **ISA** | ARMv5TE (32-bit) | ARMv8.2-A (64-bit) | ⚠️ CRITICO |
| **Cores** | 1 core @ 900 MHz | 4 cores @ 1.8-2.0 GHz | ⚠️ ALTO |
| **CPU Arch** | ARM926EJ-S (in-order) | Cortex-A55 (out-of-order) | ⚠️ ALTO |
| **GPU** | Nessuna (FB software) | Mali-G52 2EE (HW accel) | ⚠️ CRITICO |
| **Vendor** | Allwinner | Rockchip | ⚠️ CRITICO |
| **Kernel** | Linux 3.4.x-4.14.x (sunxi) | Linux 5.10+ (rockchip) | ⚠️ CRITICO |
| **Bootloader** | U-Boot sunxi | U-Boot rockchip | ⚠️ ALTO |

### 2.2 File Onion OS da Modificare

#### Build System

| File | Modifica Richiesta | Priorità |
|------|-------------------|----------|
| `Makefile` | Aggiungere target `PLATFORM_MIYOOFLIP`, toolchain aarch64 | 🔴 ALTA |
| `src/common/arm_flags.mk` | Nuove flag CPU: `-march=armv8-a -mcpu=cortex-a55` | 🔴 ALTA |
| `src/common/config.mk` | Definire `PLATFORM_MIYOOFLIP` | 🔴 ALTA |
| `src/common/system/device_model.h` | Aggiungere `#define MIYOOFLIP 640` | 🔴 ALTA |

---

## 3. Display e Touchscreen

### 3.1 Differenze Principali

| Aspetto | Miyoo Mini+ | Miyoo Flip | Impatto |
|---------|-------------|------------|---------|
| **Numero schermi** | 1 (singolo interno) | 2 (interno 3.5" + esterno 1.5") | ⚠️ ALTO |
| **Risoluzione interna** | 640×480 | 640×480 | ✅ IDENTICA |
| **Interfaccia** | Framebuffer `/dev/fb0` | MIPI DSI + framebuffer | ⚠️ ALTO |
| **Driver** | sunxi-disp (Allwinner) | rockchip-drm (DRM/KMS) | ⚠️ ALTO |
| **GPU Accel** | Nessuna | Mali-G52 (OpenGL ES, Vulkan) | ⚠️ ALTO |

### 3.2 File da Modificare

| File | Modifica Richiesta | Priorità |
|------|-------------------|----------|
| `src/common/system/display.h` | Supporto dual-display (`/dev/fb0`, `/dev/fb1`) | 🔴 ALTA |
| `src/common/system/display.h` | Gestione DRM/KMS | 🔴 ALTA |

---

## 4. Sistema Input

### 4.1 Differenze Principali

| Aspetto | Miyoo Mini+ | Miyoo Flip | Impatto |
|---------|-------------|------------|---------|
| **D-pad** | ✅ 4 direzioni | ✅ 4 direzioni | ✅ IDENTICO |
| **Analog sticks** | ❌ Nessuno | ✅ **Left + Right stick** | ⚠️ CRITICO |
| **Event types** | `EV_KEY` (digital) | `EV_KEY` + **`EV_ABS`** (analogico) | ⚠️ ALTO |

### 4.2 File da Modificare

| File | Modifica Richiesta | Priorità |
|------|-------------------|----------|
| `src/common/system/keymap_hw.h` | Aggiungere definizioni per analog axes | 🔴 ALTA |
| `src/keymon/input_fd.h` | Gestire eventi `EV_ABS` | 🔴 ALTA |
| `src/keymon/keymon.c` | Supporto eventi analogici in main loop | 🔴 ALTA |

---

## 5. Power Management & Lid Sensor

### 5.1 Differenze Principali

| Aspetto | Miyoo Mini+ | Miyoo Flip | Impatto |
|---------|-------------|------------|---------|
| **PMU Chip** | AXP209 / AXP173 | RK809 / RK817 | ⚠️ CRITICO |
| **Lid Sensor** | ❌ Nessuno | ✅ Hall effect / GPIO | ⚠️ ALTO |

### 5.2 File da Modificare

| File | Modifica Richiesta | Priorità |
|------|-------------------|----------|
| `src/common/system/axp.h` | **Riscrivere completamente** per RK809/RK817 | 🔴 CRITICA |
| `src/common/system/battery.h` | Adattare per RK809 registers | 🔴 ALTA |
| `src/keymon/keymon.c` | Integrare lid sensor monitoring | 🔴 ALTA |

### 5.3 Nuovo File da Creare

| File | Scopo | Priorità |
|------|-------|----------|
| `src/common/system/rk809.h` | RK809/817 PMU driver | 🔴 P0 |
| `src/common/system/lid_sensor.h` | Lid sensor handling | 🔴 P0 |

---

## 6. Audio, PWM e Vibrazione

### 6.1 Differenze

| Aspetto | Miyoo Mini+ | Miyoo Flip | Impatto |
|---------|-------------|------------|---------|
| **Codec Audio** | Allwinner integrato | RK809/RK817 codec | ⚠️ ALTO |
| **Speakers** | Mono | **Stereo** | 🟡 MEDIO |
| **Vibrazione** | GPIO48 (on/off) | PWM controller (intensità) | 🟡 MEDIO |

### 6.2 File da Modificare

| File | Modifica Richiesta | Priorità |
|------|-------------------|----------|
| `src/common/system/rumble.h` | Riscrivere per PWM | 🟡 MEDIA |

---

## 7. Storage

### 7.1 Differenze

| Aspetto | Miyoo Mini+ | Miyoo Flip | Impatto |
|---------|-------------|------------|---------|
| **MicroSD Slots** | 1 (single) | **2 (dual)** o 1 + eMMC | ⚠️ MEDIO |

---

## 8. WiFi e Bluetooth

### 8.1 Differenze

| Aspetto | Miyoo Mini+ | Miyoo Flip | Impatto |
|---------|-------------|------------|---------|
| **WiFi** | ❌ Nessuno | ✅ 802.11 b/g/n | 🟢 BONUS |
| **Bluetooth** | ❌ Nessuno | ✅ BT 4.2/5.0 | 🟢 BONUS |

**Note:** WiFi/BT sono feature extra, non bloccanti per il porting base.

---

## 9. Tabella Riepilogativa File

### File Critici da Modificare

| File | Tipo Modifica | Complessità | Priorità |
|------|---------------|-------------|----------|
| `Makefile` | Major update | ⚠️ ALTA | 🔴 P0 |
| `src/common/system/device_model.h` | Major update | 🟡 MEDIA | 🔴 P0 |
| `src/common/system/keymap_hw.h` | Major update | ⚠️ ALTA | 🔴 P0 |
| `src/common/system/axp.h` | **REWRITE** | ⚠️ CRITICA | 🔴 P0 |
| `src/common/system/display.h` | Major update | ⚠️ ALTA | 🔴 P0 |
| `src/common/system/battery.h` | Major update | ⚠️ ALTA | 🔴 P0 |
| `src/keymon/input_fd.h` | Major update | ⚠️ ALTA | 🔴 P0 |
| `src/keymon/keymon.c` | Major update | ⚠️ ALTA | 🔴 P0 |

---

## 10. Stima Complessità Modifiche

### Classificazione per Area

| Area | # File Critici | LOC Estimate | Effort (giorni) | Rischio |
|------|----------------|--------------|-----------------|---------|
| **Build System** | 5-10 | 500 | 3-5 | 🟡 MEDIO |
| **Display/GPU** | 3-5 | 2000+ | 15-20 | 🔴 ALTO |
| **Input (Analog)** | 3-5 | 1000 | 7-10 | 🔴 ALTO |
| **Power/PMU** | 5-8 | 1500 | 10-15 | 🔴 CRITICO |
| **Lid Sensor** | 2-3 | 500 | 3-5 | 🟡 MEDIO |
| **Audio/Vibrazione** | 2-3 | 300 | 2-3 | 🟢 BASSO |
| **Storage** | 5-10 | 200 | 2-3 | 🟢 BASSO |
| **Testing/Debug** | N/A | N/A | 20-30 | 🔴 ALTO |
| **TOTALE** | **25-47** | **6500+** | **67-98 giorni** | 🔴 ALTO |

### Timeline Dettagliata

| Fase | Attività | Durata |
|------|----------|--------|
| **Settimana 1-2** | Setup toolchain, build system | 5-10 giorni |
| **Settimana 3-6** | Display driver integration | 15-20 giorni |
| **Settimana 7-8** | Input system (analog) | 7-10 giorni |
| **Settimana 9-11** | Power management (RK809) | 10-15 giorni |
| **Settimana 12-13** | Lid sensor | 3-5 giorni |
| **Settimana 14** | Audio/vibrazione | 2-3 giorni |
| **Settimana 16-19** | Testing, debugging | 20-30 giorni |

**TOTALE:** 67-98 giorni lavorativi (14-20 settimane)

---

## Conclusioni

### Riepilogo Impatti

| Categoria | Impatto | File Critici | Effort |
|-----------|---------|--------------|--------|
| CPU/GPU/Kernel | ⚠️ CRITICO | 10+ | 🔴 ALTO |
| Display | ⚠️ ALTO | 3-5 | 🔴 ALTO |
| Input | ⚠️ CRITICO | 5+ | 🔴 ALTO |
| Power/Lid | ⚠️ CRITICO | 8+ | 🔴 ALTO |
| Audio/Vibrazione | 🟡 MEDIO | 2-3 | 🟡 MEDIO |
| Storage | 🟡 MEDIO | 5-10 | 🟢 BASSO |
| WiFi/BT | 🟢 BONUS | 0-3 | 🟢 BASSO |

### Stima Finale

**Modifica codice Onion OS:** 67-98 giorni (14-20 settimane)  
**Totale con kernel/drivers:** 110-154 giorni (22-31 settimane / 5.5-7.5 mesi)

**Raccomandazione:** Questo documento evidenzia che il porting richiede **riscrittura sostanziale** di componenti core (PMU, display, input). Non è un semplice port, ma un **re-engineering** significativo del layer hardware.

---

**Documento preparato da:** Analisi tecnica basata su codebase Onion OS 4.4.0-beta  
**Versione:** 1.0  
**Prossima fase:** Fase 2 - Prototipazione kernel e toolchain
