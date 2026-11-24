# Private FFmpeg 4.4.8 AAC/HLS audio backend

## Source, VM paths, toolchain

This build intentionally reuses MiniCinema's **existing FFmpeg source path**:

    FFMPEG_SRC=/root/workspace/ffmpeg-4.4.8
    ZIG_BIN=/root/workspace/zig-0.14.1/zig

Both paths can be overridden with environment variables. RedAmp **never runs
configure inside the MiniCinema FFmpeg source tree**: build outputs and generated
headers go under `src/build/ffmpeg-redamp-arm/` (overridable as
`REDAMP_FFMPEG_BUILD`). This prevents corrupting the separate static MiniCinema
build. FFmpeg 4.4.8 is pinned; other source versions are refused.

Upstream tarball: https://ffmpeg.org/releases/ffmpeg-4.4.8.tar.xz
SHA256:
`c73848c4ae283d9eaee7be3b276affbc3543380483555500d0dd2c9b7e1c39c3`

The already-extracted MiniCinema tree is used by default without downloading
or unpacking anything. If `FFMPEG_TARBALL=/path/to/ffmpeg-4.4.8.tar.xz` is set,
the script verifies that archive's SHA256 before compiling. A modified working
source tree is not authenticated just by the `VERSION` check; audit any patches.

## ARM OpenSSL 3 development dependency

FFmpeg 4.4.8 cannot build an HTTPS backend from FFmpeg source alone. You also
need **ARM hard-float OpenSSL 3** headers and libraries built for the same
Linux/glibc ABI as Zig's `arm-linux-gnueabihf.2.28` target. The x86 host
OpenSSL headers/libraries must not be used as a substitute.

Provide the location using `OPENSSL_ARM_PREFIX`; it must contain:

    include/openssl/ssl.h
    lib/libssl.so        (cross-linker symlink)
    lib/libcrypto.so     (cross-linker symlink)
    lib/libssl.so.3      (ARM runtime)
    lib/libcrypto.so.3   (ARM runtime)

The script also accepts `lib64/` instead of `lib/` for these libraries.
You must source or build the ARM OpenSSL installation separately. RedAmp's
`build_ffmpeg.sh` does **not** compile OpenSSL or assume RedOnion has the
correct development files. Verify `readelf -h` reports ARM for both runtime
libraries and that their other DT_NEEDED dependencies are available on Miyoo.

## Build commands on the Linux VM

Run in the extracted RedAmp project:

```sh
cd src
export OPENSSL_ARM_PREFIX=/absolute/path/to/arm-openssl3
# Optional, if different from MiniCinema's defaults:
# export FFMPEG_SRC=/root/workspace/ffmpeg-4.4.8
# export ZIG_BIN=/root/workspace/zig-0.14.1/zig
# export REDAMP_CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt
make ffmpeg-audio
```

This builds a **shared** audio-only FFmpeg 4.4.8 and a
`libredamp_ffaudio.so` bridge, then stages the result under **the current
source directory**, without requiring `App/` to exist on the VM.
Use `make` / `make all` to build the player, game-audio wrapper and FFmpeg
sequentially, or `make ffmpeg-audio` independently.

The intended staged files are:

    ./lib/libavutil_redamp.so.56
    ./lib/libavcodec_redamp.so.58
    ./lib/libavformat_redamp.so.58
    ./lib/libswresample_redamp.so.3
    ./lib/libredamp_ffaudio.so
    ./lib/libssl.so.3
    ./lib/libcrypto.so.3
    ./cacert.pem

The script checks version, required features, license settings, ARM architecture
and private DT_NEEDED names, then stages the libraries and CA bundle. Copy the
resulting `lib/*.so*` files and `cacert.pem` into the existing `App/RedAmp`
installation when packaging. RedAmp's launcher and assets are not built here. It does
not silently use host FFmpeg libraries. Before packaging, audit **all**
remaining DT_NEEDED entries using `readelf -d` and stage any missing compatible
ARM target libraries. You can run `tools/verify_ffmpeg.sh`
again at any time.

The build is `--disable-everything` followed by explicitly enabling AAC, ALAC,
MP3-in-HLS, HLS, AAC, MOV, MPEG-TS, Ogg, HTTP/HTTPS/TLS/TCP/crypto and the
resampler. Regular MP3/Ogg/Opus files and radio continue through RedAmp's
existing codecs and libcurl. Source code is in `ffbridge.c` and the runtime
loader is in `ffaudio.c`. The bridge uses **the FFmpeg 4.4.8 headers compiled
from the same source and private library set**. It exports a small opaque ABI;
RedAmp itself does not expose or include FFmpeg struct layouts.

## Behavior

- `.aac`, `.m4a`, `.alac` local files: AAC-LC, HE-AAC/HE-AAC v2 and ALAC
  through FFmpeg; can seek and read local metadata. It tries the FFmpeg backend
  only for those formats.
