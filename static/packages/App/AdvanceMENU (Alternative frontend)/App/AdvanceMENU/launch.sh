#!/bin/sh
echo $0 $*
sysdir=/mnt/SDCARD/.tmp_update

infoPanel -t "AdvanceMENU" -m "LOADING" --persistent &
sync

cd $sysdir/bin/adv
./run_advmenu.sh

# Dismiss info panel after advmenu exits
touch /tmp/dismiss_info_panel
