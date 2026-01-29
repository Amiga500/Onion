# Guida alla Risoluzione dei Problemi - ProfileManager

## Il ProfileManager Si Chiude Senza Messaggi di Errore

Se il ProfileManager mostra "loading" e poi torna al menu delle app senza mostrare errori, segui questi passaggi:

### 1. Controlla il Log di Debug

Il ProfileManager ora crea un log dettagliato che mostra esattamente dove si verifica il problema.

**Come controllare il log:**

```bash
# Collega il dispositivo al PC o usa SSH/Telnet
cd /mnt/SDCARD/.tmp_update/logs
cat profile_menu_debug.log
```

Il log mostrerà qualcosa come:

```
2026-01-29 17:56:20: === ProfileManager Starting ===
2026-01-29 17:56:20: SYSDIR: /mnt/SDCARD/.tmp_update
2026-01-29 17:56:20: Checking for profile_manager.sh
2026-01-29 17:56:20: profile_manager.sh found
2026-01-29 17:56:20: Sourcing profile_manager.sh
2026-01-29 17:56:20: ERROR: Failed to source profile_manager.sh
```

**L'ultima riga del log** mostra dove si è verificato il problema.

### 2. Problemi Comuni e Soluzioni

#### Problema: "profile_manager.sh not found"
**Causa:** File del sistema profili mancanti

**Soluzione:** 
- Reinstalla Onion OS
- Oppure scarica i file mancanti dal repository

#### Problema: "Failed to source profile_manager.sh"
**Causa:** Errore di sintassi o dipendenze mancanti

**Soluzione:**
```bash
# Controlla i permessi
chmod +x /mnt/SDCARD/.tmp_update/script/profiles/*.sh

# Verifica la sintassi
sh -n /mnt/SDCARD/.tmp_update/script/profiles/profile_manager.sh
```

#### Problema: "profile_init failed"
**Causa:** Impossibile inizializzare il sistema profili (problema SD card)

**Soluzione:**
```bash
# Verifica che la SD card sia scrivibile
touch /mnt/SDCARD/test.txt && rm /mnt/SDCARD/test.txt

# Controlla lo spazio disponibile
df -h /mnt/SDCARD

# Verifica i permessi della directory Profiles
ls -la /mnt/SDCARD/Profiles/
mkdir -p /mnt/SDCARD/Profiles
```

#### Problema: "shellect.sh not found" o "not executable"
**Causa:** Sistema menu mancante o senza permessi di esecuzione

**Soluzione:**
```bash
# Rendi eseguibile shellect.sh
chmod +x /mnt/SDCARD/.tmp_update/script/shellect.sh
```

### 3. Se il Log Non Esiste

Se il file `profile_menu_debug.log` non esiste, significa che lo script non è stato eseguito affatto.

**Possibili cause:**
1. File `profile_menu.sh` mancante
2. File `launch.sh` mancante in `/mnt/SDCARD/App/ProfileManager/`
3. Permessi errati

**Verifica:**
```bash
# Controlla che i file esistano
ls -la /mnt/SDCARD/App/ProfileManager/
ls -la /mnt/SDCARD/.tmp_update/script/profiles/

# Rendi tutto eseguibile
chmod +x /mnt/SDCARD/App/ProfileManager/launch.sh
chmod +x /mnt/SDCARD/.tmp_update/script/profiles/*.sh
```

### 4. Alternativa: Usa la CLI

Se il ProfileManager continua a non funzionare, puoi usare l'interfaccia a riga di comando:

```bash
# Lista tutti i profili
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh list

# Crea un profilo
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh create "MioProfilo" normal

# Cambia profilo
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh switch "MioProfilo"

# Cancella un profilo
/mnt/SDCARD/.tmp_update/script/profiles/profile_cli.sh delete "VecchioProfilo"
```

### 5. Segnala il Problema

Se hai controllato il log e non riesci a risolvere, per favore:

1. Copia il contenuto completo di `profile_menu_debug.log`
2. Segnala il problema includendo il log
3. Indica quale operazione stavi cercando di fare

Questo aiuterà a identificare e risolvere il problema.

### Informazioni sul Sistema

**Posizione dei file:**
- Script: `/mnt/SDCARD/.tmp_update/script/profiles/`
- Log: `/mnt/SDCARD/.tmp_update/logs/profile_menu_debug.log`
- Dati profili: `/mnt/SDCARD/Profiles/`
- App: `/mnt/SDCARD/App/ProfileManager/`

**Il sistema profili NON influenza l'avvio:**
- I profili NON vengono caricati all'avvio
- Se il dispositivo è lento ad avviarsi, non è colpa dei profili
- Il sistema profili funziona solo quando lo attivi manualmente

### Contatto

Per supporto, allega sempre il file `profile_menu_debug.log` completo.
