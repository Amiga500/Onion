#!/bin/sh
sysdir=/mnt/SDCARD/.tmp_update

#Load current time
timepath=/mnt/SDCARD/Saves/CurrentProfile/saves/currentTime.txt
currentTime=0
# Load current time
if [ -f $timepath ]; then
    read -r currentTime < $timepath
fi
# Numbers only: they go into $(( )) (as in runtime.sh's update_time)
case "$currentTime" in
    '' | *[!0-9]*) currentTime=0 ;;
esac
#Add 4 hours to the current time
hours=4
if [ -f $sysdir/config/startup/addHours ]; then
    read -r hours < $sysdir/config/startup/addHours
fi
case "$hours" in
    '' | - | *[!0-9-]* | ?*-*) hours=4 ;;
esac
addTime=$(($hours * 3600))
if [ ! -f $sysdir/config/.ntpState ]; then
    currentTime=$(($currentTime + $addTime))
fi
date +%s -s @$currentTime
