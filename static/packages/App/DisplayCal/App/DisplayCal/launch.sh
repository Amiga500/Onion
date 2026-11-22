#!/bin/sh
# DisplayCal v0.8.4: single launcher; --apply uses shell built-ins only.
# GUI logging is opt-in: DISPLAYCAL_DEBUG=1 ./launch.sh
# RedOnion may call --apply at boot and after Blue Light Filter is disabled.
# DisplayCal never patches RedOnion runtime/Blue Light scripts.
set -eu

CFGDIR=${DISPLAYCAL_CFGDIR:-/mnt/SDCARD/.tmp_update/config/display}
NODE=${DISPLAYCAL_NODE:-/proc/mi_modules/mi_disp/mi_disp0}
SERIAL_FILE=${DISPLAYCAL_SERIAL_FILE:-/tmp/deviceSN}
BLF_FLAG=${DISPLAYCAL_BLF_FLAG:-/tmp/.blfOn}

valid_serial() {
    case "${1-}" in ''|*[!0-9a-fA-F]*) return 1 ;; esac
    [ "${#1}" -le 64 ] || return 1
    case "$1" in *[1-9a-fA-F]*) return 0 ;; *) return 1 ;; esac
}

# Matches modern Onion/RedOnion: environment variable, then /tmp/deviceSN.
# Both are read without launching read_uuid, cat, sed, tr, etc.
get_id() {
    PROFILE=${SERIAL_NUMBER:-}
    if ! valid_serial "$PROFILE"; then
        PROFILE=''
        if [ -r "$SERIAL_FILE" ]; then
            IFS= read -r PROFILE < "$SERIAL_FILE" || :
        fi
    fi
    if ! valid_serial "$PROFILE"; then PROFILE=default; fi
}

valid_channel() {
    case "${1-}" in
        0|[1-9]|[1-9][0-9]|1[0-9][0-9]|2[0-4][0-9]|25[0-5]) return 0 ;;
        *) return 1 ;;
    esac
}
valid_warmth() {
    N=${1-}
    case "$N" in -*) N=${N#-} ;; +*) N=${N#+} ;; esac
    case "$N" in 0|[1-9]|[1-9][0-9]|100) return 0 ;; *) return 1 ;; esac
}

write_rgb() {
    valid_channel "$1" && valid_channel "$2" && valid_channel "$3" || {
        printf 'DisplayCal: invalid RGB value (expected 0..255)\n' >&2; return 2;
    }
    [ -e "$NODE" ] || { printf 'DisplayCal: display node missing: %s\n' "$NODE" >&2; return 3; }
    # SigmaStar proc command expects B G R, not R G B.
    printf 'colortemp 0 0 0 0 %s %s %s\n' "$3" "$2" "$1" > "$NODE"
}

with_warmth() {
    valid_channel "$1" && valid_channel "$2" && valid_channel "$3" && valid_warmth "${4-0}" || {
        printf 'DisplayCal: invalid RGB/warmth value\n' >&2; return 2;
    }
    # Canonical integer values from the app's saved config; no shell eval/source.
    R=$1 G=$2 B=$3 W=${4-0}
    if [ "$W" -gt 0 ]; then
        G=$((G - (W * 28 + 50) / 100))
        B=$((B - (W * 56 + 50) / 100))
    elif [ "$W" -lt 0 ]; then
        COOL=$((-W))
        R=$((R - (COOL * 56 + 50) / 100))
        G=$((G - (COOL * 28 + 50) / 100))
    fi
    [ "$R" -ge 0 ] || R=0
    [ "$G" -ge 0 ] || G=0
    [ "$B" -ge 0 ] || B=0
    # Safety limit applies to EFFECTIVE channels, after warmth attenuation.
    # Individual channels may still be zero. Never write a total under 70.
    if [ $((R + G + B)) -lt 70 ]; then
        printf 'DisplayCal: RGB TOO DARK (minimum effective total: 70)\n' >&2
        return 2
    fi
    write_rgb "$R" "$G" "$B"
}

apply_saved() {
    CFG="$CFGDIR/cal-$PROFILE.cfg"
    # Unconfigured devices should not slow boot or alter the panel.
    [ -e "$CFG" ] || return 0
    [ -r "$CFG" ] || { printf 'DisplayCal: cannot read %s\n' "$CFG" >&2; return 2; }
    RR='' GG='' BB='' WW=0
    # Pure ash/dash built-ins. Unlike sed/head x4, this creates NO subprocesses.
    while IFS= read -r LINE || [ -n "$LINE" ]; do
        # BusyBox ash/POSIX-shell only: Windows CRLF (or an ending control byte)
        # does not make an otherwise valid config value fail validation.
        case "$LINE" in *[[:cntrl:]]) LINE=${LINE%?} ;; esac
        case "$LINE" in
            R=*) RR=${LINE#R=} ;;
            G=*) GG=${LINE#G=} ;;
            B=*) BB=${LINE#B=} ;;
            W=*) WW=${LINE#W=} ;;
        esac
    done < "$CFG"
    # A neutral profile explicitly disables calibration. Avoid an unnecessary
    # boot-time write that might override another display component's state.
    if valid_channel "$RR" && valid_channel "$GG" && valid_channel "$BB" && valid_warmth "$WW"; then
        if [ "$RR" -eq 128 ] && [ "$GG" -eq 128 ] && [ "$BB" -eq 128 ] && [ "$WW" -eq 0 ]; then
            return 0
        fi
    fi
    with_warmth "$RR" "$GG" "$BB" "$WW"
}

