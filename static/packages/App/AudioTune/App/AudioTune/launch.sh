#!/bin/sh
# AudioTune beta — app, audio restore and optional runtime boot integration.
# Firmware-specific native VQE is never used without the verified MI_AO ABI.
APP_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd) || exit 1
PROFILE=/mnt/SDCARD/.tmp_update/config/audiotune.ini
HOOK="$APP_DIR/libaudiotune_vqe.so"
AO_LIB=/config/lib/libmi_ao.so
AO_SHA256=696a7270facf93987800ba7a340a4160ed11da0d7805d5b30bfac8ce44311212
AUDIO_BIN=/mnt/SDCARD/miyoo/app/audioserver
# The audio server's libraries. Stock: exactly RedOnion runtime.sh's order,
# so a server restarted here loads what runtime.sh would. Hooked: the same
# with /config/lib first, as the MI_AO library whose ABI is verified must be
# the one loaded. The UI (audiotune) keeps LD_LIBRARY_PATH below.
STOCK_LIBS="/lib:/config/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/.tmp_update/lib/parasyte"
HOOK_LIBS="/config/lib:/lib:/mnt/SDCARD/miyoo/lib:/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/.tmp_update/lib/parasyte"
# For the UI (audiotune) and the tools run here; the server gets its own above.
export LD_LIBRARY_PATH="/config/lib:/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/miyoo/lib:/customer/lib:/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
SERVER_BIN="$AUDIO_BIN"
SERVER_ARG=
SERVER_PID=
PROFILE_HASH=
HOOK_HASH=
HOOK_PID=
DEBUG=${AUDIOTUNE_DEBUG:-0}
log() { [ "$DEBUG" = 1 ] && printf 'AudioTune: %s\n' "$*" >&3 || :; }
# Debugging is explicitly opt-in, RAM-only, and disabled for beta users.
if [ "$DEBUG" = 1 ]; then exec 3>>/tmp/audiotune-beta.log;
else exec 3>/dev/null; fi
sha() { sha256sum "$1" 2>/dev/null | sed -n '1s/ .*//p'; }
live_server_pid() {
    # A SIGKILLed audioserver may remain a zombie until its parent reaps it.
    # A zombie cannot play audio and must not block the stock fallback.
    [ -r "/proc/$1/stat" ] || return 1
    read -r _spid _scomm _sstate _srest < "/proc/$1/stat" || return 1
    [ "$_sstate" != Z ] && [ "$_sstate" != X ]
}
server_running() {
    for _running_pid in $(pidof audioserver 2>/dev/null); do
        if live_server_pid "$_running_pid"; then return 0; fi
    done
    return 1
}
read_server() {
    # Never alter the runtime's audio process unless it is unique and known.
    set --
    for _candidate in $(pidof audioserver 2>/dev/null); do
        if live_server_pid "$_candidate"; then set -- "$@" "$_candidate"; fi
    done
    [ "$#" -eq 1 ] && [ -r "/proc/$1/cmdline" ] || return 1
    SERVER_PID=$1
    SERVER_BIN=$(tr '\000' '\n' < "/proc/$SERVER_PID/cmdline" | sed -n '1p')
    SERVER_ARG=$(tr '\000' '\n' < "/proc/$SERVER_PID/cmdline" | sed -n '2p')
    count=$(tr '\000' '\n' < "/proc/$SERVER_PID/cmdline" | wc -l | tr -d ' ')
    [ "$SERVER_BIN" = "$AUDIO_BIN" ] && [ -x "$SERVER_BIN" ] || return 1
    [ "$count" = 1 ] || [ "$count" = 2 ] || return 1
    case "$SERVER_ARG" in ''|-*|[0-9]*) ;; *) return 1 ;; esac
    if [ -n "$SERVER_ARG" ]; then
        echo "$SERVER_ARG" | grep -Eq '^[-]?[0-9]+$' || return 1
    fi
}
is_hooked() {
    [ -n "$SERVER_PID" ] || return 1
    [ -r "/proc/$SERVER_PID/maps" ] || return 1
    # Both the preload and the exact library whose ABI was verified must be
    # loaded. Environment variables alone do not prove native EQ is safe.
    awk -v hook="$HOOK" -v ao="$AO_LIB" '
        ($NF==hook || ($(NF-1)==hook && $NF=="(deleted)")) {h=1}
        ($NF==ao) {a=1}
        END {exit !(h && a)}' "/proc/$SERVER_PID/maps" 2>/dev/null
}
env_value() {
    [ -r "/proc/$SERVER_PID/environ" ] || return 0
    tr '\000' '\n' < "/proc/$SERVER_PID/environ" | sed -n "s/^$1=//p" | head -n 1
}
profile_valid() {
    [ -r "$PROFILE" ] && [ -f "$HOOK" ] && [ -r "$AO_LIB" ] || return 1
    grep -q '^eq_enabled=1$' "$PROFILE" &&
    grep -Eq '^preset=[1-6]$' "$PROFILE" || return 1
    # A saved Custom profile with five zero gains is effectively Flat.
    # Do this before hashing/loading the hook: --boot-audio must leave the
    # stock server alone, and normal exit must restore stock if it is hooked.
    if grep -q '^preset=6$' "$PROFILE"; then
        awk '
            /^custom=/ {
                sub(/^custom=/, "")
                gsub(/^[[:space:]]+|[[:space:]]+$/, "")
                n=split($0, gain, /[[:space:]]+/)
                seen=1; valid=(n==5); nonzero=0
                if (!valid) next
                for (i=1; i<=5; i++) {
                    if (gain[i] !~ /^[+-]?[0-9]+$/) valid=0
                    if (gain[i]+0 != 0) nonzero=1
                }
            }
            END { exit !(seen && valid && nonzero) }
        ' "$PROFILE" || return 1
    fi
    command -v sha256sum >/dev/null 2>&1 || return 1
    [ "$(sha "$AO_LIB")" = "$AO_SHA256" ] || { log 'Unsupported MI_AO firmware: leaving stock audio untouched.'; return 1; }
    PROFILE_HASH=$(sha "$PROFILE")
    HOOK_HASH=$(sha "$HOOK")
    [ -n "$PROFILE_HASH" ] && [ -n "$HOOK_HASH" ]
}
stop_server() {
    pid=$1
    [ -n "$pid" ] || return 1
    # Stop only the uniquely verified audioserver process. If it ignores
    # SIGTERM, force it down before attempting a replacement; never leave
    # a delayed termination that can strand the device without sound.
    kill -TERM "$pid" 2>/dev/null || { ! server_running; return $?; }
    n=0
    while [ "$n" -lt 30 ]; do
        if ! server_running; then return 0; fi
        sleep 0.1
        n=$((n+1))
    done
    log "audioserver $pid did not stop after SIGTERM; escalating to SIGKILL"
    if [ -r "/proc/$pid/comm" ]; then
        read -r current_comm < "/proc/$pid/comm"
        case "$current_comm" in *audioserver*) ;; *) return 1 ;; esac
    fi
    kill -KILL "$pid" 2>/dev/null || { ! server_running; return $?; }
    n=0
    while [ "$n" -lt 15 ]; do
        if ! server_running; then return 0; fi
        sleep 0.1
        n=$((n+1))
    done
    log 'audioserver did not stop even after SIGKILL; declining duplicate start'
    return 1
}
start_server() {
    mode=$1
    [ -x "$SERVER_BIN" ] || return 1
    if [ "$mode" = hooked ]; then
        # Only this process receives LD_PRELOAD: never preload into sh or runtime utilities.
        if [ -n "$SERVER_ARG" ]; then
            AUDIOTUNE_NATIVE_VQE=1 AUDIOTUNE_VQE_PROFILE="$PROFILE" \
            AUDIOTUNE_VQE_PROFILE_SHA256="$PROFILE_HASH" \
            AUDIOTUNE_VQE_HOOK_SHA256="$HOOK_HASH" LD_PRELOAD="$HOOK" LD_LIBRARY_PATH="$HOOK_LIBS" \
                "$SERVER_BIN" "$SERVER_ARG" >&3 2>&3 &
        else
            AUDIOTUNE_NATIVE_VQE=1 AUDIOTUNE_VQE_PROFILE="$PROFILE" \
            AUDIOTUNE_VQE_PROFILE_SHA256="$PROFILE_HASH" \
            AUDIOTUNE_VQE_HOOK_SHA256="$HOOK_HASH" LD_PRELOAD="$HOOK" LD_LIBRARY_PATH="$HOOK_LIBS" \
                "$SERVER_BIN" >&3 2>&3 &
        fi
    else
        if [ -n "$SERVER_ARG" ]; then
            LD_PRELOAD= LD_LIBRARY_PATH="$STOCK_LIBS" AUDIOTUNE_NATIVE_VQE=0 "$SERVER_BIN" "$SERVER_ARG" >&3 2>&3 &
        else
            LD_PRELOAD= LD_LIBRARY_PATH="$STOCK_LIBS" AUDIOTUNE_NATIVE_VQE=0 "$SERVER_BIN" >&3 2>&3 &
        fi
    fi
    spawned=$!
    n=0
    while [ "$n" -lt 12 ]; do
        if read_server; then
            if [ "$mode" != hooked ]; then return 0; fi
            if is_hooked; then HOOK_PID=$SERVER_PID; return 0; fi
            # The server may still be loading its SigmaStar dependencies.
        fi
        sleep 0.1
        n=$((n+1))
    done
    log 'Server start failed identity/library validation.'
    # Stop only the process we launched, never an unrelated replacement.
    if read_server && [ "$SERVER_PID" = "$spawned" ]; then
        stop_server "$SERVER_PID" || :
    fi
    return 1
}
# Determine whether switching to/from the native server is necessary.
needs_sync() {
    valid=0
    profile_valid && valid=1
    read_server || { log 'Audio server not uniquely identified; skipping EQ changes.'; return 1; }
    already=0
    is_hooked && already=1
    if [ "$already" = 0 ]; then
        existing=$(env_value LD_PRELOAD)
        [ -z "$existing" ] || { log 'Foreign server preload in use; not replacing.'; return 1; }
    fi
    if [ "$valid" = 0 ]; then [ "$already" = 1 ] && return 0 || return 1; fi
    if [ "$already" = 0 ]; then return 0; fi
    oldprofile=$(env_value AUDIOTUNE_VQE_PROFILE_SHA256)
    oldhook=$(env_value AUDIOTUNE_VQE_HOOK_SHA256)
    [ "$oldprofile" = "$PROFILE_HASH" ] && [ "$oldhook" = "$HOOK_HASH" ] && return 1
    return 0
}
sync_server() {
    if ! needs_sync; then return 0; fi
    valid=0
    profile_valid && valid=1
    read_server || return 1
    old_pid=$SERVER_PID
    old_bin=$SERVER_BIN
    old_arg=$SERVER_ARG
    if ! stop_server "$old_pid"; then
        log 'Audio server stop failed; recovering stock if old process is gone.'
        if ! server_running; then
            SERVER_BIN=$old_bin; SERVER_ARG=$old_arg
            start_server stock || :
        fi
        return 1
    fi
    SERVER_BIN=$old_bin
    SERVER_ARG=$old_arg
    if [ "$valid" = 1 ]; then
        if start_server hooked; then log 'Native EQ server started.'; return 0; fi
        if server_running; then log 'Unexpected audio server after failed hook; no duplicate started.'; return 1; fi
        SERVER_BIN=$old_bin; SERVER_ARG=$old_arg
        log 'Hook start failed; restoring stock audio.'
    fi
    start_server stock || { log 'Stock restore failed; reboot to recover audio.'; return 1; }
}
restore_stock() {
    read_server || { log 'No single running server to restore.'; return 1; }
    if ! is_hooked; then log 'Stock audio already running.'; return 0; fi
    old_pid=$SERVER_PID; old_bin=$SERVER_BIN; old_arg=$SERVER_ARG
    if ! stop_server "$old_pid"; then
        if ! server_running; then
            SERVER_BIN=$old_bin; SERVER_ARG=$old_arg
            start_server stock || :
        fi
        return 1
    fi
    SERVER_BIN=$old_bin; SERVER_ARG=$old_arg
    start_server stock
}
boot_audio() {
    # Invoked from runtime.sh only when audioserver is absent.
    [ "$#" -eq 1 ] && printf '%s' "$1" | grep -Eq '^[-]?[0-9]+$' || return 1
    server_running && return 0
    profile_valid || return 1
    SERVER_BIN=$AUDIO_BIN; SERVER_ARG=$1
    start_server hooked
}
case "${1:-}" in
    --boot-audio) shift; boot_audio "$@"; exit $? ;;
    --restore) restore_stock; exit $? ;;
    --status)
        if read_server && is_hooked; then echo "AudioTune VQE hook loaded (PID $SERVER_PID)";
        elif server_running; then echo 'Stock/other audio server running';
        else echo 'No audio server running'; fi
        exit 0 ;;
    '') ;;
    *) echo 'Usage: launch.sh [--boot-audio VOLUME|--restore|--status]' >&2; exit 2 ;;
