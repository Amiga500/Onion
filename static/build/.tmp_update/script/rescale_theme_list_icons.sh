#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Resize the current theme's list assets to fit a game-list row count
# (Tweaks > Appearance > Game lists > Rescale theme list icons to fit).
# Adapted from robcodedev/onionos-mainui-patcher; uses Onion's pngScale.
#
# Usage: rescale_theme_list_icons.sh THEME_PATH MAX_HEIGHT ROW_COUNT
#
# The three icons keep a one-time .bak original and are always made from it,
# never larger than the original. bg-list-s.png is never changed: for a row
# count other than 6, bg-list-s_N.png is made from it, which Open MainUI uses
# for that row count (docs/THEMES.md). 6 rows puts the originals back.

set -u

PNGSCALE="${PNGSCALE:-/mnt/SDCARD/.tmp_update/bin/pngScale}"

theme_path=${1:-}
max_height=${2:-}
row_count=${3:-}

case "$max_height" in '' | *[!0-9]*) echo "Invalid max height: $max_height" >&2; exit 2 ;; esac
case "$row_count" in '' | *[!0-9]*) echo "Invalid row count: $row_count" >&2; exit 2 ;; esac
if [ -z "$theme_path" ] || [ ! -d "$theme_path" ]; then
    echo "Missing theme path: $theme_path" >&2
    exit 2
fi
if [ "$max_height" -le 0 ] || [ "$row_count" -lt 6 ] || [ "$row_count" -gt 20 ]; then
    echo "Invalid size: $max_height px, $row_count rows" >&2
    exit 2
fi

theme_path=${theme_path%/}

restore_one() {
    file="$theme_path/$1"
    [ -f "$file.bak" ] || return 0
    cp "$file.bak" "$file"
}

if [ "$row_count" -eq 6 ]; then
    status=0
    restore_one "skin/icon-folder.png" || status=1
    restore_one "skin/icon-game.png" || status=1
    restore_one "skin/ic-favorite-mark.png" || status=1
    exit "$status"
fi

if [ ! -x "$PNGSCALE" ]; then
    echo "pngScale not found: $PNGSCALE" >&2
    exit 3
fi

# PNG IHDR: big-endian width at offset 16, height at offset 20.
png_size() {
    set -- $(od -An -tu1 -j16 -N8 "$1" 2> /dev/null)
    [ "$#" -eq 8 ] || return 1
    echo "$(($1 * 16777216 + $2 * 65536 + $3 * 256 + $4)) $(($5 * 16777216 + $6 * 65536 + $7 * 256 + $8))"
}

# A deliberate wide spacer icon (at least 120 px and 3:1) keeps its geometry,
# as Open MainUI expects (docs/THEMES.md).
is_wide_icon() {
    size=$(png_size "$1") || return 1
    set -- $size
    [ "$2" -gt 0 ] && [ "$1" -ge 120 ] && [ "$1" -ge $(($2 * 3)) ]
}

resize_icon() {
    file="$theme_path/$1"
    backup="$file.bak"
    if [ ! -f "$backup" ]; then
        [ -f "$file" ] || return 0
        cp "$file" "$backup" || return 1
    fi

    if is_wide_icon "$backup"; then
        cp "$backup" "$file"
        return
    fi

    size=$(png_size "$backup") || return 1
    set -- $size
    width=$1
    height=$2
    if [ "$height" -le "$max_height" ]; then
        # Already fits: never upscale.
        cp "$backup" "$file"
        return
    fi

    tmp="$file.tmp.$$"
    if "$PNGSCALE" "$backup" "$tmp" "$width" "$max_height" > /dev/null 2>&1 && [ -s "$tmp" ]; then
        mv -f "$tmp" "$file"
    else
        rm -f "$tmp"
        return 1
    fi
}

resize_row_background() {
    source="$theme_path/skin/bg-list-s.png"
    target="$theme_path/skin/bg-list-s_${row_count}.png"
    [ -f "$source" ] || return 0
    [ -f "$target" ] && return 0

    size=$(png_size "$source") || return 1
    set -- $size
    width=$1
    # Fits the row already: Open MainUI uses bg-list-s.png as it is.
    [ "$2" -gt "$max_height" ] || return 0

    # Same width, row height: stretched, as the row is.
    tmp="$target.tmp.$$"
    if "$PNGSCALE" "$source" "$tmp" "$width" "$max_height" stretch > /dev/null 2>&1 && [ -s "$tmp" ]; then
        mv -f "$tmp" "$target"
    else
        rm -f "$tmp"
        return 1
    fi
}

status=0
resize_icon "skin/icon-folder.png" || status=1
resize_icon "skin/icon-game.png" || status=1
resize_icon "skin/ic-favorite-mark.png" || status=1
resize_row_background || status=1
sync
exit "$status"
