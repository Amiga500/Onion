#!/bin/sh
# Backup: the saves and the settings in one zip in Backups/ on the SD card,
# and back from one of those zips.
#
# A backup holds the save folders of every profile (Saves, without the
# GameSwitcher screenshots and the saved clock), RedOnion's settings (.tmp_update/config and
# system.json), RetroArch's main settings, DraStic's saves and settings, and
# the Favorites and Recents lists. Games, BIOS files, themes and apps are not
# in it. A restore writes those files back over the current ones; files that
# are not in the backup are left as they are, except the other half of a
# Tweaks option (.name / .name_). It first backs up the current ones, so a
# restore can be undone.
#
# BACKUP_SD and BACKUP_7Z are for the host tests (test/test_scripts.sh).

SD=${BACKUP_SD:-/mnt/SDCARD}
sysdir=$SD/.tmp_update
BACKUP_DIR=$SD/Backups
PREFIX=RedOnion_backup_
Z7=${BACKUP_7Z:-7z}
LOG=/tmp/backup_app.log
# Never in a backup, nor written back: the GameSwitcher screenshots (big,
# made again while playing) and the clock saved for devices without an RTC
# (an old time written back would set the clock back at the next boot).
EXCLUDE_ROMSCREENS='-xr!romScreens'
EXCLUDE_CLOCK='-xr!currentTime.txt'
# Syncthing's keys and database (.tmp_update/config/syncthing), left out
# of a backup and not written back from one: a copy put back would give
# another device the same identity, or roll the database back under a
# running Syncthing. The folders it syncs are backed up as usual.
EXCLUDE_SYNCTHING='-x!.tmp_update/config/syncthing'
MAX_LISTED=30

# What a backup holds, relative to the SD card, one per line. Missing ones
# are left out.
backup_items() {
    (
        cd "$SD" || exit 1
        for item in Saves .tmp_update/config system.json RetroArch/.retroarch/retroarch.cfg \
            Emu/NDS/backup Emu/NDS/savestates Emu/NDS/config/drastic.cfg \
            Emu/NDS/resources/settings.json Roms/*.json; do
            [ -e "$item" ] && echo "$item"
        done
    )
}

# Can a path stored in a backup be written back? Only what backup_items
# puts in a backup (and the folders above it), never ".." or an absolute path.
allowed_path() {
    case "$1" in
        /* | "") return 1 ;;
    esac
    case "/$1/" in
        */../* | */./*) return 1 ;;
    esac
    case "$1" in
        Roms/*/*) return 1 ;;
        Saves | Saves/* | .tmp_update | .tmp_update/config | .tmp_update/config/*) return 0 ;;
        system.json | RetroArch | RetroArch/.retroarch | RetroArch/.retroarch/retroarch.cfg) return 0 ;;
        Emu | Emu/NDS | Emu/NDS/backup | Emu/NDS/backup/* | Emu/NDS/savestates | Emu/NDS/savestates/*) return 0 ;;
        Emu/NDS/config | Emu/NDS/config/drastic.cfg | Emu/NDS/resources | Emu/NDS/resources/settings.json) return 0 ;;
        Roms | Roms/*.json) return 0 ;;
    esac
    return 1
}

# The paths stored in a backup, one per line.
archive_paths() {
    "$Z7" l -slt -ba "$1" 2> /dev/null | sed -n 's/^Path = //p'
}

# Is the backup whole and made of paths it may write? Prints why not (with
# "\n" kept as written, for the screens).
check_archive() {
    if ! "$Z7" t "$1" > /dev/null 2>&1; then
        printf '%s\n' "The backup file is damaged."
        return 1
    fi
    paths=$(archive_paths "$1")
    if [ -z "$paths" ]; then
        printf '%s\n' "The backup file is empty."
        return 1
    fi
    while IFS= read -r p; do
        if ! allowed_path "$p"; then
            printf '%s\n' "The backup holds a file it may not\nwrite back: $p"
            return 1
        fi
    done << EOF
$paths
EOF
    return 0
}

# Guest Mode swaps profiles: while it is on, the main profile is in
# Saves/MainProfile. A backup goes back only with Guest Mode as it was then,
# so that each profile's saves go back to the same profile.
guest_mode_on() {
    [ -d "$SD/Saves/MainProfile" ]
}

archive_guest_mode_on() {
    archive_paths "$1" | grep -q -e "^Saves/MainProfile$" -e "^Saves/MainProfile/"
}

# Free space on the SD card and size of what is backed up, in KiB.
free_kb() {
    df -k "$SD" 2> /dev/null | tail -n 1 | awk '{print $(NF-2)}'
}

items_kb() { # the GameSwitcher screenshots are left out of a backup
    (
        cd "$SD" || exit
        du -sk "$@" 2> /dev/null | awk '{s += $1} END {print s + 0}'
        for d in Saves/*/romScreens; do
            [ -d "$d" ] && du -sk "$d" 2> /dev/null
        done | awk '{s += $1} END {print -s}'
    ) | awk '{s += $1} END {print (s > 0 ? s : 0)}'
}

