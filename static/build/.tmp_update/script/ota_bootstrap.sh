#!/bin/sh
BOOTSTRAP_VERSION=1.1

echo ":: Running bootstrap script...  (version $BOOTSTRAP_VERSION)"

umount /mnt/SDCARD/miyoo/app/MainUI 2> /dev/null

# Updaters up to V4.4.0-beta-20260928 repair the SD card while it is mounted
# (fsck.fat -a) before downloading, which can damage files. On those, swap
# fsck.fat for a no-op for this update: the release being installed brings the
# real one back, and its updater no longer repairs the card during updates.
sysdir=${BOOTSTRAP_SYSDIR:-/mnt/SDCARD/.tmp_update}
if grep -q '^[[:space:]]*fsck\.fat -a' "$sysdir/script/ota_update.sh" 2> /dev/null &&
    ! grep -q 'OnionPlus no-op' "$sysdir/bin/fsck.fat" 2> /dev/null; then
    mv -f "$sysdir/bin/fsck.fat" "$sysdir/bin/fsck.fat.orig" 2> /dev/null
    printf '#!/bin/sh\n# OnionPlus no-op: no repair of the mounted SD card during updates.\necho "Disk check skipped: unsafe while the card is in use."\nexit 0\n' > "$sysdir/bin/fsck.fat"
    chmod +x "$sysdir/bin/fsck.fat"
    sync
    echo ":: Disk check disabled for this update"
fi
