#!/bin/sh

# The GameSwitcher sends RetroArch GET_STATUS when it opens, and RetroArch
# answers with the core's systemid. These cores' .info files have none, and
# printing the missing value crashed RetroArch (error 139) as soon as MENU
# was pressed (#283). The RetroArch package is reinstalled only when its
# version changes, so this adds the line on existing installs. It then drops
# core_info.cache: RetroArch does not reread a .info file while it exists.

CORES_DIR=${CORES_DIR:-/mnt/SDCARD/RetroArch/.retroarch/cores}

[ -d "$CORES_DIR" ] || exit 0

while read -r core id; do
    info="$CORES_DIR/${core}_libretro.info"
    [ -f "$info" ] || continue
    grep -q '^systemid' "$info" && continue
    # A file without a final newline would glue the new line to the last one.
    [ -n "$(tail -c 1 "$info")" ] && echo >> "$info"
    echo "systemid = \"$id\"" >> "$info"
done << EOF
gw handheld_electronic
bk elektronika_bk
cannonball cannonball
easyrpg rpg_maker
freechaf fairchild_channel_f
lutro lutro
mu palm_os
neocd neo_geo_cd
reminiscence reminiscence
theodore thomson
vemulator dreamcast_vmu
x1 sharp_x1
EOF

rm -f "$CORES_DIR/core_info.cache"
