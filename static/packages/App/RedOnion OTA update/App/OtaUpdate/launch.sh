#!/bin/sh
scriptdir=/mnt/SDCARD/.tmp_update/script

touch /tmp/stay_awake

cd $scriptdir
# New UI: the updater's questions, messages and download are drawn natively
# (prompt, progressUI); OTA_TERMINAL=1 runs it in the terminal as before.
if [ -z "$OTA_TERMINAL" ]; then
    OTA_NATIVE=1 sh $scriptdir/ota_update.sh
else
    st -q -e sh $scriptdir/ota_update.sh
fi
