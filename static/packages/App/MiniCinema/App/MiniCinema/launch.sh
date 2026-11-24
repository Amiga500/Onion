#!/bin/sh
# MiniCinema 1.1.41 - RedOnion standalone video player (KidsPlay-derived).
# Copyright 2026 MiniCinema contributors; KidsPlay engine by Veuks.
set -u
APP=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P) || exit 1
SYS=/mnt/SDCARD/.tmp_update
MIYOO=/mnt/SDCARD/miyoo
MEDIA=/mnt/SDCARD/Media/Videos
BR="$APP/minicinema"
PLAYER="$APP/minicinema-player"
FBMODE=
for candidate in "$SYS/bin/fbmode" "$MIYOO/app/fbmode" /usr/bin/fbmode; do
    if [ -x "$candidate" ]; then FBMODE="$candidate"; break; fi
done
if [ -z "$FBMODE" ]; then
    FBMODE=$(command -v fbmode 2>/dev/null || true)
fi
SELECTED=/tmp/minicinema_selected
POSRUNTIME="/tmp/minicinema_position.$$"
EXIT_FILE="/tmp/minicinema_exit.$$"
BACK_FILE="/tmp/minicinema_back.$$"
NEXT_FILE="/tmp/minicinema_next.$$"
NEXT_CANCEL_FILE="/tmp/minicinema_next_cancel.$$"
PREV_FILE="/tmp/minicinema_prev.$$"
DURRUNTIME="/tmp/minicinema_duration.$$"
SEEK_END_FILE="/tmp/minicinema_seek_end.$$"
FRAME_OK_FILE="/tmp/minicinema_frame_ok.$$"
RESTART_FILE=/tmp/minicinema_restart
PLAYLIST_FILE="$APP/data/playlist.txt"
COMPLETED_INDEX="$APP/resume/completed.tsv"
BAD_INDEX="$APP/resume/bad.tsv"
SETTINGS_FILE="$APP/data/player-settings.conf"
PLAY_KIND=back
LOG="$APP/reports/last-play.log"
ERROR_FILE=/tmp/minicinema_error
PROGRESS_INDEX="$APP/resume/index.tsv"
APP_EXIT_REQUESTED=0
AWAKE_OWNED=0
BATTERY_DISPLAY_FLAG=/tmp/hasBatteryDisplay
BATTERY_DISPLAY_OWNED=0
FB_PROBED=0
PLAY_CONTEXT=browser
SHUTDOWN_CAPTURED=0
ACTIVE_CHILD=
SLEEP_SESSION_ACTIVE=0
SLEEP_DEADLINE=0
SLEEP_END_VIDEO=0
BACKLIGHT_PATH=/sys/devices/soc0/soc/1f003400.pwm/pwm/pwmchip0/pwm0/duty_cycle
BACKLIGHT_SAVED=/tmp/minicinema_backlight
BOOT_RESUME="$APP/data/boot_resume"
SAVED_COMMAND="$APP/data/cmd_to_run.saved"
ONION_COMMAND="$SYS/cmd_to_run.sh"
PATH="$SYS/bin:$PATH"
export PATH
mkdir -p "$APP/resume" "$APP/data" "$MEDIA" "$APP/overlays" || exit 1
# Respect RedOnion's global diagnostic switch. Without it, never create,
# truncate or append the on-card log, and discard program diagnostics.
LOG_ENABLED=0
LOG_DEST=/dev/null
if [ -f "$SYS/config/.logging" ]; then
    mkdir -p "$APP/reports" || exit 1
    LOG_ENABLED=1
    LOG_DEST="$LOG"
    # Keep the previous diagnostic session through one relaunch. Never rotate
    # logs when system logging is off, and do not grow an unbounded history.
    if [ -f "$LOG" ] && ! mv -f "$LOG" "$APP/reports/last-play.prev.log"; then
        # A logging problem must never prevent playback or erase the old log.
        LOG_ENABLED=0
        LOG_DEST=/dev/null
    fi
    if [ "$LOG_ENABLED" -eq 1 ] && ! : > "$LOG"; then
        LOG_ENABLED=0
        LOG_DEST=/dev/null
    fi
