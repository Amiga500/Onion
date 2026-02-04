# Build Workflow Guide / Guida al Workflow di Build

## 🚨 CRITICAL: Docker Toolchain is REQUIRED / Docker Toolchain è OBBLIGATORIO 🚨

### ⛔ DO NOT TRY TO BUILD NATIVELY / NON COMPILARE NATIVAMENTE ⛔

**English:**
This project **CANNOT** be built directly on your Linux/Mac/Windows system. You **MUST** use the Docker toolchain!

**Why?**
- This project builds firmware for **Miyoo Mini** (ARM architecture)
- Your PC is likely x86/x64 architecture
- The libraries in this repo (`lib/` folder) are **ARM binaries**
- Native compilation will fail with errors like:
  - ❌ `SDL/SDL.h: No such file or directory`
  - ❌ `cannot find -lSDL`
  - ❌ `incompatible .so` files

**Solution:**
Use the Docker toolchain which provides the ARM cross-compilation environment:
```bash
make with-toolchain CMD="all -j$(nproc)"
```

---

**Italiano:**
Questo progetto **NON PUÒ** essere compilato direttamente sul tuo sistema Linux/Mac/Windows. **DEVI** usare la toolchain Docker!

**Perché?**
- Questo progetto compila firmware per **Miyoo Mini** (architettura ARM)
- Il tuo PC è probabilmente architettura x86/x64
- Le librerie in questa repo (cartella `lib/`) sono **binari ARM**
- La compilazione nativa fallirà con errori come:
  - ❌ `SDL/SDL.h: File o directory non esistente`
  - ❌ `impossibile trovare -lSDL`
  - ❌ file `.so` `incompatibile`

**Soluzione:**
Usa la toolchain Docker che fornisce l'ambiente di cross-compilazione ARM:
```bash
make with-toolchain CMD="all -j$(nproc)"
```

---

## ⚠️ Important: Correct Command Syntax / Sintassi Corretta dei Comandi

### ❌ Common Mistakes / Errori Comuni

These commands are **INCORRECT** and will not work as intended:

```bash
# ❌ WRONG - trying to combine incompatible targets
sudo make -j4 all git-submodules

# ❌ WRONG - with-toolchain needs CMD parameter
sudo make -j$(nproc) all with-toolchain
```

### ✅ Correct Command Sequence / Sequenza Corretta

Use these commands **in order**, one after another:

```bash
# 1. Initialize submodules (first time only, or when they change)
make git-submodules

# 2. Clean previous build
sudo make clean

# 3. Build with Docker toolchain using parallel jobs
sudo make with-toolchain CMD="all -j4"
# Or use all CPU cores:
sudo make with-toolchain CMD="all -j$(nproc)"
```

Or as a single command line:
```bash
make git-submodules && sudo make clean && sudo make with-toolchain CMD="all -j$(nproc)"
```

