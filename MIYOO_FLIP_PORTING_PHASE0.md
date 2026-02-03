# Fase 0 – Ricerca e pianificazione iniziale: Porting di Onion OS su Miyoo Flip

**Data di analisi:** Febbraio 2026  
**Versione Onion corrente:** 4.2+  
**Target device:** Miyoo Flip (clamshell con Rockchip RK3566)

---

## 1. Stato Attuale del Porting

### 1.1 Supporto Ufficiale
**Stato:** ❌ **Nessun supporto ufficiale**

Onion OS è attualmente sviluppato esclusivamente per:
- **Miyoo Mini** (MIYOO283) - Allwinner F1C100s/F1C200s
- **Miyoo Mini Plus** (MIYOO354) - Allwinner F1C500s

**Evidenze dal codice:**
- `src/common/system/device_model.h`: definisce solo `MIYOO283` e `MIYOO354`
- `src/common/system/keymap_hw.h`: mappatura input limitata a D-pad e pulsanti base (no dual analog)
- Nessun riferimento a RK3566, Rockchip, o Flip nel codebase

### 1.2 Ricerca GitHub/Community
**Risultati delle ricerche (fino a Febbraio 2026):**

- ✅ Repository OnionUI/Onion: 0 issue/PR relativi a "Miyoo Flip", "RK3566", o "Rockchip"
- ✅ Repository OnionUI (org): 0 discussioni sul porting Flip trovate
- ❓ Reddit r/MiyooMini: non accessibile dalla ricerca GitHub
- ❓ Retro Game Corps/Joey's Retro Handhelds: non accessibili dalla ricerca GitHub

**Conclusione:** Al momento della ricerca (2026), non sono stati trovati fork ufficiali o branch pubblici che tentano il porting su Miyoo Flip nel repository OnionUI.

---

## 2. Principali Blocker Tecnici Identificati

### 2.1 Architettura Hardware Completamente Diversa

| Componente | Miyoo Mini/Mini+ | Miyoo Flip | Impatto |
|------------|------------------|------------|---------|
| **CPU/SoC** | Allwinner F1C100s/F1C500s (ARM926EJ-S) | Rockchip RK3566 (quad-core Cortex-A55) | ⚠️ **CRITICO** |
| **GPU** | Nessuna GPU dedicata | Mali-G52 2EE | ⚠️ **ALTO** |
| **RAM** | 64-128 MB | 1-2 GB | ✅ Vantaggio |
| **Display** | LCD 240x320 (singolo) | LCD dual screen (interno + esterno) | ⚠️ **ALTO** |
| **Input** | D-pad + pulsanti | D-pad + **dual analog sticks** | ⚠️ **MEDIO-ALTO** |

### 2.2 Driver e Kernel

**Problema #1: Driver GPU/Display**
- Miyoo Mini/Mini+: driver framebuffer Allwinner sunxi
- Miyoo Flip: richiede driver Rockchip + Mali GPU
- Impatto: tutto il sistema di rendering va riscritto

**Problema #2: Kernel Linux**
- Attuale: kernel customizzato per Allwinner (3.4.x-4.14.x)
- Richiesto: kernel per RK3566 (5.10+ o mainline 6.x)
- Impatto: ricompilazione completa, possibile incompatibilità con binari esistenti

**Problema #3: Bootloader**
- Attuale: U-Boot per Allwinner
- Richiesto: U-Boot o RKBOOT per Rockchip
- Impatto: gestione partizioni e boot completamente diversa

### 2.3 Input System - Dual Analog Sticks

**Situazione attuale:**
```c
// src/common/system/keymap_hw.h
#define HW_BTN_UP KEY_UP
#define HW_BTN_DOWN KEY_DOWN
// ... solo pulsanti digitali, NO analog stick support
```

**Richiesto per Flip:**
- Supporto per 2 analog stick (eventi ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_RZ)
- Calibrazione analogica
- Dead zone management
- Gestione input ibrido (analogico + digitale)

