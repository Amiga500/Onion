#!/bin/sh
progdir=`dirname "$0"`
sysdir=/mnt/SDCARD/.tmp_update
homedir=/mnt/SDCARD/BIOS

rm "$sysdir/cmd_to_run.sh" 2> /dev/null

echo "Running advancemenu now !"

cd $progdir

# Run advmenu and capture exit code
HOME=$homedir ./advmenu
advmenu_exit=$?

# Log any non-zero exit codes
if [ $advmenu_exit -ne 0 ]; then
    echo "advmenu exited with code: $advmenu_exit" >> /tmp/advmenu_error.log
    date >> /tmp/advmenu_error.log
fi

(sleep 0.5 && echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable) &

if [ -f /tmp/quick_switch ]; then
    touch /tmp/run_advmenu
fi
