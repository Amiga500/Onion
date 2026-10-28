#!/bin/sh

rootdir="${MIYOOGAMELIST_EMU_DIR:-/mnt/SDCARD/Emu}"

clean_name() {
    name="$1"
    extlist="$2"

    ### REMOVE NON-ESSENTIALS ###
    # remove file extensions
    while echo "$name" | grep -qE "\.($extlist)$"; do
        name="${name%.*}"
    done

    # remove everything in brackets
    name=$(echo "$name" | sed -e 's/([^)]*)//g')
    name=$(echo "$name" | sed -e 's/\[[^]]*\]//g')

    # remove rankings
    name=$(echo "$name" | sed -e 's/^[0-9]\+\.//')

    # trim
    name=$(echo "$name" | awk '{$1=$1};1')

    ### FORMAT ###
    # move article to the start of the name, if present
    article=$(echo "$name" | sed -ne 's/.*, \(A\|The\|An\).*/\1/p')
    if [ ! -z $article ]; then
        name="$article $(echo "$name" | sed -e 's/, \(A\|The\|An\)//')"
    fi

    # change " - " to ": " for subtitles
    name=$(echo "$name" | sed -e 's/ - /: /')

    echo "$name"
}

# One <game> for the ROM file named $1 (a name, no folder), if it is a game.
add_game() {
    rom="$1"

    # ignore non-game files
    if ! echo "$rom" | grep -qE "\.($extlist)$"; then
        return
    fi

    filename="${rom%.*}"
    digest=$(clean_name "$rom" "$extlist")

    cat <<EOF >>"$gamelistpath"
    <game>
        <path>$rompath/$rom</path>
        <name>$digest</name>
        <image>$imgpath/$filename.png</image>
    </game>
EOF
}

generate_miyoogamelist() {
    basepath=$1
    gamelistpath="$basepath/miyoogamelist.xml"
    rompath=$2
    imgpath=$3
    extlist=$4

    # create backup of previous miyoogamelist.xml
    if [ -f "$gamelistpath" ]; then
        mv "$gamelistpath" "$gamelistpath.bak"
    fi

    echo '<?xml version="1.0"?>' >"$gamelistpath"
    echo '<gameList>' >>"$gamelistpath"

    for rom in $basepath/$rompath/*; do
        # ignore subfolders because miyoogamelist don't work with them
        # also ignores Imgs folder, nice
        if [ -d "$rom" ]; then
            continue
        fi
        add_game "${rom##*/}"
    done

    # Games hidden with GLO's Hide game wait in .hidden/<name>/. They are
    # listed at their usual place: MainUI skips an entry whose file is not
    # there, so the game stays hidden, and shows again once it is back.
    # Without them, a list made while a game was hidden left it out for good.
    for rom in "$basepath/$rompath/.hidden"/*/*; do
        [ -f "$rom" ] || continue
        # a visible file of the same name is listed already
        [ -e "$basepath/$rompath/${rom##*/}" ] && continue
        add_game "${rom##*/}"
    done

    echo '</gameList>' >>"$gamelistpath"
}

for system in "$rootdir"/*; do
    if [ -d "$system" ]; then
        # ignore Arcades & Ports since they have their own gamelist already
        if echo "$system" | grep -qE "(PORTS|FBA2012|FBNEO|MAME2000|ARCADE|NEOGEO|CPS1|CPS2|CPS3)"; then
            continue
        fi

        cd "$system"

        # read info from config.json
        gamelist=$(grep -E '"gamelist":' config.json | sed -e 's/^.*:\s*"\(.*\)",*/\1/')
        rompath=$(grep -E '"rompath":' config.json | sed -e 's/^.*:\s*"\(.*\)",*/\1/')
        imgpath=$(grep -E '"imgpath":' config.json | sed -e 's/^.*:\s*"\(.*\)",*/\1/')
        extlist=$(grep -E '"extlist":' config.json | sed -e 's/^.*:\s*"\(.*\)",*/\1/')

        # some Emu configs do not have the gamelist key, fallback to rompath
        if [ -z "$gamelist" ]; then
            gamelist="$rompath"
        fi

        basepath="${gamelist%/miyoogamelist.xml}"
        # ensure rompath/imgpath are relative to basepath
        rompath=".${rompath#$basepath}"
        imgpath=".${imgpath#$basepath}"

        generate_miyoogamelist "$basepath" "$rompath" "$imgpath" "$extlist"
    fi
done
