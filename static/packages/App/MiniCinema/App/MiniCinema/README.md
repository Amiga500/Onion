# MiniCinema

**Version: 1.1.41**

**Created by Rob and Veuks.**

MiniCinema is a modern video player for RedOnion, based on FFplay/FFmpeg 4.4.8 and Veuks' KidsPlay
improvements. It targets Miyoo Mini, Mini Plus and Mini Flip.

MiniCinema uses a lightweight framebuffer browser and a separately compiled video playback engine.
The original Onion video player is not modified.

## Supported features

- Lightweight browser for folders and videos, natural filename sorting, resume percentages,
  completed (`END`) and failed (`BAD`) markers, and optional image previews.
- Non-recursive folder video counts, plus letter-group and page navigation with shoulder buttons.
- FFmpeg-based video playback with fit, fill, stretch and original-size aspect modes;
  selectable scaler and Quality, Balanced (default) and Speed performance profiles.
- Automatic resume, 15-second checkpoints, 3-second startup rewind, restart confirmation,
  chapter navigation and optional previous/next video controls.
- Audio and text-subtitle track selection, preferred languages, delays, subtitle size and
  position, and optional Dialogue boost for movies with quiet speech.
- External and embedded text subtitles, including language-tagged SRT files.
- Manual and automatic lossless PNG previews, fast compression, and confirmed replacement.
- Configurable X shortcut for track selection, mute, overlay changes, video navigation,
  restart, frame stepping, preview capture, exit, OSD dismissal and session-only live statistics.
- Optional 640x480 PNG screen bezels, and a shared Controls Help image in browser and player.
- Five-second cancelable auto-next countdown, optional battery warnings, configurable
  temporary OSD duration, and hold-Y battery view.
- Short-POWER pause/sleep recovery, adjustable pause auto-dimming and a sleep timer,
  controlled-shutdown resume,
  direct-file launch and optional diagnostic logging.

## Installation

Install MiniCinema under `App/MiniCinema/` on the SD card. Normal app use needs both the
`minicinema` browser and the `minicinema-player` playback engine, along with the compatible runtime
libraries expected under `App/MiniCinema/libs/`. Direct-file playback skips the browser.

Videos belong in `/mnt/SDCARD/Media/Videos/` or any of its subfolders. The launcher requires
RedOnion's existing `fbmode` utility. It does not install or replace system libraries.

To play a file directly (for example, from a RedOnion Emu launcher), pass its path to the app:

```sh
/mnt/SDCARD/App/MiniCinema/launch.sh "/mnt/SDCARD/Media/Videos/Movie.mp4"
```

This mode does not open or require the browser executable. Normal app launches without a file
argument still open the MiniCinema browser. Playlist navigation needs a playlist from the browser.

## Playback controls

| Button | Action |
| --- | --- |
| A | Resume if paused; reveal progress while playing (optional pause shortcut) |
| B | Pause; next B exits if enabled, otherwise show progress while paused |
| POWER (short) | Pause/checkpoint and immediately blank the video; resume on wake |
| LEFT / RIGHT | Seek back/forward 10 seconds; hold to seek repeatedly, then faster |
| UP / DOWN | Tap for +/-5 minutes; hold for repeated +/-10-minute jumps |
| L1 / R1 | Optional previous / next chapter (disabled by default) |
| L2 / R2 | Optional previous / next video (disabled by default) |
| Hold Y | Show battery level while held; release to hide immediately |
| START (release) | Cycle Fit, Fill, Stretch and Original, if shortcut enabled (default On) |
| SELECT (release) | Open or close the playback options menu |
| MENU (one tap) | Return to the MiniCinema browser |
| MENU (double tap) | Exit MiniCinema to RedOnion MainUI |
| START+SELECT | Immediate exit to RedOnion MainUI |
| X | Configurable action, default Off; see Settings > Controls |

MENU double-tap is recognized within 320 ms. Holding MENU does not exit. A single tap
returns to the browser. MENU combined with RedOnion's system brightness shortcut does not exit
MiniCinema; brightness remains controlled by RedOnion keymon. Explicit MENU and START+SELECT exits
start a three-second teardown watchdog after saving the checkpoint and exit marker. B returns to the
browser on the next B press while paused if `b_second_press_exit=1` (the default),
regardless of the delay between presses. With this shortcut disabled, B shows progress
while paused instead of exiting. A always resumes a paused video.
By default, pressing A during playback only reveals progress. Enable its optional shortcut to
pause with A and resume with another A. START aspect cycling can also be disabled; the aspect
option remains available in the playback menu. START+SELECT still exits with either setting.

UP jumps forward five minutes and DOWN jumps back five minutes. Holding for 650 ms repeats
10-minute jumps approximately every 700 ms until released. Shoulder buttons do not perform
time-based seeking. If enabled chapter controls find no chapters, L1/R1 show a short message.
Rewind notices appear on the left and forward notices on the right. The progress bar has a small
position marker. The player clears the display to black as soon as its SDL renderer starts,
before media loading. The startup progress OSD is Off by default. Browser Settings can turn it
On to display immediately after the first video frame; initial timing values may be inaccurate
until synchronization settles. Pressing A or seeking still brings up the OSD when requested.