# IMPORTANT: --apply dispatch occurs before resolving app path, setting up GUI,
# mkdir, logging, library paths, or spawning any external programs.
if [ "${1-}" = --apply ]; then
    [ "$#" -eq 1 ] || { printf 'Usage: launch.sh --apply\n' >&2; exit 2; }
    get_id
    # Blue Light Filter owns colortemp while active. Do not overwrite it.
    # The filter may start *after* this boot hook; that later write still wins.
    [ -e "$BLF_FLAG" ] && exit 0
    # Never apply a shared default profile to an unidentified device at startup.
    [ "$PROFILE" != default ] || exit 0
    apply_saved
    exit $?
fi

# RedOnion owns DISP recovery. Runs RedOnion's disp_csc_reset.sh --repair
# only if the CSC section is missing. The repair script uses the proven
# l/disp_init sequence; no repair or SDK reinitialization happens in the GUI.
# $1 = "before" or "after" (used only in messages). Never fails the launcher.
repair_if_needed() {
    grep -q CscMatrix "$NODE" 2>/dev/null && return 0
    REPAIR_SCRIPT=${DISPLAYCAL_REPAIR_SCRIPT:-}
    if [ -z "$REPAIR_SCRIPT" ]; then
        for CANDIDATE in /mnt/SDCARD/.tmp_update/script/disp_csc_reset.sh \
                         /mnt/SDCARD/.tmp_update/bin/disp_csc_reset.sh; do
            if [ -f "$CANDIDATE" ]; then
                REPAIR_SCRIPT=$CANDIDATE
                break
            fi
        done
        if [ -z "$REPAIR_SCRIPT" ]; then
            REPAIR_SCRIPT=$(command -v disp_csc_reset.sh 2>/dev/null || :)
        fi
    fi
    if [ -n "$REPAIR_SCRIPT" ] && [ -f "$REPAIR_SCRIPT" ]; then
        # Matches the uploaded RedOnion script's --repair path.
        # Nonzero result is reported, never hidden by set -e.
        if ! sh "$REPAIR_SCRIPT" --repair; then
            printf 'DisplayCal: DISP repair %s GUI failed\n' "$1" >&2
        fi
    else
        printf 'DisplayCal: CSC missing %s GUI, disp_csc_reset.sh not installed\n' "$1" >&2
    fi
    return 0
}

case "${1-}" in
    --neutral)
        write_rgb 128 128 128
        printf 'DisplayCal: neutral R=128 G=128 B=128\n' ;;
    --set)
        { [ "$#" -eq 4 ] || [ "$#" -eq 5 ]; } || {
            printf 'Usage: launch.sh --set R G B [WARMTH]\n' >&2; exit 2;
        }
        with_warmth "$2" "$3" "$4" "${5-0}"
        printf 'DisplayCal: applied RGB/warmth\n' ;;
    --serial|--config|'')
        get_id
        case "${1-}" in
            --serial)
                [ "$PROFILE" != default ] || {
                    printf 'DisplayCal: no valid SERIAL_NUMBER or /tmp/deviceSN\n' >&2; exit 1;
                }
                printf '%s\n' "$PROFILE" ;;
            --config)
                printf '%s/cal-%s.cfg\n' "$CFGDIR" "$PROFILE" ;;
            '')
                mkdir -p "$CFGDIR"
                printf '%s\n' "$PROFILE" > /tmp/displaycal-device-id
                DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
                export LD_LIBRARY_PATH="/mnt/SDCARD/.tmp_update/lib:/config/lib:/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

                # Repair BEFORE displaycal-gui opens the framebuffer...
                repair_if_needed before

                cd "$DIR"
                # No exec: the shell must stay alive to run the post-exit check.
                # The GUI's exit code is kept and returned at the end.
                GUI_RC=0
                if [ "${DISPLAYCAL_DEBUG:-0}" = 1 ]; then
                    LOGDIR=${DISPLAYCAL_LOGDIR:-/mnt/SDCARD/.tmp_update/logs}
                    mkdir -p "$LOGDIR"
                    ./displaycal-gui >> "$LOGDIR/displaycal.log" 2>&1 || GUI_RC=$?
                else
                    ./displaycal-gui || GUI_RC=$?
                fi

                # ...and AFTER it exits (Save, Cancel or crash), in case the
                # Display page's SDK calls left the CSC section missing.
                repair_if_needed after
                exit "$GUI_RC" ;;
        esac ;;
    *)
        printf 'Usage: launch.sh [--apply|--serial|--config|--neutral|--set R G B [WARMTH]]\n' >&2
        exit 2 ;;
esac
