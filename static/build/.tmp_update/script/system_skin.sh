#!/bin/sh
# Make the system artwork that themes cannot reach follow the theme.
#
# Usage: system_skin.sh THEME_PATH   (called by themeSwitcher on every apply)
#
# Package Manager, the Search/Filter tools, the Themes app badge, the Quick
# Guide, Activity Tracker and the File Explorer icons load fixed files, not
# theme images. A theme can ship crimson (or any) copies of those files in
# its "system" folder, using the same paths as on the SD card:
#
#   Themes/<theme>/system/App/PackageManager/res/bgApp.png
#   Themes/<theme>/system/.tmp_update/res/icon_search.png
#   Themes/<theme>/system/App/PackageManager/data/App/Quick Guide/App/Onion_Manual/page1.png
#
# Applying such a theme copies them over the system files. Applying a theme
# without a "system" folder puts Onion's originals back from
# .tmp_update/res/system-skin-default. Files under App/PackageManager/data
# are package sources: the installed copy of the app (App/Onion_Manual/...)
# is updated too, when the app is installed. A few text settings (RetroArch
# menu title, AdvanceMENU and PCSX menu accents) are switched with sed so the
# user's other settings stay untouched.

SDROOT=${SDROOT:-/mnt/SDCARD}
DEFAULTS=$SDROOT/.tmp_update/res/system-skin-default
STATE=$SDROOT/.tmp_update/config/.systemSkin

# Accent colours: Onion's original value, then the one used with a theme
# that has a "system" folder (RedOnion crimson).
RA_TITLE_ONION=0xFF9160fd
RA_TITLE_THEME=0xFFc43b59
ADVMENU_ONION=7247c2
ADVMENU_THEME=9e2c45
PCSX_ONION=8526eb
PCSX_THEME=9e2c45

# Copy every file of a skin folder to its place on the SD card. Files whose
# folder does not exist (an app that is not installed) are skipped.
copy_skin() { # skin_root
    [ -d "$1" ] || return 0
    (cd "$1" && find . -type f) | while IFS= read -r rel; do
        rel=${rel#./}
        dst="$SDROOT/$rel"
        [ -d "$(dirname "$dst")" ] && cp -f "$1/$rel" "$dst"
        case "$rel" in
        App/PackageManager/data/*/*/*)
            # data/<App|Emu|RApp>/<package name>/<path on the SD card>
            inst=${rel#App/PackageManager/data/}
            inst=${inst#*/}
            inst=${inst#*/}
            [ -d "$(dirname "$SDROOT/$inst")" ] && cp -f "$1/$rel" "$SDROOT/$inst"
            ;;
        esac
    done
    return 0
}

replace_in() { # file, sed expression
    [ -f "$1" ] && sed -i "$2" "$1"
    return 0
}

set_text_colors() { # onion | theme
    if [ "$1" = theme ]; then
        ra_from=$RA_TITLE_ONION ra_to=$RA_TITLE_THEME
        adv_from=$ADVMENU_ONION adv_to=$ADVMENU_THEME
        pcsx_from=$PCSX_ONION pcsx_to=$PCSX_THEME
    else
        ra_from=$RA_TITLE_THEME ra_to=$RA_TITLE_ONION
        adv_from=$ADVMENU_THEME adv_to=$ADVMENU_ONION
        pcsx_from=$PCSX_THEME pcsx_to=$PCSX_ONION
    fi
    replace_in "$SDROOT/RetroArch/.retroarch/assets/rgui/Onion.cfg" \
        "s/^rgui_title_color = \"$ra_from\"/rgui_title_color = \"$ra_to\"/"
    replace_in "$SDROOT/BIOS/.advance/advmenu.rc" "/^ui_color /s/ $adv_from/ $adv_to/g"
    pcsx_skin="RApp/PCSX-ReARMed/skin/skin.txt"
    replace_in "$SDROOT/App/PackageManager/data/RApp/Sony - PlayStation (PCSX standalone)/$pcsx_skin" \
        "s/^selection_color=$pcsx_from\$/selection_color=$pcsx_to/"
    replace_in "$SDROOT/$pcsx_skin" "s/^selection_color=$pcsx_from\$/selection_color=$pcsx_to/"
}

apply_system_skin() { # theme_path
    theme=${1%/}
    overlay="$theme/system"
    applied=$(cat "$STATE" 2> /dev/null)

    if [ -d "$overlay" ]; then
        # Another theme's skin was active: start again from the originals.
        [ -n "$applied" ] && [ "$applied" != "$theme" ] && copy_skin "$DEFAULTS"
        copy_skin "$overlay"
        set_text_colors theme
        mkdir -p "$(dirname "$STATE")"
        echo "$theme" > "$STATE"
    elif [ -n "$applied" ]; then
        copy_skin "$DEFAULTS"
        set_text_colors onion
        rm -f "$STATE"
    fi
    sync
}

if [ "${0##*/}" = "system_skin.sh" ]; then
    [ -n "$1" ] || {
        echo "usage: $0 THEME_PATH" >&2
        exit 1
    }
    apply_system_skin "$1"
fi
