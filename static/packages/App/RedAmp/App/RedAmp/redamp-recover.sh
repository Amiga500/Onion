#!/bin/sh
# Standalone emergency recovery: can run EVEN IF the RedAmp binary is broken.
# Run: /mnt/SDCARD/App/RedAmp/redamp-recover.sh
# For safe launcher use: redamp-recover.sh --backlight-only
DIR="$(cd "$(dirname "$0")" && pwd)" || exit 1
BACKLIGHT=/sys/class/pwm/pwmchip0/pwm0/duty_cycle
MARKER="$DIR/data/backlight.saved"

restore_brightness() {
  [ -f "$MARKER" ] || return 0
  IFS= read -r level < "$MARKER" || level=50
  case "$level" in
    ''|*[!0-9]*) level=50 ;;
  esac
  [ "$level" -ge 1 ] 2>/dev/null && [ "$level" -le 1000000 ] 2>/dev/null || level=50
  if [ -w "$BACKLIGHT" ] && printf '%s' "$level" > "$BACKLIGHT"; then
    rm -f "$MARKER"
    echo "RedAmp: backlight restored to $level"
    return 0
  fi
  echo "RedAmp: could not restore backlight; marker retained" >&2
  return 1
}

restore_brightness || exit 1
[ "${1:-}" = --backlight-only ] && exit 0

# Preferred path: the player's own validated, serialized restoration routine.
if [ -x "$DIR/redamp" ]; then
  export MA_HOME="$DIR"
  export LD_LIBRARY_PATH="$DIR/lib:/config/lib:/customer/lib:$LD_LIBRARY_PATH"
  if "$DIR/redamp" --uninstall; then
    rm -f "$DIR/data/game_music.consent"
    echo "RedAmp: system audio library verified/restored"
    exit 0
  fi
fi

# Last resort when the executable is missing/broken. Do NOT replace the
# library while a service may still be running; require manual intervention.
PIDF="$DIR/data/service.pid"
if [ -f "$PIDF" ]; then
  IFS= read -r pid < "$PIDF" || pid=
  case "$pid" in ''|*[!0-9]*) pid= ;; esac
  if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
    echo "RedAmp: a background service is still alive; cannot safely recover" >&2
    exit 1
  fi
fi
# Never delete either backup until verified. Never replace a valid new OnionOS library.
LIB=${MA_LIBDIR:-/mnt/SDCARD/miyoo/lib}
CUR="$LIB/libpadsp.so"
ORIG="$LIB/libpadsp_orig.so"
SAVED="$DIR/data/libpadsp_onion.so"
is_wrapper() { [ -f "$1" ] && grep -q MINIAMP_PADSP_WRAPPER "$1" 2>/dev/null; }
is_elf() {
  # Recovery runs on ARM Miyoo only: reject PC or otherwise incompatible
  # libraries. ET_DYN=03 00, EM_ARM=28 00 in little-endian ELF32.
  [ -f "$1" ] || return 1
  size="$(wc -c < "$1" 2>/dev/null)" || return 1
  [ "$size" -ge 64 ] 2>/dev/null || return 1
  magic="$(od -An -tx1 -N6 "$1" 2>/dev/null | tr -d ' \n')"
  header="$(od -An -tx1 -j16 -N4 "$1" 2>/dev/null | tr -d ' \n')"
  [ "$magic" = 7f454c460101 ] && [ "$header" = 03002800 ]
}
if is_elf "$CUR" && ! is_wrapper "$CUR"; then
  rm -f "$DIR/data/game_music.consent"
  echo "RedAmp: original OnionOS audio library already present"
  exit 0
fi
source=
if is_elf "$ORIG" && ! is_wrapper "$ORIG"; then source="$ORIG"
elif is_elf "$SAVED" && ! is_wrapper "$SAVED"; then source="$SAVED"
fi
if [ -z "$source" ]; then
  echo "RedAmp: NO VERIFIED BACKUP. Restore libpadsp.so from your RedOnion installation." >&2
  exit 1
fi
TMP="$CUR.redamp-recovery.$$"
trap 'rm -f "$TMP"' 0 1 2 3 15
if cp "$source" "$TMP" && cmp -s "$source" "$TMP" && mv -f "$TMP" "$CUR" && \
   cmp -s "$source" "$CUR" && is_elf "$CUR" && ! is_wrapper "$CUR"; then
  rm -f "$DIR/data/game_music.consent"
  echo "RedAmp: verified original audio library restored from $source"
  exit 0
fi
echo "RedAmp: recovery FAILED. Original backups were preserved." >&2
exit 1