- Local file paths may contain spaces; raw ADTS `.aac` uses the AAC demuxer directly,
  with a larger bounded probe. This fixes the earlier rejection of paths such as
  `AAC - 5MINS - LARGE.aac`. Actual decoding of the user's specific file on Miyoo still
  requires a device test. A host path-validation regression is included.
- `.m3u8`/`.aac` HTTP(S) station URLs: routed directly to FFmpeg. For generic
  URLs, RedAmp's existing curl probe can recognize HLS playlist body, AAC ADTS
  headers or AAC/HLS Content-Type before switching to FFmpeg.
- All FFmpeg decoded audio is **48 kHz interleaved float stereo**. The
  converter is recreated when sample format, input rate, or channel layout
  changes. EQ, volume and output path are unchanged.
- Opening a missing/incompatible FFmpeg backend fails cleanly. Normal codecs
  continue to work and the player reports a clear playback error.
- HTTP(S) streams use protocol/format allowlists, limited probing, verified
  HTTPS by default, a trusted CA file in verified mode and 10–20 s connection/read deadlines. Station
  switching sets a cancellation flag read by the FFmpeg interrupt callback.
  **Actual TLS certificate propagation through HLS redirects and encrypted
  segments must be validated on-device**.
- A dropped live stream retries with 1/2/4/8-second delays, resetting after
  a sustained healthy period. FFmpeg's own HLS code handles playlist and
  segment handling. ICY and HLS titles are best effort; many stations supply
  no per-song metadata.
- The optional Music in games background service deliberately refuses FFmpeg
  AAC/HLS. Normal local MP3/FLAC/Vorbis/Opus/MP3 radio behavior is unaffected.

## Tests and current limitations

`make test` runs the original player regressions, plus a deterministic mock
bridge verifying loader isolation, missing-module errors, metadata, seek,
48 kHz stereo decode routing and cancellation. Mock tests **do not prove**
FFmpeg 4.4.8 itself compiles or decodes audio. The dev ZIP contains **no ARM
FFmpeg `.so` files and no ARM RedAmp player executable**. Run `make` to build
all three components, or target them independently, then validate on hardware.

Testing checklist on Miyoo Plus / RedOnion: local AAC-LC/HE-AAC v2/ALAC,
44.1/48 kHz audio, local seeking, HLS packed AAC, HLS MPEG-TS AAC, AES-128,
HTTPS with valid/invalid certs, timed ID3 titles, ICY metadata, blocked
servers, rapid switching, Wi-Fi loss and reconnection, CPU/RAM/SD use, and
no audio-library changes while Music in games is disabled.

Live AAC/HLS pause currently stops decoder reads, but does not proactively
close the FFmpeg connection after 30 seconds; resume/reconnection behavior
needs additional device testing. Streaming titles and HLS discontinuities may
vary by station. The read callback is cancellable at FFmpeg interrupt points,
but not every TLS or network operation is necessarily instantaneous.

## Licensing / publication checklist

FFmpeg 4.4.8 is built **without GPL and nonfree components**, with
`--enable-version3` required by its OpenSSL 3 configuration. Consequently,
the private FFmpeg libraries use the **LGPL version 3 or later** and OpenSSL 3
is Apache 2.0. The RedAmp application is MIT. Dynamic linking does not erase
redistribution duties: preserve notices, provide the exact corresponding
FFmpeg 4.4.8 source, any modifications, configuration, and LGPL-compliant
library-replacement provisions when distributing an ARM binary package.
The app includes license texts in `App/RedAmp/licenses/`; the exact build
configuration is in `tools/build_ffmpeg.sh`. Keep OpenSSL and CA trust data
updated, and perform an upstream security audit before a public release.

## HLS TLS security patch (updated in v1.1.10)

RedAmp applies `src/tools/patch_ffmpeg_hls.py` only to its private FFmpeg 4.4.8 source
copy. It supplies a per-HLS-instance trusted CA file and enforces TLS verification by default on
nested media playlists, segments and AES-128 key requests. The v1.1.10 patch also propagates the
explicit Allow insecure HTTPS opt-in to these requests. In that mode no CA file is required.
The updated patch upgrades an already-patched private source tree in place. This is intentionally not
implemented by only adding two names to `save_avio_options()`; FFmpeg's HTTP context does
not consistently expose TLS child settings to that function. The patch fails closed
when the expected FFmpeg 4.4.8 source anchors differ. Do not reuse unpatched v1.0.10
`libavformat_redamp.so.58` libraries when deploying v1.0.11.

Run `make ffmpeg-audio` to rebuild the patched FFmpeg library and its bridge; use
`make ffbridge` for later changes restricted to `ffbridge.c`/`adts_duration.c` once
the private FFmpeg library set already exists. Neither target changes MiniCinema.

The host patch test checks the source transformation and upgrade; a certificate-rejection test
using actual ARM libraries, HTTPS HLS playlists, segments and AES-128 keys is still
required before treating this security fix as fully device-verified.
