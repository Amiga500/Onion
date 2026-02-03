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
