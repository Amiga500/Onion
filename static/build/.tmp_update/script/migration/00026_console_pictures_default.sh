#!/bin/sh

# RedOnion now ships console pictures (script/console_pictures.sh) and uses
# them by default in Open MainUI's carousel and list: without a
# console-view.json there is no "thumbs" and the theme's pictures show. A
# console-view.json that already exists is left alone, "thumbs" or not: its
# owner chose (Tweaks > Console view > Console pictures turns them on). The
# grid keeps the theme's icons either way.

CONFIG=${CONSOLE_VIEW_CONFIG:-/mnt/SDCARD/.tmp_update/config/console-view.json}
THUMBS=${CONSOLE_PICTURES_DEST:-/mnt/SDCARD/miyoo/app/consoles}

[ -e "$CONFIG" ] && exit 0
[ -f "$THUMBS/_default.png" ] || exit 0
printf '{\n\t"thumbs":\t"%s"\n}\n' "$THUMBS" > "$CONFIG"