**Complessità:** ⚠️ MEDIO-ALTA (richiede modifica a keymon, input handling, RetroArch config)

### 2.4 Lid Sensor (Sensore Chiusura Coperchio)

**Problema specifico del clamshell:**
- Miyoo Flip ha un sensore magnetico/meccanico per rilevare apertura/chiusura
- Richiesto: 
  - Driver per lid sensor (GPIO o hall effect sensor)
  - Gestione eventi ACPI-like o custom
  - Trigger per suspend/resume
  - Salvataggio stato prima della chiusura

**Impatto:** ⚠️ ALTO (esperienza utente critica per clamshell)

### 2.5 Vibration Motor

**Situazione attuale:**
```c
// src/common/system/rumble.h
// Supporto rumble esistente ma limitato
```

**Richiesto per Flip:**
- Driver per motore vibrazione (probabilmente GPIO-controlled PWM)
- Integrazione con RetroArch rumble API
- Calibrazione intensità

**Complessità:** ⚠️ BASSA-MEDIA (feature non critica, architettura già presente)

### 2.6 Power Management Avanzato

**Differenze critiche:**
- Mini/Mini+: AXP173/AXP209 PMU (chip Allwinner dedicato)
- Flip: PMU Rockchip RK809/RK817 (completamente diverso)

**Funzionalità da reimplementare:**
- Lettura batteria
- Gestione ricarica
- Suspend/resume
- CPU frequency scaling (cpufreq)
- **Shutdown automatico su chiusura coperchio**

**Complessità:** ⚠️ CRITICA

### 2.7 Storage e Filesystem

**Considerazioni:**
- Mini/Mini+: SD card singola, filesystem ext4/FAT32
- Flip: potrebbero essere presenti eMMC internal + SD card

**Impatto:** ⚠️ MEDIO (layout partizioni da verificare)

---

## 3. Fork e Branch Non Ufficiali

### 3.1 Ricerca Effettuata
- ✅ GitHub OnionUI org: nessun branch o fork pubblico trovato
- ❌ GitLab/Codeberg: non verificato (non accessibile)
- ❌ Forum cinesi (Baidu, Tieba): non verificati

### 3.2 Progetti Correlati da Investigare
1. **MinUI** - OS alternativo per Miyoo, possibile support Flip?
2. **MuOS** - altro OS custom, potrebbe avere branch Flip
3. **Koriki** - fork per altri dispositivi Miyoo
4. **Porting RetroArch standalone** - base comune per emulatori

**Raccomandazione:** Verificare se altri progetti hanno già affrontato il porting RK3566.

---

## 4. Piano di Porting in 10 Fasi Principali

### Fase 1: Ricerca e Preparazione (Priorità: 🔴 ALTA)
**Obiettivo:** Raccogliere tutte le informazioni tecniche sul Miyoo Flip

**Task:**
1. Ottenere specifiche hardware complete del Miyoo Flip
2. Dump firmware stock e analisi kernel/driver
3. Identificare versione kernel e configurazione hardware
4. Mappare GPIO, device tree, layout partizioni
5. Contattare community (Discord Miyoo, Reddit) per informazioni
6. Verificare se esistono SDK/toolchain Rockchip RK3566 adatti

**Deliverable:**
- Documento con specifiche complete hardware
- Dump firmware stock analizzato
- Device tree del Flip

**Stima tempo:** 2-3 settimane  
**Dipendenze:** Accesso fisico a Miyoo Flip

---

### Fase 2: Toolchain e Build Environment (Priorità: 🔴 ALTA)
**Obiettivo:** Setup ambiente di sviluppo cross-compilation per RK3566

**Task:**
1. Setup cross-compiler ARM64 (aarch64-linux-gnu-gcc 11+)
2. Build kernel Linux 5.10+ per RK3566
3. Configurare device tree per Miyoo Flip
4. Setup U-Boot/bootloader per RK3566
5. Creare immagine minimale bootable (Linux + busybox)
6. Testare boot su Flip reale

