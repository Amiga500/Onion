#!/bin/sh
scriptinfo="Sets the emulated time\nfor the system clock."

sysdir=/mnt/SDCARD/.tmp_update
savedir=/mnt/SDCARD/Saves/CurrentProfile/saves

cd $sysdir
HOME=/mnt/SDCARD
./bin/clock

date +%s > $savedir/currentTime.txt

# Non-zero: GLO goes back to the game list (as after Scraper). Zero would
# start the game the menu was opened on.
exit 1