fi
log() {
    [ "$LOG_ENABLED" -eq 1 ] || return 0
    printf '%s\n' "$*" >> "$LOG"
}
# Read the auto-next preference from the same global file as SRT preferences.
# Do not source the file as shell code: only the exact boolean setting matters.
auto_next_enabled() {
    [ -r "$SETTINGS_FILE" ] || return 1
    awk -F= '{ sub(/\r$/, "") } $1 == "auto_next" { value=$2 }
        END { exit (value != "1") }' "$SETTINGS_FILE"
}
# Read one key without executing the settings file as shell code.
mc_setting() {
    [ -r "$SETTINGS_FILE" ] || return 0
    awk -F= -v wanted="$1" '{ sub(/\r$/, "") }
        $1 == wanted { value=$2 } END { print value }' "$SETTINGS_FILE"
}
# A sleep timer covers the complete auto-next chain, not each new video.
# Refresh it after returning to the browser and selecting a new video.
start_sleep_session() {
    if [ "$SLEEP_SESSION_ACTIVE" -eq 1 ]; then return; fi
    SLEEP_SESSION_ACTIVE=1
    SLEEP_DEADLINE=0
    SLEEP_END_VIDEO=0
    setting=$(mc_setting sleep_timer_minutes)
    case "$setting" in
        15|30|60|90)
            now=$(date +%s 2>/dev/null || true)
            case "$now" in ''|*[!0-9]*) ;; *) SLEEP_DEADLINE=$((now + setting * 60));; esac
            ;;
        -1) SLEEP_END_VIDEO=1 ;;
    esac
    log "SLEEP TIMER: $setting, deadline=$SLEEP_DEADLINE, end_video=$SLEEP_END_VIDEO"
}
remember_resume_enabled() {
    # Enabled by default; an explicit 0 disables all position/history writes.
    [ -r "$SETTINGS_FILE" ] || return 0
    awk -F= '{ sub(/\r$/, "") } $1 == "remember_resume" { value=$2 }
        END { exit (value == "0") }' "$SETTINGS_FILE"
}
# Last-resort recovery when the player exits/crashes while it owns the PWM.
# The player removes this marker on a clean restoration.
restore_backlight() {
    if [ -r "$BACKLIGHT_SAVED" ]; then
        IFS= read -r duty < "$BACKLIGHT_SAVED" || true
        case "$duty" in
            ''|*[!0-9]*) ;;
            *) if [ "$duty" -gt 0 ] && [ "$duty" -le 1000000 ]; then
                   printf '%s\n' "$duty" > "$BACKLIGHT_PATH" 2>/dev/null || true
               fi ;;
        esac
        rm -f "$BACKLIGHT_SAVED"
    fi
}

# Preserve Onion's exact command only for a controlled shutdown, never on a
# normal return to MainUI. RedOnion must be tested for the same flag behavior.
record_shutdown() {
    [ -f /tmp/.offOrder ] || return 1
    if [ "$SHUTDOWN_CAPTURED" -eq 1 ]; then return 0; fi
    SHUTDOWN_CAPTURED=1
    if [ -r "$SAVED_COMMAND" ] && [ ! -e "$SYS/config/.noAutoStart" ]; then
        temporary="$APP/data/.boot_resume.$$"
        if printf '%s\n' "$PLAY_CONTEXT" > "$temporary" &&
            mv -f "$temporary" "$BOOT_RESUME" &&
            cp "$SAVED_COMMAND" "$ONION_COMMAND"; then
            log "SHUTDOWN: saved $PLAY_CONTEXT startup state"
        else
            rm -f "$BOOT_RESUME" "$temporary"
        fi
    else
        rm -f "$BOOT_RESUME"
    fi
    sync
    return 0
}

handle_term() {
    trap - TERM INT
    if [ -n "$ACTIVE_CHILD" ]; then
        kill -TERM "$ACTIVE_CHILD" 2>/dev/null || :
        wait "$ACTIVE_CHILD" 2>/dev/null || :
        ACTIVE_CHILD=
    fi
    restore_backlight
    if record_shutdown; then exit 0; fi
    exit 143
}