**Deliverable:**
- Toolchain funzionante
- Kernel Linux custom bootabile
- Script di build automatizzati

**Stima tempo:** 3-4 settimane  
**Dipendenze:** Fase 1 completata

---

### Fase 3: Driver Base - Display e Framebuffer (Priorità: 🔴 ALTA)
**Obiettivo:** Far funzionare l'output video su schermo principale

**Task:**
1. Identificare driver display panel (MIPI DSI, eDP, o LVDS?)
2. Configurare driver Mali G52 o framebuffer semplice
3. Port di SDL2 per nuovo framebuffer device
4. Test rendering base (draw pixel, blit image)
5. Implementare double-buffering
6. Gestione risoluzione e aspect ratio

**Deliverable:**
- Display funzionante con output grafico
- SDL2 compatibile
- Test app che disegna sullo schermo

**Stima tempo:** 4-6 settimane  
**Dipendenze:** Fase 2 completata  
**Blocker critico:** Senza display funzionante, sviluppo bloccato

---

### Fase 4: Driver Input - Pulsanti e Analog Sticks (Priorità: 🔴 ALTA)
**Obiettivo:** Input completo funzionante (D-pad, pulsanti, analog stick)

**Task:**
1. Identificare device input (`/dev/input/eventX`)
2. Mappare tutti i pulsanti fisici a Linux input events
3. Implementare supporto dual analog stick (ABS_X/Y/RX/RY)
4. Aggiungere calibrazione analog stick
5. Gestire dead zone e sensibilità
6. Update `keymap_hw.h` con nuove definizioni
7. Test con SDL_GetJoystickAxis()

**Deliverable:**
- Driver input completo
- Tutti pulsanti e stick mappati
- Tool di calibrazione

**Stima tempo:** 2-3 settimane  
**Dipendenze:** Fase 3 completata

---

### Fase 5: Power Management e Batteria (Priorità: 🟡 MEDIA-ALTA)
**Obiettivo:** Lettura batteria, suspend/resume, CPU scaling

**Task:**
1. Identificare PMU chip (RK809/RK817)
2. Driver per lettura percentuale batteria
3. Driver per stato ricarica
4. Implementare suspend/resume (rtcwake o similar)
5. CPU frequency scaling (cpufreq-dt)
6. Monitoring temperatura (thermal zones)
7. Port di `src/common/system/battery.h` e `thermal.h`

**Deliverable:**
- Lettura batteria accurata
- Suspend/resume funzionante
- CPU scaling per risparmio energetico

**Stima tempo:** 3-4 settimane  
**Dipendenze:** Fase 2-4 completate

---

### Fase 6: Lid Sensor e Gestione Clamshell (Priorità: 🟡 MEDIA-ALTA)
**Obiettivo:** Auto-suspend/resume su chiusura/apertura coperchio

**Task:**
1. Identificare tipo di lid sensor (GPIO, hall effect, switch meccanico)
2. Creare driver/monitor per eventi lid
3. Integrare con power management (auto-suspend on close)
4. Salvataggio stato emulazione prima di suspend
5. Recovery stato dopo resume
6. Gestire edge case (chiusura durante gameplay)

**Deliverable:**
- Lid sensor funzionante
- Auto-suspend/resume smooth
- Salvataggio stato automatico

**Stima tempo:** 2-3 settimane  
**Dipendenze:** Fase 5 completata  
**Note:** Feature distintiva del clamshell, alta priorità UX

---

### Fase 7: Audio e Vibrazione (Priorità: 🟢 MEDIA)
**Obiettivo:** Audio output e rumble/vibrazione

**Task:**
1. Identificare codec audio (probabilmente integrato in RK3566)
2. Configurare ALSA/driver audio
3. Port di SDL_mixer/audio output
4. Test playback audio con vari sample rate
5. Identificare GPIO per vibration motor
6. Implementare PWM driver per intensità vibrazione
7. Integrare con RetroArch rumble API

