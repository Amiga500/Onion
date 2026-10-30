#!/bin/sh
# Console pictures for Open MainUI 1.1.0's carousel and list (Tweaks > Main
# menu > Console view > Console pictures): KyleBing's "Retro game console
# icons" (CC BY 4.0, res/consoles/LICENSE.txt), one per emulator folder.
# Open MainUI looks up <thumbs>/<emulator folder in lower case>.png, then
# <thumbs>/_default.png, then the theme's picture. map.txt names the icon for
# each folder: Games lists Emu folders, Expert RApp folders.
#
# Copies only the pictures missing from the folder, so a picture put there by
# hand is never replaced. Run by the installer on every update.

src=${CONSOLE_PICTURES_SRC:-/mnt/SDCARD/.tmp_update/res/consoles}
dest=${CONSOLE_PICTURES_DEST:-/mnt/SDCARD/miyoo/app/consoles}

[ -f "$src/map.txt" ] || exit 0
mkdir -p "$dest" || exit 1

copy_new() { # source, destination
    [ -e "$2" ] || cp "$1" "$2"
}

[ -f "$src/_default.png" ] && copy_new "$src/_default.png" "$dest/_default.png"

while read -r name icon _; do
    case "$name" in
        '' | '#'*) continue ;;
    esac
    [ -f "$src/$icon.png" ] && copy_new "$src/$icon.png" "$dest/$name.png"
done < "$src/map.txt"

[ -f "$src/LICENSE.txt" ] && cp -f "$src/LICENSE.txt" "$dest/LICENSE-console-icons.txt"
exit 0