cleanup() {
    restore_backlight
    if [ -f /tmp/.offOrder ]; then record_shutdown || :; fi
    # Leave a pre-existing flag belonging to another process untouched.
    if [ "$AWAKE_OWNED" -eq 1 ]; then rm -f /tmp/stay_awake; fi
    if [ "$BATTERY_DISPLAY_OWNED" -eq 1 ]; then rm -f "$BATTERY_DISPLAY_FLAG"; fi
    rm -f "$EXIT_FILE" "$BACK_FILE" "$NEXT_FILE" "$NEXT_CANCEL_FILE" "$PREV_FILE" \
        "$POSRUNTIME" "$DURRUNTIME" "$SEEK_END_FILE" "$FRAME_OK_FILE" \
        "$SELECTED" "$RESTART_FILE"
}
trap cleanup 0
trap handle_term TERM INT
# Suppress RedOnion's duplicate low-battery icon while MiniCinema is running.
# Noclobber makes creation exclusive: never claim or remove an existing flag.
if (set -C; : > "$BATTERY_DISPLAY_FLAG") 2>/dev/null; then
    BATTERY_DISPLAY_OWNED=1
fi
rm -f "$ERROR_FILE"
# A prior crashed instance may have left a dimmed screen.
restore_backlight
# Only snapshot a launch command that actually refers to MiniCinema. This
# protects against replacing another app's autostart command on poweroff.
if [ -r "$ONION_COMMAND" ]; then
    case "$(cat "$ONION_COMMAND")" in
        *'/MiniCinema/'*|*'/Emu/VIDEOS/'*) cp "$ONION_COMMAND" "$SAVED_COMMAND" ;;
        *) rm -f "$SAVED_COMMAND" ;;
    esac
else
    rm -f "$SAVED_COMMAND"
fi
# Consume one-shot boot state immediately, preventing reboot loops. A normal
# MainUI launch has /tmp/prev_state and must not consume the boot request.
BOOT_KIND=
if [ -r "$BOOT_RESUME" ]; then
    if [ ! -e /tmp/prev_state ] && [ ! -e "$SYS/config/.noAutoStart" ]; then
        IFS= read -r BOOT_KIND < "$BOOT_RESUME" || true
    fi
    rm -f "$BOOT_RESUME"
fi
case "$BOOT_KIND" in video|browser) ;; *) BOOT_KIND= ;; esac

# Same per-video hash as previous versions, now inside resume/.
position_file() {
    if command -v md5sum >/dev/null 2>&1; then
        key=$(printf '%s' "$1" | md5sum)
    else
        key=$(printf '%s' "$1" | cksum)
    fi
    key=${key%% *}
    case "$key" in ''|*[!0-9a-fA-F]*) return 1 ;; esac
    printf '%s/resume/.resume-%s.pos\n' "$APP" "$key"
}

# Uses the natural order written by the browser. No directory rescans in
# the playback engine. Paths are separated by newlines, like the selection file.
playlist_neighbor() {
    [ -r "$PLAYLIST_FILE" ] || return 0
    awk -v target="$1" -v direction="$2" '
        $0 == target {
            seen=1
            if(direction=="previous" && previous!="") {print previous;exit}
            next
        }
        seen && direction=="next" {print;exit}
        {previous=$0}
    ' "$PLAYLIST_FILE"
}

# Completed is separate from the resume index, so a video remains marked
# watched after clearing its resume position at normal end-of-file.
# A separate BAD index prevents broken or unsupported videos from showing as
# successfully completed, even if FFplay exits with status 0 before decoding.
set_bad() {
    target=$1
    failed=$2
    temporary="$APP/resume/.bad.$$"
    if [ -f "$BAD_INDEX" ]; then
        awk -v target="$target" '$0 != target' "$BAD_INDEX" > "$temporary" || : > "$temporary"
    else
        : > "$temporary"
    fi
    if [ "$failed" -eq 1 ]; then printf '%s\n' "$target" >> "$temporary"; fi
    mv -f "$temporary" "$BAD_INDEX" || log 'WARN: bad index update failed'
}

set_completed() {
    target=$1
    finished=$2
    temporary="$APP/resume/.completed.$$"
    if [ -f "$COMPLETED_INDEX" ]; then
        awk -v target="$target" '$0 != target' "$COMPLETED_INDEX" > "$temporary" || : > "$temporary"
    else
        : > "$temporary"
    fi
    if [ "$finished" -eq 1 ]; then printf '%s\n' "$target" >> "$temporary"; fi
    mv -f "$temporary" "$COMPLETED_INDEX" || log 'WARN: completed index update failed'
}

