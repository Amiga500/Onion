#!/bin/sh
cd /mnt/SDCARD/Emu/PORTS
scriptinfo="Scans and imports games\nfrom the Ports collection."

./import.sh

# Non-zero: GLO goes back to the list. Zero would start the selected game.
exit 1