**Deliverable:**
- Audio funzionante
- Vibrazione controllabile
- Integrazione con emulatori

**Stima tempo:** 2-3 settimane  
**Dipendenze:** Fase 4 completata

---

### Fase 8: Port Applicazioni Onion Core (Priorità: 🔴 ALTA)
**Obiettivo:** Adattare tutte le app Onion per nuovo hardware

**Task:**
1. Ricompilare tutti i binari in `src/` per ARM64
2. Update device detection in `device_model.h` (add MIYOO_FLIP)
3. Port di MainUI per nuova risoluzione
4. Adattare `gameSwitcher` per dual analog
5. Update `batteryMonitorUI` per nuovi driver
6. Test di tutte le app Onion (tweaks, package manager, etc.)
7. Fix di bug specifici ARM64/RK3566

**Deliverable:**
- Tutti i tool Onion funzionanti
- MainUI adattato
- System apps (batmon, tweaks, etc.) funzionanti

**Stima tempo:** 4-6 settimane  
**Dipendenze:** Fasi 3-7 completate

---

### Fase 9: Port RetroArch e Core Emulatori (Priorità: 🔴 ALTA)
**Obiettivo:** Emulatori funzionanti con nuova architettura

**Task:**
1. Build RetroArch per RK3566 con nuovi driver
2. Configurare input mapping per dual analog
3. Port di tutti i core emulatori (NES, SNES, GBA, PSX, N64, etc.)
4. Ottimizzazioni per Mali GPU (use OpenGL ES 3.x)
5. Test performance e tweaking
6. Fix shader/filtri grafici
7. Configurazione audio latency

**Deliverable:**
- RetroArch funzionante
- Almeno 20+ core emulatori stabili
- Performance accettabile (60fps per SNES/GBA)

**Stima tempo:** 6-10 settimane  
**Dipendenze:** Fase 8 completata  
**Note:** Fase più critica per usabilità finale

---

### Fase 10: Testing, Ottimizzazione e Release (Priorità: 🟡 MEDIA)
**Obiettivo:** Stabilizzare, ottimizzare, documentare, rilasciare

**Task:**
1. Testing estensivo di tutte le funzionalità
2. Ottimizzazioni performance (GPU, CPU scaling)
3. Battery life testing e ottimizzazioni
4. Thermal management tuning
5. Creazione installer/immagine SD
6. Documentazione utente
7. Setup repository GitHub per Onion-Flip
8. Release beta pubblica

**Deliverable:**
- Build stabile e testata
- Immagine SD pronta all'uso
- Documentazione completa
- Repository pubblico

**Stima tempo:** 4-6 settimane  
**Dipendenze:** Tutte le fasi precedenti completate

---

## 5. Riepilogo Priorità e Timeline

### Priorità ALTA (Blocker Critici) 🔴
1. **Fase 1** - Ricerca (2-3 settimane)
2. **Fase 2** - Toolchain (3-4 settimane)
3. **Fase 3** - Display (4-6 settimane) ⚠️ **BLOCKER**
4. **Fase 4** - Input (2-3 settimane)
5. **Fase 8** - Port Apps Onion (4-6 settimane)
6. **Fase 9** - Port Emulatori (6-10 settimane) ⚠️ **BLOCKER**

**Subtotale fase critiche:** ~21-32 settimane (5-8 mesi)

### Priorità MEDIA-ALTA 🟡
1. **Fase 5** - Power Management (3-4 settimane)
2. **Fase 6** - Lid Sensor (2-3 settimane)

**Subtotale:** ~5-7 settimane

### Priorità MEDIA 🟢
1. **Fase 7** - Audio/Vibrazione (2-3 settimane)
2. **Fase 10** - Testing/Release (4-6 settimane)

**Subtotale:** ~6-9 settimane