ensure_audio() {
    if ! pgrep audioserver >/dev/null 2>&1; then
        if [ -x "$MIYOO/app/audioserver" ]; then
            volume=$(/customer/app/jsonval vol 2>/dev/null || true)
            case "$volume" in ''|*[!0-9]*) volume=20 ;; esac
            defvol=$(awk -v v="$volume" \
                'BEGIN { printf "%.0f\n", 48 * (log(1 + v) / log(10)) - 60 }')
            case "$defvol" in ''|*[!0-9-]*) defvol=-30 ;; esac
            "$MIYOO/app/audioserver" "$defvol" >/dev/null 2>&1 &
        fi
    fi
    i=0
    while [ "$i" -lt 40 ] && { ! pgrep audioserver >/dev/null 2>&1 ||
                                [ ! -e /tmp/audio_fifo_server ]; }; do
        sleep 0.1
        i=$((i+1))
    done
    if ! pgrep audioserver >/dev/null 2>&1 || [ ! -e /tmp/audio_fifo_server ]; then
        log 'WARN: audio FIFO unavailable; playback may be silent.'
    fi
}

prepare_display() {
    if [ -z "$FBMODE" ] || [ ! -x "$FBMODE" ]; then
        log 'ERROR: RedOnion fbmode not found (required for safe display handoff).'
        return 1
    fi
    # The Miyoo SDL2 player currently uses 640x480 with two pages.
    # Probe without changing geometry; RedOnion owns native mode restoration.
    if [ "$FB_PROBED" -eq 0 ]; then
        original=$("$FBMODE" --probe 2>> "$LOG_DEST" || true)
        [ -n "$original" ] && log "DISPLAY BEFORE: $original"
        FB_PROBED=1
    fi
    if ! "$FBMODE" 640x480 --pages 2 --preclear --no-clear >> "$LOG_DEST" 2>&1; then
        log 'ERROR: fbmode failed to set 640x480 with two pages.'
        return 1
    fi
    return 0
}