**Key Points:**
- `git-submodules` must be run separately (it's not a build target)
- `with-toolchain` requires `CMD="..."` parameter
- The parallel flags (`-j4` or `-j$(nproc)`) go **inside** the CMD parameter
- Use `sudo` only if needed for file permissions (usually for Docker)

---

## English Version

### Updating and Rebuilding Without Re-cloning

When new changes are pushed to the branch you're working on, follow these steps to update and rebuild:

#### 1. Update Your Local Branch

```bash
# Make sure you're on the correct branch
git checkout copilot/optimize-code-performance

# Fetch the latest changes from remote
git fetch origin

# Pull the latest changes (this will update your local branch)
git pull origin copilot/optimize-code-performance
```

Or simply:
```bash
git pull
```

#### 2. Clean Previous Build Artifacts

The Makefile provides two cleaning options:

**Option A: Standard Clean** (recommended for most cases)
```bash
make clean
```
This removes:
- Build directory (`build/`)
- Distribution directory (`dist/`)
- Test build directory (`build_test/`)
- Temporary config files
- Compiled object files (`.o`)
- Setup cache flag

**Option B: Deep Clean** (for thorough cleanup)
```bash
make deepclean
```
This does everything `clean` does, PLUS:
- Removes all cache files
- Cleans third-party dependencies (RetroArch, SearchFilter, Terminal, DinguxCommander)

⚠️ **Note**: `deepclean` takes longer to rebuild because it cleans third-party dependencies.

#### 3. Rebuild the Project

After cleaning, rebuild with:
```bash
make all
```

Or to create a release package:
```bash
make release
```

### Complete Workflow Example

```bash
# 1. Update the branch
git pull origin copilot/optimize-code-performance

# 2. Clean previous build
make clean

# 3. Rebuild
make all
```

### Common Scenarios

#### Scenario 1: Quick Update (No Major Changes)
```bash
git pull
make clean
make all
```

#### Scenario 2: Major Changes or Build Issues
```bash
git pull
make deepclean
make all
```

#### Scenario 3: Just Update Without Rebuilding
```bash
git pull
# Review changes, then decide whether to rebuild
```

---

## Versione Italiana

### Aggiornare e Ricompilare Senza Riclonare

Quando vengono pubblicate nuove modifiche sul branch su cui stai lavorando, segui questi passaggi per aggiornare e ricompilare:

#### 1. Aggiorna il Tuo Branch Locale

```bash
# Assicurati di essere sul branch corretto
git checkout copilot/optimize-code-performance

# Scarica le ultime modifiche dal remote
git fetch origin

# Aggiorna il branch locale con le modifiche remote
git pull origin copilot/optimize-code-performance
```

Oppure semplicemente:
```bash
git pull
```

#### 2. Pulisci gli Artifact di Build Precedenti

Il Makefile offre due opzioni di pulizia:

**Opzione A: Pulizia Standard** (consigliata per la maggior parte dei casi)
```bash
make clean
```
Questo rimuove:
- Directory di build (`build/`)
- Directory di distribuzione (`dist/`)
- Directory di build per i test (`build_test/`)
- File di configurazione temporanei
- File oggetto compilati (`.o`)
- Flag di cache setup

**Opzione B: Pulizia Profonda** (per una pulizia completa)
```bash
make deepclean
```
Questo fa tutto quello che fa `clean`, PIÙ:
- Rimuove tutti i file di cache
- Pulisce le dipendenze di terze parti (RetroArch, SearchFilter, Terminal, DinguxCommander)

⚠️ **Nota**: `deepclean` richiede più tempo per ricompilare perché pulisce anche le dipendenze di terze parti.

#### 3. Ricompila il Progetto

Dopo la pulizia, ricompila con:
```bash
make all
```

Oppure per creare un pacchetto di release:
```bash
make release
```

### Esempio di Workflow Completo

```bash
# 1. Aggiorna il branch
git pull origin copilot/optimize-code-performance

# 2. Pulisci la build precedente
make clean

# 3. Ricompila
make all
```

### Scenari Comuni

#### Scenario 1: Aggiornamento Rapido (Nessuna Modifica Importante)
```bash
git pull
make clean
make all
```

#### Scenario 2: Modifiche Importanti o Problemi di Build
```bash
git pull
make deepclean
make all
```

#### Scenario 3: Solo Aggiornamento Senza Ricompilare
```bash
git pull
# Rivedi le modifiche, poi decidi se ricompilare
```

---

## Docker Toolchain Builds / Build con Docker Toolchain

### English Version

The Onion project uses a Docker-based cross-compilation toolchain for building firmware for the Miyoo Mini device.

#### First-Time Setup

1. **Initialize Git Submodules** (required for third-party dependencies):
   ```bash
   make git-submodules
   ```
   This downloads RetroArch, SearchFilter, Terminal, and DinguxCommander.

2. **Pull Docker Toolchain**:
   ```bash
   make toolchain
   ```
   Or it will be automatically pulled when you first use `with-toolchain`.

#### Building with Docker Toolchain

The `with-toolchain` target runs build commands inside the Docker container:

```bash
# Basic syntax
make with-toolchain CMD="<command>"

# Build with parallel jobs
make with-toolchain CMD="all -j4"

# Build using all CPU cores
make with-toolchain CMD="all -j$(nproc)"
```

#### Complete Build Workflow with Docker

**First Time Setup:**
```bash
# 1. Initialize submodules
make git-submodules

# 2. Clean (optional, but recommended)
make clean

# 3. Build with Docker toolchain
make with-toolchain CMD="all -j4"
```

**Subsequent Builds (after git pull):**
```bash
# 1. Update code
git pull

# 2. Update submodules (if needed)
make git-submodules

# 3. Clean previous build
make clean

# 4. Rebuild with Docker
make with-toolchain CMD="all -j$(nproc)"
```

#### Why Use Docker Toolchain?

- ✅ **Cross-compilation**: Builds ARM binaries for Miyoo Mini on x86/x64 hosts
- ✅ **Consistent environment**: Same build environment for all developers
- ✅ **No manual toolchain setup**: Everything is containerized

#### Common Docker Build Commands

```bash
# Standard build with 4 parallel jobs
make with-toolchain CMD="all -j4"

# Build using all available CPU cores
make with-toolchain CMD="all -j$(nproc)"

# Clean and build
make clean && make with-toolchain CMD="all -j4"

# Deep clean and build
make deepclean && make with-toolchain CMD="all -j$(nproc)"

# Build only core components
make with-toolchain CMD="core -j4"

# Build and create release package
make with-toolchain CMD="release -j4"
```

#### Interactive Docker Shell

To explore or debug inside the Docker container:
```bash
make toolchain
# This opens an interactive bash shell inside the container
```

### Versione Italiana

Il progetto Onion utilizza una toolchain Docker per la cross-compilazione del firmware per il dispositivo Miyoo Mini.

#### Configurazione Iniziale

1. **Inizializza i Submodule Git** (richiesto per le dipendenze di terze parti):
   ```bash
   make git-submodules
   ```
   Questo scarica RetroArch, SearchFilter, Terminal e DinguxCommander.

2. **Scarica la Docker Toolchain**:
   ```bash
   make toolchain
   ```
   Oppure verrà scaricata automaticamente al primo uso di `with-toolchain`.

#### Build con Docker Toolchain

Il target `with-toolchain` esegue i comandi di build all'interno del container Docker:

```bash
# Sintassi base
make with-toolchain CMD="<comando>"

# Build con job paralleli
make with-toolchain CMD="all -j4"

# Build usando tutti i core CPU
make with-toolchain CMD="all -j$(nproc)"
```

#### Workflow Completo di Build con Docker

**Prima Configurazione:**
```bash
# 1. Inizializza i submodule
make git-submodules

# 2. Pulisci (opzionale, ma consigliato)
make clean

# 3. Build con Docker toolchain
make with-toolchain CMD="all -j4"
```

**Build Successive (dopo git pull):**
```bash
# 1. Aggiorna il codice
git pull

# 2. Aggiorna i submodule (se necessario)
make git-submodules

# 3. Pulisci la build precedente
make clean

# 4. Ricompila con Docker
make with-toolchain CMD="all -j$(nproc)"
```

#### Perché Usare Docker Toolchain?

- ✅ **Cross-compilazione**: Crea binari ARM per Miyoo Mini su host x86/x64
- ✅ **Ambiente consistente**: Stesso ambiente di build per tutti gli sviluppatori
- ✅ **Nessuna configurazione manuale**: Tutto è containerizzato

#### Comandi Docker Comuni

```bash
# Build standard con 4 job paralleli
make with-toolchain CMD="all -j4"

# Build usando tutti i core disponibili
make with-toolchain CMD="all -j$(nproc)"

# Pulisci e compila
make clean && make with-toolchain CMD="all -j4"

# Pulizia profonda e compila
make deepclean && make with-toolchain CMD="all -j$(nproc)"

# Build solo componenti core
make with-toolchain CMD="core -j4"

# Build e crea pacchetto release
make with-toolchain CMD="release -j4"
```

#### Shell Docker Interattiva

Per esplorare o fare debug all'interno del container Docker:
```bash
make toolchain
# Questo apre una shell bash interattiva all'interno del container
```

---

## Common Mistakes to Avoid / Errori Comuni da Evitare

### ❌ Incorrect Commands / Comandi Errati

**DON'T DO THIS / NON FARE QUESTO:**
```bash
# Wrong: Trying to run multiple targets as if they're one
make -j4 all git-submodules  # ❌ WRONG

# Wrong: with-toolchain without CMD parameter
make -j4 all with-toolchain  # ❌ WRONG

# Wrong: Parallel flag on with-toolchain itself
make -j4 with-toolchain CMD="all"  # ❌ WRONG (parallel flag in wrong place)
```

**DO THIS INSTEAD / FAI INVECE QUESTO:**
```bash
# Correct: Run targets separately in sequence
make git-submodules
make clean
make with-toolchain CMD="all -j4"  # ✅ CORRECT

# Or in one line with && operator
make git-submodules && make clean && make with-toolchain CMD="all -j4"  # ✅ CORRECT
```

### Understanding Target Dependencies / Comprendere le Dipendenze dei Target

- `git-submodules` is a **standalone target** - run it separately
- `with-toolchain` is a **wrapper target** - it needs CMD parameter with the actual build command
- `all`, `core`, `apps`, `external` are **build targets** - these go inside CMD
- `-j4` or `-j$(nproc)` **parallel flags** go inside the CMD parameter, not outside

### ❌ The Native Build Error / L'Errore di Build Nativa

**If you see these errors, you forgot to use Docker toolchain!**

```
SDL/SDL.h: No such file or directory
SDL/SDL.h: File o directory non esistente
cannot find -lSDL
impossibile trovare -lSDL
incompatible .so
.so incompatibile
```

**What happened:**
- You tried to run `make all` or `make -j4 all` directly
- Your system tried to compile natively (x86/x64)
- The ARM libraries are incompatible with your architecture
- SDL headers/libraries are not installed (and shouldn't be)

**Solution:**
```bash
# STOP the current build (Ctrl+C if it's running)

# Use Docker toolchain instead
make git-submodules  # if first time
make clean  # clean the failed build
make with-toolchain CMD="all -j$(nproc)"  # ✅ CORRECT
```

**Remember:** `make all` ❌ vs `make with-toolchain CMD="all"` ✅

---

## Tips / Suggerimenti

### Check What Changed / Controlla Cosa È Cambiato
```bash
# See what files changed
git log --oneline -10

# See detailed changes
git diff HEAD~5
```

### Build Performance / Prestazioni di Build

- Use `make clean` for incremental changes (faster)
- Use `make deepclean` only when necessary (slower but thorough)
- The build system uses cache to speed up rebuilds

### Parallel Builds / Build Paralleli

To speed up compilation, you can use parallel jobs **inside Docker**:
```bash
# ✅ CORRECT - parallel flag inside CMD
make with-toolchain CMD="all -j4"  # Uses 4 parallel jobs
make with-toolchain CMD="all -j$(nproc)"  # Uses all available CPU cores

# ❌ WRONG - don't use parallel flag outside with-toolchain
make -j4 with-toolchain CMD="all"  # This doesn't work as expected
```

---

## Troubleshooting / Risoluzione Problemi

### 🔴 "SDL/SDL.h: No such file" or ".so incompatibile" Errors

**English:**
This is the most common error! It means you tried to build natively instead of using Docker.

**Symptoms:**
```
SDL/SDL.h: No such file or directory
cannot find -lSDL
incompatible .so files
saltato ../../lib/libsqlite3.so incompatibile
```

**Solution:**
1. Stop the build (Ctrl+C)
2. Clean the failed build: `make clean`
3. Use Docker toolchain: `make with-toolchain CMD="all -j$(nproc)"`

**Italiano:**
Questo è l'errore più comune! Significa che hai provato a compilare nativamente invece di usare Docker.

**Sintomi:**
```
SDL/SDL.h: File o directory non esistente
impossibile trovare -lSDL
file .so incompatibile
saltato ../../lib/libsqlite3.so incompatibile
```

**Soluzione:**
1. Ferma la build (Ctrl+C)
2. Pulisci la build fallita: `make clean`
3. Usa la toolchain Docker: `make with-toolchain CMD="all -j$(nproc)"`

### Build Fails After Update / Build Fallisce Dopo Aggiornamento

1. Try deep clean with Docker:
   ```bash
   make deepclean
   make with-toolchain CMD="all -j$(nproc)"
   ```

2. Check if you have local uncommitted changes:
   ```bash
   git status
   git stash  # Temporarily save your changes
   git pull
   make git-submodules  # Update submodules
   make clean
   make with-toolchain CMD="all -j$(nproc)"
   git stash pop  # Restore your changes
   ```

3. Verify you're on the correct branch:
   ```bash
   git branch -vv
   ```

### Docker Not Found / Docker Non Trovato

If you get "docker: command not found":
```bash
# Install Docker first
# On Ubuntu/Debian:
sudo apt-get update
sudo apt-get install docker.io
sudo systemctl start docker
sudo usermod -aG docker $USER
# Log out and back in for group changes to take effect

# Then try again:
make with-toolchain CMD="all -j$(nproc)"
```

### Merge Conflicts / Conflitti di Merge

If you get merge conflicts during `git pull`:
```bash
# See which files have conflicts
git status

# Edit the conflicting files to resolve
# Then:
git add <resolved-files>
git commit
```

---

## Quick Reference / Riferimento Rapido

| Command | Purpose | When to Use |
|---------|---------|-------------|
| `git pull` | Update branch | Always before rebuilding |
| `make git-submodules` | Initialize/update submodules | First time or when submodules change |
| `make clean` | Clean build artifacts | After most updates |
| `make deepclean` | Deep clean everything | Major changes or issues |
| `make all` | Build project (native) | Standard build without Docker |
| `make with-toolchain CMD="all -j4"` | Build with Docker (4 cores) | Cross-compile for Miyoo Mini |
| `make with-toolchain CMD="all -j$(nproc)"` | Build with Docker (all cores) | Fastest Docker build |
| `make release` | Create release package | For distribution |
| `make toolchain` | Interactive Docker shell | Debug or explore toolchain |
| `git status` | Check repository state | Before pulling |
| `git log -5` | See recent commits | To check what changed |

---

## Additional Resources / Risorse Aggiuntive

- Main README: [README.md](README.md)
- Makefile targets: Run `make` to see all available targets
- Git documentation: https://git-scm.com/doc

For more help, check the repository issues or contact the maintainers.
