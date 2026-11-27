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

# What an uninstall that could not run or stopped part-way left (Fable
# review, 4.5.1): the app's command files in the rom folders, and their
# rows in the rom list caches. Without the app they open nothing, and GLO
# would keep them (it drops the rows only once ~Filter.miyoocmd is gone).
# After a full uninstall there is none: nothing is started.
for romdir in "$SDROOT"/Roms/*/; do
    romdir=${romdir%/}
    [ -f "$romdir/~Filter.miyoocmd" ] || [ -f "$romdir/~Refresh roms.miyoocmd" ] ||
        [ -f "$romdir/~Clear filter.miyoocmd" ] || continue
    rm -f "$romdir/~Filter.miyoocmd" "$romdir/~Refresh roms.miyoocmd" "$romdir/~Clear filter.miyoocmd"
    name=${romdir##*/}
    db="$romdir/${name}_cache6.db"
    [ -s "$db" ] || continue
    table=$(printf '%s' "${name}_roms" | sed 's/"/""/g')
    sqlite3 "$db" "DELETE FROM \"$table\" WHERE path LIKE '%.miyoocmd' AND disp IN ('~Filter', '~Refresh roms', '~Clear filter');" 2> /dev/null
done

rm -rf "$APP"
exit 0