### Playback options (SELECT)

Playback Options has three pages: the main menu, Subtitles, and Video Actions. Use UP/DOWN to
navigate, A or RIGHT to open a section, and LEFT/RIGHT to adjust applicable values. SELECT closes
all pages. B returns from a section to Playback Options, or closes the main menu. The bottom
playback OSD stays hidden while the menu is open, including during adjustments. RedOnion, not
MiniCinema, draws the system volume OSD through keymon; changing that bar's colors requires a
RedOnion keymon change.

The main page contains Audio Track (only for multiple tracks), Audio Delay, Subtitles (only when
supported text subtitles exist), Aspect Ratio (current video only), Auto Next, Controls Help and
Video Actions.
The Subtitles page contains On/Off, subtitle track, delay (0.1-second steps, up to 15 seconds each
way), size and position. Video Actions contains Previous Video, Next Video, Restart and Capture
Preview and session-only Live stats. A Chapter list appears when chapter metadata exists.
Restart and Replace preview ask for A to confirm or B to cancel. Changing Aspect Ratio
in the player never changes the browser's saved default. Undefined audio-language metadata (`und`)
is not displayed. The menu does not include a Video Information screen.

Automatic next-video playback is **off by default**. After natural completion, if another video
is available, MiniCinema displays a five-second next-video countdown. Press B to cancel and
return to the browser; the finished video is still marked `END`.

Audio and subtitle delay, subtitle size and position, automatic-next, browser previews, shoulder
preferences, resume and auto capture are stored in `data/player-settings.conf`. They are loaded
on subsequent plays or browser launches.
The browser's Subtitles On/Off preference controls whether subtitles start enabled for new videos
(default On). The player's Subtitles menu can change visibility for the current video without
rewriting that global default. Selected audio/subtitle tracks are not remembered. Aspect mode starts
with the browser's global default, but pressing START to cycle aspect modes never changes that
setting. A video that reaches its natural end after decoding video frames has its resume point
cleared and is marked `END`. If the player fails, or auto-exits without producing a video frame, it
is marked `BAD` instead. Retrying a file successfully clears `BAD`. Seeking to the last seconds
does not mark a video complete or trigger auto-next. Returning manually to the browser preserves
resume progress. Replaying a completed video and leaving partway through clears its previous `END`
mark and shows the latest saved position instead.

In addition to H.264/H.265, MiniCinema builds a small set of older MPEG, H.263, AVI and
Windows Media decoders. Common audio formats include AAC, MP3, MP2, AC-3/E-AC-3, DTS, WMA,
PCM and several ADPCM variants. The actual file must still have a supported container and
video decoder, and decoding demanding files may exceed the Miyoo's CPU budget.

## Browser controls

| Button | Action |
| --- | --- |
| UP / DOWN | Previous / next entry; wraps between first and last |
| A | Open folder or play selected video (resume if possible) |
| X | Restart selected video from beginning, after confirmation (A yes, B cancel) |
| SELECT | Open browser Settings menu |
| START | Toggle video PNG previews on/off (saved globally) |
| B | Parent folder; at browser root, stay in MiniCinema |
| L2 / R2 | Previous / next page (8 entries; always available in browser) |
| L1 / R1 | Previous / next initial-letter group (always available in browser) |
| Hold UP + L2 | First item |
| Hold DOWN + R2 | Last item |
| Hold Y | Show battery level while held; release to hide immediately |
| MENU or START+SELECT | Return to RedOnion MainUI |

The footer shows SELECT OPTIONS (or SEL OPTIONS when space is limited). The video count is
right-aligned and has reserved space even when many videos are listed. START toggles previews.

The bottom-right browser counter shows the selected video's number among the files in the
current folder (for example, `3/12`). When a folder is selected, it instead shows `(n)`, the
number of videos directly in that folder. Subfolders are not counted; results are cached in RAM
without writing to SD. The browser remembers the folder, highlighted video, scroll position and
preview visibility. START hides or restores the entire preview panel, giving filenames the full list
width. Previews are not
decoded from SD while disabled. Videos sort naturally: `Episode 2` comes before `Episode 10`.
Filenames with Latin-1 letters such as å, ä, ö, Å, Ä and Ö render with embedded DejaVu
Sans glyphs. Other Unicode scripts still need more glyph coverage. Next/previous video during
playback uses that same order. With resume enabled, the browser displays progress percentage when
duration is known, `END` for completed videos, and `BAD` for failed videos. With resume disabled,
progress and `END` are hidden. All marks fit in three characters. For older resume files without
duration information, a red dot is shown until the video is played again. Watched indicators never
require probing or decoding media while browsing.

### Browser Settings (SELECT)

SELECT opens Settings. Choose a section with UP/DOWN and A or RIGHT. Inside each section,
UP/DOWN selects an option and LEFT/RIGHT changes its value; A also adjusts.
B goes back to the section list, then closes the menu. It preserves the previously selected
section. Subtitles is a separate main Settings section; B returns directly to the section list.
SELECT also closes the menu. START toggles previews, and START+SELECT exits to MainUI.
The selected-row background is aligned with the glyphs. The Settings menu uses the same compact
dark-panel design as the player.

