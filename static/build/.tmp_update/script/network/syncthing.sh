#!/bin/sh
# Syncthing for RedOnion (Tweaks > Network > Syncthing).
#
# The Syncthing program (about 25 MB) is not part of the release: the first
# start downloads the official linux-arm build of the version below and
# checks it against its SHA-256 before using it. It is kept in .tmp_update/bin,
# which updates don't remove, and downloaded again only when RedOnion moves
# to another version.
#
# No folder is shared by default: the user adds folders and devices from the
# web interface, http://<device IP>:8384 (user onion, password onion until
# changed there). Configuration, keys and database live in
# .tmp_update/config/syncthing.
#
# Usage: syncthing.sh {start|stop|status}
#   start   download if needed, then start (nothing if already running)
#   stop    stop Syncthing if running
#   status  exit 0 if running

sysdir=/mnt/SDCARD/.tmp_update
miyoodir=/mnt/SDCARD/miyoo
export LD_LIBRARY_PATH="/lib:/config/lib:$miyoodir/lib:$sysdir/lib:$sysdir/lib/parasyte"
export PATH="$sysdir/bin:$PATH"

ST_VERSION=2.1.7
ST_SHA256=d8b59029e25992bc0d280fccc5626e5dca743b285fca1a36db1148fcf871b639
ST_URL="https://github.com/syncthing/syncthing/releases/download/v$ST_VERSION/syncthing-linux-arm-v$ST_VERSION.tar.gz"
ST_GUI_ADDRESS="0.0.0.0:8384"
ST_GUI_USER=onion
ST_GUI_PASSWORD=onion
# Download plus the unpacked program (~25 MB), with some margin.
ST_FREE_KB_NEEDED=51200

st_bin=$sysdir/bin/syncthing
st_stamp=$sysdir/bin/syncthing.version
st_home=$sysdir/config/syncthing
st_dl=$sysdir/download/syncthing
st_lock=/tmp/syncthing_install.lock
services_paused_flag=/tmp/services_paused_in_game
proc_dir=/proc

logfile=syncthing
. $sysdir/script/log.sh

main() {
    case "$1" in
        start) st_start ;;
        stop) st_stop ;;
        status) st_running ;;
        *)
            echo "Usage: $0 {start|stop|status}"
            exit 1
            ;;
    esac
}

# Pids of processes named exactly "syncthing" (/proc/<pid>/comm), in
# st_pids. pgrep matches patterns, so "pgrep syncthing" would also find
# this script (syncthing.sh).
st_find_pids() {
    st_pids=
    for _pd in "$proc_dir"/[0-9]*; do
        read -r _pc < "$_pd/comm" 2> /dev/null || continue
        [ "$_pc" = "syncthing" ] || continue
        st_pids="$st_pids ${_pd##*/}"
    done
}

st_running() {
    st_find_pids
    [ -n "$st_pids" ]
}

# The program is there and is the version this script asks for.
st_installed() {
    [ -f "$st_bin" ] && [ -f "$st_stamp" ] && [ "$(cat "$st_stamp")" = "$ST_VERSION" ]
}

st_sha256() { # file
    if command -v sha256sum > /dev/null 2>&1; then
        sha256sum "$1" | awk '{ print $1 }'
    else
        # OpenSSL 3 prints "SHA2-256(file)= <hash>"
        openssl dgst -sha256 "$1" | awk '{ print $NF }'
    fi
}

st_free_kb() { # free space on the SD card, in KB
    df -k /mnt/SDCARD | awk 'NR == 2 { print $4 }'
}

st_wifi_on() {
    [ "$(/customer/app/jsonval wifi 2> /dev/null)" = "1" ]
}

st_has_ip() { # as check_ntpstate in update_networking.sh
    ifconfig wlan0 2> /dev/null | grep -q 'inet addr:'
}

# At boot the first start runs right after Wi-Fi is switched on, before
# DHCP has given an address: wait for it (up to 60 s) before downloading.
st_wait_ip() {
    _waited=0
    while ! st_has_ip; do
        if [ $_waited -ge 60 ]; then
            return 1
        fi
        sleep 1
        _waited=$((_waited + 1))
    done
}

# One install at a time: a second call (another toggle, the network check
# after a game) returns at once. The lock holds the installer's pid, so a
# lock left by an installer that was killed doesn't block later installs.
st_lock_take() {
    if mkdir "$st_lock" 2> /dev/null; then
        echo $$ > "$st_lock/pid"
        return 0
    fi
    # No pid yet: the other installer has just made the lock.
    _lock_pid=$(cat "$st_lock/pid" 2> /dev/null)
    if [ -z "$_lock_pid" ] || [ -d "$proc_dir/$_lock_pid" ]; then
        return 1
    fi
    log "Syncthing: removing a stale download lock"
    rm -rf "$st_lock"
    mkdir "$st_lock" 2> /dev/null || return 1
    echo $$ > "$st_lock/pid"
}

# Download, verify and unpack the program.
st_install() {
    if ! st_lock_take; then
        log "Syncthing: download already running"
        return 1
    fi

    st_install_steps
    _ret=$?

    rm -rf "$st_dl"
    rm -rf "$st_lock"
    return $_ret
}