play_video() {
    PLAY_CONTEXT=video
    video=$1
    APP_EXIT_REQUESTED=0
    PLAY_KIND=back
    if [ ! -f "$video" ]; then
        log "ERROR: video missing: $video"
        return 1
    fi
    posfile=$(position_file "$video") || {
        log "ERROR: could not generate resume key: $video"
        return 1
    }
    RESUME_ENABLED=1
    if ! remember_resume_enabled; then RESUME_ENABLED=0; fi
    start=0
    if [ "$RESUME_ENABLED" -eq 1 ] && [ -r "$posfile" ]; then
        IFS= read -r start < "$posfile" || true
        case "$start" in ''|*[!0-9]*) start=0 ;; esac
        # Start three seconds before the saved checkpoint on any new player
        # launch. Do not subtract time again in the player or checkpoint path.
        if [ "$start" -gt 3 ]; then start=$((start - 3))
        elif [ "$start" -gt 0 ]; then start=0
        fi
    fi
    if [ -f "$RESTART_FILE" ]; then
        start=0
        rm -f "$RESTART_FILE"
        if [ "$RESUME_ENABLED" -eq 1 ]; then
            rm -f "$posfile"
            set_completed "$video" 0
        fi
    fi
    checkpoint_output=$posfile
    position_output=$POSRUNTIME
    if [ "$RESUME_ENABLED" -eq 1 ]; then
        printf '%s\n' "$start" > "$POSRUNTIME"
    else
        # Neither a runtime position nor an SD-card checkpoint is needed.
        checkpoint_output=''
        position_output=''
        rm -f "$POSRUNTIME"
    fi
    rm -f "$DURRUNTIME" "$SEEK_END_FILE" "$FRAME_OK_FILE"
    preview_dir="${video%/*}/Imgs"
    preview_name="${video##*/}"
    preview_name="${preview_name%.*}"
    preview_file="$preview_dir/$preview_name.png"
    previous=$(playlist_neighbor "$video" previous)
    following=$(playlist_neighbor "$video" next)
    has_previous=0
    has_next=0
    if [ -n "$previous" ] && [ -f "$previous" ]; then has_previous=1; fi
    if [ -n "$following" ] && [ -f "$following" ]; then has_next=1; fi
    title=${video##*/}
    title=${title%.*}
    # Covers direct-file launches as well as the internal browser.
    printf '%s\n' "$video" > "$APP/data/.last-file.txt"
    ensure_audio
    player_lib="$APP/libs:/config/lib:/customer/lib:$SYS/lib"
    player_preload="$APP/libs/libminivsync.so"
    if [ -r "$MIYOO/lib/libpadsp.so" ]; then
        player_preload="$player_preload:$MIYOO/lib/libpadsp.so"
    else
        log 'WARN: libpadsp missing; DSP audio bridge unavailable.'
    fi
    unset PADSP_NO_DSP
    pref_audio=$(mc_setting preferred_audio_language)
    pref_subtitle=$(mc_setting preferred_subtitle_language)
    case "$pref_audio" in eng|swe|deu|fra|spa|jpn|ita|nld|nor|dan|fin|zho|por|ara|tur|pol) ;; *) pref_audio=auto ;; esac
    case "$pref_subtitle" in eng|swe|deu|fra|spa|jpn|ita|nld|nor|dan|fin|zho|por|ara|tur|pol) ;; *) pref_subtitle=auto ;; esac
    night_mode=$(mc_setting night_mode)
    [ "$night_mode" = 1 ] || night_mode=0
    # Preferred external subtitle, then untagged file, then any other named
    # language. Never interpolate an untrusted settings value into a glob.
    subtitle=''
    stem=${video%.*}
    case "$pref_subtitle" in
        swe) aliases='swe sv' ;; eng) aliases='eng en' ;;
        deu) aliases='deu de ger' ;; fra) aliases='fra fr fre' ;;
        spa) aliases='spa es' ;; jpn) aliases='jpn ja' ;;
        ita) aliases='ita it' ;; nld) aliases='nld nl dut' ;;
        nor) aliases='nor no nob nno nb nn' ;; dan) aliases='dan da' ;;
        fin) aliases='fin fi' ;; zho) aliases='zho zh chi' ;;
        por) aliases='por pt' ;; ara) aliases='ara ar' ;;
        tur) aliases='tur tr' ;; pol) aliases='pol pl' ;;
        *) aliases='' ;;
    esac
    for lang in $aliases; do
        if [ -r "$stem.$lang.srt" ]; then subtitle=$stem.$lang.srt; break; fi
    done
    if [ -z "$subtitle" ] && [ -r "$stem.srt" ]; then subtitle=$stem.srt; fi
    if [ -z "$subtitle" ]; then
        for candidate in "$stem".*.srt; do
            if [ -r "$candidate" ]; then subtitle=$candidate; break; fi
        done
    fi
    if [ -n "$subtitle" ]; then
        log "SUBTITLES: $subtitle"
    else
        log "SUBTITLES: no matching external .srt for $video"
    fi
    prepare_display || return 1
    rm -f "$EXIT_FILE" "$BACK_FILE" "$NEXT_FILE" "$NEXT_CANCEL_FILE" "$PREV_FILE"
    log "PLAY: $video"
    log "RESUME: $start seconds"
    # A global scaler choice also applies to the explicit aspect-ratio scale
    # filters; -sws_flags by itself does not necessarily control those.
    scaler=bilinear
    if [ -r "$SETTINGS_FILE" ]; then
        setting=$(awk -F= '{ sub(/\r$/, "") } $1 == "video_scaler" { value=$2 }
            END { print value }' "$SETTINGS_FILE")
        case "$setting" in bicubic|bilinear|fast_bilinear) scaler=$setting ;; esac
    fi
    # Per-video decoder optimizations use the browser's global preference.
    # Missing or invalid settings use Balanced's selective optimizations.
    perf_mode=balanced
    if [ -r "$SETTINGS_FILE" ]; then
        setting=$(awk -F= '{ sub(/\r$/, "") } $1 == "performance_mode" { value=$2 }
            END { print value }' "$SETTINGS_FILE")
        case "$setting" in quality|balanced|speed) perf_mode=$setting ;; esac
    fi
    log "PERFORMANCE: $perf_mode"
    overlay_name=$(mc_setting screen_overlay)
    screen_overlay=
    case "$overlay_name" in
        ''|.*|*/*|*\\*) ;;
        *.png|*.PNG)
            if [ -f "$APP/overlays/$overlay_name" ]; then
                screen_overlay="$APP/overlays/$overlay_name"
            fi ;;
    esac
    log "OVERLAY: ${screen_overlay:-off}"
    if [ -r "$APP/help/controls.png" ]; then
        log "HELP: controls.png ok"
    else
        log "HELP: controls.png missing"
    fi
    # The browser alone changes this persistent preference. START in the player
    # only changes the aspect mode for the current playback session.
    default_aspect=0
    if [ -r "$SETTINGS_FILE" ]; then
        setting=$(awk -F= '{ sub(/\r$/, "") } $1 == "default_aspect_ratio" { value=$2 }
            END { print value }' "$SETTINGS_FILE")
        case "$setting" in 0|1|2|3) default_aspect=$setting ;; esac
    fi
    start_sleep_session
    # All graphs output 640x480. Original uses a centered native-pixel crop.
    set -- -hide_banner -loglevel warning -nostats -framedrop -autoexit \
        -sws_flags "$scaler" \
        -vf "scale=640:480:force_original_aspect_ratio=decrease:flags=$scaler,pad=640:480:(ow-iw)/2:(oh-ih)/2" \
        -vf "scale=640:480:force_original_aspect_ratio=increase:flags=$scaler,crop=640:480" \
        -vf "scale=640:480:flags=$scaler" \
        -vf "crop=w='min(iw,640)':h='min(ih,480)',pad=640:480:(ow-iw)/2:(oh-ih)/2"
    log "SCALER: $scaler; pixel format: BGRA"
    if [ "$start" -gt 0 ]; then
        set -- "$@" -ss "$start"
    fi
    if [ "$pref_audio" != auto ]; then set -- "$@" -ast "m:language:$pref_audio"; fi
    if [ "$pref_subtitle" != auto ]; then set -- "$@" -sst "m:language:$pref_subtitle"; fi
    set -- "$@" -i "$video"
    next_title=${following##*/}
    next_title=${next_title%.*}
    # Only remove a stay_awake flag we were entitled to create ourselves.
    AWAKE_OWNED=0
    if [ ! -e /tmp/stay_awake ]; then AWAKE_OWNED=1; fi
    MC_VIDEO_SCALER="$scaler" MC_DEFAULT_ASPECT="$default_aspect" MC_PERF_MODE="$perf_mode" \
    MC_NIGHT_MODE="$night_mode" MC_SCREEN_OVERLAY="$screen_overlay" MC_NEXT_TITLE="$next_title" \
    MC_SLEEP_DEADLINE="$SLEEP_DEADLINE" MC_SLEEP_END_VIDEO="$SLEEP_END_VIDEO" \
    MC_AUTO_NEXT_CANCEL_FILE="$NEXT_CANCEL_FILE" \
    MC_SRT_FILE="$subtitle" VC_START_SECONDS="$start" VC_POSITION_FILE="$position_output" \
    VC_CHECKPOINT_FILE="$checkpoint_output" VC_MEDIA_KIND=video \
    VC_DURATION_FILE="$DURRUNTIME" MC_SETTINGS_FILE="$SETTINGS_FILE" \
    MC_PREVIEW_FILE="$preview_file" MC_NEXT_FILE="$NEXT_FILE" MC_PREV_FILE="$PREV_FILE" \
    MC_HAS_NEXT="$has_next" MC_HAS_PREV="$has_previous" \
    MC_SEEK_END_FILE="$SEEK_END_FILE" MC_FRAME_OK_FILE="$FRAME_OK_FILE" \
    VC_MEDIA_TITLE="$title" VC_SCREENSHOT_FILE='' \
    VC_OSD_FONT="$MIYOO/app/Helvetica-Neue-2.ttf" \
    VC_OSD_FONT_SIZE=24 VC_TIMER_FONT="$MIYOO/app/Helvetica-Neue-2.ttf" \
    VC_TIMER_FONT_SIZE=24 MC_EXIT_APP_FILE="$EXIT_FILE" \
    MC_BACK_BROWSER_FILE="$BACK_FILE" \
    LD_LIBRARY_PATH="$player_lib" LD_PRELOAD="$player_preload" \
    SDL_VIDEODRIVER=mini SDL_AUDIODRIVER=dsp \
        "$PLAYER" "$@" >> "$LOG_DEST" 2>&1 &
    ACTIVE_CHILD=$!
    wait "$ACTIVE_CHILD"
    status=$?
    ACTIVE_CHILD=
    restore_backlight
    if [ "$AWAKE_OWNED" -eq 1 ]; then rm -f /tmp/stay_awake; fi
    AWAKE_OWNED=0
    if record_shutdown; then
        # Never classify a shutdown as DONE, BAD, or auto-next. The checkpoint
        # was saved by kp_shutdown on SIGTERM or by the POWER pause handler.
        PLAY_KIND=shutdown
        return 0
    fi
    PLAY_CONTEXT=browser
    if [ "$status" -eq 143 ]; then
        # SIGTERM always takes priority over stale/competing navigation markers.
        # Preserve the checkpoint; never mark DONE, BAD, or auto-advance.
        PLAY_KIND=back
        log 'INTERRUPTED: SIGTERM (143), resume retained'
    elif [ -f "$EXIT_FILE" ]; then
        APP_EXIT_REQUESTED=1
        PLAY_KIND=exit
        log 'EXIT: double/long MENU or START+SELECT; returning to MainUI'
    elif [ -f "$BACK_FILE" ]; then
        PLAY_KIND=back
        log 'BACK: MENU or B after pause; returning to video browser'
    elif [ -f "$NEXT_FILE" ]; then
        PLAY_KIND=next
        log 'NEXT: next video requested'
    elif [ -f "$PREV_FILE" ]; then
        PLAY_KIND=previous
        log 'PREVIOUS: previous video requested'
    elif [ "$status" -eq 0 ] && [ ! -f "$FRAME_OK_FILE" ]; then
        # FFplay can autoexit successfully even when no valid video frame
        # was decoded. Treat that as BAD, never DONE or auto-next.
        PLAY_KIND=error
        log 'ERROR: player ended without a decoded video frame'
    elif [ "$status" -eq 0 ] && [ -f "$SEEK_END_FILE" ]; then
        # A manual seek to the last seconds is not a completed viewing.
        # Do not mark DONE or launch the next episode automatically.
        PLAY_KIND=back
        log 'END: reached boundary after manual seek (not marked complete)'
    elif [ "$status" -eq 0 ]; then
        # Normal end-of-file, not user return: next playback starts at 0.
        if [ "$RESUME_ENABLED" -eq 1 ]; then
            rm -f "$posfile"
            set_completed "$video" 1
        fi
        if [ "$SLEEP_END_VIDEO" -eq 0 ] && auto_next_enabled && [ ! -f "$NEXT_CANCEL_FILE" ]; then
            PLAY_KIND=next
        else
            PLAY_KIND=back
        fi
        log 'END: video completed'
    else
        PLAY_KIND=error
        log "WARN: playback error status $status (resume retained)"
    fi
    if [ "$PLAY_KIND" = error ]; then
        if [ "$RESUME_ENABLED" -eq 1 ]; then set_completed "$video" 0; fi
        set_bad "$video" 1
    elif [ "$status" -ne 143 ] && [ -f "$FRAME_OK_FILE" ]; then
        # A video that successfully decoded a frame is no longer BAD,
        # even when the viewer returns before reaching the end.
        set_bad "$video" 0
    fi
    log "PLAYER STATUS: $status"
    # When resume is disabled, retain old history untouched and hidden.
    # Durations support percentages; old entries without a duration remain dots.
    if [ "$RESUME_ENABLED" -eq 1 ]; then
    saved=0
    if [ -r "$posfile" ]; then
        IFS= read -r saved < "$posfile" || true
        case "$saved" in ''|*[!0-9]*) saved=0 ;; esac
    fi
    temp_index="$APP/resume/.index.$$"
    if [ -f "$PROGRESS_INDEX" ]; then
        awk -F '\t' -v target="$video" '$1 != target' "$PROGRESS_INDEX" > "$temp_index" || : > "$temp_index"
    else
        : > "$temp_index"
    fi
    if [ "$saved" -gt 0 ]; then
        # A new partially watched session supersedes an old END marker.
        # Wait for a decoded frame and a real checkpoint, so opening a
        # completed video and immediately failing doesn't erase its history.
        # Whole line with awk: the firmware's BusyBox 1.20.2 ignores -x
        # with grep -F (a substring search).
        if [ -f "$FRAME_OK_FILE" ] && [ -r "$COMPLETED_INDEX" ] &&
            awk -v target="$video" '$0 == target { found = 1 } END { exit !found }' \
                "$COMPLETED_INDEX"; then
            set_completed "$video" 0
        fi
        media_duration=0
        if [ -r "$DURRUNTIME" ]; then
            IFS= read -r media_duration < "$DURRUNTIME" || true
            case "$media_duration" in ''|*[!0-9]*) media_duration=0 ;; esac
        fi
        printf '%s\t%s\t%s\n' "$video" "$saved" "$media_duration" >> "$temp_index"
    fi
    mv -f "$temp_index" "$PROGRESS_INDEX" || log 'WARN: resume index update failed'
    fi
    if [ "$PLAY_KIND" = error ]; then
        printf '%s\n' 'Video playback failed' > "$ERROR_FILE"
        [ "$status" -eq 0 ] && status=1
    fi
    rm -f "$POSRUNTIME" "$DURRUNTIME" "$SEEK_END_FILE" "$FRAME_OK_FILE" \
        "$EXIT_FILE" "$BACK_FILE" "$NEXT_FILE" "$NEXT_CANCEL_FILE" "$PREV_FILE"
    # A handled termination returns to the browser as an ordinary Back;
    # do not let its nonzero process status create a playback-error popup.
    if [ "$status" -eq 143 ]; then return 0; fi
    return "$status"
}

