# OnionPlus vs base release

Documento di confronto statistico tra il tip **OnionPlus** e la base **OnionUI/Onion main** (pre-port).

| Riferimento | Valore |
|---|---|
| Branch tip | `amiga/OnionPlus` → `300390a7` |
| Base / merge-base | `07505ea5` (`OnionUI/Onion` main = Amiga500/Onion main pre-OnionPlus) |
| Commit avanti | **5** (tutti del 2026-08-20, autore Amiga500) |
| Delta aggregato | **97 file**, **+24 094** / **−284** |
| Unit test (tip) | **66 suite · 1 373 test** (`RUN_TEST` in `test/Makefile.unit`) |

Fonte: `git fetch amiga OnionPlus` + `git diff` / `git show` sul workspace `/home/Andy/Onion+`.

---

## Sintesi

OnionPlus porta tre filoni da OniOpus46 sopra Onion main:

1. **NEON / performance** — conversioni pixel e helper grafici.
2. **Hardening crash/memoria** — `file` / `str` / `signal_handler` / JSON e componenti common.
3. **Suite unit-test host** — framework leggero + 66 suite (dopo una fase intermedia a 17).

Il volume è dominato dai test (~95% degli inserimenti); il codice di produzione in `src/` è più contenuto ma ad alto impatto.

---

## I 5 commit

| # | Hash | Messaggio | File | + / − | Categoria |
|---|------|-----------|------|-------|-----------|
| 1 | `d7aed5a1` | OnionPlus: NEON pixel conversions ported from OniOpus46. | 7 | +483 / −80 | NEON / perf |
| 2 | `6da7f28b` | apply clang-format changes | 2 | +2 / −2 | Formattazione |
| 3 | `ad402fa2` | OnionPlus: port common crash/memory hardening from OniOpus46. | 33 | +9 724 / −193 | Hardening + test |
| 4 | `1a1e3f84` | Limit unit-test suite list to ported hardening tests. | 1 | +2 / −2 | Unit test (intermedio) |
| 5 | `300390a7` | OnionPlus: expand host unit-test suite from OniOpus46. | 58 | +13 894 / −18 | Unit test + Makefile |

Ordine: dal più vecchio al tip (`git log --reverse 07505ea5..amiga/OnionPlus`).

### Note per commit

- **`d7aed5a1` (NEON)** — nuovo `neon_pixel.h` (+301) e aggiornamenti a `rotate180.h`, `surfaceSetAlpha.h`, `IMG_Save.h`, `screenshot.h`, `jpg2png.c`, `pngScale.c`.
- **`6da7f28b` (format)** — solo ritocchi clang-format su due header già toccati dal port NEON.
- **`ad402fa2` (hardening)** — 14 file `src/common` (+540 / −193) e 19 file di test iniziali (+9 184). Introduce `signal_handler.h`, rafforza `file.c` / `str.c` / `json.h`, aggiunge `test/onion_test.h` e `test/Makefile.unit`.
- **`1a1e3f84` (lista limitata)** — riduce `TESTS` da una lista aspirazionale (~67 nomi, molti `.c` ancora assenti) a **17 suite** già presenti (**583** `RUN_TEST`).
- **`300390a7` (espansione)** — porta il resto delle suite host: tip a **66 / 1 373**, più `perf.h`, wiring root `Makefile` / `test/Makefile*`.

---

## Ripartizione aggregata

### Per directory

| Area | File | Inserimenti | Eliminazioni |
|------|------|-------------|--------------|
| `src/` | 23 | +1 116 | −273 |
| `test/` (include `test/Makefile*`) | 73 | +22 974 | −10 |
| `Makefile` (root) | 1 | +4 | −1 |
| **Totale (shortstat)** | **97** | **+24 094** | **−284** |

Wiring Makefile sotto `test/` (es. `Makefile.unit` +606): già incluso nella riga `test/`.  
Di cui: **74** file aggiunti (`A`), **23** modificati (`M`). Nessun file `docs/` nel delta.

### Per categoria funzionale (classificazione file)

| Categoria | File | Inserimenti | Eliminazioni |
|-----------|------|-------------|--------------|
| Unit test | 70 | +22 310 | −2 |
| Makefile / wiring | 4 | +668 | −9 |
| NEON / perf | 9 | +576 | −80 |
| Hardening (`src/common`) | 14 | +540 | −193 |

---

## File chiave

| File | Stato | Delta | Ruolo |
|------|-------|-------|-------|
| `src/common/utils/neon_pixel.h` | A | +301 | Conversioni pixel NEON |
| `src/common/utils/file.c` | M | +205 / −69 | Hardening path/IO |
| `src/common/utils/file.h` | M | +8 | API file |
| `src/common/utils/str.c` | M | +41 / −20 | String safety |
| `src/common/utils/str.h` | M | +1 / −4 | API stringhe |
| `src/common/utils/signal_handler.h` | A | +47 | Gestione segnali / crash |
| `src/common/utils/json.h` | M | +13 / −4 | Guardie null/parse |
| `src/common/utils/perf.h` | A | +92 | Helper perf host |
| `src/common/utils/msleep.h` | M | +2 / −1 | Sleep testabile |
| `test/onion_test.h` | A | +162 | Framework `TEST` / `RUN_TEST` |
| `test/Makefile.unit` | A | +606 | Elenco e build 66 suite |
| `Makefile` | M | +4 / −1 | Target `unit-test` |

Altri hardening rilevanti in `src/common`: `list.h`, `log.c`, `process.h`, `flags.h`, `state.h`, `settings.h`, `clock.h`, `axp.h`.

---

## Unit test — verifica conteggio

Conteggio statico sul tip `300390a7` (nessuna riesecuzione completa di `make unit-test`):

| Checkpoint | Suite in `TESTS` | `RUN_TEST` nelle suite elencate |
|------------|------------------|----------------------------------|
| Dopo `ad402fa2` | 67 (lista aspirazionale) | ~583 eseguibili (molti target senza `.c`) |
| Dopo `1a1e3f84` | **17** | **583** |
| Tip `300390a7` | **66** | **1 373** |

Le 17 suite intermedie:

```
test_str test_str_security test_file test_file_security
test_json test_json_security test_json_null_guards test_list
test_signal_handler test_state test_state_security test_flags
test_process test_clock test_critical_fixes test_null_safety
test_system_utils
```

---

## Comandi di riproduzione

```bash
git fetch amiga OnionPlus
git log --oneline 07505ea5..amiga/OnionPlus
git diff --shortstat 07505ea5..amiga/OnionPlus
git diff --numstat 07505ea5..amiga/OnionPlus
git merge-base amiga/OnionPlus origin/main   # → 07505ea5
```

Vista interattiva (canvas Cursor):  
`~/.cursor/projects/home-Andy-Onion/canvases/onionplus-vs-base.canvas.tsx`
