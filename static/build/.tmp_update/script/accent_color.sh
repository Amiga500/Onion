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
# (res/accent/<colour>/, made by tools/accent/gen_accent.py, listed in
# res/accent/files.txt), are copied into the theme overrides
# (Saves/CurrentProfile/theme/skin), which both read before the theme's own
# files. Only with Silky RedOnion: another theme keeps its own colours, and
# the copies are removed.
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
#
# Speed: Tweaks runs this on every change of the colour, and the boot before
# every MainUI start. A command started here costs a few milliseconds on the
# console, so the usual paths start almost none: the files are read with
# the shell's own read, the copies made with one cp and removed with one rm
# (the first version, one cp, od and mkdir per picture, took 1-2 seconds
# per change in Tweaks; robcodedev). Only resized theme images, or the
# user's own overrides among the pictures, take the slower file-by-file way.

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
FILES=$ACCENT_RES/files.txt

# The theme folder: the argument, or system.json's "theme" value.
theme_path=${1:-}
if [ -z "$theme_path" ] && [ -f "$SYSTEM_JSON" ]; then
    while IFS= read -r _l || [ -n "$_l" ]; do
        case "$_l" in
            *\"theme\"*:*\"*\"*)
                _l=${_l#*\"theme\"}
                _l=${_l#*\"}
                theme_path=${_l%%\"*}
                break
                ;;
        esac
    done < "$SYSTEM_JSON"
fi
theme_path=${theme_path%/}
theme_name=${theme_path##*/}

# The chosen colour: its line in palette.txt (Tweaks' order, 0 = red).
index=0
[ -f "$ACCENT_CONFIG" ] && read -r index < "$ACCENT_CONFIG"
case "$index" in '' | *[!0-9]*) index=0 ;; esac
colour=red
rgb="9E2C45 D65670 782036"
_n=0
if [ -f "$ACCENT_RES/palette.txt" ]; then
    while read -r _name _a _l _d; do
        case "$_name" in '' | \#*) continue ;; esac
        if [ "$_n" -eq 0 ] || [ "$_n" -eq "$index" ]; then
            colour=$_name
            rgb="$_a $_l $_d"
        fi
        _n=$((_n + 1))
    done < "$ACCENT_RES/palette.txt"
    # out of range: the first (red), already taken above
    [ "$index" -lt "$_n" ] || {
        while read -r _name _a _l _d; do
            case "$_name" in '' | \#*) continue ;; esac
            colour=$_name
            rgb="$_a $_l $_d"
            break
        done < "$ACCENT_RES/palette.txt"
    }
fi

if [ "$theme_name" != "$ACCENT_THEME" ] || [ "$colour" = red ] || [ ! -d "$ACCENT_RES/$colour" ]; then
    want_red=1
else
    want_red=0
fi
if [ "$want_red" -eq 1 ]; then
    colour=red
    if [ -f "$ACCENT_RES/palette.txt" ]; then
        while read -r _name _a _l _d; do
            case "$_name" in '' | \#*) continue ;; esac
            rgb="$_a $_l $_d"
            break
        done < "$ACCENT_RES/palette.txt"
    fi
fi

# The colours for the programs, written only when they change.
write_rgb() {
    _old=""
    [ -f "$ACCENT_RGB" ] && read -r _old < "$ACCENT_RGB"
    [ "$_old" = "$rgb" ] && return 0
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
# (Tweaks > Game lists > Layout), the copies are made again. Usually there
# are none, and nothing is started.
resized=""
for f in "$theme_path"/skin/bg-list-s_*.png "$theme_path"/skin/*.png.bak; do
    [ -f "$f" ] || continue
    f=${f%.bak}
    resized="$resized${f##*/}:$(png_size "$f" | tr ' ' x) "
done

# The set of pictures: files.txt's first line ("# set <id>", made by
# gen_accent.py), so an update that brings other pictures (the top bars'
# logo, 4.5.1) copies them again, even for the colour already applied.
files_id=
[ -f "$FILES" ] && IFS= read -r files_id < "$FILES"
case "$files_id" in "# set "*) files_id=${files_id#\# set } ;; *) files_id= ;; esac

state="$theme_name|$colour|$resized|$files_id"

# Nothing changed: nothing to do (every boot comes here).
_first=""
[ -f "$LIST" ] && read -r _first < "$LIST"
if [ -f "$LIST" ] && [ "$_first" = "$state" ]; then
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

# One picture, file by file: as large as the theme's own file is now
# (resized for the row count, or as it came). For the slow way only.
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

# The previous colour's files go first: one rm for all of them. Only paths
# under skin/ made of plain file-name characters, never one with "..".
if [ -f "$LIST" ]; then
    set --
    {
        read -r _skip
        while IFS= read -r f; do
            case "$f" in
                *..* | *[!A-Za-z0-9._/%+\&-]*) ;;
                skin/*) set -- "$@" "$OVERRIDES/$f" ;;
            esac
        done
    } < "$LIST"
    [ "$#" -gt 0 ] && rm -f "$@"
fi

mkdir -p "$OVERRIDES/skin"
printf '%s\n' "$state" > "$LIST.new"
if [ "$colour" != red ]; then
    # The list of pictures (one per line, skin/...); made by find if missing
    if [ ! -f "$FILES" ]; then
        FILES=/tmp/accent_files.$$
        (cd "$ACCENT_RES/$colour" && find skin -type f -name '*.png') > "$FILES"
    fi
    # The fast way: no resized theme images and none of the pictures already
    # overridden by the user (checked with the shell's own test).
    fast=1
    [ -n "$resized" ] && fast=0
    if [ "$fast" -eq 1 ]; then
        while IFS= read -r f; do
            case "$f" in '' | \#*) continue ;; esac
            [ -e "$OVERRIDES/$f" ] && {
                fast=0
                break
            }
        done < "$FILES"
    fi
    if [ "$fast" -eq 1 ] && cp -r "$ACCENT_RES/$colour/skin" "$OVERRIDES/"; then
        while IFS= read -r f; do
            case "$f" in '' | \#*) continue ;; esac
            echo "$f" >> "$LIST.new"
        done < "$FILES"
    else
        while IFS= read -r f; do
            case "$f" in '' | \#*) continue ;; esac
            place "$f" "$f"
        done < "$FILES"
        # The selection for a game-list row count other than 6
        for f in "$theme_path"/skin/bg-list-s_*.png; do
            [ -f "$f" ] || continue
            place skin/bg-list-s.png "skin/${f##*/}"
        done
    fi
fi
mv -f "$LIST.new" "$LIST"
case "$FILES" in /tmp/accent_files.*) rm -f "$FILES" ;; esac
rmdir "$OVERRIDES/skin/extra" "$OVERRIDES/skin" 2> /dev/null

write_rgb
sync
