#!/bin/sh
echo $0 $*
sysdir=/mnt/SDCARD/.tmp_update

infoPanel -t "AdvanceMENU" -m "LOADING" --persistent &
touch /tmp/dismiss_info_panel
sync

# avoid running two advmenu instances at once (SDL limitation on Miyoo Mini)
if ! pgrep advmenu > /dev/null; then
    cd $sysdir/bin/adv
    ./run_advmenu.sh
fi
