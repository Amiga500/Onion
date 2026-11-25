#!/bin/sh
# OTA update demo (wip/new-ui only, not for a release): the updater's new
# screens from start to end with a made-up release. Nothing is downloaded,
# written or installed, the saved channel stays, and there is no reboot.
scriptdir=/mnt/SDCARD/.tmp_update/script

# The updater marks an update as available (.updateAvailable): the demo's
# made-up one must not stay marked.
flag=/mnt/SDCARD/.tmp_update/.updateAvailable
[ -e "$flag" ] && had_flag=1

touch /tmp/stay_awake
cd $scriptdir
OTA_NATIVE=1 OTA_DEMO=1 sh $scriptdir/ota_update.sh
rm -f /tmp/stay_awake
[ -n "$had_flag" ] || rm -f "$flag"