if [ ! -x "$PLAYER" ] || [ ! -r "$APP/libs/libminivsync.so" ]; then
    log 'ERROR: missing player or private vsync library.'
    log 'Build the player and vsync shim: cd src && make engine vsync'
    exit 1
fi

# External direct-file launch does not depend on the MiniCinema browser.
# An Onion reboot can supply the originally selected Emu path; use the last
# playing video instead if this is a one-shot controlled boot resume.
if [ "$#" -gt 0 ] && [ "$BOOT_KIND" != browser ]; then
    direct_video=$1
    if [ "$BOOT_KIND" = video ] && remember_resume_enabled &&
        [ -r "$APP/data/.last-file.txt" ]; then
        last_video=
        IFS= read -r last_video < "$APP/data/.last-file.txt" || true
        if [ -f "$last_video" ]; then direct_video=$last_video; fi
    fi
    play_video "$direct_video"
    result=$?
    exit "$result"
fi

if [ ! -x "$BR" ]; then
    log 'ERROR: missing browser executable.'
    exit 1
fi
# A previously shutdown video starts directly at its existing checkpoint.
# Resume=0 deliberately does not reload an old checkpoint.
if [ "$BOOT_KIND" = video ] && remember_resume_enabled &&
    [ -r "$APP/data/.last-file.txt" ]; then
    IFS= read -r boot_video < "$APP/data/.last-file.txt" || true
    if [ -n "$boot_video" ] && [ -f "$boot_video" ]; then
        log "BOOT RESUME: $boot_video"
        play_video "$boot_video" || :
        if [ "$PLAY_KIND" = shutdown ] || [ "$APP_EXIT_REQUESTED" -eq 1 ]; then exit 0; fi
    fi
