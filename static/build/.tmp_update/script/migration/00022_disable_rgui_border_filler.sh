#!/bin/sh

# RetroArch menu (RGUI): Settings > User Interface > Appearance > Border
# Filler OFF, so no frame is drawn around the menu. New installs get it
# from the default retroarch.cfg; this sets it once on existing installs.

RA_CFG=${RA_CFG:-/mnt/SDCARD/RetroArch/.retroarch/retroarch.cfg}
SETTING=rgui_border_filler_enable

[ -f "$RA_CFG" ] || exit 0

if grep -q "^$SETTING = " "$RA_CFG"; then
    sed -i "s/^$SETTING = .*/$SETTING = \"false\"/" "$RA_CFG"
else
    echo "$SETTING = \"false\"" >> "$RA_CFG"
fi
