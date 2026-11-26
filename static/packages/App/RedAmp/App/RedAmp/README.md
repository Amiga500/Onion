# RedAmp
Version 1.2.1

Powerful music player for Miyoo handhelds, designed for RedOnion and compatible OnionOS setups.

RedAmp supports local MP3, FLAC, WAV, Ogg Vorbis, Opus and optional AAC/M4A/ALAC playback. Internet
radio supports MP3, Vorbis, Opus and optional AAC/HLS using private FFmpeg 4.4.8 libraries. It
includes equalizer, visualizers, saved playlists, favorites, artwork and an indexed music library.

## Navigation and controls

RedAmp has four destinations: Library, Now Playing, Queue and Settings. SELECT cycles through
these destinations; the highlighted tab indicates the current view. Library and Settings keep
their position when switching away. New installations open Library with the Modern appearance.
Choose Settings > Display & Skins > Playing appearance to switch to the classic Winamp window.
The active queue is always a separate screen, independent of the selected appearance.

| Button | Action |
|---|---|
| A | Open or play the selected track; play/pause in Now Playing |
| B | Back or cancel |
| X | Toggle favorite on a track |
| Y | Actions for the selected item or view |
| SELECT | Cycle Library, Now Playing, Queue, Settings |
| START | Pause/resume |
| L/R | Previous/next track (keyboard: switch page) |
| Left/Right | Navigate Library grid, seek in track lists, or adjust settings |
| L2/R2 | Page through track lists |
| MENU (release) | Exit; recognized screenshot chords do not exit |
| SELECT + START | Exit |

No essential feature needs a long press. Dialogs use A/B for confirmation/cancel; the shared
keyboard retains A Type, X Erase, Y Save/Search and L/R Page. In EQ, X opens presets, A toggles
EQ, and B goes back. Open EQ and the full-screen visualizer from Now Playing's Y Actions menu.
Settings are grouped into Audio & Equalizer, Display & Skins, Album Covers, Internet & Radio,
Power & Standby, Music in Games and Library Management. Quit RedAmp exits directly.
Modern appearance uses a standalone equalizer; Winamp retains the classic EQ layout.

## System battery indicator

RedAmp creates `/tmp/hasBatteryDisplay` when the foreground player starts, suppressing RedOnion's
blinking low-battery system overlay. It removes the marker when exiting only if RedAmp created it.
A marker created by another process is never deleted by RedAmp. The OS battery-monitoring and
critical shutdown mechanisms remain active; the marker only changes the warning display.

## Music library and Search

Library home has Artists, Albums, Songs, Folders, Playlists, Favorites, Search and Internet Radio.
Use the directional pad to navigate its grid. RedAmp indexes up to 8192 tracks in `Media/Music`
(or `Music`) and caches tags in `data/library-cache-v5.bin`. It does not follow symlink directories.
Folders can browse elsewhere and open M3U/M3U8/PLS files. B goes back one level; SELECT changes
views without losing the library position. Choose Y > Library views from any library screen
to jump directly to a category. The playing track is marked independently of selection.

Search uses the existing index to match title, artist, album artist, album and filename, with
Unicode case-insensitive partial matching. Matching is accent-sensitive. Use Y > Edit search
or A when there are no results. The keyboard offers QWERTY, International and Symbols pages,
Shift, Space, and UTF-8-aware Backspace. Names and queries support up to 40 Unicode characters.

## Play queue, saved playlists and favorites

A on an individual track plays an existing queue entry or appends it and plays it. It never
replaces the queue. X toggles a favorite. Y opens the actions menu: Play next, Add to queue,
Add to saved playlist, Add all to queue, and Play all (replace queue), as appropriate. Replacing
an existing queue requires confirmation, including opening an external playlist file in Folders.
In shuffle mode, Play next starts a new shuffle sequence with the selected track next; manual
queue moves also rebuild the shuffle order. The Queue screen displays stored order, not the
randomized sequence. START pauses/resumes; shuffle and repeat apply to the active queue.

On Queue, Y offers Move track, Remove from play queue, Save play queue as playlist, and Clear
play queue. Move track uses Up/Down to choose a destination and A to commit or B to cancel.
Removing the currently playing track stops playback. Clearing the queue requires confirmation.
Neither operation affects saved collections or music files.

