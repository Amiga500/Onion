# OnionPlus – secondo giro di ottimizzazioni

Base: commit `fee6c4b` di `onionplus-compact`. Sono stati modificati 10 file, presenti in questa cartella con la struttura del repository. Dalla radice del repository si applica con:

```
patch -p1 < onionplus-perf2.patch
```

## Bug trovati e corretti

**Leak di connessioni SQLite a ogni ricerca nella cache della MainUI** (`src/playActivity/cacheDB.h`)
- `cache_db_prepare()` chiudeva il database mentre lo statement era ancora aperto. `sqlite3_close()` falliva con `SQLITE_BUSY`, il puntatore veniva azzerato e la connessione restava aperta.
- Misura sull'host, con una cache da 5.000 giochi: 51 file descriptor e circa 29 MB persi in 51 ricerche, cioè circa 570 KB a ricerca.
- Nel gameSwitcher la ricerca parte per ogni voce dei recenti: con collezioni grandi il consumo arrivava a decine di MB su un dispositivo da 128 MB.
- Correzione: `sqlite3_close_v2()`, che chiude la connessione quando lo statement viene finalizzato. Dopo: 0 descrittori persi. Stessa protezione aggiunta in `play_activity_db_close()`.
- Chiuso anche un piccolo leak di `game_name` quando la cache non esiste. Nel riesame precedente l'avevo dato per corretto dal fork per errore.

**Numerazione delle righe nella lista dei recenti** (`file.c`, `state.h`, `gs_history.h`)
- `file_delete_line()` e `file_read_lineN()` leggevano a blocchi da 1 KB, mentre `readHistory()` usava blocchi da 1,5 KB. Una riga più lunga di 1 KB veniva quindi contata in modo diverso dalle varie funzioni.
- Conseguenza verificata sull'host: il gameSwitcher cancellava una voce valida al posto del doppione, e rimuovere un gioco dopo una riga lunga poteva cancellarne un altro.
- Ora tutte le funzioni contano le righe reali con `getline()`, compreso il conteggio del cambio rapido di gioco in `state.h`.

## Prestazioni

**Doppioni nei recenti** (`readHistory`)
- Prima: una riscrittura completa del file sulla SD per ogni doppione.
- Ora: i doppioni vengono raccolti e rimossi con una sola riscrittura atomica, con la nuova funzione `file_delete_lines()`.
- I numeri di riga salvati in ogni voce tengono già conto delle righe cancellate prima.

**`playActivity stop_all`**, eseguito in modo sincrono prima di ogni sospensione
- Nuovo indice `play_activity_play_time_index`: la ricerca delle sessioni aperte o non valide passa da una scansione completa della tabella a una ricerca su indice.
- La tabella cresce di una riga per ogni avvio e per ogni ripresa dalla sospensione.
- UPDATE e DELETE ora girano in una sola transazione: un ciclo di journal e `fsync` invece di due.

**Tempo di gioco nel gameSwitcher**
- Ora viene calcolato dal thread di precaricamento, insieme a nome e core, quando la visualizzazione del tempo è attiva.
- Il thread dell'interfaccia non apre più il database alla prima visualizzazione di ogni gioco. Se il tempo manca, resta il calcolo di riserva in `renderHeader()`.

**`runtime.sh`, avvio dei giochi**
- `check_is_game`, estrazione del percorso della ROM, script di avvio con `:`, estensione, cartella e nome del file di configurazione: ora usano l'espansione dei parametri della shell invece di `echo | grep/awk/basename/dirname`. Sono circa 15 processi in meno a ogni avvio.
- I comandi su più righe continuano a usare il vecchio parsing con `awk`.
- `start_audioserver`: se l'audioserver è già attivo (il caso normale) non calcola più il volume iniziale, che costava `jsonval`, `awk` e una subshell.
- Il controllo del carattere `$` usava `grep -q "\$"`, cioè la fine riga di una regex: era sempre vero, e `cmd_to_run.sh` veniva riscritto sulla SD a ogni avvio. Ora la riscrittura avviene solo se nel percorso c'è davvero un `$`.
- Il contenuto di `cmd_to_run.sh` viene letto per il log solo se il logging è attivo.
- `change_resolution`: niente più `cut` per larghezza e altezza.

## Non modificato, e perché

**Query `LIKE '%…'` sulla cache della MainUI.** Lo schema e gli indici della cache sono della MainUI, che è un binario chiuso. Una ricerca esatta preliminare potrebbe raddoppiare le scansioni quando non trova nulla. Il costo è già stato ridotto nel giro precedente: ora la query parte solo per le ROM nuove e nel gameSwitcher gira in background.

**Polling del coperchio sul Flip.** Richiede una prova su un dispositivo reale.

## Verifiche

- `make unit-test`: 1.419 test e 71.410 asserzioni, tutti superati.
- `test_host/test_cache_db_leak.c`: 0 descrittori persi (prima 51 su 51 ricerche).
- `test_host/test_stop_all_index.c`, su 60.000 sessioni: il piano di esecuzione usa l'indice; sessioni chiuse e righe non valide gestite come prima.
- `test_host/test_readhistory_lines.c`: tutti i numeri di riga corrispondono alla riga giusta (prima 2 errati) e i doppioni rimossi sono quelli giusti.
- `test_host/test_runtime_parse.sh`: vecchio e nuovo parsing coincidono su 14 casi (`:`, `$`, estensioni maiuscole, file senza estensione, app), sia con dash sia con bash in modalità POSIX.
- `test_host/test_gs_romscreen_tsan.c`, con il tempo di gioco attivo: nessuna data race, nessun leak.
- `sh -n runtime.sh` e controllo sintattico di tutti i moduli: nessun avviso nuovo rispetto a `fee6c4b`.

**Da provare sul dispositivo:**
- avvio e uscita da giochi, app e porte (il parsing di `runtime.sh` è stato testato con dash, non con il busybox del Miyoo);
- sospensione e ripresa (`stop_all`);
- gameSwitcher con molti recenti e rimozione di un gioco;
- cambio rapido di gioco.
