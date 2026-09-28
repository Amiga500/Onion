#!/bin/sh
echo $0 $*
sysdir=/mnt/SDCARD/.tmp_update

infoPanel -t "AdvanceMENU" -m "LOADING" --persistent &
touch /tmp/dismiss_info_panel
sync

# avoid running two advmenu instances at once (SDL limitation on Miyoo Mini)
if pgrep advmenu > /dev/null; then
    echo "AdvanceMENU is already running, not starting a second instance"
    exit 0
fi

cd $sysdir/bin/adv
./run_advmenu.sh
