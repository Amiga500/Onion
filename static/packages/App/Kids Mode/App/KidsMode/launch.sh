#!/bin/sh
# Kids Mode arm app — one tap in Onion's Apps tab arms it (a PIN and a
# session timer the first time). RedOnion's runtime.sh then shows the kid
# launcher in place of MainUI. Unlock from inside the kid UI by holding
# SELECT+START for 3 seconds and entering the PIN.

progdir="$(CDPATH= cd -- "$(dirname "$0")" > /dev/null 2>&1 && pwd -P)"
cd "$progdir" || exit 1

# Already armed: runtime.sh starts the launcher when this app ends.
[ -f /mnt/SDCARD/.kidmode ] && exit 0

exec sh ./kid_mode_loop.sh arm
