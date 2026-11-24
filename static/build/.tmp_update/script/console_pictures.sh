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

# POSIX-shell ASCII lowercase, without starting tr for every emulator folder.
# Set lower_result in the caller (command substitution would fork a subshell).
lower() {
    lower_result=$1
    case "$lower_result" in
        *[A-Z]*) ;;
        *) return ;;
    esac
    lower_rest=$lower_result
    lower_result=
    while [ -n "$lower_rest" ]; do
        lower_char=${lower_rest%"${lower_rest#?}"}
        lower_rest=${lower_rest#?}
        case "$lower_char" in
            A) lower_char=a ;; B) lower_char=b ;; C) lower_char=c ;;
            D) lower_char=d ;; E) lower_char=e ;; F) lower_char=f ;;
            G) lower_char=g ;; H) lower_char=h ;; I) lower_char=i ;;
            J) lower_char=j ;; K) lower_char=k ;; L) lower_char=l ;;
            M) lower_char=m ;; N) lower_char=n ;; O) lower_char=o ;;
            P) lower_char=p ;; Q) lower_char=q ;; R) lower_char=r ;;
            S) lower_char=s ;; T) lower_char=t ;; U) lower_char=u ;;
            V) lower_char=v ;; W) lower_char=w ;; X) lower_char=x ;;
            Y) lower_char=y ;; Z) lower_char=z ;;
        esac
        lower_result=$lower_result$lower_char
    done
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
        lower "${folder##*/}"
        name=$lower_result
        [ -e "$dest/$name.png" ] && continue
        [ -n "$(map_icon "$name")" ] && continue
        # ROM folder after Roms/ (gba, ports/shortcuts), icon file name
        rom=$(config_value rompath "$config")
        lower "${rom%/}"
        rom=$lower_result
        case "$rom" in */roms/*) rom=${rom#*/roms/} ;; *) rom=${rom##*/} ;; esac
        icon=$(config_value icon "$config")
        lower "${icon##*/}"
        icon=$lower_result
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
