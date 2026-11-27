#!/bin/sh

# The "List shortcuts (Filter+Refresh)" app is no longer in the Package
# Manager (UI review, 4.5.1): its "~Filter" and "~Refresh roms" rows sat
# among the games of every list, and Refresh roms is in MainUI's SELECT
# menu. Where it is installed, it is uninstalled as its own uninstall.sh
# does (filter uninstall_filter: its .miyoocmd files and its rows in the
# rom list caches), then removed. A filter set now is cleared first: the
# uninstall drops the "~Filter: keyword" row that marks it, and its games
# would stay hidden with nothing to show them again.

SDROOT=${SDROOT:-/mnt/SDCARD}
APP=${FILTER_APP_DIR:-$SDROOT/App/Filter}
SYSDIR=${SYSDIR:-$SDROOT/.tmp_update}

[ -d "$APP" ] || exit 0

for active in "$SDROOT"/Emu/*/active_filter "$SDROOT"/RApp/*/active_filter; do
    [ -f "$active" ] || continue
    (cd "$SYSDIR" && ./bin/filter clear_filter "${active%/active_filter}")
done
(cd "$SYSDIR" && ./bin/filter uninstall_filter)
rm -rf "$APP"
exit 0
