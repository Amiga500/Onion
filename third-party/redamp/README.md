# RedAmp source
Version 1.1.10

Version 1.1.10 adds persistent Allow insecure HTTPS (default Off) for native radio, covers and
private FFmpeg live/HLS streams. It suppresses NTP when enabled and propagates the chosen TLS
policy to nested HLS requests. Rebuild with `make player` and `make ffmpeg-audio`; the latter
updates the cached private FFmpeg source in place. Cover requests are paced at 1.5 seconds and
503/429 retries use 5/15/30-second backoff and Retry-After handling. Long server delays end the
batch without recording a negative-cache miss.

Version 1.1.9 keeps the radio spectrum still and shows Loading radio... until decoded audio
reaches output. Downloaded artwork and missing-cover records use album artist and album title
keys. Generic folder artwork is suppressed in mixed-album directories.

Version 1.1.8 starts a silent shared NTP worker at launch when online with an invalid clock.
Changing stations cancels only the wait, not synchronization. Superseded playback requests no
longer publish errors or completion events. Radio logs HTTP type, bytes and decoder results.

Version 1.1.7 skips clock synchronization without a usable network interface and default route.
Paused radio stations are restored without opening a network connection. Automatic cover
fetching stays quiet on network/time failures. NTP uses silent bounded fallbacks, including a
minimal direct SNTP client if system tools fail, and logs each attempt.

Version 1.1.6 shares bounded automatic NTP synchronization between the radio and cover workers.
HTTPS verification remains enabled. Radio reports detailed errors and stops certificate retries.
Artwork downloads survive navigation, distinguish processed albums from saved images, and report
completion or failure. Manual fetching bypasses earlier negative-cache records. The host suite
simulates NTP fallback/cancellation/deadlines, redirects, cover errors and artwork display after
switching to the Player. No system clock changes are made by host builds or tests.

Version 1.1.5 replaced the fixed UI polling cadence with input/deadline waits and 30 FPS
animations. Static views redraw only on visible changes, and the scaled skin main window is
cached with regional updates. The Player footer includes the hold-X visualizer shortcut.
Stay-awake ownership is stored in /tmp with a two-second retry limit, and cover misses are loaded
once per fetch and compacted. OSS requests six 8 KB fragments; the game-music ring is unchanged.

This directory contains the RedAmp player, codec bridge, audio wrapper, build scripts and
host regression tests. The archive follows the MiniCinema-style `App/` and `src/` layout.

Version 1.1.0 introduced `covers.c` (threaded MusicBrainz/Cover Art Archive fetcher),
a two-slot artwork cache, variable list fonts and progress bars, and a stop-on-first-error
radio handler.
Host cover tests run against local mock endpoints; no production credentials are required.

On a source-only VM, no `App/` tree is required. Running `make` builds the game-audio wrapper,
private ARM dependencies, player and private FFmpeg audio libraries into the source tree.
`make wrapper`, `make player`, `make arm-deps` and `make ffmpeg-audio` work independently.

Use `make host` for desktop tests and `make test` for the regression suite. ARM builds need
MiniCinema's cross-compiler or Zig setup and the FFmpeg 4.4.8 tree at
`/root/workspace/ffmpeg-4.4.8`. See `docs/BUILDING.md`, `docs/ARM_DEPENDENCIES.md` and
`docs/FFMPEG_AUDIO.md`. The host executable is not a Miyoo binary.

The application assets, required font files, default RedOnion skin and licenses are in
`App/RedAmp/` within the complete source archive.

For this update, `make player` rebuilds every modified player component. The v1.0.11
ARM FFmpeg and game-audio libraries can stay in place if the v1.0.11 security fixes were
already compiled and installed. Real device, TLS and cover-download behavior still need testing.
