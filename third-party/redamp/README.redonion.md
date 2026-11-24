# RedAmp source

Source of **RedAmp 1.1.11** by Rob (@robcodedev), as given with its first
public test build (its `src/` folder; `README.md` here is the author's). See
`static/packages/App/RedAmp/App/RedAmp/licenses/` for its licences.

The package in `static/packages/App/RedAmp` ships the binaries built from it
(`redamp`, `lib/*.so*`); RedOnion's build does not compile them. Building
needs FFmpeg 4.4.8 and MiniCinema's cross-compiler or Zig setup: see
`docs/BUILDING.md`.

Kept outside `src/` so that the clang-format workflow leaves it as written.

"Music in games" needs RedOnion's launchers to source
`App/RedAmp/redonion-launch-audio.sh` before starting a game or MainUI; this
build of RedOnion does not, so the option has no effect in games yet.
