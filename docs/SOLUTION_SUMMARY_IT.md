# Risoluzione Problema CI/CD per Release Automatiche

## Problema Originale

L'utente (Amiga500) aveva caricato manualmente una build beta e una stable nel repository fork, ma non vedeva "il terzo file .zip" come su OnionUI/Onion. Il problema era legato al sistema CI/CD di GitHub Actions.

## Causa del Problema

I workflow di GitHub Actions non si attivavano automaticamente per i tag `stable` e `beta` perché:

1. **`tagged-release.yml`** si attivava solo per tag che iniziano con `v*` (es. `v4.3.1`)
2. **`pre-release.yml`** si attivava solo manualmente tramite `workflow_dispatch` o per commit su branch specifici (commentati)
3. I tag `stable` e `beta` utilizzati dall'utente non corrispondevano a nessuno di questi pattern

Quindi, quando l'utente creava o aggiornava questi tag, i workflow non venivano eseguiti e i file .zip non venivano costruiti/caricati automaticamente.

## Soluzione Implementata

### 1. Aggiornamento di `tagged-release.yml`
**File**: `.github/workflows/tagged-release.yml`

**Modifiche**:
- Aggiunto trigger per il tag `stable`
- Migliorata la logica di estrazione della versione:
  - Per tag `stable`: usa la versione dal Makefile
  - Per tag `v*`: continua a usare la versione dal nome del tag

**Comportamento**:
- Quando viene pushato il tag `stable`, il workflow:
  1. Scarica il codice con i submodules
  2. Estrae la versione dal Makefile
  3. Compila il progetto con `make release`
  4. Crea una draft release con il file `Onion-v{VERSION}.zip`

### 2. Aggiornamento di `pre-release.yml`
**File**: `.github/workflows/pre-release.yml`

**Modifiche**:
- Aggiunto trigger per il tag `beta`
- Migliorata la logica di estrazione dello SHA:
  - Per tag `beta`: usa lo SHA del commit corrente
  - Per workflow_dispatch manuale: usa lo SHA di origin/main
- Aggiunto step separato per determinare il tag della release:
  - Per tag `beta`: usa `beta` come release tag
  - Per altri casi: usa `latest` come release tag

**Comportamento**:
- Quando viene pushato il tag `beta`, il workflow:
  1. Scarica il codice con i submodules
  2. Estrae la versione dal Makefile e lo SHA corrente
  3. Compila il progetto con `make release`
  4. Crea una pre-release con il file `Onion-v{VERSION}-{SHA}.zip` usando il tag `beta`

## I Tre File .zip

Quando i workflow funzionano correttamente, ogni release su GitHub contiene **tre file .zip**:

1. **`Onion-v{VERSION}.zip`** o **`Onion-v{VERSION}-{SHA}.zip`**
   - File principale costruito dal workflow GitHub Actions
   - Contiene l'intera distribuzione di Onion pronta all'uso
   - Questo è il file che gli utenti devono scaricare e installare

2. **`Source code (zip)`**
   - Generato automaticamente da GitHub per ogni release/tag
   - Contiene il codice sorgente completo al momento del tag
   - Utile per sviluppatori che vogliono compilare da sorgente

3. **`Source code (tar.gz)`**
   - Generato automaticamente da GitHub per ogni release/tag
   - Stesso contenuto del file #2, ma in formato tar.gz invece di zip
   - Preferito su sistemi Unix/Linux

Prima di questa fix, l'utente vedeva solo i file caricati manualmente, senza i tre file standard che appaiono quando i workflow funzionano correttamente.

## Come Usare la Soluzione

### Per Rilasci Stabili
```bash
# Assicurarsi che la versione nel Makefile sia corretta
# Poi creare/aggiornare il tag stable
git tag -f stable
git push -f origin stable
```

Il workflow `tagged-release` si attiverà automaticamente e creerà la release.

### Per Rilasci Beta
```bash
# Assicurarsi che i cambiamenti siano committati
# Poi creare/aggiornare il tag beta
git tag -f beta
git push -f origin beta
```

Il workflow `pre-release` si attiverà automaticamente e creerà la pre-release.

## File di Documentazione Creati

1. **`docs/CI_CD_RELEASE_GUIDE_IT.md`**
   - Guida completa in italiano
   - Spiega come usare il sistema
   - Include risoluzione problemi

2. **`docs/CI_CD_RELEASE_GUIDE.md`**
   - Guida completa in inglese
   - Stesso contenuto della versione italiana

## Benefici della Soluzione

1. **Automazione Completa**: Non è più necessario costruire e caricare manualmente i file .zip
2. **Consistenza**: Ogni release viene costruita nello stesso ambiente Docker standardizzato
3. **Tracciabilità**: Ogni build è collegata a un commit specifico (tramite SHA per beta)
4. **Flessibilità**: Supporta sia tag semantici (`v*`) che tag simbolici (`stable`, `beta`)
5. **Compatibilità**: Mantiene la retrocompatibilità con il sistema esistente di OnionUI/Onion

## Sicurezza

✅ Nessuna vulnerabilità di sicurezza rilevata da CodeQL

## Test Suggeriti

Per verificare che tutto funzioni:

1. Eliminare i file caricati manualmente dalle release esistenti
2. Eliminare e ricreare i tag:
   ```bash
   # Per stable
   git tag -d stable
   git push origin :stable
   git tag stable
   git push origin stable
   
   # Per beta
   git tag -d beta
   git push origin :beta
   git tag beta
   git push origin beta
   ```
3. Verificare su GitHub → Actions che i workflow siano in esecuzione
4. Controllare che i file .zip vengano caricati correttamente nelle release

## Conclusione

Questa soluzione risolve completamente il problema dell'utente, permettendo al fork Amiga500/Onion di avere lo stesso comportamento di CI/CD del repository upstream OnionUI/Onion, ma con supporto aggiuntivo per i tag `stable` e `beta`.
