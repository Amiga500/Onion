#!/bin/sh

# The OTA app is "RedOnion OTA update", but a package update keeps the label
# the app already has (so a name the user gave it survives): installs from
# before the rename kept "Onion OTA update" in Apps. Only that old default
# name is changed.

CONFIG=${OTA_APP_CONFIG:-/mnt/SDCARD/App/OtaUpdate/config.json}

[ -f "$CONFIG" ] || exit 0
grep -q '"label"[[:space:]]*:[[:space:]]*"Onion OTA update"' "$CONFIG" || exit 0
sed -i 's/"label"[[:space:]]*:[[:space:]]*"Onion OTA update"/"label": "RedOnion OTA update"/' "$CONFIG"
