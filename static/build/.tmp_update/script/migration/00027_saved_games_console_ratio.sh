#!/bin/sh

# Open MainUI 1.1.0's Saved Games shows save pictures with the console's
# screen shape ("console ratio", game-menu.json saved_games.
# correct_aspect_ratio): NES and the other 4:3 consoles no longer look
# squashed. Open MainUI turns it off by default; RedOnion turns it on, as
# robcodedev suggests (the GameSwitcher's Load preview does the same).
# A choice already in game-menu.json is kept, and a file jq can't read (for
# example with comments) is left alone. Tweaks > Main menu > Game list
# menus > "Saved Games: console ratio" changes it.

CONFIG=${GAME_MENU_CONFIG:-/mnt/SDCARD/.tmp_update/config/game-menu.json}

if [ ! -e "$CONFIG" ]; then
    printf '{\n\t"saved_games":\t{\n\t\t"correct_aspect_ratio":\ttrue\n\t}\n}\n' > "$CONFIG"
    exit 0
fi

command -v jq > /dev/null 2>&1 || exit 0
jq -e 'type == "object" and ((.saved_games // {}) | type == "object")' "$CONFIG" > /dev/null 2>&1 || exit 0
jq -e '.saved_games.correct_aspect_ratio != null' "$CONFIG" > /dev/null 2>&1 && exit 0

tmp="$CONFIG.tmp"
if jq '.saved_games.correct_aspect_ratio = true' "$CONFIG" > "$tmp" 2> /dev/null && [ -s "$tmp" ]; then
    mv -f "$tmp" "$CONFIG"
else
    rm -f "$tmp"
fi
