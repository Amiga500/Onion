# Building RedAmp

## Project layout

`App/RedAmp/` holds application assets, fonts, skins and licenses. `src/` contains sources,
tests and cross-build scripts. The VM needs only the `src/RedAmp/` tree; no App directory is
required for compilation.

## Build on the VM

```sh
cd /root/workspace/RedOnion/src/RedAmp
make                 # wrapper, arm-deps, player, ffmpeg-audio
make wrapper         # optional game-audio wrapper
make arm-deps        # pinned ARM curl, Opus, zlib and OpenSSL dependencies
make player          # ARM RedAmp executable
make ffmpeg-audio    # private FFmpeg libraries and FFmpeg bridge
```

Build outputs are `redamp`, `libpadsp_wrap.so`, `lib/*.so*` and `cacert.pem` in the source
directory. Copy these to the matching paths in the SD card's `App/RedAmp/`, preserving
`launch.sh`, fonts, skins, station list and license files.

The working MiniCinema toolchain defaults are
`/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc` and
`/root/workspace/zig-0.14.1/zig`. FFmpeg uses `/root/workspace/ffmpeg-4.4.8` by default.
A private copy protects MiniCinema's existing in-tree FFmpeg build files. See
`FFMPEG_AUDIO.md` for build configuration and `ARM_DEPENDENCIES.md` for pinned sources.

## Host tests

```sh
cd src
make host
make test
```

Host tests cover parsers, audio, playlists, library, artwork and game-music simulation.
The FFmpeg loader has a mock test; **that is not a real AAC decoder test**. The path
regression test checks local AAC filenames containing spaces. A genuine ARM AAC playback
test must be run on the handheld with RedAmp's private FFmpeg libraries installed.

For source-only VMs, `make test` skips tests requiring `App/` assets. With a full checkout,
set `REDAMP_APP_DIR` to the absolute `App/RedAmp` path to test skins and launcher integration.

Do not copy host binaries to the Miyoo. Test the player normally before enabling game audio.
