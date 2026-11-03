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
#
# A folder renamed or copied by hand (Emu/GBA -> Emu/GBA mGBA) has no name in
# map.txt: its console is found from its config.json, the ROM folder first
# (rompath ../../Roms/GBA: gba, in map.txt or roms.txt), then its icon
# (.../gba.png: gba, a name of map.txt or a picture of that name).
# "console_pictures.sh renamed", run at every boot, only does those folders
# and only when the pictures folder exists: a picture deleted on purpose for
# a folder of map.txt comes back with an update, not at every boot.

src=${CONSOLE_PICTURES_SRC:-/mnt/SDCARD/.tmp_update/res/consoles}
dest=${CONSOLE_PICTURES_DEST:-/mnt/SDCARD/miyoo/app/consoles}
sd=${CONSOLE_PICTURES_SD:-/mnt/SDCARD}

[ -f "$src/map.txt" ] || exit 0

lower() {
    echo "$1" | tr 'A-Z' 'a-z'
}

# The icon a table (map.txt, roms.txt) names for a name, or nothing.
map_icon() { # name [table]
    [ -f "$src/${2:-map.txt}" ] || return 0
    awk -v n="$1" '$1 == n { print $2; exit }' "$src/${2:-map.txt}"
}

# A string value of config.json ("key": "value", also on a one-line file).
config_value() { # key, file
    sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p" "$2" | head -n 1
}

copy_renamed() {
    for config in "$sd"/Emu/*/config.json "$sd"/RApp/*/config.json; do
        [ -f "$config" ] || continue
        folder=${config%/config.json}
        name=$(lower "${folder##*/}")
        [ -e "$dest/$name.png" ] && continue
        [ -n "$(map_icon "$name")" ] && continue
        # ROM folder after Roms/ (gba, ports/shortcuts), icon file name
        rom=$(config_value rompath "$config")
        rom=$(lower "${rom%/}")
        case "$rom" in */roms/*) rom=${rom#*/roms/} ;; *) rom=${rom##*/} ;; esac
        icon=$(config_value icon "$config")
        icon=$(lower "${icon##*/}")
        icon=${icon%.png}
        for picture in "$(map_icon "$rom")" "$(map_icon "$rom" roms.txt)" \
            "$(map_icon "$icon")" "$icon"; do
            [ -n "$picture" ] || continue
            if [ -f "$src/$picture.png" ]; then
                cp "$src/$picture.png" "$dest/$name.png"
                break
            fi
        done
    done
}

if [ "$1" = renamed ]; then
    [ -d "$dest" ] && copy_renamed
    exit 0
fi

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

copy_renamed

[ -f "$src/LICENSE.txt" ] && cp -f "$src/LICENSE.txt" "$dest/LICENSE-console-icons.txt"
exit 0
