#!/bin/sh
# SD Card Doctor v0.6.3 - run from the same directory as config.json
APP_DIR="$(CDPATH= cd "$(dirname "$0")" && pwd)" || exit 1
cd "$APP_DIR" || exit 1

# Use firmware-provided SDL and C libraries; do not bundle system libraries.
export LD_LIBRARY_PATH="/mnt/SDCARD/.tmp_update/lib:/customer/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Startup/scan errors remain in RAM, so diagnostics do not write a log to a
# potentially failing card. Copy /tmp/sdcarddoctor.log off-device if needed.
LOG=/tmp/sdcarddoctor.log
{
    echo "========== SD Card Doctor v0.6.3 =========="
    date 2>/dev/null
    echo "appdir=$APP_DIR"
    echo "--- app output ---"
} >"$LOG" 2>&1

if [ ! -x ./sdcarddoctor ]; then
    echo "sdcarddoctor executable missing or not executable" >>"$LOG"
    exit 127
fi
./sdcarddoctor "$@" >>"$LOG" 2>&1
code=$?
printf '%s\n' "--- exit status: $code ---" >>"$LOG" 2>&1
exit "$code"
