# Prebuilt binaries

Files here are copied into `.tmp_update/bin` by `make core`, after the build
(see the `core` target in the top-level Makefile), replacing any binary of the
same name built from `src/`.

| File | Source | SHA-256 |
| --- | --- | --- |
| `gameSwitcher` | robcodedev's build of the beta 8 GameSwitcher (ARM, 102,580 bytes) | `a9927cab30219eddffc79a3394e02ba050dbc06072aad2a65404e287f1c82412` |
| `warmup` | robcodedev's boot warmup, his file `warmup3` (ARM, 5,572 bytes) | `c2a8984c3eebfd4e61e57e2ccf63a46b6773eb77d987de0fd2f13b571d700f90` |

`gameSwitcher` contains every string of the beta 8 GameSwitcher built from
`src/gameSwitcher`, plus robcodedev's changes. Delete the file to go back to
the one built from source.

`warmup` has no source in `src/`. `runtime.sh` (`start_warmup`) runs it in the
background during boot: it calls `SDL_Init(SDL_INIT_VIDEO)` once, so the
one-time MI video setup (about 2 s after a cold boot) is paid before MainUI
starts. It needs `libSDL-1.2.so.0` from `miyoo/lib`. Delete the file and boot
is as before.