size_label() { # KiB
    if [ "$1" -ge 1024 ]; then
        echo "$(($1 / 1024)) MB"
    else
        echo "$1 KB"
    fi
}

# The backups, newest first (the name holds the date and time).
list_backups() {
    ls "$BACKUP_DIR" 2> /dev/null | grep "^$PREFIX.*\.zip$" | sort -r
}

# "RedOnion_backup_2026-11-10_09-30-15_before-restore.zip"
# -> "2026-11-10 09:30:15 (before restore)"
backup_label() {
    echo "$1" | sed -e "s/^$PREFIX//" -e 's/\.zip$//' \
        -e 's/^\([0-9-]*\)_\([0-9][0-9]\)-\([0-9][0-9]\)-\([0-9][0-9]\)/\1 \2:\3:\4/' \
        -e 's/_before-restore/ (before restore)/' -e 's/_\([0-9][0-9]*\)$/ (\1)/'
}

# A new backup. $1: what to add to the name ("" or "_before-restore").
# Sets BACKUP_FILE and BACKUP_ERROR; returns 0 when made, 1 when made but
# some files could not be read, 2 when not made.
make_backup() {
    suffix=$1
    BACKUP_FILE=""
    BACKUP_ERROR=""
    items=$(backup_items)
    if [ -z "$items" ]; then
        BACKUP_ERROR="There is nothing to back up."
        return 2
    fi
    set --
    while IFS= read -r item; do
        set -- "$@" "$item"
    done << EOF
$items
EOF

    if ! mkdir -p "$BACKUP_DIR" 2> /dev/null; then
        BACKUP_ERROR="The Backups folder cannot be made."
        return 2
    fi
    # What a backup cut short (power off, empty battery) left behind.
    rm -f "$BACKUP_DIR/.$PREFIX"*.part
    # The zip is at most about the size of the files (they are compressed),
    # with some room left on the card.
    need=$(($(items_kb "$@") + 5120))
    free=$(free_kb)
    case "$free" in '' | *[!0-9]*) free=0 ;; esac
    if [ "$free" -lt "$need" ]; then
        BACKUP_ERROR="Not enough space on the SD card:\n$(size_label "$need") needed, $(size_label "$free") free."
        return 2
    fi

    stamp=$(date +%Y-%m-%d_%H-%M-%S)
    name="$PREFIX$stamp$suffix.zip"
    n=2
    while [ -e "$BACKUP_DIR/$name" ]; do
        name="$PREFIX$stamp${suffix}_$n.zip"
        n=$((n + 1))
    done
    part="$BACKUP_DIR/.$name.part"
    rm -f "$part"

    (cd "$SD" && "$Z7" a -tzip -mx1 -y -bd "$part" "$@" "$EXCLUDE_ROMSCREENS" "$EXCLUDE_CLOCK" "$EXCLUDE_SYNCTHING") >> "$LOG" 2>&1
    rc=$?
    if [ $rc -gt 1 ] || [ ! -s "$part" ]; then
        rm -f "$part"
        sync
        BACKUP_ERROR="The backup could not be made (7z error $rc)."
        return 2
    fi
    mv -f "$part" "$BACKUP_DIR/$name"
    sync
    BACKUP_FILE="$BACKUP_DIR/$name"
    [ $rc -eq 1 ] && return 1
    return 0
}

