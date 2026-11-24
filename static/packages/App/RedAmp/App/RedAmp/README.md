# RedAmp
Version 1.1.11

Powerful music player for Miyoo handhelds, designed for RedOnion and compatible OnionOS setups.

RedAmp supports local MP3, FLAC, WAV, Ogg Vorbis, Opus and optional AAC/M4A/ALAC playback. Internet
radio supports MP3, Vorbis, Opus and optional AAC/HLS using private FFmpeg 4.4.8 libraries. It
includes equalizer, visualizers, playlists, artwork and an indexed music library.

## Controls

| Button | Action |
|---|---|
| MENU (release) | Exit from any screen; MENU+button shortcuts do not exit |
| SELECT + START | Exit RedAmp |
| SELECT (tap) | Switch between Player and Library from either screen |
| SELECT (hold 0.85s) | Open Options in Player or Library without switching views |
| START | Pause or resume (when pressed alone) |
| A | Play/select |
| B (player) | No action; use SELECT to switch between Player and Library |
| B (library) | Back one level; at Library home stays on Library home |
| B (hold 0.65s, Library) | Open the quick Artists / Albums / Songs / Folders / Home picker |
| X (tap) | Toggle normal/expanded playlist when on the main player screen |
| X (hold 1s) | Open visualizer immediately after holding 1 second, without releasing X |
| X / Y (library) | Add selected music / play all; home-screen X opens the Library menu |
| Y (player) | Equalizer |
| L / R | Previous/next track |
| Left / Right | Seek in playlist and library, or change a selected setting |
| L2 / R2 (library) | Page through library entries |

MENU exits when released, not when pressed. Holding MENU and using another button (for example,
POWER for a system screenshot shortcut) does not exit if both button events reach RedAmp. For
reliable SELECT + START detection, the individual SELECT and START actions occur on button release.
Playlist layout and the last Player/Library mode are saved between sessions. In the library, press
X on its home screen for the manual cover-fetch and rescan menu. In EQ, press X for presets,
A to apply and hear each preset without leaving the list, and B to return to the EQ controls.
Preset curves are more distinctive, with preamp headroom to reduce distortion from boosted bands.

## System battery indicator

RedAmp creates `/tmp/hasBatteryDisplay` when the foreground player starts, suppressing RedOnion's
blinking low-battery system overlay. It removes the marker when exiting only if RedAmp created it.
A marker created by another process is never deleted by RedAmp. The OS battery-monitoring and
critical shutdown mechanisms remain active; the marker only changes the warning display.

## Music library

Choose Artists, Albums, Songs or Folders. RedAmp scans `Media/Music` (or `Music` as a fallback) for
up to 8192 tracks, caching metadata in `data/library-cache-v5.bin`. It does not follow symlink
directories. Album covers can come from `cover.jpg`, `cover.png`, `folder.jpg`, `folder.png`, MP3
ID3v2 APIC or FLAC picture tags. The Folders view can also browse outside those library roots.
Selecting a song or playing an album keeps the indexed album list or Folder browser open; the
playing song is marked independently of the selected row, and a small playback strip and progress
bar appear at the bottom. Returning to the library preserves the previous view and scroll position.
LEFT/RIGHT seeks in the indexed library and Folders browser (including holds); L2/R2 pages through
entries. Tap SELECT to return to the main player at any library depth; hold SELECT to open Options
without leaving the current library level. Hold B from any indexed Library level or Folder path
for the quick views picker; use Up/Down to select Artists, Albums, Songs, Folders or Library home,
A to switch directly and B to cancel. A short B press still goes back one level.
Backing out of Artists, Albums or Songs restores that category at Library home.
The Folder browser and indexed Library use the same row counts and regular-weight text. Playing
tracks have a vertically centred play or pause marker in both views and in the bottom status strip.
The library and expanded playlist show a compact progress bar when the track length is known.

## Display and list text size

The Options screen now has a **Display & Covers** submenu. Lists in normal and expanded playlists,
library views, folder browser and skin selector have Small, Medium and Large font and row sizes.
Medium is the new default even for existing users with no `list_text_size` setting. Changes apply
immediately and retain the selected item. Footer shortcut hints stay at 18px independently of
the list-size setting.
The Player footer includes **Hold X — Visualizer**, alongside the normal X layout shortcut.
The Library and Folder browser use tighter and matching row heights (28/30/36 px) with a clear
separator above the now-playing strip. The cursor remains on the category just exited at root.
The expanded playlist now-playing title and elapsed/total time use the selected list font size.
The Library and Folder browser now share identical text/progress positioning. Folder entries
use the same regular-weight row font and selected/unselected colors as indexed Library entries.
Both Options screens use
a matching wider, better-spaced layout.

## Automatic and player album covers

The Display & Covers submenu provides **Download album covers** (default Off), **Save covers in
album folders** (default Off), and **Show cover on player** (default On). A manual **Fetch missing
covers now** command is in the X menu on the library home screen. Background fetching starts after
an indexed-library scan when enabled, pauses with the screen off, and does not run during game
music. Fetching continues when browsing or returning to the Player. To stop it, use the library
home X menu's **Cancel cover fetch** item. No music files are modified by a cache download.

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

Normal playlist layout displays a pre-scaled 150x150 cover on the right when available; the track
list becomes narrower. Expanded playlist has a 22px cover thumbnail in its now-playing footer.
Without artwork, the full playlist width is used. Radio stations show no placeholder cover. A
two-slot image cache keeps album-browsing artwork independent of the currently playing song.

## Skins

**Ember** is the preferred default if an authentic `Ember.wsz` is installed; otherwise RedOnion is
used. RedOnion is a CC0 Breeze Dark adaptation with burgundy `#86112B`, with brighter red controls
and selected text for contrast against dark backgrounds. The original Breeze Dark skin is preserved
unchanged. Existing saved skin choices are retained. Choose **Smooth scaling** in Options > Display
& Covers for **Smooth** (bilinear) or **Sharp** (default; classic 2x nearest-neighbor pixels).
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

In Options, select **Allow insecure HTTPS** and press A. Default: Off. This setting is persistent.
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

Music in games is disabled until explicitly enabled in Options. RedAmp itself does not replace
RedOnion's `miyoo/lib/libpadsp.so`. This feature requires RedOnion's game/menu launcher to source
`redonion-launch-audio.sh` before launching the target process. The Options menu reports unverified
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
and are particularly costly. For maximum battery life, turn **Mini visualizer** off in Display &
Covers, use screen-off playback, lower the backlight, and disable automatic cover downloads when
unnecessary. Sharp skin rendering is also less CPU-intensive than Smooth. Actual battery savings
have not been measured. `MA_PROFILE=1` logs frame/CPU statistics and total loop/frame counts.

## Screen-off playback

In Options, select **Play during standby** and press A. Default: On. This setting is persistent.

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
distribute host x86 binaries to the Miyoo. For v1.1.11, run `make player` and copy the updated player
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
