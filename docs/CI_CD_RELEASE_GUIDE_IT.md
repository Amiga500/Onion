# Guida al Sistema di Release CI/CD

## Problema Risolto

Il problema era che i workflow di GitHub Actions non si attivavano automaticamente quando venivano creati i tag `stable` e `beta`, quindi i file .zip delle release non venivano costruiti e caricati automaticamente.

## Soluzione Implementata

I workflow sono stati aggiornati per supportare i tag personalizzati:

### 1. **Tagged Release** (`.github/workflows/tagged-release.yml`)
- **Trigger**: Si attiva quando viene creato un tag `v*` o `stable`
- **Output**: Crea un file `Onion-v{VERSION}.zip` come draft release
- **Comportamento**:
  - Per tag `v*`: usa la versione dal tag (es. `v4.3.1` → versione `4.3.1`)
  - Per tag `stable`: usa la versione dal Makefile

### 2. **Pre-release** (`.github/workflows/pre-release.yml`)
- **Trigger**: Manualmente tramite workflow_dispatch (interfaccia GitHub Actions)
- **Output**: Crea un file `Onion-v{VERSION}-{SHA}.zip` come pre-release
- **Comportamento**:
  - Usa la versione dal Makefile + SHA del commit corrente (da `HEAD`)

## Come Usare

### Per Creare una Release Stabile

1. **Aggiorna la versione** nel `Makefile` se necessario
2. **Crea e pusha il tag `stable`**:
   ```bash
   git tag -f stable
   git push -f origin stable
   ```
3. Il workflow `tagged-release` si attiverà automaticamente e:
   - Costruirà il progetto
   - Creerà il file `Onion-v{VERSION}.zip`
   - Lo caricherà nella release con tag `stable` (come draft)

### Per Creare una Release Beta

1. **Aggiorna la versione** nel `Makefile` se necessario (formato: `X.Y.Z-beta-DD_MM_YYYY`)
2. **Vai su GitHub → Actions** e seleziona il workflow "Pre-release"
3. Clicca **"Run workflow"**, seleziona il branch e clicca **"Run workflow"**
4. Il workflow `pre-release` si attiverà automaticamente e:
   - Costruirà il progetto
   - Creerà il file `Onion-v{VERSION}-{SHA}.zip`
   - Lo caricherà nella release con tag `latest` (come pre-release)

### Per Release con Versione Specifica

Puoi ancora usare il formato tradizionale con tag `v*`:
```bash
git tag v4.4.0
git push origin v4.4.0
```

Questo creerà una release draft con il file `Onion-v4.4.0.zip`.

## I Tre File .zip nella Release

Quando il workflow funziona correttamente, vedrai **tre file .zip** nella pagina della release:

1. **`Onion-v{VERSION}.zip` o `Onion-v{VERSION}-{SHA}.zip`**
   - Il file principale costruito dal workflow
   - Contiene l'intera distribuzione di Onion

2. **`Source code (zip)`**
   - Generato automaticamente da GitHub
   - Contiene il codice sorgente al momento del tag

3. **`Source code (tar.gz)`**
   - Generato automaticamente da GitHub
   - Stessa cosa del punto 2, ma in formato tar.gz

## Note Importanti

- **Non caricare manualmente** i file .zip nelle release - lascia che i workflow lo facciano automaticamente
- Se hai già caricato file manualmente, puoi **eliminarli** e ricreare il tag per far ripartire il workflow
- I workflow richiedono che i **submodule siano inizializzati** correttamente
- Per **forzare un rebuild**, puoi eliminare e ricreare il tag:
  ```bash
  git tag -d stable          # elimina localmente
  git push origin :stable    # elimina su GitHub
  git tag stable             # crea di nuovo
  git push origin stable     # pusha di nuovo
  ```

## Verifica dei Workflow

Dopo aver pushato un tag, puoi verificare lo stato del workflow:

1. Vai su GitHub → Actions
2. Cerca il workflow "Tagged release" o "Pre-release"
3. Controlla che sia in esecuzione e completi con successo
4. Verifica che il file .zip sia stato caricato nella release

## Risoluzione Problemi

### Il workflow non si attiva
- Verifica che il tag sia stato pushato correttamente: `git ls-remote --tags origin`
- Controlla che il file workflow non abbia errori di sintassi

### Il workflow fallisce durante la build
- Controlla i log del workflow su GitHub Actions
- Verifica che i submodule siano inizializzati
- Assicurati che la versione nel Makefile sia valida

### Il file .zip non viene caricato
- Verifica che la build sia completata con successo
- Controlla che il file esista in `release/` dopo la build
- Verifica i permessi del GITHUB_TOKEN