# Tweaks keeps an option as a pair of files in .tmp_update/config: .name when
# it is on, .name_ when it is off, never both. A restore writes the backup's
# file of each pair; the other one, left from a change made after the backup,
# would stay next to it (and .name wins), so it is removed.
restore_flag_pairs() {
    _rf_list=$(archive_paths "$1" | sed -n 's#^\.tmp_update/config/\(\.[^/]*\)$#\1#p')
    [ -n "$_rf_list" ] || return 0
    while IFS= read -r _rf_name; do
        case "$_rf_name" in
            *_) _rf_other=${_rf_name%_} ;;
            *) _rf_other=${_rf_name}_ ;;
        esac
        # Both in the backup: nothing of the pair is left over.
        if printf '%s\n' "$_rf_list" | grep -qxF -- "$_rf_other"; then
            continue
        fi
        if [ -f "$sysdir/config/$_rf_other" ]; then
            rm -f "$sysdir/config/$_rf_other"
            echo "Removed $_rf_other (the backup has $_rf_name)" >> "$LOG"
        fi
    done << FLAGS
$_rf_list
FLAGS
}

# Writes a backup back. Sets RESTORE_ERROR; returns 0 when done.
restore_backup() {
    RESTORE_ERROR=""
    archive=$1
    if ! RESTORE_ERROR=$(check_archive "$archive"); then
        return 1
    fi
    RESTORE_ERROR=""
    if archive_guest_mode_on "$archive"; then
        if ! guest_mode_on; then
            RESTORE_ERROR="This backup was made in Guest Mode.\nTurn Guest Mode on first, then restore it."
            return 1
        fi
    elif guest_mode_on; then
        RESTORE_ERROR="Guest Mode is on, and this backup was\nmade without it. Turn Guest Mode off first."
        return 1
    fi

    # The current saves and settings first, so that this can be undone.
    make_backup _before-restore
    if [ $? -gt 1 ]; then
        RESTORE_ERROR="The current saves could not be backed up\nfirst, so nothing was restored.\n$BACKUP_ERROR"
        return 1
    fi

    "$Z7" x -y -aoa "-o$SD" "$archive" "$EXCLUDE_CLOCK" "$EXCLUDE_SYNCTHING" >> "$LOG" 2>&1
    rc=$?
    if [ $rc -ne 0 ]; then
        sync
        RESTORE_ERROR="Restoring stopped (7z error $rc).\nYour files from before are in\n$(basename "$BACKUP_FILE")"
        return 1
    fi

    restore_flag_pairs "$archive"

    # MainUI's settings: at boot RedOnion reads them from this device's copy
    # in .tmp_update/config/system (made at each shutdown), so the restored
    # ones go there too.
    if archive_paths "$archive" | grep -qx "system.json"; then
        uuid=$(read_uuid 2> /dev/null)
        if [ -n "$uuid" ]; then
            mkdir -p "$sysdir/config/system"
            cp -f "$SD/system.json" "$sysdir/config/system/$uuid.json"
        fi
    fi
    # The theme in the restored settings gets its icons at the next boot
    # (RedOnion applies them again when this file does not name it).
    rm -f "$sysdir/config/theme-applied-"*
    sync
    return 0
}

# ---- Screens (stubbed by the tests) ----

ui_choose() { # title, message, choices... -> index, 255 for B
    title=$1
    message=$2
    shift 2
    if [ -n "$message" ]; then
        LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so prompt -t "$title" -m "$message" "$@"
    else
        LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so prompt -t "$title" "$@"
    fi
}

ui_info() { # title, message: waits for A or B
    infoPanel --title "$1" --message "$2"
}

ui_busy() { # title, message: shown until ui_busy_end
    rm -f /tmp/dismiss_info_panel
    infoPanel --title "$1" --message "$2" --persistent &
    busy_pid=$!
}

ui_busy_end() {
    touch /tmp/dismiss_info_panel
    [ -n "$busy_pid" ] && wait "$busy_pid" 2> /dev/null
    busy_pid=""
    rm -f /tmp/dismiss_info_panel
}

