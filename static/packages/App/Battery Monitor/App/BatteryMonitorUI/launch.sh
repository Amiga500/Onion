#!/bin/sh
echo $0 $*
cd $(dirname "$0")
./batteryMonitorUI
# A SIGKILL during the battery test skips the app's own restore: put the CPU clock, governor and
# brightness back from what the test saved.
[ -f /tmp/batterymonitor_bt_restore ] && ./batteryMonitorUI --restore
