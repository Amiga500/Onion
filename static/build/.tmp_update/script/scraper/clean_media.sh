#!/bin/sh
# Media of deleted games: the covers, Game Info media and manuals left behind
# when ROMs are deleted (suggested on Reddit by ProfessionalTheme193: renaming
# or deleting old art by hand took ages).
#
# usage: clean_media.sh [--delete] CONSOLE_DIR...
#   CONSOLE_DIR: a console folder, e.g. /mnt/SDCARD/Roms/GB
#   Without --delete it only lists what would go; with it, deletes it.
#   Prints one file per line; a console without any game is skipped and
#   reported as "skip: <dir>" (its Imgs may be waiting for the ROMs to come).
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

delete=0
if [ "$1" = "--delete" ]; then
    delete=1
    shift
fi

tmp=${TMPDIR:-/tmp}/clean_media.$$
mkdir -p "$tmp" || exit 1
trap 'rm -rf "$tmp"' EXIT

for dir in "$@"; do
    dir=${dir%/}
    [ -d "$dir" ] || continue

    # Names still in use: every file and folder outside the media folders.
    find "$dir" ! -path "$dir" \
        ! -path "$dir/Imgs" ! -path "$dir/Imgs/*" \
        ! -path "$dir/media" ! -path "$dir/media/*" \
        ! -path "$dir/Manuals" ! -path "$dir/Manuals/*" \
        ! -path "$dir/.*" ! -path "$dir/*/.*" \
        \( -type f -o -type d \) > "$tmp/games" 2> /dev/null

    if [ ! -s "$tmp/games" ]; then
        echo "skip: $dir"
        continue
    fi
    # Folders keep their whole name (folder art is "<folder name>.png");
    # files lose their extension.
    find "$dir" ! -path "$dir" ! -path "$dir/.*" ! -path "$dir/*/.*" -type d \
        ! -path "$dir/Imgs" ! -path "$dir/Imgs/*" \
        ! -path "$dir/media" ! -path "$dir/media/*" \
        ! -path "$dir/Manuals" ! -path "$dir/Manuals/*" > "$tmp/folders" 2> /dev/null

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
