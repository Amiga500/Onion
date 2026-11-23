#!/bin/sh

# Game Info auto scroll on by default (machiavallian, Rob): a long
# description scrolls by itself instead of being cut off at the bottom of
# the window. Open MainUI scrolls it while .gameInfoAutoScroll exists, and
# Tweaks removes the file for Off, so it cannot be shipped in configs.pak:
# every update adds a missing file back, and Off would turn on again.
# A migration runs once per SD card: new installs and the update to b5 get
# it on (2000 ms, 20 px/s, Open MainUI's own values), and Off chosen later
# in Tweaks stays off. A file already there (the user's delay and speed)
# is kept.

CONFIG=${GAME_INFO_SCROLL_CONFIG:-/mnt/SDCARD/.tmp_update/config/.gameInfoAutoScroll}

[ -e "$CONFIG" ] && exit 0
mkdir -p "$(dirname "$CONFIG")"
printf '2000,20\n' > "$CONFIG"
