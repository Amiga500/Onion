#!/bin/sh
scriptinfo="Opens AdvanceMENU with the\nselected emulator."

emupath="$2"
sysdir=/mnt/SDCARD/.tmp_update
advmenu_rc_path=/mnt/SDCARD/BIOS/.advance/advmenu.rc

if [ -f "$advmenu_rc_path" ]; then
    # use a unique temp file next to advmenu.rc (same filesystem, no race condition)
    temp="$advmenu_rc_path.$$"
    grep -vwE '^[[:space:]]*(emulator_include|menu_base|menu_rel)' "$advmenu_rc_path" > "$temp"
    echo "emulator_include \"$(basename "$emupath")\"" >> "$temp"
    mv -f "$temp" "$advmenu_rc_path"
fi

cd $sysdir/bin/adv
./run_advmenu.sh
