#!/bin/sh

# The console pictures are now trimmed of their transparent padding: Open
# MainUI scales the picture to the height it has, so the padding made the
# consoles look small (robcodedev). console_pictures.sh only copies the
# pictures that are missing, so the copies of the old ones, recognised by
# their checksum, are removed and copied again. A picture changed or put
# there by hand never matches and stays.

THUMBS=${CONSOLE_PICTURES_DEST:-/mnt/SDCARD/miyoo/app/consoles}
OLD=${CONSOLE_PICTURES_OLD_MD5:-/mnt/SDCARD/.tmp_update/script/migration/data/00027_old_console_pictures.md5}
COPY=${CONSOLE_PICTURES_SCRIPT:-/mnt/SDCARD/.tmp_update/script/console_pictures.sh}

[ -d "$THUMBS" ] && [ -f "$OLD" ] || exit 0

removed=0
for picture in "$THUMBS"/*.png; do
    [ -f "$picture" ] || continue
    sum=$(md5sum "$picture" | cut -d ' ' -f 1)
    if grep -qx "$sum" "$OLD"; then
        rm -f "$picture" && removed=$((removed + 1))
    fi
done

[ $removed -gt 0 ] && sh "$COPY"
exit 0
