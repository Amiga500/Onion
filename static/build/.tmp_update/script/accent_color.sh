#!/bin/sh
# Accent colour of the Silky RedOnion theme (Tweaks > Look and feel >
# Accent colour): red (the theme's own), blue, purple, teal, pink, orange.
#
# Usage: accent_color.sh [THEME_PATH]
#   THEME_PATH: the theme being applied (themeSwitcher); without it, the
#   active theme from system.json (the boot, before MainUI starts, and
#   Tweaks when the colour changes).
#
# The colour is the theme's: Open MainUI and the RedOnion programs draw the
# selection, the buttons and the menu icons from its images. For a colour
# other than red, the images that carry the red accent, recoloured
# (res/accent/<colour>/, made by tools/accent/gen_accent.py), are copied
# into the theme overrides (Saves/CurrentProfile/theme/skin), which both
# read before the theme's own files. Only with Silky RedOnion: another theme
# keeps its own colours, and the copies are removed.
#
# The files copied are listed in the overrides' .accent_files, after a first
# line with the state they were made for; only those are ever removed, so
# the user's own theme tweaks stay. A file the user already overrides is
# not replaced. The theme's list images resized for the game-list row count
# (rescale_theme_list_icons.sh: bg-list-s_N.png, *.png.bak originals) are
# matched: the copy is resized the same way, or the theme's red resized
# image would be used.
#
# config/display/accentRGB gets the colour in use, as "accent light dark"
# (RRGGBB): the programs that draw without theme images read it (red when
# it is missing).

SDROOT=${SDROOT:-/mnt/SDCARD}
sysdir=${SYSDIR:-$SDROOT/.tmp_update}
ACCENT_RES=${ACCENT_RES:-$sysdir/res/accent}
OVERRIDES=${OVERRIDES:-$SDROOT/Saves/CurrentProfile/theme}
ACCENT_CONFIG=${ACCENT_CONFIG:-$sysdir/config/display/accentColor}
ACCENT_RGB=${ACCENT_RGB:-$sysdir/config/display/accentRGB}
SYSTEM_JSON=${SYSTEM_JSON:-/appconfigs/system.json}
PNGRESIZE=${PNGRESIZE:-$sysdir/bin/pngresize}
PNGSCALE=${PNGSCALE:-$sysdir/bin/pngScale}
ACCENT_THEME="Silky RedOnion"

LIST=$OVERRIDES/.accent_files

# The theme folder: the argument, or system.json's "theme" value.
theme_path=${1:-}
if [ -z "$theme_path" ] && [ -f "$SYSTEM_JSON" ]; then
    theme_path=$(sed -n 's/.*"theme"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$SYSTEM_JSON" | head -n 1)
fi
theme_path=${theme_path%/}
theme_name=${theme_path##*/}

# The chosen colour: its line in palette.txt (Tweaks' order, 0 = red).
index=0
[ -f "$ACCENT_CONFIG" ] && read -r index < "$ACCENT_CONFIG"
case "$index" in '' | *[!0-9]*) index=0 ;; esac
palette() { grep -v '^#' "$ACCENT_RES/palette.txt" 2> /dev/null; }
line=$(palette | sed -n "$((index + 1))p")
[ -n "$line" ] || line=$(palette | head -n 1)
[ -n "$line" ] || line="red 9E2C45 D65670 782036"
set -- $line
colour=$1
rgb="$2 $3 $4"

if [ "$theme_name" != "$ACCENT_THEME" ] || [ ! -d "$ACCENT_RES/$colour" ]; then
    colour=red
    set -- $(palette | head -n 1)
    rgb="${2:-9E2C45} ${3:-D65670} ${4:-782036}"
fi

# The colours for the programs, written only when they change (this runs
# before every MainUI start).
write_rgb() {
    [ "$(cat "$ACCENT_RGB" 2> /dev/null)" = "$rgb" ] && return 0
    mkdir -p "${ACCENT_RGB%/*}"
    printf '%s\n' "$rgb" > "$ACCENT_RGB.tmp" && mv -f "$ACCENT_RGB.tmp" "$ACCENT_RGB"
}

# PNG IHDR: big-endian width at offset 16, height at offset 20.
png_size() {
    set -- $(od -An -tu1 -j16 -N8 "$1" 2> /dev/null)
    [ "$#" -eq 8 ] || return 1
    echo "$(($1 * 16777216 + $2 * 65536 + $3 * 256 + $4)) $(($5 * 16777216 + $6 * 65536 + $7 * 256 + $8))"
}

# The theme's resized list images, by name and size: when they change
# (Tweaks > Game lists > Layout), the copies are made again.
resized_state() {
    for f in "$theme_path"/skin/bg-list-s_*.png "$theme_path"/skin/*.png.bak; do
        [ -f "$f" ] || continue
        f=${f%.bak}
        echo "${f##*/}:$(png_size "$f" | tr ' ' x)"
    done | tr '\n' ' '
}

state="$theme_name|$colour|$(resized_state)"

# Nothing changed: nothing to do (every boot comes here).
if [ -f "$LIST" ] && [ "$(head -n 1 "$LIST")" = "$state" ]; then
    write_rgb
    exit 0
fi

scale() { # src dst width height
    if [ -x "$PNGRESIZE" ]; then
        "$PNGRESIZE" --quiet --overwrite --stretch "$1" "$2" "$3" "$4"
    else
        "$PNGSCALE" "$1" "$2" "$3" "$4" stretch
    fi
}

# Copies one recoloured image into the overrides, as large as the theme's
# own file is now (resized for the row count, or as it came).
place() { # rel_src rel_dst
    src=$ACCENT_RES/$colour/$1
    rel=$2
    dst=$OVERRIDES/$rel
    theme_file=$theme_path/$rel
    [ -e "$dst" ] && return 0 # the user's own override stays
    mkdir -p "${dst%/*}"
    want=$(png_size "$theme_file" 2> /dev/null)
    have=$(png_size "$src")
    if [ -n "$want" ] && [ "$want" != "$have" ]; then
        set -- $want
        scale "$src" "$dst.tmp" "$1" "$2" > /dev/null 2>&1 && [ -s "$dst.tmp" ] &&
            mv -f "$dst.tmp" "$dst" || {
            rm -f "$dst.tmp"
            return 0
        }
    else
        cp -f "$src" "$dst" || return 0
    fi
    echo "$rel" >> "$LIST.new"
}

# The previous colour's files go first.
if [ -f "$LIST" ]; then
    tail -n +2 "$LIST" | while IFS= read -r f; do
        case "$f" in
            *..*) ;;
            skin/*) rm -f "$OVERRIDES/$f" ;;
        esac
    done
fi

mkdir -p "$OVERRIDES"
printf '%s\n' "$state" > "$LIST.new"
if [ "$colour" != red ]; then
    (cd "$ACCENT_RES/$colour" && find skin -type f -name '*.png') | while IFS= read -r f; do
        place "$f" "$f"
    done
    # The selection for a game-list row count other than 6
    for f in "$theme_path"/skin/bg-list-s_*.png; do
        [ -f "$f" ] || continue
        place skin/bg-list-s.png "skin/${f##*/}"
    done
fi
mv -f "$LIST.new" "$LIST"
rmdir "$OVERRIDES/skin/extra" "$OVERRIDES/skin" 2> /dev/null

write_rgb
sync