st_install_steps() {
    _free=$(st_free_kb)
    if [ -n "$_free" ] && [ "$_free" -lt "$ST_FREE_KB_NEEDED" ]; then
        log "Syncthing: not enough free space on the SD card (${_free} KB)"
        return 1
    fi

    if ! st_wait_ip; then
        log "Syncthing: no network address after 60 s, download skipped"
        return 1
    fi

    rm -rf "$st_dl"
    mkdir -p "$st_dl"
    _archive="$st_dl/syncthing.tar.gz"

    log "Syncthing: downloading v$ST_VERSION"
    # -k as in ota_update.sh (no CA bundle on the device): the archive is
    # checked against its SHA-256 below instead. A stalled transfer (under
    # 1 KB/s for a minute) gives up, so the lock is never held for good.
    if ! curl -k -L -f -s --connect-timeout 20 --retry 2 --retry-delay 5 \
        --speed-limit 1024 --speed-time 60 --max-time 1200 \
        -o "$_archive" "$ST_URL"; then
        log "Syncthing: download failed"
        return 1
    fi

    _sum=$(st_sha256 "$_archive")
    if [ "$_sum" != "$ST_SHA256" ]; then
        log "Syncthing: checksum mismatch ($_sum), download discarded"
        return 1
    fi

    _member="syncthing-linux-arm-v$ST_VERSION/syncthing"
    if ! (cd "$st_dl" && gzip -dc syncthing.tar.gz | tar -xf - "$_member") || [ ! -s "$st_dl/$_member" ]; then
        log "Syncthing: could not unpack the download"
        return 1
    fi

    if ! mv -f "$st_dl/$_member" "$st_bin.new" ||
        ! chmod +x "$st_bin.new" ||
        ! mv -f "$st_bin.new" "$st_bin" ||
        ! echo "$ST_VERSION" > "$st_stamp"; then
        log "Syncthing: could not install the program"
        rm -f "$st_bin.new"
        return 1
    fi
    sync
    log "Syncthing: v$ST_VERSION installed"
}

# First start: keys and a configuration with no folders, with the web
# interface login (the same onion/onion as the other network services).
# The template for new folders is set for the SD card's FAT32: no
# permissions to sync, and modification times only kept to 2 seconds
# (without the 2 s window every file copied from a PC would look changed).
st_generate() {
    log "Syncthing: creating configuration"
    mkdir -p "$st_home"
    HOME=/mnt/SDCARD STHOMEDIR="$st_home" "$st_bin" generate \
        --gui-user="$ST_GUI_USER" --gui-password="$ST_GUI_PASSWORD" \
        --no-port-probing > /dev/null 2>&1
    if [ ! -s "$st_home/config.xml" ]; then
        log "Syncthing: could not create the configuration"
        return 1
    fi
    # A new configuration has no folder: these only match the folder
    # template in <defaults>.
    sed -i \
        -e 's/ignorePerms="false"/ignorePerms="true"/' \
        -e 's#<modTimeWindowS>0</modTimeWindowS>#<modTimeWindowS>2</modTimeWindowS>#' \
        "$st_home/config.xml"
    sync
}

st_start() {
    if st_running; then
        return 0
    fi

    if ! st_installed; then
        st_install || return 1
        # The download can take a while: the toggle may have been turned
        # off, Wi-Fi switched off or a game started meanwhile.
        if [ ! -f "$sysdir/config/.syncthingState" ] || [ -f "$services_paused_flag" ] || ! st_wifi_on; then
            log "Syncthing: installed, not started (toggle off, no Wi-Fi or game running)"
            return 0
        fi
        if st_running; then
            return 0
        fi
    fi

    # Never start without the login: Syncthing would create an open
    # configuration itself, reachable by anyone on the network.
    if [ ! -s "$st_home/config.xml" ]; then
        st_generate || return 1
    fi

    if [ -f "$sysdir/config/.logging" ]; then
        mkdir -p "$sysdir/logs"
        _logargs="--log-file=$sysdir/logs/syncthing-daemon.log --log-max-size=1048576 --log-max-old-files=1"
    else
        _logargs="--log-file=-"
    fi

    log "Syncthing: starting (web interface on port 8384)"
    # STHOMEDIR / STGUIADDRESS: the variables Syncthing's own Docker image
    # uses. HOME: folder paths typed in the web interface start from the SD
    # card. GOMEMLIMIT: the devices have 128 MB of RAM; a soft limit makes
    # the Go runtime collect garbage harder near it instead of growing.
    (
        cd /
        unset LD_PRELOAD
        export HOME=/mnt/SDCARD
        export STHOMEDIR="$st_home"
        export STGUIADDRESS="$ST_GUI_ADDRESS"
        export GOMEMLIMIT=64MiB
        # shellcheck disable=SC2086
        exec "$st_bin" serve --no-browser --no-restart --no-upgrade $_logargs > /dev/null 2>&1
    ) &
}

# Ask Syncthing to stop (it closes its database), then force it after 5 s.
st_stop() {
    st_find_pids
    [ -z "$st_pids" ] && return 0

    log "Syncthing: stopping"
    # shellcheck disable=SC2086
    kill $st_pids 2> /dev/null
    _tries=0
    while [ $_tries -lt 5 ]; do
        sleep 1
        st_running || return 0
        _tries=$((_tries + 1))
    done
    # shellcheck disable=SC2086
    kill -9 $st_pids 2> /dev/null
    log "Syncthing: killed"
}

if [ -f $sysdir/config/.logging ]; then
    main "$@"
else
    main "$@" > /dev/null 2>&1
fi