Library > Playlists allows creating, opening, renaming and deleting named collections. Use
Y Actions to add tracks to playlists or remove a selected saved track. Deleting a playlist
requires confirmation. Favorites may contain music and radio stations. Press X on a track to
toggle its status; a smooth heart marks favorites. Playlists and Favorites are independent of
the active queue. Names must be FAT-safe and unique ignoring case. Saved playlists are stored
in `data/playlists/*.m3u`, Favorites in `data/favorites.m3u`, and the queue in `data/playlist.m3u`.
Back up these files to retain collections. Up to 128 playlists and 10,000 tracks per collection
are supported.

## Display and list text size

Settings > Display & Skins provides Modern/Winamp appearance, skin selection, scaling, classic
mini visualizer, time display and Small/Medium/Large list text sizes. Settings > Album Covers
controls artwork separately for Now Playing and Library. The Modern Now Playing view uses
large artwork, title/artist/album information, elapsed time, progress and playback status.
The Winamp appearance preserves the original classic skin window, with track details below it.

## Automatic and player album covers

Settings > Album Covers provides **Download album covers** (default Off), **Save covers in
album folders** (default Off), **Show cover on player** (default On), and
**Show covers in Library** (default On). Turning library covers off widens the list without
affecting downloaded artwork or Player covers. A manual **Fetch missing covers now** command is in Settings > Library Management (or Library home > Y Actions). Background fetching starts after
an indexed-library scan when enabled, pauses with the screen off, and does not run during game
music. Fetching continues when browsing or returning to the Player. To stop it, use the library
maintenance menu's **Cancel cover fetch** item. No music files are modified by a cache download.

The footer reports albums checked and images actually saved. A completion message reports saved,
existing and unavailable covers, or a clock, network, server, response or storage error. Details
are recorded in `log.txt`. A manual fetch retries earlier negative-cache entries. RedAmp does not
cache temporary network or server errors as missing artwork. Automatic downloads and radio share
one bounded NTP clock check (see Radio).

RedAmp searches MusicBrainz release groups, requiring score >=90, an artist match (ignoring case
and the prefix "The "), and a matching album title (ignoring case). It then retrieves Cover Art
Archive's `front-500`. Requests are paced at least 1.5 seconds apart. HTTP 503 (busy) and 429
(rate limiting) are retried after 5, 15 and 30 seconds, or longer if the server sends a
`Retry-After` value. If it asks for more than 120 seconds, the batch stops instead of retrying
too early. A persistently busy server is reported separately from missing artwork. The delay
is cancellable. The User-Agent identifies RedAmp, its version and the RedOnion project URL.
Downloads must decode within 1024x1024 and 3 MB. Missing covers are recorded in `data/covers/missing-v2.txt`
and retried after 30 days; expired and duplicate entries are compacted.

Artwork is keyed by album artist (or track artist if absent) and album title, not directory.
Downloads are saved atomically as `data/covers/album-<hash>.jpg`. When folder saving is enabled,
RedAmp additionally writes `redamp-<hash>.jpg` beside the representative track. A generic folder
image is used only when the directory is not known to contain conflicting albums. Embedded
track artwork takes priority over downloaded art. Tracks without a matching image remain blank.
Existing user folder images are not overwritten. Artwork is refreshed after download completion
without sharing renderer surfaces between threads.

Modern Now Playing displays artwork at 236x236 when enabled; Winamp mode uses a smaller
cover below the classic skin. A two-slot image cache keeps browsing artwork independent
of the currently playing song. When artwork is unavailable, Modern shows a generic record icon.

## Skins

**Ember** is the preferred default if an authentic `Ember.wsz` is installed; otherwise RedOnion is
used. RedOnion is a CC0 Breeze Dark adaptation with burgundy `#86112B`, with brighter red controls
and selected text for contrast against dark backgrounds. The original Breeze Dark skin is preserved
unchanged. Existing saved skin choices are retained. Choose **Smooth scaling** in Settings > Display & Skins for **Smooth** (bilinear) or **Sharp** (default; classic 2x nearest-neighbor pixels).
Filtering applies only to the Winamp player window, not library text or album artwork. Smooth
scaling may need hardware performance testing on older Mini models. RedAmp places its name on the
main player header at runtime for every installed skin, without changing the original bitmaps.
Original Ember must be obtained separately using `src/tools/fetch_cc0_skins.py`; if absent, the safe
fallback is RedOnion. See `skins/SKIN_CREDITS.md` for exact provenance and optional CC0 skin
installation.

## Radio

