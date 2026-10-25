#!/bin/sh

# Silky RedOnion replaced Silky by DiMo as the default theme in beta 8. An
# update keeps the theme in system.json, so a device that was on the old
# default stayed on it and showed Onion's purple instead of the RedOnion
# look. Once, on update: move a device still on the old default to the new
# one. Any other theme is left alone, and choosing Silky by DiMo again in
# Themes afterwards sticks (this runs only once).
#
# The installer reapplies the theme after the migrations (themeSwitcher
# --reapply_icons, as system.json and active_theme now differ), which also
# applies the theme's icons and system skin.

SDCARD=${SDCARD:-/mnt/SDCARD}
OLD_THEME="$SDCARD/Themes/Silky by DiMo"
NEW_THEME="$SDCARD/Themes/Silky RedOnion"

[ -f "$NEW_THEME/config.json" ] || exit 0

# Only the "theme" value, keeping the file's layout. The theme path is
# written with a trailing slash; accept it without one too.
switch_theme() { # file
    [ -f "$1" ] || return 0
    grep -q "\"theme\":[[:space:]]*\"$OLD_THEME/\{0,1\}\"" "$1" || return 0
    sed "s|\(\"theme\":[[:space:]]*\"\)$OLD_THEME/\{0,1\}\"|\1$NEW_THEME/\"|" "$1" > "$1.tmp" &&
        [ -s "$1.tmp" ] && sync && mv -f "$1.tmp" "$1" || rm -f "$1.tmp"
}

switch_theme "$SDCARD/system.json"

# runtime.sh keeps a copy per device and copies it over system.json at boot
# (load_settings): without changing these too, the next boot would put the
# old theme back.
for device_settings in "$SDCARD"/.tmp_update/config/system/*.json; do
    switch_theme "$device_settings"
done

exit 0
