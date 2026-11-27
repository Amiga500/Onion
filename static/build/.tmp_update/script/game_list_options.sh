#!/bin/sh
sysdir=/mnt/SDCARD/.tmp_update
radir=/mnt/SDCARD/RetroArch/.retroarch
ext_cache_path=$radir/cores/cache/core_extensions.cache
globalscriptdir=/mnt/SDCARD/App/romscripts

recentlist=/mnt/SDCARD/Roms/recentlist.json
recentlist_hidden=/mnt/SDCARD/Roms/recentlist-hidden.json
recentlist_temp=/tmp/recentlist-temp.json

save_dir=/mnt/SDCARD/Saves/CurrentProfile/saves
gpsp_migration_log="$save_dir/gpSP/migration_status.log"

[ -d "$radir/cores/cache" ] || mkdir -p "$radir/cores/cache"

# Logging setup
logfile=${0##*/}
logfile=${logfile%.sh}
. $sysdir/script/log.sh

cd $sysdir

# The Mini Plus (354) and the Mini Flip (285) have WiFi.
device_has_networking() {
    case "$1" in
        354 | 285) echo 1 ;;
        *) echo 0 ;;
    esac
}

# Tweaks > Game lists > List menus > GLO (game options): an entry Off writes
# config/.hideGlo<name> and the entry is left out.
glo_shown() {
    [ ! -f "$sysdir/config/.hideGlo$1" ]
}

device_model=
IFS= read -r device_model < /tmp/deviceModel || :
case $device_model in
    354 | 285) has_networking=1 ;;
    *) has_networking=0 ;;
esac

ROM_TYPE_UNKNOWN=0
ROM_TYPE_GAME=1
ROM_TYPE_APP=2

TAB_GAMES=1
TAB_FAVORITES=2
TAB_APPS=3
TAB_RECENTS=10
TAB_EXPERT=16
current_tab=$(cat /tmp/state.json | grep "\"type\":" | sed -e 's/^.*:\s*//g' | sed -e 's/\s*,$//g' | xargs | awk '{ print $2 }')
# For the scripts started from here: the Scraper offers "Scrape all
# favorites" in its main menu when opened from the Favorites tab.
export glo_tab="$current_tab"

cmd=$(cat ./cmd_to_run.sh 2> /dev/null)
rompath=""
emupath=""
emulabel=""
system_label=""
launcher_core=""
launch_path=""
romtype=$ROM_TYPE_UNKNOWN
romext=""
romroot=""
romcfgpath=""
default_core=""
retroarch_core=""
corepath=""
coreinfopath=""
coreinfo=""
corename=""
manpath=""

menu_options=""
menu_option_labels=""
menu_option_args=""
menu_option_info=""

