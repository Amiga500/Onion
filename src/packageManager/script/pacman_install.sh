#!/bin/sh
# pacman_install.sh <data dir> <package>: copy a package's files to the SD
# card, keeping the label and imgpath the user gave its config.json.
# Exits non-zero when a copy failed (SD card full...), so the Package
# Manager can say so.
datadir="$1"
package="$2"
sdcard="${PACMAN_SDCARD:-/mnt/SDCARD}"

get_json_value() {
    grep "\"$1\"\s*:" | awk '{split($0,a,/\s*:\s*/); print a[2]}' | awk -F'"' '{print $2}' | tr -d '\n'
}

# The value goes into a sed replacement: \, & and the "," delimiter are
# escaped (a label such as "Search & Find" or "Arcade, Neo Geo" used to be
# mangled, or make sed fail and the label be lost).
sed_escape() {
    printf '%s' "$1" | sed -e 's/[\\&,]/\\&/g'
}

set_json_value() {
    key="$1"
    value="$(sed_escape "$2")"
    file="$3"

    sed -i "s,\"$key\"\s*:\s*\".*\",\"$key\": \"$value\"," "$file"
}

cd "$datadir" || exit 1
[ -d "./$package" ] || exit 1

# The installed copy of the package's config.json. The name was cut with
# sed, where the package name is a pattern ("." and "[" in a name broke it).
config=$(find "./$package" -name config.json -print | head -n 1)
config="$sdcard/${config#"./$package/"}"
preserved_label=""
preserved_imgpath=""

if [ -f "$config" ]; then
    preserved_label=$(cat "$config" | get_json_value label)
    preserved_imgpath=$(cat "$config" | get_json_value imgpath)
fi

status=0
for FILE in ./"$package"/*; do
    [ -e "$FILE" ] || continue
    cp -rf "$FILE" "$sdcard/" || status=1
done

if [ -f "$config" ]; then
    if [ -n "$preserved_label" ]; then
        set_json_value label "$preserved_label" "$config"
    fi
    if [ -n "$preserved_imgpath" ]; then
        set_json_value imgpath "$preserved_imgpath" "$config"
    fi
fi

sync
exit $status
