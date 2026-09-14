# Onion Tests

## Host unit tests (OnionRefactor)

No device, SDL, gtest, or Docker toolchain required:

```bash
make unit-test
```

Suites:

- `test_str.c` — `src/common/utils/str.c`
- `test_file.c` — `src/common/utils/file.c` (plus `str.c` / `log.c` as dependencies)
- `test_hash.c` — `src/common/utils/hash.h` (`FNV1A_Pippip_Yurii`, buffers padded +8)
- `test_json.c` — `src/common/utils/json.h` (cJSON helpers + load/save)
- `test_flags.c` — `src/common/utils/flags.h` (config flags are this + a fixed path)
- `test_process.c` — `src/common/utils/process.h` (`searchpid` / `isRunning`)
- `test_state.c` — `state_getAppName` contract (`cd /mnt/SDCARD/App/` + name until `;`)
- `test_str_security.c` — NULL / empty-delim edges for `str_*`
- `test_file.c` also covers `file_read` on a directory (exists but `fopen` fails → `NULL`)

These tests pin **current** behaviour, including quirks (empty `file_read` returns an allocated `""`, not `NULL`). Change production code only after a test fails for the old contract.

## Existing integration test

`make test` still builds the original `test_infoPanel.cpp` target (gtest + SDL, typically inside the Miyoo toolchain).