- Playback: remember resume, auto next, video scaler, Performance mode (Quality/Balanced/Speed),
  default aspect ratio, Startup OSD On/Off (default Off), OSD duration (1/2/3/5/8/10 seconds;
  default 2 seconds), and Screen overlay (Off or an available PNG).
- Controls: player-only L1/R1 previous/next chapter and L2/R2 previous/next video, with
  descriptions directly beside the button labels and On/Off at the right. Also includes START
  aspect shortcut (default On), A pause/resume shortcut (default Off), second B exits
  (default On), and X button action (default Off). These toggles do not disable the shoulder
  buttons in the browser.
- Audio: audio delay, preferred audio language, and Dialogue boost On/Off (default Off).
- Subtitles: On/Off default for new videos (default On), delay, size, vertical position and
  preferred subtitle language.
- Browser & previews: show previews and automatic PNG capture after N seconds.
- Power & Battery: pause auto dim (Off/15/30/60/90/120 seconds, default 30), low-battery
  notice (Off/5/10/15%, default Off), and Sleep timer (Off/15/30/60/90 minutes or End of video;
  default Off). The timed option pauses and checkpoints before turning off the backlight.
  A wakes and resumes; other controls wake the screen without resuming. The timer applies across
  automatically chained videos. Timed expiry blanks the display but does not suspend the OS.
  End of video returns to the browser instead of auto-next.
- Controls help: opens the same full-screen player-controls PNG used by the playback menu.

Shared preferences are saved to `data/player-settings.conf`. The player loads updated preferences
when a video starts; scaler and default aspect changes take effect on the next video. The default
aspect is Fit (0), Fill (1), Stretch (2) or Original (3), editable only from browser Settings. An
absent settings file uses defaults. Custom SDL_ttf font paths remain manual-only. The player always
negotiates BGRA output with FFmpeg for the Mini renderer, so there is no color-conversion setting.
The scaler menu labels Bicubic as best quality, Bilinear as balanced and Fast Bilinear as fastest;
actual sharpness and speed depend on the source video. Performance mode is Balanced by default.
The menu describes Quality as normal decoding, Balanced as faster decoding for larger sources, and
Speed as a mode that may skip frames. Speed may also reduce image quality.

`startup_osd_immediate=0` (default) disables the automatic startup progress OSD entirely.
Set `startup_osd_immediate=1` to display it immediately after the first video frame; its initial
timing values may be inaccurate. This setting does not affect manually requested progress OSD.
The OSD duration also controls the separate low-battery warning. It does not time-limit subtitles,
open menus, or the battery icon shown while holding Y. Changes in Browser Settings are saved
globally
and apply to subsequent videos.

### Player shoulder buttons and shortcuts

Both shoulder pairs are always available in the browser for letter-group and page navigation.
Only their **playback** actions are disabled by default. Enable them in browser Settings >
Controls, or edit these shared preferences in `App/MiniCinema/data/player-settings.conf`:

```ini
shoulder_l1_r1=1
shoulder_l2_r2=1
start_aspect_shortcut=1
a_pause_shortcut=0
b_second_press_exit=1
x_button_action=0
```

Use `0` to disable the associated shoulder pair while playing video; missing or invalid values
also disable it in playback. Browser letter-group and page navigation remain available. Set
`b_second_press_exit=0` to disable second-B exit while paused. In playback,
L1/R1 skip chapters and L2/R2 change videos when enabled.
`start_aspect_shortcut=0` disables the START aspect toggle, while `a_pause_shortcut=1` lets A
pause and resume playback. Their default values are 1 and 0, respectively. In Controls,
`x_button_action` has these choices: Off (0), Audio track (1), Subtitles (2), Screen overlay
On/Off (3), Mute (4), Cycle overlays (5), Live stats (6), Close OSD (7), Restart video (8),
Next video (9), Next chapter (10), Exit player (11), Next frame (12), Take preview (13).
Cycle overlays includes Off as a choice; Screen overlay (3) only toggles the selected image.
Restart video asks for confirmation. Next frame works only when paused. Take preview deliberately
replaces an existing preview without confirmation and saves a clean frame without subtitles or OSD.
Exit player returns to the MiniCinema browser when launched normally, or exits the application
when launched directly. Live stats show codec/resolution/FPS, dropped frames, A/V difference,
buffer levels, CPU usage and clock, sampled twice a second only while the panel is enabled.
Close OSD hides temporary playback information without disabling subtitles or held-Y battery.
X still restarts the selected video in the browser, regardless of this setting. Saving other
preferences preserves these lines.

### Custom subtitle font and decoding performance

A custom subtitle font is manual-only; add it to the same global settings file if needed:

```ini
subtitle_font=/mnt/SDCARD/Fonts/MyFont.ttf
```

