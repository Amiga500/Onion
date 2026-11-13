#!/bin/sh
# Kids Mode: the play time used so far, saved to the SD card at a shutdown.
# Copied to .tmp_update/checkoff/ while Kids Mode is armed; runtime.sh runs it
# before shutting down. (The timer counts in /tmp and saves once a minute.)

[ -f /mnt/SDCARD/.kidmode ] || exit 0
used=$(cat /tmp/kidsmode_timer_used 2> /dev/null)
case "$used" in '' | *[!0-9]*) exit 0 ;; esac

state=/mnt/SDCARD/Saves/KidsMode/timer_state.txt
bonus=$(sed -n 3p "$state" 2> /dev/null)
case "$bonus" in '' | *[!0-9]*) bonus=0 ;; esac
printf '%s\n%s\n%s\n' "$(date +%Y-%m-%d)" "$used" "$bonus" > "$state.tmp" &&
    mv -f "$state.tmp" "$state"