fi
PLAY_CONTEXT=browser
prepare_display || exit 1
while :; do
    rm -f "$SELECTED"
    log 'BROWSER: starting (Media/Videos)'
    PLAY_CONTEXT=browser
    "$BR" >> "$LOG_DEST" 2>&1 &
    ACTIVE_CHILD=$!
    wait "$ACTIVE_CHILD"
    result=$?
    ACTIVE_CHILD=
    if record_shutdown; then exit 0; fi
    if [ "$result" -ne 0 ]; then
        if [ "$result" -ne 1 ]; then
            log "ERROR: browser returned $result (4=directory, 5=framebuffer, 6=input)"
        fi
        break
    fi
    if [ ! -s "$SELECTED" ]; then
        log 'ERROR: browser returned no selected video.'
        break
    fi
    IFS= read -r selected < "$SELECTED" || true
    case "$selected" in
        "$MEDIA"/*) ;;
        *) log 'ERROR: selected path outside Media/Videos.'; break ;;
    esac
    if [ -f "$selected" ]; then
        SLEEP_SESSION_ACTIVE=0
        current="$selected"
        while [ -f "$current" ]; do
            printf '%s\n' "$current" > "$APP/data/.last-file.txt"
            play_video "$current" || {
                [ -f "$ERROR_FILE" ] || printf '%s\n' 'Video could not be played' > "$ERROR_FILE"
            }
            if [ "$PLAY_KIND" = shutdown ]; then exit 0; fi
            if [ "$APP_EXIT_REQUESTED" -eq 1 ]; then break; fi
            case "$PLAY_KIND" in
                next|previous)
                    neighbor=$(playlist_neighbor "$current" "$PLAY_KIND")
                    if [ -n "$neighbor" ] && [ -f "$neighbor" ]; then
                        current=$neighbor
                        continue
                    fi
                    ;;
            esac
            break
        done
        if [ "$APP_EXIT_REQUESTED" -eq 1 ]; then break; fi
    else
        log 'ERROR: selected video disappeared.'
    fi
    prepare_display || break
done
exit 0
