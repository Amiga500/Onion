#!/bin/sh

# Open MainUI 1.0.6 lists Apps in the SD card's folder order, as the stock
# MainUI does, unless .appsort exists. Create it once so Apps stay sorted by
# name as in earlier OnionPlus releases. Deleting the file switches to the
# card order, and as a one-time migration it never comes back.

config="${sysdir:-/mnt/SDCARD/.tmp_update}/config"
mkdir -p "$config"
: > "$config/.appsort"