The video scaler and audio delay are both available in the browser Settings GUI (Playback >
Video scaler and Audio > Audio delay). They may also be specified in `player-settings.conf` as
`video_scaler=bilinear` and `audio_delay_ms=0`, respectively.

`subtitle_font` is optional. Invalid or missing fonts fall back to the default subtitle font. Only
subtitles use this custom font; the player OSD remains on its normal font and both menus keep the
embedded DejaVu font. Numeric-only subtitle dialogue is preserved, including when SRT cue
separators are missing. The player caches clean frames for OSD redraw but avoids restoring a
freshly uploaded clean frame before drawing its first overlay. Automatic PNG preview capture
remains synchronous. The filter graph requests BGRA output directly, matching the Miyoo's ARGB8888
texture path.

`video_scaler` accepts `bicubic`, `bilinear` (default) or `fast_bilinear`. The setting controls
explicit aspect-ratio scaling filters as well as the fallback swscale conversion. The sharper
bicubic mode costs more CPU; fast bilinear may look softer. The actual speed improvement depends on
the source video and the Miyoo device.

Performance mode is available under Browser Settings > Playback and is saved globally as
`performance_mode=quality`, `balanced` (default) or `speed`. The selected mode is evaluated
separately for each video's codec and resolution. It changes decoder settings, not the scaler or
source file:

| Decoder optimization | Balanced | Speed |
| --- | --- | --- |
| Skip loop filtering | Non-reference frames above SD; all above 720p | All |
| Fast decoding flag | Above SD | All resolutions |
| Half-resolution decode | Above SD if width > 640 and codec supports it | Width/codec limit |
| Skip non-reference frames | No | Yes |

Here, "above SD" means more than 854 x 480 pixels; "above 720p" means more than 1280 x 720.
Half-resolution decoding uses the decoder's supported lowres level, never exceeding its limit;
H.264, HEVC and VP9 generally do not support it in this FFmpeg build. On supported older codecs,
it can reduce clarity, even in Speed, so it is never applied to sources 640 pixels wide or less.
Speed can reduce image quality and motion smoothness (particularly with B-frames). Balanced makes
less aggressive trade-offs and leaves SD-sized videos unchanged. Quality disables the additional
decoder optimizations; actual results depend on the codec, video and device.

`audio_delay_ms` ranges from -2000 to +2000 in 100 ms steps. Positive delays make audio play later
relative to video; negative delays make it earlier. This adjusts FFplay's audio clock for video
synchronization, not the underlying audio samples. Verify lip sync and seeking on hardware.

The Audio and Subtitles Settings sections offer preferred stream language: Auto (default),
English, Spanish, French, German, Arabic, Chinese, Danish, Dutch, Finnish, Italian,
Japanese, Norwegian, Polish, Portuguese, Swedish or Turkish. When a language is absent,
FFplay falls back to its regular best-stream choice rather than disabling audio or subtitles.
The preferences are saved as `preferred_audio_language` and `preferred_subtitle_language` using
three-letter tags such as `eng`, `swe`, `nld`, `nor`, `dan`, `fin`, `zho`, `por`, `ara`, `tur`
and `pol`. Common alternate two-letter codes are recognized in language-tagged SRT filenames.
They apply to the initial track selection; you can
still change supported tracks through the player menu. Audio decoders request stereo output
where supported, avoiding unnecessary multichannel conversion on the Miyoo.

`night_mode=1` enables Dialogue boost, using a light compressor and gain to make quieter speech
more audible and soften sudden loud effects. On supported 5.1 streams it also weights the centre
(dialogue) channel in the stereo downmix, keeping all six decoded channels until the filter.
Default `night_mode=0` uses ordinary audio handling. This feature changes the sound and should
be tested with actual speakers and headphones; it is not loudness normalization.

When resume is enabled, periodic checkpoints are written every 15 seconds.
Pausing, seeking and exiting also save an immediate checkpoint at the actual position. Each new
player launch that restores a saved position starts three seconds earlier, including after a full
shutdown or reopening from MainUI. Seeking to an earlier keyframe may increase the actual rewind.
Normal pause/resume and sleep/wake do not apply this rewind.
Paused playback wakes the UI at roughly 50 ms intervals to reduce unnecessary work.

### Resume and automatic preview preferences

These preferences may also be changed in browser Settings, or manually in
`App/MiniCinema/data/player-settings.conf`:

```ini
remember_resume=1
auto_preview_seconds=-1
```

Set `remember_resume=0` to start every video from the beginning without writing periodic position
files or updating playback progress. Existing saved positions and completed marks are retained on
SD, but hidden while this setting is off. The separate `BAD` status is still displayed.
`remember_resume=1` restores normal resume and progress behavior.

`auto_preview_seconds=-1` (or a missing entry) disables automatic screenshots. Set it to a number
from `0` to `3600` to capture a clean frame after that many seconds of video playback, but only
when starting from the beginning and no matching PNG preview exists. Existing artwork is never
replaced. In Browser Settings, A from Off or 0 selects 5s; RIGHT progresses Off, 0, 5s, 10s,
and so on. LEFT from 0 returns to Off. Preview capture works independently of the preview panel.