Add HTTP or HTTPS station URLs to `stations.m3u`, or open local M3U/PLS files through Folders. When
a stream fails, RedAmp stops automatic station cycling instead of attempting every station. DNS and
connection failures end their retry loop after the first failed attempt; selecting a station
manually still retries on demand. Certificate verification errors are not retried; the player logs
the detailed reason, response type, byte count and decoder result. Cancelled station opens do not
report errors for newer requests. Redirect and HTTP error bodies are not passed to the audio
decoder. Live MP3, Vorbis and Opus use RedAmp's existing decoders. While opening a station,
the top player display shows `Loading radio...` and the mini spectrum/scope stays still. The normal display and audio
visualization return after the first decoded audio block is sent to output.

On Miyoo, an invalid clock triggers a silent NTP worker at launch if the network is usable.
Internet radio and missing artwork wait for the shared worker before networking. Changing stations
or cancelling artwork does not cancel clock synchronization.
With no usable IPv4 network interface/default route, RedAmp skips NTP and automatic cover fetches.
A previously paused radio station is restored without opening a connection; press A or START to
play it. A local route does not guarantee Internet access, so automatic cover network/time errors
are logged rather than shown as startup notifications. Manual cover fetching still reports results.

If the clock is implausible, RedAmp tries the system `ntpdate` with two time servers, then BusyBox
`ntpd` even if `ntpdate` was installed but failed. A final direct SNTP fallback to Google Public NTP
IPv4 addresses avoids dependency on the firmware utilities or DNS. The total attempt is bounded to
20 seconds, shared by network workers and followed by a 30-second failure cooldown. The worker
is stopped on application exit.
No persistent service, Wi-Fi activation or timezone changes are made. Each attempt and its result
are recorded in `log.txt`; there are no repeating synchronization popups. NTP/SNTP is unauthenticated
and may fail if UDP port 123 is blocked. HTTP streams can still play if time sync fails; HTTPS
certificate verification is enabled by default. A manual request may report a network or date error.
AAC/HE-AAC and HLS use the private FFmpeg 4.4.8 library set in `lib/` when installed. Verified
HTTPS requires a trusted CA bundle in `cacert.pem` or an available system trust store. Certificate
verification must also cover HLS segments and AES-128 keys using the private RedAmp FFmpeg patch.
Use the v1.1.10 private FFmpeg libraries to support both HTTPS modes. Radio metadata is displayed when supplied by the station. The bundled
`stations.m3u` contains 86 station entries. Individual live availability, regional access and codec
negotiation have not been verified on-device.

### Allow insecure HTTPS (optional)

In Settings > Internet & Radio, select **Allow insecure HTTPS** and press A. Default: Off. This setting is persistent.
When enabled, RedAmp skips its NTP synchronization and disables HTTPS certificate and hostname
verification for radio and cover requests, including redirects. The updated private FFmpeg bridge
and HLS patch also disable TLS verification for live streams, nested playlists, segments and AES
keys. A trusted CA file is not required in this mode. HTTPS still uses encryption, but does not
authenticate the server: an attacker on the network could intercept or replace audio and artwork.
Only enable this setting if you accept that risk. There is no automatic insecure fallback.
Switching the setting reconnects an actively playing station and cancels a running cover batch;
start a new cover fetch if needed. Local audio is unaffected. With an invalid system date, existing
negative-cache records are not expired and new misses are not persisted until time is valid.

Raw `.aac` files use an ADTS frame-count scan to obtain an accurate duration without trusting
FFmpeg's bitrate-based guess. The background library worker scans whole files and caches validated
results in memory; playback reuses them without rereading each file. For an uncached file over 32
MiB, playback starts without a length rather than blocking for a full-file scan. An unknown or
malformed ADTS header yields unknown length, never FFmpeg's unreliable bitrate guess. The new
`library-cache-v5.bin` automatically invalidates old, possibly incorrect duration metadata; the
original audio file is never modified. For this fix, you must install the newly built
`libredamp_ffaudio.so`; updating the main `redamp` binary alone is insufficient.

## Optional music in games

Music in games is disabled until explicitly enabled in Settings > Music in Games. RedAmp itself does not replace
RedOnion's `miyoo/lib/libpadsp.so`. This feature requires RedOnion's game/menu launcher to source
`redonion-launch-audio.sh` before launching the target process. The settings screen reports unverified
RedOnion launch integration until a launcher has sourced the hook at least once. A separate **Game
music level** defaults to 70% for mixing headroom; the regular music volume setting is independent.
The feature remains experimental and needs device-specific validation. AAC/HLS playback is not
handed into games.

To recover the original audio library from an older installation which used library replacement, run
`redamp-recover.sh`. Do not delete the app before recovery succeeds. `redamp-uninstall.sh` provides
a convenient recovery entry point.

## Battery and CPU use

