#!/bin/bash
prog_dir=`dirname "$0"`
package_dir="$1"
advmenu_rc=`realpath "$2"`
dir_name=`basename "$package_dir"`

echo "-- Adding $dir_name configs to $advmenu_rc"

cd "$package_dir"

if [ ! -d "$package_dir" ] || [ ! -f "$advmenu_rc" ]; then
    echo "[ERROR] auto_advmenu_rc: couldn't find the necessary paths"
    exit 1
fi

get_json_value() {
    echo "$1" | grep "\"$2\"\s*:" | awk '{split($0,a,":"); print a[2]}' | awk -F'"' '{print $2}' | tr -d '\n'
}

find . -name config.json -type f -exec dirname {} \; | sort -t/ -k4 | (
    while read emupath ; do
        config_json=`cat "$emupath/config.json"`
        advcommand="$emupath/advcommand.sh"

        if [ ! -f "$advcommand" ]; then
            continue
        fi
    
        emuname=`basename "$emupath"`

        # Escape regex special characters in emuname for safe pattern matching
        emuname_escaped=$(printf '%s\n' "$emuname" | sed 's/[.[{\^$|?*+()]/\\&/g')

        # Remove any existing entries for this emulator to prevent duplicates
        if grep -q "^emulator[_a-z]* \"$emuname_escaped\"" "$advmenu_rc" 2>/dev/null; then
            # Create temporary file without the existing entries for this emulator
            # Match lines starting with emulator directives followed by the emulator name
            # Note: grep -v returns 1 if all lines match (resulting in empty output), which is valid
            grep -v "^emulator[_a-z]* \"$emuname_escaped\"" "$advmenu_rc" > "$advmenu_rc.tmp"
            grep_exit=$?
            if [ $grep_exit -eq 0 ] || [ $grep_exit -eq 1 ]; then
                # Success (0) or all lines matched/empty result (1) are both valid
                mv "$advmenu_rc.tmp" "$advmenu_rc"
            else
                # grep failed for other reasons, remove temp file and keep original
                rm -f "$advmenu_rc.tmp"
            fi
        fi

        rel_dir="/mnt/SDCARD/$dir_name/$emuname"
        rompath=`get_json_value "$config_json" rompath`
        imgpath=`get_json_value "$config_json" imgpath`
        extlist=`get_json_value "$config_json" extlist`

        if [ "$extlist" != "" ]; then
            if echo "$extlist" | grep -q '\bmiyoocmd\b'; then
                extlist=`echo "$extlist" | sed -e 's/miyoocmd//g' -e 's/||*/|/g' -e 's/^|//g' -e 's/|$//g'`
            fi
            extlist="*.${extlist//|/\:\*.}"
        fi

        # emulator "GBA" generic "/mnt/SDCARD/Emu/GBA/advcommand.sh" "%p"
        # emulator_roms "GBA" "/mnt/SDCARD/Roms/GBA"
        # emulator_roms_filter "GBA" "*.gba:*.bin:*.zip:*.7z"
        # emulator_altss "GBA" "/mnt/SDCARD/Roms/GBA/Snaps"
        # emulator_flyers "GBA" "/mnt/SDCARD/Roms/GBA/Imgs"

        echo -e "\nemulator \"$emuname\" generic \"$rel_dir/advcommand.sh\" \"%p\"" >> "$advmenu_rc"
        echo "emulator_roms \"$emuname\" \"$rel_dir/$rompath\"" >> "$advmenu_rc"

        if [ "$extlist" != "" ]; then
            echo "emulator_roms_filter \"$emuname\" \"$extlist\"" >> "$advmenu_rc"
        fi

        echo "emulator_altss \"$emuname\" \"$rel_dir/$(dirname "$imgpath")/Snaps\"" >> "$advmenu_rc"
        echo "emulator_flyers \"$emuname\" \"$rel_dir/$imgpath\"" >> "$advmenu_rc"
    done
)
