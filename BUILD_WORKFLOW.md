# Build Workflow Guide / Guida al Workflow di Build

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

To speed up compilation, you can use parallel jobs:
```bash
make -j4 all  # Uses 4 parallel jobs
make -j$(nproc) all  # Uses all available CPU cores
```

---

## Troubleshooting / Risoluzione Problemi

### Build Fails After Update / Build Fallisce Dopo Aggiornamento

1. Try deep clean:
   ```bash
   make deepclean
   make all
   ```

2. Check if you have local uncommitted changes:
   ```bash
   git status
   git stash  # Temporarily save your changes
   git pull
   make clean
   make all
   git stash pop  # Restore your changes
   ```

3. Verify you're on the correct branch:
   ```bash
   git branch -vv
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
| `make clean` | Clean build artifacts | After most updates |
| `make deepclean` | Deep clean everything | Major changes or issues |
| `make all` | Build project | Standard build |
| `make release` | Create release package | For distribution |
| `git status` | Check repository state | Before pulling |
| `git log -5` | See recent commits | To check what changed |

---

## Additional Resources / Risorse Aggiuntive

- Main README: [README.md](README.md)
- Makefile targets: Run `make` to see all available targets
- Git documentation: https://git-scm.com/doc

For more help, check the repository issues or contact the maintainers.