On the Miyoo, RedAmp waits for input or the next UI deadline with `poll()`. Animations run at
30 FPS: mini or full-screen visualizers, titles that do not fit, and the brief
0.6-second response period after input. Otherwise a frame is presented only when something visible
changes, such as a clock tick, progress-bar pixel, pause blink, toast or newly available data. With
the screen off, input still wakes the app immediately, and housekeeping runs about once per second.
Audio completion and error events also wake the UI without waiting for its next deadline.
The scaled main skin window is cached, and only changed areas are rescaled. Host screenshot
tests retain their frame-based input cadence; a separate timed-input test exercises the new loop.

The OSS output requests six 8 KB fragments (about 256 ms at 48 kHz stereo), with 8 KB writes
when accepted. This reduces the frequency of writes; pause and seek can take longer to be heard
because more audio may be queued. Actual buffering depends on the device and audioserver.
Sample rate, decoder and resampling quality are unchanged. The game-music ring retains its
previous output chunk size.

The mini spectrum/scope and full-screen visualizer require FFT analysis and frequent redraws,
and are particularly costly. For maximum battery life, turn **Mini visualizer** off in Settings > Display & Skins, use screen-off playback, lower the backlight, and disable automatic cover downloads when
unnecessary. Sharp skin rendering is also less CPU-intensive than Smooth. Actual battery savings
have not been measured. `MA_PROFILE=1` logs frame/CPU statistics and total loop/frame counts.

## Screen-off playback

In Settings > Power & Standby, select **Play during standby** and press A. Default: On. This setting is persistent.

- On: RedAmp uses `/tmp/stay_awake` while playing. With compatible keymon versions, POWER or
  closing the lid turns off the screen while music keeps playing.
- Off: RedAmp uses RedOnion's `/tmp/video_playing` activity marker instead. Compatible keymon
  versions prevent idle sleep during playback but still suspend the player on POWER or lid close.
  Playback resumes on wake. Older OnionOS versions may not recognize this marker.
- RedAmp's own **Turn screen off** option continues playback in either mode.

RedAmp releases only its own marker when paused, stopped or closed, including in its background
music service. Each marker has a separate ownership record in `/tmp`; foreign flags are retried
at most once every two seconds. On SIGCONT the foreground player resets button holds, redraws and
reconnects an active radio station. A paused station stays paused. Whether the detached music
service is suspended as well depends on the installed keymon's process-selection behavior.
Actual POWER, lid and audio-device resume behavior needs verification on the target hardware.

## Building

The source and build scripts are in `src/`; the runtime assets are in `App/RedAmp/`. On the VM,
`make` builds the ARM game-audio wrapper, dependencies, player and private FFmpeg backend, with no
App directory required there. The default FFmpeg source path is `/root/workspace/ffmpeg-4.4.8`. See
`src/docs/BUILDING.md` and `src/docs/FFMPEG_AUDIO.md`. To install, combine the assets with matching
ARM `redamp`, `libpadsp_wrap.so`, `lib/*.so*` and `cacert.pem` from the build output. Don't
distribute host x86 binaries to the Miyoo. For v1.2.1, run `make player` and copy the updated player
to the app. The v1.1.10 private FFmpeg libraries, dependency and game-wrapper builds can remain.
When upgrading from an earlier release, run `make ffmpeg-audio` as well for the v1.1.10 HTTPS
policy patch and bridge. Older corrected bridges work in verified mode, but explicitly insecure
live FFmpeg playback requires the updated bridge and HLS library.

## AI-assisted development

RedAmp has been developed with AI assistance for implementation, interface changes, debugging and
documentation. Project maintainers direct development and carry out hardware tests. AI-generated
changes can contain mistakes and are subject to the same review as other code. AI assistance does
not change the licenses, copyright notices or credits of upstream work.

## License and credits

RedAmp is based on MiniAmp 0.2.3 by Keith Snyder. The original MIT license and copyright, including
`Copyright (c) 2026 Keith Snyder`, are preserved in `licenses/LICENSE`. RedAmp's modifications are
also distributed under the MIT license where applicable. The original author retains copyright to
the original code; the fork does not transfer it.

The CC0 skins and third-party components have their own notices. License texts for fonts, OpenSSL
and FFmpeg and the third-party notice summary are together in `licenses/`. `skins/SKIN_CREDITS.md`
documents artwork provenance. Redistributing ARM FFmpeg and other libraries also requires the
corresponding source, build information and applicable license obligations. See
`licenses/THIRD_PARTY_NOTICES.md` and `src/docs/FFMPEG_AUDIO.md`.
