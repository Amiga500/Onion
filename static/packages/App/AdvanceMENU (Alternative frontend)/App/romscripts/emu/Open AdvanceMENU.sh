#!/bin/sh
scriptinfo="Opens AdvanceMENU with the\nselected emulator."

emupath="$2"
sysdir=/mnt/SDCARD/.tmp_update
advmenu_rc_path=/mnt/SDCARD/BIOS/.advance/advmenu.rc

if [ -f "$advmenu_rc_path" ]; then
    # use a unique temp file next to advmenu.rc (same filesystem, no race condition)
    temp="$advmenu_rc_path.$$"
    if grep -vwE '^[[:space:]]*(emulator_include|menu_base|menu_rel)' "$advmenu_rc_path" > "$temp" \
        && echo "emulator_include \"$(basename "$emupath")\"" >> "$temp"; then
        mv -f "$temp" "$advmenu_rc_path"
    else
        rm -f "$temp"
    fi
fi

cd $sysdir/bin/adv
./run_advmenu.sh