main() {
    
    if [ ! -f ./cmd_to_run.sh ]; then
        log "cmd_to_run.sh not found"
        exit 1
    fi

    # Defer the Recents rewrite where possible. DynamicLabel romscripts that
    # inspect Recents will trigger it early so they see the original state.
    recents_removed=0
    if [ ! -f "$sysdir/config/.showRecents" ]; then
        currentrecentlist=$recentlist_hidden
    else
        currentrecentlist=$recentlist
    fi

    if [ $current_tab -eq $TAB_GAMES ]; then
        log "tab: games"
    elif [ $current_tab -eq $TAB_FAVORITES ]; then
        log "tab: favorites"
    elif [ $current_tab -eq $TAB_APPS ]; then
        log "tab: apps"
    elif [ $current_tab -eq $TAB_RECENTS ]; then
        log "tab: recents"
    elif [ $current_tab -eq $TAB_EXPERT ]; then
        log "tab: expert"
    fi

    if [ $current_tab -eq $TAB_APPS ]; then
        # No menu for apps, but the entry the launcher just added to
        # Recents goes, as before.
        remove_recent_entry
        exit
    fi

    emupath=$(dirname $(echo "$cmd" | awk '{ gsub(/"/, "", $2); st = index($2,".."); if (st) { print substr($2,0,st) } else { print $2 } }'))

    if [ "$emupath" == "/mnt/SDCARD/App" ]; then
        romtype=$ROM_TYPE_APP
    else
        rompath=$(echo "$cmd" | awk '{ st = index($0,"\" \""); print substr($0,st+3,length($0)-st-3) }')

        if echo "$rompath" | grep -q ":"; then
            rompath=$(echo "$rompath" | awk '{st = index($0,":"); print substr($0,st+1)}')
        fi

        export cookie_rom_path="$rompath"

        if check_is_game "$rompath"; then
            romtype=$ROM_TYPE_GAME
        fi

        _romfile=${rompath##*/}
        romext=${_romfile##*.}
        case $romext in
            *[A-Z]*) romext=$(printf '%s' "$romext" | tr '[:upper:]' '[:lower:]') ;;
        esac

        emu_config_json=$(cat "$emupath/config.json")
        romroot="$emupath/$(get_json_value "$emu_config_json" "rompath")"
        emulabel="$(get_json_value "$emu_config_json" "label" | sed -e 's/^\s*//g' -e 's/\s*$//g')"
        system_label=$emulabel
        launch_path="$emupath/launch.sh"
    fi

    if [ $current_tab -eq $TAB_FAVORITES ]; then
        emulabel="Favorites"
    elif [ $current_tab -eq $TAB_RECENTS ]; then
        emulabel="Recents"
    fi

    log "cmd: $cmd"
    # example: LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so "/mnt/SDCARD/Emu/GBATEST/../../.tmp_update/proxy.sh" "/mnt/SDCARD/Emu/GBATEST/../../Roms/GBATEST/mGBA/Final Fantasy IV Advance (U).zip"
    log "romtype: $romtype"
    log "rompath: $rompath"
    # example: "/mnt/SDCARD/Emu/GBATEST/../../Roms/GBATEST/mGBA/Final Fantasy IV Advance (U).zip"
    log "romext: $romext"

    log "emupath: $emupath"
    log "emulabel: $emulabel"

    log "romroot: $romroot"
    log "launch: $launch_path"

    log "has_networking: $has_networking"

    skip_game_options=0

    if [ ! -f "$rompath" ] || [ "$romext" == "miyoocmd" ] || [ $romtype -ne $ROM_TYPE_GAME ]; then
        skip_game_options=1
    fi

    if [ $skip_game_options -eq 0 ]; then
        get_core_info

        game_core_label="Game core"

        if [ ! -f "$radir/cores/$default_core.so" ]; then
            default_core=""
        fi

        add_reset_core=0

        if [ -f "$corepath" ] && [ -f "$coreinfopath" ]; then
            coreinfo=$(cat "$coreinfopath")
            corename=$(get_info_value "$coreinfo" corename)

            game_core_label="Game core: $corename"

            if [ "$retroarch_core" == "$default_core" ]; then
                game_core_label="$game_core_label (Default)"
            else
                add_reset_core=1
            fi

            glo_shown Reset && add_menu_option reset_game "Reset game" "Starts the game without\nloading the auto save state"
        fi

        romdirname=${rompath##*Roms/}
        romdirname=${romdirname%%/*}
        romname=${_romfile%."$romext"}
        manpath="/mnt/SDCARD/Roms/$romdirname/Manuals/$romname.pdf"

        log "romdirname: $romdirname"
        log "manpath: $manpath"

        if [ -f "$manpath" ] && glo_shown Manual; then
            add_menu_option open_manual "Game manual" "Opens the manual for this game"
        fi

        if glo_shown Core; then
            add_menu_option change_core "$game_core_label" "Change the RetroArch core\nused to run this game"
        fi

        if [ $add_reset_core -eq 1 ] && glo_shown Core; then
            add_menu_option reset_core "Restore default core" "Restores the default core\nfor this rom"
        fi

        # Add "Migrate mGBA Save" option dynamically
        if [ "$retroarch_core" == "gpsp_libretro" ] && [ -f "$save_dir/mGBA/$romname.srm" ]; then
            add_menu_option transfer_mgba_save "Transfer mGBA save" \
                "Transfer mGBA save to gpSP.\nConfirmation will be requested if a\nnon-empty save already exists." \
                "$romname"
        fi
    fi # skip_game_options

    # Hide game: Games and Expert list the files of a folder, so the game is
    # moved into .hidden beside it (names starting with a dot are not listed).
    if [ $current_tab -eq $TAB_GAMES ] || [ $current_tab -eq $TAB_EXPERT ]; then
        if [ $skip_game_options -eq 0 ] && glo_shown Hide; then
            add_menu_option hide_game "Hide game" "Hides this game from the list.\nIts saves are kept: bring it back\nfrom Hidden games."
        fi
        if [ -n "$rompath" ]; then
            hidden_count=$(count_hidden_games "${rompath%/*}/.hidden")
            if [ "$hidden_count" -gt 0 ]; then
                add_menu_option show_hidden_games "Hidden games ($hidden_count)" "Brings a hidden game of\nthis folder back to the list"
            fi
        fi
    fi

    if [ $current_tab -eq $TAB_GAMES ]; then
        add_filter_options
    fi

    # No "Refresh list" here: Open MainUI's Select menu has "Refresh roms" for
    # the same list, and a filter it drops with the cache is noticed above.

    add_script_files "$globalscriptdir"

    if [ $current_tab -eq $TAB_GAMES ]; then
        add_script_files "$globalscriptdir/emu"
    elif [ $current_tab -eq $TAB_EXPERT ]; then
        add_script_files "$globalscriptdir/rapp"
    elif [ $current_tab -eq $TAB_FAVORITES ]; then
        add_script_files "$globalscriptdir/favorites"
    elif [ $current_tab -eq $TAB_RECENTS ]; then
        add_script_files "$globalscriptdir/recents"
    fi

    if [ $current_tab -eq $TAB_GAMES ] || [ $current_tab -eq $TAB_EXPERT ]; then
        add_script_files "$emupath/romscripts"
    fi

    # Nothing to list (entries hidden in Tweaks > List menus, or none that
    # applies): one entry that says where hidden ones come back, instead of
    # an empty menu.
    if [ -z "$menu_options" ]; then
        add_menu_option : "Nothing to show" "Hidden entries come back in Tweaks >\nGame lists > List menus > GLO"
    fi

    log "\n\n=================================================================================================="
    
    # Display menu and get user selection
    show_prompt
    retcode=$?

    # Keep the original Recents semantics without blocking the first frame of
    # the GLO menu on a potentially slow SD-card file rewrite.
    remove_recent_entry

    log "\n\n=================================================================================================="
    log "retcode: $retcode"

    # Execute chosen action
    menu_action=$(get_item "$menu_options" $retcode)
    menu_arg=$(get_item "$menu_option_args" $retcode)
    runcmd="$menu_action $menu_arg"
    log "> $runcmd"
    log "\n\n==================================================================================================\n\n"
    eval $runcmd
    log "\n\n=================================================================================================="

    rm -f ./cmd_to_run.sh 2> /dev/null

    exit 1
}

# Real newline used when decoding descriptions (which store literal "\n").
_glo_sep=$(printf '\037')
_glo_nl='
'

show_prompt() {
    # POSIX shell positional arguments are an array: no generated script,
    # chmod, eval, temp file or per-description sed processes are required.
    set --
    while IFS= read -r _glo_label; do
        set -- "$@" "$_glo_label"
    done <<EOF
$menu_option_labels
EOF

    while IFS= read -r _glo_info; do
        _glo_rest=$_glo_info
        _glo_decoded=
        while :; do
            case $_glo_rest in
                *\\n*)
                    _glo_decoded=$_glo_decoded${_glo_rest%%\\n*}$_glo_nl
                    _glo_rest=${_glo_rest#*\\n}
                    ;;
                *)
                    _glo_decoded=$_glo_decoded$_glo_rest
                    break
                    ;;
            esac
        done
        set -- "$@" "$_glo_decoded"
    done <<EOF
$menu_option_info
EOF

    LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so ./bin/prompt -t "Game options" -i "$@"
}

check_is_game() {
    case $1 in
        *retroarch* | */../../Roms/*) return 0 ;;
        *) return 1 ;;
    esac
}

remove_recent_entry() {
    [ "$recents_removed" -eq 0 ] || return 0
    recents_removed=1
    [ -f "$currentrecentlist" ] || return 0
    IFS= read -r firstrecententry < "$currentrecentlist" || :
    sed '1d' "$currentrecentlist" > "$recentlist_temp" &&
        mv "$recentlist_temp" "$currentrecentlist"
}

add_game_to_recent_list() {
    echo "$firstrecententry" | cat - "$currentrecentlist" > "$recentlist_temp"
    mv "$recentlist_temp" "$currentrecentlist"
}

# Read a romscript once. Outputs networking flag, label and description on
# separate lines, using the same metadata conventions as get_info_value.
script_metadata() {
    awk '
        function val(s,   a,n,v) {
            n = split(s, a, /[[:space:]]*=[[:space:]]*/)
            if (n < 2) return ""
            v = a[2]
            gsub(/["\r]/, "", v)
            return v
        }
        /require_networking([^[:alnum:]_]|$)/ { network = network val($0) }
        /scriptlabel([^[:alnum:]_]|$)/       { label = label val($0) }
        /scriptinfo([^[:alnum:]_]|$)/        { info = info val($0) }
        END { printf "%s\037%s\037%s", network, label, info }
    ' "$1"
}

# Replace %LIST% using shell string operations, not a sed process per script.
expand_script_label() {
    _glo_left=$1
    _glo_label_result=
    while :; do
        case $_glo_left in
            *%LIST%*)
                _glo_label_result=$_glo_label_result${_glo_left%%\%LIST\%*}$emulabel
                _glo_left=${_glo_left#*%LIST%}
                ;;
            *)
                _glo_label_result=$_glo_label_result$_glo_left
                break
                ;;
        esac
    done
}

add_script_files() {
    scriptdir=$1
    [ -d "$scriptdir" ] || return 0

    for entry in "$scriptdir"/*.sh; do
        [ -f "$entry" ] || continue
        scriptname=${entry##*/}
        scriptname=${scriptname%.sh}

        # Netplay and Scraper can be hidden in Tweaks (List menus).
        case $scriptname in
            Netplay) glo_shown Netplay || continue ;;
            Scraper) glo_shown Scraper || continue ;;
        esac

        _glo_meta=$(script_metadata "$entry")
        _glo_net=${_glo_meta%%"$_glo_sep"*}
        _glo_meta=${_glo_meta#*"$_glo_sep"}
        scriptlabel=${_glo_meta%%"$_glo_sep"*}
        scriptinfo=${_glo_meta#*"$_glo_sep"}

        log "$entry"
        log "require_networking: $_glo_net"
        if [ "$has_networking" -eq 0 ] && [ "$_glo_net" = 1 ]; then
            continue
        fi

        if [ -z "$scriptlabel" ]; then
            scriptlabel=$scriptname
        elif [ "$scriptlabel" = DynamicLabel ]; then
            # A dynamic label may consult Recents. Preserve the state that the
            # original GLO gave these scripts before running one.
            remove_recent_entry
            # DynamicLabel scripts remain supported unchanged; their execution
            # may itself be a significant part of startup time.
            "$entry" "$rompath" "$emupath" "DynamicLabel" "$emulabel" "$retroarch_core" "$romdirname" "$romext"
            IFS= read -r scriptlabel < /tmp/DynamicLabel.tmp || :
            rm -f /tmp/DynamicLabel.tmp
        fi

        expand_script_label "$scriptlabel"
        scriptlabel=$_glo_label_result

        if [ -z "$scriptinfo" ]; then
            scriptinfo="Runs the script:\n$scriptlabel"
        fi

        if [ "$scriptlabel" != none ]; then
            log "adding romscript: $scriptlabel"
            add_menu_option run_script "$scriptlabel" "$scriptinfo" "$entry"
        fi
    done
}

add_menu_option() {
    action=$1
    label=$2
    info=$3
    args=$4

    # Entries are separated by real newlines. Description "\n" sequences are
    # left untouched here; show_prompt decodes them without spawning sed.
    if [ -z "$menu_options" ]; then
        menu_options=$action
        menu_option_labels=$label
        menu_option_args="\"$args\""
        menu_option_info=$info
    else
        menu_options="$menu_options
$action"
        menu_option_labels="$menu_option_labels
$label"
        menu_option_args="$menu_option_args
\"$args\""
        menu_option_info="$menu_option_info
$info"
    fi
}

get_item() {
    content="$1"
    index="$2"
    # Convert escaped newlines (\\n) back into real lines and extract the correct line
    echo -e "$content" | sed "$((index + 1))q;d"
}

get_core_info() {
    retroarch_core=""

    romcfgpath="${rompath%/*}/.game_config/${_romfile%."$romext"}.cfg"

    if [ -f "$romcfgpath" ]; then
        romcfg=$(cat "$romcfgpath")
        retroarch_core=$(get_info_value "$romcfg" core)
    fi

    if grep -q "default_core=" "$launch_path"; then
        default_core="$(cat "$launch_path" | grep "default_core=" | head -1 | awk '{split($0,a,"="); print a[2]}' | xargs)_libretro"
    else
        default_core=$(cat "$launch_path" | grep ".retroarch/cores/" | awk '{st = index($0,".retroarch/cores/"); s = substr($0,st+17); st2 = index(s,".so"); print substr(s,0,st2-1)}' | xargs)
    fi

    # The core chosen for all games of this system (Game core > All games),
    # in place of the launcher's own, while it is installed.
    launcher_core=$default_core
    system_core=$(system_default_core "$emupath")
    if [ -n "$system_core" ] && [ -f "$radir/cores/$system_core.so" ]; then
        default_core=$system_core
    fi

    if [ "$retroarch_core" == "" ]; then
        retroarch_core="$default_core"
    fi

    log "launcher_core: $launcher_core"
    log "default_core: $default_core"
    log "retroarch_core: $retroarch_core"

    corepath="$radir/cores/$retroarch_core.so"
    coreinfopath="$radir/cores/$retroarch_core.info"

    export cookie_core_path="$corepath"
}

get_info_value() {
    echo "$1" | grep "$2\b" | awk '{split($0,a,/\s*=\s*/); print a[2]}' | tr -d '"' | tr -d '\n' | tr -d '\r'
}

get_json_value() {
    echo "$1" | grep "\"$2\"\s*:" | awk '{split($0,a,/\s*:\s*/); print a[2]}' | awk -F'"' '{print $2}' | tr -d '\n' | tr -d '\r'
}

reset_game() {
    log ":: reset_game $*"
    touch /tmp/reset_game
    add_game_to_recent_list
    exit 0
}

open_manual() {
    log ":: open_manual $*"
    if [ -f "$manpath" ]; then
        LD_LIBRARY_PATH="$sysdir/lib/parasyte:$LD_LIBRARY_PATH" $sysdir/bin/green/green "$manpath"
    fi
}

change_core() {
    log ":: change_core $*"
    ext="$romext"
    is_archive=""

    if [ "$ext" == "zip" ] || [ "$ext" == "7z" ]; then
        is_archive="$ext"

        if ! cat "$emupath/config.json" | grep -q "\"shortname\"\s*:\s*1"; then
            if [ "$ext" == "zip" ]; then
                zip_files=$(unzip -l "$rompath" | sed '1,3d;$d' | sed '$d' | sort -n -r)
            else
                zip_files=$(./bin/7z l -ba "$rompath" | awk '{$1="";$2="";$3="";print $0;}' | sort -n -r)
            fi

            log "zip/7z output:"
            log "$zip_files"

            inner_name=$(basename "$(echo "$zip_files" | grep "[!]")")
            if [ "$inner_name" == "" ]; then
                inner_name=$(basename "$(echo "$zip_files" | head -n 1)")
            fi
            ext=$(echo "$inner_name" | awk -F. '{print tolower($NF)}')
            log "inner extension: $ext"
            log "-------------------------------------"

        fi
    fi

    get_core_extensions
    available_corenames=""
    available_cores=""

    count=0
    is_valid=0
    selected_index=0

    while [ $is_valid -eq 0 ]; do
        single_ext_cache_path="$radir/cores/cache/ext_cores_$ext.cache"

        if [ ! -f "$single_ext_cache_path" ]; then
            while read entry; do
                tmp_extensions=$(echo "$entry" | awk '{split($0,a,";"); print a[3]}')

                if ! echo "$tmp_extensions" | tr '|' '\n' | grep -q "$ext"; then
                    continue
                fi

                echo "$entry" >> $single_ext_cache_path
            done < $ext_cache_path
        fi

        if [ "$default_core" == "" ]; then
            is_valid=1
        fi

        while read entry; do
            tmp_corename=$(echo "$entry" | awk '{split($0,a,";"); print a[1]}')
            tmp_core=$(echo "$entry" | awk '{split($0,a,";"); print a[2]}')

            if [ "$tmp_core" == "$default_core" ]; then
                is_valid=1
                tmp_corename="$tmp_corename (Default)"
            fi

            if [ "$tmp_core" == "$retroarch_core" ]; then
                selected_index=$count
            fi

            count=$(($count + 1))

            if [ $count -ge 100 ]; then
                break
            fi

            available_cores="$available_cores $tmp_core"
            available_corenames="$available_corenames \"$tmp_corename\""
        done < $single_ext_cache_path

        if [ "$is_archive" == "" ]; then
            break
        fi

        if [ $is_valid -eq 0 ]; then
            ext="$is_archive"
            is_archive=""
        fi
    done

    log "cores: $available_cores"
    log "corenames: $available_corenames"

    if [ $is_valid -eq 0 ]; then
        infoPanel --title "Game core (.$ext)" --message "Not available for this game\n(.$ext files)" --auto
        exit 1
    fi

    runcmd="LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so ./bin/prompt -t \"Game core (.$ext)\" -s $selected_index $available_corenames"
    eval $runcmd
    retcode=$?

    if [ $retcode -lt 0 ] || [ $retcode -ge $count ]; then
        exit 1
    fi

    new_core=$(echo $available_cores | awk -v N=$((retcode + 1)) '{print $N}')

    log "new default core: $new_core"

    # For this game only, or as the default of every game of the system.
    # A core chosen for a single game keeps winning over the system one.
    LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so ./bin/prompt -t "Game core" \
        "Only this game" "All ${system_label:-these} games"
    scope=$?
    if [ $scope -eq 1 ]; then
        set_system_default_core "$emupath" "$new_core" "$launcher_core"
        reset_core # this game follows the system default
        return
    elif [ $scope -ne 0 ]; then
        exit 1
    fi

    if [ "$new_core" == "$default_core" ]; then
        reset_core
    else
        if [ -f "$romcfgpath" ]; then
            awk '!/core /' "$romcfgpath" > temp && mv temp "$romcfgpath"
        else
            mkdir -p "$(dirname "$romcfgpath")"
        fi

        echo "core = \"$new_core\"" >> "$romcfgpath"
    fi
}

# File holding the core chosen for all games of a system, kept in the config
# folder so updates (which reinstall the emulator packages) leave it alone.
# Same helper in runtime.sh, which reads it at launch.
system_core_file() { # emulator folder (/mnt/SDCARD/Emu/GBA)
    _sc_key=${1#/mnt/SDCARD/}
    _sc_key=$(echo "${_sc_key%/}" | tr '/' '-')
    echo "$sysdir/config/system_cores/$_sc_key"
}

system_default_core() { # emulator folder: the core, or nothing
    _sc_file=$(system_core_file "$1")
    [ -f "$_sc_file" ] || return 0
    head -n 1 "$_sc_file" | tr -d ' \r\n'
}

set_system_default_core() { # emulator folder, core, the launcher's own core
    _sc_file=$(system_core_file "$1")
    if [ -z "$2" ] || [ "$2" = "$3" ]; then
        rm -f "$_sc_file" # back to the launcher's default
    else
        mkdir -p "$(dirname "$_sc_file")"
        echo "$2" > "$_sc_file"
    fi
}

reset_core() {
    if [ -f "$romcfgpath" ]; then
        rm -f "$romcfgpath"
        rm -d "$(dirname "$romcfgpath")"
    fi
}

get_core_extensions() {
    if [ ! -f "$ext_cache_path" ]; then
        ./bin/infoPanel --title "Game core" --message "Reading the cores...\n \nThis may take a minute." --persistent &
        ./script/build_ext_cache.sh "$radir"
        touch /tmp/dismiss_info_panel
        sync
    fi
}

rename_rom() {
    log ":: rename_rom $*"
    prev_name="$(basename "$rompath" ".$romext")"
    runcmd="LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so ./bin/kbinput -i \"$prev_name\" -t \"RENAME ROM\""

    eval $runcmd > temp
    retcode=$?

    kboutput=$(cat temp | tail -1)
    rm -f temp

    log "kb retcode: $retcode"

    if [ $retcode -ne 0 ]; then
        return
    fi

    new_name="$kboutput"

    log "rename: '$prev_name' -> '$new_name'"

    cd $sysdir
    ./bin/renameRom "$rompath" "$new_name"
}

# ---- Hide game ----
# A hidden game goes, with its files, into <its folder>/.hidden/<rom name>/:
# Open MainUI (as stock) does not list names starting with a dot, the names
# stay the same (so do its saves and box art), and each game keeps its files
# together, so bringing it back puts exactly them back.

# The game's files: the rom and, for a .cue or a .gdi, the tracks it names
# in the same folder (left beside the playlist, a .bin would be listed).
glo_game_files() { # rom path
    echo "$1"
    _gf_dir=$(dirname "$1")
    case "$(echo "${1##*.}" | tr 'A-Z' 'a-z')" in
        cue) sed -n 's/^[[:space:]]*FILE[[:space:]]*"\([^"]*\)".*/\1/p' "$1" ;;
        gdi) sed '1d' "$1" | awk '{ if (index($0, "\"")) { split($0, q, "\""); print q[2] } else if (NF >= 5) print $5 }' ;;
    esac | tr -d '\r' | while IFS= read -r _gf_track; do
        case "$_gf_track" in
            "" | */* | "$(basename "$1")") ;;
            *) [ -f "$_gf_dir/$_gf_track" ] && echo "$_gf_dir/$_gf_track" ;;
        esac
    done
}

count_hidden_games() { # .hidden folder
    _ch_n=0
    for _ch_d in "$1"/*/; do
        [ -d "$_ch_d" ] && _ch_n=$((_ch_n + 1))
    done
    echo $_ch_n
}

# A hidden game leaves Recents (both lists): its entry would start a file
# that is no longer there. Matched by its path as given and as resolved
# (Recents may hold either form). Favorites keep it: it works again once the
# game is back, at the same path.
drop_from_recents() { # rom path
    _dr_real="$(cd "$(dirname "$1")" 2> /dev/null && pwd -P)/$(basename "$1")"
    for _dr_list in "$recentlist" "$recentlist_hidden"; do
        [ -f "$_dr_list" ] || continue
        awk -v a="$1" -v b="$_dr_real" '{
            v = $0
            if (match(v, /"rompath"[ \t]*:[ \t]*"[^"]*"/)) {
                v = substr(v, RSTART, RLENGTH)
                sub(/^"rompath"[ \t]*:[ \t]*"/, "", v)
                sub(/"$/, "", v)
                if (v == a || v == b) next
            }
            print
        }' "$_dr_list" > "$recentlist_temp" && mv "$recentlist_temp" "$_dr_list"
    done
}

# The rom list is rebuilt the next time it is opened.
drop_rom_list_cache() {
    [ -n "$romroot" ] || return 0
    rm -f "$romroot/$(basename "$romroot")_cache6.db"
}

hide_game() {
    log ":: hide_game $rompath"
    _hg_name=$(basename "$rompath")
    LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so ./bin/prompt -t "Hide game" \
        "Hide ${_hg_name%.*}" "Cancel"
    [ $? -eq 0 ] || exit 1
    _hg_dir="$(dirname "$rompath")/.hidden/${_hg_name%.*}"
    if [ -e "$_hg_dir" ]; then
        infoPanel --title "Hide game" --message "A hidden game with this name\nis already there." --auto
        return
    fi
    if ! mkdir -p "$_hg_dir"; then
        infoPanel --title "Hide game" --message "Could not create the hidden folder." --auto
        return
    fi
    # The whole list first: the .cue is read for its tracks before it moves.
    _hg_files=$(glo_game_files "$rompath")
    echo "$_hg_files" | while IFS= read -r _hg_file; do
        [ -n "$_hg_file" ] && mv -n -- "$_hg_file" "$_hg_dir/"
    done
    drop_from_recents "$rompath"
    sync
    log "hidden in $_hg_dir: $(ls "$_hg_dir" | tr '\n' '|')"
    drop_rom_list_cache
}

show_hidden_games() {
    log ":: show_hidden_games"
    _sh_root="$(dirname "$rompath")/.hidden"
    set --
    for _sh_d in "$_sh_root"/*/; do
        [ -d "$_sh_d" ] && set -- "$@" "$(basename "$_sh_d")"
    done
    [ $# -gt 0 ] || exit 1
    LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so ./bin/prompt -t "Hidden games" "$@"
    _sh_i=$?
    [ $_sh_i -lt $# ] || exit 1
    shift $_sh_i
    unhide_game "$_sh_root/$1"
}

unhide_game() { # .hidden/<rom name> folder
    log ":: unhide_game $1"
    _ug_dest=$(dirname "$(dirname "$1")")
    for _ug_f in "$1"/* "$1"/.[!.]*; do
        [ -e "$_ug_f" ] || continue
        if [ -e "$_ug_dest/$(basename "$_ug_f")" ]; then
            infoPanel --title "Hidden games" --message "$(basename "$_ug_f")\nis already in the list folder." --auto
            return
        fi
    done
    for _ug_f in "$1"/* "$1"/.[!.]*; do
        [ -e "$_ug_f" ] && mv -n -- "$_ug_f" "$_ug_dest/"
    done
    rmdir "$1" 2> /dev/null
    rmdir "$(dirname "$1")" 2> /dev/null
    sync
    drop_rom_list_cache
}

# Filter list, or Clear filter and Current filter while one is set. Tweaks >
# List menus > GLO > "Filter list" Off (config/.hideGloFilter)
# hides Filter list and Current filter; Clear filter stays while a filter is
# set, so a list filtered before is never stuck that way.
add_filter_options() {
    if [ -f "$emupath/active_filter" ] && ! filter_is_applied; then
        log "rom list cache rebuilt since the filter was set: dropping active_filter"
        rm -f "$emupath/active_filter"
    fi
    if [ -f "$emupath/active_filter" ]; then
        filter_kw=$(cat "$emupath/active_filter")
        add_menu_option clear_filter "Clear filter" "Clear the current rom list filter"
        if glo_shown Filter; then
            add_menu_option filter_roms "Current filter: $filter_kw" "Change the rom list filter"
        fi
    elif glo_shown Filter; then
        add_menu_option filter_roms "Filter list" "Filter the rom list by keyword"
    fi
}

clear_filter() {
    log ":: clear_filter $*"
    log "./bin/filter clear_filter \"$emupath\""
    filter clear_filter "$emupath"
    filter_drop_rows clear
}

filter_roms() {
    log ":: filter_roms $*"
    log "./bin/filter filter \"$emupath\""
    filter filter "$emupath"
    # set, or left as it was; an empty keyword or no filter: cleared
    if [ -f "$emupath/active_filter" ]; then
        filter_drop_rows filter
    else
        filter_drop_rows clear
    fi
}

# The filter tool puts rows of its own in the game list: "~Filter" and
# "~Refresh roms" (put back each time a filter is cleared), "~Clear
# filter" while one is set. They are commands of the old "List shortcuts"
# app, which RedOnion no longer has (UI review, 4.5.1): without it they
# open nothing, and Refresh roms is in the SELECT menu. They are dropped
# from the rom list cache; set, the filter keeps its "~Filter: keyword"
# row, which marks it (filter_is_applied), and GLO's Clear filter clears
# it. With the app still installed (its ~Filter.miyoocmd there), nothing
# is dropped.
filter_drop_rows() { # clear | filter
    [ -n "$romroot" ] || return 0
    [ -f "$romroot/~Filter.miyoocmd" ] && return 0
    cache_name=$(basename "$romroot")
    cache_db="$romroot/${cache_name}_cache6.db"
    [ -f "$cache_db" ] || return 0
    cache_table=$(echo "${cache_name}_roms" | sed 's/"/""/g')
    if [ "$1" = filter ]; then
        rows="disp = '~Refresh roms' OR disp = '~Clear filter'"
        rm -f "$romroot/~Clear filter.miyoocmd"
    else
        rows="disp = '~Filter' OR disp = '~Refresh roms' OR disp = '~Clear filter'"
    fi
    sqlite3 "$cache_db" "DELETE FROM \"$cache_table\" WHERE path LIKE '%.miyoocmd' AND ($rows);" 2> /dev/null
}

# A filter is kept in two places: the keyword in $emupath/active_filter and
# the hidden rows in the rom list cache, which also gets a "~Filter: keyword"
# entry. Rebuilding the cache (Refresh roms in the launcher's Select menu)
# shows every game again but leaves active_filter behind, and the menu kept
# offering "Clear filter" for a filter that was gone.
filter_is_applied() {
    [ -n "$romroot" ] || return 0
    cache_name=$(basename "$romroot")
    cache_db="$romroot/${cache_name}_cache6.db"
    [ -f "$cache_db" ] || return 1
    cache_table=$(echo "${cache_name}_roms" | sed 's/"/""/g')
    # On any sqlite3 error the filter is kept: better a stale entry than
    # forgetting one that still hides games.
    filter_rows=$(sqlite3 "$cache_db" "SELECT count(*) FROM \"$cache_table\" WHERE disp LIKE '~Filter: %';" 2> /dev/null) || return 0
    [ "$filter_rows" != "0" ]
}

run_script() {
    log ":: run_script $*"
    scriptpath="$1"
    if [ -f "$scriptpath" ]; then
        cmd_before=$(cat "$sysdir/cmd_to_run.sh" 2> /dev/null)
        cd "$emupath"
        chmod a+x "$scriptpath"
        LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so "$scriptpath" "$rompath" "$emupath"
        retcode=$?
        # Status 0 starts the selected game, as Reset game does: put back the
        # Recents entry taken out above, or the GameSwitcher shows this game's
        # picture under the previous game's title. A script that chose another
        # game (Random game) wrote its own command and its own entry.
        if [ $retcode -eq 0 ] && [ -n "$firstrecententry" ] &&
            [ "$(cat "$sysdir/cmd_to_run.sh" 2> /dev/null)" = "$cmd_before" ]; then
            add_game_to_recent_list
        fi
        exit $retcode
    else
        infoPanel --title "Script not found" --message "$(basename "$scriptpath")" --auto
    fi
}

transfer_mgba_save() {
    log ":: transfer_mgba_save $*"
    romname="$1"

    # Call the external transfer script, prompting user if needed
    $sysdir/script/transfer_mgba_save.sh "$romname"

    if [ $? -eq 0 ]; then
        log "Transfer successful, marking migration as done for ROM: $romname"

        # Remove rom from migration needed list
        if [ -f "$gpsp_migration_log" ]; then
            sed -i "/^$romname$/d" "$gpsp_migration_log"
        fi

        # Show info panel
        infoPanel --title "Save transferred" --message "mGBA save transferred to gpSP." --auto
    fi
}

main