### Timeline Totale Stimata
**BEST CASE:** 32 settimane (~8 mesi)  
**REALISTIC:** 48 settimane (~12 mesi)  
**WORST CASE:** 72+ settimane (18+ mesi)

*Assumendo 1 sviluppatore full-time con esperienza embedded Linux*

---

## 6. Risorse e Requisiti

### 6.1 Hardware Necessario
- ✅ Miyoo Flip device (almeno 2 unità per testing)
- ✅ SD card multiple (per testing e brick recovery)
- ✅ UART/Serial adapter per debug (fondamentale!)
- ✅ Logic analyzer (opzionale ma utile per GPIO)
- ✅ Multimetro per identificare GPIO/power rails

### 6.2 Competenze Richieste
1. **Embedded Linux development** (kernel, drivers, device tree) - ⚠️ CRITICO
2. **Cross-compilation ARM64/Rockchip**
3. **SDL2 e multimedia programming**
4. **RetroArch internals e libretro API**
5. **Hardware debugging e reverse engineering**
6. **C/C++ avanzato**
7. **Git e gestione progetti open source**

### 6.3 Software e Tooling
- Rockchip SDK o Buildroot/Yocto per RK3566
- ARM64 cross-compiler
- U-Boot e kernel Linux sources
- Device tree compiler
- Serial terminal (minicom, screen)
- RetroArch source tree
- OnionUI source tree

---

## 7. Rischi e Mitigazioni

### Rischio 1: Documentazione Hardware Insufficiente 🔴
**Impatto:** CRITICO - Impossibilità di sviluppare driver  
**Probabilità:** ALTA  
**Mitigazione:** 
- Reverse engineering firmware stock
- Contattare produttore Miyoo per specifiche
- Community collaboration

### Rischio 2: Performance Insufficiente 🟡
**Impatto:** MEDIO - Emulazione lenta o audio choppy  
**Probabilità:** MEDIA  
**Mitigazione:**
- Sfruttare GPU Mali G52 per rendering
- CPU overclocking se necessario
- Ottimizzazioni assembly ARM64

### Rischio 3: Brick Device Durante Testing 🟡
**Impatto:** MEDIO-ALTO - Perdita device di test  
**Probabilità:** MEDIA  
**Mitigazione:**
- Backup firmware stock via UART/maskrom mode
- Dual-boot con firmware originale
- Recovery partition

### Rischio 4: Mancanza di Supporto Community 🟢
**Impatto:** BASSO-MEDIO  
**Probabilità:** MEDIA  
**Mitigazione:**
- Documentazione dettagliata
- Release incrementali
- Comunicazione trasparente

---

## 8. Alternative da Considerare

### 8.1 Fork di Altri OS
Invece di portare Onion da zero, valutare:
1. **JelOS/ROCKNIX** - già supportano RK3566 su altri device
2. **ArkOS** - esperienza con Rockchip devices
3. **RetroPie** - base solida, potrebbe essere più semplice

**Pro:** Meno lavoro iniziale, community esistente  
**Contro:** Perdita identità/features Onion

### 8.2 Dual Boot Soluzione
- Mantenere firmware stock Miyoo Flip
- Boot Onion da SD card quando richiesto
- Fallback sicuro al firmware originale

**Pro:** Sicurezza, non-distruttivo  
**Contro:** Complessità bootloader

---

## 9. Conclusioni e Raccomandazioni

### 9.1 Fattibilità Tecnica
**VERDETTO:** ✅ **TECNICAMENTE FATTIBILE** ma **ALTAMENTE COMPLESSO**

Il porting è possibile ma richiede:
- Team di almeno 2-3 sviluppatori esperti
- 12-18 mesi di sviluppo
- Budget per hardware e testing
- Supporto della community

