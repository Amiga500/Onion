#!/bin/sh
# RedAmp for RedOnion / Miyoo. Only the user's explicit opt-in may install
# the game-music audio wrapper. This launcher never modifies miyoo/lib.
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR" || exit 1
mkdir -p "$DIR/data" || exit 1
LOG="$DIR/log.txt"
[ -f "$LOG" ] && mv "$LOG" "$DIR/log.prev.txt"
echo "===== RedAmp $(cat "$DIR/VERSION" 2>/dev/null) $(date)" > "$LOG"

# Restore brightness left by a crash or a forcibly killed player.
"$DIR/redamp-recover.sh" --backlight-only >> "$LOG" 2>&1
trap '"$DIR/redamp-recover.sh" --backlight-only >> "$LOG" 2>&1' 0

ONION_LIB=/mnt/SDCARD/miyoo/lib
is_wrapper() { [ -f "$1" ] && grep -q MINIAMP_PADSP_WRAPPER "$1" 2>/dev/null; }
# The player's own audio must always use the original OnionOS backend,
# rather than mixing the same track into itself.
LD_PRELOAD=
for p in "$ONION_LIB/libpadsp_orig.so" "$ONION_LIB/libpadsp.so" "$DIR/data/libpadsp_onion.so" \
         /mnt/SDCARD/.tmp_update/lib/libpadsp.so /customer/lib/libpadsp.so; do
  if [ -f "$p" ] && ! is_wrapper "$p"; then LD_PRELOAD="$p"; break; fi
done
[ -n "$LD_PRELOAD" ] && export LD_PRELOAD
export LD_LIBRARY_PATH="$DIR/lib:/config/lib:/customer/lib:$LD_LIBRARY_PATH"
export MA_HOME="$DIR"
export MA_VERSION="$(cat "$DIR/VERSION" 2>/dev/null)"
# For optional performance diagnostics, set MA_PROFILE=1 outside this script.
echo "audio: LD_PRELOAD=${LD_PRELOAD:-none}" >> "$LOG"
"$DIR/redamp" >> "$LOG" 2>&1
rc=$?
"$DIR/redamp" --cleanup-stay-awake >> "$LOG" 2>&1 || true
echo "redamp exited with code $rc" >> "$LOG"
exit "$rc"
