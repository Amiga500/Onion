#!/bin/sh

# The paths below are relative: run them in this package's folder whatever
# the caller's (the find below would delete every file but *.sh and *.json
# of any other folder).
cd "$(dirname "$0")" || exit 1

# Cleanup
rm "/mnt/SDCARD/Roms/ADVMAME/~Run advmenu.shortcut" 2> /dev/null
rm "/mnt/SDCARD/Roms/ADVMAME/~Run advmenu.miyoocmd" 2> /dev/null
rm -rf ./.advance 2> /dev/null
rm -rf ./tools 2> /dev/null
rm ./advmame.sh 2> /dev/null
find . -type f ! -name "*.sh" ! -name "*.json" -exec rm {} \; 2> /dev/null
