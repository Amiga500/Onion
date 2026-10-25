#!/bin/sh
cd /mnt/SDCARD/Emu/SCUMMVM
scriptinfo="Change how your ScummVM\ngame shortcuts are named.\n\nNo games will be scanned or imported."

./import_scummvm.sh skip_scan

# Non-zero: GLO goes back to the list. Zero would start the selected game.
exit 1
