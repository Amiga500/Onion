#!/bin/sh

# RedOnion now ships console pictures (script/console_pictures.sh) and uses
# them by default in Open MainUI's carousel and list: without a
# console-view.json there is no "thumbs" and the theme's pictures show. The
# carousel gets the look robcodedev recommends for them: no selection box,
# the other consoles at 70% size and 50% opacity, smooth slides. A
# console-view.json that already exists is left alone: its owner chose
# (Tweaks > Console view). The grid keeps the theme's icons either way.

CONFIG=${CONSOLE_VIEW_CONFIG:-/mnt/SDCARD/.tmp_update/config/console-view.json}
THUMBS=${CONSOLE_PICTURES_DEST:-/mnt/SDCARD/miyoo/app/consoles}

[ -e "$CONFIG" ] && exit 0
[ -f "$THUMBS/_default.png" ] || exit 0
printf '{\n\t"thumbs":\t"%s",\n\t"carousel":\t{\n\t\t"selection":\tfalse,\n\t\t"scale_unselected":\t0.7,\n\t\t"opacity_unselected":\t0.5,\n\t\t"easing":\t"smooth"\n\t}\n}\n' "$THUMBS" > "$CONFIG"
