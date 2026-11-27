#!/bin/sh

# Two app names from the UI review (4.5.1): "RedOnion OTA update" in title
# case as the other apps (Package Manager, Quick Guide...), and "Display
# Calibration" (240p Suite's test patterns, next to DisplayCal in Apps and
# easily taken for it) as "240p Test Suite". A package update keeps the
# label already installed (pacman_install.sh), so only those old default
# names are changed here; a name the user gave stays.

OTA=${OTA_APP_CONFIG:-/mnt/SDCARD/App/OtaUpdate/config.json}
SUITE=${SUITE_APP_CONFIG:-/mnt/SDCARD/App/240pSuite/config.json}

rename() { # config old new
    [ -f "$1" ] || return 0
    grep -q "\"label\"[[:space:]]*:[[:space:]]*\"$2\"" "$1" || return 0
    sed -i "s/\"label\"[[:space:]]*:[[:space:]]*\"$2\"/\"label\": \"$3\"/" "$1"
}

rename "$OTA" "RedOnion OTA update" "RedOnion OTA Update"
rename "$SUITE" "Display Calibration" "240p Test Suite"
exit 0
