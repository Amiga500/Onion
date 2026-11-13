#!/bin/sh
export sysdir="/mnt/SDCARD/.tmp_update"
export miyoodir="/mnt/SDCARD/miyoo"
export LD_LIBRARY_PATH="/lib:/config/lib:$miyoodir/lib:$sysdir/lib:$sysdir/lib/parasyte:/sbin:/usr/sbin:/bin:/usr/bin"
export PATH="$sysdir/bin:$PATH"

# to push the default display values into mi_disp when a soft reset of mi_disp is done (if /dev/l doesn't exist for some reason)
# this script is called by `disp_init` but can be called standalone
# it will read system.json, process the values to what mi_disp expects and push them in

# --repair (for Open MainUI, when it cannot apply Settings > Display): after some games (Atari ones on the
# Mini Plus) the display is left without its colour processing: mi_disp0 has no CSC section, the colours
# are off, and Settings > Display fails ("Please reboot the device"); writing to mi_disp0 does not help.
# Set the display up again as the PICO-8 launcher does: stop /dev/l (or a disp_init that replaced it) and
# start disp_init, which calls this script to write the colours. Waits until the CSC section is back
# (at most 3 s); exit 1 if it is not. Nothing is done while it is there.
if [ "$1" = "--repair" ]; then
  node=/proc/mi_modules/mi_disp/mi_disp0
  [ -e "$node" ] || exit 0
  grep -q CscMatrix "$node" && exit 0
  echo "display: no CSC section in mi_disp0, restarting disp_init"
  killall -2 l 2> /dev/null
  killall -9 disp_init 2> /dev/null
  i=0
  while killall -0 l 2> /dev/null && [ $i -lt 20 ]; do
    sleep 0.1
    i=$((i + 1))
  done
  "$sysdir/bin/disp_init" > /dev/null 2>&1 &
  i=0
  while [ $i -lt 30 ]; do
    grep -q CscMatrix "$node" && exit 0
    sleep 0.1
    i=$((i + 1))
  done
  echo "display: CSC section still missing"
  exit 1
fi

# mi_disp doesn't accept the values raw from system.json, they're manipulated before handing off to mi_disp.
# Same formulas as the stock MainUI and Open MainUI use in Settings > Display (and runtime.sh at boot on
# the Mini Flip): lumination + 35, hue * 5, saturation * 5, (contrast + 15) * 2. They were lumination + 34
# and contrast + 40 here, so after disp_init the colours were a little off what Settings had set.

# grab the values from system.json
filename="/appconfigs/system.json"

readout=$(jq -r '.lumination, .hue, .saturation, .contrast' "$filename")

while IFS= read -r line; do
  [ -z "$lumination" ] && lumination="$line" && continue
  [ -z "$hue" ] && hue="$line" && continue
  [ -z "$saturation" ] && saturation="$line" && continue
  [ -z "$contrast" ] && contrast="$line" && continue
done <<EOF
$readout
EOF

# check if they're within bounds, if not force defaults of 7/10/10/10 like miyoo do.
if ! echo "$lumination" | grep -qE '^[0-9]+$' || [ "$lumination" -lt 0 ] || [ "$lumination" -gt 20 ]; then lumination=7; fi
if ! echo "$hue" | grep -qE '^[0-9]+$' || [ "$hue" -lt 0 ] || [ "$hue" -gt 20 ]; then hue=10; fi
if ! echo "$saturation" | grep -qE '^[0-9]+$' || [ "$saturation" -lt 0 ] || [ "$saturation" -gt 20 ]; then saturation=10; fi
if ! echo "$contrast" | grep -qE '^[0-9]+$' || [ "$contrast" -lt 0 ] || [ "$contrast" -gt 20 ]; then contrast=10; fi

# process ready for mi_disp to accept
lumaProcessed=$((lumination + 35))
satProcessed=$((saturation * 5))
contProcessed=$(((contrast + 15) * 2))
hueProcessed=$((hue * 5))

devid=0 # the mi_disp ID
cscMatrix=3 # dont change this, it's the type of csc (E_MI_DISP_CSC_MATRIX_RGB_TO_BT709_PC)
sharpness=0 # set by default to 0
gain=0 # no effect but required by the below

# check if /proc/mi_modules/mi_disp/mi_disp0 exists before trying to push in values,
# if it doesn't, fully respawn /dev/l (causes screen to reinitialise fully, which causes visual artifacts like at startup)
sync

if [ -e "/proc/mi_modules/mi_disp/mi_disp0" ]; then
  echo "csc $devid $cscMatrix $contProcessed $hueProcessed $lumaProcessed $satProcessed $sharpness $gain" > /proc/mi_modules/mi_disp/mi_disp0
else
  echo "mi_disp0 does not exist. Cannot push values."
  cat /proc/ls
  /dev/l &
fi

# echo "SetCsc command executed with the following parameters:"
# echo "Contrast: $contProcessed"
# echo "Hue: $hueProcessed"
# echo "Luma: $lumaProcessed"
# echo "Saturation: $satProcessed"
# echo "Sharpness: $sharpness"
# echo "Gain: $gain"