### 9.2 Raccomandazioni Immediate
1. ✅ **Confermare interesse community** - Poll/survey su Discord/Reddit
2. ✅ **Acquisire hardware** - Procurare 2-3 Miyoo Flip per testing
3. ✅ **Setup team** - Reclutare sviluppatori con esperienza embedded
4. ✅ **Dump firmware stock** - PRIMA di modificare qualsiasi cosa
5. ❌ **Non iniziare sviluppo senza specs** - Rischio fallimento alto

### 9.3 Go/No-Go Decision
**CONSIGLIO:** 🟡 **PROCEED WITH CAUTION**

Iniziare con **Fase 1 (Ricerca)** e rivalutare dopo 1 mese:
- Se hardware è reverse-engineerable → GO
- Se mancano specifiche critiche → NO-GO (troppo rischioso)
- Se esiste community supporto attivo → GO
- Se nessuno è interessato/aiuta → NO-GO

### 9.4 Prossimi Step Consigliati
1. Pubblicare questo documento su OnionUI GitHub Discussions
2. Creare poll su Reddit r/MiyooMini per gauge interesse
3. Contattare sviluppatori di MinUI/MuOS per collaborazione
4. Acquisire Miyoo Flip e iniziare reverse engineering
5. Setup repository `OnionUI/Onion-Flip` per tracking progresso

---

## 10. Riferimenti e Risorse

### 10.1 Repository Relevanti
- [OnionUI/Onion](https://github.com/OnionUI/Onion) - Source ufficiale
- [Rockchip Linux](https://github.com/rockchip-linux) - Driver e kernel
- [LibreELEC RK3566](https://github.com/LibreELEC/LibreELEC.tv) - Riferimento per RK3566
- [RetroArch](https://github.com/libretro/RetroArch) - Emulator frontend

### 10.2 Documentazione Tecnica
- [Rockchip RK3566 Datasheet](http://opensource.rock-chips.com/wiki_RK3566) (se disponibile)
- [ARM Mali G52 Documentation](https://developer.arm.com/ip-products/graphics-and-multimedia/mali-gpus)
- [Linux Kernel Device Tree](https://www.kernel.org/doc/html/latest/devicetree/)
- [U-Boot for Rockchip](https://docs.u-boot.org/en/latest/board/rockchip/rockchip.html)

### 10.3 Community e Forum
- Discord OnionUI (verificare se esiste canale #porting)
- Reddit r/MiyooMini
- Retro Game Corps (video reviews/guides)
- Joey's Retro Handhelds (news e discussioni)
- Dingoonity forums (console handheld community)

---

**Documento preparato da:** Analisi tecnica basata su codebase OnionUI  
**Versione:** 1.0  
**Ultimo aggiornamento:** Febbraio 2026  
**Status:** DRAFT - Richiede validation community e hardware access

---

## Appendice A: Glossario Tecnico

- **SoC (System on Chip):** Chip integrato che contiene CPU, GPU, memoria controller, etc.
- **Device Tree:** Struttura dati che descrive hardware al kernel Linux
- **PMU (Power Management Unit):** Chip dedicato alla gestione alimentazione
- **Framebuffer:** Area di memoria per pixel del display
- **Cross-compilation:** Compilare codice su un sistema (x86) per target diverso (ARM)
- **Bootloader:** Software che carica il sistema operativo all'avvio
- **GPIO (General Purpose Input/Output):** Pin programmabili per interfaccia hardware
- **PWM (Pulse Width Modulation):** Tecnica per controllare intensità motori/LED
- **ABS_X/Y (Absolute axis):** Eventi input per joystick analogici
- **Mali GPU:** Serie di GPU ARM per dispositivi embedded
- **UART:** Interfaccia seriale per debug e comunicazione

---

## Appendice B: Changelog

**v1.0 (Febbraio 2026)**
- Prima versione del documento
- Analisi completa basata su ricerca GitHub
- Piano in 10 fasi con stime tempo
- Identificazione blocker critici

---

*Questo documento è un work-in-progress e sarà aggiornato man mano che più informazioni diventano disponibili dalla community e dagli sforzi di reverse engineering.*