While video is playing, MiniCinema creates `/tmp/stay_awake` to prevent automatic suspend. A short
POWER press is monitored through `/dev/input/event0`. MiniCinema immediately pauses, checkpoints,
removes its own stay-awake flag and draws a black frame without the pause/progress OSD. RedOnion
keymon remains responsible for the actual backlight and sleep transition. When keymon uses
SIGSTOP/SIGCONT for sleep, the player resumes as soon as keymon continues it
on wake, without waiting for another button. POWER events queued during suspension are drained
on wake, with only a brief debounce for duplicates. Fresh sleep presses are accepted on every
cycle. A video playing before sleep resumes playing; if it was already paused, it remains paused.
For sleep configurations without
SIGSTOP, the POWER, clock and backlight fallbacks remain. Another player key can also recover a
missed wake; that key is consumed to avoid unintended seeks/exits. This behavior still requires
hardware testing on each Miyoo model. MiniCinema never removes a stay-awake flag belonging to
another application.

While normally paused, the backlight dims to duty cycle 3 after the chosen pause dim interval
(default 30 seconds), then switches off 15 seconds later. Settings > Power & Battery offers
Off, 15, 30, 60, 90 and 120 seconds; Off disables both automatic steps. A while auto-dimmed wakes
and resumes
immediately unless a menu is open. Other keys restore brightness without acting; their releases are
also consumed. The automatic pause dimmer
does not run during POWER-initiated sleep, where keymon controls the display. Playback, menu
interaction, changes to system brightness, or leaving the player cancel normal pause dimming.
The launcher uses `/tmp/minicinema_backlight` as a crash-recovery safety net. Device-specific PWM
paths and power events need checking on the Mini, Mini Plus and Mini Flip before public release.

On a controlled power shutdown, MiniCinema attempts to restore the original Onion launch command
and record whether it was showing a video or the browser. The next boot should return to that
view; videos reload their saved checkpoint when `remember_resume=1`. This uses Onion's
`cmd_to_run.sh` and `/tmp/.offOrder` behavior and remains **experimental on RedOnion**. Holding
MENU at boot and disabling Onion's auto-resume still take precedence. Normal exits do not create
a boot-resume marker, and a consumed marker is deleted immediately to avoid restart loops.

### Video previews and subtitles

For an external UTF-8 or Windows-1252 SRT, use the video basename with `.srt` or a language
suffix such as `.sv.srt`, `.swe.srt`, `.en.srt` or `.eng.srt`. MiniCinema tries the preferred
subtitle language first, then the plain `.srt`, then another matching language-suffixed file.
Windows-1252 is a fallback only when the SRT fails UTF-8 validation. For a browser preview,
place a PNG
inside an `Imgs/` directory under the video's directory:

```text
Media/Videos/Movies/
    Movie One.mp4
    Movie One.sv.srt
    Movie Two.mkv
    Imgs/
        Movie One.png
        Movie Two.png
```

Subtitles default to On for new videos unless disabled under Browser Settings > Subtitles.
They can also be toggled for the current video from SELECT > Subtitles. External SRT
takes precedence unless an embedded subtitle track is explicitly selected. When no SRT was loaded,
basic embedded text subtitles (such as MKV SubRip or MP4 mov_text) can be displayed without
advanced ASS positioning and styling. The build enables text subtitle decoders, but **not bitmap
subtitle decoders** (PGS, DVD/VobSub or DVB). Those tracks cannot currently be displayed. Subtitle
cues use regular-weight system fonts with a dark backing panel. If text does not appear, enable
RedOnion logging and check `reports/last-play.log`. Common SRT timestamps, UTF-8 BOM and CRLF are
supported, as are SRT files that omit a blank line after a cue. Long subtitle lines wrap by
measured pixel width without splitting UTF-8 characters; the display is limited to six lines to
avoid covering too much video. Font size, position and timing use the SELECT menu; these
preferences are saved globally for subsequent videos.

Capture preview samples a clean decoded frame at 320x240 and stores a lossless RGB PNG in
`Imgs/<video name>.png`. PNG Sub filtering and zlib compression reduce SD usage; automatic
captures use fast level 1 and manual captures level 3, never stronger compression. If zlib is
unavailable, stored DEFLATE blocks are used. Captures write a `.png.tmp` file and rename it
atomically so interrupted writes cannot damage an existing preview.

In SELECT > Video Actions, Capture preview saves immediately when there is no image. Once one
exists, the row changes to Replace preview; A opens a confirmation and B cancels. Replacement
uses the same temporary-file process. Automatic captures never overwrite existing previews.
Capture may still cause a brief one-time interruption during playback, especially on slow cards.
The browser also supports RGB, RGBA, grayscale and indexed PNG previews. Missing or
unsupported images show a text placeholder.

