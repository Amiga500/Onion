#!/bin/sh
# Media of deleted games: the covers, Game Info media and manuals left behind
# when ROMs are deleted (suggested on Reddit by ProfessionalTheme193: renaming
# or deleting old art by hand took ages).
#
# usage: clean_media.sh [--delete] CONSOLE_DIR...
#   CONSOLE_DIR: a console folder, e.g. /mnt/SDCARD/Roms/GB
#   Without --delete it only lists what would go; with it, deletes it.
#   Prints one file per line; a console without any game is skipped and
#   reported as "skip: <dir>" (its Imgs may be waiting for the ROMs to come),
#   and so is one whose games could not all be read (an SD card read error:
#   their media would count as left behind).
#
# What is looked at, in each console folder (the scraper's places, and where
# Open MainUI and GLO look for them):
#   Imgs/<name>.png                cover
#   media/<kind>/<name>.<ext>      Game Info media (box2dfront, box2dback,
#                                  box3d, screenshot, videos, manuals, ...);
#                                  gallery pictures may be <name>-<n>.<ext>
#   Manuals/<name>.pdf             manual (GLO)
# A file stays when its name, without its extension (or without a "-<n>"
# gallery number), is the name of anything left in the console folder: a ROM
# without its extension, or a folder (folder art), at any depth, case
# ignored. That is generous on purpose: a stray Tetris.txt keeps Tetris.png;
# nothing a game still uses goes. Hidden files and folders are never
# touched, nor gamelist.xml.
# Covers shared with another console count too: a console installed in Emu
# or RApp whose config.json has its imgpath in this folder's Imgs (MAME 2003
# and MAME 2000: Roms/MAME2003 and Roms/MAME2000 use Roms/ARCADE/Imgs) keeps
# the covers of its games. Without that, cleaning ARCADE deleted them.
# Files named in the console's miyoogamelist.xml or gamelist.xml (<image>,
# <thumbnail>, <marquee>, <video>, <manual>) stay too, whatever their name:
# hand-made or imported lists may point at "Imgs/tetris-cover.png".

delete=0
if [ "$1" = "--delete" ]; then
    delete=1
    shift
fi

tmp=${TMPDIR:-/tmp}/clean_media.$$
mkdir -p "$tmp" || exit 1
trap 'rm -rf "$tmp"' EXIT

# A config.json path (relative to Emu/<x> or RApp/<x>, or under /mnt/SDCARD)
# as a path under the SD card root $2, without a trailing slash.
sd_path() {
    case "$1" in
        ../../*) p="$2/${1#../../}" ;;
        /mnt/SDCARD/*) p="$2/${1#/mnt/SDCARD/}" ;;
        *) return 1 ;;
    esac
    echo "${p%/}"
}

# The ROM folders of other consoles whose covers are in $1/Imgs, one per line.
shared_rom_dirs() {
    roms=${1%/*}
    sd=${roms%/*}
    own=$(echo "$1" | tr 'A-Z' 'a-z')
    for cfg in "$sd"/Emu/*/config.json "$sd"/RApp/*/config.json; do
        [ -f "$cfg" ] || continue
        img=$(sed -n 's/.*"imgpath"[^"]*"\([^"]*\)".*/\1/p' "$cfg" | head -n 1)
        rom=$(sed -n 's/.*"rompath"[^"]*"\([^"]*\)".*/\1/p' "$cfg" | head -n 1)
        img=$(sd_path "$img" "$sd") || continue
        rom=$(sd_path "$rom" "$sd") || continue
        [ "$(echo "$img" | tr 'A-Z' 'a-z')" = "$own/imgs" ] || continue
        [ "$(echo "$rom" | tr 'A-Z' 'a-z')" = "$own" ] && continue
        [ -d "$rom" ] && echo "$rom"
    done
}

# Every file and folder of a ROM folder outside its media folders and
# hidden ones (the names still in use); with "-type d", the folders only.
# Fails when find could not read all of it.
names_in() {
    find "$1" ! -path "$1" \
        ! -path "$1/Imgs" ! -path "$1/Imgs/*" \
        ! -path "$1/media" ! -path "$1/media/*" \
        ! -path "$1/Manuals" ! -path "$1/Manuals/*" \
        ! -path "$1/.*" ! -path "$1/*/.*" \
        \( ${2:--type f -o -type d} \) 2> /dev/null
}

