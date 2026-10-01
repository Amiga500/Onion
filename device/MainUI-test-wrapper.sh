#!/bin/sh
# Runs Open MainUI in the Onion MainUI slot, falling back to the stock binary
# if it is missing or a DISABLED file is present. See docs/BUILDING.md.
#
# Output follows Onion's own convention: silent unless
# /mnt/SDCARD/.tmp_update/config/.logging exists, and when it does, appended to
# .tmp_update/logs/ alongside every other Onion log. Onion's runtime.sh writes
# `main 2>&1 > /dev/null`, which redirects stderr to the console rather than
# discarding it; `> /dev/null 2>&1` below is the ordering that silences both.
# This matters here because SDL's blit diagnostics go to stdout.
sysdir=/mnt/SDCARD/.tmp_update
testdir="$sysdir/mainui-test"

# Run stock only when it can actually run; otherwise Open MainUI is better
# than no launcher at all.
if [ -x "$testdir/stock/MainUI" ] &&
    { [ -f "$testdir/DISABLED" ] || [ ! -x "$testdir/MainUI" ]; }; then
    exec "$testdir/stock/MainUI"
fi

export LD_LIBRARY_PATH="$sysdir/lib:/mnt/SDCARD/miyoo/lib:/config/lib:/lib"
cd /mnt/SDCARD/miyoo/app

# exec and the MainUI basename retain Onion keymon's process-name contract.
# Logging is best effort: a full or read-only card starts without a log.
log="$sysdir/logs/MainUI.log"
if [ ! -f "$sysdir/config/.logging" ] || ! mkdir -p "$sysdir/logs" 2> /dev/null; then
    exec "$testdir/MainUI" --sd-root /mnt/SDCARD --device real --handoff-dir /tmp \
        > /dev/null 2>&1
fi

# Keep one previous session log once it reaches 1 MiB. Rotation happens before
# exec so keymon still sees MainUI directly. A single session may exceed 1 MiB.
if [ -f "$log" ]; then
    bytes=$(wc -c 2> /dev/null < "$log")
    if [ "${bytes:-0}" -ge 1048576 ] 2> /dev/null; then
        mv -f "$log" "$log.1" 2> /dev/null
    fi
fi
if { echo "--- MainUI start $(date '+%Y-%m-%d %H:%M:%S') ---" >> "$log"; } 2> /dev/null; then
    exec "$testdir/MainUI" --sd-root /mnt/SDCARD --device real --handoff-dir /tmp \
        >> "$log" 2>&1
fi
exec "$testdir/MainUI" --sd-root /mnt/SDCARD --device real --handoff-dir /tmp \
    > /dev/null 2>&1