The battery indicator reads RedOnion's `/tmp/percBat` first (0-100), then
`/sys/class/power_supply/*/capacity` from a battery-type power supply. Unknown appears as `--%`.
The indicator is temporary in playback and browser modes. While MiniCinema runs, the launcher
creates `/tmp/hasBatteryDisplay` if absent to suppress RedOnion's separate low-battery icon. It
removes that flag on exit only if MiniCinema created it. Settings > Power & Battery also
provides a low-battery notice with Off (default), 5%, 10% and 15% thresholds. When enabled,
MiniCinema checks the battery every 30 seconds while playing and shows a red LOW battery icon
and percentage once per player session. Its display time uses Playback > OSD duration
(default 2 seconds). It does not pause playback.

### Controls Help and screen overlays

Both the browser Settings root and the player Playback Options main menu have a **Controls help**
entry. They display the same `App/MiniCinema/help/controls.png` image, an 8-bit, non-interlaced
640x480 RGB/RGBA PNG up to 2 MB. B or SELECT closes help and returns to the previous menu; other
buttons are ignored except POWER, which retains its normal sleep behavior. The player pauses
while help is open and resumes only if the video was already playing. The image never appears
in previews. A missing or unsupported image shows a help-unavailable message.

Supply your own `App/MiniCinema/help/controls.png` image. The source package intentionally does
not include a default image or artwork export files. Use neutral descriptions for configurable
shortcuts so the same picture remains accurate in both the browser and player.

Screen overlays are optional user-provided PNGs in `App/MiniCinema/overlays/`, exactly 640x480.
With RGBA transparency, a bezel can cover the borders of Fit mode without obscuring the picture.
Select Screen overlay under Settings > Playback or in the player's Playback Options. Both show
longer filenames and save to the same global setting. The player can switch overlays immediately
without restarting playback. The overlay is drawn above video but below subtitles and menus,
and it does not appear in captured previews.
Transparent areas are skipped, and fully opaque areas are copied efficiently. A full-screen
semi-transparent overlay can cost significant CPU time, especially in Speed mode with demanding
videos. Overlays remain available in all performance modes. X can toggle the selected overlay or
cycle through Off and all overlay files when configured.
Both shortcuts work while playing or paused. The battery OSD centers its icon and percentage
(or LOW notice) as a group within the existing dark box.

### Display and aspect ratio

Seek and chapter notices appear in the upper right, away from the bottom progress bar. START cycles
four fixed 640x480 outputs: Fit preserves aspect with borders, Fill crops, Stretch fills without
preserving aspect, and Original centers the original pixels (cropping large sources). The browser
Settings menu chooses the default mode for new videos (Fit when unset). START only changes the
current video's mode. Changing modes rebuilds a video-filter graph, so a brief hiccup is possible.
RedOnion's `fbmode` performs display ownership changes; the launcher does not modify the system
video player or shared libraries.

## Recommended video format

MiniCinema currently uses **software decoding** on the Miyoo. A 1920x1080 H.264 High Profile video
can lag badly even when displayed at 640x360: the full 1080p frame must still be decoded. Preparing
a compact file beforehand is more reliable than trying to play demanding sources.

Recommended final settings:

| Setting | Recommendation |
| --- | --- |
| Container | MP4 |
| Video | H.264/x264, 8-bit, 4:2:0 (yuv420p) |
| Resolution | 640x480 maximum; 640x360 for widescreen |
| Pixel aspect ratio | Square pixels (anamorphic disabled) |
| Aspect ratio | Preserve source; add black borders if needed, never stretch |
| Frame rate | Source rate, constant, usually 24/25/30 fps; do not convert to 60 fps |
| Quality | RF/CRF 20; x264 Medium; Main Profile, Level 3.1 |
| Color | Limited range; preserve the source's appropriate color space |
| Audio | AAC stereo, 48 kHz, 256 kb/s |
| Loudness target | -14 LUFS integrated, -3 dBTP true peak, LRA 7 LU |

For potentially troublesome sources, H.264 Baseline Profile can be easier to decode than Main, with
some compression penalty. 640x480 is the upper-bound recommendation, not a promise that every
codec, bitrate or unusual encode will play smoothly.

### HandBrake + Shutter Encoder workflow

Both tools are free. Encode the picture with HandBrake at RF 20, preset Medium, H.264 Main/3.1,
constant source frame rate, 8-bit 4:2:0. Disable anamorphic. To create a fixed 640x480 frame, use
sizing and border padding to preserve the picture's aspect ratio. Avoid filters except where needed
for interlaced sources. Use the source's correct color space (often BT.709 for HD).

If possible, create a **lossless FLAC 16-bit stereo, 48 kHz** intermediate audio track with 0 dB
gain and DRC disabled, downmixing 5.1 audio only when necessary. Many applications do not support
FLAC in MP4, so **MKV is the safer intermediate container**. It will be remuxed as MP4 with AAC in
the final step. If HandBrake cannot offer the intermediate format, use the FFmpeg commands below or
encode AAC directly; a lossless intermediate is an optional quality measure.

In Shutter Encoder, normalize to -14 LUFS, -3 dBTP true peak and LRA 7 LU, outputting AAC stereo at
256 kb/s and 48 kHz. Use an appropriate loudness-analysis workflow for precise results; lossy AAC
encoding can change the measured true peak slightly. For forced subtitles, burning only the forced
track into the picture is the most universally reliable option.