# Lets the player pick a backup. Sets PICKED (file name); returns 1 on B or
# when there is none.
pick_backup() { # title
    pick_title=$1
    PICKED=""
    files=$(list_backups | head -n $MAX_LISTED)
    if [ -z "$files" ]; then
        ui_info "$pick_title" "There are no backups yet.\nThey are kept in the Backups folder."
        return 1
    fi
    set --
    while IFS= read -r f; do
        set -- "$@" "$(backup_label "$f")"
    done << EOF
$files
EOF
    ui_choose "$pick_title" "" "$@"
    i=$?
    [ $i -eq 255 ] && return 1
    PICKED=$(echo "$files" | sed -n "$((i + 1))p")
    [ -n "$PICKED" ]
}

screen_backup() {
    ui_busy "BACKUP" "Backing up your saves and settings...\n \nThis can take a few minutes."
    make_backup ""
    rc=$?
    ui_busy_end
    case $rc in
        0) ui_info "BACKUP DONE" "Saved in the Backups folder:\n$(basename "$BACKUP_FILE")\n($(size_label "$(du -k "$BACKUP_FILE" | awk '{print $1}')"))" ;;
        1) ui_info "BACKUP DONE" "Saved in the Backups folder:\n$(basename "$BACKUP_FILE")\n \nSome files could not be read and are not in it." ;;
        *) ui_info "BACKUP FAILED" "$BACKUP_ERROR" ;;
    esac
}

screen_restore() {
    pick_backup "RESTORE" || return
    label=$(backup_label "$PICKED")
    ui_choose "RESTORE" "Put back the saves and settings\nfrom $label?\nYour current ones are backed up first." \
        "Cancel" "Restore"
    [ $? -eq 1 ] || return

    ui_busy "RESTORE" "Restoring your saves and settings...\n \nThis can take a few minutes."
    restore_backup "$BACKUP_DIR/$PICKED"
    rc=$?
    ui_busy_end
    if [ $rc -ne 0 ]; then
        ui_info "RESTORE FAILED" "$RESTORE_ERROR"
        return
    fi
    ui_choose "RESTORE DONE" "Restart now so that every setting\nis used. Don't change the volume or\nbrightness before restarting." \
        "Restart now" "Later"
    if [ $? -eq 0 ]; then
        finish
        "$sysdir/bin/shutdown" -r
        exit 0
    fi
}

screen_delete() {
    pick_backup "DELETE A BACKUP" || return
    label=$(backup_label "$PICKED")
    ui_choose "DELETE A BACKUP" "Delete the backup from\n$label?" "Cancel" "Delete"
    [ $? -eq 1 ] || return
    rm -f "$BACKUP_DIR/$PICKED"
    sync
}

# The notes shown with Select in the menu (prompt does not turn "\n" into
# line breaks there, so they are written on several lines).
NOTE_BACKUP="Saves (also Guest Mode's), RedOnion and
RetroArch settings, DraStic saves,
Favorites and Recents go in one zip
in the Backups folder of the SD card."
NOTE_RESTORE="Puts back the saves and settings
of a backup. Your current ones are
backed up first, so it can be undone."
NOTE_DELETE="Deletes a backup from the
Backups folder."

main() {
    touch /tmp/stay_awake
    : > "$LOG"
    selected=0
    while true; do
        LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so prompt -t "BACKUP" -i -s "$selected" \
            "Back up now" "Restore a backup..." "Delete a backup..." \
            "$NOTE_BACKUP" "$NOTE_RESTORE" "$NOTE_DELETE"
        selected=$?
        case $selected in
            0) screen_backup ;;
            1) screen_restore ;;
            2) screen_delete ;;
            *) break ;;
        esac
    done
    finish
}

# The log kept with Tweaks > Advanced > Diagnostics > Enable logging.
finish() {
    if [ -f "$sysdir/config/.logging" ]; then
        mkdir -p "$sysdir/logs"
        cp -f "$LOG" "$sysdir/logs/backup.log" 2> /dev/null
        sync
    fi
    rm -f /tmp/stay_awake
}

[ -n "$BACKUP_NO_MAIN" ] || main
