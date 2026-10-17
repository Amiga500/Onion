# Prebuilt binaries

Files here replace the binaries of the same name that `make core` builds from
`src/`, after the build (see the `core` target in the top-level Makefile).

| File | Source | SHA-256 |
| --- | --- | --- |
| `gameSwitcher` | robcodedev's build of the beta 8 GameSwitcher (ARM, 102,580 bytes) | `a9927cab30219eddffc79a3394e02ba050dbc06072aad2a65404e287f1c82412` |

`gameSwitcher` contains every string of the beta 8 GameSwitcher built from
`src/gameSwitcher`, plus robcodedev's changes. Delete the file to go back to
the one built from source.