### FFmpeg: quick one-step MP4 conversion

Run these commands on a computer with a full FFmpeg build, **not on the Miyoo**. This command
preserves the picture inside a fixed 640x480 frame, normalizes volume in one pass, and outputs
H.264/AAC. One-pass `loudnorm` is convenient but not as precise as two-pass measurement.

```sh
ffmpeg -i "input.mp4" \
  -vf "scale=640:480:force_original_aspect_ratio=decrease:flags=lanczos,\
pad=640:480:(ow-iw)/2:(oh-ih)/2:color=black,setsar=1,format=yuv420p" \
  -c:v libx264 -preset medium -crf 20 -profile:v main -level 3.1 \
  -fps_mode cfr -color_range tv \
  -af "loudnorm=I=-14:TP=-3:LRA=7" \
  -c:a aac -b:a 256k -ar 48000 -ac 2 \
  -movflags +faststart "output-mini.mp4"
```

The above filter is shown split across shell lines using a backslash. If your shell does not
support that form, put the complete `-vf` filter on one line. `-fps_mode cfr` normally retains the
nominal input frame rate; for unusual variable-frame-rate sources, explicitly specify the original
intended rate (for example `-r 25` for 25 fps footage).

### FFmpeg: lossless intermediate, then AAC normalization

For a workflow closer to HandBrake + Shutter Encoder, make an intermediate MKV with FLAC audio:

```sh
ffmpeg -i "input.mp4" \
  -vf "scale=640:480:force_original_aspect_ratio=decrease,\
pad=640:480:(ow-iw)/2:(oh-ih)/2,setsar=1,format=yuv420p" \
  -c:v libx264 -preset medium -crf 20 -profile:v main -level 3.1 \
  -fps_mode cfr -c:a flac -sample_fmt s16 -ar 48000 -ac 2 "intermediate.mkv"
```

Normalize once, encode to AAC and copy the already compressed H.264 video without re-encoding:

```sh
ffmpeg -i "intermediate.mkv" -c:v copy \
  -af "loudnorm=I=-14:TP=-3:LRA=7" \
  -c:a aac -b:a 256k -ar 48000 -ac 2 \
  -movflags +faststart "output-mini.mp4"
```

For exact loudness targets, use two-pass `loudnorm`: first measure with `print_format=json`, then
pass the reported `measured_I`, `measured_TP`, `measured_LRA` and `measured_thresh` back to the
second pass together with `linear=true`. Check the final AAC file using a loudness meter.

## Runtime files

- `App/MiniCinema/data/`: browser state, playlist, global `player-settings.conf`, and optional
controlled-shutdown `boot_resume` / `cmd_to_run.saved` files.
- `App/MiniCinema/resume/`: saved positions, percentage index and completed-video list.
- `App/MiniCinema/reports/last-play.log`: current diagnostic session, when logging is enabled.
- `App/MiniCinema/reports/last-play.prev.log`: previous diagnostic session, when enabled.
- `App/MiniCinema/libs/`: compatible private SDL2 and playback runtime libraries.

Diagnostics are written only while `/mnt/SDCARD/.tmp_update/config/.logging` exists. Without
that flag, MiniCinema leaves existing reports untouched and sends diagnostics to `/dev/null`.
When enabled, `last-play.log` becomes `last-play.prev.log` before a new log is created, keeping
one prior session. Logging is optional and playback does not depend on it.

The launcher creates the `data/`, `resume/` and `reports/` directories if absent. It requests a
640x480, two-page framebuffer handoff through RedOnion `fbmode`. RedOnion's runtime is responsible
for restoring the native MainUI display mode after the app exits, including on 752x560 panels.

## Build the modified player on your Linux VM

Source/build files are in `src/`. MiniCinema requires FFmpeg 4.4.8, Zig 0.14.1 and a
compatible Miyoo SDL2 port. The Zig wrappers use an `arm-linux-gnueabihf.2.28` target to pin the
glibc ABI. The default Zig executable is `/root/workspace/zig-0.14.1/zig` in the persistent
VM/container workspace.

The runtime `libSDL2-2.0.so.0` used with MiniCinema is an SDL **2.0.20** Miyoo rebuild supplied
through **Veuks' Kids Mode**. Its source lineage is Rparadise-Team's fork of Steward-Fu's port:

- FFmpeg: https://ffmpeg.org/releases/ffmpeg-4.4.8.tar.xz
- Miyoo SDL2 fork: https://github.com/Rparadise-Team/sdl2_miyoo_new
- Original Miyoo SDL2 port: https://github.com/steward-fu/sdl2
- Kids Mode: https://github.com/Veuks/onion-kids-mode

The identified SDL2 rebuild uses Zig 0.14's Clang/LLD (LLVM 19.1.7), the Miyoo Mini MI_GFX video
renderer and **OSS `/dev/dsp` audio**, which follows RedOnion's `libpadsp`/audio-server path rather
than direct MI_AO output. Its renderer supports 640x480 RGB565/ARGB8888 textures. The exact SDL2
source commit and original build recipe are not documented; this package does not rebuild SDL2.

