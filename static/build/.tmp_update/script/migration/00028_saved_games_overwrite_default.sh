#!/bin/sh

# RedOnion's default for Saved Games > "Playing an older save" is Overwrite:
# the chosen save replaces the auto save, with no "Previous Session" left
# behind. Open MainUI uses Backup while game-menu.json has no
# saved_games.resume_mode, so the key is written once here. A resume_mode
# that is already there stays (Tweaks writes every choice, Backup included).
# A file jq cannot read (comments, damage) or a saved_games that is not an
# object is left alone: Tweaks shows Backup then, which is what MainUI uses.

CONFIG=${GAME_MENU_CONFIG:-/mnt/SDCARD/.tmp_update/config/game-menu.json}
JQ=${JQ:-/mnt/SDCARD/.tmp_update/bin/jq}

if [ ! -e "$CONFIG" ]; then
    mkdir -p "$(dirname "$CONFIG")"
    printf '{\n\t"saved_games":\t{\n\t\t"resume_mode":\t"overwrite"\n\t}\n}\n' > "$CONFIG"
    exit 0
fi

"$JQ" -e '(.saved_games == null or (.saved_games | type) == "object")
    and (.saved_games.resume_mode == null)' "$CONFIG" > /dev/null 2>&1 || exit 0

tmp="$CONFIG.tmp"
if "$JQ" --tab '.saved_games.resume_mode = "overwrite"' "$CONFIG" > "$tmp" 2> /dev/null &&
    [ -s "$tmp" ]; then
    mv -f "$tmp" "$CONFIG"
else
    rm -f "$tmp"
fi
