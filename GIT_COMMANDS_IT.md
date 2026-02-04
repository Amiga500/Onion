# Guida Comandi Git per Onion

## Comandi Git Essenziali

### Aggiornare il Repository Locale

Per ottenere le ultime modifiche dal repository remoto:

```bash
# Se sei sul branch principale (main)
git pull origin main

# Se sei su un branch specifico
git pull origin nome-del-branch

# Oppure semplicemente (scarica dal branch tracciato)
git pull
```

### Verificare lo Stato del Repository

```bash
# Vedere su quale branch sei
git branch

# Vedere tutti i branch (inclusi quelli remoti)
git branch -a

# Controllare lo stato del repository
git status

# Vedere gli ultimi commit
git log --oneline -10
```

### Scaricare Modifiche Senza Applicarle

```bash
# Scarica le modifiche dal remoto senza fare merge
git fetch origin

# Vedi cosa è cambiato
git log HEAD..origin/main

# Poi puoi decidere se fare merge
git merge origin/main
# oppure
git pull
```

### Aggiornare i Submodules

```bash
# Dopo aver fatto pull, aggiorna i submodules
git submodule update --init --recursive

# Oppure usa il comando Makefile
make git-submodules
```

### Cambiare Branch

```bash
# Passare al branch main
git checkout main

# Creare e passare a un nuovo branch
git checkout -b mio-nuovo-branch

# Passare a un branch esistente
git checkout nome-branch
```

### Salvare le Tue Modifiche

```bash
# Aggiungere file specifici
git add percorso/del/file

# Aggiungere tutte le modifiche
git add .

# Fare commit con messaggio
git commit -m "Descrizione delle modifiche"

# Inviare al repository remoto
git push origin nome-branch
```

### Salvare Temporaneamente le Modifiche

```bash
# Salvare le modifiche temporaneamente (stash)
git stash

# Fare pull delle ultime modifiche
git pull

# Riapplicare le modifiche salvate
git stash pop
```

## Scenari Comuni

### Scenario 1: Aggiornare il Codice Prima di Lavorare

```bash
# 1. Verifica su quale branch sei
git branch

# 2. Scarica le ultime modifiche
git pull

# 3. Aggiorna i submodules
make git-submodules

# 4. Ora puoi iniziare a lavorare
```

### Scenario 2: Hai Modifiche Locali e Vuoi Fare Pull

```bash
# Opzione A: Salvare temporaneamente le modifiche
git stash
git pull
git stash pop

# Opzione B: Fare commit delle modifiche prima
git add .
git commit -m "WIP: lavoro in corso"
git pull
```

### Scenario 3: Vedere Cosa è Cambiato

```bash
# Vedere le modifiche non ancora salvate
git diff

# Vedere le modifiche già aggiunte (staged)
git diff --staged

# Vedere tutte le modifiche rispetto all'ultimo commit
git diff HEAD

# Vedere la storia dei commit
git log --oneline --graph --all
```

### Scenario 4: Risolvere Conflitti dopo Pull

```bash
# Se dopo git pull hai conflitti
# 1. Git ti dirà quali file hanno conflitti
git status

# 2. Apri i file in conflitto e risolvi manualmente
# (cerca i marker <<<<<<, ======, >>>>>>)

# 3. Dopo aver risolto, aggiungi i file
git add file-risolto.c

# 4. Completa il merge
git commit -m "Risolti conflitti di merge"
```

## Comandi per il Tuo Branch Corrente

Attualmente sei su: **copilot/optimize-code-performance-again**

```bash
# Per aggiornare questo branch:
git pull origin copilot/optimize-code-performance-again

# Oppure semplicemente:
git pull

# Per vedere lo stato:
git status

# Per vedere le differenze con il remoto:
git fetch origin
git log HEAD..origin/copilot/optimize-code-performance-again
```

## Riferimento Rapido

| Comando | Descrizione |
|---------|-------------|
| `git pull` | Aggiorna branch corrente dal remoto |
| `git fetch` | Scarica modifiche senza fare merge |
| `git status` | Controlla stato repository |
| `git branch` | Lista i branch |
| `git checkout <branch>` | Cambia branch |
| `git log` | Visualizza cronologia commit |
| `git diff` | Visualizza modifiche |
| `git stash` | Salva temporaneamente modifiche |
| `git add .` | Aggiunge tutte le modifiche |
| `git commit -m "msg"` | Salva modifiche con messaggio |
| `git push` | Invia modifiche al remoto |

## Link Utili

- [BUILD.md](BUILD.md) - Guida completa per compilare Onion
- [Documentazione Git (Italiano)](https://git-scm.com/book/it/v2)
- [Repository GitHub](https://github.com/Amiga500/Onion)