If needed, install the download tools before obtaining Zig 0.14.1 in the Linux build environment:

```sh
apt-get update
apt-get install -y curl xz-utils ca-certificates
```

Place Zig 0.14.1 in `/root/workspace/zig-0.14.1/`, using a persistent workspace mount to keep it
across container recreation. From the MiniCinema source directory containing `Makefile`, run:

```sh
make all
make check
```

`make all` builds `libminivsync.so` first, then the browser, then the FFmpeg player.

The build script has default locations for FFmpeg, SDL2 and Zig; set `FFMPEG_SRC`, `SDL2_ROOT` and
`ZIG_BIN` to override them on another Linux environment. `SDL2_ROOT` supplies the development
headers and SDL_image/SDL_ttf libraries; `SDL2_LIB_DIR` overrides which SDL2 shared library is
used for linking. Without that override, the script searches the local SDL2 tree and may select a
different Steward-Fu prebuilt with direct MI_AO audio. For consistent OSS audio behavior, link
against the same compatible SDL2 rebuild that will be installed in MiniCinema's private `libs/`.
The `minicinema` browser and `minicinema-player` playback engine are produced in `src/`; install
both under `App/MiniCinema/`.
`make vsync` creates `src/libminivsync.so`, which must be installed under `App/MiniCinema/libs/`.
The launcher requires this library. You may override `ZIG_TARGET` for `make vsync` if needed; its
default is `arm-linux-gnueabihf.2.28`. The build script restores the original FFmpeg
`fftools/ffplay.c` and the temporary include files on completion. MiniCinema does not install
binaries or replace global RedOnion libraries.

## AI-assisted development

MiniCinema has been developed with AI assistance for implementation, interface changes, debugging
and documentation. Project maintainers direct development and carry out hardware tests.
AI-generated changes can contain mistakes and are subject to the same review as other code. AI
assistance does not change the licenses, copyright notices or credits of upstream work.

## License and credits

**MiniCinema creators: Rob and Veuks.**

MiniCinema builds on work by **Veuks**, creator of Kids Mode and KidsPlay, including its playback
improvements and display synchronization. Thank you to Veuks for publishing the source:
https://github.com/Veuks/onion-kids-mode. That project distributes a GPL-3.0 license. MiniCinema's
new browser and subtitle components also identify themselves as GPL-3.0-or-later. See `LICENSE` for
the GPL version 3 text. These terms do not supersede licenses on third-party components.

The player is a modified **FFplay** derived from **FFmpeg**, originally written by Fabrice Bellard
and maintained by the FFmpeg contributors. The FFplay source preserves its LGPL-2.1-or-later
notice. MiniCinema builds against FFmpeg 4.4.8, with libraries linked statically into
`minicinema-player`. The included build script does not enable FFmpeg's optional GPL or nonfree
components. The `libx264` encoder used in the conversion examples is a *desktop encoding tool*; it
is not an external library linked into MiniCinema's playback engine.

The SDL2 runtime used with MiniCinema is the SDL **2.0.20** Miyoo rebuild distributed with
Veuks' Kids Mode, from https://github.com/Rparadise-Team/sdl2_miyoo_new (a fork of Steward-Fu's
https://github.com/steward-fu/sdl2). Its audio backend is OSS `/dev/dsp`, not Steward-Fu's MI_AO
backend. The SDL2 zlib notice is supplied in `SDL2-license.txt`. Retain the notices applicable to
the actual SDL2 build, its port changes, and any separately distributed SDL_image/SDL_ttf and
FreeType libraries; the fork's own repository also contains licensing information.

MiniCinema's embedded, pre-rasterized DejaVu Sans glyphs use the DejaVu/Bitstream Vera font terms;
see `DejaVu-font-notice.txt`. The player also uses font files provided by the installed system. See
`FFmpeg-LGPL-2.1.txt` and `SDL2-license.txt` for the corresponding supplied notices.

### Source availability and redistribution

When distributing a compiled MiniCinema player, also provide the corresponding MiniCinema source,
the exact FFmpeg source version, the changes to FFplay and the build instructions/configuration.
Because the FFmpeg libraries are **statically linked**, check the LGPL requirements for providing
relinkable application objects or another compliant means of rebuilding and relinking with a
modified LGPL library. Merely including the LGPL text, or linking to an upstream download, is not
by itself a complete compliance strategy. The official FFmpeg checklist is at
https://ffmpeg.org/legal.html.

The SDL2 port and any redistributed runtime libraries also need their corresponding license notices
and, where applicable, source/relinking materials. In particular, prebuilt graphics and firmware
libraries (including SwiftShader/EGL/GLES and SigmaStar `libmi_*` components) must be checked
individually for provenance and redistribution rights. MiniCinema's source licenses alone do not
authorize redistribution of vendor binaries. An installation using libraries already supplied by
the device is different from packaging those binaries for redistribution.

This section summarizes upstream license information and practical distribution considerations; it
is not legal advice. Contributors and redistributors should review the actual licenses of the
specific source revisions and binaries they provide.
