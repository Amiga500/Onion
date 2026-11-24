# MiniCinema source

Source of **MiniCinema 1.1.41** by Rob (@robcodedev) and Veuks, as given
with its release (its `src/` folder). GPL-3.0, see
`static/packages/App/MiniCinema/App/MiniCinema/LICENSE`.

The package in `static/packages/App/MiniCinema` ships the binaries built from
it (`minicinema`, `minicinema-player`, `libs/libminivsync.so`); RedOnion's
build does not compile them. Building needs FFmpeg 4.4.8, the Miyoo SDL2 and
Zig 0.14.1: see `Makefile`, `build_engine.sh` and the app's `README.md`.

Kept outside `src/` so that the clang-format workflow leaves it as written.
