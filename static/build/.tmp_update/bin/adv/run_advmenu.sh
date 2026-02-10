#!/bin/sh
progdir=`dirname "$0"`
sysdir=/mnt/SDCARD/.tmp_update
homedir=/mnt/SDCARD/BIOS
logdir=/mnt/SDCARD/.tmp_update/logs

rm "$sysdir/cmd_to_run.sh" 2> /dev/null

# Ensure log directory exists
mkdir -p "$logdir"

echo "Running advancemenu now !"

cd $progdir

# Run advmenu and capture all output to log file
echo "=== AdvanceMENU started at $(date) ===" >> "$logdir/advmenu.log"
HOME=$homedir ./advmenu >> "$logdir/advmenu.log" 2>&1
advmenu_exit=$?

# Log exit code
echo "=== AdvanceMENU exited with code: $advmenu_exit at $(date) ===" >> "$logdir/advmenu.log"
echo "" >> "$logdir/advmenu.log"

(sleep 0.5 && echo 1 > /sys/class/pwm/pwmchip0/pwm0/enable) &

if [ -f /tmp/quick_switch ]; then
    touch /tmp/run_advmenu
fi
