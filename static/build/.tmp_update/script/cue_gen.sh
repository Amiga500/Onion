#!/bin/sh
# Creates a CUE sheet for every .bin game that has none, in the folder of the
# game's files. The tracks of a game are its .bin files that differ only by
# "(Track N)"; everything else in the name (region, disc, revision) stays,
# so "Game (USA) (Disc 1) (Track 02).bin" belongs to "Game (USA) (Disc 1).cue",
# the name m3u_gen.sh looks for. An existing CUE is never overwritten.

if [ -z "$rootdir" ]; then
    rootdir="/mnt/SDCARD/Roms"
fi

if [ $# -gt 0 ]; then
    targets="$1"
else
    targets="PS SEGACD NEOCD PCE PCFX AMIGA"
fi

cd "$rootdir" || exit 1

# Prints the CUE for the tracks given on stdin as "track<TAB>file" lines.
cue_sheet() {
    sort -n | while IFS="$(printf '\t')" read -r track file; do
        if [ "$track" -eq 1 ]; then
            printf 'FILE "%s" BINARY\n    TRACK %02d MODE1/2352\n        INDEX 01 00:00:00\n' "$file" "$track"
        else
            printf 'FILE "%s" BINARY\n    TRACK %02d AUDIO\n        INDEX 00 00:00:00\n        INDEX 01 00:02:00\n' "$file" "$track"
        fi
    done
}

find $targets -maxdepth 3 -name "*.bin" -type f 2> /dev/null | sort | (
    count=0
    skipped=0
    group=""
    tracks=""
    tab="$(printf '\t')"

    # Writes the CUE of the game collected so far, unless one already exists.
    flush() {
        [ -n "$group" ] && [ -n "$tracks" ] || return 0
        cue_path="$group.cue"
        if [ -e "$cue_path" ]; then
            echo "SKIP \"$cue_path\" (already exists)"
            skipped=$((skipped + 1))
            return 0
        fi
        echo "GAME \"$group\""
        printf '%s\n' "$tracks" | cue_sheet > "$cue_path"
        cat "$cue_path"
        count=$((count + 1))
    }

    while IFS= read -r target; do
        dir_path=$(dirname "$target")
        target_name=$(basename "$target")
        target_base="${target_name%.*}"

        # "(Track N)" or "Track N" picks the track; the rest of the name is the game.
        track_number=$(echo "$target_base" | sed -n -E 's/.*[Tt]rack[[:space:]]*0*([0-9]+).*/\1/p')
        game_base=$(echo "$target_base" | sed -E 's/[[:space:]]*[(]?[Tt]rack[[:space:]]*[0-9]+[)]?//' | sed 's/[[:space:]]*$//')
        [ -n "$track_number" ] || track_number=1
        [ -n "$game_base" ] || game_base="$target_base"

        # Compared as plain strings: names with [ ] or other pattern
        # characters are matched literally.
        if [ "$dir_path/$game_base" != "$group" ]; then
            flush
            group="$dir_path/$game_base"
            tracks=""
        fi

        # The CUE sits next to its files, so FILE is the bare file name.
        if [ -n "$tracks" ]; then
            tracks="$tracks
$track_number$tab$target_name"
        else
            tracks="$track_number$tab$target_name"
        fi
    done
    flush

    # print totals
    echo "$count cue $([ $count -eq 1 ] && echo "file" || echo "files") created"
    if [ $skipped -gt 0 ]; then
        echo "$skipped existing cue $([ $skipped -eq 1 ] && echo "file" || echo "files") kept"
    fi
)

find $targets -maxdepth 1 -type f -name "*_cache6.db" -exec rm -f {} \;

echo "Success"
