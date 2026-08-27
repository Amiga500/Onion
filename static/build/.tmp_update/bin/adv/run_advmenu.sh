#!/bin/sh
progdir=`dirname "$0"`
sysdir=/mnt/SDCARD/.tmp_update
homedir=/mnt/SDCARD/BIOS

rm $sysdir/cmd_to_run.sh 2> /dev/null

echo "Running advancemenu now !"

cd $progdir

HOME=$homedir ./advmenu

if [ -f /tmp/quick_switch ]; then
    # coming back from a game: the backlight pwm was disabled when launching it
    (sleep 0.5 && echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable) &
    touch /tmp/run_advmenu
fi
