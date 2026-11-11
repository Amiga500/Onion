#!/bin/sh
# themes_extract_theme.sh <preview dir>: extract a compact theme (a preview
# of an archive in Themes/) into Themes/<name>, and delete the archive once
# every theme in it is extracted. Exits non-zero when the theme could not be
# extracted: the Theme Switcher then keeps the current theme.
#
# 7z's result used to be ignored: an extraction that failed half-way (SD
# card full, damaged archive) left a cut theme folder, the preview was
# removed and, with the folder there, the archive was deleted too. Now the
# theme is extracted to a hidden folder first and moved into place only
# when 7z succeeded and its config.json is there.
preview_dir="$1"
theme_dir="${THEMES_DIR:-/mnt/SDCARD/Themes}"
staging="$theme_dir/.extracting"

if [ ! -f "$preview_dir/source" ]; then
    exit 2
fi

theme_name="$(basename "$preview_dir")"
archive_path="$(cat "$preview_dir/source")"

if [ ! -f "$archive_path" ]; then
    exit 2
fi

main() {
    echo "theme: $theme_name"
    echo "path: $archive_path"

    # A swap cut short (power off between the two moves below) left the
    # theme only as .<name>.old: put it back before anything else, or the
    # rm -rf "$old" below would delete the only copy.
    if [ ! -d "$theme_dir/$theme_name" ] && [ -d "$theme_dir/.$theme_name.old" ]; then
        mv "$theme_dir/.$theme_name.old" "$theme_dir/$theme_name"
    fi

    rm -rf "$staging"
    mkdir -p "$staging" || return 1

    # extract theme from compressed file
    if ! 7z x -aoa -bd -o"$staging" "$archive_path" "$theme_name/*" ||
        [ ! -f "$staging/$theme_name/config.json" ]; then
        rm -rf "$staging"
        sync
        return 1
    fi

    # Into place: an older copy of the theme is replaced whole.
    old="$theme_dir/.$theme_name.old"
    rm -rf "$old"
    if [ -d "$theme_dir/$theme_name" ] && ! mv "$theme_dir/$theme_name" "$old"; then
        rm -rf "$staging"
        return 1
    fi
    if ! mv "$staging/$theme_name" "$theme_dir/$theme_name"; then
        [ -d "$old" ] && mv "$old" "$theme_dir/$theme_name"
        rm -rf "$staging"
        return 1
    fi
    rm -rf "$old" "$staging"
    sync

    rm -rf "$theme_dir/.previews/$theme_name"

    file_list=`7z l -slt "$archive_path" "*/config.json" | grep '^Path = ' | sed 's/^Path = //g' | tail -n +2 | awk -v prefix="$theme_dir/" '$0=prefix $0'`

    md5hash "$archive_path" > "$theme_dir/$theme_name/md5hash"

    if check_should_remove "$file_list"; then
        echo "deleting: $archive_path"
        rm -f "$archive_path"
    fi
    return 0
}

check_should_remove() {
    file_list="$1"
    touch /tmp/remove_theme_archive

    echo "$file_list" | while read line; do
        output_dir="$(dirname "$line")"
        if [ ! -d "$output_dir" ]; then
            rm -f /tmp/remove_theme_archive
            break
        fi
    done

    if [ -f /tmp/remove_theme_archive ]; then
        rm -f /tmp/remove_theme_archive
        return 0
    fi

    return 1
}

md5hash() {
    echo `md5sum "$1" | awk '{ print $1; }'`
}

main