# The media files named in a ROM folder's game lists, one path per line.
# Plain index/substr for the XML entities: no escapes in a gsub
# replacement, and no "name (" concatenation (BusyBox awk).
listed_media() {
    for xml in "$1/miyoogamelist.xml" "$1/gamelist.xml"; do
        [ -f "$xml" ] && cat "$xml"
    done | awk -v q="'" -v dq='"' '
        function unent(v, e, c,    i, out) {
            out = ""
            while ((i = index(v, e)) > 0) {
                out = out substr(v, 1, i - 1) c
                v = substr(v, i + length(e))
            }
            return out v
        }
        {
            line = $0
            while (match(line, /<(image|thumbnail|marquee|video|manual)>[^<]*</)) {
                v = substr(line, RSTART, RLENGTH - 1)
                line = substr(line, RSTART + RLENGTH)
                sub(/^<[a-z]*>/, "", v)
                v = unent(v, "&apos;", q)
                v = unent(v, "&quot;", dq)
                v = unent(v, "&lt;", "<")
                v = unent(v, "&gt;", ">")
                v = unent(v, "&amp;", "&")
                if (v != "")
                    print v
            }
        }'
}

for dir in "$@"; do
    dir=${dir%/}
    [ -d "$dir" ] || continue

    # Names still in use: every file and folder outside the media folders.
    # A folder that could not all be read: its games would be missing from
    # the list and their media taken for left behind.
    if ! names_in "$dir" > "$tmp/games"; then
        echo "skip: $dir (read error)"
        continue
    fi

    if [ ! -s "$tmp/games" ]; then
        echo "skip: $dir"
        continue
    fi
    # Folders keep their whole name (folder art is "<folder name>.png");
    # files lose their extension.
    names_in "$dir" "-type d" > "$tmp/folders"

    # The games of consoles sharing this folder's covers
    shared_rom_dirs "$dir" > "$tmp/shared"
    unread=
    while IFS= read -r other; do
        names_in "$other" >> "$tmp/games" || unread=1
        names_in "$other" "-type d" >> "$tmp/folders"
    done < "$tmp/shared"
    if [ -n "$unread" ]; then
        echo "skip: $dir (read error)"
        continue
    fi

    # Media the game lists name, here and in the folders sharing the covers
    listed_media "$dir" >> "$tmp/games"
    while IFS= read -r other; do
        listed_media "$other" >> "$tmp/games"
    done < "$tmp/shared"

    # Media: files directly in Imgs and Manuals, and in each media/<kind>.
    {
        [ -d "$dir/Imgs" ] && find "$dir/Imgs" -maxdepth 1 -type f ! -name ".*"
        [ -d "$dir/Manuals" ] && find "$dir/Manuals" -maxdepth 1 -type f ! -name ".*"
        for kind in "$dir"/media/*/; do
            [ -d "$kind" ] && find "$kind" -maxdepth 1 -type f ! -name ".*"
        done
    } > "$tmp/media" 2> /dev/null

    [ -s "$tmp/media" ] || continue

    awk -v folders="$tmp/folders" '
        function base(p) { sub(/.*\//, "", p); return p }
        function noext(n) { if (match(n, /.\.[^.]*$/)) return substr(n, 1, RSTART); return n }
        BEGIN {
            while ((getline line < folders) > 0)
                folder[line] = 1
        }
        FILENAME != ARGV[ARGC - 1] {
            n = base($0)
            keep[tolower(($0 in folder) ? n : noext(n))] = 1
            next
        }
        {
            n = noext(base($0))
            k = tolower(n)
            if (k in keep)
                next
            if (match(k, /-[0-9]+$/) && (substr(k, 1, RSTART - 1) in keep))
                next
            print
        }
    ' "$tmp/games" "$tmp/media" > "$tmp/orphans"

    while IFS= read -r file; do
        [ -n "$file" ] || continue
        if [ $delete -eq 1 ]; then
            rm -f "$file" && echo "$file"
        else
            echo "$file"
        fi
    done < "$tmp/orphans"
done

[ $delete -eq 1 ] && sync
exit 0
