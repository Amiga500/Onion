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

        if echo `cat "$advmenu_rc"` | grep -q "emulator \"$emuname\""; then
            continue
        fi

        rel_dir="/mnt/SDCARD/$dir_name/$emuname"
        rompath=`get_json_value "$config_json" rompath`
        imgpath=`get_json_value "$config_json" imgpath`
        extlist=`get_json_value "$config_json" extlist`

        if [ "$extlist" != "" ]; then
            if echo "$extlist" | grep -q '\bmiyoocmd\b'; then
                extlist=`echo "$extlist" | sed -e 's/miyoocmd//g' -e 's/||*/|/g' -e 's/^|//g' -e 's/|$//g'`
            fi
            
            # Prioritize .m3u for multi-disc games to avoid duplicates
            # If m3u is present, remove other disc image formats that would create duplicates
            if echo "$extlist" | grep -q '\bm3u\b'; then
                # Remove disc formats that m3u replaces: cue, bin, iso, img, ccd, mdf, nrg, mds, chd
                # Keep formats that are independent: pbp, adf, adz, dms, fdi, ipf, hdf, hdz, lha, slave, info, uae, rp9, zip, 7z
                extlist=`echo "$extlist" | sed -e 's/|cue//g' -e 's/|bin//g' -e 's/|iso//g' -e 's/|img//g' -e 's/|ccd//g' -e 's/|mdf//g' -e 's/|nrg//g' -e 's/|mds//g' -e 's/|chd//g' -e 's/|toc//g' -e 's/|cbn//g' -e 's/cue|//g' -e 's/bin|//g' -e 's/iso|//g' -e 's/img|//g' -e 's/ccd|//g' -e 's/mdf|//g' -e 's/nrg|//g' -e 's/mds|//g' -e 's/chd|//g' -e 's/toc|//g' -e 's/cbn|//g' -e 's/||*/|/g' -e 's/^|//g' -e 's/|$//g'`
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
