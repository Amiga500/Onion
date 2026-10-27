#!/bin/sh
cd /mnt/SDCARD/RApp/SCUMMVM
scriptinfo="Scans and imports games\nfor ScummVM."

./import_scummvm.sh

# Non-zero: GLO goes back to the list. Zero would start the selected game.
exit 1