esac
cd "$APP_DIR" || exit 1
[ -x ./audiotune ] || { echo 'AudioTune binary not installed.' >&2; exit 127; }
# Use short-lived IPC files, never persistent state/log files on the SD card.
IPC="/tmp/audiotune-$$"
REQUEST="$IPC.request"; DECISION="$IPC.decision"; READY="$IPC.ready"; DONE="$IPC.done"
STATE="$IPC.state"
cleanup() { rm -f "$REQUEST" "$DECISION" "$READY" "$DONE" "$STATE"; }
trap 'cleanup' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP
if read_server && is_hooked; then echo TEST_ACTIVE > "$STATE"; else echo STOCK > "$STATE"; fi
BRIDGE=
if [ -f /mnt/SDCARD/miyoo/lib/libpadsp.so ]; then BRIDGE=/mnt/SDCARD/miyoo/lib/libpadsp.so
elif [ -f /customer/lib/libpadsp.so ]; then BRIDGE=/customer/lib/libpadsp.so
else export AUDIOTUNE_NO_AUDIO=1; fi
export AUDIOTUNE_VQE_STATE="$STATE"
export AUDIOTUNE_APPLY_REQUEST="$REQUEST"
export AUDIOTUNE_APPLY_DECISION="$DECISION"
export AUDIOTUNE_APPLY_READY="$READY"
export AUDIOTUNE_APPLY_DONE="$DONE"
if [ -n "$BRIDGE" ]; then
    LD_PRELOAD="$BRIDGE" ./audiotune >&3 2>&3 &
