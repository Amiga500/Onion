#!/bin/sh
scriptinfo="Sets the emulated time\nfor the system clock."

sysdir=/mnt/SDCARD/.tmp_update
savedir=/mnt/SDCARD/Saves/CurrentProfile/saves

cd $sysdir
HOME=/mnt/SDCARD
./bin/clock

date +%s > $savedir/currentTime.txt

# Non-zero: GLO goes back to the game list. Zero would start the game.
exit 1