else
    LD_PRELOAD= ./audiotune >&3 2>&3 &
fi
APP_PID=$!
app_alive() {
    kill -0 "$APP_PID" 2>/dev/null || return 1
    [ -r "/proc/$APP_PID/status" ] && grep -q '^State:[[:space:]]*Z' "/proc/$APP_PID/status" && return 1
    return 0
}
completed=0
count=0
while app_alive; do
    if [ -f "$REQUEST" ] && [ "$completed" = 0 ]; then
        completed=1
        rm -f "$REQUEST"
        if needs_sync; then
            echo APPLY > "$DECISION"
            n=0
            while app_alive && [ ! -f "$READY" ] && [ "$n" -lt 30 ]; do
                sleep 0.05
                n=$((n+1))
            done
            # The UI has now displayed the black audio-update screen.
            if [ -f "$READY" ]; then sync_server || :; fi
        else
            echo SKIP > "$DECISION"
        fi
        echo DONE > "$DONE"
    fi
    # Re-establish stock sound after a hooked-server crash while app is open.
    count=$((count+1))
    if [ $((count % 10)) -eq 0 ] && [ -n "$HOOK_PID" ] && ! kill -0 "$HOOK_PID" 2>/dev/null; then
        if ! server_running; then
            SERVER_BIN=$AUDIO_BIN
            [ -n "$SERVER_ARG" ] && start_server stock || :
        fi
        HOOK_PID=
    fi
    sleep 0.1
done
wait "$APP_PID"
rc=$?
# A crash before the request is never permission to restart the audio server.
exit "$rc"
