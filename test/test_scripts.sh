#!/bin/sh
# Host tests for shell functions in static/build/.tmp_update. Each function
# is extracted from the real script text (sed between "name() {" and the
# closing "}" at column 0) and run against stubs in a temp directory, so
# the test exercises the shipped code rather than a copy.
#
# Run: make -f Makefile.unit test_scripts   (also part of make unit-test)

ROOT=$(cd "$(dirname "$0")/.." 2> /dev/null && pwd)
[ -f "$ROOT/static/build/.tmp_update/runtime.sh" ] || ROOT=$(cd "$(dirname "$0")/../.." && pwd)
RUNTIME="$ROOT/static/build/.tmp_update/runtime.sh"
NETWORK="$ROOT/static/build/.tmp_update/script/network/update_networking.sh"

tests=0
asserts=0
fails=0
skipped=0
current=""
current_failed=0

extract_fn() { # file name
    sed -n "/^$2() {/,/^}/p" "$1"
}

begin() {
    current=$1
    current_failed=0
    tests=$((tests + 1))
    TMP=$(mktemp -d)
    # TESTS_TRACE=1 (CI's BusyBox run): the block's name before it runs, so a
    # block that hangs is named in the log when the step's time runs out.
    [ "${TESTS_TRACE:-0}" = 1 ] && echo "  [RUN ] $current" >&2
    return 0
}

end() {
    rm -rf "$TMP"
    if [ $current_failed -eq 0 ]; then
        echo "  [ OK ] $current"
    else
        echo "  [FAIL] $current"
    fi
}

# A whole line equal to TEXT (FILE "-": standard input). Not grep -x: the
# console's BusyBox 1.20.2 never matches with -x (and -xF is a substring
# search), so the suite could not run there.
has_line() { # file text
    HL_TEXT=$2 awk '$0 == ENVIRON["HL_TEXT"] { found = 1 } END { exit !found }' "$1"
}

# Line SECOND comes right after line FIRST in FILE (leading and trailing
# blanks ignored). Not grep with a line break in the pattern: GNU grep reads
# it as "or" (either line passed), BusyBox's as a literal line break.
line_follows() { # file first second
    LF_A=$2 LF_B=$3 awk '{ t = $0; sub(/^[ \t]+/, "", t); sub(/[ \t]+$/, "", t) }
        prev == ENVIRON["LF_A"] && t == ENVIRON["LF_B"] { found = 1 }
        { prev = t }
        END { exit !found }' "$1"
}

# Sets $? (an environment for one function call is set, the call made,
# the status kept, the environment taken back: "VAR=x function" does not
# work in the console's BusyBox 1.20.2 ash).
rc() { return "$1"; }

# Runs a command for at most SECONDS (no "timeout": BusyBox 1.20.2's takes
# -t SECONDS, GNU's a bare number).
with_limit() { # seconds, command...
    _wl_s=$1
    shift
    "$@" &
    _wl_pid=$!
    (sleep "$_wl_s" && kill "$_wl_pid") > /dev/null 2>&1 &
    _wl_dog=$!
    wait "$_wl_pid"
    _wl_rc=$?
    kill "$_wl_dog" 2> /dev/null
    return $_wl_rc
}

# A block that cannot run on this PC (no 7-Zip, jq, python3, C compiler...)
# is counted and shown in the result: with TESTS_NO_SKIP=1 (CI) it fails.
skip() {
    skipped=$((skipped + 1))
    echo "  [SKIP] $*"
}

check() { # description, then a test command
    desc=$1
    shift
    asserts=$((asserts + 1))
    if ! "$@"; then
        echo "    FAIL: $current: $desc"
        fails=$((fails + 1))
        current_failed=1
    fi
}

# ---- update_networking.sh: disable_flag ----

eval "$(extract_fn "$NETWORK" disable_flag)"

begin disable_flag_moves_flag_to_underscore
sysdir=$TMP
mkdir -p "$sysdir/config"
touch "$sysdir/config/.sshState"
disable_flag sshState
check ".sshState removed" test ! -e "$sysdir/config/.sshState"
check ".sshState_ created" test -e "$sysdir/config/.sshState_"
check "no stray file named after the directory" test "$(ls -A "$sysdir/config" | wc -l)" -eq 1
end

begin disable_flag_missing_flag_is_noop
sysdir=$TMP
mkdir -p "$sysdir/config"
out=$(disable_flag ftpState 2>&1)
check "no error output" test -z "$out"
check "nothing created" test -z "$(ls -A "$sysdir/config")"
end

begin disable_flag_overwrites_old_marker
sysdir=$TMP
mkdir -p "$sysdir/config"
touch "$sysdir/config/.httpState" "$sysdir/config/.httpState_"
disable_flag httpState
check ".httpState removed" test ! -e "$sysdir/config/.httpState"
check ".httpState_ kept" test -e "$sysdir/config/.httpState_"
end

# ---- update_networking.sh: start_services_outside_game ----

eval "$(extract_fn "$NETWORK" start_services_outside_game)"

stub_services() { # records which checkers ran; killall records its args
    log() { :; }
    for svc in ftp ssh telnet http smbd; do
        eval "check_${svc}state() { echo $svc >> \"\$TMP/started\"; }"
    done
    killall() { echo "$*" >> "$TMP/killed"; }
    netscript=$TMP/netscript # wifi_power_save.sh records its argument
    mkdir -p "$netscript"
    printf '#!/bin/sh\necho "$*" >> "%s/power_save"\n' "$TMP" > "$netscript/wifi_power_save.sh"
    chmod +x "$netscript/wifi_power_save.sh"
}

begin boot_services_start_without_game
stub_services
services_paused_flag="$TMP/paused"
start_services_outside_game
check "all five checkers ran" test "$(wc -l < "$TMP/started")" -eq 5
check "nothing killed" test ! -e "$TMP/killed"
check "then Wi-Fi power save follows the services" test "$(cat "$TMP/power_save")" = auto
end

begin boot_services_skipped_while_game_runs
stub_services
services_paused_flag="$TMP/paused"
: > "$services_paused_flag"
start_services_outside_game
check "no checker ran" test ! -e "$TMP/started"
end

begin boot_services_stopped_when_game_starts_meanwhile
stub_services
services_paused_flag="$TMP/paused"
check_smbdstate() { : > "$services_paused_flag"; } # the game launches here
start_services_outside_game
check "services killed after the game started" grep -q dropbear "$TMP/killed"
check "and power save set for the paused services" test "$(cat "$TMP/power_save")" = auto
end

begin boot_services_flag_path_matches_runtime
flag_line=$(grep '^services_paused_flag=' "$NETWORK")
flag_path=${flag_line#services_paused_flag=}
check "update_networking.sh defines the flag" test -n "$flag_path"
check "launch_game creates it" grep -q ": > $flag_path" "$RUNTIME"
check "launch_game_postprocess removes it" grep -q "rm -f $flag_path" "$RUNTIME"
check "services only start through the guard" test "$(grep -c '^ *check_sshstate &' "$NETWORK")" -eq 1
end

# ---- update_networking.sh: service toggles without Wi-Fi ----

for fn in flag_enabled check_smbdstate check_ftpstate check_sshstate check_telnetstate check_httpstate; do
    eval "$(extract_fn "$NETWORK" $fn)"
done

net_env() { # running (1/0): service processes reported as running
    sysdir=$TMP
    mkdir -p "$sysdir/config"
    filebrowserbin="$TMP/filebrowser"
    filebrowserdb="$TMP/fb.db"
    touch "$filebrowserbin"
    WIFI_STATE_CACHED=0
    wifi_enabled() { [ "$WIFI_STATE_CACHED" -eq 1 ]; }
    wifi_disabled() { [ "$WIFI_STATE_CACHED" -eq 0 ]; }
    log() { :; }
    sleep() { :; }
    if [ "$1" -eq 1 ]; then
        is_running() { true; }
        is_running_exact() { case "$1" in *telnetd*) false ;; *) true ;; esac; }
    else
        is_running() { false; }
        is_running_exact() { false; }
    fi
    killall() { echo "$*" >> "$TMP/killed"; }
    pkill() { echo "$*" >> "$TMP/killed"; }
    for f in smbdState ftpState sshState telnetState httpState; do
        touch "$sysdir/config/.$f"
    done
}

toggles_kept() {
    for f in smbdState ftpState sshState telnetState httpState; do
        [ -f "$sysdir/config/.$f" ] || return 1
    done
}

run_checkers() {
    check_smbdstate
    check_ftpstate
    check_sshstate
    check_telnetstate
    check_httpstate
}

begin wifi_off_stops_services_keeps_toggles
net_env 1
run_checkers
check "all five toggles still on" toggles_kept
check "running services stopped" test "$(wc -l < "$TMP/killed")" -ge 5
end

begin wifi_off_not_running_keeps_toggles
net_env 0
run_checkers
check "all five toggles still on" toggles_kept
check "nothing killed" test ! -e "$TMP/killed"
end

# ---- Syncthing ----

SYNCTHING="$ROOT/static/build/.tmp_update/script/network/syncthing.sh"

begin syncthing_follows_toggle_and_wifi
eval "$(extract_fn "$NETWORK" check_syncthingstate)"
eval "$(extract_fn "$NETWORK" lowest_pid_named)"
sysdir=$TMP
mkdir -p "$sysdir/config" "$TMP/proc/42"
netscript=$TMP/net
mkdir -p "$netscript"
printf '#!/bin/sh\necho "$1" >> "%s/st_calls"\n' "$TMP" > "$netscript/syncthing.sh"
chmod +x "$netscript/syncthing.sh"
log() { :; }
proc_dir=$TMP/proc
WIFI=1
wifi_enabled() { [ "$WIFI" -eq 1 ]; }
touch "$sysdir/config/.syncthingState"
check_syncthingstate
check "toggle on, Wi-Fi on: started" has_line "$TMP/st_calls" 'start'
rm -f "$TMP/st_calls"
WIFI=0
echo syncthing > "$TMP/proc/42/comm"
check_syncthingstate
check "Wi-Fi off while running: stopped" has_line "$TMP/st_calls" 'stop'
check "and the toggle kept" test -f "$sysdir/config/.syncthingState"
rm -f "$TMP/st_calls" "$sysdir/config/.syncthingState"
echo syncthing.sh > "$TMP/proc/42/comm"
check_syncthingstate
check "off and not running (the script itself is not Syncthing): nothing called" test ! -e "$TMP/st_calls"
unset proc_dir netscript
end

begin syncthing_is_a_remembered_service
check "kept across a Wi-Fi change" grep -q '^services=.*syncthingState' "$NETWORK"
check "restored from the saved state" grep -q 'authhttpState|syncthingState)' "$NETWORK"
check "turned off with the others" sh -c "sed -n '/^disable_all_services() {/,/^}/p' '$NETWORK' | grep -q 'disable_flag syncthingState' && sed -n '/^disable_all_services() {/,/^}/p' '$NETWORK' | grep -q 'syncthing.sh stop'"
check "update_networking.sh syncthing toggle" sh -c "grep -A7 '        syncthing) ' '$NETWORK' | grep -q check_syncthingstate"
check "the toggle retries a failed download at once" sh -c "grep -A7 '        syncthing) ' '$NETWORK' | grep -q 'export ST_RETRY_NOW=1'"
check "paused for a game: stopped, and waited for" sh -c "grep -A3 'killall -9 dropbear bftpd filebrowser telnetd smbd\$' '$RUNTIME' | grep -q 'syncthing.sh stop'"
check "and back after the game" grep -q 'for service in smbd http ssh ftp telnet syncthing; do' "$RUNTIME"
check "Tweaks toggles it in the background" sh -c "grep -A3 'void network_setSyncthingState' '$ROOT/src/tweaks/network.h' | grep -q '.syncthingState' && grep -A4 'void network_setSyncthingState' '$ROOT/src/tweaks/network.h' | grep -q 'execServiceState(\"syncthing\", true)'"
end

st_env() {
    for fn in st_find_pids st_running st_installed st_sha256 st_lock_take st_install st_install_steps st_wifi_on st_has_ip st_wait_ip st_free_kb \
        st_failed_recently st_install_then_start st_start; do
        eval "$(extract_fn "$SYNCTHING" $fn)"
    done
    sysdir=$TMP/sd
    mkdir -p "$sysdir/bin" "$TMP/proc"
    eval "$(grep -e '^st_bin=' -e '^st_stamp=' "$SYNCTHING")"
    st_dl=$sysdir/download/syncthing
    st_lock=$TMP/st_lock
    st_failed=$TMP/st_failed
    ST_RETRY_AFTER=600
    services_paused_flag=$TMP/services_paused
    rm -f "$st_failed" "$services_paused_flag"
    proc_dir=$TMP/proc
    ST_VERSION=9.9.9
    ST_FREE_KB_NEEDED=10
    ST_URL=http://example.invalid/st.tar.gz
    log() { echo "$*" >> "$TMP/st_log"; }
    st_wait_ip() { true; }
    st_free_kb() { echo 999999; }
    # The release archive the mocked curl serves.
    mkdir -p "$TMP/pkg/syncthing-linux-arm-v9.9.9"
    printf 'ELF' > "$TMP/pkg/syncthing-linux-arm-v9.9.9/syncthing"
    (cd "$TMP/pkg" && tar -czf "$TMP/st.tar.gz" syncthing-linux-arm-v9.9.9)
    curl() {
        while [ $# -gt 0 ]; do
            [ "$1" = "-o" ] && { cp "$TMP/st.tar.gz" "$2"; return 0; }
            shift
        done
        return 1
    }
}

begin syncthing_download_is_checked
st_env
ST_SHA256=0000
st_install
check "wrong checksum: refused" test $? -ne 0
check "nothing installed" test ! -e "$st_bin"
check "the download is removed" test ! -e "$st_dl"
check "the lock is released" test ! -e "$st_lock"
ST_SHA256=$(sha256sum "$TMP/st.tar.gz" | awk '{ print $1 }')
st_install
check "right checksum: installed" test -s "$st_bin" -a -x "$st_bin"
check "in .tmp_update/etc, which the update trigger keeps" sh -c 'case "$1" in "$2"/etc/*) ;; *) exit 1 ;; esac && grep -q -- "-not -name \"etc\"" "$3"' _ "$st_bin" "$sysdir" "$ROOT/static/dist/miyoo/app/MainUI"
check "for this version" st_installed
ST_VERSION=9.9.10
not_installed() { ! st_installed; }
check "another version asks for a new download" not_installed
unset -f curl
end

begin syncthing_one_download_at_a_time
st_env
st_lock_take
check "the first takes the lock" test "$(cat "$st_lock/pid")" = "$$"
mkdir -p "$proc_dir/$$" # the first download is still running
lock_busy() { ! st_lock_take; }
check "a second call waits for it" lock_busy
rm -rf "$st_lock"
mkdir -p "$st_lock"
echo 999999 > "$st_lock/pid"
st_lock_take
check "a lock left by a killed download is taken over" test "$(cat "$st_lock/pid")" = "$$"
rm -rf "$st_lock"
end

begin syncthing_download_stops_for_a_game
st_env
ST_SHA256=0000
# A slow transfer, until it is killed.
curl() { while :; do sleep 0.1; done; }
(sleep 1; : > "$services_paused_flag") &
t0=$(date +%s)
st_install
st_rc=$?
t1=$(date +%s)
check "stopped when a game starts" test $st_rc -eq 3
check "within a couple of seconds" test $((t1 - t0)) -le 4
check "said so" grep -q "a game started, download stopped" "$TMP/st_log"
check "the lock is released" test ! -e "$st_lock"
st_install_then_start_lock() { st_lock_take && st_install_then_start; }
mkdir -p "$TMP/proc/$$"
st_install_then_start_lock
check "a game is not a failure: retried after it" test ! -e "$st_failed"
unset -f curl st_install_then_start_lock
rm -f "$services_paused_flag"
end

begin syncthing_failed_download_waits_before_retrying
st_env
ST_SHA256=0000
mkdir -p "$TMP/proc/$$"
st_lock_take
st_install_then_start
check "a failed download is remembered" test -s "$st_failed"
check "and the lock released" test ! -e "$st_lock"
: > "$TMP/st_log"
st_start
check "an automatic start right after does not download again" test $? -ne 0
check "and says why" grep -q "not tried again yet" "$TMP/st_log"
check "nothing started in the background" test ! -e "$st_lock"
echo $(($(date +%s) - 700)) > "$st_failed"
not_recent() { ! st_failed_recently; }
check "after 10 minutes it is tried again" not_recent
echo $(($(date +%s) - 10)) > "$st_failed"
st_install_then_start() { sleep 1; }
export ST_RETRY_NOW=1; st_start; _rc=$?; unset ST_RETRY_NOW; rc $_rc
check "the Tweaks toggle tries at once, in the background" test $? -eq 0 -a -d "$st_lock"
check "the lock names the background job" sh -c "test \"\$(cat '$st_lock/pid')\" != $$"
sleep 1.5
rm -rf "$st_lock"
unset -f st_install_then_start curl not_recent
end

begin syncthing_found_by_exact_name
st_env
mkdir -p "$TMP/proc/10" "$TMP/proc/11"
echo syncthing.sh > "$TMP/proc/10/comm"
st_find_pids
check "only syncthing.sh: not running" test -z "$st_pids"
echo syncthing > "$TMP/proc/11/comm"
st_find_pids
check "syncthing: found" test "$(echo $st_pids)" = 11
unset proc_dir
end

begin syncthing_stop_waits_and_forces
st_env
eval "$(extract_fn "$SYNCTHING" st_stop)"
proc_dir=$TMP/proc_stop # only this test's process
rm -f "$TMP/kills" "$TMP/ticks"
ST_STOP_TRIES=25
mkdir -p "$TMP/proc_stop/77"
echo syncthing > "$TMP/proc_stop/77/comm"
: > "$TMP/st_log"
sleep() { echo tick >> "$TMP/ticks"; }
# Exits at TERM, after two checks.
kill() {
    echo "$*" >> "$TMP/kills"
    case "$1" in -9) ;; *) rm -rf "$TMP/proc_stop/77" ;; esac
}
st_stop
check "asked with a plain TERM" test "$(cat "$TMP/kills")" = "77"
check "gone at the first check" test "$(wc -l < "$TMP/ticks")" -eq 1
check "logged as stopped" grep -q "Syncthing: stopped" "$TMP/st_log"
# Ignores TERM: killed after the last check.
rm -f "$TMP/kills" "$TMP/ticks"
ST_STOP_TRIES=25
mkdir -p "$TMP/proc_stop/77"
echo syncthing > "$TMP/proc_stop/77/comm"
kill() { echo "$*" >> "$TMP/kills"; [ "$1" = -9 ] && rm -rf "$TMP/proc_stop/77"; true; }
ST_STOP_TRIES=25
st_stop
check "25 checks, 0.2 s apart (5 s)" test "$(wc -l < "$TMP/ticks")" -eq 25
check "then killed" test "$(tail -n 1 "$TMP/kills")" = "-9 77"
check "and logged" grep -q "Syncthing: killed" "$TMP/st_log"
check "checked every 0.2 s in the script" sh -c "sed -n '/^st_stop() {/,/^}/p' '$SYNCTHING' | grep -q 'sleep 0.2'"
unset -f sleep kill
unset proc_dir
end

begin flip_display_colours_restored_at_boot
eval "$(extract_fn "$RUNTIME" restore_display_colours)"
flip_csc_block=$(awk '/# Mini Flip: put the display colours/ { on = 1 } on { print } on && /^    fi$/ { exit }' "$RUNTIME")
check "the block is found" test -n "$flip_csc_block"
MI_DISP_NODE=$TMP/mi_disp0
: > "$MI_DISP_NODE"
sysjson_get() {
    case "$1" in lumination) echo 7 ;; hue) echo 10 ;; saturation) echo 12 ;; contrast) echo 15 ;; esac
}
log() { echo "$*" >> "$TMP/csc_log"; }
MODEL_MMF=285
DEVICE_ID=354
eval "$flip_csc_block"
check "not on a Mini Plus" test ! -s "$MI_DISP_NODE"
DEVICE_ID=285
eval "$flip_csc_block"
check "on the Flip, at once (nothing waits for warmup): Open MainUI's formulas, BT709, sharpness 0" test "$(cat "$MI_DISP_NODE")" = "csc 0 3 60 50 42 60 0 0"
check "not in the background" sh -c '! printf "%s\n" "$1" | grep -q "&\$"' _ "$flip_csc_block"
check "logged" grep -q "display colours restored: lumination 7, hue 10, saturation 12, contrast 15" "$TMP/csc_log"
sysjson_get() {
    case "$1" in lumination) echo "" ;; hue) echo 25 ;; saturation) echo x ;; contrast) echo 0 ;; esac
}
MODEL_MM=283
restore_display_colours
check "missing or out of range on the Flip: its defaults (lumination 10)" test "$(cat "$MI_DISP_NODE")" = "csc 0 3 30 50 45 50 0 0"
DEVICE_ID=283
restore_display_colours
check "missing or out of range on the Mini: its defaults (lumination 7)" test "$(cat "$MI_DISP_NODE")" = "csc 0 3 30 50 42 50 0 0"
DEVICE_ID=354
restore_display_colours
check "missing or out of range on the Mini+: its defaults (lumination 10)" test "$(cat "$MI_DISP_NODE")" = "csc 0 3 30 50 45 50 0 0"
DEVICE_ID=285
rm -f "$MI_DISP_NODE" "$TMP/csc_log"
restore_display_colours
check "no display device: nothing written, said so" sh -c 'test ! -e "$1" && grep -q "missing, not restored" "$2"' _ "$MI_DISP_NODE" "$TMP/csc_log"
check "not through MI_SYS (restore_lcd_csc no longer run)" sh -c "! grep -q 'bin/restore_lcd_csc' '$RUNTIME'"
check "start_warmup keeps warmup's pid" sh -c "sed -n '/^start_warmup() {/,/^}/p' '$RUNTIME' | grep -q 'warmup_pid=\\\$!'"
check "before the blue light filter" sh -c "awk '/# Mini Flip: put the display colours/ { c = NR } /# Blue Light Filter, then DisplayCal/ { b = NR } END { exit !(c && b && c < b) }' '$RUNTIME'"
unset -f sysjson_get log
unset MI_DISP_NODE MODEL_MM
end

begin syncthing_never_starts_open
check "the login is set before the first start" grep -q -- '--gui-user="$ST_GUI_USER" --gui-password="$ST_GUI_PASSWORD"' "$SYNCTHING"
check "no start without a configuration" sh -c "sed -n '/^st_launch() {/,/^}/p' '$SYNCTHING' | grep -q 'st_generate || return 1'"
check "FAT32: permissions ignored, 2 s time window" sh -c "grep -q 'ignorePerms=\"true\"' '$SYNCTHING' && grep -q '<modTimeWindowS>2</modTimeWindowS>' '$SYNCTHING'"
end

# ---- Scraper: Game Info media from ScreenScraper ----

SCRAPER_SS="$ROOT/static/build/.tmp_update/script/scraper/scrap_screenscraper.sh"
SCRAPER_MENU="$ROOT/static/build/.tmp_update/script/scraper/menu.sh"

# The gi_* functions and their settings, as the scraper defines them.
gi_env() {
    eval "$(sed -n '/^GI_DEFAULT=/,/^saveMetadata=false/p' "$SCRAPER_SS" | grep -v '^saveMetadata')"
    GREEN=; YELLOW=; NONE=
    Region1=us; Region2=wor; Region3=eu; Region4=; Region5=; Region6=; Region7=; Region8=
    gi_game=$(cat "$ROOT/test/data/screenscraper_game.json")
    gi_medias=$(printf '%s\n' "$gi_game" | jq '.response.jeu.medias')
    gi_root=$TMP/Roms/SFC
    gi_name="Trials & Co"
    gi_rel="RPG/Trials & Co.sfc"
    mkdir -p "$gi_root/RPG"
    GameInfo_Success=0
    GameInfo_Fail=0
}

begin scraper_game_info_entry
gi_env
gi_entry > "$TMP/entry"
check "path, XML-escaped" has_line "$TMP/entry" '    <path>./RPG/Trials &amp; Co.sfc</path>'
check "name in the first region of the order (us)" has_line "$TMP/entry" '    <name>Trials of Mana</name>'
check "description in English, escaped" grep -q '<desc>Three heroes &amp; a &lt;mana&gt; &quot;tree&quot;' "$TMP/entry"
check "genres in English, or the first language" has_line "$TMP/entry" '    <genre>Action RPG, Aventure</genre>'
check "rating: 17/20 as 0.85" has_line "$TMP/entry" '    <rating>0.85</rating>'
check "date of the region, a year as 1 January" has_line "$TMP/entry" '    <releasedate>20200101T000000</releasedate>'
check "developer, publisher, players" sh -c "grep -q '<developer>Square</developer>' '$TMP/entry' && grep -q '<publisher>Square Enix</publisher>' '$TMP/entry' && grep -q '<players>1-3</players>' '$TMP/entry'"
gi_game='{"response":{"jeu":{"noms":[{"region":"jp","text":"X"}]}}}'
gi_entry > "$TMP/entry2"
check "missing fields are left out" sh -c "! grep -q '<desc>\|<rating>\|<genre>' '$TMP/entry2' && grep -q '<name>X</name>' '$TMP/entry2'"
gi_game='{"response":{"jeu":{"noms":[{"region":"us","text":"Y"}],"dates":[{"region":"us","text":"1995-09-30"}],"note":{"text":"16"}}}}'
gi_entry > "$TMP/entry3"
check "full date as YYYYMMDDT000000" has_line "$TMP/entry3" '    <releasedate>19950930T000000</releasedate>'
check "16/20 as 0.8" has_line "$TMP/entry3" '    <rating>0.8</rating>'
end

begin scraper_game_info_gamelist
gi_env
gi_write_desc
check "no gamelist.xml: made with the entry" sh -c "grep -q '^<gameList>' '$gi_root/gamelist.xml' && grep -q '<path>./RPG/Trials &amp; Co.sfc</path>' '$gi_root/gamelist.xml' && tail -1 '$gi_root/gamelist.xml' | grep -q '^</gameList>$'"
printf '<?xml version="1.0"?><gameList><game><path>./Other.sfc</path><desc>keep</desc></game></gameList>' > "$gi_root/gamelist.xml"
gi_write_desc
check "one-line list: entry inside gameList, old entry kept" sh -c "test \$(grep -c '<game>' '$gi_root/gamelist.xml') -eq 2 && grep -q '<desc>keep</desc>' '$gi_root/gamelist.xml' && tail -1 '$gi_root/gamelist.xml' | grep -q '^</gameList>$' && head -1 '$gi_root/gamelist.xml' | grep -q '^<?xml version=\"1.0\"?><gameList><game><path>./Other.sfc'"
GameInfoMedia=desc
missing=$(gi_missing)
check "desc found by its escaped path" test -z "$missing"
printf '<gameList>\n<game><path>./A.sfc</path></game>\n' > "$gi_root/gamelist.xml"
gi_write_desc
rc=$?
check "a list without </gameList>: refused" test $rc -ne 0
check "and left alone" sh -c "! grep -q 'Trials' '$gi_root/gamelist.xml'"
end

begin scraper_game_info_entry_with_an_apostrophe
gi_env
GameInfoMedia=desc
gi_rel="Kirby's Dream & Land.gb"
printf '<gameList>\n<game><path>./Kirby'"'"'s Dream &amp; Land.gb</path></game>\n</gameList>\n' > "$gi_root/gamelist.xml"
check "another scraper's entry, apostrophe as it is" test -z "$(gi_missing)"
printf '<gameList>\n<game><path>./Kirby&#39;s Dream &amp; Land.gb</path></game>\n</gameList>\n' > "$gi_root/gamelist.xml"
check "ours (&#39;)" test -z "$(gi_missing)"
printf '<gameList>\n<game><path>Kirby&apos;s Dream &amp; Land.gb</path></game>\n</gameList>\n' > "$gi_root/gamelist.xml"
check "as &apos;, without ./" test -z "$(gi_missing)"
printf '<gameList>\n<game><path>./Kirby.gb</path></game>\n</gameList>\n' > "$gi_root/gamelist.xml"
check "another game's entry does not count" test "$(gi_missing)" = desc
gi_rel="Plain Game.gb"
printf '<gameList>\n<game><path>./Plain Game.gb</path></game>\n</gameList>\n' > "$gi_root/gamelist.xml"
jq() { echo called >> "$TMP/jq_calls"; command jq "$@"; }
rm -f "$TMP/jq_calls"
check "a plain name: found" test -z "$(gi_missing)"
check "without starting jq" test ! -e "$TMP/jq_calls"
unset -f jq
rm -f "$gi_root/gamelist.xml"
end

begin scraper_game_info_cut_download_not_kept
gi_env
wget() { # writes a PNG's first bytes, then fails like a cut transfer
    while [ $# -gt 0 ]; do [ "$1" = -O ] && out=$2; shift; done
    printf '\211PNG\r\n\032\nHALF' > "$out"
    return "${WGET_RC:-1}"
}
export WGET_RC=1; gi_fetch http://x/cover.png "$gi_root/Imgs/cut.png" png; _rc=$?; unset WGET_RC; rc $_rc
check "a download wget reports as failed is not kept" test ! -e "$gi_root/Imgs/cut.png"
check "nor its .part" test ! -e "$gi_root/Imgs/cut.png.part"
export WGET_RC=0; gi_fetch http://x/cover.png "$gi_root/Imgs/ok.png" png; _rc=$?; unset WGET_RC; rc $_rc
check "a finished one is" test -s "$gi_root/Imgs/ok.png"
unset -f wget
end

begin scraper_gamelist_not_replaced_by_a_short_list
gi_env
printf '<?xml version="1.0"?>\n<gameList>\n<game><path>./Other.sfc</path><desc>keep</desc></game>\n</gameList>\n' > "$gi_root/gamelist.xml"
cp "$gi_root/gamelist.xml" "$TMP/before.xml"
awk() { command awk "$@" | head -c 30; } # the new list written short, as on a full SD card
gi_write_desc
check "a short new list does not replace gamelist.xml" cmp -s "$gi_root/gamelist.xml" "$TMP/before.xml"
check "and is not left behind" test ! -e "$gi_root/gamelist.xml.tmp"
unset -f awk
gi_write_desc
check "a whole one does" grep -q '<path>./RPG/Trials &amp; Co.sfc</path>' "$gi_root/gamelist.xml"
check "keeping the others" grep -q '<desc>keep</desc>' "$gi_root/gamelist.xml"
end

begin scraper_gamelist_back_after_a_cut_rename
gi_env
printf '<gameList>\n<game><path>./Other.sfc</path><desc>keep</desc></game>\n</gameList>\n' > "$gi_root/gamelist.xml.tmp"
rm -f "$gi_root/gamelist.xml"
gi_write_desc
check "the whole list left in .tmp comes back" grep -q '<desc>keep</desc>' "$gi_root/gamelist.xml"
check "with the new entry" grep -q '<path>./RPG/Trials &amp; Co.sfc</path>' "$gi_root/gamelist.xml"
printf '<gameList>\n<game><path>./Half' > "$gi_root/gamelist.xml.tmp"
rm -f "$gi_root/gamelist.xml"
gi_write_desc
check "a cut .tmp (no </gameList>) is not used" sh -c "! grep -q Half '$gi_root/gamelist.xml'"
rm -f "$gi_root/gamelist.xml" "$gi_root/gamelist.xml.tmp"
end

begin battery_icon_levels_match_open_mainui
BATTERY_H="$ROOT/src/common/theme/render/battery.h"
KIDSPLAY_UI="$ROOT/src/kidsMode/kidsplay-src/kidsplay_ui.inc"
check "RedOnion: 5, 15, 45, 70, as stock MainUI and Open MainUI" sh -c "sed -n '/^ThemeImages _getBatteryRequest/,/^}/p' '$BATTERY_H' | grep -o 'percentage < [0-9]*' | tr -d -c '0-9\n' | tr '\n' ' ' | grep -q '^5 15 45 70 $'"
check "KidsPlay: the same levels" sh -c "sed -n '/kp.charging) return 5;/,/return 4;/p' '$KIDSPLAY_UI' | grep -o 'kp.battery < [0-9]*' | tr -d -c '0-9\n' | tr '\n' ' ' | grep -q '^5 15 45 70 $'"
end

begin scraper_gamelist_written_in_batches
gi_env
GI_DESC_BATCH=3
printf '<gameList>\n<game><path>./Other.sfc</path><desc>keep</desc></game>\n</gameList>\n' > "$gi_root/gamelist.xml"
cp "$gi_root/gamelist.xml" "$TMP/before.xml"
echo '<game><path>./Stale.sfc</path>' > "$gi_root/gamelist.xml.entry" # left by a scraping cut short
gi_rel="A & B.sfc"; gi_write_desc; rc1=$?
gi_rel="C.sfc"; gi_write_desc
check "the first two wait: gamelist.xml not written yet" cmp -s "$gi_root/gamelist.xml" "$TMP/before.xml"
check "and count as done" test $rc1 -eq 0
gi_rel="D.sfc"; gi_write_desc
check "the third writes all three" test "$(grep -c '<path>' "$gi_root/gamelist.xml")" -eq 4
check "in their order, before </gameList>" sh -c "awk '/A &amp; B.sfc/ { a = NR } /C.sfc/ { c = NR } /D.sfc/ { d = NR } /<\\/gameList>/ { e = NR } END { exit !(a && a < c && c < d && d < e) }' '$gi_root/gamelist.xml'"
check "the old entry kept" grep -q '<desc>keep</desc>' "$gi_root/gamelist.xml"
check "what an earlier scraping left waiting is not used" sh -c "! grep -q Stale '$gi_root/gamelist.xml'"
gi_rel="E.sfc"; gi_write_desc
gi_flush_desc
check "the end of the scraping writes the ones still waiting" grep -q '<path>./E.sfc</path>' "$gi_root/gamelist.xml"
check "nothing left behind" sh -c "! ls '$gi_root' | grep -q 'gamelist.xml\\.'"
cp "$gi_root/gamelist.xml" "$TMP/before.xml"
gi_flush_desc
check "nothing waiting: gamelist.xml not written again" cmp -s "$gi_root/gamelist.xml" "$TMP/before.xml"
check "the scraping writes every 25, and when it ends or is closed" sh -c "grep -q '^GI_DESC_BATCH=25$' '$SCRAPER_SS' && grep -q '^trap gi_flush_desc EXIT$' '$SCRAPER_SS' && grep -q '^gi_flush_desc$' '$SCRAPER_SS'"
check "and when its terminal is closed or it is stopped (and when its progress screen closes)" has_line "$SCRAPER_SS" "trap 'gi_flush_desc; exit 1' HUP INT TERM PIPE"
check "the waiting entries are not taken for ROMs" grep -q "! -name '\*.xml.entry' ! -name '\*.xml.one'" "$SCRAPER_SS"
GI_DESC_BATCH=1
end

begin scraper_game_not_found_is_remembered
gi_env
GameInfoMedia="desc,box3d"
gi_todo=$(gi_missing)
ss_nomatch=1
cover_present=false
gi_no_game
check "no cover: looked up again next time" test "$(gi_missing | tr '\n' ' ')" = "desc box3d "
cover_present=true
ss_nomatch=0
gi_no_game
check "a failed request is not a no-match" test "$(gi_missing | tr '\n' ' ')" = "desc box3d "
ss_nomatch=1
gi_no_game
check "with its cover, no match: not asked again" test -z "$(gi_missing)"
check "written down per item" test "$(grep -c 'RPG/Trials & Co.sfc' "$gi_root/media/.gameinfo_none")" -eq 2
check "reset at each lookup" sh -c "grep -A4 '^search_on_screenscraper() {' '$SCRAPER_SS' | grep -q 'ss_nomatch=0'"
eval "$(sed -n '/^search_on_screenscraper() {/,/^}/p' "$SCRAPER_SS")"
curl() { printf '%s\n' "$ss_answer"; }
for ss_answer in "Erreur : Rom/Iso/Dossier non trouvée ! " "Erreur de login : Vérifier vos identifiants utilisateur !" "Erreur : Votre quota de scrape est dépassé"; do
    url=x
    search_on_screenscraper > /dev/null 2>&1
    echo "$ss_nomatch" >> "$TMP/ss_nomatch"
done
unset -f curl
check "only 'non trouvée' is a no-match (not a login or quota error)" test "$(tr '\n' ' ' < "$TMP/ss_nomatch")" = "1 0 0 "
check "used where a ROM is given up" test "$(grep -c '^				*gi_no_game$' "$SCRAPER_SS")" -eq 2
rm -rf "$gi_root/media"
end

begin scraper_game_info_missing_and_urls
gi_env
GameInfoMedia="desc,box2dfront,box2dback,box3d,screenshot,video,manual"
check "everything missing at first" test "$(gi_missing | tr '\n' ' ')" = "desc box2dfront box2dback box3d screenshot video manual "
mkdir -p "$gi_root/media/box3d" "$gi_root/media/videos"
touch "$gi_root/media/box3d/Trials & Co.png" "$gi_root/media/videos/Trials & Co.mp4"
check "files there are not missing" test "$(gi_missing | tr '\n' ' ')" = "desc box2dfront box2dback screenshot manual "
printf '<gameList>\n<game><path>RPG/Trials &amp; Co.sfc</path></game>\n</gameList>\n' > "$gi_root/gamelist.xml"
check "another scraper's entry without ./ counts too" test "$(gi_missing | tr '\n' ' ')" = "box2dfront box2dback screenshot manual "
rm -f "$gi_root/gamelist.xml"
GameInfoMedia="box2dfront"
check "only the wanted ones" test "$(gi_missing)" = "box2dfront"
GameInfoMedia=""
check "none wanted: nothing missing" test -z "$(gi_missing)"
check "media URL in the region order (us before jp)" test "$(gi_media_url box-2D)" = "https://x/box2d_us?a=1"
check "media of another region when there is no better one" test "$(gi_media_url box-2D-back)" = "https://x/back_eu?a=1"
check "no media of a type: nothing" test -z "$(gi_media_url box-3D)"
gi_medias=null
check "no medias at all: nothing" test -z "$(gi_media_url box-2D)"
end

begin scraper_game_info_downloads
gi_env
mkdir -p "$TMP/bin"
# wget: the content depends on the URL (-O file url).
cat > "$TMP/bin/wget" << 'MOCK'
#!/bin/sh
while [ $# -gt 1 ]; do [ "$1" = "-O" ] && out=$2; shift; done
url=$1
echo "$url" >> "$(dirname "$0")/../wget_urls"
case "$url" in
    *back_eu*) printf '\377\330\377\340JFIF' > "$out" ;;
    *ss\?*) printf '\211PNG\r\n\032\nDATA' > "$out" ;;
    *title*) printf '<html>quota</html>' > "$out" ;;
    *box2d_us*) printf '\211PNG\r\n\032\nDATA' > "$out" ;;
    *video*) printf '\000\000\000\030ftypmp42DATA' > "$out" ;;
    *manual*) printf '%%PDF-1.4 DATA' > "$out" ;;
esac
MOCK
# jpg2png_stb: a PNG out of the JPEG.
printf '#!/bin/sh\nprintf "\\211PNG\\r\\n\\032\\nCONV" > "$2"\n' > "$TMP/bin/jpg2png_stb"
chmod +x "$TMP/bin/"*
gi_oldpath=$PATH
PATH="$TMP/bin:$PATH"
GameInfoMedia="box2dfront,box2dback,box3d,screenshot,video,manual"
gi_download "$(gi_missing)" > "$TMP/out"
M="$gi_root/media"
check "box front as PNG" sh -c "head -c 4 '$M/box2dfront/Trials & Co.png' | od -An -tx1 | tr -d ' \n' | grep -q '^89504e47$'"
check "picture asked at the screen's size, as PNG" grep -q 'box2d_us?a=1&maxwidth=752&maxheight=560&outputformat=png' "$TMP/wget_urls"
check "JPEG box back turned into PNG" grep -q CONV "$M/box2dback/Trials & Co.png"
check "screenshot saved" test -s "$M/screenshot/Trials & Co.png"
check "a title screen that is a page of text is not kept" test ! -e "$M/screenshot/Trials & Co-1.png"
check "no stray .part files" sh -c "! find '$M' -name '*.part*' | grep -q ."
check "video (normalized) as mp4" grep -q ftyp "$M/videos/Trials & Co.mp4"
check "manual as pdf" grep -q '%PDF' "$M/manuals/Trials & Co.pdf"
check "no 3D box on ScreenScraper: none made" test ! -e "$M/box3d/Trials & Co.png"
check "counted: 5 got, 1 not found" test "$GameInfo_Success $GameInfo_Fail" = "5 1"
check "what ScreenScraper doesn't have is written down" has_line "$M/.gameinfo_none" 'box3d RPG/Trials & Co.sfc'
check "and not asked again" test -z "$(gi_missing)"
check "a failed download (title page) is not written down" sh -c "! grep -q screenshot '$M/.gameinfo_none'"
check "each item reported" sh -c "grep -q 'Game Info box3d: not found' '$TMP/out' && grep -q 'Game Info manual: OK' '$TMP/out'"
rm -f "$M/box2dfront/Trials & Co.png"
printf '#!/bin/sh\nwhile [ $# -gt 1 ]; do [ "$1" = "-O" ] && out=$2; shift; done\nprintf "<html>no</html>" > "$out"\n' > "$TMP/bin/wget"
GameInfoMedia=box2dfront
gi_download box2dfront > /dev/null
check "an error page instead of a picture: nothing kept" test ! -e "$M/box2dfront/Trials & Co.png"
PATH=$gi_oldpath
end

begin scraper_game_info_menu_and_config
eval "$(sed -n '/^GI_ITEMS=/,/^Menu_Config_GameInfo()/p' "$SCRAPER_MENU" | grep -v '^Menu_Config_GameInfo')"
check "switch one off" test "$(gi_toggle desc,box2dfront,screenshot box2dfront)" = "desc,screenshot"
check "switch one on, in the menu's order" test "$(gi_toggle desc,screenshot video)" = "desc,screenshot,video"
check "the last one off: none (not the default again)" test "$(gi_toggle video video)" = "none"
check "from none" test "$(gi_toggle none manual)" = "manual"
check "menu: Game Info entry in the configuration" grep -q 'Menu_Config_GameInfo ;;' "$SCRAPER_MENU"
check "menu: the current ROM can be scraped again (Game Info media)" has_line "$SCRAPER_MENU" 'Option2="Scrape current rom: $romname"'
check "same default list in the menu and the scraper" test "$(grep '^GI_DEFAULT=' "$SCRAPER_MENU")" = "$(grep '^GI_DEFAULT=' "$SCRAPER_SS")"
check "default config has the key" sh -c "jq -e '.ScreenscraperGameInfo == \"desc,box2dfront,box2dback,box3d,screenshot\"' '$ROOT/static/configs/.tmp_update/config/scraper.json' > /dev/null"
check "config_repair adds it to old configs" grep -q 'if .ScreenscraperGameInfo == null then .ScreenscraperGameInfo = "desc,box2dfront,box2dback,box3d,screenshot"' "$ROOT/static/build/.tmp_update/script/scraper/config_repair.sh"
check "scraper: Game Info after the cover, whatever happened to it" grep -q '\[ -n "\$gi_todo" \] && gi_download "\$gi_todo"' "$SCRAPER_SS"
check "scraper: a ROM is skipped only with its cover and all its Game Info" grep -q 'if \[ "\$cover_present" = true \] && \[ -z "\$gi_todo" \]; then' "$SCRAPER_SS"
# The game's result on the progress screen when no line says it: a cover
# already there with new Game Info media is FOUND (it counted nowhere,
# beta 6), with nothing new ALREADY THERE; no cover and no picture or a
# failed download: NOT FOUND, unless Game Info media came
sed -n '/# Something new for the game: found, even without a new cover./,/^        fi$/p' "$SCRAPER_SS" > "$TMP/gi_result.sh"
check "the result block is found" test -s "$TMP/gi_result.sh"
gi_res() { # result set by the cover, Game Info before, after
    (gi_result=$1 gi_before=$2 GameInfo_Success=$3 PROGRESS_UI=1; . "$TMP/gi_result.sh")
}
check "cover there, Game Info downloaded: found" test "$(gi_res skipped 4 6)" = "@@found"
check "cover there, nothing new: skipped" test "$(gi_res skipped 4 4)" = "@@skipped"
check "no cover found, Game Info downloaded: found" test "$(gi_res notfound 0 1)" = "@@found"
check "no cover found, nothing: not found" test "$(gi_res notfound 0 0)" = "@@notfound"
check "a new cover (Scraped!, counted by progressUI): nothing more" test -z "$(gi_res '' 0 3)"
check "in the terminal: no @@ line" test -z "$( (gi_result=skipped gi_before=0 GameInfo_Success=1 PROGRESS_UI=; . "$TMP/gi_result.sh") )"
check "each case sets its result" sh -c 'test "$(grep -c "gi_result=\(skipped\|notfound\)$" "$1")" -eq 3' _ "$SCRAPER_SS"
unset -f gi_res
end

# ---- runtime.sh: MainUI return path ----

eval "$(extract_fn "$RUNTIME" perf_dirty)"

begin perf_dirty_only_with_logging
sysdir=$TMP
mkdir -p "$sysdir/config"
perf_dirty
check "empty without logging" test -z "$perf_dirty_kb"
touch "$sysdir/config/.logging"
perf_dirty
case "$perf_dirty_kb" in *Dirty=*kB*Writeback=*kB*) ok=0 ;; *) ok=1 ;; esac
check "Dirty and Writeback with logging" test $ok -eq 0
end

begin mainui_return_single_sync
fn=$(extract_fn "$RUNTIME" launch_main_ui)
check "one global sync per MainUI cycle" test "$(printf '%s\n' "$fn" | grep -c '^ *sync$')" -eq 1
end

# ---- runtime.sh: cmd_with_rom_path (vs the rev|sed pipeline) ----

eval "$(extract_fn "$RUNTIME" cmd_with_rom_path)"

pipeline_rom_path() { # the multi-line fallback, as in launch_game
    _prp=$(echo "$1" | rev | sed 's/^"[^"]*"//g' | rev)"\"$2\""
    printf '%s\n' "$_prp"
}

same_as_pipeline() { # command, new path
    cmd_with_rom_path "$1" "$2"
    [ "$cmd_with_rom" = "$(pipeline_rom_path "$1" "$2")" ]
}

begin cmd_with_rom_path_matches_pipeline
R=/mnt/SDCARD/Roms/GBA/Game.gba
check "RetroArch command" same_as_pipeline 'LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so "/mnt/SDCARD/Emu/GBA/../../.tmp_update/proxy.sh" "/mnt/SDCARD/Emu/GBA/../../Roms/GBA/Game.gba"' "$R"
check "custom launch script" same_as_pipeline 'LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so "/mnt/SDCARD/Emu/X/launch.sh" "/mnt/SDCARD/Roms/X/a b (c).zip"' "$R"
check "empty last field" same_as_pipeline 'run "a" ""' "$R"
check "no closing quote at the end" same_as_pipeline 'run "a" b' "$R"
check "single quote only" same_as_pipeline 'run b"' "$R"
check "no quotes" same_as_pipeline 'run b' "$R"
check "path with \$ and spaces" same_as_pipeline 'run "x" "/mnt/SDCARD/Roms/A/\$weird name.zip"' '/mnt/SDCARD/Roms/A/$weird name.zip'
end

# ---- the Scraper, however its menu ended, gives standby and the CPU back ----

begin scraper_leaves_no_stay_awake
SCRAPER_GLO="$ROOT/static/build/App/romscripts/Scraper.sh"
mkdir -p "$TMP/sys/bin" "$TMP/stub"
# The terminal: the menu started a scraping and was left early
printf '#!/bin/sh\ntouch "%s/stay_awake"\necho performance > "%s/governor"\n[ -f "%s/background" ] && echo "echo job >> %s/job_log" > "%s/scraper_script.sh"\nexit 0\n' \
    "$TMP" "$TMP" "$TMP" "$TMP" "$TMP" > "$TMP/sys/bin/st"
printf '#!/bin/sh\n:\n' > "$TMP/stub/pressMenu2Kill"
printf '#!/bin/sh\n:\n' > "$TMP/stub/pkill"
chmod +x "$TMP/sys/bin/st" "$TMP"/stub/*
# The new UI runs the menu without the terminal: the same stand-in
mkdir -p "$TMP/sys/script/scraper"
cp "$TMP/sys/bin/st" "$TMP/sys/script/scraper/menu.sh"
sed -e "s#/mnt/SDCARD/.tmp_update#$TMP/sys#g" -e "s#/tmp/stay_awake#$TMP/stay_awake#g" \
    -e "s#/tmp/scraper_script.sh#$TMP/scraper_script.sh#g" \
    -e "s#/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor#$TMP/governor#g" "$SCRAPER_GLO" > "$TMP/Scraper.sh"
for mode in native terminal; do
    term=; [ $mode = terminal ] && term=1
    rm -f "$TMP/stay_awake" "$TMP/background" "$TMP/job_log" "$TMP/governor"
    SCRAPER_TERMINAL=$term PATH="$TMP/stub:$PATH" sh "$TMP/Scraper.sh" GB "" > /dev/null 2>&1
    check "$mode: no stay_awake left" test ! -e "$TMP/stay_awake"
    check "$mode: governor back to ondemand" test "$(cat "$TMP/governor")" = ondemand
    touch "$TMP/background"
    SCRAPER_TERMINAL=$term PATH="$TMP/stub:$PATH" sh "$TMP/Scraper.sh" GB "" > /dev/null 2>&1
    n=0; while [ ! -f "$TMP/job_log" ] && [ $n -lt 50 ]; do sleep 0.1; n=$((n + 1)); done; sleep 0.2
    check "$mode: background job run, then the governor back" test -f "$TMP/job_log" -a "$(cat "$TMP/governor")" = ondemand
done
end

# ---- time sync: the RTC is written in UTC, by both ways ----

begin time_sync_writes_the_rtc_in_utc
gt_run() { # api|ntp -> the hwclock calls
    rm -f "$TMP/hw"
    (
        eval "$(extract_fn "$NETWORK" get_time | sed "s#/tmp/ntp_synced#$TMP/ntp_synced#g")"
        sysdir=$TMP
        log() { :; }
        flag_enabled() { return 1; }
        playActivity() { :; }
        play_activity_resume() { :; }
        store_tz() { :; }
        jq() { :; }
        date() { return 0; }
        hwclock() { echo "hwclock $*" >> "$TMP/hw"; }
        ntpdate() { return 0; }
        if [ "$1" = api ]; then
            curl() { printf 'utc_datetime: 2026-11-21T03:00:00.000000+00:00\nutc_offset: -05:00\n'; }
        else
            curl() { :; }
        fi
        get_time
    ) > /dev/null 2>&1
    cat "$TMP/hw" 2> /dev/null
}
check "web API: hwclock -w -u" test "$(gt_run api)" = "hwclock -w -u"
check "NTP fallback: hwclock -w -u" test "$(gt_run ntp)" = "hwclock -w -u"
unset -f gt_run
end

# ---- System log snapshot: no Wi-Fi name or key in the archive ----

begin snapshot_redacts_wifi_keys
eval "$(extract_fn "$ROOT/static/build/.tmp_update/script/diagnostics/util_snapshot.sh" get_wpa_supplicant)"
wpa_conf_path=$TMP/wpa_supplicant.conf
cat > "$wpa_conf_path" << 'WPA'
ctrl_interface=/var/run/wpa_supplicant
update_config=1
network={
	ssid="HomeNet"
	psk="typed-passphrase-1"
	key_mgmt=WPA-PSK
}
network={
	ssid="Second"
	psk=5e884898da28047151d0e56f8dc6292773603d0d6aabbdd62a11ef721d1542d8
}
network={
	ssid="Third"
	psk="first"SECOND-HALF"
}
network={
	ssid=436166c3a9
	key_mgmt=SAE
	sae_password="sae-secret-3"
}
#	psk="old-commented-key"
network={
	ssid="Corp"
	key_mgmt=WPA-EAP
	identity="someone@example.org"
	password="eap-secret-4"
	wep_key0=0102030405
}
WPA
out=$(get_wpa_supplicant)
leak=""
for secret in HomeNet typed-passphrase-1 5e884898da28 SECOND-HALF 436166c3a9 sae-secret-3 old-commented-key someone@example.org eap-secret-4 0102030405 Corp Third Second; do
    case "$out" in *"$secret"*) leak="$leak $secret" ;; esac
done
check "no name or key left:${leak:- none}" test -z "$leak"
check "the rest of the file is kept" sh -c 'printf "%s\n" "$1" | grep -q "^update_config=1$" && printf "%s\n" "$1" | grep -q "key_mgmt=SAE"' _ "$out"
unset -f get_wpa_supplicant
end

# ---- LaunchBox box art: scaled without the graphics hardware ----
# jpg2png and pngScale took video memory without checking it; a big
# LaunchBox picture hung a Mini Flip until a 20 s POWER reset.

begin scraper_launchbox_box_art_in_software
lb="$ROOT/static/build/.tmp_update/script/scraper/scrap_launchbox.sh"
code=$(grep -vE '^[[:space:]]*#' "$lb")
check "no jpg2png (graphics hardware) call" sh -c '! printf "%s\n" "$1" | grep -qE "(^|[^_[:alnum:]])jpg2png[[:space:]]"' _ "$code"
check "JPEG box art goes through jpg2png_stb" sh -c 'printf "%s\n" "$1" | grep -q "jpg2png_stb \"\$img.dl.\$mediaextension\" \"\$img.tmp.png\" 250 360"' _ "$code"
check "the game's picture only by a rename (MENU can cut a write)" sh -c '
    [ "$(printf "%s\n" "$1" | grep -c "mv -f \"\$img.tmp.png\" \"\$img.png\"")" -eq 2 ]' _ "$code"
check "every pngScale call is PNGSCALE_SOFTWARE=1" sh -c '
    all=$(printf "%s\n" "$1" | grep -c "pngScale ")
    soft=$(printf "%s\n" "$1" | grep -c "PNGSCALE_SOFTWARE=1 pngScale ")
    [ "$all" -ge 1 ] && [ "$all" -eq "$soft" ]' _ "$code"
check "a picture that could not be scaled is not kept" sh -c 'printf "%s\n" "$1" | grep -q "Picture could not be converted"' _ "$code"
end

# ---- no grep -x in what is shipped: BusyBox 1.20.2 (the firmware's) ----
# never matches with -x, and with -F it searches for a substring

begin shipped_scripts_have_no_grep_x
found=$(grep -rnE 'grep( +-[a-zA-Z]+)* +-[a-zA-Z]*x' "$ROOT/static" "$ROOT/src" "$ROOT/test" --include='*.sh' 2> /dev/null |
    grep -vE '^[^:]+:[0-9]+:[[:space:]]*#' | sed "s#^$ROOT/##" | head -n 3)
check "a whole-line grep (-x) is used in: ${found:-nothing}" test -z "$found"
# The forms used instead, as the console's tools run them
check "whole line with ^...\$" sh -c "printf 'x abc\nabc\n' | grep -q '^abc\$'"
check "the newline-wrapped case: a whole line only" sh -c '
    l=".name_
.other"
    case "
$l
" in *"
.name
"*) exit 1 ;; esac
    case "
$l
" in *"
.name_
"*) exit 0 ;; esac
    exit 1'
# The two helpers that replaced grep -xF
eval "$(extract_fn "$ROOT/static/packages/Emu/Nintendo - GBA (gpSP)/Emu/GBA/launch.sh" needs_migration)"
eval "$(extract_fn "$ROOT/static/build/.tmp_update/script/scraper/scrap_screenscraper.sh" gi_has_line)"
not_needed() { ! needs_migration; }
not_a_line() { ! gi_has_line "$@"; }
migration_log=$TMP/migration.log
printf 'Super Mario.gba\nZelda [!].gba' > "$migration_log"
romname='Mario.gba'
check "gpSP: a name inside another is not listed" not_needed
romname='Zelda [!].gba'
check "gpSP: the whole name, last line without a line end" needs_migration
migration_log=$TMP/none
check "gpSP: no migration log" not_needed
printf 'box3d GB/Tetris.gb\n' > "$TMP/gameinfo_none"
check "scraper: the whole line" gi_has_line 'box3d GB/Tetris.gb' "$TMP/gameinfo_none"
check "scraper: part of a line is not it" not_a_line 'box3d GB/Tetris' "$TMP/gameinfo_none"
unset -f not_needed not_a_line
unset -f needs_migration gi_has_line
end

# ---- model tests compare with the value ($MODEL_MM), not the word ----

begin model_tests_use_the_value
found=$(grep -rnE '(-eq|-ne|-gt|-lt|-ge|-le) +MODEL_' "$ROOT/static" "$ROOT/src" --include='*.sh' 2> /dev/null |
    grep -vE '^[^:]+:[0-9]+:[[:space:]]*#' | sed "s#^$ROOT/##" | head -n 3)
check "MODEL_ compared without \$ in: ${found:-nothing}" test -z "$found"
check "the installer's charger set-up is for the Mini" \
    grep -q '^    if \[ \$DEVICE_ID -eq \$MODEL_MM \]; then$' "$ROOT/static/dist/miyoo/app/.tmp_update/install.sh"
end

# ---- no sed -E in what is shipped: BusyBox 1.20.2's sed knows only -r ----

begin shipped_scripts_have_no_sed_E
found=$(grep -rnE 'sed( +-[a-zA-Z]+)* +-[a-zA-Z]*E' "$ROOT/static" "$ROOT/src" --include='*.sh' 2> /dev/null |
    grep -vE '^[^:]+:[0-9]+:[[:space:]]*#' | sed "s#^$ROOT/##" | head -n 3)
check "sed -E is used in: ${found:-nothing}" test -z "$found"
end

# ---- runtime.sh: a value that is not a number does not stop the boot ----

begin boot_survives_bad_time_and_pwm_files
# update_time: a syntax error in $(( )) ended runtime.sh at every boot
ut_run() { # currentTime.txt content, addHours content -> the time set last
    rm -rf "$TMP/sd" "$TMP/date_log"
    mkdir -p "$TMP/sd/Saves/CurrentProfile/saves" "$TMP/sys/config/startup"
    printf '%b' "$1" > "$TMP/sd/Saves/CurrentProfile/saves/currentTime.txt"
    printf '%b' "$2" > "$TMP/sys/config/startup/addHours"
    (
        eval "$(extract_fn "$RUNTIME" update_time | sed "s#/mnt/SDCARD#$TMP/sd#g; s#/tmp/rtc_available#$TMP/rtc#g")"
        sysdir=$TMP/sys
        date() { case "$*" in "+%s") echo 5 ;; *) echo "$*" >> "$TMP/date_log" ;; esac; }
        playActivity() { :; }
        log() { :; }
        update_time
        echo "boot goes on" >> "$TMP/date_log"
    ) 2> /dev/null
    tail -n 2 "$TMP/date_log" | tr '\n' ' '
}
check "valid files: time + hours" test "$(ut_run '1000\n' '2\n')" = "+%s -s @8200 boot goes on "
check "no line end" test "$(ut_run '1000' '2')" = "+%s -s @8200 boot goes on "
check "currentTime.txt not a number: from 0" test "$(ut_run 'LD_PRELOAD=x\n' '2\n')" = "+%s -s @7200 boot goes on "
check "currentTime.txt with two lines: the first" test "$(ut_run '1000\n2000\n' '2\n')" = "+%s -s @8200 boot goes on "
check "addHours empty: 4 hours" test "$(ut_run '1000\n' '')" = "+%s -s @15400 boot goes on "
check "addHours 4.5: 4 hours" test "$(ut_run '1000\n' '4.5\n')" = "+%s -s @15400 boot goes on "
check "addHours negative" test "$(ut_run '10000\n' '-1\n')" = "+%s -s @6400 boot goes on "
# The backlight's PWM frequency (init_system)
pwm_run() { # .pwmfrequency content -> frequency
    mkdir -p "$TMP/sys/config"
    printf '%b' "$1" > "$TMP/sys/config/.pwmfrequency"
    (
        sysdir=$TMP/sys
        eval "$(sed -n '/^    pwmfile="\$sysdir\/config\/.pwmfrequency"$/,/^    fi$/p' "$RUNTIME")"
        echo "$frequency"
    ) 2> /dev/null
}
check "pwm 3: 400 Hz" test "$(pwm_run '3\n')" = 400
check "pwm not a digit: 800 Hz" test "$(pwm_run 'x\n')" = 800
check "pwm 12: 800 Hz" test "$(pwm_run '12\n')" = 800
check "pwm empty: 800 Hz" test "$(pwm_run '')" = 800
unset -f ut_run pwm_run
end

# ---- runtime.sh: get_full_resolution_path (vs the grep|sed pipeline) ----

begin full_resolution_path_matches_pipeline
eval "$(extract_fn "$RUNTIME" get_full_resolution_path | sed "s#/tmp/new_res_available#$TMP/new_res_available#g")"
sysdir=$TMP
touch "$TMP/new_res_available"
frp_same() { # command: the fast path gives what the pipeline gives
    printf '%s\n' "$1" > "$TMP/cmd_to_run.sh"
    _want=$(grep -o '".*launch\.sh"' "$TMP/cmd_to_run.sh" | sed 's/"//g; s/launch\.sh/full_resolution/')
    [ "$(get_full_resolution_path)" = "$_want" ]
}
check "DraStic (a ROM name with spaces and brackets)" \
    frp_same 'LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so "/mnt/SDCARD/Emu/NDS/launch.sh" "/mnt/SDCARD/Roms/NDS/Mario Kart DS (USA).nds"'
check "the exact path for DraStic" \
    test "$(get_full_resolution_path)" = /mnt/SDCARD/Emu/NDS/full_resolution
check "two launch.sh: up to the last one, the first replaced" \
    frp_same 'run "/mnt/SDCARD/Emu/A/launch.sh" "/mnt/SDCARD/Roms/A/launch.sh" x'
check "RetroArch (no launch.sh): nothing" \
    frp_same 'LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so "/mnt/SDCARD/Emu/GBA/../../.tmp_update/proxy.sh" "/mnt/SDCARD/Roms/GBA/Game.gba"'
rm -f "$TMP/new_res_available"
check "no 752x560 mode: nothing" test -z "$(get_full_resolution_path)"
unset -f frp_same get_full_resolution_path
end

# ---- runtime.sh: cleanup_appendconfig ----

eval "$(extract_fn "$RUNTIME" cleanup_appendconfig)"

appendconfig_env() {
    sysdir=$TMP
    log() { :; }
    printf '%s\n' "$1" > "$sysdir/cmd_to_run.sh"
    printf '%s\n' "$2" > "$TMP/launch.sh"
}

begin cleanup_appendconfig_reset
appendconfig_env 'run "core" --appendconfig "/tmp/reset.cfg" "rom"' './retroarch -L core --appendconfig "/tmp/reset.cfg" "$1"'
cleanup_appendconfig "$TMP/launch.sh"
check "cmd cleaned" test "$(cat "$sysdir/cmd_to_run.sh")" = 'run "core" "rom"'
check "launch.sh cleaned" test "$(cat "$TMP/launch.sh")" = './retroarch -L core "$1"'
end

begin cleanup_appendconfig_auto_load
appendconfig_env 'run "core" --appendconfig "/tmp/auto_load_state.cfg" "rom"' 'x --appendconfig "/tmp/auto_load_state.cfg" y'
cleanup_appendconfig "$TMP/launch.sh"
check "cmd cleaned" test "$(cat "$sysdir/cmd_to_run.sh")" = 'run "core" "rom"'
check "launch.sh cleaned" test "$(cat "$TMP/launch.sh")" = 'x y'
end

begin cleanup_appendconfig_nothing_to_do
appendconfig_env 'run "core" "rom"' './retroarch "$1"'
cleanup_appendconfig "$TMP/launch.sh"
check "cmd unchanged" test "$(cat "$sysdir/cmd_to_run.sh")" = 'run "core" "rom"'
check "launch.sh unchanged" test "$(cat "$TMP/launch.sh")" = './retroarch "$1"'
end

# ---- runtime.sh: start_audioserver (cached pid) ----

eval "$(extract_fn "$RUNTIME" start_audioserver)"

begin audioserver_cached_pid_skips_pgrep
# A script named audioserver: the kernel names its process after the file.
# (A copy of sleep did the same until sleep became a multi-call binary, as
# with uutils on newer Ubuntu: run under another name it just exits.)
printf '#!/bin/sh\nwhile :; do sleep 1; done\n' > "$TMP/audioserver"
chmod +x "$TMP/audioserver"
"$TMP/audioserver" &
as_pid=$!
started=0
runifnecessary() { started=$((started + 1)); }
audioserver_pid=""
# runs in a $(...) subshell: count in a file. The process is found by its
# name in /proc (pgrep -x never matches in BusyBox 1.20.2, and its pgrep
# NAME looks at the first word of the command line: /bin/sh for a script).
pgrep() {
    echo >> "$TMP/pgrep_calls"
    for _pg in /proc/[0-9]*; do
        read -r _pg_comm < "$_pg/comm" 2> /dev/null && [ "$_pg_comm" = audioserver ] && echo "${_pg#/proc/}"
    done
    return 0
}
pgrep_calls() { wc -l < "$TMP/pgrep_calls"; }
start_audioserver
check "found by pgrep the first time" test "$audioserver_pid" = "$as_pid"
start_audioserver
start_audioserver
check "later calls use /proc, not pgrep" test "$(pgrep_calls)" -eq 1
check "never started a second server" test $started -eq 0
kill $as_pid 2> /dev/null
wait $as_pid 2> /dev/null
start_audioserver 2> /dev/null # starting path: no jsonval on the host
check "gone: pgrep again, then started" test "$(pgrep_calls)" -eq 2
check "started once" test $started -eq 1
unset -f pgrep pgrep_calls runifnecessary
end

# ---- runtime.sh: read_file_to (vs $(cat)) ----

eval "$(extract_fn "$RUNTIME" read_file_to)"

same_as_cat() { # printf format for the file content
    printf "$1" > "$TMP/f"
    read_file_to got "$TMP/f"
    [ "$got" = "$(cat "$TMP/f")" ]
}

begin read_file_to_matches_cat
check "single line" same_as_cat 'LD_PRELOAD=x "a b" "c"\n'
check "no final newline" same_as_cat 'abc'
check "empty file" same_as_cat ''
check "trailing blank lines" same_as_cat 'a\n\n\n'
check "blank lines inside" same_as_cat 'a\n\nb\n'
check "leading and trailing spaces" same_as_cat '  a b  \n  c\t\n'
check "backslashes and quotes" same_as_cat 'a\\\\n "b\\\\" $x `y`\n'
check "CRLF" same_as_cat 'a\r\nb\r\n'
read_file_to got "$TMP/missing"
check "missing file is empty" test -z "$got"
check "update_networking.sh has the same helper" test "$(extract_fn "$NETWORK" read_file_to)" = "$(extract_fn "$RUNTIME" read_file_to)"
end

# ---- update_networking.sh: store_tz ----

eval "$(extract_fn "$NETWORK" store_tz)"
eval "$(extract_fn "$NETWORK" set_tzid)"

begin store_tz_writes_only_changes
sysdir=$TMP
mkdir -p "$sysdir/config"
sync() { echo >> "$TMP/syncs"; }
syncs() { [ -f "$TMP/syncs" ] && wc -l < "$TMP/syncs" || echo 0; }
store_tz "UTC-02:00"
check ".tz written" test "$(cat "$sysdir/config/.tz")" = "UTC-02:00"
check ".tz_sync written" test "$(cat "$sysdir/config/.tz_sync")" = "UTC-02:00"
check "TZ applied" test "$TZ" = "UTC-02:00"
check "one sync" test "$(syncs)" -eq 1
store_tz "UTC-02:00"
check "same zone: no write, no sync" test "$(syncs)" -eq 1
rm "$sysdir/config/.tz_sync"
store_tz "UTC-02:00"
check "missing .tz_sync: written again" test "$(syncs)" -eq 2
store_tz "UTC+05:30"
check "new zone: written" test "$(cat "$sysdir/config/.tz")" = "UTC+05:30"
check "new zone: synced" test "$(syncs)" -eq 3
unset -f sync syncs
unset TZ
end

# ---- ota_update.sh: which release each channel installs ----

OTA="$ROOT/static/build/.tmp_update/script/ota_update.sh"
if command -v jq > /dev/null; then
    eval "$(extract_fn "$OTA" get_release_info)"
    eval "$(grep '^get_version()' "$OTA")"
    eval "$(grep '^build_date()' "$OTA")"
    eval "$(grep '^PACKAGE_PREFIXES=' "$OTA")"

    ota_asset() { # tag, prerelease (true/false), published_at (default: from the tag date)
        _pub=${3:-$(echo "$1" | sed 's/^.*-\([0-9]\{4\}\)\([0-9]\{2\}\)\([0-9]\{2\}\)-.*$/\1-\2-\3T12:00:00Z/')}
        printf '{"tag_name":"%s","prerelease":%s,"draft":false,"published_at":"%s","body":"","assets":[{"name":"OnionPlus-v%s.zip","browser_download_url":"https://x/%s.zip","size":1048576,"created_at":"2026-09-27T00:00:00Z"}]}' "$1" "$2" "$_pub" "$1" "$1"
    }
    eval "$(grep '^TESTERS_REPOSITORY=' "$OTA")"
    curl() { # the last argument is the URL
        for _u; do :; done
        case "$_u" in
            */releases/latest) cat "$TMP/latest.json" ;;
            */"$TESTERS_REPOSITORY"/releases) cat "$TMP/testers.json" ;;
            */releases) cat "$TMP/list.json" ;;
        esac
    }
    installUI() { echo "4.4.0-beta-20260926-aaaaaaaa"; }
    GITHUB_REPOSITORY=Amiga500/Onion

    begin ota_beta_takes_newest_build
    printf '[%s,%s]' "$(ota_asset 4.4.0-beta-20260927-bbbbbbbb false)" "$(ota_asset 4.4.0-beta-20260920-cccccccc true)" > "$TMP/list.json"
    channel=beta
    get_release_info > /dev/null
    check "no prerelease newer: newest release offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.4.0-beta-20260927-bbbbbbbb
    printf '[%s,%s]' "$(ota_asset 4.4.0-beta-20260928-dddddddd true)" "$(ota_asset 4.4.0-beta-20260927-bbbbbbbb false)" > "$TMP/list.json"
    get_release_info > /dev/null
    check "newer prerelease offered" test "$Release_FullVersion" = 4.4.0-beta-20260928-dddddddd
    # GitHub's order on 8 Oct 2026: the stable release listed before a beta
    # published 22 hours later.
    printf '[%s,%s,%s]' "$(ota_asset 4.4.0-beta-20261007-9e96c27c false 2026-10-07T02:47:19Z)" \
        "$(ota_asset 4.4.0-beta-20261008-1168b7bf true 2026-10-08T00:17:19Z)" \
        "$(ota_asset 4.4.0-beta-20261007-06ffca3f true 2026-10-07T03:18:18Z)" > "$TMP/list.json"
    get_release_info > /dev/null
    check "newest published offered, whatever GitHub lists first" test "$Release_FullVersion" = 4.4.0-beta-20261008-1168b7bf
    printf '[%s]' "$(ota_asset 4.4.0-beta-20261007-9e96c27c false)" | sed 's/"draft":false/"draft":true/' > "$TMP/list.json"
    get_release_info > /dev/null
    check "draft only: nothing offered" test "$?" -eq 1
    echo '[]' > "$TMP/list.json"
    get_release_info > /dev/null
    check "no release at all: nothing offered" test "$?" -eq 1
    end

    begin ota_testers_channel_reads_its_own_repository
    # The testers' builds are released in their own repository: the beta
    # channel (and the updaters already out there) never sees them.
    check "testers repository is not the main one" test -n "$TESTERS_REPOSITORY" -a "$TESTERS_REPOSITORY" != "$GITHUB_REPOSITORY"
    installUI() { echo "4.5.1-b2-20261119-6ec827a0"; }
    printf '[%s]' "$(ota_asset 4.5.1-b2-20261121-1a2b3c4d true)" > "$TMP/testers.json"
    printf '[%s]' "$(ota_asset 4.5.1-b2-20261119-6ec827a0 true)" > "$TMP/list.json"
    channel=testers
    get_release_info > /dev/null
    check "testers: the testers' newest build offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.1-b2-20261121-1a2b3c4d
    channel=beta
    get_release_info > /dev/null
    check "beta: the testers' build not offered" test "$?" -eq 1
    echo '[]' > "$TMP/testers.json"
    channel=testers
    get_release_info > /dev/null
    check "testers: no release, nothing offered" test "$?" -eq 1
    installUI() { echo "4.4.0-beta-20260926-aaaaaaaa"; }
    # The saved channel: testers only with config/.otaTesters
    _ota_sysdir=$sysdir
    sysdir=$TMP/sys; mkdir -p "$sysdir/config"
    ota_read_channel() { eval "$(sed -n '/^channel=\$(cat /,/^esac/p' "$OTA")"; echo "$channel"; }
    echo testers > "$sysdir/config/ota_channel"
    check "saved testers without the flag: beta" test "$(ota_read_channel)" = beta
    touch "$sysdir/config/.otaTesters"
    check "saved testers with the flag: testers" test "$(ota_read_channel)" = testers
    echo beta > "$sysdir/config/ota_channel"
    check "saved beta: beta" test "$(ota_read_channel)" = beta
    rm -f "$sysdir/config/ota_channel"
    check "nothing saved: stable" test "$(ota_read_channel)" = stable
    # The menu: testers listed and saved only with the flag
    eval "$(extract_fn "$OTA" channel_choice)"
    clear() { :; }
    ota_menu() { echo "$1" > "$TMP/menu_items"; menu_choice=$(cat "$TMP/menu_pick"); }
    echo testers > "$TMP/menu_pick"
    ( channel_choice ); rc=$?
    check "with the flag: testers in the menu" sh -c 'printf "%b" "$(cat "$1")" | grep -q "^testers\$"' _ "$TMP/menu_items"
    check "with the flag: testers saved" test $rc -eq 0 -a "$(cat "$sysdir/config/ota_channel")" = testers
    rm -f "$sysdir/config/.otaTesters" "$sysdir/config/ota_channel"
    ( channel_choice ); rc=$?
    check "without the flag: menu stable and beta only" test "$(printf '%b' "$(cat "$TMP/menu_items")" | tr '\n' ' ')" = "stable beta"
    check "without the flag: testers not saved" test ! -e "$sysdir/config/ota_channel"
    echo beta > "$TMP/menu_pick"
    ( channel_choice )
    check "beta still saved" test "$(cat "$sysdir/config/ota_channel")" = beta
    unset -f clear ota_menu ota_read_channel channel_choice
    sysdir=$_ota_sysdir
    end

    begin ota_stable_takes_latest_release
    ota_asset 4.4.0-beta-20260927-bbbbbbbb false > "$TMP/latest.json"
    echo '[]' > "$TMP/list.json"
    channel=stable
    get_release_info > /dev/null
    check "latest release offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.4.0-beta-20260927-bbbbbbbb
    ota_asset 4.4.0-beta-20260926-aaaaaaaa false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "installed build: up to date" test "$?" -eq 1
    end

    begin ota_never_offers_an_older_build
    # A device on beta 6 (10 Oct) must not be offered the stable of 9 Oct.
    installUI() { echo "4.4.0-beta-20261010-f7c522a5"; }
    channel=stable
    ota_asset 4.4.0-beta-20261009-8c2d8c75 false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "older stable: not offered" test "$?" -eq 1
    ota_asset 4.4.0-beta-20261010-0badc0de false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "same-day build: offered" test "$?" -eq 0
    ota_asset 4.4.0-beta-20261011-1234abcd false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "newer build: offered" test "$?" -eq 0
    check "build date parsed" test "$(build_date 4.4.0-beta-20261010-f7c522a5)" = 20261010
    check "date only: parsed" test "$(build_date 4.4.0-beta-20261010)" = 20261010
    check "no date: empty" test -z "$(build_date 4.4.0)"
    # A build made by hand with a word after the commit keeps its date.
    check "suffix: date parsed" test "$(build_date 4.4.0-beta-20261023-a6a42951-readline)" = 20261023
    installUI() { echo "4.4.0-beta-20261023-a6a42951-readline"; }
    ota_asset 4.4.0-beta-20261007-9e96c27c false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "suffix: older stable not offered" test "$?" -eq 1
    ota_asset 4.4.0-beta-20261025-0123abcd false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "suffix: newer build offered" test "$?" -eq 0
    installUI() { echo "4.4.0-beta-20260926-aaaaaaaa"; }
    end

    begin ota_4_5_0_betas_after_the_4_4_0_stable
    # The betas after the 4.4.0 stable are 4.5.0-b1, -b2...: a higher version
    # number, so the beta channel offers them over the stable, while the
    # stable is not offered back to a device on a 4.5.0 beta.
    check "4.5.0 is newer than 4.4.0" test "$(get_version 4.5.0)" -gt "$(get_version 4.4.0)"
    channel=beta
    installUI() { echo "4.4.0-20261025-e894906a"; }
    printf '[%s,%s]' "$(ota_asset 4.4.0-20261025-e894906a false)" "$(ota_asset 4.5.0-b1-20261027-1a2b3c4d true)" > "$TMP/list.json"
    get_release_info > /dev/null
    check "beta channel: 4.5.0-b1 offered over the 4.4.0 stable" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.0-b1-20261027-1a2b3c4d
    channel=stable
    installUI() { echo "4.5.0-b1-20261027-1a2b3c4d"; }
    ota_asset 4.4.0-20261025-e894906a false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "stable channel on 4.5.0-b1: 4.4.0 not offered" test "$?" -eq 1
    ota_asset 4.5.0-20261101-5e6f7a8b false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "stable channel on 4.5.0-b1: the 4.5.0 stable offered" test "$?" -eq 0
    check "4.5.0-b1: date parsed" test "$(build_date 4.5.0-b1-20261027-1a2b3c4d)" = 20261027
    # b2 after b1: the same number, a newer build date.
    channel=beta
    printf '[%s]' "$(ota_asset 4.5.0-b2-20261108-9a8b7c6d true)" > "$TMP/list.json"
    get_release_info > /dev/null
    check "beta channel on 4.5.0-b1: 4.5.0-b2 offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.0-b2-20261108-9a8b7c6d
    installUI() { echo "4.5.0-b2-20261108-9a8b7c6d"; }
    printf '[%s]' "$(ota_asset 4.5.0-b1-20261027-1a2b3c4d true)" > "$TMP/list.json"
    get_release_info > /dev/null
    check "beta channel on 4.5.0-b2: b1 not offered back" test "$?" -eq 1
    installUI() { echo "4.4.0-beta-20260926-aaaaaaaa"; }
    end

    begin ota_4_5_1_beta_after_the_4_5_0_stable
    # 4.5.1-b1 for the testers, then the beta channel: offered over the 4.5.0
    # stable there, not on the stable channel, and the 4.5.1 stable offered
    # to it later. Also as a build made by hand (no commit hash).
    check "the Makefile's version is 4.5.1-b6" test "$(sed -n 's/^VERSION_BASE=//p' "$ROOT/Makefile")" = 4.5.1-b6
    check "4.5.1 is newer than 4.5.0" test "$(get_version 4.5.1)" -gt "$(get_version 4.5.0)"
    installUI() { echo "4.5.0-20261106-3d9f2add"; }
    channel=beta
    printf '[%s,%s]' "$(ota_asset 4.5.0-20261106-3d9f2add false)" "$(ota_asset 4.5.1-b1-20261116-1a2b3c4d true)" > "$TMP/list.json"
    get_release_info > /dev/null
    check "beta channel: 4.5.1-b1 offered over the 4.5.0 stable" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.1-b1-20261116-1a2b3c4d
    channel=stable
    ota_asset 4.5.0-20261106-3d9f2add false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "stable channel on 4.5.0: nothing new (the beta is not offered)" test "$?" -eq 1
    installUI() { echo "4.5.1-b1-20261116"; }
    check "4.5.1-b1 made by hand: date parsed" test "$(build_date 4.5.1-b1-20261116)" = 20261116
    ota_asset 4.5.0-20261106-3d9f2add false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "stable channel on 4.5.1-b1: 4.5.0 not offered back" test "$?" -eq 1
    ota_asset 4.5.1-20261201-5e6f7a8b false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "stable channel on 4.5.1-b1: the 4.5.1 stable offered" test "$?" -eq 0
    channel=beta
    printf '[%s]' "$(ota_asset 4.5.1-b2-20261120-9a8b7c6d true)" > "$TMP/list.json"
    get_release_info > /dev/null
    check "beta channel on 4.5.1-b1: 4.5.1-b2 offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.1-b2-20261120-9a8b7c6d
    installUI() { echo "4.5.1-b2-20261119"; }
    printf '[%s]' "$(ota_asset 4.5.1-b1-20261116-1a2b3c4d true)" > "$TMP/list.json"
    get_release_info > /dev/null
    check "beta channel on 4.5.1-b2: b1 not offered back" test "$?" -eq 1
    # 4.5.1-b3 for the testers (their own repository), over the b2
    installUI() { echo "4.5.1-b2-20261120-ec94ce86"; }
    channel=testers
    printf '[%s]' "$(ota_asset 4.5.1-b3-20261121-8d1d5212 true)" > "$TMP/testers.json"
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b2: 4.5.1-b3 offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.1-b3-20261121-8d1d5212
    printf '[%s]' "$(ota_asset 4.5.1-b2-20261120-ec94ce86 true)" > "$TMP/testers.json"
    installUI() { echo "4.5.1-b3-20261121-8d1d5212"; }
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b3: b2 not offered back" test "$?" -eq 1
    # 4.5.1-b4 for the testers, over the b3 (the "alpha" the new testers got)
    installUI() { echo "4.5.1-b3-20261121-95f2577c"; }
    printf '[%s]' "$(ota_asset 4.5.1-b4-20261122-1a2b3c4d true)" > "$TMP/testers.json"
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b3: 4.5.1-b4 offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.1-b4-20261122-1a2b3c4d
    printf '[%s]' "$(ota_asset 4.5.1-b3-20261121-95f2577c true)" > "$TMP/testers.json"
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b3: the same b3 is up to date" test "$?" -eq 1
    installUI() { echo "4.5.1-b4-20261122-1a2b3c4d"; }
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b4: b3 not offered back" test "$?" -eq 1
    # 4.5.1-b5 for the testers, over the b4
    installUI() { echo "4.5.1-b4-20261122-537d70d4"; }
    printf '[%s]' "$(ota_asset 4.5.1-b5-20261125-4d5e6f7a true)" > "$TMP/testers.json"
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b4: 4.5.1-b5 offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.1-b5-20261125-4d5e6f7a
    printf '[%s]' "$(ota_asset 4.5.1-b4-20261122-537d70d4 true)" > "$TMP/testers.json"
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b4: the same b4 is up to date" test "$?" -eq 1
    installUI() { echo "4.5.1-b5-20261125-4d5e6f7a"; }
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b5: b4 not offered back" test "$?" -eq 1
    # 4.5.1-b6 for the testers (the new look), over the b5 they have
    installUI() { echo "4.5.1-b5-20261125-b102b879"; }
    printf '[%s]' "$(ota_asset 4.5.1-b6-20261127-393be556 true)" > "$TMP/testers.json"
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b5: 4.5.1-b6 offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.1-b6-20261127-393be556
    printf '[%s]' "$(ota_asset 4.5.1-b5-20261125-b102b879 true)" > "$TMP/testers.json"
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b5: the same b5 is up to date" test "$?" -eq 1
    installUI() { echo "4.5.1-b6-20261127-393be556"; }
    get_release_info > /dev/null
    check "testers channel on 4.5.1-b6: b5 not offered back" test "$?" -eq 1
    channel=beta
    installUI() { echo "4.5.1-b2-20261119"; }
    ota_asset 4.5.1-20261201-5e6f7a8b false > "$TMP/latest.json"
    channel=stable
    get_release_info > /dev/null
    check "stable channel on 4.5.1-b2: the 4.5.1 stable offered" test "$?" -eq 0
    installUI() { echo "4.4.0-beta-20260926-aaaaaaaa"; }
    end

    begin ota_4_5_0_stable_over_the_4_4_0_stable
    # 4.5.0 follows the 4.4.0 stable directly (no public 4.5.0 beta): both
    # channels offer it to a 4.4.0 device, and it is not offered again.
    channel=stable
    installUI() { echo "4.4.0-20261027-6cbd9b07"; }
    ota_asset 4.5.0-20261106-1a2b3c4d false > "$TMP/latest.json"
    get_release_info > /dev/null
    check "stable channel on 4.4.0: the 4.5.0 stable offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.0-20261106-1a2b3c4d
    channel=beta
    printf '[%s,%s]' "$(ota_asset 4.5.0-20261106-1a2b3c4d false)" "$(ota_asset 4.4.0-20261027-6cbd9b07 false)" > "$TMP/list.json"
    get_release_info > /dev/null
    check "beta channel on 4.4.0: the 4.5.0 stable offered" test "$?" -eq 0 -a "$Release_FullVersion" = 4.5.0-20261106-1a2b3c4d
    installUI() { echo "4.5.0-20261106-1a2b3c4d"; }
    channel=stable
    get_release_info > /dev/null
    check "stable channel on 4.5.0: up to date" test "$?" -eq 1
    installUI() { echo "4.4.0-beta-20260926-aaaaaaaa"; }
    end

    begin ota_finds_redonion_and_onionplus_packages
    # The rename: releases carry RedOnion-v, OnionPlus-v or both.
    ota_release() { # version, then package names
        _v=$1
        shift
        _assets=
        for _n; do
            _assets="$_assets${_assets:+,}{\"name\":\"$_n\",\"browser_download_url\":\"https://x/$_n\",\"size\":1048576,\"created_at\":\"2026-10-20T00:00:00Z\"}"
        done
        printf '{"tag_name":"%s","prerelease":false,"draft":false,"published_at":"2026-10-20T12:00:00Z","body":"","assets":[%s]}' "$_v" "$_assets"
    }
    channel=stable
    v=4.4.0-beta-20261020-abcdef12
    ota_release $v "OnionPlus-v$v.zip" > "$TMP/latest.json"
    get_release_info > /dev/null
    check "OnionPlus only: offered" test "$?" -eq 0 -a "$Release_FullVersion" = "$v"
    ota_release $v "RedOnion-v$v.zip" > "$TMP/latest.json"
    get_release_info > /dev/null
    check "RedOnion only: offered" test "$?" -eq 0 -a "$Release_FullVersion" = "$v"
    ota_release $v "OnionPlus-v$v.zip" "RedOnion-v$v.zip" > "$TMP/latest.json"
    get_release_info > /dev/null
    check "both: RedOnion downloaded ($Release_url)" test "$Release_url" = "https://x/RedOnion-v$v.zip"
    check "both: same version" test "$Release_FullVersion" = "$v"
    ota_release $v "RedOnion-v$v.zip.sha256" "OnionPlus-v$v.zip" > "$TMP/latest.json"
    get_release_info > /dev/null
    check "only a .zip is a package" test "$Release_url" = "https://x/OnionPlus-v$v.zip"
    ota_release $v "Onion-v$v.zip" > "$TMP/latest.json"
    get_release_info > /dev/null 2>&1
    check "another project's package: error" test "$?" -eq 2
    # What updaters before the rename select (contains "OnionPlus-v"): a
    # release with both packages still gives them one.
    ota_release $v "OnionPlus-v$v.zip" "RedOnion-v$v.zip" > "$TMP/latest.json"
    check "old updaters still find OnionPlus-v" test "$(jq '[.assets[]? | select(.name | contains("OnionPlus-v"))] | length' "$TMP/latest.json")" -eq 1
    end
    unset -f curl installUI ota_asset ota_release
else
    skip "ota_update.sh tests (jq not installed)"
fi

# ---- ota_update.sh: the new UI's download (progressUI) ----

begin ota_native_download_on_progressui
OTA="$ROOT/static/build/.tmp_update/script/ota_update.sh"
mkdir -p "$TMP/sys/bin"
# progressUI: records what it is told; RC_UI is its answer (2: stopped)
printf '#!/bin/sh\ncat > "%s/ui_in"\nexit ${RC_UI:-0}\n' "$TMP" > "$TMP/sys/bin/progressUI"
chmod +x "$TMP/sys/bin/progressUI"
ota_dl() { # rc of progressUI -> the download's return code
    rm -f "$TMP/file" "$TMP/ui_in"
    (
        eval "$(extract_fn "$OTA" onion_percent)"
        eval "$(sed -n '/^if \[ -n "\$OTA_NATIVE" \]/,/^fi$/p' "$OTA" | sed -n '/^\tdownload_file() {/,/^\t}/p')"
        sysdir=$TMP/sys
        ui_unbusy() { :; }
        stop_download() { kill "$_dl_pid" 2> /dev/null; }
        curl() { i=0; while [ $i -lt 3 ]; do printf 'xxxx' >> "$TMP/file"; i=$((i + 1)); sleep 1; done; }
        Release_Version=4.5.1; channel=testers
        RC_UI=$1 download_file https://x "$TMP/file" 12
        echo $? > "$TMP/rc"
    )
    cat "$TMP/rc"
}
check "done: curl's result" test "$(ota_dl 0)" -eq 0
check "the title, the steps and closing by itself" sh -c "grep -q '^@@title RedOnion 4.5.1' '$TMP/ui_in' && grep -q '^@@total 100' '$TMP/ui_in' && grep -q '^@@autoclose' '$TMP/ui_in'"
check "percent and MB as the item, three panels" sh -c "grep -q '^@@item [0-9]* [0-9.]* of 0.0 MB' '$TMP/ui_in' && grep -q '^@@panel 3 TIME LEFT|' '$TMP/ui_in'"
check "stopped on the screen: 130 and no partial file" test "$(ota_dl 2)" -eq 130 -a ! -e "$TMP/file"
end

# ---- ota_update.sh: apply_update keeps the installed boot scripts ----

begin ota_apply_keeps_the_boot_scripts
(
    eval "$(extract_fn "$OTA" apply_update)"
    sysdir=$TMP
    Release_Version=4.5.1-b3
    ota_menu() { menu_choice=Yes; }
    clear() { :; }
    umount() { :; }
    press_a() { :; }
    sync() { :; }
    sleep() { :; }
    reboot() { echo reboot > "$TMP/rebooted"; }
    # 7z: what it was asked, one argument per line; it fails, as a cut
    # extraction would
    mkdir -p "$TMP/stub"
    printf '#!/bin/sh\nprintf "%%s\\n" "$@" > "%s/7z_args"\nexit 2\n' "$TMP" > "$TMP/stub/7z"
    chmod +x "$TMP/stub/7z"
    PATH="$TMP/stub:$PATH"
    apply_update > /dev/null 2>&1
) 2> /dev/null
echo $? > "$TMP/rc"
check "7z was run" test -f "$TMP/7z_args"
check "the boot stub updater is not extracted" grep -q '^-x!\.tmp_update/updater$' "$TMP/7z_args"
check "the boot stub runtime.sh is not extracted" grep -q '^-x!\.tmp_update/runtime\.sh$' "$TMP/7z_args"
check "nothing else is left out (installer, trigger)" test "$(grep -c '^-x' "$TMP/7z_args")" -eq 2
check "into the card root" grep -q '^-o/mnt/SDCARD$' "$TMP/7z_args"
check "files on the card are replaced (-aoa, not -aos)" grep -q '^-aoa$' "$TMP/7z_args"
check "a failed extraction: exit 6, no reboot" test "$(cat "$TMP/rc")" = 6 -a ! -e "$TMP/rebooted"
end

# ---- ota_update.sh: get_version ignores the files around it ----

eval "$(grep '^get_version()' "$OTA")"
begin ota_get_version_ignores_files
touch "$TMP/a"
check "letters dropped with a file named a in the directory" test "$(cd "$TMP" && get_version v4.4.0)" = 4004000000
end

# ---- ota_update.sh: the bootstrap runs only a script ----

begin ota_bootstrap_runs_only_a_script
eval "$(extract_fn "$OTA" run_bootstrap)"
GITHUB_REPOSITORY=Amiga500/Onion
curl() { for _u; do :; done; echo "$*" > "$TMP/curl_args"; printf '%s' "$BOOTSTRAP_BODY"; }
BOOTSTRAP_BODY='#!/bin/sh
echo ran > "$BOOTSTRAP_MARK"'
BOOTSTRAP_MARK="$TMP/ran"
export BOOTSTRAP_MARK
run_bootstrap
check "script: run" test -f "$TMP/ran"
check "follows redirects (-L)" grep -q -- ' -L ' "$TMP/curl_args"
check "fails on HTTP errors (-f)" grep -q -- ' -f ' "$TMP/curl_args"
check "read from the OnionPlus branch of the repository" grep -q 'raw.githubusercontent.com/Amiga500/Onion/OnionPlus/' "$TMP/curl_args"
rm -f "$TMP/ran"
BOOTSTRAP_BODY='404: Not Found'
out=$(run_bootstrap 2>&1)
check "error page: not run" test -z "$out"
BOOTSTRAP_BODY=
out=$(run_bootstrap 2>&1)
check "nothing: not run" test -z "$out"
check "release lists read with -L, compressed, with a time limit" test "$(grep -c 'curl -k -s -L --compressed --connect-timeout 15 --max-time 60 https://api.github.com' "$OTA")" -eq 2
unset -f curl run_bootstrap
unset BOOTSTRAP_BODY BOOTSTRAP_MARK
end

# ---- ota_update.sh: Wi-Fi off must not leave the updater hanging ----

eval "$(extract_fn "$OTA" enable_wifi)"

begin ota_wifi_off_does_not_hang
# No network ever comes up: udhcpc keeps retrying, as the real one does.
ui_busy() { echo "$1" >> "$TMP/busy"; }
wait_for_link() { return 0; }
ip() { :; }
sleep() { :; }
insmod() { :; }
ifconfig() { :; }
pkill() { :; }
clear() { :; }
udhcpc() {
    echo "$*" > "$TMP/udhcpc_args"
    while [ -d "$TMP" ] && [ ! -f "$TMP/stop" ]; do command sleep 0.1; done
}
(enable_wifi > /dev/null 2>&1; touch "$TMP/returned") &
n=0
while [ ! -f "$TMP/returned" ] && [ $n -lt 50 ]; do
    command sleep 0.1
    n=$((n + 1))
done
touch "$TMP/stop"
check "enable_wifi returns without a network" test -f "$TMP/returned"
check "udhcpc started on wlan0" grep -q -- '-i wlan0' "$TMP/udhcpc_args"
check "wpa_supplicant called by its full path" \
    grep -q '/mnt/SDCARD/miyoo/app/wpa_supplicant -B' "$OTA"
check "connection check has a timeout" grep -q 'curl -k -sS -I -L -f --connect-timeout [0-9]* --max-time [0-9]' "$OTA"
check "connection check asks for the headers only (#311)" grep -q 'curl -k -sS -I -L -f' "$OTA"
wait
# Each step on the screen: "Checking the Wi-Fi and the connection..." stayed
# up to a minute and a half while Wi-Fi was turned on (beta 6)
check "the steps on the screen: on, connecting, address" test "$(tr '\n' '|' < "$TMP/busy")" = "Wi-Fi is off: turning it on for the update...|Connecting to Wi-Fi (this can take up to a minute)...|Connected to Wi-Fi: getting an address...|"
# Not joined (far from the router, a wrong password): no 20 s more for an
# address that cannot come
rm -f "$TMP/busy" "$TMP/udhcpc_args"
wait_for_link() { return 1; }
wait_for_ip() { echo waited > "$TMP/waited_ip"; }
enable_wifi > /dev/null 2>&1
check "not joined: no udhcpc" test ! -e "$TMP/udhcpc_args"
check "not joined: no wait for an address" test ! -e "$TMP/waited_ip"
check "not joined: the last step shown is connecting" test "$(tail -n 1 "$TMP/busy")" = "Connecting to Wi-Fi (this can take up to a minute)..."
check "the connection check says it too" grep -q '^	ui_busy "Checking the connection to GitHub..."$' "$OTA"
unset -f ip sleep insmod ifconfig pkill clear udhcpc ui_busy wait_for_link wait_for_ip
end

# ---- ota_update.sh: Wi-Fi turned on by the updater goes off again ----

eval "$(extract_fn "$OTA" restore_wifi)"
eval "$(extract_fn "$OTA" wait_for_ip)"

begin ota_restores_wifi_it_turned_on
pkill() { echo "$*" >> "$TMP/pkill"; }
wifi_setting_on() { return 1; }
wifi_started_here=1
restore_wifi
check "off in Settings: wpa_supplicant stopped" grep -q 'wpa_supplicant' "$TMP/pkill"
check "off in Settings: udhcpc stopped" grep -q 'udhcpc' "$TMP/pkill"
rm -f "$TMP/pkill"
restore_wifi
check "runs once" test ! -e "$TMP/pkill"
wifi_started_here=1
wifi_setting_on() { return 0; }
restore_wifi
check "turned on in Settings meanwhile: left on" test ! -e "$TMP/pkill"
wifi_started_here=0
wifi_setting_on() { return 1; }
restore_wifi
check "already on before the updater: left on" test ! -e "$TMP/pkill"
check "restored on every exit" grep -q "^	trap 'stop_download; restore_wifi; restore_keys' EXIT" "$OTA"
unset -f pkill wifi_setting_on
end

begin ota_waits_for_wifi_on_in_settings
# Wi-Fi up but not connected yet: no second wpa_supplicant, and the
# system's udhcpc is not killed.
# Connected at the third look (ip runs in a subshell: count in a file).
ip() { echo x >> "$TMP/ip_calls"; [ "$(wc -l < "$TMP/ip_calls")" -ge 3 ] && echo "1.0.0.0 via 10.0.0.1 dev wlan0 src 10.0.0.2"; }
sleep() { :; }
clear() { :; }
pkill() { echo "$*" >> "$TMP/pkill"; }
udhcpc() { touch "$TMP/udhcpc"; }
wifi_radio_running() { return 0; }
wifi_started_here=0
enable_wifi > /dev/null 2>&1
check "udhcpc not restarted" test ! -e "$TMP/udhcpc"
check "nothing killed" test ! -e "$TMP/pkill"
check "not marked as turned on here" test "$wifi_started_here" = 0
check "address found" test -n "$IP"
# Wi-Fi off, whatever system.json says: the updater turns it on.
rm -f "$TMP/ip_calls" "$TMP/udhcpc"
wifi_started_here=0
ip() { :; }
wifi_radio_running() { return 1; }
wifi_setting_on() { return 0; }
wait_for_link() { return 0; }
(enable_wifi > /dev/null 2>&1; echo "$wifi_started_here" > "$TMP/started")
check "off but on in system.json: turned on" test -e "$TMP/udhcpc"
check "off but on in system.json: marked as turned on here" test "$(cat "$TMP/started")" = 1
check "decided by the process, not the setting" sh -c "sed -n '/^enable_wifi() {/,/^}/p' \"$OTA\" | grep -q 'wifi_radio_running' && ! sed -n '/^enable_wifi() {/,/^}/p' \"$OTA\" | grep -q 'wifi_setting_on'"
unset -f ip sleep clear pkill udhcpc wifi_setting_on wifi_radio_running wait_for_link
end

begin ota_waits_for_boot_network
eval "$(extract_fn "$OTA" wait_boot_network)"
eval "$(extract_fn "$OTA" network_ready)"
sleep() { echo x >> "$TMP/slept"; }
clear() { :; }
ip() { :; }
wifi_setting_on() { return 1; }
# update_networking.sh still running for three more looks.
pgrep() { echo x >> "$TMP/pgrep"; [ "$(wc -l < "$TMP/pgrep")" -le 3 ]; }
wait_boot_network > /dev/null
check "waits while the start-up runs" test "$(wc -l < "$TMP/slept")" -eq 2
check "then goes on" test "$(wc -l < "$TMP/pgrep")" -eq 4
rm -f "$TMP/slept"
pgrep() { return 1; }
wait_boot_network > /dev/null
check "nothing running: no wait" test ! -e "$TMP/slept"
pgrep() { return 0; }
wait_boot_network > /dev/null
check "gives up after 45 s" test "$(wc -l < "$TMP/slept")" -eq 45
# Connected with Wi-Fi on: no wait for the time sync still running.
rm -f "$TMP/slept"
ip() { echo "1.0.0.0 via 10.0.0.1 dev wlan0 src 10.0.0.2"; }
wifi_setting_on() { return 0; }
wait_boot_network > /dev/null
check "connected, Wi-Fi on: no wait" test ! -e "$TMP/slept"
# Connected with Wi-Fi off: the temporary Wi-Fi of the time sync, waited for.
wifi_setting_on() { return 1; }
wait_boot_network > /dev/null
check "temporary Wi-Fi: waited for" test "$(wc -l < "$TMP/slept")" -eq 45
check "before Wi-Fi is touched" sh -c "sed -n '/^main() {/,/^}/p' \"$OTA\" | grep -A1 '^	wait_boot_network' | grep -q '^	enable_wifi'"
unset -f sleep clear pgrep wait_boot_network network_ready ip wifi_setting_on
end

begin ota_waits_for_wlan0_after_power_on
eval "$(extract_fn "$OTA" wait_for_wlan0)"
# The chip shows up at the fourth look, after the power-on.
wlan0_present() { echo x >> "$TMP/looks"; [ "$(wc -l < "$TMP/looks")" -ge 4 ]; }
sleep() { echo "$1" >> "$TMP/slept"; }
wait_for_wlan0 10
check "waits for wlan0" test $? -eq 0
check "in half-second steps until it appears" test "$(cat "$TMP/slept" | tr '\n' ' ')" = "0.5 0.5 0.5 "
rm -f "$TMP/slept"
wlan0_present() { return 1; }
wait_for_wlan0 10
check "never appears: gives up" test $? -eq 1
check "after 10 s" test "$(wc -l < "$TMP/slept")" -eq 20
check "bring-up waits for wlan0, not a fixed 2 s" sh -c "sed -n '/^enable_wifi() {/,/^}/p' \"$OTA\" | grep -A1 'axp_test wifion' | grep -q 'wait_for_wlan0'"
unset -f wlan0_present sleep wait_for_wlan0
end

begin ota_dhcp_after_link_up
eval "$(extract_fn "$OTA" wait_for_link)"
wlan0_linked() { echo x >> "$TMP/looks"; [ "$(wc -l < "$TMP/looks")" -ge 5 ]; }
sleep() { echo "$1" >> "$TMP/slept"; }
wait_for_link 20
check "waits for the network to be joined" test $? -eq 0 -a "$(wc -l < "$TMP/slept")" -eq 4
rm -f "$TMP/slept"
wlan0_linked() { return 1; }
wait_for_link 20
check "never joined: goes on after 20 s" test $? -eq 1 -a "$(wc -l < "$TMP/slept")" -eq 40
check "udhcpc started after the link is up" sh -c "sed -n '/^enable_wifi() {/,/^}/p' \"$OTA\" | grep -A6 'start_wpa_supplicant' | grep -q 'if wait_for_link 50; then'"
unset -f wlan0_linked sleep wait_for_link
end

begin ota_wifi_running_needs_wlan0
eval "$(extract_fn "$OTA" wifi_radio_running)"
pgrep() { echo "$*" > "$TMP/pgrep_args"; return 0; }
wlan0_present() { return 1; }
wifi_radio_running
check "wpa_supplicant listed but no wlan0: Wi-Fi off" test $? -eq 1
wlan0_present() { return 0; }
wifi_radio_running
check "wpa_supplicant and wlan0: Wi-Fi on" test $? -eq 0
check "matched by command line (a killed one has none)" grep -q -- '-f' "$TMP/pgrep_args"
pgrep() { return 1; }
wifi_radio_running
check "no wpa_supplicant: Wi-Fi off" test $? -eq 1
unset -f pgrep wlan0_present wifi_radio_running
end

begin ota_dhcp_started_once_joined
# Wi-Fi off: wpa_supplicant once, then the wait for the link, then udhcpc.
ip() { :; }
sleep() { :; }
clear() { :; }
insmod() { :; }
ifconfig() { :; }
iw() { :; }
pkill() { echo "pkill $*" >> "$TMP/steps"; }
udhcpc() { echo udhcpc >> "$TMP/steps"; }
wifi_radio_running() { return 1; }
wait_for_wlan0() { return 0; }
start_wpa_supplicant() { echo wpa >> "$TMP/steps"; }
wait_for_link() { echo "link $1" >> "$TMP/steps"; }
wait_for_ip() { echo "ip $1" >> "$TMP/steps"; }
enable_wifi > /dev/null 2>&1
# udhcpc runs in the background: its line may come after "ip 20". Poll
# for it (a bare `wait` would also wait for any job left by other tests).
n=0
while ! has_line "$TMP/steps" 'udhcpc' && [ $n -lt 50 ]; do
    command sleep 0.1
    n=$((n + 1))
done
check "wpa_supplicant, up to 50 s for the link, then udhcpc" test "$(grep -v '^udhcpc$' "$TMP/steps" | tr '\n' ' ')" = "pkill -9 wpa_supplicant wpa link 50 pkill -9 udhcpc ip 20 "
check "udhcpc started" has_line "$TMP/steps" 'udhcpc'
unset -f ip sleep clear insmod ifconfig iw pkill udhcpc wifi_radio_running wait_for_wlan0 start_wpa_supplicant wait_for_link wait_for_ip
end

begin ota_channel_cancel_keeps_channel
mkdir -p "$TMP/script" "$TMP/config"
sysdir=$TMP
echo beta > "$TMP/config/ota_channel"
clear() { :; }
for f in channel_choice ota_menu drop_keys quiet_keys; do eval "$(extract_fn "$OTA" $f)"; done
printf '#!/bin/sh\ncat > /dev/null\nexit 1\n' > "$TMP/script/shellect.sh"
chmod +x "$TMP/script/shellect.sh"
(channel_choice; touch "$TMP/went_on") > /dev/null 2>&1
check "B leaves the updater" test ! -e "$TMP/went_on"
check "saved channel kept" test "$(cat "$TMP/config/ota_channel")" = beta
printf '#!/bin/sh\ncat > /dev/null\necho stable\n' > "$TMP/script/shellect.sh"
(channel_choice; touch "$TMP/went_on") > /dev/null 2>&1
check "choice saved" test "$(cat "$TMP/config/ota_channel")" = stable
check "choice goes on" test -e "$TMP/went_on"
ask_leave() { return 0; }
printf '#!/bin/sh\ncat > /dev/null\nexit 2\n' > "$TMP/script/shellect.sh"
rm -f "$TMP/went_on"
(channel_choice; touch "$TMP/went_on") > /dev/null 2>&1
check "MENU, then START: leaves" test ! -e "$TMP/went_on"
ask_leave() { printf '#!/bin/sh\ncat > /dev/null\necho beta\n' > "$TMP/script/shellect.sh"; return 1; }
printf '#!/bin/sh\ncat > /dev/null\nexit 2\n' > "$TMP/script/shellect.sh"
(channel_choice; touch "$TMP/went_on") > /dev/null 2>&1
check "MENU, then A: the menu again" test "$(cat "$TMP/config/ota_channel")" = beta
check "MENU, then A: goes on" test -e "$TMP/went_on"
check "Mini without Wi-Fi is told so" grep -q 'This device has no Wi-Fi' "$OTA"
unset -f clear channel_choice ota_menu drop_keys quiet_keys ask_leave
end

begin ota_channel_menu_starts_on_saved_channel
# The Update channel menu always started on stable, so a tester who pressed
# A at once got stable. It starts on the saved channel (shellect.sh -s).
mkdir -p "$TMP/script" "$TMP/config"
sysdir=$TMP
clear() { :; }
for f in channel_choice ota_menu drop_keys quiet_keys; do eval "$(extract_fn "$OTA" $f)"; done
printf '#!/bin/sh\ncat > /dev/null\nwhile [ $# -gt 0 ]; do [ "$1" = -s ] && echo "$2" > "%s/start"; shift; done\necho stable\n' "$TMP" > "$TMP/script/shellect.sh"
chmod +x "$TMP/script/shellect.sh"
: > "$TMP/config/.otaTesters"
for c in testers:3 beta:2 stable:1 :1 junk:1; do
    channel=${c%%:*}
    (channel_choice) > /dev/null 2>&1
    check "saved \"${c%%:*}\": starts on item ${c##*:}" test "$(cat "$TMP/start")" = "${c##*:}"
done
rm -f "$TMP/config/.otaTesters"
channel=testers
(channel_choice) > /dev/null 2>&1
check "saved testers without .otaTesters: starts on stable" test "$(cat "$TMP/start")" = 1
rm -f "$TMP/start" "$TMP/config/ota_channel"
unset -f clear channel_choice ota_menu drop_keys quiet_keys
# shellect.sh -s itself, in a pseudo terminal: A takes the item it starts on.
if command -v python3 > /dev/null 2>&1; then
    SHL="$ROOT/static/build/.tmp_update/script/shellect.sh"
    pick() { # start, keys...
        _s=$1
        shift
        python3 - "$SHL" "$_s" "$TMP/picked" "$@" << 'EOF'
import os, pty, select, struct, sys, termios, time, fcntl
shl, start, out, keys = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
send = {"A": b" ", "UP": b"\x1b[A", "DOWN": b"\x1b[B"}
cmd = "printf 'stable\\nbeta\\ntesters' | sh '%s' -m -s '%s' -t T -b B > '%s'" % (shl, start, out)
pid, fd = pty.fork()
if pid == 0:
    os.execvp("sh", ["sh", "-c", cmd])
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 29, 53, 0, 0))
def pump(t):
    end = time.time() + t
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try:
                os.read(fd, 4096)
            except OSError:
                return
pump(1.0)
for k in keys:
    os.write(fd, send[k])
    pump(0.6)
end = time.time() + 5
while time.time() < end:
    done, _ = os.waitpid(pid, os.WNOHANG)
    if done:
        break
    pump(0.1)
else:
    os.kill(pid, 9)
EOF
        cat "$TMP/picked" 2> /dev/null
    }
    check "-s 3, A: testers" test "$(pick 3 A)" = testers
    check "-s 2, A: beta" test "$(pick 2 A)" = beta
    check "-s 1, A: stable" test "$(pick 1 A)" = stable
    check "-s 3, Up, A: beta" test "$(pick 3 UP A)" = beta
    check "-s 2, Down, A: testers" test "$(pick 2 DOWN A)" = testers
    check "-s 2, Up, Up, A: stable" test "$(pick 2 UP UP A)" = stable
    check "-s 9 (past the end): stable" test "$(pick 9 A)" = stable
    check "-s x: stable" test "$(pick x A)" = stable
    unset -f pick
    rm -f "$TMP/picked"
else
    skip "shellect -s in a pseudo terminal (no python3)"
fi
end

if command -v jq > /dev/null; then
    begin ota_github_error_is_not_up_to_date
    # Rate limit (60 requests an hour per address), no answer, or no
    # package: an error, not "Version is up to date".
    eval "$(extract_fn "$OTA" get_release_info)"
    eval "$(extract_fn "$OTA" release_info_error)"
    installUI() { echo "4.4.0-beta-20261010-5a3b4ac9"; }
    for ch in stable beta; do
        channel=$ch
        curl() { echo '{"message":"API rate limit exceeded for 1.2.3.4.","documentation_url":"x"}'; }
        out=$(get_release_info 2>&1)
        check "$ch, rate limit: error" test $? -eq 2
        check "$ch, rate limit: reason shown" sh -c 'echo "$1" | grep -q "API rate limit exceeded"' _ "$out"
        curl() { :; }
        get_release_info > /dev/null 2>&1
        check "$ch, no answer: error" test $? -eq 2
    done
    channel=stable
    curl() { echo '{"message": "Not Found","documentation_url":"x"}'; }
    get_release_info > /dev/null 2>&1
    check "no release: nothing to install" test $? -eq 1
    curl() { echo '{"tag_name":"x","assets":[{"name":"other.zip","size":1}]}'; }
    get_release_info > /dev/null 2>&1
    check "no OnionPlus package: error" test $? -eq 2
    # The boot check keeps the flag when GitHub doesn't answer.
    eval "$(extract_fn "$OTA" main)"
    ip() { echo "1.0.0.0 via 10.0.0.1 dev wlan0 src 10.0.0.2"; }
    sysdir=$TMP
    touch "$TMP/.updateAvailable"
    get_release_info() { return 2; }
    (cmd=check; main) > /dev/null 2>&1
    check "boot check, GitHub error: flag kept" test -e "$TMP/.updateAvailable"
    unset -f curl installUI ip get_release_info main release_info_error
    end
else
    skip "OTA GitHub error tests (jq not installed)"
fi

# ---- runtime.sh: network check skipped while update_networking.sh runs ----

# The flags in a folder of this run's, not the real /tmp: two runs of the
# suite at once took each other's network_changed.
NETTMP=$(mktemp -d)
for f in check_networking run_network_check queue_network_check; do
    eval "$(extract_fn "$RUNTIME" $f | sed "s#/tmp/network_#$NETTMP/network_#g")"
done

net_setup() {
    sysdir=$TMP
    mkdir -p "$sysdir/script/network"
    printf '#!/bin/sh\necho "$1" >> "%s/net_runs"\n' "$TMP" > "$sysdir/script/network/update_networking.sh"
    chmod +x "$sysdir/script/network/update_networking.sh"
    rm -rf $NETTMP/network_changed $NETTMP/network_changed.taken $NETTMP/network_check_queued
}
unset -f sleep # the waiter really waits (an earlier section stubs it)
pgrep() { [ -f "$TMP/running" ]; } # pgrep -f update_networking.sh
log() { :; }
perf_begin() { :; }
perf_end() { :; }
net_runs() { [ -f "$TMP/net_runs" ] && wc -l < "$TMP/net_runs" | tr -d ' ' || echo 0; }

begin network_check_runs_when_idle
net_setup
: > $NETTMP/network_changed
check_networking > /dev/null
check "check ran once" test "$(net_runs)" = 1
check "change consumed" test ! -e $NETTMP/network_changed
end

begin network_check_queued_while_running
net_setup
: > $NETTMP/network_changed
: > "$TMP/running"
check_networking > /dev/null
check_networking > /dev/null # a second state change: still one waiter
check "not run while the other check runs" test "$(net_runs)" = 0
check "one waiter queued" test -d $NETTMP/network_check_queued
rm -f "$TMP/running" # the running check ends
i=0
while [ -d $NETTMP/network_check_queued ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
sleep 0.3
check "run once after it ended" test "$(net_runs)" = 1
check "change consumed" test ! -e $NETTMP/network_changed
end

begin network_check_taken_once
net_setup
run_network_check
check "nothing pending: no run" test "$(net_runs)" = 0
: > $NETTMP/network_changed
run_network_check
run_network_check
check "a pending change runs once" test "$(net_runs)" = 1
end
unset -f pgrep log perf_begin perf_end net_runs net_setup
rm -rf "$NETTMP"

# ---- update_networking.sh: libpadspblocker pid lookup ----

eval "$(extract_fn "$NETWORK" lowest_pid_named)"
eval "$(extract_fn "$NETWORK" libpadspblocker)"

fake_proc() { # pid name
    mkdir -p "$proc_dir/$1"
    echo "$2" > "$proc_dir/$1/comm"
}

begin libpadspblocker_pid_lookup
proc_dir="$TMP/proc"
fake_proc 905 wpa_supplicant
fake_proc 1203 wpa_supplicant # sorts before 905 as text
fake_proc 812 wpa_supplicant
fake_proc 830 udhcpc
fake_proc 840 udhcpc.script # longer name: not the daemon
fake_proc 850 sh
mkdir -p "$proc_dir/860" # process gone: no comm
grep() { echo "$*" >> "$TMP/greps"; return 1; } # maps checked, no preload
killall() { echo "$*" >> "$TMP/killed"; }
libpadspblocker
check "lowest wpa_supplicant pid" test "$wpa_pid" = 812
check "udhcpc pid, exact name" test "$udhcpc_pid" = 830
check "maps of both checked" test "$(cat "$TMP/greps")" = "-q libpadsp.so /proc/812/maps
-q libpadsp.so /proc/830/maps"
check "nothing killed without the preload" test ! -e "$TMP/killed"
rm -rf "$proc_dir"
mkdir -p "$proc_dir"
fake_proc 812 wpa_supplicant
wpa_pid=x
libpadspblocker
check "no udhcpc: empty pid, nothing done" test -z "$udhcpc_pid" -a ! -e "$TMP/killed"
unset -f grep killall
unset proc_dir
end

begin no_pgrep_x_in_shipped_scripts
# The firmware's BusyBox 1.20.2 pgrep -x never matches anything.
check "no pgrep with -x under static/" test -z "$(command grep -rlE '^[^#]*pgrep +-[a-z]*x' "$ROOT/static")"
end

# ---- runtime.sh: temporaries of interrupted saves removed at boot ----

begin boot_removes_interrupted_save_temporaries
# The first command of load_settings, run on a card with leftovers
mkdir -p "$TMP/sd/.tmp_update/config"
touch "$TMP/sd/system.json.tmp.123.1" "$TMP/sd/.tmp_update/config/display.tmp.9.2" "$TMP/sd/system.json" "$TMP/sd/.tmp_update/config/.showRecents"
eval "$(sed -n '/^load_settings() {/,/^}/p' "$RUNTIME" | grep -m 1 '^    rm -f ' | sed "s#/mnt/SDCARD#$TMP/sd#g")"
check "load_settings removes system.json and config temporaries" test ! -e "$TMP/sd/system.json.tmp.123.1" -a ! -e "$TMP/sd/.tmp_update/config/display.tmp.9.2"
check "and nothing else" test -e "$TMP/sd/system.json" -a -e "$TMP/sd/.tmp_update/config/.showRecents"
check "it is load_settings' first command" sh -c "sed -n '/^load_settings() {/,/^}/p' \"$RUNTIME\" | grep -v '^[[:space:]]*#' | sed -n 2p | grep -q '^    rm -f /mnt/SDCARD/system.json.tmp'"
check "before batmon or keymon can save" sh -c "grep -n '^    init_system\|^    batmon &' \"$RUNTIME\" | head -1 | grep -q init_system"
end

# ---- runtime.sh: detect_device_model ----

MODEL_MM=283
MODEL_MMF=285
MODEL_MMP=354
eval "$(extract_fn "$RUNTIME" detect_device_model)"

stub_axp() { # exit code of `axp 0`
    mkdir -p "$TMP/bin"
    printf '#!/bin/sh\nexit %s\n' "$1" > "$TMP/bin/axp"
    chmod +x "$TMP/bin/axp"
}

probe() { # hall present (1/0), axp exit code
    stub_axp "$2"
    if [ "$1" -eq 1 ]; then
        hall_sensor="$TMP/hallvalue"
        echo 1 > "$hall_sensor"
    else
        hall_sensor="$TMP/missing/hallvalue"
    fi
    _saved_path=$PATH; PATH="$TMP/bin:$PATH"; detect_device_model; _rc=$?; PATH=$_saved_path; rc $_rc
}

begin detect_flip_with_axp
check "hall + axp -> 285" test "$(probe 1 0)" = 285
end

begin detect_flip_when_axp_probe_fails
check "hall, axp fails -> 285 (was 283)" test "$(probe 1 1)" = 285
end

begin detect_plus
check "no hall + axp -> 354" test "$(probe 0 0)" = 354
end

begin detect_mini
check "no hall, no axp -> 283" test "$(probe 0 1)" = 283
end

# ---- game_list_options.sh: network scripts on WiFi devices ----

GLO="$ROOT/static/build/.tmp_update/script/game_list_options.sh"
eval "$(extract_fn "$GLO" device_has_networking)"

begin glo_networking_on_wifi_devices
check "Mini Plus (354) has networking" test "$(device_has_networking 354)" = 1
check "Mini Flip (285) has networking" test "$(device_has_networking 285)" = 1
check "Mini (283) has none" test "$(device_has_networking 283)" = 0
check "unknown model has none" test "$(device_has_networking "")" = 0
end

# ---- romscripts: Scraper.sh hands control back to the game list ----

SCRAPER="$ROOT/static/build/App/romscripts/Scraper.sh"

begin scraper_returns_to_game_list
check "last command is exit 1 (0 makes GLO start the game)" \
    test "$(grep -v '^[[:space:]]*$' "$SCRAPER" | tail -n 1)" = "exit 1"
end

begin glo_maintenance_scripts_return_to_the_list
# Repair favorites, Sort favorites (A-Z, by system) and Clean recents ended
# with the tools command, status 0, which GLO takes as "start the game": the
# selected game launched, and since GLO had already taken it out of Recents,
# the GameSwitcher showed its picture under the previous game's title.
mkdir -p "$TMP/bin"
printf '#!/bin/sh\necho "$*" >> "%s/tools.log"\nexit 0\n' "$TMP" > "$TMP/bin/tools"
chmod +x "$TMP/bin/tools"
for script in favorites/favfix.sh:favfix favorites/favsort.sh:favsort \
    favorites/favsort2.sh:favsort2 "recents/clean_recents.sh:recents --clean_all"; do
    file="$ROOT/static/build/App/romscripts/${script%%:*}"
    args=${script#*:}
    rm -f "$TMP/tools.log"
    PATH="$TMP/bin:$PATH" sh "$file" > /dev/null 2>&1
    status=$?
    check "${script%%:*}: back to the list (status 1, was 0)" test "$status" -eq 1
    check "${script%%:*}: still runs tools $args" test "$(cat "$TMP/tools.log" 2> /dev/null)" = "$args"
done
# The import and rename scripts of the Ports and ScummVM packages too, and
# Set time of the Clock app (issue #334: B in the clock started the game).
for file in "$ROOT/static/packages/Emu/Ports Collection/Emu/PORTS/romscripts/Import games (Ports).sh" \
    "$ROOT/static/packages/App/Clock (Set emulated time)/App/romscripts/Set time.sh" \
    "$ROOT/static/packages/Emu/SCUMM (ScummVM)/Emu/SCUMMVM/romscripts/Import games (ScummVM).sh" \
    "$ROOT/static/packages/Emu/SCUMM (ScummVM)/Emu/SCUMMVM/romscripts/Change names (ScummVM).sh" \
    "$ROOT/static/packages/RApp/SCUMM (ScummVM standalone)/RApp/scummvm/scripts/romscripts/Import games (ScummVM).sh" \
    "$ROOT/static/packages/RApp/SCUMM (ScummVM standalone)/RApp/scummvm/scripts/romscripts/Change names (ScummVM).sh"; do
    check "${file##*/}: ends with exit 1" test "$(grep -v '^[[:space:]]*$' "$file" | tail -n 1)" = "exit 1"
done
end

begin glo_script_that_starts_the_game_keeps_recents
# GLO takes the selected game out of Recents when it opens. A script that
# returns 0 starts that game (Netplay): the entry goes back, as
# Reset game does, so the GameSwitcher title matches the game. A script that
# returns 1 leaves it out; one that wrote another command (Random game)
# handles its own entry.
glo_run() { # script body
    (
        eval "$(extract_fn "$GLO" run_script)"
        eval "$(extract_fn "$GLO" add_game_to_recent_list)"
        log() { :; }
        sysdir=$TMP/sys emupath=$TMP rompath=$TMP/rom.gb
        currentrecentlist=$TMP/recent.json recentlist_temp=$TMP/recent.tmp
        firstrecententry='{"label":"Mario"}'
        printf '{"label":"Zelda"}\n' > "$currentrecentlist"
        mkdir -p "$TMP/sys"
        echo 'launch mario' > "$TMP/sys/cmd_to_run.sh"
        printf '#!/bin/sh\n%s\n' "$1" > "$TMP/script.sh"
        run_script "$TMP/script.sh"
    )
}
glo_run 'exit 0'
check "status 0 is passed on (start the game)" test $? -eq 0
check "and the game is first in Recents again" test "$(head -n 1 "$TMP/recent.json")" = '{"label":"Mario"}'
glo_run 'exit 1'
check "status 1 is passed on (back to the list)" test $? -eq 1
check "and Recents is left as it was" test "$(cat "$TMP/recent.json")" = '{"label":"Zelda"}'
glo_run 'echo "launch random" > "$(dirname "$0")/sys/cmd_to_run.sh"; exit 0'
check "another game chosen: started" test $? -eq 0
check "and its entry is the script's business" test "$(cat "$TMP/recent.json")" = '{"label":"Zelda"}'
unset -f glo_run
end

begin system_default_core
# GLO > Game core > "All <system> games" makes a core the default of every
# game of that system (asked on Reddit: mGBA for all GBA games). It is kept
# in .tmp_update/config/system_cores, which updates leave alone (they
# reinstall Emu/<system>/launch.sh), and a core set for one game still wins.
for fn in system_core_file system_default_core; do
    check "runtime.sh and GLO share $fn" test "$(extract_fn "$GLO" $fn)" = "$(extract_fn "$RUNTIME" $fn)"
done
sysdir_saved=${sysdir-}
    eval "$(extract_fn "$GLO" system_core_file)"
    eval "$(extract_fn "$GLO" system_default_core)"
    eval "$(extract_fn "$GLO" set_system_default_core)"
    sysdir=$TMP/sys
    check "the file is named after the emulator folder" \
        test "$(system_core_file /mnt/SDCARD/Emu/GBA)" = "$TMP/sys/config/system_cores/Emu-GBA"
    check "a trailing slash is the same folder" \
        test "$(system_core_file /mnt/SDCARD/Emu/GBA/)" = "$TMP/sys/config/system_cores/Emu-GBA"
    check "no file: no system core" test -z "$(system_default_core /mnt/SDCARD/Emu/GBA)"
    set_system_default_core /mnt/SDCARD/Emu/GBA mgba_libretro gpsp_libretro
    check "All games: the core is saved" test "$(system_default_core /mnt/SDCARD/Emu/GBA)" = mgba_libretro
    check "other systems are not touched" test -z "$(system_default_core /mnt/SDCARD/Emu/GB)"
    set_system_default_core /mnt/SDCARD/Emu/GBA gpsp_libretro gpsp_libretro
    check "back to the launcher's core: the file goes" test ! -e "$TMP/sys/config/system_cores/Emu-GBA"
check "change_core asks for the scope and saves All games" sh -c \
    "sed -n '/^change_core() {/,/^}/p' '$GLO' | grep -q 'Only this game' && sed -n '/^change_core() {/,/^}/p' '$GLO' | grep -q 'set_system_default_core \"\$emupath\" \"\$new_core\" \"\$launcher_core\"'"

# GLO shows the system core as the default (get_core_info uses ash's "==":
# only default_core and launcher_core are checked under this shell)
    eval "$(extract_fn "$GLO" get_core_info)"
    eval "$(extract_fn "$GLO" get_info_value)"
    eval "$(extract_fn "$GLO" system_core_file)"
    eval "$(extract_fn "$GLO" system_default_core)"
    log() { :; }
    sysdir=$TMP/sys radir=$TMP/ra emupath=/mnt/SDCARD/Emu/GBA romext=gba
    rompath=$TMP/Roms/GBA/Game.gba launch_path=$TMP/launch.sh
    mkdir -p "$TMP/ra/cores" "$TMP/Roms/GBA" "$TMP/sys/config/system_cores"
    : > "$TMP/ra/cores/mgba_libretro.so"
    echo 'default_core=gpsp' > "$launch_path"
    get_core_info
    check "no system core: the launcher's" test "$default_core" = gpsp_libretro
    echo mgba_libretro > "$TMP/sys/config/system_cores/Emu-GBA"
    get_core_info
    check "system core: the default" test "$default_core:$launcher_core" = mgba_libretro:gpsp_libretro
    rm -f "$TMP/ra/cores/mgba_libretro.so"
    get_core_info
    check "system core not installed: the launcher's" test "$default_core" = gpsp_libretro

# runtime.sh starts the game with it
    eval "$(extract_fn "$RUNTIME" override_game_core)"
    eval "$(extract_fn "$RUNTIME" launcher_emu_dir)"
    eval "$(extract_fn "$RUNTIME" system_core_file)"
    eval "$(extract_fn "$RUNTIME" system_default_core)"
    eval "$(extract_fn "$RUNTIME" get_info_value)" 2> /dev/null
    get_info_value() { echo "$1" | grep "$2\b" | awk '{split($0,a,/\s*=\s*/); print a[2]}' | tr -d '"'; }
    log() { :; }
    sysdir=$TMP/sys2 miyoodir=/mnt/SDCARD/miyoo rompath=/mnt/SDCARD/Roms/GBA/Game.gba
    mkdir -p "$TMP/sys2/config/system_cores" "$TMP/rapp"
    check "launcher folder of launch.sh" test "$(launcher_emu_dir /mnt/SDCARD/Emu/GBA/launch.sh)" = /mnt/SDCARD/Emu/GBA
    check "launcher folder of a proxy" test "$(launcher_emu_dir /mnt/SDCARD/Emu/GBA/../../.tmp_update/proxy.sh)" = /mnt/SDCARD/Emu/GBA
    echo mgba_libretro > "$TMP/sys2/config/system_cores/Emu-GBA"
    : > "$TMP/launch.sh"
    # the core must exist under /mnt/SDCARD/RetroArch: fake it with a missing one
    override_game_core "$TMP/none.cfg" /mnt/SDCARD/Emu/GBA/launch.sh
    check "core file missing on this PC: falls back, no command written" test ! -e "$TMP/sys2/cmd_to_run.sh"
    check "it looked for the system core" sh -c "sed -n '/^override_game_core() {/,/^}/p' '$RUNTIME' | grep -q 'system_default_core \"\$(launcher_emu_dir \"\$launch_path\")\"'"
    check "a game's own core is read first" sh -c "sed -n '/^override_game_core() {/,/^}/p' '$RUNTIME' | awk '/get_info_value \"\\\$romcfg\" core/{a=NR} /system_default_core/{b=NR} END{exit !(a && b && a < b)}'"
sysdir=$sysdir_saved
unset -f get_core_info get_info_value system_core_file system_default_core set_system_default_core override_game_core launcher_emu_dir log
end

begin m3u_gen_lists_only_discs_and_bracketed_titles
# #310: the extension filter was a character class ("*[cue|gdi|...]"), so
# "(Disc 1).png" box art was taken for discs, and a title with [ ] (such as
# "[T-En]") was a glob class too: its discs were never moved or listed.
M3U="$ROOT/static/build/.tmp_update/script/m3u_gen.sh"
m3u_run() { # mode (-sd / -md)
    M3U_ROMS_ROOT="$TMP/Roms" M3U_CUE_GEN=true sh "$M3U" "$1" > /dev/null 2>&1
}
mkdir -p "$TMP/Roms/PS"
cd "$TMP/Roms/PS"
: > "Example (Disc 1).chd"; : > "Example (Disc 2).chd"
: > "Picture (Disc 1).png"; : > "Picture (Disc 2).png"
: > "Example [T-En] (Disc 1).chd"; : > "Example [T-En] (Disc 2).chd"
: > "Other (Disc 1) (Rev 1).cue"; : > "Other (Disc 1) (Rev 1).bin"
: > "Other (Disc 2) (Rev 1).cue"; : > "Other (Disc 2) (Rev 1).bin"
cd "$ROOT"
m3u_run -sd
P=$TMP/Roms/PS
check "plain title: two discs listed" test "$(cat "$P/Example.m3u" 2> /dev/null)" = "$(printf '.multi-disc/Example (Disc 1).chd\n.multi-disc/Example (Disc 2).chd')"
check "pictures are not discs: no playlist" test ! -e "$P/Picture.m3u"
check "and they stay where they are" test -f "$P/Picture (Disc 1).png" -a -f "$P/Picture (Disc 2).png"
check "bracketed title: discs moved" test -f "$P/.multi-disc/Example [T-En] (Disc 1).chd" -a -f "$P/.multi-disc/Example [T-En] (Disc 2).chd"
check "bracketed title: both listed" test "$(cat "$P/Example [T-En].m3u" 2> /dev/null)" = "$(printf '.multi-disc/Example [T-En] (Disc 1).chd\n.multi-disc/Example [T-En] (Disc 2).chd')"
check "type 2: the .cue files listed, not the .bin" test "$(cat "$P/Other (Rev 1).m3u" 2> /dev/null)" = "$(printf '.multi-disc/Other (Disc 1) (Rev 1).cue\n.multi-disc/Other (Disc 2) (Rev 1).cue')"
check "and the .bin moved with them" test -f "$P/.multi-disc/Other (Disc 2) (Rev 1).bin"
# One folder per game (-md)
rm -rf "$TMP/Roms"; mkdir -p "$TMP/Roms/PS"
: > "$TMP/Roms/PS/Game [!] (Disc 1).chd"; : > "$TMP/Roms/PS/Game [!] (Disc 2).chd"
m3u_run -md
check "-md: bracketed title in its own folder" test "$(cat "$TMP/Roms/PS/Game [!].m3u" 2> /dev/null)" = "$(printf '.Game [!]/Game [!] (Disc 1).chd\n.Game [!]/Game [!] (Disc 2).chd')"
# Type 3, "(Disc 1 of 2)": the playlist name has no space at the end...
rm -rf "$TMP/Roms"; mkdir -p "$TMP/Roms/SEGACD"
: > "$TMP/Roms/SEGACD/Lunar (Disc 1 of 2).iso"; : > "$TMP/Roms/SEGACD/Lunar (Disc 2 of 2).iso"
m3u_run -sd
check "type 3: playlist named without the space" test "$(cat "$TMP/Roms/SEGACD/Lunar.m3u" 2> /dev/null)" = "$(printf '.multi-disc/Lunar (Disc 1 of 2).iso\n.multi-disc/Lunar (Disc 2 of 2).iso')"
check "and no \"Lunar .m3u\"" test ! -e "$TMP/Roms/SEGACD/Lunar .m3u"
# ...but one made before keeps its name (its saves follow it)
rm -rf "$TMP/Roms"; mkdir -p "$TMP/Roms/SEGACD"
: > "$TMP/Roms/SEGACD/Lunar (Disc 1 of 2).iso"; : > "$TMP/Roms/SEGACD/Lunar (Disc 2 of 2).iso"
: > "$TMP/Roms/SEGACD/Lunar .m3u"
m3u_run -sd
check "type 3: an old \"Lunar .m3u\" is kept and filled" test -s "$TMP/Roms/SEGACD/Lunar .m3u"
check "and no second playlist" test ! -e "$TMP/Roms/SEGACD/Lunar.m3u"
unset -f m3u_run
end

begin press_menu_to_kill_spares_glo
# pressMenu2Kill runs "pkill -9 -f PATTERN": a literal pattern must not
# also match the GLO script that launched it.
bad=""
for f in $(grep -rl "pressMenu2Kill" "$ROOT/static" --include=*.sh); do
    for pat in $(sed -n 's/^[[:space:]]*pressMenu2Kill \([^ $&]*\) &.*/\1/p' "$f"); do
        if echo "/bin/sh ./script/game_list_options.sh" | grep -q -- "$pat"; then
            bad="$bad ${f#$ROOT/}:$pat"
        fi
    done
done
check "no pattern matches game_list_options.sh:$bad" test -z "$bad"
end

# ---- game_list_options.sh: a filter dropped with the rom list cache ----

eval "$(extract_fn "$GLO" filter_is_applied)"

glo_filter_env() { # rows in the cache matching '~Filter: %' (none: no cache)
    emupath=$TMP/Emu/GBA
    romroot=$emupath/../../Roms/GBA
    mkdir -p "$emupath" "$TMP/Roms/GBA"
    echo zelda > "$emupath/active_filter"
    rm -f "$TMP/Roms/GBA/GBA_cache6.db"
    if [ -n "$1" ]; then
        echo "$1" > "$TMP/Roms/GBA/GBA_cache6.db"
    fi
}
sqlite3() { # db, query: the db file holds the count the query would return
    case "$2" in
        *'FROM "GBA_roms" WHERE disp LIKE '"'~Filter: %'"*) ;;
        *) return 1 ;;
    esac
    [ "$(cat "$1")" = error ] && return 1
    cat "$1"
}

begin glo_hide_game_and_bring_it_back
# Hide game moves the game, with the tracks of a .cue, into
# <folder>/.hidden/<rom name>/ (dot names are not listed); Hidden games puts
# exactly those files back. The rom list cache is dropped both times.
for fn in glo_game_files count_hidden_games drop_from_recents drop_rom_list_cache hide_game show_hidden_games unhide_game; do
    eval "$(extract_fn "$GLO" $fn)"
done
log() { :; }
infoPanel() { echo "$*" >> "$TMP/info"; }
R="$TMP/Roms/PS"
mkdir -p "$R" "$TMP/bin"
printf '#!/bin/sh\nexit $(cat "%s/answer")\n' "$TMP" > "$TMP/bin/prompt"
chmod +x "$TMP/bin/prompt"
: > "$R/PS_cache6.db"
printf 'FILE "Game (Track 1).bin" BINARY\r\nFILE "Game (Track 2).bin" BINARY\r\n' > "$R/Game.cue"
: > "$R/Game (Track 1).bin"; : > "$R/Game (Track 2).bin"; : > "$R/Game 2.cue"; : > "$R/Other.chd"
romroot="$R"
recentlist="$TMP/recentlist.json"; recentlist_hidden="$TMP/recentlist-hidden.json"; recentlist_temp="$TMP/recent.tmp"
printf '{"label":"Other","rompath":"%s/Other.chd","type":5}\n{"label":"Game 2","rompath":"%s/Game 2.cue","type":5}\n' "$R" "$R" > "$recentlist"
printf '{"label":"Other","rompath": "%s/Other.chd","type":5}\n' "$(cd "$R" && pwd -P)" > "$recentlist_hidden"
cd "$TMP"
rompath="$R/Game.cue"
echo 1 > "$TMP/answer"
( hide_game ); rc=$?
check "cancel: nothing moves" test -f "$R/Game.cue" -a ! -d "$R/.hidden"
check "cancel: back to the list" test $rc -eq 1
echo 0 > "$TMP/answer"
( hide_game )
check "the cue and its tracks are hidden" test -f "$R/.hidden/Game/Game.cue" -a -f "$R/.hidden/Game/Game (Track 1).bin" -a -f "$R/.hidden/Game/Game (Track 2).bin"
check "nothing of them stays in the list" test ! -e "$R/Game.cue" -a ! -e "$R/Game (Track 1).bin"
check "another game with a longer name stays" test -f "$R/Game 2.cue"
check "the list cache is dropped" test ! -e "$R/PS_cache6.db"
check "one hidden game" test "$(count_hidden_games "$R/.hidden")" -eq 1
rompath="$R/Other.chd"
( hide_game )
check "a single-file game is hidden" test -f "$R/.hidden/Other/Other.chd"
check "it leaves Recents" sh -c '! grep -q "Other.chd" "$1"' _ "$recentlist"
check "and the other entries stay" grep -q 'Game 2.cue' "$recentlist"
check "also from the hidden Recents (resolved path, spaces)" sh -c '! grep -q "Other.chd" "$1"' _ "$recentlist_hidden"
check "two hidden games" test "$(count_hidden_games "$R/.hidden")" -eq 2
: > "$R/PS_cache6.db"
rompath="$R/Game 2.cue"
echo 0 > "$TMP/answer"   # the first in the list: Game
( show_hidden_games )
check "Game is back with its tracks" test -f "$R/Game.cue" -a -f "$R/Game (Track 2).bin"
check "its hidden folder is gone" test ! -d "$R/.hidden/Game"
check "the other stays hidden" test -f "$R/.hidden/Other/Other.chd"
check "the list cache is dropped again" test ! -e "$R/PS_cache6.db"
echo 0 > "$TMP/answer"
( show_hidden_games )
check "the last one back removes .hidden" test -f "$R/Other.chd" -a ! -d "$R/.hidden"
echo 0 > "$TMP/answer"
( show_hidden_games ); rc=$?
check "no hidden game: back to the list" test $rc -eq 1
# A name already taken is not overwritten
rompath="$R/Other.chd"; ( hide_game ); : > "$R/Other.chd"
( show_hidden_games )
check "taken name: the hidden copy stays" test -f "$R/.hidden/Other/Other.chd"
check "and it says why" grep -q "already in the list folder" "$TMP/info"
cd "$ROOT"
unset -f glo_game_files count_hidden_games drop_from_recents drop_rom_list_cache hide_game show_hidden_games unhide_game log infoPanel
check "offered in Games and Expert" sh -c "grep -q 'add_menu_option hide_game \"Hide game\"' '$GLO' && grep -q 'add_menu_option show_hidden_games' '$GLO'"
end

begin miyoogamelist_lists_hidden_games_at_their_place
# A list made while a game was hidden (GLO Hide game, .hidden/<name>/) left
# it out, so it never showed again once back: it is listed at its usual
# path, which MainUI skips while the file is not there.
GEN="$ROOT/static/build/.tmp_update/script/miyoogamelist_gen.sh"
mkdir -p "$TMP/Emu/GBA" "$TMP/Roms/GBA/.hidden/Zelda" "$TMP/Roms/GBA/.hidden/Dup"
printf '{\n\t"label": "GBA",\n\t"rompath": "../../Roms/GBA",\n\t"imgpath": "../../Roms/GBA/Imgs",\n\t"extlist": "gba|zip"\n}\n' > "$TMP/Emu/GBA/config.json"
: > "$TMP/Roms/GBA/Mario (USA).gba"
: > "$TMP/Roms/GBA/.hidden/Zelda/Zelda (USA).gba"; : > "$TMP/Roms/GBA/.hidden/Zelda/readme.txt"
: > "$TMP/Roms/GBA/.hidden/Dup/Mario (USA).gba"
(cd "$TMP" && MIYOOGAMELIST_EMU_DIR="$TMP/Emu" sh "$GEN" > /dev/null 2>&1)
X="$TMP/Roms/GBA/miyoogamelist.xml"
check "the visible game is listed" grep -q '<path>./Mario (USA).gba</path>' "$X"
check "the hidden one too, at its usual path" grep -q '<path>./Zelda (USA).gba</path>' "$X"
check "not with its hidden path" sh -c '! grep -q "\.hidden" "$1"' _ "$X"
check "a hidden copy of a visible game is not listed twice" test "$(grep -c 'Mario (USA).gba' "$X")" -eq 1
check "non-game files are left out" sh -c '! grep -q readme "$1"' _ "$X"
end

begin glo_filter_dropped_with_cache
glo_filter_env 1
check "filter row in the cache: applied" filter_is_applied
glo_filter_env 0
check "cache rebuilt without the filter row: not applied" test "$(filter_is_applied && echo yes)" = ""
glo_filter_env ""
check "no cache at all: not applied" test "$(filter_is_applied && echo yes)" = ""
glo_filter_env error
check "sqlite3 error: filter kept" filter_is_applied
romroot=""
check "unknown rom folder: filter kept" filter_is_applied
check "no Refresh list entry: the Select menu has Refresh roms" \
    test -z "$(grep -n 'add_menu_option refresh_roms\|^refresh_roms()' "$GLO")"
unset -f sqlite3
end

begin glo_filter_can_be_hidden
# Tweaks > Game lists > List menus > GLO > "Filter list" Off writes
# config/.hideGloFilter: no Filter list, no Current filter, but Clear filter
# while a filter is set.
eval "$(extract_fn "$GLO" glo_shown)"
eval "$(extract_fn "$GLO" add_filter_options)"
log() { :; }
add_menu_option() { echo "$1" >> "$TMP/menu"; }
filter_is_applied() { return 0; }
sysdir="$TMP/sys"; mkdir -p "$sysdir/config"
emupath="$TMP/Emu/GBA"; mkdir -p "$emupath"
menu() { rm -f "$TMP/menu"; add_filter_options; tr '\n' ' ' < "$TMP/menu" 2> /dev/null; }
check "shown, no filter: Filter list" test "$(menu)" = "filter_roms "
echo zelda > "$emupath/active_filter"
check "shown, a filter: Clear filter and Current filter" test "$(menu)" = "clear_filter filter_roms "
: > "$sysdir/config/.hideGloFilter"
check "hidden, a filter: only Clear filter" test "$(menu)" = "clear_filter "
rm -f "$emupath/active_filter"
check "hidden, no filter: nothing" test -z "$(menu)"
check "Tweaks writes the same flag" grep -q '{"Filter list", "Filter", false,' "$ROOT/src/tweaks/main_menu.h"
check "Tweaks names the flags .hideGlo<name>" grep -qF '"%s/.hideGlo%s"' "$ROOT/src/tweaks/main_menu.h"
unset -f filter_is_applied add_menu_option menu
end

begin glo_entries_can_be_hidden
# List menus > GLO > "Reset game/Game manual/Game core/Hide game/Netplay/
# Scraper" Off: the entry is left out of GLO.
eval "$(extract_fn "$GLO" glo_shown)"
sysdir="$TMP/sys"; mkdir -p "$sysdir/config"
check "no flag: shown" glo_shown Reset
: > "$sysdir/config/.hideGloReset"
if glo_shown Reset; then r=shown; else r=hidden; fi
check "flag: hidden" test "$r" = hidden
for name in Reset Manual Core Hide; do
    check "the menu checks $name" grep -q "glo_shown $name" "$GLO"
done
check "Game core and Restore default core go together" test "$(grep -c 'glo_shown Core' "$GLO")" -eq 2
check "an empty GLO says where the entries went" grep -qF 'add_menu_option : "Nothing to show"' "$GLO"
check "Hidden games is not behind a flag" sh -c "grep 'show_hidden_games \"Hidden games' '$GLO' | grep -vq glo_shown"

# Scripts: Netplay and Scraper are skipped while hidden, others stay.
eval "$(extract_fn "$GLO" script_metadata)"
eval "$(extract_fn "$GLO" expand_script_label)"
eval "$(extract_fn "$GLO" add_script_files)"
_glo_sep=$(printf '\037')
log() { :; }
add_menu_option() { echo "$2" >> "$TMP/menu"; }
has_networking=1
emulabel=GBA
sd="$TMP/romscripts"; mkdir -p "$sd"
for n in Netplay Scraper Other; do
    printf 'scriptlabel="%s"\nrequire_networking=0\n' "$n" > "$sd/$n.sh"
done
scripts() { rm -f "$TMP/menu"; add_script_files "$sd"; tr '\n' ' ' < "$TMP/menu" 2> /dev/null; }
check "all shown" test "$(scripts)" = "Netplay Other Scraper "
: > "$sysdir/config/.hideGloNetplay"
check "Netplay hidden" test "$(scripts)" = "Other Scraper "
: > "$sysdir/config/.hideGloScraper"
check "Scraper hidden too" test "$(scripts)" = "Other "
unset -f add_menu_option scripts
end

# ---- SD card: a notice when the FAT was not unmounted cleanly ----

begin sd_card_dirty_notice
eval "$(extract_fn "$RUNTIME" sd_card_dirty)"
eval "$(extract_fn "$RUNTIME" check_sd_card)"
log() { :; }
sd_card_notice() { echo shown >> "$TMP/panel"; }
sysdir="$TMP/sys"; mkdir -p "$sysdir/config"
printf '[    2.1] mmcblk0: mmc0:aaaa SC64G 59.5 GiB\n[    2.3] FAT-fs (mmcblk0p1): Volume was not properly unmounted. Some data may be corrupt. Please run fsck.\n' > "$TMP/dirty"
printf '[    2.1] mmcblk0: mmc0:aaaa SC64G 59.5 GiB\n[    2.4] random: crng init done\n' > "$TMP/clean"
printf '[    9.0] FAT-fs (sda1): Volume was not properly unmounted. Some data may be corrupt. Please run fsck.\n' > "$TMP/usb"
export SD_DMESG_CMD="cat $TMP/dirty"; sd_card_dirty; _rc=$?; unset SD_DMESG_CMD; rc $_rc
check "the kernel's message: dirty" test $? -eq 0
export SD_DMESG_CMD="cat $TMP/clean"; sd_card_dirty; _rc=$?; unset SD_DMESG_CMD; rc $_rc
check "no message: clean" test $? -ne 0
export SD_DMESG_CMD="cat $TMP/usb"; sd_card_dirty; _rc=$?; unset SD_DMESG_CMD; rc $_rc
check "another FAT device (a USB stick) does not count" test $? -ne 0
export SD_DMESG_CMD="cat $TMP/missing"; sd_card_dirty; _rc=$?; unset SD_DMESG_CMD; rc $_rc
check "dmesg failing: clean, no notice" test $? -ne 0
check_sd_card 1
check "dirty: the notice shows" test "$(cat "$TMP/panel" 2> /dev/null)" = shown
check "and is remembered" test -f "$sysdir/config/.sdDirtyNotified"
check_sd_card 1
check "still dirty on the next boot: not shown again" test "$(grep -c shown "$TMP/panel")" -eq 1
check_sd_card 0
check "a clean boot forgets it" test ! -e "$sysdir/config/.sdDirtyNotified"
check_sd_card 1
check "dirty again later: shown again" test "$(grep -c shown "$TMP/panel")" -eq 2
check "read at boot, shown before the auto launch" sh -c "grep -q 'sd_card_dirty && sd_card_was_dirty=1' '$RUNTIME' && grep -q 'check_sd_card \"\$sd_card_was_dirty\"' '$RUNTIME'"
unset -f sd_card_notice
# The notice: SD Card Doctor (when installed), how to repair on a PC, OK.
eval "$(extract_fn "$RUNTIME" sd_card_notice)"
prompt() { # answers from a file, one per call; records the choices offered
    shift 2; shift 2
    echo "$*" >> "$TMP/offered"
    _a=$(head -n 1 "$TMP/answers"); sed -i 1d "$TMP/answers"; return "${_a:-255}"
}
start_audioserver() { echo started >> "$TMP/audio"; }
SD_DOCTOR_DIR=$TMP/doctor; mkdir -p "$SD_DOCTOR_DIR"
printf '#!/bin/sh\necho "ran $*" > "%s/doctor_ran"\n' "$TMP" > "$SD_DOCTOR_DIR/launch.sh"; chmod +x "$SD_DOCTOR_DIR/launch.sh"
rm -f "$TMP/offered"; printf '1\n0\n' > "$TMP/answers"; sd_card_notice
check "with SD Card Doctor: three choices" grep -q "^Check it now (SD Card Doctor) How to repair it on a PC OK$" "$TMP/offered"
check "the PC page: OK first, or the check on the console" test "$(sed -n 2p "$TMP/offered")" = "OK Check it now (SD Card Doctor)"
check "its OK goes on: no question again, nothing opened (it took three presses to leave)" sh -c "test \$(wc -l < '$TMP/offered') -eq 2 && test ! -f '$TMP/doctor_ran'"
rm -f "$TMP/offered"; printf '1\n1\n' > "$TMP/answers"; sd_card_notice
check "the check from the PC page opens SD Card Doctor" test -f "$TMP/doctor_ran"
rm -f "$TMP/offered" "$TMP/doctor_ran"; printf '0\n' > "$TMP/answers"; sd_card_notice
check "Check it now opens SD Card Doctor" test -f "$TMP/doctor_ran"
check "on Safe repair, the cursor on CHECK FAT STATUS (--fat-status)" test "$(cat "$TMP/doctor_ran")" = "ran --fat-status"
check "its launcher passes the option on, the app reads it" sh -c 'grep -qF "./sdcarddoctor \"\$@\"" "$1" && grep -q "strcmp(argv\[1\],\"--fat-status\")==0" "$2"' _ "$ROOT/static/packages/App/SD Card Doctor/App/SDCardDoctor/launch.sh" "$ROOT/src/sdcarddoctor/sdcarddoctor.c"
# prompt's click without libpadsp.so, with the audio server running: killed
# by SIGFPE on A (exit 136) at boot, so SD Card Doctor never started.
check "the audio server is started for the notice" test -s "$TMP/audio"
check "each question plays its click through the audio server (libpadsp.so)" test "$(sed -n '/^sd_card_notice()/,/^}/p' "$RUNTIME" | grep -c 'LD_PRELOAD="\$miyoodir/lib/libpadsp.so" prompt -t')" -eq "$(sed -n '/^sd_card_notice()/,/^}/p' "$RUNTIME" | grep -c 'prompt -t')"
rm -f "$TMP/offered" "$TMP/doctor_ran"; printf '255\n' > "$TMP/answers"; sd_card_notice
check "B goes on, nothing opened" test ! -f "$TMP/doctor_ran"
rm -f "$SD_DOCTOR_DIR/launch.sh" "$TMP/offered"; printf '0\n0\n' > "$TMP/answers"; sd_card_notice
check "without it: two choices" grep -q "^How to repair it on a PC OK$" "$TMP/offered"
check "the first is the PC page, its OK goes on" sh -c "test \$(wc -l < '$TMP/offered') -eq 2 && test \"\$(sed -n 2p '$TMP/offered')\" = OK"
check "SD Card Doctor is preinstalled (where the notice looks for it)" sh -c 'grep -qF "cp -a \"\$(PACKAGES_APP_DEST)/SD Card Doctor/.\" \$(BUILD_DIR)/" "$1" && test -f "$2/static/packages/App/SD Card Doctor/App/SDCardDoctor/launch.sh" && grep -q "SD_DOCTOR_DIR:-/mnt/SDCARD/App/SDCardDoctor" "$3"' _ "$ROOT/Makefile" "$ROOT" "$RUNTIME"
check "the PC page says how on Windows, Mac and Linux" sh -c 'grep -q "Tools > Check" "$1" && grep -q "First Aid" "$1" && grep -q "fsck.fat -a" "$1"' _ "$RUNTIME"
long=$(sed -n '/^sd_card_notice()/,/^}/p' "$RUNTIME" | grep -o '\-m "[^"]*"' | sed 's/^-m "//; s/"$//; s/\\n/\n/g' | awk 'length > 44' | head -n 1)
check "message lines of 44 characters or less ($long)" test -z "$long"
unset -f prompt sd_card_notice log start_audioserver
end

begin ota_reboots_like_4_4_0
check "the OTA ends with a plain reboot, as in 4.4.0" sh -c "grep -A2 'Press A to reboot\"\$' '$OTA' | grep -q 'sleep 1' && grep -q '^			reboot$' '$OTA'"
check "the OTA no longer calls bin/shutdown" sh -c "! grep -q 'bin/shutdown' '$OTA'"
check "shutdown -r reboots" grep -q 'if \[ "\$1" = "-r" \]; then' "$ROOT/static/build/.tmp_update/bin/shutdown"
end

begin installer_reboots_with_the_clean_shutdown
INST="$ROOT/static/dist/miyoo/app/.tmp_update/install.sh"
# Run, not read: a call commented out or moved behind an exit passed a
# grep for its text.
eval "$(extract_fn "$INST" installer_reboot | sed "s#/mnt/SDCARD#$TMP/sd#g")"
sync() { echo sync >> "$TMP/calls"; }
reboot() { echo reboot >> "$TMP/calls"; }
mkdir -p "$TMP/sd/.tmp_update/bin"
printf '#!/bin/sh\necho "shutdown $*" >> "%s/calls"\n' "$TMP" > "$TMP/sd/.tmp_update/bin/shutdown"
chmod +x "$TMP/sd/.tmp_update/bin/shutdown"
installer_reboot
check "the installer's reboot: sync, bin/shutdown -r, then reboot" test "$(tr '\n' ' ' < "$TMP/calls")" = "sync shutdown -r reboot "
# The console's settings: saved ones are not replaced by the card's copy,
# which during an installation holds the defaults (beta 3: brightness,
# volume and colours reset by the update)
read_uuid() { echo 0123abcd; }
mkdir -p "$TMP/sd/.tmp_update/config/system"
echo '{"brightness": 3}' > "$TMP/sd/.tmp_update/config/system/0123abcd.json"
echo '{"brightness": 7}' > "$TMP/sd/system.json"
rm -f "$TMP/calls"
installer_reboot
check "settings saved for this console: the card's copy is dropped before the shutdown" test ! -e "$TMP/sd/system.json"
check "and the saved ones are left alone" grep -q '"brightness": 3' "$TMP/sd/.tmp_update/config/system/0123abcd.json"
check "then the same reboot" test "$(tr '\n' ' ' < "$TMP/calls")" = "sync sync shutdown -r reboot "
rm -f "$TMP/sd/.tmp_update/config/system/0123abcd.json"
echo '{"brightness": 7}' > "$TMP/sd/system.json"
installer_reboot
check "none saved (a first installation): the card's copy stays, for the shutdown to save" test -f "$TMP/sd/system.json"
# An OTA update reboots plainly: the card's system.json was there when the
# installation started, the live settings, newer than the saved ones
echo '{"brightness": 3}' > "$TMP/sd/.tmp_update/config/system/0123abcd.json"
echo '{"brightness": 10}' > "$TMP/sd/system.json"
card_settings_live=1
installer_reboot
check "the card's copy there at the start (OTA update): kept, for the shutdown to save" grep -q '"brightness": 10' "$TMP/sd/system.json"
card_settings_live=0
installer_reboot
check "not there at the start (after a shutdown): dropped" test ! -e "$TMP/sd/system.json"
echo '{"brightness": 7}' > "$TMP/sd/system.json"
reset_configs=1
installer_reboot
check "Reinstall (reset settings): the card's defaults are kept" test -f "$TMP/sd/system.json"
unset reset_configs
rm -f "$TMP/sd/.tmp_update/config/system/0123abcd.json"
echo '{"brightness": 7}' > "$TMP/sd/system.json"
unset card_settings_live
read_uuid() { return 1; }
installer_reboot
check "no serial number: the card's copy stays" test -f "$TMP/sd/system.json"
unset -f read_uuid
rm -f "$TMP/calls" "$TMP/sd/.tmp_update/bin/shutdown" "$TMP/sd/system.json"
installer_reboot
check "with a plain reboot only if the script is missing" test "$(tr '\n' ' ' < "$TMP/calls")" = "sync reboot "
unset -f sync reboot installer_reboot
check "used at the end of the installation" sh -c "tail -n 5 '$INST' | grep -q '^installer_reboot$'"
check "and when the extraction fails" sh -c "grep -B2 'installer_reboot' '$INST' | grep -q 'Installation failed'"
check "no other plain reboot but the firmware check's (no shutdown script left there)" test "$(grep -cE '^[[:space:]]+reboot$' "$INST")" -eq 2
check "the shutdown script stops the installer's updater" grep -q 'killall -9 updater' "$ROOT/static/build/.tmp_update/bin/shutdown"
end

begin accent_colour_follows_tweaks_and_the_theme
AC="$ROOT/static/build/.tmp_update/script/accent_color.sh"
RES="$ROOT/static/build/.tmp_update/res/accent"
mkdir -p "$TMP/sd/Themes/Silky RedOnion/skin" "$TMP/sd/Themes/Other/skin" "$TMP/sys/config/display" "$TMP/ov" "$TMP/bin"
cp "$ROOT/static/build/Themes/Silky RedOnion/skin/bg-list-s.png" "$TMP/sd/Themes/Silky RedOnion/skin/"
silky_json() { printf '{\n\t"theme":\t"%s/sd/Themes/%s/",\n\t"brightness":\t7\n}\n' "$TMP" "$1" > "$TMP/system.json"; }
# a stand-in pngresize: copies the picture and notes the size asked
printf '#!/bin/sh\nfor a; do case "$a" in --*) ;; *) set -- "$@" "$a" ;; esac; shift; done\ncp "$1" "$2"; echo "$3x$4" >> "%s/resized"\n' "$TMP" > "$TMP/bin/pngresize"
chmod +x "$TMP/bin/pngresize"
ac() { SDROOT="$TMP/sd" SYSDIR="$TMP/sys" ACCENT_RES="$RES" OVERRIDES="$TMP/ov" SYSTEM_JSON="$TMP/system.json" PNGRESIZE="$TMP/bin/pngresize" PNGSCALE=/none sh "$AC" "$@"; }
silky_json "Silky RedOnion"
ac
check "no colour chosen: red, nothing copied" test "$(cat "$TMP/sys/config/display/accentRGB")" = "9E2C45 D65670 782036"
check "and no theme image in the overrides" test ! -e "$TMP/ov/skin/bg-list-s.png"
echo 1 > "$TMP/sys/config/display/accentColor"
mkdir -p "$TMP/ov/skin"
echo mine > "$TMP/ov/skin/icon-A-54.png"
ac
check "blue: the colours for the programs" test "$(cat "$TMP/sys/config/display/accentRGB")" = "2F5FB3 5E94F2 224788"
check "the blue selection in the overrides" cmp -s "$TMP/ov/skin/bg-list-s.png" "$RES/blue/skin/bg-list-s.png"
check "and the GameSwitcher bar, in skin/extra" cmp -s "$TMP/ov/skin/extra/gs-bottom-bar.png" "$RES/blue/skin/extra/gs-bottom-bar.png"
check "the user's own override stays" grep -q mine "$TMP/ov/skin/icon-A-54.png"
check "and is not listed as ours" sh -c "! grep -q '^skin/icon-A-54.png$' '$TMP/ov/.accent_files'"
files_id=$(sed -n '1s/^# set //p' "$RES/files.txt")
check "files.txt starts with the id of its set of pictures" test -n "$files_id"
check "the state on the first line" test "$(head -n 1 "$TMP/ov/.accent_files")" = "Silky RedOnion|blue||$files_id"
check "the id line is not taken for a picture" sh -c "! grep -q '^#' '$TMP/ov/.accent_files'"
check "the top bars' logo follows the colour" cmp -s "$TMP/ov/skin/miyoo-topbar.png" "$RES/blue/skin/miyoo-topbar.png"
check "in the GameSwitcher too" cmp -s "$TMP/ov/skin/extra/gs-top-bar.png" "$RES/blue/skin/extra/gs-top-bar.png"
touch -d '2001-01-01' "$TMP/ov/skin/bg-list-s.png" 2> /dev/null || touch -t 200101010000 "$TMP/ov/skin/bg-list-s.png"
before=$(ls -l "$TMP/ov/skin/bg-list-s.png")
ac
check "nothing changed: nothing copied again (every boot)" test "$(ls -l "$TMP/ov/skin/bg-list-s.png")" = "$before"
# An update that brings another set of pictures (the top bars, 4.5.1):
# copied again for the colour already applied, from the state it left
mkdir -p "$TMP/res2"
cp -r "$RES/." "$TMP/res2/"
rm -f "$TMP/ov/skin/miyoo-topbar.png"
sed -i '/^skin\/miyoo-topbar.png$/d' "$TMP/ov/.accent_files"
SDROOT="$TMP/sd" SYSDIR="$TMP/sys" ACCENT_RES="$TMP/res2" OVERRIDES="$TMP/ov" SYSTEM_JSON="$TMP/system.json" PNGRESIZE="$TMP/bin/pngresize" PNGSCALE=/none sh "$AC"
check "same set: the missing copy stays missing (nothing done)" test ! -e "$TMP/ov/skin/miyoo-topbar.png"
sed -i '1s/.*/# set 0123456789ab/' "$TMP/res2/files.txt"
SDROOT="$TMP/sd" SYSDIR="$TMP/sys" ACCENT_RES="$TMP/res2" OVERRIDES="$TMP/ov" SYSTEM_JSON="$TMP/system.json" PNGRESIZE="$TMP/bin/pngresize" PNGSCALE=/none sh "$AC"
check "a new set: copied again for the same colour" cmp -s "$TMP/ov/skin/miyoo-topbar.png" "$RES/blue/skin/miyoo-topbar.png"
check "with the new id in the state" grep -q '|0123456789ab$' "$TMP/ov/.accent_files"
sed -i '1s/^/x/' "$TMP/ov/.accent_files"
ac
check "back to this set (a state from before the id): copied again" test "$(head -n 1 "$TMP/ov/.accent_files")" = "Silky RedOnion|blue||$files_id"
# a game-list row count other than 6: the theme's resized selection is matched
cp "$TMP/sd/Themes/Silky RedOnion/skin/bg-list-s.png" "$TMP/sd/Themes/Silky RedOnion/skin/bg-list-s_8.png"
ac
check "the resized selection gets a blue copy" test -f "$TMP/ov/skin/bg-list-s_8.png"
check "listed with the others" grep -q '^skin/bg-list-s_8.png$' "$TMP/ov/.accent_files"
check "the state notes the resized images" sh -c "head -n 1 '$TMP/ov/.accent_files' | grep -q 'bg-list-s_8.png:640x60'"
# same picture size: copied, not resized
check "a copy as large as the theme's is not resized" test ! -e "$TMP/resized"
printf '\211PNG\r\n\032\n\000\000\000\rIHDR\000\000\002\200\000\000\000\055' > "$TMP/sd/Themes/Silky RedOnion/skin/bg-list-s_8.png"
ac
check "the theme's smaller one: the copy is resized to its size" test "$(cat "$TMP/resized" 2> /dev/null)" = "640x45"
echo 3 > "$TMP/sys/config/display/accentColor"
ac
check "teal: blue's files replaced" cmp -s "$TMP/ov/skin/bg-list-s.png" "$RES/teal/skin/bg-list-s.png"
echo 0 > "$TMP/sys/config/display/accentColor"
ac
check "back to red: the copies removed" test ! -e "$TMP/ov/skin/bg-list-s.png"
check "the resized one too" test ! -e "$TMP/ov/skin/bg-list-s_8.png"
check "the user's own override still there" grep -q mine "$TMP/ov/skin/icon-A-54.png"
echo 2 > "$TMP/sys/config/display/accentColor"
ac
check "purple" cmp -s "$TMP/ov/skin/bg-list-s.png" "$RES/purple/skin/bg-list-s.png"
ac "$TMP/sd/Themes/Other/"
check "another theme (themeSwitcher's argument): the copies removed" test ! -e "$TMP/ov/skin/bg-list-s.png"
check "and red for the programs" test "$(cat "$TMP/sys/config/display/accentRGB")" = "9E2C45 D65670 782036"
silky_json "Silky RedOnion"
ac
check "Silky RedOnion again: purple again" cmp -s "$TMP/ov/skin/bg-list-s.png" "$RES/purple/skin/bg-list-s.png"
echo 9 > "$TMP/sys/config/display/accentColor"
ac
check "a value out of range: red" test ! -e "$TMP/ov/skin/bg-list-s.png"
printf 'skin/../../escape\n' >> "$TMP/ov/.accent_files"
touch "$TMP/escape"
echo 1 > "$TMP/sys/config/display/accentColor"
ac
check "a listed path out of the overrides is never removed" test -e "$TMP/escape"
# speed: the usual change starts only a handful of programs (it was one cp,
# od and mkdir per picture, 1-2 s in Tweaks): count them with stand-ins
mkdir -p "$TMP/count"
for c in cp rm mkdir mv rmdir sync od find sed head grep tr cat; do
    printf '#!/bin/sh\necho %s >> "%s/count/log"\nexec /bin/%s "$@" 2>/dev/null || exec /usr/bin/%s "$@"\n' "$c" "$TMP" "$c" "$c" > "$TMP/count/$c"
    chmod +x "$TMP/count/$c"
done
silky_json "Silky RedOnion"
rm -f "$TMP/sd/Themes/Silky RedOnion/skin/bg-list-s_8.png" "$TMP/ov/skin/icon-A-54.png" # the usual case: no resized list images, no own overrides
echo 4 > "$TMP/sys/config/display/accentColor"
rm -f "$TMP/count/log"
SDROOT="$TMP/sd" SYSDIR="$TMP/sys" ACCENT_RES="$RES" OVERRIDES="$TMP/ov" SYSTEM_JSON="$TMP/system.json" PNGRESIZE=/none PNGSCALE=/none PATH="$TMP/count:$PATH" sh "$AC"
check "a colour change starts at most 10 programs" test "$(wc -l < "$TMP/count/log")" -le 10
check "pink copied by it" cmp -s "$TMP/ov/skin/bg-list-s.png" "$RES/pink/skin/bg-list-s.png"
rm -f "$TMP/count/log"
SDROOT="$TMP/sd" SYSDIR="$TMP/sys" ACCENT_RES="$RES" OVERRIDES="$TMP/ov" SYSTEM_JSON="$TMP/system.json" PNGRESIZE=/none PNGSCALE=/none PATH="$TMP/count:$PATH" sh "$AC"
check "nothing changed (every MainUI start): no program at all" test ! -s "$TMP/count/log"
# The list icons resized once (Tweaks > Game lists): .bak originals stay,
# even back at 6 rows. Every picture went the slow way (one cp, two od and
# a mkdir each: seconds stuck at each change, beta 6). Now one cp for all,
# and only the resized one fitted.
cp "$ROOT/static/build/Themes/Silky RedOnion/skin/ic-favorite-mark.png" "$TMP/sd/Themes/Silky RedOnion/skin/ic-favorite-mark.png.bak"
cp "$TMP/sd/Themes/Silky RedOnion/skin/ic-favorite-mark.png.bak" "$TMP/sd/Themes/Silky RedOnion/skin/ic-favorite-mark.png"
echo 1 > "$TMP/sys/config/display/accentColor"
rm -f "$TMP/count/log" "$TMP/resized"
SDROOT="$TMP/sd" SYSDIR="$TMP/sys" ACCENT_RES="$RES" OVERRIDES="$TMP/ov" SYSTEM_JSON="$TMP/system.json" PNGRESIZE="$TMP/bin/pngresize" PNGSCALE=/none PATH="$TMP/count:$PATH" sh "$AC"
check "icons resized once, back at 6 rows (.bak left): still a handful of programs" test "$(wc -l < "$TMP/count/log")" -le 14
check "one cp for the pictures" test "$(grep -c '^cp$' "$TMP/count/log")" -eq 1
check "blue copied by it" cmp -s "$TMP/ov/skin/bg-list-s.png" "$RES/blue/skin/bg-list-s.png"
check "the icon as large as the theme's: copied, not resized" sh -c "cmp -s '$TMP/ov/skin/ic-favorite-mark.png' '$RES/blue/skin/ic-favorite-mark.png' && test ! -e '$TMP/resized'"
# the theme's icon smaller (rows other than 6): the copy fitted to it
printf '\211PNG\r\n\032\n\000\000\000\rIHDR\000\000\000\030\000\000\000\030' > "$TMP/sd/Themes/Silky RedOnion/skin/ic-favorite-mark.png"
echo 5 > "$TMP/sys/config/display/accentColor"
rm -f "$TMP/count/log" "$TMP/resized"
SDROOT="$TMP/sd" SYSDIR="$TMP/sys" ACCENT_RES="$RES" OVERRIDES="$TMP/ov" SYSTEM_JSON="$TMP/system.json" PNGRESIZE="$TMP/bin/pngresize" PNGSCALE=/none PATH="$TMP/count:$PATH" sh "$AC"
check "resized icon: the copy fitted to the theme's size" test "$(cat "$TMP/resized" 2> /dev/null)" = "24x24"
check "the others copied as they are (orange)" cmp -s "$TMP/ov/skin/bg-list-s.png" "$RES/orange/skin/bg-list-s.png"
check "still a handful of programs" test "$(wc -l < "$TMP/count/log")" -le 16
check "no .tmp left" test -z "$(find "$TMP/ov" -name '*.tmp')"
rm -f "$TMP/sd/Themes/Silky RedOnion/skin/ic-favorite-mark.png" "$TMP/sd/Themes/Silky RedOnion/skin/ic-favorite-mark.png.bak"
unset -f ac silky_json
# the colours and the pictures
check "palette: the saved numbers' order, new colours at the end" test "$(grep -v '^#' "$RES/palette.txt" | cut -d' ' -f1 | tr '\n' ' ')" = "red blue purple teal pink orange green gold graphite "
check "Tweaks: the colours in rainbow order" grep -q '.value_labels = {"Red", "Orange", "Gold", "Green", "Teal", "Blue", "Purple", "Pink", "Graphite"}' "$ROOT/src/tweaks/menus.h"
# Each row of Tweaks saves the number of the colour it names
order=$(sed -n 's/^static const int ACCENT_ORDER\[\] = {\(.*\)};$/\1/p' "$ROOT/src/tweaks/actions.h" | tr -d ' ')
labels=$(sed -n 's/.*\.value_labels = {\("Red", "Orange".*\)},$/\1/p' "$ROOT/src/tweaks/menus.h" | tr -d '" ' | tr 'A-Z' 'a-z')
palette=$(grep -v '^#' "$RES/palette.txt" | cut -d' ' -f1 | tr '\n' ' ')
i=1
for n in $(echo "$order" | tr ',' ' '); do
    label=$(echo "$labels" | cut -d, -f$i)
    name=$(echo "$palette" | cut -d' ' -f$((n + 1)))
    [ "$label" = "$name" ] || echo "row $i: $label saves $name" >> "$TMP/accent_map"
    i=$((i + 1))
done
check "every row saves the colour it names (red, orange, gold...)" test ! -e "$TMP/accent_map" -a "$i" -eq 10
check "a saved number the list does not have is red" grep -q "return 0;" "$ROOT/src/tweaks/actions.h"
# Stepping through the colours applied each one (eight in a row from Red to
# Graphite), then each one that stayed 500 ms: the list stuck while
# choosing (beta 6). Saved at once, applied on leaving its page or Tweaks.
check "Tweaks: the colour only saved while choosing" sh -c "sed -n '/^void action_setAccentColor/,/^}/p' '$ROOT/src/tweaks/actions.h' | grep -q 'accent_pending = true' && ! sed -n '/^void action_setAccentColor/,/^}/p' '$ROOT/src/tweaks/actions.h' | grep -q 'accent_color.sh'"
check "applied once its page is left" sh -c "sed -n '/^void accent_applyPending/,/^}/p' '$ROOT/src/tweaks/actions.h' | grep -q 'menu_stack\[menu_level\] == accent_pending_page'"
check "Tweaks: checked in its loop before all_changed is read, and on leaving" sh -c "awk '/accent_applyPending\(false\);/ { a = NR } /if \(all_changed\) \{/ && !b { b = NR } END { exit !(a && b && a < b) }' '$ROOT/src/tweaks/tweaks.c' && grep -q 'accent_applyPending(true);' '$ROOT/src/tweaks/tweaks.c'"
# White text on the selection: as readable as on the first six (4.5:1 or more)
grep -v '^#' "$RES/palette.txt" | while read -r n a l d; do
    awk -v h="$a" 'function lin(c) { c /= 255; return c <= 0.03928 ? c / 12.92 : ((c + 0.055) / 1.055) ^ 2.4 }
        function hx(s,  i, v) { v = 0; for (i = 1; i <= 2; i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1; return v }
        BEGIN { L = 0.2126 * lin(hx(substr(h, 1, 2))) + 0.7152 * lin(hx(substr(h, 3, 2))) + 0.0722 * lin(hx(substr(h, 5, 2)))
                exit (1.05 / (L + 0.05) >= 4.5 ? 0 : 1) }' || echo "$n" >> "$TMP/accent_dim"
done
check "white text reads on every colour's selection (4.5:1)" test ! -e "$TMP/accent_dim"
for c in blue purple teal pink orange green gold graphite; do
    [ -f "$RES/$c/skin/bg-list-s.png" ] && [ -f "$RES/$c/skin/icon-A-54.png" ] && [ -f "$RES/$c/skin/ic-game-f.png" ] || echo "$c" >> "$TMP/missing"
    [ "$(cd "$RES/$c" && find skin -type f | sort | cksum)" = "$(cd "$RES/blue" && find skin -type f | sort | cksum)" ] || echo "$c" >> "$TMP/differ"
done
check "every colour has the selection, the A button and the menu icons" test ! -e "$TMP/missing"
check "and the same files" test ! -e "$TMP/differ"
check "files.txt lists exactly the pictures of each colour" test "$(grep -v '^#' "$RES/files.txt" | sort | cksum)" = "$(cd "$RES/blue" && find skin -type f | sort | cksum)"
check "the top bars' logo is among them (MainUI and the GameSwitcher)" sh -c "grep -q '^skin/miyoo-topbar.png$' '$RES/files.txt' && grep -q '^skin/extra/gs-top-bar.png$' '$RES/files.txt'"
check "the full-screen brand art and the empty battery stay red" sh -c "! find '$RES' -name 'power-0%-icon.png' -o -name 'bootScreen.png' -o -name 'chargingState*' -o -name 'Screen_Off*' -o -name 'splash-saving.png' | grep -q ."
check "the boot applies it before MainUI" sh -c "extract_fn() { sed -n \"/^\$2() {/,/^}/p\" \"\$1\"; }; extract_fn '$ROOT/static/build/.tmp_update/runtime.sh' launch_main_ui | grep -q 'script/accent_color.sh'"
check "and before Kids Mode" sh -c "sed -n '/^launch_kids_mode() {/,/^}/p' '$ROOT/static/build/.tmp_update/runtime.sh' | grep -q 'script/accent_color.sh'"
check "the Theme Switcher applies it with the theme" grep -q 'accent_color.sh' "$ROOT/src/themeSwitcher/installTheme.h"
check "the programs without theme images read it" grep -q 'define CUI_ACCENT accent_rgb(ACCENT_MAIN)' "$ROOT/src/common/theme/render/canvas_ui.h"
check "with red when the file is missing" grep -q '{0x9E2C45, 0xD65670, 0x782036}' "$ROOT/src/common/theme/accent.h"
check "Battery Monitor's own pictures follow it (header badge, battery)" sh -c "grep -q 'recolourAccent(background);' '$ROOT/src/batteryMonitorUI/batteryMonitorUI.c' && grep -q 'recolourAccent(waiting_screen);' '$ROOT/src/batteryMonitorUI/batteryMonitorUI.c'"
check "the C recolouring uses the same red as the pictures' script" sh -c "grep -q 'RED = (\"9E2C45\"' '$ROOT/tools/accent/gen_accent.py' && grep -q 'red_h = 0.96345f, red_s = 0.72152f, red_v = 0.61961f' '$ROOT/src/common/theme/accent.h'"
end

begin installer_uses_the_consoles_brightness
INST="$ROOT/static/dist/miyoo/app/.tmp_update/install.sh"
eval "$(extract_fn "$INST" installer_settings_files | sed -e "s#/mnt/SDCARD#$TMP/sd#g" -e "s#/appconfigs#$TMP/app#g")"
INSTALLER_CARD_JSON="$TMP/sd/system.json" INSTALLER_STOCK_JSON="$TMP/app/system.json"
eval "$(extract_fn "$INST" installer_brightness)"
INSTALLER_SN_FILE="$TMP/deviceSN"
INSTALLER_SETTINGS_DIR="$TMP/sd/.tmp_update/config/system"
mkdir -p "$TMP/sd" "$TMP/app"
printf '{\n\t"vol":\t20,\n\t"brightness":\t3,\n\t"lumination":\t7\n}\n' > "$TMP/app/system.json"
check "brightness 3: the boot's level 9, not the fixed 80" test "$(installer_brightness)" = 9
for pair in 0:3 1:4 2:6 4:12 5:17 6:25 7:35 8:50 9:70 10:100; do
    printf '{\n\t"brightness":\t%s\n}\n' "${pair%%:*}" > "$TMP/app/system.json"
    [ "$(installer_brightness)" = "${pair#*:}" ] || echo "${pair%%:*} gave $(installer_brightness)" >> "$TMP/bad"
done
check "every level the same as runtime.sh's formula" test ! -e "$TMP/bad"
# the levels against the boot's own formula, so the two cannot drift apart
for b in 0 1 2 3 4 5 6 7 8 9 10; do
    printf '{"brightness": %s}\n' "$b" > "$TMP/app/system.json"
    [ "$(installer_brightness)" = "$(awk "BEGIN { print int(3 * exp(0.350656 * $b) + 0.5) }")" ] || echo "$b" >> "$TMP/bad2"
done
check "and as its awk line computes them" test ! -e "$TMP/bad2"
check "the formula is still runtime.sh's" grep -q 'int(3 \* exp(0.350656 \* \$brightness) + 0.5)' "$ROOT/static/build/.tmp_update/runtime.sh"
rm -f "$TMP/app/system.json"
printf '{\n\t"brightness":\t5\n}\n' > "$TMP/sd/system.json"
check "no /appconfigs copy: the card's system.json" test "$(installer_brightness)" = 17
printf '{\n\t"brightness":\t12\n}\n' > "$TMP/app/system.json"
check "a value out of range is skipped for the next file" test "$(installer_brightness)" = 17
rm -f "$TMP/sd/system.json"
check "and with no valid value at all: 80, as before" test "$(installer_brightness)" = 80
rm -f "$TMP/app/system.json"
check "no file: 80" test "$(installer_brightness)" = 80
# An update: after a clean shutdown the settings are only in the console's
# saved file (the card's system.json moved there, the /appconfigs link gone)
mkdir -p "$TMP/sd/.tmp_update/config/system"
printf '{\n\t"brightness":\t3\n}\n' > "$TMP/sd/.tmp_update/config/system/ABC123.json"
printf 'ABC123' > "$TMP/deviceSN"
check "update after a shutdown: the console's saved settings (brightness 3)" test "$(installer_brightness)" = 9
printf '{\n\t"brightness":\t9\n}\n' > "$TMP/sd/.tmp_update/config/system/OTHER9.json"
check "another console's settings on the card are not used" test "$(installer_brightness)" = 9
printf '{\n\t"brightness":\t5\n}\n' > "$TMP/app/system.json"
check "the console's saved settings come before a stale /appconfigs file" test "$(installer_brightness)" = 9
rm -f "$TMP/app/system.json" "$TMP/deviceSN"
check "no serial, two consoles' files: 80" test "$(installer_brightness)" = 80
rm -f "$TMP/sd/.tmp_update/config/system/OTHER9.json"
check "no serial, one console's file: that one" test "$(installer_brightness)" = 9
printf '../../etc' > "$TMP/deviceSN"
rm -f "$TMP/sd/.tmp_update/config/system/ABC123.json"
check "a serial with a path in it is not followed" test "$(installer_brightness)" = 80
# An OTA update reboots plainly: the card's system.json is still there, the
# live settings; the saved ones are from the shutdown before
printf 'ABC123' > "$TMP/deviceSN"
printf '{\n\t"brightness":\t3\n}\n' > "$TMP/sd/.tmp_update/config/system/ABC123.json"
printf '{\n\t"brightness":\t10\n}\n' > "$TMP/sd/system.json"
check "OTA update (no shutdown): the card's live settings, not the older saved ones" test "$(installer_brightness)" = 100
rm -f "$TMP/sd/system.json" "$TMP/sd/.tmp_update/config/system/ABC123.json"
rm -f "$TMP/deviceSN"
unset INSTALLER_SN_FILE INSTALLER_SETTINGS_DIR INSTALLER_CARD_JSON INSTALLER_STOCK_JSON
check "the serial comes from runtime.sh's /tmp/deviceSN, written before check_installer" sh -c "grep -n 'echo -n \"\$SERIAL_NUMBER\" > /tmp/deviceSN\|^    check_installer\$' '$ROOT/static/build/.tmp_update/runtime.sh' | head -2 | tail -1 | grep -q check_installer"
check "the shutdown moves the card's system.json away (why it is read from config/system)" grep -q 'mv -f /mnt/SDCARD/system.json' "$ROOT/static/build/.tmp_update/bin/shutdown"
unset -f installer_brightness
check "the card's settings are looked at before main runs (nothing has saved them yet)" sh -c "grep -n '^card_settings_live=0\$\|^main\$' '$INST' | head -1 | grep -q card_settings_live"
check "the OTA update reboots plainly (why the card's file can be the live one)" grep -q '^			reboot$' "$ROOT/static/build/.tmp_update/script/ota_update.sh"
check "main sets the backlight with it" sh -c "extract_fn() { sed -n \"/^\$2() {/,/^}/p\" \"\$1\"; }; extract_fn '$INST' main | grep -q '^    installer_display main\$' && extract_fn '$INST' installer_display | grep -q '_duty=\$(installer_brightness)'"
# The installer's MainUI removes the old system first, seconds shown at the
# firmware's start-up brightness (beta 6): install.sh --display before it.
mkdir -p "$TMP/pwm/pwm0" "$TMP/cfg" "$TMP/sys/config/system"
echo 0123SN > "$TMP/sn"
printf '{\n\t"brightness":\t1,\n\t"lumination":\t5\n}\n' > "$TMP/sys/config/system/0123SN.json"
: > "$TMP/disp"
INSTALLER_PWM_DIR="$TMP/pwm" INSTALLER_DISP_NODE="$TMP/disp" INSTALLER_DISPLAY_LOG="$TMP/cfg/.installer_display.log" \
    INSTALLER_SN_FILE="$TMP/sn" INSTALLER_SETTINGS_DIR="$TMP/sys/config/system" \
    INSTALLER_CARD_JSON="$TMP/none.json" INSTALLER_STOCK_JSON="$TMP/none.json" sh "$INST" --display > "$TMP/display_out" 2>&1
check "--display: the console's brightness 1 (duty 4)" test "$(cat "$TMP/pwm/pwm0/duty_cycle" 2> /dev/null)" = 4
check "--display: the backlight on, period 800" test "$(cat "$TMP/pwm/pwm0/enable" 2> /dev/null)/$(cat "$TMP/pwm/pwm0/period" 2> /dev/null)" = 1/800
check "--display: its display colours too" grep -q '^csc 0 3 50 50 40 50 0 0$' "$TMP/disp"
check "--display: a log line saying from which file" grep -q "^pre: .*settings $TMP/sys/config/system/0123SN.json, duty 4 (now 4, period 800)" "$TMP/cfg/.installer_display.log"
check "--display: nothing else done (no installation)" test ! -s "$TMP/display_out"
check "the installer's MainUI calls it before removing the old system" sh -c "awk '/install.sh\" --display/ { a = NR } /-exec rm -rf/ { b = NR } END { exit !(a && b && a < b) }' '$ROOT/static/dist/miyoo/app/MainUI'"
check "no fixed 80 written to the backlight" sh -c "! grep -q 'echo 80 > \$pwmdir' '$INST'"
end

begin installer_uses_the_consoles_display_colours
# Settings › Display's colours during the installation, from the same file
# as the brightness: the panel's own colours looked brighter than the
# console's with a lowered lumination or contrast (robcodedev, beta 6)
INST="$ROOT/static/dist/miyoo/app/.tmp_update/install.sh"
eval "$(extract_fn "$INST" installer_settings_files | sed -e "s#/mnt/SDCARD#$TMP/sd#g" -e "s#/appconfigs#$TMP/app#g")"
INSTALLER_CARD_JSON="$TMP/sd/system.json" INSTALLER_STOCK_JSON="$TMP/app/system.json"
eval "$(extract_fn "$INST" installer_display_colours)"
INSTALLER_SN_FILE="$TMP/deviceSN" INSTALLER_SETTINGS_DIR="$TMP/sd/.tmp_update/config/system"
INSTALLER_DISP_NODE="$TMP/mi_disp0" MODEL_MM=283 DEVICE_ID=354
mkdir -p "$TMP/sd" "$TMP/app"
: > "$TMP/mi_disp0"
printf '{\n\t"brightness":\t7,\n\t"lumination":\t4,\n\t"hue":\t10,\n\t"saturation":\t12,\n\t"contrast":\t6\n}\n' > "$TMP/sd/system.json"
installer_display_colours
# runtime.sh: csc 0 3 (c+15)*2 h*5 l+35 s*5 0 0
check "the console's colours, as runtime.sh writes them" test "$(cat "$TMP/mi_disp0")" = "csc 0 3 42 50 39 60 0 0"
rm -f "$TMP/sd/system.json"
printf '{\n\t"brightness":\t7\n}\n' > "$TMP/app/system.json"
installer_display_colours
check "none saved: the defaults (Mini+ / Flip lumination 10)" test "$(cat "$TMP/mi_disp0")" = "csc 0 3 50 50 45 50 0 0"
DEVICE_ID=283
installer_display_colours
check "none saved on the Mini: its lumination 7" test "$(cat "$TMP/mi_disp0")" = "csc 0 3 50 50 42 50 0 0"
printf '{\n\t"lumination":\t99,\n\t"contrast":\t3\n}\n' > "$TMP/app/system.json"
installer_display_colours
check "a value out of range: its default" test "$(cat "$TMP/mi_disp0")" = "csc 0 3 36 50 42 50 0 0"
rm -f "$TMP/mi_disp0"
installer_display_colours
check "no display node: nothing written" test ! -e "$TMP/mi_disp0"
check "same formula as runtime.sh" grep -qF 'echo "csc 0 3 $(((_c + 15) * 2)) $((_h * 5)) $((_l + 35)) $((_s * 5)) 0 0"' "$ROOT/static/build/.tmp_update/runtime.sh"
check "set right after the backlight" sh -c "sed -n '/^installer_display() {/,/^}/p' '$INST' | grep -A1 'pwm0/enable\$' | grep -q '^    installer_display_colours\$'"
rm -f "$TMP/app/system.json"
unset -f installer_settings_files installer_display_colours
unset INSTALLER_SN_FILE INSTALLER_SETTINGS_DIR INSTALLER_DISP_NODE MODEL_MM DEVICE_ID INSTALLER_CARD_JSON INSTALLER_STOCK_JSON
end

begin shutdown_survives_its_terminal_and_has_a_fallback
# bin/shutdown run on stand-ins: its paths moved into $TMP, the commands it
# calls replaced by stubs that log what they were asked.
SHUTDOWN="$ROOT/static/build/.tmp_update/bin/shutdown"
mkdir -p "$TMP/stub" "$TMP/sd/.tmp_update/config" "$TMP/app"
echo 354 > "$TMP/deviceModel"
for c in fuser sync umount swapoff mv mkdir rm read_uuid reboot poweroff; do
    printf '#!/bin/sh\necho "%s $*" >> "%s/log"\n' "$c" "$TMP" > "$TMP/stub/$c"
done
# killall: the first call hangs up the script, as closing its terminal
# does; the second sends it SIGTERM, as keymon does to what it stops
cat > "$TMP/stub/killall" << EOF
#!/bin/sh
echo "killall \$*" >> "$TMP/log"
n=\$(grep -c '^killall' "$TMP/log")
[ \$n -eq 1 ] && kill -HUP \$PPID
[ \$n -eq 2 ] && kill -TERM \$PPID
exit 0
EOF
# sleep: returns at once, and ends the script at the endless wait
cat > "$TMP/stub/sleep" << EOF
#!/bin/sh
echo "sleep \$*" >> "$TMP/log"
[ "\$1" = 10 ] && kill -9 \$PPID
exit 0
EOF
# pgrep: is a second stage running? (the answer in \$TMP/second_stage)
printf '#!/bin/sh\necho "pgrep $*" >> "%s/log"\n[ -f "%s/second_stage" ]\n' "$TMP" "$TMP" > "$TMP/stub/pgrep"
# su: what it would start, and with which LD_PRELOAD
printf '#!/bin/sh\necho "su LD_PRELOAD=${LD_PRELOAD:-none}" >> "%s/log"\n' "$TMP" > "$TMP/stub/su"
chmod +x "$TMP"/stub/*
sed -e "s#/tmp/_shutdown#$TMP/_shutdown#g" -e "s#/tmp/deviceModel#$TMP/deviceModel#g" \
    -e "s#/mnt/SDCARD#$TMP/sd#g" -e "s#/appconfigs#$TMP/app#g" -e "s#/sbin/#$TMP/stub/#g" \
    -e "s#/usr/bin/nohup#nohup#g" "$SHUTDOWN" > "$TMP/_shutdown"
cp "$TMP/_shutdown" "$TMP/shutdown"
shutdown_run() { # the script's name, its argument -> what it did
    rm -f "$TMP/log"
    if [ -n "$shutdown_preload" ]; then
        PATH="$TMP/stub:$PATH" LD_PRELOAD="$shutdown_preload" sh "$TMP/$1" $2 > /dev/null 2>&1
    else
        PATH="$TMP/stub:$PATH" sh "$TMP/$1" $2 > /dev/null 2>&1
    fi
    grep -E '^(killall -9 main|killall -9 smbd|umount -r|reboot|poweroff|sleep 30|su )' "$TMP/log" | tr '\n' ','
}
# The second stage: hung up and sent SIGTERM, it still unmounts and reboots
check "second stage: SIGHUP and SIGTERM do not stop it" \
    test "$(shutdown_run _shutdown -r | cut -d, -f1-4)" = "killall -9 main,killall -9 smbd,umount -r $TMP/sd,reboot "
check "second stage, power off on a Plus: poweroff" \
    sh -c "printf '%s' \"\$1\" | grep -q 'umount -r [^,]*,poweroff ,'" _ "$(shutdown_run _shutdown)"
# The first stage: starts the second without LD_PRELOAD; if that one never
# gets to kill it, it reboots by itself after 30 s
rm -f "$TMP/second_stage"
shutdown_preload=/nonexistent/libpadsp.so
out=$(shutdown_run shutdown -r)
shutdown_preload=
check "first stage: the second stage is started without LD_PRELOAD" \
    sh -c "printf '%s' \"\$1\" | grep -q '^su LD_PRELOAD=none,'" _ "$out"
check "first stage, the second stage killed early: fallback reboot after 30 s" \
    test "$out" = "su LD_PRELOAD=none,sleep 30,reboot ,"
touch "$TMP/second_stage"
check "first stage, second stage running: not started again, left to finish" \
    test "$(shutdown_run shutdown -r)" = "sleep 30,"
rm -f "$TMP/second_stage"
check "first stage, power off on a Plus: fallback poweroff" \
    test "$(shutdown_run shutdown)" = "su LD_PRELOAD=none,sleep 30,poweroff ,"
unset -f shutdown_run
end

# ---- migration 00021: Apps sorted by name once, as before Open MainUI 1.0.6 ----

begin migration_sorts_apps_by_name
sysdir=$TMP sh "$ROOT/static/build/.tmp_update/script/migration/00021_sort_apps_by_name.sh"
check ".appsort created" test -f "$TMP/config/.appsort"
check "not shipped in the default configs (an update would bring it back)" \
    test ! -e "$ROOT/static/configs/.tmp_update/config/.appsort"
end

# ---- "Update available!" goes away once the update is installed ----

INSTALLER="$ROOT/static/dist/miyoo/app/.tmp_update/install.sh"

# ---- installer: POWER at the prompt keeps the installer ----

begin installer_prompt_cancel_keeps_the_installer
(
    eval "$(extract_fn "$INSTALLER" prompt_update)"
    # prompt: the answers queued in $TMP/answers, one per call (255 = POWER)
    prompt() {
        _a=$(sed -n 1p "$TMP/answers")
        sed -i 1d "$TMP/answers"
        return "$_a"
    }
    run_installation() { echo "$1 $2" >> "$TMP/installed"; }
    ask() { # answers... -> prints prompt_update's status and what ran
        printf '%s\n' "$@" > "$TMP/answers"
        rm -f "$TMP/installed"
        prompt_update
        echo "$? $(cat "$TMP/installed" 2> /dev/null)"
    }
    ask 255 > "$TMP/r_power"
    ask 0 > "$TMP/r_update"
    ask 2 > "$TMP/r_system"
    ask 1 0 > "$TMP/r_reinstall"
    ask 1 1 255 > "$TMP/r_back_then_power"
    ask 1 1 0 > "$TMP/r_back_then_update"
)
check "POWER: status 1, nothing installed" test "$(cat "$TMP/r_power")" = "1 "
check "Update: status 0" test "$(cat "$TMP/r_update")" = "0 0 0"
check "OS/RetroArch only: status 0" test "$(cat "$TMP/r_system")" = "0 0 1"
check "Reinstall confirmed: status 0" test "$(cat "$TMP/r_reinstall")" = "0 1 0"
check "Reinstall cancelled, then POWER: status 1" test "$(cat "$TMP/r_back_then_power")" = "1 "
check "Reinstall cancelled, then Update: status 0" test "$(cat "$TMP/r_back_then_update")" = "0 0 0"
# main() needs the device (framebuffer, PWM): its MENU branch must clean up
# only after an installation
check "main cleans up only when prompt_update installed something" \
    sh -c "sed -n '/^main() {/,/^}/p' \"$INSTALLER\" | grep -q '^        prompt_update && cleanup\$'"
end

# ---- installer: the extraction's status, read once 7z has ended ----

begin installer_extraction_status_waits_for_7z
unzip_run() { # what the stand-in 7z does -> result
    rm -rf "$TMP/t"
    mkdir -p "$TMP/t/stub" "$TMP/t/sys"
    touch "$TMP/t/pkg.pak"
    case "$1" in
        ok) printf '#!/bin/sh\necho " 50%%"\nsleep 0.2\necho " 99%%"\nexit 0\n' ;;
        fails) printf '#!/bin/sh\necho " 10%%"\nexit 2\n' ;;
        # the shell that waits for 7z ends before it can write the status
        no_status) printf '#!/bin/sh\nkill -9 $PPID\nexit 0\n' ;;
    esac > "$TMP/t/stub/7z"
    chmod +x "$TMP/t/stub/7z"
    (
        eval "$(extract_fn "$INSTALLER" unzip_progress | sed "s#/tmp/#$TMP/t/#g")"
        verify_file() { :; }
        installer_reboot() { echo reboot > "$TMP/t/rebooted"; }
        sysdir=$TMP/t/sys
        PATH="$TMP/t/stub:$PATH"
        unzip_progress "$TMP/t/pkg.pak" "1/1: Updating RedOnion..." /dest > /dev/null 2>&1
        echo returned > "$TMP/t/returned"
    )
    r=""
    [ -f "$TMP/t/returned" ] && r="${r}returned "
    [ -f "$TMP/t/sys/.installFailed" ] && r="${r}failed "
    [ -f "$TMP/t/rebooted" ] && r="${r}reboot "
    grep -q '100%' "$TMP/t/.update_msg" 2> /dev/null && r="${r}100% "
    echo "$r"
}
check "7z succeeds: 100%, the installation goes on" test "$(unzip_run ok)" = "returned 100% "
check "7z fails: installation failed, reboot" test "$(unzip_run fails)" = "failed reboot "
check "no status written: a failure, not a success" test "$(unzip_run no_status)" = "failed reboot "
unset -f unzip_run
end

# ---- installer: the migrations leave a "temp" at the card's root alone ----

begin installer_migrations_keep_a_temp_folder
(
    eval "$(extract_fn "$INSTALLER" run_migration_scripts | sed "s#/tmp/#$TMP/tmp/#g")"
    sysdir=$TMP/sys
    mkdir -p "$sysdir/script/migration" "$sysdir/config" "$TMP/tmp" "$TMP/root/temp"
    echo photo > "$TMP/root/temp/keep.txt"
    for id in 00002 00001; do
        printf '#!/bin/sh\necho %s >> "%s/ran"\n' "$id" "$TMP" > "$sysdir/script/migration/${id}_test.sh"
    done
    cd "$TMP/root"   # install_configs leaves the card's root as the working directory
    run_migration_scripts > /dev/null 2>&1
)
check "the user's temp folder is still there" test -f "$TMP/root/temp/keep.txt"
check "both migrations ran, in order" test "$(cat "$TMP/ran" 2> /dev/null | tr '\n' ' ')" = "00001 00002 "
check "the state is the last one" test "$(cat "$TMP/sys/config/migration_state" 2> /dev/null)" = 00002
check "no list left in /tmp" test -z "$(ls "$TMP/tmp")"
end

# ---- an update made with Guest Mode on keeps the guest's profile ----

begin installer_configs_keep_guest_mode
(
    eval "$(extract_fn "$INSTALLER" install_configs | sed "s#/mnt/SDCARD#$TMP/sd#g")"
    sysdir=$TMP/sd/.tmp_update
    mkdir -p "$sysdir/config" "$TMP/sd/Saves/CurrentProfile/config" "$TMP/stub"
    touch "$sysdir/config/configs.pak"
    echo '{}' > "$sysdir/config/keymap.json"
    printf '#!/bin/sh\necho "$*" >> "%s/7z_args"\n' "$TMP" > "$TMP/stub/7z"
    chmod +x "$TMP/stub/7z"
    PATH="$TMP/stub:$PATH"
    cd "$TMP"
    install_configs 0 > /dev/null 2>&1   # Guest Mode off
    install_configs 1 > /dev/null 2>&1
    mkdir -p "$TMP/sd/Saves/MainProfile" # Guest Mode on
    install_configs 0 > /dev/null 2>&1
    install_configs 1 > /dev/null 2>&1
)
check "Guest Mode off: everything extracted (update)" test "$(sed -n 1p "$TMP/7z_args")" = "x -aos $TMP/sd/.tmp_update/config/configs.pak"
check "Guest Mode off: everything extracted (reinstall)" test "$(sed -n 2p "$TMP/7z_args")" = "x -aoa $TMP/sd/.tmp_update/config/configs.pak"
check "Guest Mode on: no GuestProfile (update)" test "$(sed -n 3p "$TMP/7z_args")" = "x -aos $TMP/sd/.tmp_update/config/configs.pak -x!Saves/GuestProfile"
check "Guest Mode on: no GuestProfile (reinstall)" test "$(sed -n 4p "$TMP/7z_args")" = "x -aoa $TMP/sd/.tmp_update/config/configs.pak -x!Saves/GuestProfile"
end

begin installer_keeps_the_x_and_y_choice
# "B button" for X and "A button" for Y are stored as "" by Tweaks: an
# update set them back to Search and GLO
(
    eval "$(extract_fn "$INSTALLER" install_configs | sed "s#/mnt/SDCARD#$TMP/sd#g")"
    sysdir=$TMP/sd/.tmp_update
    mkdir -p "$sysdir/config" "$TMP/stub"
    touch "$sysdir/config/configs.pak"
    printf '{\n    "mainui_button_x": "",\n    "mainui_button_y": ""\n}\n' > "$sysdir/config/keymap.json"
    printf '#!/bin/sh\n:\n' > "$TMP/stub/7z"
    chmod +x "$TMP/stub/7z"
    PATH="$TMP/stub:$PATH"
    cd "$TMP"
    install_configs 0 > /dev/null 2>&1
)
check "X stays on B button" grep -q '"mainui_button_x": ""' "$TMP/sd/.tmp_update/config/keymap.json"
check "Y stays on A button" grep -q '"mainui_button_y": ""' "$TMP/sd/.tmp_update/config/keymap.json"
check "nothing left at the card's root" test ! -e "$TMP/temp_keymap.json"
check "a fresh install gets Search and GLO (default keymap.json)" \
    grep -q '"mainui_button_x": "app:Search"' "$ROOT/static/configs/.tmp_update/config/keymap.json"
end

begin guest_mode_switch_decides_by_the_parked_main_profile
GUEST_APP="$ROOT/static/packages/App/Guest Mode/App/Guest_Mode"
mkdir -p "$TMP/stub"
# jq: -r prints the theme; otherwise the file unchanged
printf '#!/bin/sh\nif [ "$1" = -r ]; then echo "%s/sd/Themes/T/"; else for a; do :; done; cat "$a"; fi\n' "$TMP" > "$TMP/stub/jq"
printf '#!/bin/sh\necho "infoPanel $*" >> "%s/log"\n' "$TMP" > "$TMP/stub/infoPanel"
printf '#!/bin/sh\n:\n' > "$TMP/stub/themeSwitcher"
chmod +x "$TMP"/stub/*
guest_switch() { # the profiles before (folder=who, ...) -> after, and the status
    rm -rf "$TMP/sd" "$TMP/log"
    mkdir -p "$TMP/sd/App/Guest_Mode/data" "$TMP/sd/Roms" "$TMP/sd/Themes/T"
    for f in launch.sh data/configON.json data/configOFF.json; do
        sed "s#/mnt/SDCARD#$TMP/sd#g; s#/tmp/rtc_available#$TMP/rtc#g" "$GUEST_APP/$f" > "$TMP/sd/App/Guest_Mode/$f"
    done
    touch "$TMP/rtc"
    echo '{"theme":"x"}' > "$TMP/sd/system.json"
    for p; do
        mkdir -p "$TMP/sd/Saves/${p%=*}/lists" "$TMP/sd/Saves/${p%=*}/theme"
        echo "${p#*=}" > "$TMP/sd/Saves/${p%=*}/who"
    done
    (cd "$TMP/sd/App/Guest_Mode" && PATH="$TMP/stub:$PATH" sh ./launch.sh > /dev/null 2>&1)
    rc=$?
    for d in "$TMP"/sd/Saves/*; do
        [ -d "$d" ] || continue
        w=$(cat "$d/who" 2> /dev/null)
        [ -d "$d/CurrentProfile" ] && w="$w+inside:$(cat "$d/CurrentProfile/who")"
        n=${d##*/}
        case "$n" in GuestProfile-unused-*) n=GuestProfile-unused ;; esac
        echo "$n=$w"
    done | LC_ALL=C sort | tr '\n' ','
    echo "rc=$rc"
}
check "main on: the guest's profile becomes current" \
    test "$(guest_switch CurrentProfile=main GuestProfile=guest)" = "CurrentProfile=guest,MainProfile=main,rc=0"
check "guest on: the owner's profile comes back" \
    test "$(guest_switch CurrentProfile=guest MainProfile=main)" = "CurrentProfile=main,GuestProfile=guest,rc=0"
check "guest on, an empty GuestProfile left by an update: kept aside, nothing nested" \
    test "$(guest_switch CurrentProfile=guest MainProfile=main GuestProfile=empty)" = "CurrentProfile=main,GuestProfile-unused=empty,GuestProfile=guest,rc=0"
check "no guest profile at all: nothing changed" \
    test "$(guest_switch CurrentProfile=main)" = "CurrentProfile=main,rc=1"
check "no guest profile at all: a message" grep -q '^infoPanel --title Guest Mode' "$TMP/log"
unset -f guest_switch
end

begin update_flag_cleared_after_update
check "installer removes .updateAvailable" \
    sh -c "sed -n '/^run_installation() {/,/^}/p' \"$INSTALLER\" | grep -q 'rm -f /mnt/SDCARD/.tmp_update/.updateAvailable'"
check "OTA check removes it when up to date" \
    sh -c "sed -n '/^main() {/,/^}/p' \"$OTA\" | grep -c 'rm -f \"\$sysdir/.updateAvailable\"' | grep -q '^2$'"
# Run the real check mode: up to date drops the flag, newer release sets it.
eval "$(extract_fn "$OTA" main)"
ip() { echo "1.0.0.0 via 10.0.0.1 dev wlan0 src 10.0.0.2"; }
sysdir=$TMP
touch "$TMP/.updateAvailable"
get_release_info() { return 1; }
(cmd=check; main) > /dev/null 2>&1
check "boot check, up to date: flag removed" test ! -e "$TMP/.updateAvailable"
get_release_info() { return 0; }
(cmd=check; main) > /dev/null 2>&1
check "boot check, newer release: flag set" test -e "$TMP/.updateAvailable"
unset -f ip get_release_info main
end

# ---- cue_gen.sh: one CUE per game, next to its files (#264) ----

CUEGEN="$ROOT/static/build/.tmp_update/script/cue_gen.sh"

begin cue_gen_writes_valid_cues
mkdir -p "$TMP/PS/Alpha Game" "$TMP/PS/Beta Game" "$TMP/SEGACD"
for f in "PS/Flat Game (Track 1).bin" "PS/Flat Game (Track 2).bin" "PS/Flat Game (Track 10).bin" \
    "PS/Alpha Game/Alpha Game.bin" "PS/Beta Game/Beta Game (Track 01).bin" \
    "PS/Beta Game/Beta Game (Track 02).bin" "PS/Hack [T-En] (Track 1).bin" \
    "PS/Hack [T-En] (Track 2).bin" "PS/Novastorm (USA) (Disc 1).bin" \
    "PS/Novastorm (USA) (Disc 2).bin" "SEGACD/Kept (USA) (Track 1).bin" \
    "PS/Region (Track 01) (Japan).bin" "PS/Region (Track 02) (Japan).bin" \
    "PS/Region (Track 01) (USA).bin" "PS/Region (Track 02) (USA).bin"; do
    : > "$TMP/$f"
done
echo ORIGINAL > "$TMP/SEGACD/Kept (USA).cue"
rootdir=$TMP sh "$CUEGEN" "PS SEGACD" > "$TMP/cue_gen.log" 2>&1
cues=$(cd "$TMP" && find PS SEGACD -name '*.cue' | sort | tr '\n' '|')
check "one CUE per game, in the game's folder" test "$cues" = \
    "PS/Alpha Game/Alpha Game.cue|PS/Beta Game/Beta Game.cue|PS/Flat Game.cue|PS/Hack [T-En].cue|PS/Novastorm (USA) (Disc 1).cue|PS/Novastorm (USA) (Disc 2).cue|PS/Region (Japan).cue|PS/Region (USA).cue|SEGACD/Kept (USA).cue|"
check "FILE is the bare file name" grep -q '^FILE "Beta Game (Track 02).bin" BINARY$' "$TMP/PS/Beta Game/Beta Game.cue"
check "no folder in any FILE" test -z "$(grep -h '^FILE "[^"]*/' "$TMP"/PS/*.cue "$TMP"/PS/*/*.cue)"
check "tracks in numeric order (10 after 2)" test "$(grep -o 'TRACK [0-9]*' "$TMP/PS/Flat Game.cue" | tr '\n' ' ')" = "TRACK 01 TRACK 02 TRACK 10 "
check "track 1 is data, the others audio" test "$(grep -o 'TRACK [0-9]* [A-Z0-9/]*' "$TMP/PS/Flat Game.cue" | tr '\n' ' ')" = "TRACK 01 MODE1/2352 TRACK 02 AUDIO TRACK 10 AUDIO "
check "an audio track has its 2 s pregap" test "$(grep -c '^        INDEX 01 00:02:00$' "$TMP/PS/Flat Game.cue")" -eq 2
check "square brackets keep track 1" grep -q '^FILE "Hack \[T-En\] (Track 1).bin" BINARY$' "$TMP/PS/Hack [T-En].cue"
check "each disc gets its own CUE" test "$(grep -c '^FILE' "$TMP/PS/Novastorm (USA) (Disc 2).cue")" -eq 1
check "existing CUE left untouched" test "$(cat "$TMP/SEGACD/Kept (USA).cue")" = ORIGINAL
check "no CUE without a name" test -z "$(find "$TMP" -name '.cue')"
# Region after the track: the four files sort Japan 1, USA 1, Japan 2,
# USA 2 (review of #270 by @yuruyang).
check "region after the track: both tracks in each CUE" test \
    "$(grep -c '^FILE "Region (Track 0[12]) (Japan).bin"' "$TMP/PS/Region (Japan).cue")-$(grep -c '^FILE "Region (Track 0[12]) (USA).bin"' "$TMP/PS/Region (USA).cue")" = 2-2
check "region after the track: nothing skipped" test -z "$(grep '^SKIP' "$TMP/cue_gen.log" | grep -v 'Kept (USA)')"
check "totals reported" grep -q '^8 cue files created$' "$TMP/cue_gen.log"
end

# ---- runtime.sh: LOADING/SAVING screen per Tweaks setting ----

eval "$(extract_fn "$RUNTIME" show_launch_screen)"

begin launch_screen_follows_tweaks_setting
sysdir=$TMP
mkdir -p "$TMP/config/display"
infoPanel() { echo "infoPanel $*" >> "$TMP/calls"; }
bootScreen() { echo "bootScreen $*" >> "$TMP/calls"; }
touch() { echo "touch $*" >> "$TMP/calls"; }
show_launch_screen LOADING; wait
check "no setting: popup with the message, as in Onion" has_line "$TMP/calls" 'infoPanel --message LOADING --persistent --romscreen'
rm -f "$TMP/calls"
echo 1 > "$TMP/config/display/launchScreen"
show_launch_screen SAVING; wait
check "full screen: screenshot without message" has_line "$TMP/calls" 'infoPanel --persistent --romscreen'
check "full screen: dismissed like the popup" has_line "$TMP/calls" 'touch /tmp/dismiss_info_panel'
rm -f "$TMP/calls"
echo 2 > "$TMP/config/display/launchScreen"
show_launch_screen LOADING; wait
check "black: screen cleared, no infoPanel" test "$(cat "$TMP/calls")" = "bootScreen clear"
rm -f "$TMP/calls"
echo 3 > "$TMP/config/display/launchScreen"
show_launch_screen SAVING; wait
check "full popup: message without screenshot" has_line "$TMP/calls" 'infoPanel --message SAVING --persistent'
check "full popup: dismissed like the popup" has_line "$TMP/calls" 'touch /tmp/dismiss_info_panel'
# Saved Games: the GameSwitcher screenshot is not the state being loaded.
rm -f "$TMP/calls"
rm -f "$TMP/config/display/launchScreen"
show_launch_screen LOADING no_screenshot; wait
check "Saved Games, no setting: message without screenshot" test "$(grep -c infoPanel "$TMP/calls")-$(grep -c 'infoPanel --message LOADING --persistent$' "$TMP/calls")" = 1-1
rm -f "$TMP/calls"
echo 0 > "$TMP/config/display/launchScreen"
show_launch_screen LOADING no_screenshot; wait
check "Saved Games, screenshot + message: message only" has_line "$TMP/calls" 'infoPanel --message LOADING --persistent'
rm -f "$TMP/calls"
echo 1 > "$TMP/config/display/launchScreen"
show_launch_screen LOADING no_screenshot; wait
check "Saved Games, screenshot only: black screen" test "$(cat "$TMP/calls")" = "bootScreen clear"
rm -f "$TMP/calls"
echo 2 > "$TMP/config/display/launchScreen"
show_launch_screen LOADING no_screenshot; wait
check "Saved Games, black: still black" test "$(cat "$TMP/calls")" = "bootScreen clear"
rm -f "$TMP/calls"
echo 3 > "$TMP/config/display/launchScreen"
show_launch_screen LOADING no_screenshot; wait
check "Saved Games, message only: unchanged" has_line "$TMP/calls" 'infoPanel --message LOADING --persistent'
rm -f "$TMP/calls"
echo 0 > "$TMP/config/display/launchScreen"
show_launch_screen LOADING ""; wait
check "empty second argument: screenshot as before" has_line "$TMP/calls" 'infoPanel --message LOADING --persistent --romscreen'
check "LOADING passes the Saved Games choice" grep -q 'show_launch_screen "LOADING" "$launch_screen_shot"' "$RUNTIME"
check "SAVING keeps the screenshot" grep -q '^[[:space:]]*show_launch_screen "SAVING"$' "$RUNTIME"
check "Saved Games: flag from MainUI, not keymon's resume" grep -q '\[ -f /tmp/force_auto_load_state \] && \[ ! -f /tmp/resume_last_game \]' "$RUNTIME"
check "Saved Games: read before the core override removes the flag" test \
    "$(grep -n 'launch_screen_shot="no_screenshot"' "$RUNTIME" | cut -d: -f1)" -lt "$(grep -n 'override_game_core "$romcfgpath"' "$RUNTIME" | cut -d: -f1)"
check "keymon's resume marks itself" grep -q 'temp_flag_set("resume_last_game", true)' "$ROOT/src/common/system/state.h"
check "both screens use it" test "$(grep -c 'show_launch_screen "\(LOADING\|SAVING\)"' "$RUNTIME")" -eq 2
check "no direct LOADING/SAVING infoPanel left" sh -c "! grep -q 'infoPanel --message \"\(LOADING\|SAVING\)\"' \"$RUNTIME\""
unset -f infoPanel bootScreen touch show_launch_screen
end

# ---- script/system_skin.sh: system artwork follows the theme ----

SKIN_SCRIPT="$ROOT/static/build/.tmp_update/script/system_skin.sh"

begin system_skin_follows_theme
SDROOT="$TMP/sd"
. "$SKIN_SCRIPT"
sd=$SDROOT
def="$sd/.tmp_update/res/system-skin-default"
red="$sd/Themes/Red/system"
qg="App/PackageManager/data/App/Quick Guide/App/Onion_Manual/page1.png"
fe="App/PackageManager/data/App/File Explorer (DinguxCommander)/App/Commander_Italic/res/up.png"
pcsx_pkg="App/PackageManager/data/RApp/Sony - PlayStation (PCSX standalone)/RApp/PCSX-ReARMed/skin/skin.txt"
for f in "App/PackageManager/res/bgApp.png" "$qg" "$fe"; do
    mkdir -p "$(dirname "$def/$f")" "$(dirname "$red/$f")" "$(dirname "$sd/$f")"
    echo onion > "$def/$f"
    echo onion > "$sd/$f"
    echo red > "$red/$f"
done
mkdir -p "$sd/App/Onion_Manual" "$sd/Themes/DiMo" "$sd/BIOS/.advance" "$sd/RetroArch/.retroarch/assets/rgui" "$sd/RApp/PCSX-ReARMed/skin"
echo onion > "$sd/App/Onion_Manual/page1.png"
printf 'ui_color menu_item ffffff 000000\nui_color menu_item_select ffffff 7247c2\nui_color backdrop 7247c2 000000\nui_mode custom 7247c2\n' > "$sd/BIOS/.advance/advmenu.rc"
printf 'rgui_entry_normal_color = "0xFF565a68"\nrgui_title_color = "0xFF9160fd"\n' > "$sd/RetroArch/.retroarch/assets/rgui/Onion.cfg"
mkdir -p "$(dirname "$sd/$pcsx_pkg")"
printf 'font=x.ttf\nselection_color=8526eb\n' > "$sd/$pcsx_pkg"
printf 'font=x.ttf\nselection_color=8526eb\n' > "$sd/RApp/PCSX-ReARMed/skin/skin.txt"

apply_system_skin "$sd/Themes/Red/"
check "red: Package Manager artwork replaced" test "$(cat "$sd/App/PackageManager/res/bgApp.png")" = red
check "red: package source replaced" test "$(cat "$sd/$qg")" = red
check "red: installed Quick Guide follows its package" test "$(cat "$sd/App/Onion_Manual/page1.png")" = red
check "red: app that is not installed is not created" test ! -e "$sd/App/Commander_Italic"
check "red: RetroArch title colour" has_line "$sd/RetroArch/.retroarch/assets/rgui/Onion.cfg" 'rgui_title_color = "0xFFc43b59"'
check "red: AdvanceMENU accents" test "$(grep -c 9e2c45 "$sd/BIOS/.advance/advmenu.rc")" -eq 2
check "red: other AdvanceMENU settings untouched" has_line "$sd/BIOS/.advance/advmenu.rc" 'ui_mode custom 7247c2'
check "red: PCSX package and installed menu colour" test "$(grep -c '^selection_color=9e2c45$' "$sd/$pcsx_pkg" "$sd/RApp/PCSX-ReARMed/skin/skin.txt" | grep -c ':1$')" -eq 2
check "red: state remembers the theme" test "$(cat "$sd/.tmp_update/config/.systemSkin")" = "$sd/Themes/Red"

apply_system_skin "$sd/Themes/DiMo/"
check "onion: Package Manager artwork restored" test "$(cat "$sd/App/PackageManager/res/bgApp.png")" = onion
check "onion: installed Quick Guide restored" test "$(cat "$sd/App/Onion_Manual/page1.png")" = onion
check "onion: RetroArch title colour restored" has_line "$sd/RetroArch/.retroarch/assets/rgui/Onion.cfg" 'rgui_title_color = "0xFF9160fd"'
check "onion: AdvanceMENU accents restored" test "$(grep -c 7247c2 "$sd/BIOS/.advance/advmenu.rc")" -eq 3
check "onion: PCSX colour restored" has_line "$sd/RApp/PCSX-ReARMed/skin/skin.txt" 'selection_color=8526eb'
check "onion: state cleared" test ! -e "$sd/.tmp_update/config/.systemSkin"

echo custom > "$sd/App/PackageManager/res/bgApp.png"
apply_system_skin "$sd/Themes/DiMo/"
check "no skin applied: switching between plain themes copies nothing" test "$(cat "$sd/App/PackageManager/res/bgApp.png")" = custom
unset SDROOT
end

begin system_skin_sets_match
theme_system="$ROOT/static/build/Themes/Silky RedOnion/system"
defaults="$ROOT/static/build/.tmp_update/res/system-skin-default"
(cd "$theme_system" && find . -type f | sort) > "$TMP/theme"
(cd "$defaults" && find . -type f | sort) > "$TMP/defaults"
check "every crimson file has an Onion original" cmp -s "$TMP/theme" "$TMP/defaults"
check "the set is not empty" test "$(wc -l < "$TMP/theme")" -gt 30
# The originals must be the files the build ships in those places.
source_of() { # path on the SD card -> source file in this repository
    case "$1" in
    App/PackageManager/res/*) echo "$ROOT/src/packageManager/res/${1##*/}" ;;
    "App/PackageManager/data/App/Activity Tracker/App/PlayActivity/res/"*) echo "$ROOT/src/playActivityUI/res/${1##*/}" ;;
    App/PackageManager/data/*) p=${1#App/PackageManager/data/} && echo "$ROOT/static/packages/$p" ;;
    .tmp_update/res/toggle-on.png | .tmp_update/res/toggle-off.png) echo "$ROOT/static/build/.tmp_update/res/${1##*/}" ;;
    .tmp_update/res/*) [ -f "$ROOT/src/themeSwitcher/res/${1##*/}" ] && echo "$ROOT/src/themeSwitcher/res/${1##*/}" ;;
    esac
}
mismatch=0
while IFS= read -r rel; do
    rel=${rel#./}
    src=$(source_of "$rel")
    [ -n "$src" ] && [ -f "$src" ] || continue # SearchFilter: submodule
    cmp -s "$src" "$defaults/$rel" || { echo "    differs: $rel"; mismatch=1; }
done < "$TMP/defaults"
check "originals match the shipped files" test $mismatch -eq 0
check "themeSwitcher runs the script" grep -q 'system_skin.sh' "$ROOT/src/themeSwitcher/installTheme.h"
end

# ---- script/shellect.sh: START leaves the app without SIGKILL ----

SHELLECT="$ROOT/static/build/.tmp_update/script/shellect.sh"

begin shellect_start_quits_own_terminal_only
extract_fn "$SHELLECT" quit_terminal > "$TMP/quit.sh"
# A terminal stand-in named st that runs its -c string itself: a script with
# a #! line (the kernel names the process after the file). A copy of sh
# named st is not a shell when sh is BusyBox (applet "st" not found).
printf '#!/bin/sh\n[ "$1" = -c ] && eval "$2"\n' > "$TMP/st"
chmod +x "$TMP/st"
# st -e sh script.sh: the menu runs in a shell under the terminal.
"$TMP/st" -c "sh -c 'term_reset() { :; }; . \"$TMP/quit.sh\"; quit_terminal'; echo still-open" > "$TMP/out" 2>&1
rc=$?
check "the menu's own terminal is asked to quit (SIGTERM, 143)" test $rc -eq 143
check "nothing runs after the terminal quits" sh -c "! grep -q still-open '$TMP/out'"
sh -c "term_reset() { :; }; . '$TMP/quit.sh'; quit_terminal"
check "no terminal above: the menu just leaves" test $? -eq 1
check "no name-based kill left (it hit Open MainUI in mainui-test)" sh -c "! grep -v '^[[:space:]]*#' '$SHELLECT' | grep -q pkill"
check "end of input leaves the key loop" grep -q '\[ -n "\$key" \] || exit 1' "$SHELLECT"
check "MENU (a lone Esc) leaves like START" grep -q 'stty min 0 time 1' "$SHELLECT"
check "with -m only Up/Down, A and MENU are accepted" grep -q "A2|B2|\"\$space\"?) ;;" "$SHELLECT"
# (two menus: the terminal's and the new UI's, both with -m)
check "OTA menus leave with MENU only (-m: START ignored)" test "$(grep -c 'shellect.sh -m -s "${3:-1}" -t' "$ROOT/static/build/.tmp_update/script/ota_update.sh")" -eq "$(grep -c 'shellect.sh ' "$ROOT/static/build/.tmp_update/script/ota_update.sh")"
check "OTA menus all go through ota_menu" test "$(grep -c '^	ota_menu "' "$ROOT/static/build/.tmp_update/script/ota_update.sh")" -eq 3
check "with -m MENU returns 2 instead of closing the terminal" grep -q '\[ -z "\$menu_only_exit" \] || { term_reset; exit 2; }' "$SHELLECT"
end

begin scraper_closes_its_own_terminal_only
# The scraper menu closed its terminal with "pkill st", which matched any
# process with "st" in its name (hostapd, installUI).
SCRAPER_MENU="$ROOT/static/build/.tmp_update/script/scraper/menu.sh"
extract_fn "$SCRAPER_MENU" close_terminal > "$TMP/close.sh"
# A terminal stand-in named st that runs its -c string itself: a script with
# a #! line (the kernel names the process after the file). A copy of sh
# named st is not a shell when sh is BusyBox (applet "st" not found).
printf '#!/bin/sh\n[ "$1" = -c ] && eval "$2"\n' > "$TMP/st"
chmod +x "$TMP/st"
"$TMP/st" -c "sh -c '. \"$TMP/close.sh\"; close_terminal'; sleep 1; echo still-open" > "$TMP/out" 2>&1
rc=$?
check "the menu's own terminal is asked to quit (SIGTERM, 143)" test $rc -eq 143
check "nothing runs after the terminal quits" sh -c "! grep -q still-open '$TMP/out'"
sh -c ". '$TMP/close.sh'; close_terminal; echo returned" > "$TMP/out2"
check "no terminal above: nothing killed, it returns" grep -q returned "$TMP/out2"
check "no name-based kill left in the scraper" sh -c "! grep -v '^[[:space:]]*#' \"\$1\"/*.sh | grep -q 'pkill st'" _ "$ROOT/static/build/.tmp_update/script/scraper"
end

# ---- ota_update.sh: "Press A to continue, MENU to exit" ----

OTA_SCRIPT="$ROOT/static/build/.tmp_update/script/ota_update.sh"

begin ota_info_screen_menu_exits
for f in wait_key read_button ask_leave drop_keys keys_red; do eval "$(extract_fn "$OTA_SCRIPT" $f)"; done
eval "$(grep -E '^(ESC|CR)=' "$OTA_SCRIPT")"
NL='
'
YELLOW= NC=
printf ' ' > "$TMP/a"
wait_key "x" < "$TMP/a" > /dev/null
check "A continues" test $? -eq 0
printf 'x\n\033[Ayy ' > "$TMP/other"
wait_key "x" < "$TMP/other" > /dev/null
check "other buttons (START, the D-pad...) are ignored until A" test $? -eq 0
printf '\033' > "$TMP/menu"
out=$(wait_key "x" < "$TMP/menu")
check "MENU (a lone Esc), then nothing: leaves" test $? -eq 1
esc_x=$(printf '\033')
plain=$(printf '%s' "$out" | sed "s/$esc_x\[[0-9;]*m//g")
check "MENU asks first" sh -c 'echo "$1" | grep -q "Exit the updater? START: yes, A: no"' _ "$plain"
check "the buttons in red (START:, A:)" sh -c 'case "$1" in *"$2[31;1mSTART:"*) exit 0 ;; esac; exit 1' _ "$out" "$esc_x"
printf '\033\n' > "$TMP/menu_start"
wait_key "x" < "$TMP/menu_start" > /dev/null
check "MENU, then START: leaves" test $? -eq 1
# Without a terminal the follow-up read takes the next byte: keep MENU
# last in the input, and test "stay" with ask_leave itself.
printf ' ' > "$TMP/a"
ask_leave "q" < "$TMP/a" > /dev/null
check "asked, A: stays" test $? -eq 1
printf 'x\033' > "$TMP/menu_again"
ask_leave "q" < "$TMP/menu_again" > /dev/null
check "asked, MENU again does not answer (end of input: leaves)" test $? -eq 0
wait_key "x" < /dev/null > /dev/null
check "no input (terminal gone) leaves" test $? -eq 1
check "the version screen offers MENU to exit" grep -q 'wait_key "Press A to continue, MENU to exit" || exit 0' "$OTA_SCRIPT"
check "every Press A prompt drops the buttons pressed before" test "$(grep -c 'read -n 1 -s -r -p' "$OTA_SCRIPT")" -eq "$(grep -c 'drop_keys; read -n 1 -s -r -p' "$OTA_SCRIPT")"
check "echo off for the whole updater" grep -q "^	quiet_keys$" "$OTA_SCRIPT"
unset -f wait_key read_button ask_leave drop_keys
unset YELLOW NC
end

begin ota_connection_check_ignores_the_clock
# A device with its clock wrong (no clock battery, no time sync yet) failed
# the check on a working network: the certificate looked not yet valid.
# The check skips the certificate, like the list and the download, and
# says why it failed.
eval "$(extract_fn "$OTA_SCRIPT" check_connection)"
curl() {
    echo "$*" > "$TMP/curl_args"
    case " $* " in
        *" -k "*) [ -f "$TMP/offline" ] && { echo "curl: (6) Could not resolve host: github.com" >&2; return 6; }; return 0 ;;
        *) echo "curl: (60) SSL certificate problem: certificate is not yet valid" >&2; return 60 ;;
    esac
}
wget() { echo "wget used" > "$TMP/wget_used"; return 4; }
read() { :; }
out=$( (check_connection) 2>&1); rc=$?
check "wrong clock: connection OK" test $rc -eq 0
check "the certificate is not checked" grep -q -- "-k" "$TMP/curl_args"
check "still a 15 s connect timeout" grep -q -- "--connect-timeout 15" "$TMP/curl_args"
check "and an end to it" grep -q -- "--max-time 30" "$TMP/curl_args"
# The bundled wget (mbed TLS 2.2.1) timed out on github.com: curl checks.
check "wget not used for the check" test ! -e "$TMP/wget_used"
touch "$TMP/offline"
out=$( (check_connection) 2>&1); rc=$?
check "offline: exits 2" test $rc -eq 2
check "offline: the reason is shown ($out)" sh -c "echo \"\$1\" | grep -q 'not reachable (curl: (6) Could not resolve host: github.com)'" _ "$out"
check "the download uses curl too" grep -q 'curl -k -L -f -s --connect-timeout 15 --retry 3' "$OTA_SCRIPT"
unset -f curl wget read check_connection
end

begin ota_connection_check_headers_only
# #311: on some home networks the github.com page stalled after the HTTP 200
# until the time limit. The check asks for the headers only; a server that
# refuses HEAD gets a compressed GET instead.
eval "$(extract_fn "$OTA_SCRIPT" check_connection)"
read() { :; }
curl() {
    echo "$*" >> "$TMP/calls"
    case " $* " in
        *" -I "*) return ${HEAD_RC:-0} ;;
        *" --compressed "*) return 0 ;;
    esac
    return 99
}
(check_connection) > /dev/null 2>&1; rc=$?
check "HEAD answered: OK" test $rc -eq 0
check "HEAD answered: one request, headers only" test "$(wc -l < "$TMP/calls")" -eq 1 -a "$(grep -c -- ' -I ' "$TMP/calls")" -eq 1
rm -f "$TMP/calls"
HEAD_RC=22; (check_connection) > /dev/null 2>&1; rc=$?
check "HEAD refused (HTTP error): the compressed page instead, OK" test $rc -eq 0 -a "$(grep -c -- '--compressed' "$TMP/calls")" -eq 1
rm -f "$TMP/calls"
HEAD_RC=6; (check_connection) > /dev/null 2>&1; rc=$?
check "offline: no second try, exits 2" test $rc -eq 2 -a "$(wc -l < "$TMP/calls")" -eq 1
unset -f curl read check_connection; unset HEAD_RC
end

# ---- ota_update.sh: MENU during the download asks, and stops it ----

begin ota_download_menu_asks_then_stops
for f in download_file stop_download read_button onion_percent; do eval "$(extract_fn "$OTA_SCRIPT" $f)"; done
# The onion drawing is checked in ota_onion_progress_fits_the_screen
onion_draw() { :; }
onion_status() { :; }
eval "$(grep -E '^(ESC|CR)=' "$OTA_SCRIPT")"
NL='
'
YELLOW= NC=
curl() { # writes the -o file after 1 s, then a marker
    _o=
    while [ $# -gt 0 ]; do
        [ "$1" = "-o" ] && _o=$2
        shift
    done
    sleep 1
    echo zip > "$_o"
    echo done > "$TMP/curl_done"
    return ${CURL_RC:-0}
}
ask_leave() { echo asked >> "$TMP/asked"; return ${ANSWER:-0}; }

download_file "https://x/u.zip" "$TMP/u.zip" < /dev/null > /dev/null
check "no buttons: the download ends normally" test $? -eq 0
check "no buttons: the file is there" test -s "$TMP/u.zip"
check "no buttons: nothing asked" test ! -e "$TMP/asked"

rm -f "$TMP/u.zip" "$TMP/curl_done"
printf 'x\033[A' > "$TMP/keys"
download_file "https://x/u.zip" "$TMP/u.zip" < "$TMP/keys" > /dev/null
check "other buttons and the D-pad: the download ends normally" test $? -eq 0
check "other buttons and the D-pad: nothing asked" test ! -e "$TMP/asked"

rm -f "$TMP/u.zip" "$TMP/curl_done"
printf '\033' > "$TMP/menu"
export ANSWER=1; download_file "https://x/u.zip" "$TMP/u.zip" < "$TMP/menu" > /dev/null; _rc=$?; unset ANSWER; rc $_rc
check "MENU, then A: the download goes on" test $? -eq 0
check "MENU, then A: asked once" test "$(wc -l < "$TMP/asked")" -eq 1
check "MENU, then A: the file is there" test -s "$TMP/u.zip"

rm -f "$TMP/u.zip" "$TMP/curl_done" "$TMP/asked"
export ANSWER=0; download_file "https://x/u.zip" "$TMP/u.zip" < "$TMP/menu" > /dev/null; _rc=$?; unset ANSWER; rc $_rc
check "MENU, then START: stopped (130)" test $? -eq 130
sleep 2
check "MENU, then START: the download was ended" test ! -e "$TMP/curl_done"
check "MENU, then START: no partial file" test ! -e "$TMP/u.zip"

rm -f "$TMP/asked"
export CURL_RC=22; download_file "https://x/u.zip" "$TMP/u.zip" < /dev/null > /dev/null; _rc=$?; unset CURL_RC; rc $_rc
check "curl's error comes back" test $? -eq 22
unset CURL_RC

check "the updater uses it" grep -q 'download_file "\$Release_url" "\$sysdir/download/\$Release_Version.zip"' "$OTA_SCRIPT"
check "a stop leaves the updater" grep -q 'Download stopped.' "$OTA_SCRIPT"
check "leaving the updater ends the download" grep -q "trap 'stop_download; restore_wifi; restore_keys' EXIT" "$OTA_SCRIPT"
unset -f curl ask_leave download_file stop_download read_button onion_draw onion_status onion_percent
unset YELLOW NC ANSWER
end

# ---- runtime.sh: the console last selected survives a shutdown ----

begin console_position_kept_with_the_option
for f in restore_console_position save_console_position; do eval "$(extract_fn "$RUNTIME" $f)"; done
log() { :; }
console_position_tmp="$TMP/tmp-console-position.json"
console_position_sd="$TMP/console-position.json"
console_position_flag="$TMP/.rememberConsole"
echo '{"games":"gba"}' > "$console_position_tmp"
save_console_position
check "option off: nothing saved" test ! -e "$console_position_sd"
touch "$console_position_flag"
save_console_position
check "option on: saved at shutdown" test "$(cat "$console_position_sd")" = '{"games":"gba"}'
rm -f "$console_position_tmp"
restore_console_position
check "option on: restored at boot" test "$(cat "$console_position_tmp")" = '{"games":"gba"}'
rm -f "$console_position_flag" "$console_position_tmp"
restore_console_position
check "option off: not restored" test ! -e "$console_position_tmp"
save_console_position
check "option off: the saved copy goes" test ! -e "$console_position_sd"
check "saved on every shutdown" line_follows "$RUNTIME" 'touch /tmp/shutting_down' 'save_console_position'
check "restored at boot" line_follows "$RUNTIME" 'restore_romwinidx' 'restore_console_position'
printf 'a\nx\nb\n' > "$TMP/lf"
! line_follows "$TMP/lf" a b
check "and the check fails when the second line is elsewhere" test $? -eq 0
unset -f restore_console_position save_console_position log
end

# ---- console_pictures.sh + migration 00026: KyleBing's console pictures ----

begin console_pictures_copied_without_replacing
CP="$ROOT/static/build/.tmp_update/script/console_pictures.sh"
RES="$ROOT/static/build/.tmp_update/res/consoles"
mkdir -p "$TMP/consoles"
echo mine > "$TMP/consoles/gba.png"
CONSOLE_PICTURES_SRC="$RES" CONSOLE_PICTURES_DEST="$TMP/consoles" sh "$CP"
check "runs fine" test $? -eq 0
check "a picture put there by hand stays" test "$(cat "$TMP/consoles/gba.png")" = mine
check "_default.png copied" cmp -s "$RES/_default.png" "$TMP/consoles/_default.png"
check "Emu PSX gets the PlayStation picture" cmp -s "$RES/ps.png" "$TMP/consoles/psx.png"
check "Emu ATARI gets the 2600 picture" cmp -s "$RES/atari2600.png" "$TMP/consoles/atari.png"
check "RApp snes9x2005 gets the SNES picture" cmp -s "$RES/sfc.png" "$TMP/consoles/snes9x2005.png"
check "the licence goes with them" grep -q "CC BY 4.0" "$TMP/consoles/LICENSE-console-icons.txt"
check "Search (Emu/SEARCH, made by the Search app) has its own picture" cmp -s "$RES/search.png" "$TMP/consoles/search.png"
check "and it is a 500x370 PNG like the others" python3 -c "
import struct,sys
d=open(sys.argv[1],'rb').read(24); assert d[:8]==b'\\x89PNG\\r\\n\\x1a\\n' and struct.unpack('>II',d[16:24])==(500,370)
" "$RES/search.png"
missing=
while read -r name icon _; do
    case "$name" in '' | '#'*) continue ;; esac
    [ -f "$RES/$icon.png" ] || missing="$missing $icon"
done < "$RES/map.txt"
check "every icon map.txt names is shipped ($missing)" test -z "$missing"
unmapped=
for d in "$ROOT"/static/packages/Emu/*/Emu/* "$ROOT"/static/packages/RApp/*/RApp/*; do
    n=$(basename "$d" | tr 'A-Z' 'a-z')
    grep -q "^$n " "$RES/map.txt" || unmapped="$unmapped $n"
done
unmapped=$(echo $unmapped | tr ' ' '\n' | sort | tr '\n' ' ')
check "folders without their own picture use _default.png ($unmapped)" test "$unmapped" = "bk gametank gme jaguar opera puzzlescript uw8 vemulator "
rm "$TMP/consoles/fc.png"
CONSOLE_PICTURES_SRC="$RES" CONSOLE_PICTURES_DEST="$TMP/consoles" sh "$CP"
check "a missing one comes back on the next update" test -f "$TMP/consoles/fc.png"
check "the installer runs it before the migrations" grep -q 'sh $sysdir/script/console_pictures.sh' "$ROOT/static/dist/miyoo/app/.tmp_update/install.sh"
MIG="$ROOT/static/build/.tmp_update/script/migration/00026_console_pictures_default.sh"
CONSOLE_VIEW_CONFIG="$TMP/console-view.json" CONSOLE_PICTURES_DEST="$TMP/consoles" sh "$MIG"
check "no console-view.json: the pictures are turned on" grep -q "\"thumbs\":	\"$TMP/consoles\"" "$TMP/console-view.json"
check "and the carousel look: valid JSON with no selection box, 0.7, 0.5, smooth, 180 ms" python3 -c "
import json,sys
d=json.load(open(sys.argv[1])); c=d['carousel']
assert c=={'selection':False,'scale_unselected':0.7,'opacity_unselected':0.5,'easing':'smooth','duration_ms':180}, c
" "$TMP/console-view.json"
echo '{"games": "carousel"}' > "$TMP/console-view.json"
CONSOLE_VIEW_CONFIG="$TMP/console-view.json" CONSOLE_PICTURES_DEST="$TMP/consoles" sh "$MIG"
check "an existing console-view.json is left alone" test "$(cat "$TMP/console-view.json")" = '{"games": "carousel"}'
rm -f "$TMP/console-view.json"
CONSOLE_VIEW_CONFIG="$TMP/console-view.json" CONSOLE_PICTURES_DEST="$TMP/none" sh "$MIG"
check "no pictures: nothing written" test ! -e "$TMP/console-view.json"
end

# ---- console_pictures.sh: folders renamed or copied by hand ----

begin console_pictures_for_renamed_folders
RES="$ROOT/static/build/.tmp_update/res/consoles"
CP="$ROOT/static/build/.tmp_update/script/console_pictures.sh"
SD="$TMP/sd"
mk() { # folder, config.json text
    mkdir -p "$SD/$1"; printf '%s\n' "$2" > "$SD/$1/config.json"
}
mk "Emu/GBA mGBA" '{
"label":"GBA",
"icon":"../../Icons/Default/gba.png",
"rompath":"../../Roms/GBA",
"extlist":"gba"
}'
mk "Emu/SNES2" '{"label": "SNES", "rompath": "../../Roms/SFC/", "icon": "../../Icons/Default/sfc.png"}'
mk "Emu/MyGB" '{
    "rompath": "../../Roms/My Game Boy",
    "icon": "../../Icons/Default/gb.png"
}'
mk "Emu/WEIRD" '{
"rompath":"../../Roms/WEIRD",
"icon":"../../Icons/Default/weird.png"
}'
mk "RApp/vba_m copy" '{
    "rompath": "../../Roms/GBA",
    "icon": "../../Icons/Default/rapp/vbam.png"
}'
mk "Emu/HANDMADE" '{"rompath":"../../Roms/GBA"}'
mk "Emu/GBA" '{"rompath":"../../Roms/GBA"}'
mkdir -p "$TMP/consoles"
echo mine > "$TMP/consoles/handmade.png"
run() { CONSOLE_PICTURES_SRC="$RES" CONSOLE_PICTURES_DEST="$TMP/consoles" CONSOLE_PICTURES_SD="$SD" sh "$CP" "$@"; }
run renamed
check "renamed folder: picture of its ROM folder" cmp -s "$RES/gba.png" "$TMP/consoles/gba mgba.png"
check "one-line config, rompath ending in /" cmp -s "$RES/sfc.png" "$TMP/consoles/snes2.png"
check "unknown ROM folder: the icon's name" cmp -s "$RES/gb.png" "$TMP/consoles/mygb.png"
check "nothing known: no picture (the default one shows)" test ! -e "$TMP/consoles/weird.png"
check "a copied RApp folder too" cmp -s "$RES/gba.png" "$TMP/consoles/vba_m copy.png"
check "a picture put there by hand stays" test "$(cat "$TMP/consoles/handmade.png")" = mine
check "boot pass: folders of map.txt are not its business" test ! -e "$TMP/consoles/gba.png"
check "boot pass: no default, no licence" test ! -e "$TMP/consoles/_default.png" -a ! -e "$TMP/consoles/LICENSE-console-icons.txt"
rm -rf "$TMP/none"
CONSOLE_PICTURES_SRC="$RES" CONSOLE_PICTURES_DEST="$TMP/none" CONSOLE_PICTURES_SD="$SD" sh "$CP" renamed
check "boot pass without the pictures folder: nothing created" test ! -e "$TMP/none"
run
check "update: the map.txt ones too" cmp -s "$RES/gba.png" "$TMP/consoles/gba.png"
check "update: renamed folders as well" cmp -s "$RES/sfc.png" "$TMP/consoles/snes2.png"
check "runtime runs the boot pass in background" grep -q '^    sh \$sysdir/script/console_pictures.sh renamed &$' "$RUNTIME"
# Every bundled emulator, copied under a new name: the same picture as
# the original, except the 7 folders that have none (default picture).
rm -rf "$SD" "$TMP/consoles"; mkdir -p "$TMP/consoles"
wrong=
for c in "$ROOT"/static/packages/Emu/*/Emu/*/config.json "$ROOT"/static/packages/RApp/*/RApp/*/config.json; do
    n=$(basename "$(dirname "$c")")
    mkdir -p "$SD/Emu/copy of $n"; cp "$c" "$SD/Emu/copy of $n/config.json"
done
run renamed
for c in "$ROOT"/static/packages/Emu/*/Emu/*/config.json "$ROOT"/static/packages/RApp/*/RApp/*/config.json; do
    n=$(basename "$(dirname "$c")"); l=$(echo "$n" | tr 'A-Z' 'a-z')
    want=$(awk -v n="$l" '$1 == n { print $2; exit }' "$RES/map.txt")
    got="$TMP/consoles/copy of $l.png"
    if [ -z "$want" ]; then
        continue
    elif [ ! -e "$got" ]; then
        wrong="$wrong $l:none"
    elif ! cmp -s "$RES/$want.png" "$got"; then
        # or the picture of its ROM folder, shared with another emulator
        # (MAME 2003-Xtreme reads Roms/ARCADE: the arcade picture)
        r=$(sed -n 's/.*"rompath"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$c" | head -n 1)
        r=$(echo "${r%/}" | tr 'A-Z' 'a-z'); r=${r#*/roms/}
        alt=$(awk -v n="$r" '$1 == n { print $2; exit }' "$RES/map.txt" "$RES/roms.txt")
        [ -n "$alt" ] && cmp -s "$RES/$alt.png" "$got" || wrong="$wrong $l"
    fi
done
check "a copy of any bundled folder gets its console's picture ($wrong)" test -z "$wrong"
unset -f mk run
end

# ---- migration 00027: console pictures trimmed of their padding ----

begin console_pictures_replaced_when_unchanged
RES="$ROOT/static/build/.tmp_update/res/consoles"
MIG="$ROOT/static/build/.tmp_update/script/migration/00027_console_pictures_trimmed.sh"
OLDSUMS="$ROOT/static/build/.tmp_update/script/migration/data/00027_old_console_pictures.md5"
CP="$ROOT/static/build/.tmp_update/script/console_pictures.sh"
check "the old checksums are listed (one per line)" sh -c "[ \$(grep -cE '^[0-9a-f]{32}$' '$OLDSUMS') -eq \$(wc -l < '$OLDSUMS') ] && [ \$(wc -l < '$OLDSUMS') -ge 70 ]"
same=
for f in "$RES"/*.png; do
    has_line "$OLDSUMS" "$(md5sum "$f" | cut -d ' ' -f 1)" && same="$same $(basename "$f")"
done
check "no new picture is in the old list ($same)" test -z "$same"
mkdir -p "$TMP/consoles"
echo old-gba > "$TMP/consoles/gba.png"
echo old-psx > "$TMP/consoles/psx.png"
echo mine > "$TMP/consoles/sfc.png"
cp "$RES/gb.png" "$TMP/consoles/gb.png"
printf '%s\n%s\n' "$(md5sum "$TMP/consoles/gba.png" | cut -d ' ' -f 1)" "$(md5sum "$TMP/consoles/psx.png" | cut -d ' ' -f 1)" > "$TMP/old.md5"
run() { CONSOLE_PICTURES_DEST="$TMP/consoles" CONSOLE_PICTURES_OLD_MD5="$TMP/old.md5" CONSOLE_PICTURES_SRC="$RES" CONSOLE_PICTURES_SCRIPT="$CP" sh "$MIG"; }
run
check "runs fine" test $? -eq 0
check "an old picture is replaced by the new one" cmp -s "$RES/gba.png" "$TMP/consoles/gba.png"
check "also under another folder's name (psx = ps)" cmp -s "$RES/ps.png" "$TMP/consoles/psx.png"
check "a picture changed by hand stays" test "$(cat "$TMP/consoles/sfc.png")" = mine
check "a new picture already there stays" cmp -s "$RES/gb.png" "$TMP/consoles/gb.png"
run
check "running it again changes nothing" test "$(cat "$TMP/consoles/sfc.png")" = mine
CONSOLE_PICTURES_DEST="$TMP/none" CONSOLE_PICTURES_OLD_MD5="$TMP/old.md5" sh "$MIG"
check "no pictures folder: nothing to do" test ! -e "$TMP/none"
check "the installer copies the pictures before the migrations" sh -c "grep -n 'sh \$sysdir/script/console_pictures.sh' '$ROOT/static/dist/miyoo/app/.tmp_update/install.sh' | head -1 | cut -d: -f1 | { read a; b=\$(grep -n '^    run_migration_scripts' '$ROOT/static/dist/miyoo/app/.tmp_update/install.sh' | head -1 | cut -d: -f1); [ \$a -lt \$b ]; }"
unset -f run
end

# ---- migration 00022: RetroArch menu Border Filler OFF ----

begin migration_disables_rgui_border_filler
MIG="$ROOT/static/build/.tmp_update/script/migration/00022_disable_rgui_border_filler.sh"
printf 'menu_driver = "rgui"\nrgui_border_filler_enable = "true"\nrgui_border_filler_thickness_enable = "true"\n' > "$TMP/ra.cfg"
RA_CFG="$TMP/ra.cfg" sh "$MIG"
check "existing setting turned off" has_line "$TMP/ra.cfg" 'rgui_border_filler_enable = "false"'
check "other settings untouched" has_line "$TMP/ra.cfg" 'rgui_border_filler_thickness_enable = "true"'
check "only one line for the setting" test "$(grep -c '^rgui_border_filler_enable' "$TMP/ra.cfg")" -eq 1
printf 'menu_driver = "rgui"\n' > "$TMP/ra2.cfg"
RA_CFG="$TMP/ra2.cfg" sh "$MIG"
check "missing setting added as off" has_line "$TMP/ra2.cfg" 'rgui_border_filler_enable = "false"'
RA_CFG="$TMP/none.cfg" sh "$MIG"
check "no RetroArch config: nothing created" test ! -e "$TMP/none.cfg"
check "new installs default to off" has_line "$ROOT/static/configs/RetroArch/.retroarch/retroarch.cfg" 'rgui_border_filler_enable = "false"'
end

# ---- migration 00023: devices on the old default theme get Silky RedOnion ----

begin migration_moves_old_default_theme_to_redonion
MIG="$ROOT/static/build/.tmp_update/script/migration/00023_default_theme_redonion.sh"
sd=$TMP/sd
mkdir -p "$sd/Themes/Silky RedOnion" "$sd/.tmp_update/config/system"
echo '{}' > "$sd/Themes/Silky RedOnion/config.json"
printf '{\n\t"vol":\t20,\n\t"theme":\t"%s/Themes/Silky by DiMo/",\n\t"wifi":\t0\n}\n' "$sd" > "$sd/system.json"
printf '{\n\t"theme":\t"%s/Themes/Silky by DiMo",\n\t"wifi":\t1\n}\n' "$sd" > "$sd/.tmp_update/config/system/A.json"
printf '{\n\t"theme":\t"%s/Themes/Other/",\n\t"wifi":\t1\n}\n' "$sd" > "$sd/.tmp_update/config/system/B.json"
SDCARD=$sd sh "$MIG"
check "system.json moved to Silky RedOnion" grep -qF "\"theme\":	\"$sd/Themes/Silky RedOnion/\"," "$sd/system.json"
check "other keys untouched" grep -qF '"vol":	20,' "$sd/system.json"
check "device copy moved too (no trailing slash)" grep -qF "\"$sd/Themes/Silky RedOnion/\"" "$sd/.tmp_update/config/system/A.json"
check "another theme left alone" grep -qF "\"$sd/Themes/Other/\"" "$sd/.tmp_update/config/system/B.json"
check "no temp files left" test -z "$(find "$sd" -name '*.tmp')"
rm -rf "$sd/Themes/Silky RedOnion"
printf '{\n\t"theme":\t"%s/Themes/Silky by DiMo/"\n}\n' "$sd" > "$sd/system.json"
SDCARD=$sd sh "$MIG"
check "without Silky RedOnion on the card: unchanged" grep -qF "Silky by DiMo/" "$sd/system.json"
check "new installs default to Silky RedOnion" grep -qF '"/mnt/SDCARD/Themes/Silky RedOnion/"' "$ROOT/static/build/.tmp_update/res/miyoo285_system.json"
end

# ---- Settings > About: the build date and the RedOnion version row ----
#
# Open MainUI shows the first characters of .tmp_update/onionVersion/version.txt
# under the label of string 410. These fail when a change would hide part of the
# build date there, or lose "RedOnion" from the label: in OnionPlus (Makefile,
# workflows, language files) or in an Open MainUI update (its limit or its ids).

OMUI_ABOUT="$ROOT/third-party/open-mainui/src/app/settings_page.c"

# A make run of its own: the variables of the make running these tests must
# not reach it.
onion_make_version() {
    env -u MAKEFLAGS -u MFLAGS -u MAKELEVEL -u MAKEOVERRIDES -u VERSION_DATE \
        -u VERSION_OVERRIDE "$@" make -s -C "$ROOT" version 2> /dev/null
}

begin about_shows_the_whole_build_date
limit=$(sed -n 's/.*if (version_end > \([0-9][0-9]*\)) {.*/\1/p' "$OMUI_ABOUT" | head -n 1)
check "Open MainUI's version length limit found (if this fails, Open MainUI changed how it cuts the version: review this test)" test -n "$limit"
check "Open MainUI reads .tmp_update/onionVersion/version.txt" grep -q '".tmp_update/onionVersion/version.txt"' "$OMUI_ABOUT"
check "the build writes v\$(VERSION) to version.txt" grep -qF 'echo -n "v$(VERSION)" > $(BUILD_DIR)/.tmp_update/onionVersion/version.txt' "$ROOT/Makefile"
base=$(sed -n 's/^VERSION_BASE=//p' "$ROOT/Makefile")
check "the Makefile sets VERSION_BASE" test -n "$base"
# What a release writes, before the commit hash: v<base>-<YYYYMMDD>.
shown="v$base-YYYYMMDD"
check "Settings > About shows v$base and the whole date (${#shown} characters, at most ${limit:-?})" test "${#shown}" -le "${limit:-0}"
check "a build made by hand is dated today (UTC)" test "$(onion_make_version)" = "$base-$(date -u +%Y%m%d)"
check "a release's VERSION_OVERRIDE is used as it is" test "$(onion_make_version VERSION_OVERRIDE=9.9.9-test-20260101)" = "9.9.9-test-20260101"
check "pre-releases are dated with the build day (UTC)" grep -qF 'BUILD_DATE=$(date -u +%Y%m%d)' "$ROOT/.github/workflows/pre-release.yml"
check "pre-releases put the date before the commit hash" grep -qF 'make release VERSION_OVERRIDE="${BUILD_VERSION}-${SHA_SHORT}"' "$ROOT/.github/workflows/pre-release.yml"
check "tagged releases add the date when the tag has none" grep -qF 'BUILD_VERSION="${BUILD_VERSION}-$(date -u +%Y%m%d)"' "$ROOT/.github/workflows/tagged-release.yml"
end

begin about_labels_the_version_redonion
missing=""
count=0
for lang_file in "$ROOT"/static/build/miyoo/app/lang/*.lang; do
    count=$((count + 1))
    grep -q '"410":[[:space:]]*"[^"]*RedOnion' "$lang_file" || missing="$missing $(basename "$lang_file")"
done
check "language files found" test "$count" -gt 0
check "every language names string 410 with RedOnion (missing in:${missing:- none})" test -z "$missing"
# The About row showing version.txt is labelled with string 410: same position
# in draw_about's ids[] and values[].
about_row=$(python3 - "$OMUI_ABOUT" << 'PYEOF'
import re, sys
src = open(sys.argv[1]).read()
body = src[src.index("static void draw_about("):]
ids = [int(x) for x in re.search(r"const int ids\[\] = \{([^}]*)\}", body).group(1).split(",")]
values = [v.strip() for v in re.search(r"const char \*values\[\] = \{([^}]*)\}", body).group(1).split(",")]
print(ids[values.index("page->version")] if "page->version" in values and len(ids) == len(values) else "")
PYEOF
)
check "Open MainUI labels the version.txt row with string 410 (found: ${about_row:-none})" test "$about_row" = "410"
check "Open MainUI reads the website from .tmp_update/onionVersion/website.txt" grep -q '".tmp_update/onionVersion/website.txt"' "$OMUI_ABOUT"
check "website.txt names the project" grep -q 'github.com/Amiga500/Onion' "$ROOT/static/build/.tmp_update/onionVersion/website.txt"
end

# ---- language list: "Native name (English name)" ----

begin language_names_give_the_english_name
unnamed=$(python3 - "$ROOT/static/build/miyoo/app/lang" << 'EOF'
import json, pathlib, sys
for p in sorted(pathlib.Path(sys.argv[1]).glob("*.lang")):
    name = json.loads(p.read_text(encoding="utf-8-sig")).get("lang", "")
    if p.name != "en.lang" and not (name.endswith(")") and " (" in name):
        print(f"{p.name}={name}")
EOF
)
check "every language but English ends with \" (English name)\" in ASCII brackets (missing: ${unnamed:-none})" test -z "$unnamed"
end

begin search_info_website_translated
untranslated=$(python3 - "$ROOT/static/build/miyoo/app/lang" << 'EOF'
import json, pathlib, sys
def letters(text, lo, hi):
    return any(lo <= ord(c) <= hi for c in text)
for p in sorted(pathlib.Path(sys.argv[1]).glob("*.lang")):
    d = json.loads(p.read_text(encoding="utf-8-sig"))
    # "Search" (main menu): every language but English translates it.
    if p.name != "en.lang" and d.get("153") == "Search":
        print(f"{p.name}:153")
    # Arabic and Bengali: Info, Search and Website in their own script.
    script = {"ar.lang": (0x0600, 0x06FF), "bn.lang": (0x0980, 0x09FF)}.get(p.name)
    for key in ("150", "153", "155") if script else ():
        if not letters(d.get(key, ""), *script):
            print(f"{p.name}:{key}")
EOF
)
check "Search, Info and Website translated (untranslated: ${untranslated:-none})" test -z "$untranslated"
end

begin refresh_one_and_refresh_all_differ
same_refresh=$(python3 - "$ROOT/static/build/miyoo/app/lang" << 'EOF'
import json, pathlib, sys
# Open MainUI shows both in one SELECT menu on the console grid:
# 27 "Refresh roms" (this console) and 54 "Refresh all roms".
for p in sorted(pathlib.Path(sys.argv[1]).glob("*.lang")):
    d = json.loads(p.read_text(encoding="utf-8-sig"))
    if d.get("27") == d.get("54"):
        print(p.name)
EOF
)
check "\"Refresh roms\" and \"Refresh all roms\" differ (same in: ${same_refresh:-none})" test -z "$same_refresh"
end

begin select_menu_labels_fit
# Open MainUI's SELECT menu with Silky RedOnion: text from x 20 of the
# popup art (bg-pop-menu-N.png; 4 to 6 rows repeat the 3-row one), so a
# label has the art's width less 20. The list font is wqy-microhei at 25 px
# in bold: SDL_ttf's bold adds 2 px (25 / 10) to every glyph, so a label is
# its Pillow width plus 2 px a character (within 3 px of SDL_ttf for these
# labels). Arabic and Bengali are drawn in the bold Noto fonts. Every label
# the menu can show (src/menus/context.c) in every language. Needs Pillow;
# skipped without it.
too_wide=$(python3 - "$ROOT/static/build/miyoo/app" "$ROOT/static/build/Themes/Silky RedOnion/skin" << 'EOF'
import json, pathlib, sys
try:
    from PIL import Image, ImageFont
except ImportError:
    sys.exit(0)
app, skin = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
widths = {Image.open(skin / f"bg-pop-menu-{n}.png").width for n in (1, 2, 3)}
if len(widths) != 1:
    print(f"popup art widths differ: {sorted(widths)}")
    sys.exit(0)
room = widths.pop() - 20
ids = (51, 55, 52, 50, 405, 128, 27, 401, 402, 400, 403, 404, 406,
       54, 153, 18, 1, 2, 107, 15, 125, 407, 86)
wqy = ImageFont.truetype(str(app / "wqy-microhei.ttc"), 25)
noto = {"ar": ImageFont.truetype(str(app / "NotoSansArabicUI-Bold.ttf"), 25),
        "bn": ImageFont.truetype(str(app / "NotoSansBengaliUI-Bold.ttf"), 25)}
for p in sorted((app / "lang").glob("*.lang")):
    d = json.loads(p.read_text(encoding="utf-8-sig"))
    for key in ids:
        text = d.get(str(key), "")
        width = (noto[p.stem].getlength(text) if p.stem in noto
                 else wqy.getlength(text) + 2 * len(text))
        if width > room:
            print(f"{p.stem}:{key}")
EOF
)
check "SELECT menu labels fit Silky RedOnion's popup (too wide: $(echo $too_wide))" test -z "$too_wide"
end

begin select_menu_labels_fit_320px_themes
# The same labels in a theme with the stock 320 px popup art (300 px for a
# label): Open MainUI 1.0.10 shortens the ones that do not fit with "...".
# "Add to favorites" uses each language's own "Add favorite" (303), which
# also matches "Remove Favorite" in the same menu. Still too wide, kept as
# translated until a native speaker offers a shorter one (remove a line once
# its label fits; the test fails while a listed label already fits):
known_wide="ca:54 de:54 es:54 fr:54 gl:54 gr:52 gr:54 id:54 oc:54 pl:54
pt-BR:54 pt:54 ro:54"
result=$(python3 - "$ROOT/static/build/miyoo/app" "$known_wide" << 'EOF'
import json, pathlib, sys
try:
    from PIL import ImageFont
except ImportError:
    sys.exit(0)
app = pathlib.Path(sys.argv[1])
known = set(sys.argv[2].split())
ids = (51, 55, 52, 50, 405, 128, 27, 401, 402, 400, 403, 404, 406,
       54, 153, 18, 1, 2, 107, 15, 125, 407, 86)
wqy = ImageFont.truetype(str(app / "wqy-microhei.ttc"), 25)
noto = {"ar": ImageFont.truetype(str(app / "NotoSansArabicUI-Bold.ttf"), 25),
        "bn": ImageFont.truetype(str(app / "NotoSansBengaliUI-Bold.ttf"), 25)}
wide = set()
for p in sorted((app / "lang").glob("*.lang")):
    d = json.loads(p.read_text(encoding="utf-8-sig"))
    for key in ids:
        text = d.get(str(key), "")
        width = (noto[p.stem].getlength(text) if p.stem in noto
                 else wqy.getlength(text) + 2 * len(text))
        if width > 300:
            wide.add(f"{p.stem}:{key}")
print("new=" + ",".join(sorted(wide - known)))
print("fixed=" + ",".join(sorted(known - wide)))
EOF
)
new=$(echo "$result" | sed -n 's/^new=//p')
fixed=$(echo "$result" | sed -n 's/^fixed=//p')
check "SELECT menu labels fit a 320 px popup (too wide: ${new:-none})" test -z "$new"
check "the known-wide list is current (now fit: ${fixed:-none})" test -z "$fixed"
end

# ---- rescale_theme_list_icons.sh: wide spacer rule ----

begin rescale_keeps_only_wide_game_icon
RESCALE="$ROOT/static/build/.tmp_update/script/rescale_theme_list_icons.sh"
theme="$TMP/theme"
mkdir -p "$theme/skin"
# PNG signature and IHDR with the size: all the script reads.
fake_png() { # file width height
    python3 -c "import struct,sys; open(sys.argv[1],'wb').write(b'\x89PNG\r\n\x1a\n'+struct.pack('>I',13)+b'IHDR'+struct.pack('>II',int(sys.argv[2]),int(sys.argv[3]))+bytes(5))" "$1" "$2" "$3"
}
fake_png "$theme/skin/icon-game.png" 240 40
fake_png "$theme/skin/icon-folder.png" 240 40
fake_png "$theme/skin/ic-favorite-mark.png" 40 40
cat > "$TMP/pngScale" << EOF
#!/bin/sh
echo "\${1##*/}" >> "$TMP/scaled"
cp "\$1" "\$2"
EOF
chmod +x "$TMP/pngScale"
PNGRESIZE="$TMP/none" PNGSCALE="$TMP/pngScale" sh "$RESCALE" "$theme" 30 12 > /dev/null 2>&1
check "script succeeds" test $? -eq 0
check "wide icon-game.png kept as is" test "$(grep -c '^icon-game.png.bak$' "$TMP/scaled")" -eq 0
check "wide icon-folder.png resized" grep -q '^icon-folder.png.bak$' "$TMP/scaled"
check "favorite mark resized" grep -q '^ic-favorite-mark.png.bak$' "$TMP/scaled"
end

begin rescale_uses_pngresize
# The real pngresize, built for the host, on real PNGs.
theme="$TMP/theme"
mkdir -p "$theme/skin"
real_png() { # file width height: an opaque disc on transparency, RGBA
    python3 -c "
import struct, sys, zlib
w, h = int(sys.argv[2]), int(sys.argv[3])
rows = b''.join(b'\0' + b''.join(
    (b'\xff\0\0\xff' if (x - w / 2) ** 2 + (y - h / 2) ** 2 < (min(w, h) / 2.5) ** 2 else b'\0\0\0\0')
    for x in range(w)) for y in range(h))
chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
open(sys.argv[1], 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
    + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))" "$1" "$2" "$3"
}
png_wh() { python3 -c "import struct,sys; print('%dx%d' % struct.unpack('>II', open(sys.argv[1],'rb').read()[16:24]))" "$1"; }
real_png "$theme/skin/icon-game.png" 47 45
real_png "$theme/skin/icon-folder.png" 40 40
real_png "$theme/skin/ic-favorite-mark.png" 24 24
real_png "$theme/skin/bg-list-s.png" 640 60
if cc -O1 -w -o "$TMP/pngresize" "$ROOT/src/pngresize/pngresize.c" -lm 2> /dev/null; then
    PNGRESIZE="$TMP/pngresize" PNGSCALE="$TMP/none" sh "$RESCALE" "$theme" 30 12 > /dev/null 2>&1
    check "script succeeds with pngresize only" test $? -eq 0
    check "game icon fits, size rounded (47x45 -> $(png_wh "$theme/skin/icon-game.png"))" test "$(png_wh "$theme/skin/icon-game.png")" = "31x30"
    check "folder icon fits ($(png_wh "$theme/skin/icon-folder.png"))" test "$(png_wh "$theme/skin/icon-folder.png")" = "30x30"
    check "favorite mark already fits, kept ($(png_wh "$theme/skin/ic-favorite-mark.png"))" test "$(png_wh "$theme/skin/ic-favorite-mark.png")" = "24x24"
    check "row background stretched ($(png_wh "$theme/skin/bg-list-s_12.png"))" test "$(png_wh "$theme/skin/bg-list-s_12.png")" = "640x30"
    check "originals kept as .bak" test "$(png_wh "$theme/skin/icon-game.png.bak")" = "47x45"
else
    check "host C compiler for pngresize" false
fi
end

# ---- Tweaks: a pending icon resize also runs when Tweaks exits ----

begin tweaks_exit_runs_pending_icon_resize
TWEAKS_C="$ROOT/src/tweaks/tweaks.c"
# Leaving Game lists with B, and once more after the main loop (MENU closes
# Tweaks from any menu). The function returns at once when nothing is pending.
calls=$(grep -c 'gamelists_on_menu_exit();' "$TWEAKS_C")
after_loop=$(sed -n '/Clear the screen when exiting/,/settings_save();/p' "$TWEAKS_C" | grep -c 'gamelists_on_menu_exit();')
check "called on B and on exit ($calls calls)" test "$calls" -eq 2
check "called after the main loop, before settings are saved" test "$after_loop" -eq 1
check "does nothing without a pending resize" grep -q 'if (!gamelists_rescale_pending)' "$ROOT/src/tweaks/game_lists.h"
end

# ---- runtime.sh: start_warmup (MI video setup paid during boot) ----

eval "$(extract_fn "$RUNTIME" perf_now)"
eval "$(extract_fn "$RUNTIME" perf_begin)"
eval "$(extract_fn "$RUNTIME" perf_end)"
eval "$(extract_fn "$RUNTIME" start_warmup)"

warmup_env() {
    sysdir="$TMP/sys"
    miyoodir="$TMP/miyoo"
    mkdir -p "$sysdir/bin" "$sysdir/config" "$sysdir/logs"
    touch "$sysdir/config/.logging"
}

begin warmup_without_binary_does_nothing
warmup_env
start_warmup
check "returns success" test $? -eq 0
wait
check "nothing logged" test ! -s "$sysdir/logs/timing.log"
end

begin warmup_runs_in_background_with_miyoo_libs_first
warmup_env
cat > "$sysdir/bin/warmup" << 'EOF'
#!/bin/sh
echo "$LD_LIBRARY_PATH" > "${0%/bin/warmup}/ld_path"
sleep 1
EOF
chmod +x "$sysdir/bin/warmup"
perf_now
before=$perf_cs
start_warmup
perf_now
check "boot does not wait for it (took $((perf_cs - before)) cs)" test $((perf_cs - before)) -lt 50
wait
check "SDL from miyoo/lib, as MainUI loads it" test "$(cat "$sysdir/ld_path")" = "$miyoodir/lib:/config/lib:/lib"
check "timing.log records its time and exit code" grep -Eq '^warmup [0-9]+\.[0-9]+ s \(exit 0\)$' "$sysdir/logs/timing.log"
end

begin warmup_starts_after_colours_and_boot_screen
init_fn=$(extract_fn "$RUNTIME" init_system)
check "not in init_system any more" sh -c '! printf "%s\n" "$1" | grep -q "^ *start_warmup"' _ "$init_fn"
boot_fn=$(sed -n '/^main() {/,/^}/p' "$RUNTIME")
line_of() { printf '%s\n' "$boot_fn" | grep -n "^ *$1" | head -1 | cut -d: -f1; }
flip=$(line_of restore_display_colours)
blf=$(line_of "(")
screen=$(line_of 'bootScreen "Boot"')
warm=$(line_of start_warmup)
check "robcodedev's order: Flip colours, filter and DisplayCal, boot screen, warmup ($flip < $blf < $screen < $warm)" sh -c 'test "${1:-0}" -gt 0 -a "${2:-0}" -gt "${1:-0}" -a "${3:-0}" -gt "${2:-0}" -a "${4:-0}" -gt "${3:-0}"' _ "$flip" "$blf" "$screen" "$warm"
check "nothing waits for warmup" sh -c '! grep -q "kill -0 \"\$warmup_pid\"" "$1"' _ "$RUNTIME"
unset -f line_of
end

begin arcade_cores_ignore_power_as_r3
# The power button reaches RetroArch as R3, which the FB Alpha / FinalBurn
# cores use for Diagnostic: turning the console off during a game opened the
# test menu, still on screen after power on. Every core of the family ships
# a remap that unbinds R3, as CPS-2 has since Onion 4.1.
remaps="$ROOT/static/configs/Saves/CurrentProfile/config/remaps"
for core in "FB Alpha 2012" "FB Alpha 2012 CPS-1" "FB Alpha 2012 CPS-2" \
    "FB Alpha 2012 CPS-3" "FB Alpha 2012 Neo Geo" "FinalBurn Neo"; do
    check "$core: R3 unbound" has_line "$remaps/$core/$core.rmp" 'input_player1_btn_r3 = "-1"'
done
cores="$ROOT/static/build/RetroArch/.retroarch/cores"
for info in "$cores"/fbalpha2012*_libretro.info "$cores"/fbneo_libretro.info; do
    name=$(sed -n 's/^corename = "\(.*\)"$/\1/p' "$info")
    check "remap folder matches the core name: $name" test -f "$remaps/$name/$name.rmp"
done
begin wifi_starts_with_power_save_on
# Power save off kept the radio awake while idle: a Mini Plus in the menu
# with Wi-Fi connected drained about 20% faster and ran warm. Wi-Fi now
# starts with power save on, as Onion's stock MainUI ran with it, unless a
# file-transfer service is on (wifi_power_save_follows_transfer_services).
eval "$(extract_fn "$NETWORK" wifi_on)"
log() { :; }
sleep() { :; }
ifconfig() { :; }
udhcpc() { :; }
miyoodir_saved=${miyoodir-}
netscript_saved=${netscript-}
miyoodir=$TMP
netscript=$TMP/netscript
mkdir -p "$TMP/app" "$netscript"
printf '#!/bin/sh\n' > "$TMP/app/wpa_supplicant"
printf '#!/bin/sh\necho "$*" >> "%s/ps"\n' "$TMP" > "$netscript/wifi_power_save.sh"
chmod +x "$TMP/app/wpa_supplicant" "$netscript/wifi_power_save.sh"
wifi_on > /dev/null 2>&1
check "wifi_on sets power save by the services" has_line "$TMP/ps" "auto"
check "the OTA's Wi-Fi start does the same" grep -qF 'wifi_power_save.sh auto' "$OTA"
check "no script hardcodes power save" sh -c "! grep -rq 'set power_save o' \"$ROOT/static/build\" --exclude=wifi_power_save.sh"
miyoodir=$miyoodir_saved
netscript=$netscript_saved
unset -f log sleep ifconfig udhcpc wifi_on
end

begin pdf_reader_hint_says_menu_closes_a_pdf
# The list's "START to exit" made people think START also closes a PDF, but
# inside the Green reader MENU does (START toggles a view option), and some
# forced a shutdown. The PDF Reader passes its own hint; other callers (the
# video player) keep the default.
SEL="$ROOT/static/build/.tmp_update/script/file_selector.sh"
mkdir -p "$TMP/media"
: > "$TMP/media/a.pdf"
printf '#!/bin/sh\nwhile [ $# -gt 0 ]; do [ "$1" = -b ] && printf %%s "$2" > "%s/hint"; shift; done\necho "[ ] a.pdf"\n' "$TMP" > "$TMP/shellect"
chmod +x "$TMP/shellect"
sed "s#/mnt/SDCARD/.tmp_update/script/shellect.sh#$TMP/shellect#" "$SEL" > "$TMP/sel.sh"
sh "$TMP/sel.sh" "Videos" "$TMP/media" "pdf" > /dev/null 2>&1
check "default hint kept" has_line "$TMP/hint" "Start: Exit      A: Open"
check "the choice is written" grep -q "/a.pdf" "$TMP/media/.selected.pls"
sh "$TMP/sel.sh" "PDF Reader" "$TMP/media" "pdf" "Menu: Close PDF   Start: Exit   A: Open" > /dev/null 2>&1
check "a caller's hint is shown" has_line "$TMP/hint" "Menu: Close PDF   Start: Exit   A: Open"
check "the PDF Reader passes the MENU hint" grep -qF '"pdf" "Menu: Close PDF   Start: Exit   A: Open"' "$ROOT/static/packages/App/PDF Reader (Green)/App/Green/launch.sh"
end

begin media_apps_keys_do_what_the_list_says
# PDF Reader and Video Player end to end, with the real launch.sh,
# file_selector.sh and shellect.sh in a pseudo terminal. A fake "st" types
# the keys the device's terminal sends: A is a space, START a newline, MENU a
# lone Esc, the D-pad Esc [ A..D. The PDF reader and the player are stubs
# that note the file and return at once, like MENU closing a PDF.
# The hint says "A: open, START: exit": START used to reopen the last file,
# because the terminal now ends with 143, not 137, and the last choice was
# still on disk, so the only way out was to power off.
if command -v python3 > /dev/null 2>&1; then
    SD="$TMP/sd"
    mkdir -p "$SD/.tmp_update/script" "$SD/.tmp_update/bin/green" "$SD/Media/PDF" "$SD/Media/Videos" "$TMP/sys"
    sed "s#/mnt/SDCARD#$SD#g" "$ROOT/static/build/.tmp_update/script/file_selector.sh" > "$SD/.tmp_update/script/file_selector.sh"
    cp "$ROOT/static/build/.tmp_update/script/shellect.sh" "$SD/.tmp_update/script/shellect.sh"
    chmod +x "$SD/.tmp_update/script/file_selector.sh" "$SD/.tmp_update/script/shellect.sh"
    : > "$SD/Media/PDF/a.pdf"
    : > "$SD/Media/PDF/b.pdf"
    : > "$SD/Media/Videos/a.mp4"
    # The interpreter in the #! line itself (not env): quit_terminal looks for
    # a parent process named "st", and env would leave it named python3.
    {
        printf '#!%s\n' "$(command -v python3)"
        cat << 'EOF'
# st -q -e command...: run it in a pty and type the next scene's keys.
import fcntl, os, pty, select, struct, sys, termios, time
args = sys.argv[1:]
while args and args[0] in ("-q", "-e"):
    args = args[1:]
d = os.environ["FAKE_ST"]
n = int(open(d + "/scene").read()) + 1 if os.path.exists(d + "/scene") else 1
open(d + "/scene", "w").write(str(n))
if not os.path.exists(f"{d}/keys.{n}"):
    sys.exit(137)  # out of scenes: stop a launcher that would loop
keys = open(f"{d}/keys.{n}").read().split()
send = {"A": b" ", "START": b"\n", "MENU": b"\x1b", "UP": b"\x1b[A", "DOWN": b"\x1b[B"}
pid, fd = pty.fork()
if pid == 0:
    os.execvp(args[0], args)
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
def pump(t):
    end = time.time() + t
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try:
                os.read(fd, 4096)
            except OSError:
                return
pump(1.0)
for k in keys:
    os.write(fd, send[k])
    pump(0.6)
while True:
    done, status = os.waitpid(pid, os.WNOHANG)
    if done:
        break
    pump(0.1)
sys.exit(os.waitstatus_to_exitcode(status) & 0xFF)
EOF
    } > "$SD/.tmp_update/bin/st"
    chmod +x "$SD/.tmp_update/bin/st"
    printf '#!/bin/sh\necho "$1" >> "%s/opened"\n' "$TMP" > "$SD/.tmp_update/bin/green/green"
    printf '#!/bin/sh\nfor a; do last=$a; done\necho "$last" >> "%s/played"\nls "%s" > "%s/flags_while_playing"\n' "$TMP" "$TMP" "$TMP" > "$SD/.tmp_update/bin/ffplay"
    chmod +x "$SD/.tmp_update/bin/green/green" "$SD/.tmp_update/bin/ffplay"
    PDF_LAUNCH="$ROOT/static/packages/App/PDF Reader (Green)/App/Green/launch.sh"
    VID_LAUNCH="$ROOT/static/packages/App/Video Player (FFplay)/App/FFplay/launch.sh"
    sed -e "s#/mnt/SDCARD#$SD#g" "$PDF_LAUNCH" > "$TMP/pdf.sh"
    sed -e "s#/mnt/SDCARD#$SD#g" -e "s#/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor#$TMP/sys/governor#g" -e "s#/tmp/stay_awake#$TMP/stay_awake#g" -e "s#/tmp/video_playing#$TMP/video_playing#g" "$VID_LAUNCH" > "$TMP/video.sh"
    export FAKE_ST="$TMP/st"
    # The terminal list (the new UI draws it with prompt: see
    # file_selector_native_lists below)
    export LIST_TERMINAL=1
    scenes() { # keys for each time the list opens
        rm -rf "$FAKE_ST" "$TMP/opened" "$TMP/played"
        mkdir -p "$FAKE_ST"
        i=0
        for s; do
            i=$((i + 1))
            echo "$s" > "$FAKE_ST/keys.$i"
        done
    }
    lists_shown() { cat "$FAKE_ST/scene"; }

    # Open a.pdf (the list starts with ../), close it, then START.
    scenes "DOWN A" "START"
    with_limit 30 sh "$TMP/pdf.sh" > /dev/null 2>&1
    check "PDF: A opens the file under the cursor" test "$(cat "$TMP/opened" 2> /dev/null)" = "$SD/Media/PDF/a.pdf"
    check "PDF: START in the list leaves the app (no reopening)" test "$(lists_shown)" = 2

    # MENU in the list leaves too.
    scenes "MENU"
    with_limit 30 sh "$TMP/pdf.sh" > /dev/null 2>&1
    check "PDF: MENU in the list leaves the app" sh -c "test ! -f '$TMP/opened' && test \"\$(cat '$FAKE_ST/scene')\" = 1"

    # Two PDFs in a row, then START.
    scenes "DOWN A" "DOWN DOWN A" "START"
    with_limit 30 sh "$TMP/pdf.sh" > /dev/null 2>&1
    check "PDF: the second choice opens the second file" test "$(sed -n 2p "$TMP/opened")" = "$SD/Media/PDF/b.pdf"
    check "PDF: then START leaves" test "$(lists_shown)" = 3

    # Video Player: same list, same keys.
    scenes "DOWN A" "START"
    with_limit 30 sh "$TMP/video.sh" > /dev/null 2>&1
    check "Video: A plays the file under the cursor" test "$(cat "$TMP/played" 2> /dev/null)" = "$SD/Media/Videos/a.mp4"
    check "Video: START in the list leaves the app" test "$(lists_shown)" = 2
    # #350: while playing only video_playing (no sleep timer); stay_awake
    # would keep the video running in standby (POWER, Flip lid closed).
    check "Video: video_playing set while the video plays" grep -q "^video_playing$" "$TMP/flags_while_playing"
    check "Video: no stay_awake while the video plays" sh -c "! grep -q ^stay_awake\$ '$TMP/flags_while_playing'"
    check "Video: video_playing removed after the video" test ! -e "$TMP/video_playing"
    unset FAKE_ST
    unset -f scenes lists_shown
else
    skip "PDF / video viewer key tests (python3 not installed)"
fi
unset LIST_TERMINAL
end

begin file_selector_native_lists
# The new UI: file_selector.sh with SHELLECT_NATIVE, the list drawn by
# prompt (a stand-in answering from a list: the index, 255 for B).
SD="$TMP/sd"
mkdir -p "$SD/Media/PDF/Manuals" "$TMP/bin"
: > "$SD/Media/PDF/a.pdf"; : > "$SD/Media/PDF/b.pdf"; : > "$SD/Media/PDF/Manuals/m.pdf"
mkdir -p "$SD/Media/PDF/.state_seen"; : > "$SD/Media/PDF/.state_seen/b.pdf._seen"
{
    echo '#!/bin/sh'
    echo "for a; do printf '<%s>' \"\$a\"; done >> '$TMP/args'; echo >> '$TMP/args'"
    echo "n=\$(cat '$TMP/n' 2> /dev/null || echo 0); n=\$((n + 1)); echo \$n > '$TMP/n'"
    echo "a=\$(sed -n \"\${n}p\" '$TMP/answers')"
    # like the real prompt: the index in --index-file (-1 for B, menu)
    echo 'f=; while [ $# -gt 0 ]; do [ "$1" = --index-file ] && f=$2; shift; done'
    echo 'case $a in 255) i=-1 ;; 254) i=menu ;; *) i=$a ;; esac'
    echo '[ -n "$f" ] && echo "$i" > "$f"'
    echo 'exit $a'
} > "$TMP/bin/prompt"
chmod +x "$TMP/bin/prompt"
sed -e "s#/mnt/SDCARD/.tmp_update/script/shellect.sh#$TMP/shellect.sh#" "$ROOT/static/build/.tmp_update/script/file_selector.sh" > "$TMP/fs.sh"
sed -e "s#/mnt/SDCARD/.tmp_update/bin/prompt#$TMP/bin/prompt#" "$ROOT/static/build/.tmp_update/script/shellect.sh" > "$TMP/shellect.sh"
chmod +x "$TMP/shellect.sh"
fs_run() { # answers...
    rm -f "$TMP/args" "$TMP/n" "$SD/Media/PDF/.selected.pls"
    printf '%s\n' "$@" > "$TMP/answers"
    SHELLECT_NATIVE=1 sh "$TMP/fs.sh" "PDF Reader" "$SD/Media/PDF" "pdf"
}
# ../ is first, as in the terminal. Manuals/ (1), then B in it (255):
# back in PDF, then b.pdf (seen) (3)
fs_run 1 255 3
check "the app before the folder in the header" grep -q '<-p><PDF Reader><-t><PDF>' "$TMP/args"
check "folders, names, (seen) after an opened one" grep -q '<../><Manuals/><a.pdf><b.pdf (seen)>' "$TMP/args"
check "B in a folder: back up" test "$(sed -n 3p "$TMP/args" | grep -c '<-t><PDF>')" -eq 1
check "the file chosen, without (seen)" test "$(cat "$SD/Media/PDF/.selected.pls")" = "$SD/Media/PDF/b.pdf"
fs_run 255
check "B in the first folder: leaves, nothing chosen" test ! -f "$SD/Media/PDF/.selected.pls"
fs_run 2
check "a new one opened: seen next time" test -f "$SD/Media/PDF/.state_seen/a.pdf._seen"
# ../ above the first folder (0), then B: leaves (it went on up to / and
# looped there); the answers end with a choice that would be taken if not
fs_run 0 255 2
check "above the first folder, B leaves" test ! -f "$SD/Media/PDF/.selected.pls"
check "and asks no more ($(cat "$TMP/n"))" test "$(cat "$TMP/n")" -eq 2
end

begin release_named_redonion_with_onionplus_copy
# The project is RedOnion. Updaters before the rename (beta 8 and earlier)
# select an asset containing "OnionPlus-v", so every release also carries
# the same zip under that name; ota_update.sh takes either.
PRE="$ROOT/.github/workflows/pre-release.yml"
TAGGED="$ROOT/.github/workflows/tagged-release.yml"
check "the Makefile names the zip RedOnion" has_line "$ROOT/Makefile" "TARGET=RedOnion"
check "release tag and title are RedOnion" sh -c "grep -qF 'RELEASE_TAG=RedOnion-v' '$PRE' && grep -qF 'RELEASE_TITLE=RedOnion V' '$PRE'"
check "the release zip is RedOnion-v" grep -qF 'ZIP_NAME=RedOnion-v${BUILD_VERSION}-${SHA_SHORT}.zip' "$PRE"
check "an OnionPlus-v copy is made" grep -qF 'cp "release/${ZIP_NAME}" "release/${LEGACY_ZIP_NAME}"' "$PRE"
check "the copy keeps the same version" grep -qF 'LEGACY_ZIP_NAME=OnionPlus-v${BUILD_VERSION}-${SHA_SHORT}.zip' "$PRE"
check "both zips are published" sh -c "grep -qF 'release/\${{ env.ZIP_NAME }}' '$PRE' && grep -qF 'release/\${{ env.LEGACY_ZIP_NAME }}' '$PRE'"
check "tagged releases carry both too" sh -c "grep -qF 'release/RedOnion-v' '$TAGGED' && grep -qF 'release/OnionPlus-v' '$TAGGED'"
check "the updater takes both names" has_line "$OTA" 'PACKAGE_PREFIXES="RedOnion-v OnionPlus-v"'
end

begin netplay_runs_with_power_save_off
# With power save on, a ping to an idle Mini Plus averaged 40 ms with peaks
# of 220 ms (3.5 ms, max 15 with it off). Netplay turns it off right before
# each RetroArch launch (after any Wi-Fi restart, which resets it to on) and
# Netplay.sh turns it on again when the session ends.
PS="$ROOT/static/build/.tmp_update/script/network/wifi_power_save.sh"
NETPLAY_DIR="$ROOT/static/build/.tmp_update/script/netplay"
NETPLAY="$ROOT/static/build/App/romscripts/Netplay.sh"
launches=0
unguarded=0
for f in "$NETPLAY_DIR"/*.sh; do
    n=$(grep -c '^[[:space:]]*HOME=/mnt/SDCARD/RetroArch \./retroarch ' "$f")
    launches=$((launches + n))
    bad=$(awk '
        /^[[:space:]]*#/ || /^[[:space:]]*$/ { next }
        /^[[:space:]]*HOME=\/mnt\/SDCARD\/RetroArch \.\/retroarch / {
            if (prev !~ /wifi_power_save\.sh" off$/) n++
        }
        { prev = $0 }
        END { print n + 0 }' "$f")
    unguarded=$((unguarded + bad))
done
check "every netplay script launches RetroArch (8 launches)" [ "$launches" -eq 8 ]
check "each launch turns power save off first" [ "$unguarded" -eq 0 ]
check "Netplay.sh hands power save back to the services at the end" sh -c \
    "grep -v '^[[:space:]]*\$' \"$NETPLAY\" | tail -n 2 | tr '\n' ' ' | grep -q '^\"\\\$sysdir/script/network/wifi_power_save.sh\" auto netplay_end \$'"
check "only netplay turns power save off" sh -c \
    "! grep -rl 'wifi_power_save.sh\" off' \"$ROOT/static/build\" | grep -v '/script/netplay/'"
check "the helper is executable" [ -x "$PS" ]

mkdir -p "$TMP/sys/bin" "$TMP/net/wlan0"
printf '#!/bin/sh\necho "$*" >> "%s/iw"\necho "$LD_LIBRARY_PATH" > "%s/ld"\n' "$TMP" "$TMP" > "$TMP/sys/bin/iw"
chmod +x "$TMP/sys/bin/iw"
sysdir="$TMP/sys" WIFI_NET_DIR="$TMP/net" sh "$PS" off
check "off: iw sets wlan0 power save off" has_line "$TMP/iw" "dev wlan0 set power_save off"
check "iw finds its libraries" grep -q "$TMP/sys/lib/parasyte" "$TMP/ld"
rm -f "$TMP/iw"
sysdir="$TMP/sys" WIFI_NET_DIR="$TMP/net" sh "$PS" on
check "on: iw sets wlan0 power save on" has_line "$TMP/iw" "dev wlan0 set power_save on"
check "an absent wlan1 is left alone" sh -c "! grep -q wlan1 \"$TMP/iw\""
rm -f "$TMP/iw"
sysdir="$TMP/sys" WIFI_NET_DIR="$TMP/net" sh "$PS" maybe 2> /dev/null
check "a wrong argument fails" [ $? -eq 2 ]
check "and runs nothing" [ ! -e "$TMP/iw" ]
rmdir "$TMP/net/wlan0"
sysdir="$TMP/sys" WIFI_NET_DIR="$TMP/net" sh "$PS" off
check "Wi-Fi off: nothing to do, no error" [ $? -eq 0 ]
check "and iw is not called" [ ! -e "$TMP/iw" ]
end

begin wifi_power_save_follows_transfer_services
# With power save on, FTP transfers from a phone stuttered and new files
# took about a minute to show (Reddit, beta 10). Power save is now off while
# a file-transfer service is on in Tweaks > Network, on once they are all
# off or paused for a game.
PS="$ROOT/static/build/.tmp_update/script/network/wifi_power_save.sh"
mkdir -p "$TMP/ps/bin" "$TMP/ps/config" "$TMP/ps/net/wlan0"
printf '#!/bin/sh\necho "$*" > "%s/ps/iw"\n' "$TMP" > "$TMP/ps/bin/iw"
chmod +x "$TMP/ps/bin/iw"
ps_auto() {
    rm -f "$TMP/ps/iw"
    sysdir="$TMP/ps" WIFI_NET_DIR="$TMP/ps/net" SERVICES_PAUSED_FLAG="$TMP/ps/paused" sh "$PS" auto
    cat "$TMP/ps/iw" 2> /dev/null
}
check "no service on: power save on" [ "$(ps_auto)" = "dev wlan0 set power_save on" ]
for service in ftp ssh http smbd telnet; do
    : > "$TMP/ps/config/.${service}State"
    check "$service on: power save off" [ "$(ps_auto)" = "dev wlan0 set power_save off" ]
    mv "$TMP/ps/config/.${service}State" "$TMP/ps/config/.${service}State_"
    check "$service switched off in Tweaks (.${service}State_): on again" [ "$(ps_auto)" = "dev wlan0 set power_save on" ]
done
: > "$TMP/ps/config/.ntpState"
: > "$TMP/ps/config/.authftpState"
check "time sync and service passwords do not count" [ "$(ps_auto)" = "dev wlan0 set power_save on" ]
: > "$TMP/ps/config/.ftpState"
: > "$TMP/ps/paused"
check "services paused for a game: on" [ "$(ps_auto)" = "dev wlan0 set power_save on" ]
rm -f "$TMP/ps/paused"
check "game over, FTP back: off" [ "$(ps_auto)" = "dev wlan0 set power_save off" ]
rmdir "$TMP/ps/net/wlan0"
check "Wi-Fi off: iw is not called" [ -z "$(ps_auto)" ]

# Where it is called
check "a service toggled in Tweaks sets it" sh -c \
    "sed -n '/^        ftp | telnet | http | ssh | smbd)/,/^            ;;/p' \"$NETWORK\" | sed -n '/toggle)/,/;;/p' | grep -qF 'wifi_power_save.sh auto'"
check "disableall sets it" sh -c "extract_fn() { sed -n \"/^\$2() {/,/^}/p\" \"\$1\"; }; extract_fn \"$NETWORK\" disable_all_services | grep -qF 'wifi_power_save.sh auto'"
check "services started after Wi-Fi or a game set it" sh -c "sed -n '/^start_services_outside_game() {/,/^}/p' \"$NETWORK\" | grep -qF 'wifi_power_save.sh auto'"
check "a game that pauses the services sets it" sh -c "grep -A12 ': > /tmp/services_paused_in_game' \"$RUNTIME\" | grep -qF 'wifi_power_save.sh auto'"

unset -f ps_auto
end

begin update_cleanup_keeps_ssh_host_key
# The update cleanup in miyoo/app/MainUI removed .tmp_update/etc, so dropbear
# generated a new host key on every update and SSH/SFTP clients warned (#292).
mainui="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}/static/dist/miyoo/app/MainUI"
cleanup=$(grep -e '-maxdepth 0' "$mainui")
mkdir -p "$TMP/sys/etc/dropbear" "$TMP/sys/bin" "$TMP/sys/config" "$TMP/sys/onionVersion"
echo key > "$TMP/sys/etc/dropbear/dropbear_ed25519_host_key"
(cd "$TMP/sys" && eval "$cleanup")
check "the SSH host key survives" test -f "$TMP/sys/etc/dropbear/dropbear_ed25519_host_key"
check "config and onionVersion survive" test -d "$TMP/sys/config" -a -d "$TMP/sys/onionVersion"
check "old system files are removed" test ! -e "$TMP/sys/bin"
end

begin update_trigger_leaves_the_reboot_to_the_clean_shutdown
# The trigger (miyoo/app/MainUI) runs the installer's updater; install.sh
# reboots through bin/shutdown, which kills updater first. The trigger
# must not reboot plainly then, before the card is unmounted.
mainui="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}/static/dist/miyoo/app/MainUI"
mkdir -p "$TMP/stub"
for c in reboot sleep; do
    printf '#!/bin/sh\necho "%s $*" >> "%s/log"\n' "$c" "$TMP" > "$TMP/stub/$c"
    chmod +x "$TMP/stub/$c"
done
trigger_run() { # clean|plain|missing: the trigger run on a stand-in card
    rm -rf "$TMP/sd" "$TMP/_shutdown" "$TMP/log"
    mkdir -p "$TMP/sd/miyoo/app/.tmp_update" "$TMP/sd/.tmp_update/bin" "$TMP/sd/.tmp_update/config"
    sed -e "s#/mnt/SDCARD#$TMP/sd#g" -e "s#/tmp/_shutdown#$TMP/_shutdown#g" "$mainui" > "$TMP/sd/miyoo/app/MainUI"
    if [ "$1" != missing ]; then
        touch "$TMP/sd/miyoo/app/.tmp_update/install.sh" "$TMP/sd/miyoo/app/.tmp_update/onion.pak"
    fi
    # The installer's updater: with the clean shutdown, bin/shutdown has
    # copied itself to /tmp/_shutdown and killed updater
    if [ "$1" = clean ]; then
        printf '#!/bin/sh\necho updater >> "%s/log"\ntouch "%s/_shutdown"\nexit 137\n' "$TMP" "$TMP"
    else
        printf '#!/bin/sh\necho updater >> "%s/log"\n' "$TMP"
    fi > "$TMP/sd/miyoo/app/.tmp_update/updater"
    chmod +x "$TMP/sd/miyoo/app/.tmp_update/updater"
    PATH="$TMP/stub:$PATH" sh "$TMP/sd/miyoo/app/MainUI" > /dev/null 2>&1
    tr '\n' ',' < "$TMP/log"
}
check "clean shutdown started: wait for it, plain reboot only after 30 s" \
    test "$(trigger_run clean)" = "updater,sleep 30,reboot ,sleep 10,"
check "no clean shutdown (failed installation): plain reboot at once" \
    test "$(trigger_run plain)" = "updater,reboot ,sleep 10,"
check "installer files missing: reboot and stop" \
    test "$(trigger_run missing)" = "reboot ,sleep 10,"
check "installer files missing: the installed system is left alone" \
    test -d "$TMP/sd/.tmp_update/bin"
unset -f trigger_run
end

begin cores_have_systemid
# The GameSwitcher sends RetroArch GET_STATUS, answered with the core's
# systemid: a core whose .info had none crashed RetroArch with error 139 (#283).
cores="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}/static/build/RetroArch/.retroarch/cores"
missing=""
for so in "$cores"/*_libretro.so; do
    grep -q '^systemid = "..*"' "${so%.so}.info" 2> /dev/null || missing="$missing ${so##*/}"
done
check "every core shipped has a systemid${missing:+:$missing}" test -z "$missing"
end

begin migration_renames_the_old_ota_app_label
# A package update keeps the label already installed: "Onion OTA update"
# stayed in Apps on installs from before the rename. Only that is changed.
MIG25="$ROOT/static/build/.tmp_update/script/migration/00025_ota_app_redonion_label.sh"
printf '{\n\t"label": "Onion OTA update",\n\t"launch": "launch.sh"\n}\n' > "$TMP/old.json"
OTA_APP_CONFIG="$TMP/old.json" sh "$MIG25"
check "old default renamed" grep -q '"label": "RedOnion OTA update"' "$TMP/old.json"
check "the rest kept" grep -q '"launch": "launch.sh"' "$TMP/old.json"
printf '{\n\t"label": "My updater",\n\t"launch": "launch.sh"\n}\n' > "$TMP/mine.json"
OTA_APP_CONFIG="$TMP/mine.json" sh "$MIG25"
check "a name the user chose is kept" grep -q '"label": "My updater"' "$TMP/mine.json"
OTA_APP_CONFIG="$TMP/none.json" sh "$MIG25"; rc=$?
check "no app: nothing to do" test $rc -eq 0 -a ! -e "$TMP/none.json"
end

begin migration_adds_core_systemid
mig="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}/static/build/.tmp_update/script/migration/00024_core_info_systemid.sh"
mkdir -p "$TMP/cores"
printf 'corename = "GW"\nsystemname = "Handheld Electronic"' > "$TMP/cores/gw_libretro.info"
printf 'systemid = "game_boy_advance"\n' > "$TMP/cores/gpsp_libretro.info"
touch "$TMP/cores/core_info.cache"
CORES_DIR="$TMP/cores" sh "$mig"
CORES_DIR="$TMP/cores" sh "$mig"
check "gw gets a systemid" has_line "$TMP/cores/gw_libretro.info" 'systemid = "handheld_electronic"'
check "added once" test "$(grep -c systemid "$TMP/cores/gw_libretro.info")" = 1
check "the last line stays intact" has_line "$TMP/cores/gw_libretro.info" 'systemname = "Handheld Electronic"'
check "a core with a systemid is left alone" test "$(cat "$TMP/cores/gpsp_libretro.info")" = 'systemid = "game_boy_advance"'
check "the core info cache is removed" test ! -e "$TMP/cores/core_info.cache"
end

begin installer_version_ignores_files
# Like get_version in ota_update.sh (#297): unquoted, [:alpha:] matched a file
# named a, l, p, h or : in the current directory and v4.4.0 read as 0004000000.
eval "$(extract_fn "$INSTALLER" version)"
mkdir -p "$TMP/glob" && touch "$TMP/glob/a"
check "letters dropped with a file named a in the directory" test "$(cd "$TMP/glob" && version v4.4.0)" = 4004000000
unset -f version
end

begin migration_saved_games_overwrite_default
mig="$ROOT/static/build/.tmp_update/script/migration/00028_saved_games_overwrite_default.sh"
GM="$TMP/cfg/game-menu.json"
run_mig() { GAME_MENU_CONFIG="$GM" JQ=jq sh "$mig"; }
rm -rf "$TMP/cfg"
run_mig
check "no game-menu.json: created with overwrite" test "$(jq -r .saved_games.resume_mode "$GM")" = overwrite
printf '{\n\t"games":\t{\n\t\t"reset":\tfalse\n\t},\n\t"saved_games":\t{\n\t\t"layout":\t"overlay"\n\t}\n}\n' > "$GM"
run_mig
check "other keys kept, overwrite added" test "$(jq -c . "$GM")" = '{"games":{"reset":false},"saved_games":{"layout":"overlay","resume_mode":"overwrite"}}'
check "written with tabs, as cJSON does" grep -q '^	"games":' "$GM"
echo '{"saved_games": {"resume_mode": "swap"}}' > "$GM"
run_mig
check "a choice already there stays (swap)" test "$(jq -r .saved_games.resume_mode "$GM")" = swap
echo '{"saved_games": {"resume_mode": "backup"}}' > "$GM"
run_mig
check "a choice already there stays (backup)" test "$(jq -r .saved_games.resume_mode "$GM")" = backup
echo '{"roms": {"reset": false}}' > "$GM"
run_mig
check "no saved_games section: added" test "$(jq -c .saved_games "$GM")" = '{"resume_mode":"overwrite"}'
printf '{\n// mine\n"saved_games": {}\n}\n' > "$GM"
run_mig
check "a file with comments is left alone" grep -q '// mine' "$GM"
echo '{"saved_games": 5}' > "$GM"
run_mig
check "a saved_games that is not an object is left alone" test "$(cat "$GM")" = '{"saved_games": 5}'
check "no temporary file left" test ! -e "$GM.tmp"
end

begin migration_game_info_auto_scroll_default
mig="$ROOT/static/build/.tmp_update/script/migration/00029_game_info_auto_scroll_default.sh"
GI="$TMP/cfg/.gameInfoAutoScroll"
run_mig() { GAME_INFO_SCROLL_CONFIG="$GI" sh "$mig"; }
rm -rf "$TMP/cfg"
run_mig
check "no file (Off, or never set): on at 2000 ms, 20 px/s" test "$(cat "$GI")" = "2000,20"
echo "500,75" > "$GI"
run_mig
check "the user's delay and speed stay" test "$(cat "$GI")" = "500,75"
: > "$GI"
run_mig
check "an empty file (Open MainUI's defaults) stays" test ! -s "$GI"
check "not in configs.pak's files: an update would add it back after Off" test ! -e "$ROOT/static/configs/.tmp_update/config/.gameInfoAutoScroll"
check "the format Tweaks writes and reads (delay,speed)" grep -q 'gamelists_write_pair(GAMELISTS_INFO_SCROLL_PATH' "$ROOT/src/tweaks/game_lists_core.h"
check "the next migration number" test "$(ls "$ROOT/static/build/.tmp_update/script/migration/" | grep -c '^00029_')" -eq 1
rm -rf "$TMP/cfg"
end

# ---- Backup app (App/Backup/launch.sh) ----
#
# The functions are loaded from the app itself (BACKUP_NO_MAIN skips its
# menu). The tests that make and restore zips need a 7-Zip on the PC
# (7zz, 7z or 7za, or BACKUP_TEST_7Z); without one they are skipped.

BACKUP_APP="$ROOT/static/packages/App/Backup (Saves and settings)/App/Backup/launch.sh"
backup_load() { # SD folder
    BACKUP_SD=$1 BACKUP_NO_MAIN=1 BACKUP_7Z=${BACKUP_TEST_7Z:-7z}
    export BACKUP_SD BACKUP_NO_MAIN BACKUP_7Z
    . "$BACKUP_APP"
    LOG=$TMP/backup.log
}
not_allowed() { ! allowed_path "$1"; }
not_guest_archive() { ! archive_guest_mode_on "$1"; }
zip_line_is() { # zip file, path in it, its one line
    [ -f "$1" ] && test "$("$BACKUP_TEST_7Z" x -so "$1" "$2" 2> /dev/null)" = "$3"
}
backup_7z=${BACKUP_TEST_7Z:-$(command -v 7zz || command -v 7z || command -v 7za)}

begin backup_restore_tells_keymon_to_read_the_settings
backup_load "$TMP/sd"
BACKUP_SETTINGS_CHANGED=$TMP/settings_changed
SETTINGS_CHANGED=$BACKUP_SETTINGS_CHANGED
mkdir -p "$TMP/sd/.tmp_update/config"
check_archive() { :; }
archive_guest_mode_on() { return 1; }
guest_mode_on() { return 1; }
make_backup() { BACKUP_FILE=$TMP/before.zip; return 0; }
restore_flag_pairs() { :; }
archive_paths() { :; }
fake7z() { return "${FAKE7Z_RC:-0}"; }
Z7=fake7z
export FAKE7Z_RC=2; restore_backup "$TMP/old.zip"; _rc=$?; unset FAKE7Z_RC; rc $_rc
check "extraction failed: keymon not told" test ! -e "$TMP/settings_changed"
export FAKE7Z_RC=0; restore_backup "$TMP/old.zip"; _rc=$?; unset FAKE7Z_RC; rc $_rc
check "restored: keymon reads the settings again" test -e "$TMP/settings_changed"
check "and the restart message no longer warns about volume or brightness" sh -c "! grep -q \"Don't change the volume\" '$BACKUP_APP'"
unset -f check_archive archive_guest_mode_on guest_mode_on make_backup restore_flag_pairs archive_paths fake7z
end

begin backup_restore_needs_room_for_new_files
backup_load "$TMP/sd"
SETTINGS_CHANGED=$TMP/settings_changed
mkdir -p "$TMP/sd/Saves/CurrentProfile/saves" "$TMP/sd/Backups"
echo old > "$TMP/sd/Saves/CurrentProfile/saves/there.srm"
check_archive() { :; }
archive_guest_mode_on() { return 1; }
guest_mode_on() { return 1; }
make_backup() { BACKUP_FILE=$TMP/sd/Backups/before.zip; : > "$BACKUP_FILE"; return 0; }
restore_flag_pairs() { :; }
archive_paths() { :; }
fake7z() {
    case $1 in
        l) printf 'Path = Saves\nFolder = +\nSize = 0\n\nPath = Saves/CurrentProfile/saves/there.srm\nFolder = -\nSize = 900000000\n\nPath = Saves/CurrentProfile/saves/new.srm\nFolder = -\nSize = 20000000\n\nPath = Saves/CurrentProfile/saves/tiny.srm\nFolder = -\nSize = 10\n' ;;
        x) echo extracted >> "$TMP/x" ;;
    esac
}
Z7=fake7z
check "new files only, each at least a cluster" test "$(restore_new_kb "$TMP/old.zip")" = $((19552 + 32))
free_kb() { echo 10000; }
restore_backup "$TMP/old.zip"
check "too little room: not restored" test $? -eq 1 -a ! -e "$TMP/x"
check "says so" sh -c 'printf "%s" "$1" | grep -q "Nothing was restored"' _ "$RESTORE_ERROR"
check "the copy made before is not kept" test ! -e "$TMP/sd/Backups/before.zip"
free_kb() { echo 30000; }
restore_backup "$TMP/old.zip"
check "room enough (the file written over not counted): restored" test $? -eq 0 -a -e "$TMP/x"
unset -f check_archive archive_guest_mode_on guest_mode_on make_backup restore_flag_pairs archive_paths fake7z free_kb
end

begin backup_allowed_paths
backup_load "$TMP/sd"
for p in Saves Saves/CurrentProfile/saves/Pokemon.srm Saves/GuestProfile/states/x.state0 \
    .tmp_update .tmp_update/config .tmp_update/config/.logging .tmp_update/config/system/abc.json \
    system.json RetroArch RetroArch/.retroarch RetroArch/.retroarch/retroarch.cfg \
    Emu/NDS/backup/Mario.dsv Emu/NDS/savestates/Mario_0.dss Emu/NDS/config/drastic.cfg \
    Emu/NDS/resources/settings.json Roms Roms/favourite.json "Roms/recentlist-hidden.json"; do
    check "allowed: $p" allowed_path "$p"
done
for p in "" /etc/passwd ../x Saves/../.tmp_update/runtime.sh Saves/./x .tmp_update/runtime.sh \
    .tmp_update/bin/7z RetroArch/retroarch RetroArch/.retroarch/cores/x.so Emu/GBA/launch.sh \
    Emu/NDS/launch.sh Roms/GBA/game.gba Roms/GBA/x.json Roms/game.gba App/Tweaks/launch.sh BIOS/gba_bios.bin; do
    check "refused: '$p'" not_allowed "$p"
done
end

begin backup_labels_and_list
backup_load "$TMP/sd"
check "plain" test "$(backup_label RedOnion_backup_2026-11-10_09-30-15.zip)" = "2026-11-10 09:30:15"
check "before restore" test "$(backup_label RedOnion_backup_2026-11-10_09-30-15_before-restore.zip)" = "2026-11-10 09:30:15 (before restore)"
check "same second" test "$(backup_label RedOnion_backup_2026-11-10_09-30-15_2.zip)" = "2026-11-10 09:30:15 (2)"
mkdir -p "$TMP/sd/Backups"
for f in RedOnion_backup_2026-01-02_10-00-00.zip RedOnion_backup_2026-11-10_09-30-15.zip \
    RedOnion_backup_2025-12-31_23-59-59.zip .RedOnion_backup_2026-12-01_00-00-00.zip.part \
    other.zip RedOnion_backup_notes.txt; do
    : > "$TMP/sd/Backups/$f"
done
check "newest first, only finished backups" test "$(list_backups | tr '\n' ' ')" = \
    "RedOnion_backup_2026-11-10_09-30-15.zip RedOnion_backup_2026-01-02_10-00-00.zip RedOnion_backup_2025-12-31_23-59-59.zip "
end

begin backup_items_only_what_exists
mkdir -p "$TMP/sd/Saves/CurrentProfile/saves" "$TMP/sd/.tmp_update/config" "$TMP/sd/Roms/GBA"
: > "$TMP/sd/Roms/favourite.json"
: > "$TMP/sd/Roms/GBA/list.json"
backup_load "$TMP/sd"
check "existing items, top Roms json only" test "$(backup_items | tr '\n' ' ')" = "Saves .tmp_update/config Roms/favourite.json "
mkdir -p "$TMP/sd/Saves/CurrentProfile/romScreens"
head -c 300000 /dev/urandom > "$TMP/sd/Saves/CurrentProfile/romScreens/big.png"
head -c 20000 /dev/urandom > "$TMP/sd/Saves/CurrentProfile/saves/a.srm"
kb=$(items_kb Saves)
check "size without the screenshots ($kb KiB)" test "$kb" -ge 16 -a "$kb" -lt 100
rm -rf "$TMP/sd"
mkdir -p "$TMP/sd"
check "nothing there: no items" test -z "$(backup_items)"
end

# The screens, with the menus answered from a list: ui_choose gives the next
# answer and notes its title and choices; ui_info notes its title.
backup_ui_stubs() { # answers...
    echo "$*" | tr ' ' '\n' > "$TMP/answers"
    : > "$TMP/ui"
    ui_choose() {
        printf 'CHOOSE %s | %s\n' "$1" "$(shift 2; echo "$*")" >> "$TMP/ui"
        a=$(head -n 1 "$TMP/answers")
        sed -i 1d "$TMP/answers"
        return "${a:-255}"
    }
    ui_info() { printf 'INFO %s\n' "$1" >> "$TMP/ui"; }
    ui_busy() { printf 'BUSY %s\n' "$1" >> "$TMP/ui"; }
    ui_busy_end() { :; }
}

begin backup_delete_screen
SD=$TMP/sd
mkdir -p "$SD/Backups"
: > "$SD/Backups/RedOnion_backup_2026-01-01_10-00-00.zip"
: > "$SD/Backups/RedOnion_backup_2026-02-01_10-00-00.zip"
backup_load "$SD"
backup_ui_stubs 1 0
screen_delete
check "list titled" grep -q "^CHOOSE DELETE A BACKUP | 2026-02-01 10:00:00 2026-01-01 10:00:00$" "$TMP/ui"
check "cancel keeps it" test -f "$SD/Backups/RedOnion_backup_2026-01-01_10-00-00.zip"
backup_ui_stubs 1 1
screen_delete
check "asked first" grep -q "^CHOOSE DELETE A BACKUP | Cancel Delete$" "$TMP/ui"
check "the second one deleted" test ! -e "$SD/Backups/RedOnion_backup_2026-01-01_10-00-00.zip"
check "the other kept" test -f "$SD/Backups/RedOnion_backup_2026-02-01_10-00-00.zip"
backup_ui_stubs 255
screen_delete
check "B on the list: nothing asked" test "$(grep -c CHOOSE "$TMP/ui")" -eq 1
rm -f "$SD/Backups/"*
backup_ui_stubs
screen_delete
check "none: says so" grep -q "^INFO DELETE A BACKUP$" "$TMP/ui"
end

# A small SD card: saves, settings and lists, and things a backup leaves out.
backup_fake_sd() { # folder
    mkdir -p "$1/Saves/CurrentProfile/saves/gpSP" "$1/Saves/CurrentProfile/states" \
        "$1/Saves/CurrentProfile/romScreens" "$1/Saves/CurrentProfile/config/remaps" \
        "$1/.tmp_update/config/system" "$1/.tmp_update/bin" "$1/RetroArch/.retroarch" \
        "$1/Roms/GBA" "$1/Emu/NDS/backup" "$1/Emu/NDS/config" "$1/BIOS"
    echo "save 1" > "$1/Saves/CurrentProfile/saves/gpSP/Pokemon Emerald (USA).srm"
    echo "state 1" > "$1/Saves/CurrentProfile/states/game.state0"
    echo "screen" > "$1/Saves/CurrentProfile/romScreens/game.png"
    echo "1700000000" > "$1/Saves/CurrentProfile/saves/currentTime.txt"
    echo "remap" > "$1/Saves/CurrentProfile/config/remaps/x.rmp"
    : > "$1/.tmp_update/config/.logging"
    echo '{"vol": 10}' > "$1/.tmp_update/config/system/OLD.json"
    echo "binary" > "$1/.tmp_update/bin/tool"
    echo '{"vol": 15, "theme": "a"}' > "$1/system.json"
    echo 'video_smooth = "true"' > "$1/RetroArch/.retroarch/retroarch.cfg"
    echo '{"fav": 1}' > "$1/Roms/favourite.json"
    echo "rom" > "$1/Roms/GBA/game.gba"
    echo "nds save" > "$1/Emu/NDS/backup/Mario.dsv"
    echo "cfg" > "$1/Emu/NDS/config/drastic.cfg"
    mkdir -p "$1/.tmp_update/config/syncthing/index"
    echo "key 1" > "$1/.tmp_update/config/syncthing/key.pem"
    echo "db" > "$1/.tmp_update/config/syncthing/index/000001.log"
    echo "bios" > "$1/BIOS/bios.bin"
}

if [ -z "$backup_7z" ]; then
    skip "backup zip tests: no 7-Zip on this PC (set BACKUP_TEST_7Z)"
else
    BACKUP_TEST_7Z=$backup_7z

    begin backup_make_and_restore
    SD=$TMP/sd
    backup_fake_sd "$SD"
    backup_load "$SD"
    read_uuid() { echo DEVICE1; }
    mkdir -p "$SD/Backups"
    : > "$SD/Backups/.RedOnion_backup_2026-01-01_10-00-00.zip.part"
    : > "$SD/.tmp_update/config/theme-applied-ABC"
    make_backup ""
    rc=$?
    check "made" test $rc -eq 0
    check "a part file left by a cut-short backup is removed" test ! -e "$SD/Backups/.RedOnion_backup_2026-01-01_10-00-00.zip.part"
    check "in Backups" test -f "$BACKUP_FILE" -a "$(dirname "$BACKUP_FILE")" = "$SD/Backups"
    check "no part file left" test -z "$(ls -A "$SD/Backups" | grep part)"
    paths=$(archive_paths "$BACKUP_FILE")
    for p in "Saves/CurrentProfile/saves/gpSP/Pokemon Emerald (USA).srm" Saves/CurrentProfile/states/game.state0 \
        Saves/CurrentProfile/config/remaps/x.rmp .tmp_update/config/.logging system.json \
        RetroArch/.retroarch/retroarch.cfg Roms/favourite.json Emu/NDS/backup/Mario.dsv Emu/NDS/config/drastic.cfg; do
        check "has $p" sh -c 'printf "%s\n" "$1" | { while IFS= read -r l; do [ "$l" = "$2" ] && exit 0; done; exit 1; }' _ "$paths" "$p"
    done
    for p in Saves/CurrentProfile/romScreens/game.png Saves/CurrentProfile/saves/currentTime.txt \
        Roms/GBA/game.gba .tmp_update/bin/tool BIOS/bios.bin .tmp_update/config/syncthing/key.pem \
        .tmp_update/config/syncthing/index/000001.log; do
        check "not $p" sh -c 'printf "%s\n" "$1" | { while IFS= read -r l; do [ "$l" = "$2" ] && exit 1; done; exit 0; }' _ "$paths" "$p"
    done
    check "every path may go back" check_archive "$BACKUP_FILE"
    first=$BACKUP_FILE

    # Things change, then the backup goes back.
    echo "save 2" > "$SD/Saves/CurrentProfile/saves/gpSP/Pokemon Emerald (USA).srm"
    rm -f "$SD/.tmp_update/config/.logging"
    echo '{"vol": 3, "theme": "b"}' > "$SD/system.json"
    echo "new" > "$SD/Saves/CurrentProfile/saves/new.srm"
    restore_backup "$first"
    rc=$?
    check "restored" test $rc -eq 0
    check "save back" test "$(cat "$SD/Saves/CurrentProfile/saves/gpSP/Pokemon Emerald (USA).srm")" = "save 1"
    check "flag back" test -f "$SD/.tmp_update/config/.logging"
    check "system.json back" test "$(cat "$SD/system.json")" = '{"vol": 15, "theme": "a"}'
    check "and this device's boot copy" test "$(cat "$SD/.tmp_update/config/system/DEVICE1.json")" = '{"vol": 15, "theme": "a"}'
    check "a file not in the backup stays" test "$(cat "$SD/Saves/CurrentProfile/saves/new.srm")" = new
    check "the ROM is untouched" test "$(cat "$SD/Roms/GBA/game.gba")" = rom
    check "the theme's icons are applied again at the next boot" test ! -e "$SD/.tmp_update/config/theme-applied-ABC"
    before=$(list_backups | grep _before-restore)
    check "the state before was backed up" test -n "$before"
    check "with the changed save" zip_line_is "$SD/Backups/$before" "Saves/CurrentProfile/saves/gpSP/Pokemon Emerald (USA).srm" "save 2"
    end

    begin backup_restore_screen
    SD=$TMP/sd
    backup_fake_sd "$SD"
    backup_load "$SD"
    read_uuid() { echo DEVICE1; }
    printf '#!/bin/sh\necho "$*" > "%s/shutdown_args"\n' "$TMP" > "$SD/.tmp_update/bin/shutdown"
    chmod +x "$SD/.tmp_update/bin/shutdown"
    backup_ui_stubs
    screen_backup
    check "backup done shown" grep -q "^INFO BACKUP DONE$" "$TMP/ui"
    echo "save 2" > "$SD/Saves/CurrentProfile/saves/gpSP/Pokemon Emerald (USA).srm"
    backup_ui_stubs 0 0 1
    screen_restore
    check "restore cancelled: save unchanged" test "$(cat "$SD/Saves/CurrentProfile/saves/gpSP/Pokemon Emerald (USA).srm")" = "save 2"
    check "list titled RESTORE" grep -q "^CHOOSE RESTORE | [0-9-]* [0-9:]*$" "$TMP/ui"
    backup_ui_stubs 0 1 1
    screen_restore
    check "restored" test "$(cat "$SD/Saves/CurrentProfile/saves/gpSP/Pokemon Emerald (USA).srm")" = "save 1"
    check "restart offered" grep -q "^CHOOSE RESTORE DONE | Restart now Later$" "$TMP/ui"
    check "Later: no restart" test ! -e "$TMP/shutdown_args"
    (backup_ui_stubs 0 1 0; screen_restore)
    check "Restart now: clean reboot" test "$(cat "$TMP/shutdown_args" 2> /dev/null)" = "-r"
    end

    begin backup_restore_keeps_the_clock
    SD=$TMP/sd
    backup_fake_sd "$SD"
    backup_load "$SD"
    mkdir -p "$TMP/old/Saves/CurrentProfile/saves"
    echo "1000" > "$TMP/old/Saves/CurrentProfile/saves/currentTime.txt"
    echo "old save" > "$TMP/old/Saves/CurrentProfile/saves/old.srm"
    (cd "$TMP/old" && "$BACKUP_TEST_7Z" a -tzip "$TMP/old.zip" Saves > /dev/null 2>&1)
    restore_backup "$TMP/old.zip"
    check "restored" test $? -eq 0
    check "its save written" test "$(cat "$SD/Saves/CurrentProfile/saves/old.srm")" = "old save"
    check "its saved clock not written back" test "$(cat "$SD/Saves/CurrentProfile/saves/currentTime.txt")" = 1700000000
    end

    begin backup_restore_leaves_syncthing_alone
    SD=$TMP/sd
    backup_fake_sd "$SD"
    backup_load "$SD"
    mkdir -p "$TMP/old/.tmp_update/config/syncthing" "$TMP/old/Saves/CurrentProfile/saves"
    echo "key 0" > "$TMP/old/.tmp_update/config/syncthing/key.pem"
    echo "old save" > "$TMP/old/Saves/CurrentProfile/saves/old.srm"
    (cd "$TMP/old" && "$BACKUP_TEST_7Z" a -tzip "$TMP/old.zip" Saves .tmp_update > /dev/null 2>&1)
    restore_backup "$TMP/old.zip"
    check "a backup with Syncthing's folder still goes back" test $? -eq 0
    check "its save written" test "$(cat "$SD/Saves/CurrentProfile/saves/old.srm")" = "old save"
    check "Syncthing's keys left as they are" test "$(cat "$SD/.tmp_update/config/syncthing/key.pem")" = "key 1"
    end

    begin backup_restore_puts_tweaks_switches_back
    SD=$TMP/sd
    backup_fake_sd "$SD"
    backup_load "$SD"
    # Now: logging on, SSH off, the panel on (not in the backup).
    : > "$SD/.tmp_update/config/.sshState_"
    : > "$SD/.tmp_update/config/.osdPanel"
    echo "secret" > "$SD/.tmp_update/config/.password.txt"
    # The backup: logging off, SSH on.
    rm -rf "$TMP/old"
    mkdir -p "$TMP/old/.tmp_update/config/system"
    : > "$TMP/old/.tmp_update/config/.logging_"
    : > "$TMP/old/.tmp_update/config/.sshState"
    echo "old" > "$TMP/old/.tmp_update/config/.password.txt"
    : > "$TMP/old/.tmp_update/config/system/.keep"
    rm -f "$TMP/old.zip"
    (cd "$TMP/old" && "$BACKUP_TEST_7Z" a -tzip "$TMP/old.zip" .tmp_update > /dev/null 2>&1)
    restore_backup "$TMP/old.zip"
    check "restored" test $? -eq 0
    check "logging off as in the backup" test -f "$SD/.tmp_update/config/.logging_" -a ! -e "$SD/.tmp_update/config/.logging"
    check "SSH on as in the backup" test -f "$SD/.tmp_update/config/.sshState" -a ! -e "$SD/.tmp_update/config/.sshState_"
    check "an option the backup does not have is left" test -f "$SD/.tmp_update/config/.osdPanel"
    check "other dot files only written back" test "$(cat "$SD/.tmp_update/config/.password.txt")" = "old"
    check "and logged" grep -q "Removed .logging (the backup has .logging_)" "$LOG"
    end

    begin backup_restore_needs_a_whole_safety_copy
    SD=$TMP/sd
    backup_fake_sd "$SD"
    backup_load "$SD"
    rm -rf "$TMP/old"
    mkdir -p "$TMP/old/Saves/CurrentProfile/saves"
    echo "old save" > "$TMP/old/Saves/CurrentProfile/saves/old.srm"
    rm -f "$TMP/old.zip"
    (cd "$TMP/old" && "$BACKUP_TEST_7Z" a -tzip "$TMP/old.zip" Saves > /dev/null 2>&1)
    # 7z could not read some current files: the copy is made, but not whole.
    make_backup() {
        mkdir -p "$SD/Backups"
        BACKUP_FILE="$SD/Backups/partial.zip"
        : > "$BACKUP_FILE"
        printf 'WARNING: errno=5 : Input/output error\nSaves/CurrentProfile/saves/bad.srm\n' >> "$LOG"
        return 1
    }
    printf 'WARNING: errno=5 : Input/output error\nan older run\n' >> "$LOG"
    restore_backup "$TMP/old.zip"
    check "asked first" test $? -eq 2
    check "nothing written" test ! -e "$SD/Saves/CurrentProfile/saves/old.srm"
    check "the unread file named (this run's)" test "$RESTORE_UNREAD" = "Saves/CurrentProfile/saves/bad.srm"
    check "the partial copy kept for the answer" test "$RESTORE_PARTIAL" = "$SD/Backups/partial.zip" -a -e "$SD/Backups/partial.zip"
    restore_cancel
    check "Cancel: the partial copy removed" test ! -e "$SD/Backups/partial.zip"
    restore_backup "$TMP/old.zip"
    : > "$TMP/backups_made"
    make_backup() { echo again >> "$TMP/backups_made"; return 1; }
    restore_backup "$TMP/old.zip" 1
    check "Restore anyway" test $? -eq 0
    check "written" test "$(cat "$SD/Saves/CurrentProfile/saves/old.srm")" = "old save"
    check "with the copy made before, not a second one" test ! -s "$TMP/backups_made"
    check "which is kept" test -e "$SD/Backups/partial.zip"
    check "the screen asks" grep -q '"Cancel" "Restore anyway"' "$BACKUP_APP"
    check "and names the file only" grep -q 'unread="\\n${RESTORE_UNREAD##\*/}"' "$BACKUP_APP"
    end

    begin backup_restore_refuses_foreign_paths
    SD=$TMP/sd
    backup_fake_sd "$SD"
    backup_load "$SD"
    mkdir -p "$TMP/evil/Emu/GBA"
    echo "evil" > "$TMP/evil/Emu/GBA/launch.sh"
    (cd "$TMP/evil" && "$BACKUP_TEST_7Z" a -tzip "$TMP/evil.zip" Emu > /dev/null 2>&1)
    restore_backup "$TMP/evil.zip"
    check "refused" test $? -ne 0
    check "says which file" sh -c 'printf "%s" "$1" | grep -q "Emu/GBA"' _ "$RESTORE_ERROR"
    check "nothing written" test ! -e "$SD/Emu/GBA/launch.sh"
    check "no backup made for it" test -z "$(list_backups)"
    echo "not a zip" > "$TMP/broken.zip"
    restore_backup "$TMP/broken.zip"
    check "a damaged file is refused" test $? -ne 0 -a -n "$RESTORE_ERROR"
    end

    begin backup_restore_keeps_guest_mode
    SD=$TMP/sd
    backup_fake_sd "$SD"
    backup_load "$SD"
    make_backup ""
    normal=$BACKUP_FILE
    mkdir -p "$SD/Saves/MainProfileOld"
    : > "$SD/Saves/MainProfileOld/x"
    make_backup ""
    check "a folder only starting with MainProfile is not Guest Mode" not_guest_archive "$BACKUP_FILE"
    rm -rf "$SD/Saves/MainProfileOld"
    mv "$SD/Saves/CurrentProfile" "$SD/Saves/MainProfile"
    mkdir -p "$SD/Saves/CurrentProfile/saves"
    restore_backup "$normal"
    check "Guest Mode on, backup without it: refused" test $? -ne 0
    check "says to turn it off" sh -c 'printf "%s" "$1" | grep -q "Turn Guest Mode off"' _ "$RESTORE_ERROR"
    check "main profile untouched" test ! -e "$SD/Saves/CurrentProfile/saves/gpSP"
    make_backup ""
    guest=$BACKUP_FILE
    rm -rf "$SD/Saves/CurrentProfile"
    mv "$SD/Saves/MainProfile" "$SD/Saves/CurrentProfile"
    restore_backup "$guest"
    check "Guest Mode off, backup with it: refused" test $? -ne 0
    check "says to turn it on" sh -c 'printf "%s" "$1" | grep -q "Turn Guest Mode on"' _ "$RESTORE_ERROR"
    end

    begin backup_not_enough_space_or_7z_error
    SD=$TMP/sd
    backup_fake_sd "$SD"
    backup_load "$SD"
    free_kb() { echo 100; }
    make_backup ""
    check "no space: not made" test $? -eq 2
    check "says so" sh -c 'printf "%s" "$1" | grep -q "Not enough space"' _ "$BACKUP_ERROR"
    check "no file" test -z "$(ls -A "$SD/Backups")"
    backup_load "$SD"
    BACKUP_7Z=false
    Z7=false
    make_backup ""
    check "7z fails: not made" test $? -eq 2
    check "nothing left behind" test -z "$(ls -A "$SD/Backups")"
    end
fi

# ---- Green: light copies of heavy manuals (bin/green/green) ----
begin "green_light_copy_of_heavy_manuals"
GD="$TMP/sd/.tmp_update/bin/green"
mkdir -p "$GD" "$TMP/sd/.tmp_update/config" "$TMP/t" "$TMP/manuals"
cp "$ROOT/static/build/.tmp_update/bin/green/green" "$GD/green"
printf '#!/bin/sh\necho "$*" >> "%s/opened"\n' "$TMP" > "$GD/green.bin"
# pdflight stub: MODE ok (2 pages), skip (exit 3), fail (exit 1), slow.
cat > "$GD/pdflight" << STUB
#!/bin/sh
echo "\$*" >> "$TMP/calls"
echo "reduce=\$GREEN_JPX_REDUCE pages=\$GREEN_CACHE_PAGES" > "$TMP/pdflight_env"
mode=\$(cat "$TMP/mode")
[ "\$mode" = skip ] && exit 3
[ "\$mode" = unreadable ] && exit 4
echo "pages 2"
[ "\$mode" = torn ] && { printf 'page 1/'; sleep 1; echo '2'; }
echo "page 1/2"
[ "\$mode" = slow ] && { echo partial > "\$3"; sleep 30; }
[ "\$mode" = fail ] && { echo partial > "\$3"; exit 1; }
sleep 2
echo light > "\$3"
echo "page 2/2"
exit 0
STUB
mkdir -p "$TMP/sd/.tmp_update/bin"
cat > "$TMP/sd/.tmp_update/bin/infoPanel" << STUB
#!/bin/sh
echo "\$4" >> "$TMP/panel"
while [ ! -f "$TMP/t/dismiss_info_panel" ]; do sleep 0.1; done
STUB
# detectKey stub: B "held" once while $TMP/holdB exists.
cat > "$TMP/sd/.tmp_update/bin/detectKey" << STUB
#!/bin/sh
[ "\$1" = 29 ] && [ -f "$TMP/holdB" ] && { rm -f "$TMP/holdB"; exit 0; }
exit 1
STUB
chmod +x "$GD/green" "$GD/green.bin" "$GD/pdflight" "$TMP/sd/.tmp_update/bin/infoPanel" "$TMP/sd/.tmp_update/bin/detectKey"
green() { PDFLIGHT_TMP="$TMP/t" sh "$GD/green" "$@"; }
calls() { [ -f "$TMP/calls" ] && wc -l < "$TMP/calls" | tr -d ' ' || echo 0; }
M="$TMP/manuals"
echo scan > "$M/Metroid.pdf"

echo ok > "$TMP/mode"
green "$M/Metroid.pdf"
check "heavy manual: light copy made with --auto" has_line "$TMP/calls" "--auto $M/Metroid.pdf $M/.light/Metroid.pdf.tmp"
check "heavy manual: Green opens the light copy" test "$(tail -n 1 "$TMP/opened")" = "$M/.light/Metroid.pdf"
check "light copy in place, no .tmp left" sh -c 'test "$(cat "$1/.light/Metroid.pdf")" = light && test ! -e "$1/.light/Metroid.pdf.tmp"' _ "$M"
check "progress shown page by page" grep -q 'Page 1 of 2' "$TMP/panel"
check "progress: the time left" grep -q 'Less than a minute left' "$TMP/panel"
check "progress: B to open it now" grep -q 'Hold B to open it now' "$TMP/panel"
left=$(pgrep -f "$TMP/sd/.tmp_update/bin/infoPanel" | wc -l)
check "progress panel closed" test "$left" -eq 0
check "original untouched" test "$(cat "$M/Metroid.pdf")" = scan
check "pdflight reads JPEG2000 at half size" test "$(cat "$TMP/pdflight_env")" = "reduce=1 pages=8"
printf '#!/bin/sh\necho "reduce=$GREEN_JPX_REDUCE pages=$GREEN_CACHE_PAGES mb=$GREEN_CACHE_MB" > "%s/green_env"\n' "$TMP" > "$TMP/envbin"
cp "$GD/green.bin" "$TMP/green.bin.keep"
cp "$TMP/envbin" "$GD/green.bin"
chmod +x "$GD/green.bin"
green "$M/Metroid.pdf"
check "Green gets robcodedev's settings" test "$(cat "$TMP/green_env")" = "reduce=2 pages=8 mb=20"
cp "$TMP/green.bin.keep" "$GD/green.bin"
chmod +x "$GD/green.bin"
green "$M/Metroid.pdf"
check "second time: no new copy, light copy opened" sh -c 'test "$(wc -l < "$1/calls")" -eq 1 && test "$(tail -n 1 "$1/opened")" = "$2/.light/Metroid.pdf"' _ "$TMP" "$M"
touch -t 203001010000 "$M/Metroid.pdf"
green "$M/Metroid.pdf"
check "a PDF dated years after its copy (wrong clock) is not made again" sh -c 'test "$(wc -l < "$1/calls")" -eq 1 && test "$(tail -n 1 "$1/opened")" = "$2/.light/Metroid.pdf"' _ "$TMP" "$M"
echo "a replaced, bigger scan" > "$M/Metroid.pdf"
touch -t 201001010000 "$M/Metroid.pdf"
green "$M/Metroid.pdf"
check "a replaced original (another size) is made light again" test "$(calls)" -eq 2
check "the copy notes the size of its original" test "$(cat "$M/.light/Metroid.pdf.size")" = "$(wc -c < "$M/Metroid.pdf" | tr -d ' ')"

echo text > "$M/Text.pdf"
echo skip > "$TMP/mode"
green "$M/Text.pdf"
check "not worth it: original opened" test "$(tail -n 1 "$TMP/opened")" = "$M/Text.pdf"
check "not worth it: noted, no copy" sh -c 'test -f "$1/.light/Text.pdf.skip" && test ! -e "$1/.light/Text.pdf"' _ "$M"
green "$M/Text.pdf"
check "not worth it: not tried again" test "$(calls)" -eq 3

echo scan > "$M/Bad.pdf"
echo fail > "$TMP/mode"
green "$M/Bad.pdf"
check "pdflight fails: original opened" test "$(tail -n 1 "$TMP/opened")" = "$M/Bad.pdf"
check "pdflight fails: no copy left behind" sh -c 'test ! -e "$1/.light/Bad.pdf" && test ! -e "$1/.light/Bad.pdf.tmp" && test ! -e "$1/.light/Bad.pdf.size"' _ "$M"
check "pdflight fails (memory, card full): tried again next time" sh -c 'test -f "$1/.light/Bad.pdf.fail" && test ! -e "$1/.light/Bad.pdf.skip"' _ "$M"
green "$M/Bad.pdf"
check "fails a second time: then noted" sh -c 'test -f "$1/.light/Bad.pdf.skip" && test ! -e "$1/.light/Bad.pdf.fail"' _ "$M"
before=$(calls)
green "$M/Bad.pdf"
check "and not tried at every opening" test "$(calls)" -eq "$before"
echo scan > "$M/Broken.pdf"
echo unreadable > "$TMP/mode"
green "$M/Broken.pdf"
check "a PDF pdflight cannot read: noted at once" test -f "$M/.light/Broken.pdf.skip"
echo scan > "$M/Full.pdf"
echo ok > "$TMP/mode"
mkdir -p "$TMP/dfbin"
printf '#!/bin/sh\necho "Filesystem 1K-blocks Used Available Use%% Mounted on"\necho "/dev/mmcblk0p1 1000 990 10 99%% /mnt/SDCARD"\n' > "$TMP/dfbin/df"
chmod +x "$TMP/dfbin/df"
before=$(calls)
_saved_path=$PATH; PATH="$TMP/dfbin:$PATH"; green "$M/Full.pdf"; _rc=$?; PATH=$_saved_path; rc $_rc
check "card full: the original, no copy tried" sh -c 'test "$(tail -n 1 "$1/opened")" = "$2/Full.pdf" && test "$(wc -l < "$1/calls")" -eq "$3"' _ "$TMP" "$M" "$before"

echo scan > "$M/Off.pdf"
touch "$TMP/sd/.tmp_update/config/.noPdfLight"
before=$(calls)
green "$M/Off.pdf"
check "switched off in Tweaks: original, no copy" sh -c 'test "$(tail -n 1 "$1/opened")" = "$2/Off.pdf" && test "$(wc -l < "$1/calls")" -eq "$3"' _ "$TMP" "$M" "$before"
rm -f "$TMP/sd/.tmp_update/config/.noPdfLight"

green "$M/.light/Metroid.pdf"
check "a light copy itself opens as it is" test "$(tail -n 1 "$TMP/opened")" = "$M/.light/Metroid.pdf"
green
check "no file: Green as it was" test "$(tail -n 1 "$TMP/opened")" = ""
green "$M/missing.pdf"
check "missing file: passed to Green" test "$(tail -n 1 "$TMP/opened")" = "$M/missing.pdf"

echo scan > "$M/Torn.pdf"
echo torn > "$TMP/mode"
green "$M/Torn.pdf"
check "a half-written progress line does not stop it" test "$(tail -n 1 "$TMP/opened")" = "$M/.light/Torn.pdf"

echo scan > "$M/Skip.pdf"
echo slow > "$TMP/mode"
( sleep 2; touch "$TMP/holdB" ) &
green "$M/Skip.pdf"
check "B held: the original opened now" test "$(tail -n 1 "$TMP/opened")" = "$M/Skip.pdf"
check "B held: no copy, no note, offered again" sh -c 'test ! -e "$1/.light/Skip.pdf" && test ! -e "$1/.light/Skip.pdf.tmp" && test ! -e "$1/.light/Skip.pdf.skip"' _ "$M"
left=$(pgrep -f "$GD/pdflight" | wc -l)
check "B held: pdflight stopped" test "$left" -eq 0

echo scan > "$M/Slow.pdf"
echo slow > "$TMP/mode"
PDFLIGHT_TMP="$TMP/t" sh "$GD/green" "$M/Slow.pdf" &
gpid=$!
sleep 2
kill -TERM $gpid
wait $gpid
check "stopped while making it: Green not opened" test "$(tail -n 1 "$TMP/opened")" != "$M/Slow.pdf"
check "stopped while making it: no partial copy" sh -c 'test ! -e "$1/.light/Slow.pdf.tmp" && test ! -e "$1/.light/Slow.pdf"' _ "$M"
left=$(pgrep -f "$GD/pdflight" | wc -l)
check "stopped while making it: pdflight ended" test "$left" -eq 0
pkill -f "$TMP/sd/.tmp_update/bin/infoPanel" 2> /dev/null
end

# ---- Kids Mode (App/KidsMode): in place of MainUI while armed ----

KIDS_APP="$ROOT/static/packages/App/Kids Mode/App/KidsMode"
KIDS_LOOP="$KIDS_APP/kid_mode_loop.sh"
KEYMON_C="$ROOT/src/keymon/keymon.c"

begin kids_mode_review_fixes
# Double press off while armed; the timer kept outside the app folder;
# a game that is not RetroArch ended with its children
for fn in apply_keymap_override set_timer_minutes kill_tree; do
    eval "$(extract_fn "$KIDS_LOOP" $fn)"
done
backupdir=$TMP/km
keymapcfg=$TMP/keymap.json
keymapbackup=$backupdir/keymap.json.backup
keymapnone=$backupdir/keymap-was-absent
timer_backup=$backupdir/timer_minutes.txt
restart_keymon() { :; }
log() { :; }
update_remaining_now() { :; }
config_merge() { echo "merge $*" >> "$TMP/merges"; }
printf '{"ingame_single_press": 1, "ingame_long_press": 1, "ingame_double_press": 3}\n' > "$keymapcfg"
if command -v jq > /dev/null; then
    apply_keymap_override
    check "double press off while armed" test "$(jq -c '[.ingame_single_press, .ingame_long_press, .ingame_double_press]' "$keymapcfg")" = "[2,0,0]"
    check "the parent's keymap kept for the unlock" test "$(jq .ingame_double_press "$keymapbackup")" = 3
else
    skip "Kids Mode keymap override test (jq not installed)"
fi
rm -f "$keymapcfg" "$keymapbackup"
apply_keymap_override
check "no keymap before: the one written has the double press off" grep -q '"ingame_double_press": 0' "$keymapcfg"
set_timer_minutes 45
check "the timer is copied outside the app folder" test "$(cat "$timer_backup")" = 45
# The restore at the start of a run, as an update leaves kidmode.json
restore=$(sed -n '/# The same update took the play timer with it/,/^        fi$/p' "$KIDS_LOOP")
configfile=$TMP/kidmode.json
printf '{"pin_hash": ""}\n' > "$configfile"
rm -f "$TMP/merges"
if command -v jq > /dev/null; then
    eval "$restore"
    check "after an update: the timer comes back" grep -q 'timer_minutes = \$m' "$TMP/merges"
    printf '{"timer_minutes": 10}\n' > "$configfile"
    rm -f "$TMP/merges"
    eval "$restore"
    check "a timer in kidmode.json is left as it is" test ! -e "$TMP/merges"
else
    skip "Kids Mode timer restore test (jq not installed)"
fi
# kill_tree: launch.sh and the emulator under the shell of cmd_to_run.sh
sh -c 'sh -c "sleep 30 & sleep 30 & wait" & wait' &
top=$!
sleep 0.3
kill_tree $top
sleep 0.3
left=$(pgrep -P $top 2> /dev/null; for c in $(pgrep -P $top 2> /dev/null); do pgrep -P $c; done)
check "the whole tree is ended" sh -c '! kill -0 "$1" 2> /dev/null' _ $top
check "nothing left under it${left:+: $left}" test -z "$left"
unset -f apply_keymap_override set_timer_minutes kill_tree restart_keymon log update_remaining_now config_merge
end

begin kids_mode_app_gone_gives_the_saves_back
# Armed, then App/KidsMode removed (Reinstall, deleted on a computer): the
# parent's saves were left parked in Saves/KidsMode
eval "$(extract_fn "$RUNTIME" kids_mode_restore_parked_saves | sed "s#/mnt/SDCARD#$TMP/sd#g")"
eval "$(extract_fn "$RUNTIME" kids_mode_cleanup_if_disarmed | sed "s#/mnt/SDCARD#$TMP/sd#g")"
log() { :; }
kids_mode_loop=$TMP/sd/App/KidsMode/kid_mode_loop.sh
mkdir -p "$TMP/sd/Saves/KidsMode/profile-parked-saves" "$TMP/sd/Saves/KidsMode/profile-parked-states" \
    "$TMP/sd/Saves/CurrentProfile/saves" "$TMP/sd/Saves/CurrentProfile/states"
echo parent > "$TMP/sd/Saves/KidsMode/profile-parked-saves/Tetris.srm"
echo parent > "$TMP/sd/Saves/KidsMode/profile-parked-states/Tetris.state.auto"
echo child > "$TMP/sd/Saves/CurrentProfile/saves/Tetris.srm"
echo Guest > "$TMP/sd/Saves/KidsMode/source_profile.txt"
touch "$TMP/sd/.kidmode" "$TMP/sd/Saves/KidsMode/profile_isolation_active" "$TMP/sd/Saves/KidsMode/retroarch.cfg.backup"
kids_mode_cleanup_if_disarmed
check "the parent's save is back" test "$(cat "$TMP/sd/Saves/CurrentProfile/saves/Tetris.srm")" = parent
check "the parent's state is back" test -f "$TMP/sd/Saves/CurrentProfile/states/Tetris.state.auto"
check "the child's save kept in KidsProfile/Guest" test "$(cat "$TMP/sd/Saves/KidsProfile/Guest/saves/Tetris.srm")" = child
check "nothing parked any more" test ! -e "$TMP/sd/Saves/KidsMode/profile-parked-saves" -a ! -e "$TMP/sd/Saves/KidsMode/profile-parked-states"
check ".kidmode removed" test ! -e "$TMP/sd/.kidmode"
check "the other marks stay for the app's own cleanup" test -f "$TMP/sd/Saves/KidsMode/retroarch.cfg.backup"
rm -rf "$TMP/sd/Saves/KidsMode/profile-parked-"*
kids_mode_cleanup_if_disarmed
check "nothing parked: nothing changes" test "$(cat "$TMP/sd/Saves/CurrentProfile/saves/Tetris.srm")" = parent
unset -f kids_mode_restore_parked_saves kids_mode_cleanup_if_disarmed log
end

begin kids_mode_runtime_in_place_of_mainui
sysdir=$TMP
eval "$(extract_fn "$RUNTIME" kids_mode_armed | sed "s#/mnt/SDCARD/.kidmode#$TMP/.kidmode#")"
eval "$(extract_fn "$RUNTIME" check_main_ui)"
eval "$(extract_fn "$RUNTIME" check_switcher)"
kids_mode_loop=$TMP/loop.sh
launch_kids_mode() { echo kids >> "$TMP/calls"; }
launch_main_ui() { echo mainui >> "$TMP/calls"; }
launch_switcher() { echo switcher >> "$TMP/calls"; }
check_off_order() { :; }
log() { :; }
sync() { :; }
check_main_ui
check "not armed: MainUI" test "$(cat "$TMP/calls")" = mainui
: > "$TMP/.kidmode"
check_main_ui
check "armed, Kids Mode not installed: MainUI" test "$(tail -n 1 "$TMP/calls")" = mainui
: > "$kids_mode_loop"
check_main_ui
check "armed: Kids Mode in place of MainUI" test "$(tail -n 1 "$TMP/calls")" = kids
echo game > "$sysdir/cmd_to_run.sh"
: > "$TMP/calls"
check_main_ui
check "a game to launch: neither (launch_game runs it)" test ! -s "$TMP/calls"
: > "$sysdir/.runGameSwitcher"
check_switcher
check "armed: never the GameSwitcher (the parent's recent games)" sh -c 'test ! -s "$1/calls" && test ! -e "$1/.runGameSwitcher"' _ "$TMP"
rm -f "$TMP/.kidmode"
: > "$sysdir/.runGameSwitcher"
check_switcher
check "not armed: the GameSwitcher as before" test "$(cat "$TMP/calls")" = switcher
boot=$(sed -n '/^main() {/,/^}/p' "$RUNTIME")
check "at boot: Kids Mode first, then a game it left (no startup app)" sh -c 'printf "%s\n" "$1" | awk "/if kids_mode_armed; then/ { k = NR } k && /launch_kids_mode/ && !l { l = NR } l && /state_change check_game/ && !g { g = NR } /startup_app=/ { a = NR } END { exit !(k && l > k && g > l && a > g) }"' _ "$boot"
unset -f launch_kids_mode launch_main_ui launch_switcher check_off_order log sync check_main_ui check_switcher kids_mode_armed
end

begin sd_read_only_no_relaunch_loop
# A read-only card: cmd_to_run.sh can't be removed when an app or a game
# ends, so check_game would start it again at once, forever. Instead: a
# message, then the console turns off.
sysdir="$TMP/sys"; mkdir -p "$sysdir"
eval "$(extract_fn "$RUNTIME" kids_mode_armed | sed "s#/mnt/SDCARD/.kidmode#$TMP/.kidmode#")"
eval "$(extract_fn "$RUNTIME" check_switcher)"
eval "$(extract_fn "$RUNTIME" sd_read_only_stop)"
rm -f "$TMP/.kidmode" "$sysdir/.runGameSwitcher" /tmp/quick_switch
check_off_order() { :; }
log() { :; }
sync() { :; }
infoPanel() { echo "panel $2" >> "$TMP/calls"; }
shutdown() { echo shutdown >> "$TMP/calls"; }
echo app > "$sysdir/cmd_to_run.sh"
: > "$TMP/calls"
check_switcher
check "writable card: the command goes, nothing shown" sh -c 'test ! -e "$1/cmd_to_run.sh" && test ! -s "$2"' _ "$sysdir" "$TMP/calls"
echo app > "$sysdir/cmd_to_run.sh"
rm() { return 1; } # read-only: nothing can be removed
check_switcher
unset -f rm
check "read-only card: the message, then the console turns off" test "$(tr '\n' ' ' < "$TMP/calls")" = "panel SD card is read-only shutdown "
: > "$TMP/calls"
: > "$sysdir/.runGameSwitcher"
check_switcher() { :; }
eval "$(extract_fn "$RUNTIME" check_switcher)"
launch_switcher() { echo switcher >> "$TMP/calls"; }
check_switcher
check "the GameSwitcher keeps the command on purpose: no message" test "$(cat "$TMP/calls")" = switcher
rm -f "$sysdir/.runGameSwitcher"
msg=$(sed -n 's/.*infoPanel --title "SD card is read-only" --message "\(.*\)"$/\1/p' "$RUNTIME")
check "the message fits infoPanel (under 256 chars)" test ${#msg} -gt 0 -a ${#msg} -lt 256
long=$(printf '%s' "$msg" | sed 's/\\n/\n/g' | awk 'length > 38' | head -n 1)
check "lines of 38 characters or less ($long)" test -z "$long"
unset -f check_off_order log sync infoPanel shutdown launch_switcher check_switcher sd_read_only_stop kids_mode_armed
end

begin kids_mode_runtime_starts_the_launcher
sysdir=$TMP
mkdir -p "$sysdir/bin"
eval "$(extract_fn "$RUNTIME" launch_kids_mode)"
eval "$(extract_fn "$RUNTIME" fb_probe_fields)"
log() { :; }
start_audioserver() { echo audio >> "$TMP/calls"; }
notify_resolution_change() { echo notified >> "$TMP/calls"; }
kids_mode_loop=$TMP/loop.sh
printf 'echo "$KIDSMODE_HOST $1" > "%s/loop"\n' "$TMP" > "$kids_mode_loop"
launch_kids_mode
check "started as the host, to run" test "$(cat "$TMP/loop")" = "runtime run"
check "audio server up" grep -q audio "$TMP/calls"
printf '#!/bin/sh\n[ "$1" = --probe ] && { echo "$PROBE"; exit 0; }\necho "$*" >> "%s/fbmode"\n' "$TMP" > "$sysdir/bin/fbmode"
chmod +x "$sysdir/bin/fbmode"
check "640x480 devices: no mode change" test ! -e "$TMP/fbmode"
: > $TMP/new_res_available
eval "$(extract_fn "$RUNTIME" launch_kids_mode | sed "s#/tmp/new_res_available#$TMP/new_res_available#")"
export PROBE="640x480 virtual 640x1440 bpp 32 line_length 2560 pages 3"; launch_kids_mode; _rc=$?; unset PROBE; rc $_rc
check "752x560 devices, MainUI's 3 pages: two 640x480 pages" sh -c 'grep -q "^640x480 --pages 2 " "$1/fbmode" && grep -q notified "$1/calls"' _ "$TMP"
: > "$TMP/fbmode"
export PROBE="640x480 virtual 640x960 bpp 32 line_length 2560 pages 2"; launch_kids_mode; _rc=$?; unset PROBE; rc $_rc
check "already two 640x480 pages: left alone" test ! -s "$TMP/fbmode"
rm -f $TMP/new_res_available
unset -f launch_kids_mode fb_probe_fields log start_audioserver notify_resolution_change
end

begin kids_mode_loop_hands_games_to_runtime
sysdir=$TMP
game_handoff_file=$TMP/handoff
reset_cfg=$TMP/reset.cfg
eval "$(extract_fn "$KIDS_LOOP" run_game_cmd)"
eval "$(extract_fn "$KIDS_LOOP" finish_game_handoff)"
eval "$(extract_fn "$KIDS_LOOP" strip_reset_appendconfig)"
wait_for_game_environment() { echo prepared >> "$TMP/calls"; }
allow_onion_battery_warning_for_game() { echo battery >> "$TMP/calls"; }
log() { :; }
printf './retroarch -v --appendconfig "%s" -L core.so "$1"\n' "$reset_cfg" > "$TMP/launch.sh"
printf 'LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so "%s" "%s/Roms/GBA/game.gba"\n' "$TMP/launch.sh" "$TMP" > "$sysdir/cmd_to_run.sh"
: > "$reset_cfg"
(run_game_cmd; echo "returned" > "$TMP/returned")
check "the script ends, for runtime.sh to launch the game" test ! -e "$TMP/returned"
check "the game stays in cmd_to_run.sh" grep -q game.gba "$sysdir/cmd_to_run.sh"
check "its launch script remembered" test "$(cat "$game_handoff_file")" = "$TMP/launch.sh"
check "set up first, Onion's battery warning back for the game" test "$(cat "$TMP/calls")" = "prepared
battery"
finish_game_handoff
check "back from it: handled" test $? -eq 0
check "Start over taken out of the launch script again" sh -c '! grep -q appendconfig "$1/launch.sh"' _ "$TMP"
check "and its file removed" test ! -e "$reset_cfg"
finish_game_handoff
check "no game handed over: nothing to do" test $? -eq 1
check "back from a game: the launcher on it, not resuming a video" sh -c "sed -n '/^cmd_run() {/,/^}/p' '$KIDS_LOOP' | awk '/load_state_cache/ { l = NR } /if finish_game_handoff; then/ { f = NR } /initial_mode=carousel/ { c = NR } /initial_mode\" = running/ && !r { r = NR } END { exit !(l && f > l && c > f && r > c) }'"
unset -f run_game_cmd finish_game_handoff strip_reset_appendconfig wait_for_game_environment allow_onion_battery_warning_for_game log
end

begin kids_mode_loop_writes_games_as_mainui
sysdir=$TMP
miyoodir=/mnt/SDCARD/miyoo
reset_cfg=$TMP/kidmode_reset.cfg
eval "$(extract_fn "$KIDS_LOOP" build_game_cmd | sed "s#/tmp/reset_game#$TMP/reset_game#g")"
eval "$(extract_fn "$KIDS_LOOP" strip_reset_appendconfig)"
mkdir -p "$TMP/Roms/GBA" "$TMP/Emu/GBA/.game_config"
echo rom > "$TMP/Roms/GBA/game.gba"
printf './retroarch -v --appendconfig "%s" -L core.so "$1"\n' "$reset_cfg" > "$TMP/Emu/GBA/launch.sh"
build_game_cmd "$TMP/Emu/GBA/launch.sh" "$TMP/Emu/GBA/../../Roms/GBA/game.gba"
check "as MainUI writes it (runtime.sh picks the core)" test "$(cat "$sysdir/cmd_to_run.sh")" = "LD_PRELOAD=/mnt/SDCARD/miyoo/lib/libpadsp.so \"$TMP/Emu/GBA/launch.sh\" \"$TMP/Roms/GBA/game.gba\""
check "no reset flag to resume" test ! -e "$TMP/reset_game"
check "an older version's Start over taken out of the launch script" sh -c '! grep -q appendconfig "$1"' _ "$TMP/Emu/GBA/launch.sh"
build_game_cmd "$TMP/Emu/GBA/launch.sh" "$TMP/Roms/GBA/game.gba" fresh
check "Start over: Onion's reset-game flag (runtime.sh adds /tmp/reset.cfg)" test -e "$TMP/reset_game"
check "runtime.sh still knows that flag" grep -q 'if \[ -f /tmp/reset_game \]; then' "$RUNTIME"
check "and takes the flag away if the game does not use it" sh -c "sed -n '/^finish_game_handoff() {/,/^}/p' '$KIDS_LOOP' | grep -q '/tmp/reset_game'"
unset -f build_game_cmd strip_reset_appendconfig
end

begin kids_mode_loop_leaves_shutdown_to_runtime
eval "$(extract_fn "$KIDS_LOOP" check_off_order | sed -e "s#/tmp/.offOrder#$TMP/.offOrder#" -e "s#/tmp/shutting_down#$TMP/shutting_down#")"
stop_ticker() { echo ticker-stopped >> "$TMP/calls"; }
log() { :; }
(check_off_order End; echo continued > "$TMP/cont")
check "no shutdown asked: carries on" test -e "$TMP/cont"
rm -f "$TMP/cont"
: > "$TMP/.offOrder"
(check_off_order End; echo continued > "$TMP/cont")
check "shutdown: play time saved, the script ends (runtime.sh shuts down)" sh -c 'test ! -e "$1/cont" && grep -q ticker-stopped "$1/calls"' _ "$TMP"
check "it no longer shuts down by itself" sh -c "! sed -n '/^check_off_order() {/,/^}/p' '$KIDS_LOOP' | grep -q '^ *shutdown\$'"
unset -f check_off_order stop_ticker log
end

begin kids_mode_loop_redonion_hooks
sysdir=$TMP
checkoff_src="$KIDS_APP/kidmode_checkoff.sh"
checkoff_dst=$TMP/checkoff/kidmode_checkoff.sh
eval "$(extract_fn "$KIDS_LOOP" install_hook)"
log() { :; }
mkdir -p "$TMP/startup"
: > "$TMP/startup/kidmode_boot.sh"
install_hook
check "the standalone boot hook goes" test ! -e "$TMP/startup/kidmode_boot.sh"
check "the shutdown hook installed" cmp -s "$checkoff_src" "$checkoff_dst"
check "run only when runtime.sh starts it" grep -q '\[ "$kids_host" = runtime \] || exit 0' "$KIDS_LOOP"
check "no separate keymon any more" sh -c "! grep -q 'bin/keymon\|killall keymon' '$KIDS_LOOP'"
check "keymon reads the keymap again instead" sh -c "sed -n '/^restart_keymon() {/,/^}/p' '$KIDS_LOOP' | grep -q 'touch /tmp/settings_changed'"
check "keymon's Kids Mode flag set while it runs, removed when it ends" sh -c "grep -q 'touch \"\$kidsmode_active_flag\"' '$KIDS_LOOP' && sed -n '/^disarm() {/,/^}/p' '$KIDS_LOOP' | grep -q kidsmode_active_flag"
check "the loop lock released when the script ends" grep -q '^trap release_loop_lock EXIT' "$KIDS_LOOP"
check "the Apps entry only arms (runtime.sh starts the launcher)" sh -c "grep -q '\[ -f /mnt/SDCARD/.kidmode \] && exit 0' '$KIDS_APP/launch.sh' && ! grep -q 'kid_mode_loop.sh run' '$KIDS_APP/launch.sh'"
unset -f install_hook log
end

begin kids_mode_video_overlay
# Parent Menu > Other options > Video overlay: kidui keeps the choice in
# Saves/KidsMode/video_overlay, kid_mode_loop.sh hands its PNG to KidsPlay
# as VC_OVERLAY, for videos only (Veuks).
sed -n '/# The video overlay (Parent Menu > Other options/,/^    theme_config=/p' "$KIDS_LOOP" | sed '$d' > "$TMP/overlay_block.sh"
check "the overlay block is found" test -s "$TMP/overlay_block.sh"
overlay_for() { # media_kind, choice (or "none")
    (
        appdir="$TMP/kapp" backupdir="$TMP/ksaves" media_kind=$1
        log() { :; }
        mkdir -p "$appdir/overlays" "$backupdir"
        : > "$appdir/overlays/lcd_grid_fine.png"; : > "$appdir/overlays/scanlines_light.png"
        rm -f "$backupdir/video_overlay"
        [ "$2" = none ] || echo "$2" > "$backupdir/video_overlay"
        . "$TMP/overlay_block.sh"
        echo "${video_overlay:-none}"
    )
}
check "Off (no file): none" test "$(overlay_for video none)" = none
check "LCD grid: lcd_grid_fine.png" test "$(overlay_for video lcd_grid)" = "$TMP/kapp/overlays/lcd_grid_fine.png"
check "Scanlines: scanlines_light.png" test "$(overlay_for video scanlines)" = "$TMP/kapp/overlays/scanlines_light.png"
check "not for music" test "$(overlay_for audio lcd_grid)" = none
check "an unknown choice: none" test "$(overlay_for video ../../etc/passwd)" = none
rm -f "$TMP/kapp/overlays/lcd_grid_fine.png"
check "a missing PNG: none" test "$( ( appdir="$TMP/kapp" backupdir="$TMP/ksaves" media_kind=video; log() { :; }; echo lcd_grid > "$backupdir/video_overlay"; . "$TMP/overlay_block.sh"; echo "${video_overlay:-none}" ) )" = none
check "passed to KidsPlay for videos" grep -q 'VC_OVERLAY="$video_overlay"' "$KIDS_LOOP"
check "only once (the video call, not the audio one)" test "$(grep -c 'VC_OVERLAY=' "$KIDS_LOOP")" -eq 1
check "the two PNGs ship with Kids Mode" sh -c "test -s '$KIDS_APP/overlays/lcd_grid_fine.png' && test -s '$KIDS_APP/overlays/scanlines_light.png'"
check "kidui writes the names the loop reads" sh -c "grep -q 'lcd_grid\\\\n' '$ROOT/src/kidsMode/videoui.c' && grep -q 'scanlines\\\\n' '$ROOT/src/kidsMode/videoui.c' && grep -q 'Saves/KidsMode/video_overlay' '$ROOT/src/kidsMode/videoui.c'"
check "KidsPlay reads VC_OVERLAY for videos only" sh -c "grep -q 'if (!kp.audio_only)' '$ROOT/src/kidsMode/kidsplay-src/kidsplay_ui.inc' && grep -q 'kp_load_screen_overlay(getenv(\"VC_OVERLAY\"))' '$ROOT/src/kidsMode/kidsplay-src/kidsplay_ui.inc'"
unset -f overlay_for
end

begin kids_mode_checkoff_saves_play_time
sed -e "s#/mnt/SDCARD/.kidmode#$TMP/.kidmode#" -e "s#/tmp/kidsmode_timer_used#$TMP/used#" \
    -e "s#/mnt/SDCARD/Saves/KidsMode#$TMP/saves#" "$KIDS_APP/kidmode_checkoff.sh" > "$TMP/off.sh"
mkdir -p "$TMP/saves"
printf '2026-11-12\n100\n300\n' > "$TMP/saves/timer_state.txt"
echo 640 > "$TMP/used"
sh "$TMP/off.sh"
check "not armed: nothing written" test "$(sed -n 2p "$TMP/saves/timer_state.txt")" = 100
: > "$TMP/.kidmode"
sh "$TMP/off.sh"
check "armed: the time used saved, the extra time kept" sh -c 'test "$(sed -n 2,3p "$1")" = "640
300"' _ "$TMP/saves/timer_state.txt"
echo "" > "$TMP/used"
sh "$TMP/off.sh"
check "no counter: left alone" test "$(sed -n 2p "$TMP/saves/timer_state.txt")" = 640
end

begin kids_mode_binaries_stripped
# Veuks' release workflow strips them; the repository's copies were not.
for f in bin/kidsplay bin/fb_reset lib/libminivsync.so; do
    check "$f: no debug symbols (.symtab)" sh -c '! grep -q "\.symtab" "$1"' _ "$KIDS_APP/$f"
done
end

begin kids_mode_keymon_behaviour_only_while_armed
check "Kids Mode known by its /tmp flag" grep -q 'return temp_flag_get("kidsmode");' "$KEYMON_C"
fn=$(sed -n '/^void suspend_exec(int timeout)/,/^}/p' "$KEYMON_C")
check "suspend: the LCD controller kept on in Kids Mode only" sh -c 'printf "%s\n" "$1" | grep -q "if (!kids)" && printf "%s\n" "$1" | grep -c "if (!kids)" | grep -q 3' _ "$fn"
check "POWER: a dimmed Kids Mode screen only wakes" grep -q 'else if (kids_dimmed_at_power_press)' "$KEYMON_C"
check "VOL+ does not unmute in Kids Mode" grep -q 'if (settings.mute && kidsmode_active())' "$KEYMON_C"
check "no MENU+B blue light toggle in Kids Mode" grep -q 'kidsmode_active()) {' "$KEYMON_C"
check "a playing video is not inactivity" grep -q 'temp_flag_get("kidsmode_media_playing")' "$KEYMON_C"
begin disp_csc_reset_uses_mainui_formulas
CSCR="$ROOT/static/build/.tmp_update/script/disp_csc_reset.sh"
mkdir -p "$TMP/bin"
printf '#!/bin/sh\nexec %s "$@"\n' "$(command -v jq)" > "$TMP/bin/jq"
chmod +x "$TMP/bin/jq"
sed -e "s#/appconfigs/system.json#$TMP/system.json#" -e "s#/proc/mi_modules/mi_disp/mi_disp0#$TMP/mi_disp0#g" \
    -e "s#/tmp/deviceModel#$TMP/deviceModel#g" \
    -e "s#^export PATH=.*#export PATH=\"$TMP/bin:\$PATH\"#" "$CSCR" > "$TMP/cscr.sh"
echo '{"lumination": 7, "hue": 10, "saturation": 12, "contrast": 15}' > "$TMP/system.json"
: > "$TMP/mi_disp0"
printf 285 > "$TMP/deviceModel"
sh "$TMP/cscr.sh" > /dev/null 2>&1
check "same values as Settings > Display and the Flip at boot" test "$(cat "$TMP/mi_disp0")" = "csc 0 3 60 50 42 60 0 0"
# No lumination: the model's default (10 on the Mini+ and Flip, 7 on the Mini)
echo '{"hue": 10, "saturation": 12, "contrast": 15}' > "$TMP/system.json"
for m in 285:45 354:45 283:42 none:42; do
    if [ "${m%%:*}" = none ]; then rm -f "$TMP/deviceModel"; else printf '%s' "${m%%:*}" > "$TMP/deviceModel"; fi
    : > "$TMP/mi_disp0"
    sh "$TMP/cscr.sh" > /dev/null 2>&1
    check "no lumination on model ${m%%:*}: its default" test "$(cat "$TMP/mi_disp0")" = "csc 0 3 60 50 ${m##*:} 60 0 0"
done
rm -f "$TMP/deviceModel"
fl=$(sed -n '/^restore_display_colours() {/,/^}/p' "$RUNTIME" | grep -o 'csc 0 3 [^>]*')
check "runtime.sh's formulas are the same" test "$fl" = 'csc 0 3 $(((_c + 15) * 2)) $((_h * 5)) $((_l + 35)) $((_s * 5)) 0 0" '
end

begin disp_csc_reset_repairs_lost_colour_processing
CSCR="$ROOT/static/build/.tmp_update/script/disp_csc_reset.sh"
mkdir -p "$TMP/bin"
sed -e "s#^export sysdir=.*#export sysdir=\"$TMP\"#" -e "s#/proc/mi_modules/mi_disp/mi_disp0#$TMP/mi_disp0#g" \
    -e "s#^export PATH=.*#export PATH=\"$TMP/bin:\$PATH\"#" "$CSCR" > "$TMP/cscr.sh"
printf '#!/bin/sh\necho "killall $*" >> "%s/calls"\n[ "$*" = "-0 disp_init" ] && [ -e "%s/di_running" ] && exit 0\n[ "$1" = -0 ] && exit 1\nexit 0\n' "$TMP" "$TMP" > "$TMP/bin/killall"
printf '#!/bin/sh\necho disp_init >> "%s/calls"\nsleep 0.3\necho " Interface DevTiming CscMatrix Luma" >> "%s/mi_disp0"\n' "$TMP" "$TMP" > "$TMP/bin/disp_init"
chmod +x "$TMP/bin/killall" "$TMP/bin/disp_init"
echo " Interface DevTiming CscMatrix Luma" > "$TMP/mi_disp0"
sh "$TMP/cscr.sh" --repair > /dev/null
check "colour processing there: nothing done" sh -c 'test $? -eq 0 && test ! -e "$1/calls"' _ "$TMP"
echo " DevStatus IrqNum" > "$TMP/mi_disp0"
out=$(sh "$TMP/cscr.sh" --repair)
rc=$?
check "lost: /dev/l (l) and an old disp_init stopped, disp_init started" test "$(cat "$TMP/calls")" = "killall -0 disp_init
killall -2 l
killall -9 disp_init
killall -0 l
killall -0 l
disp_init"
check "and waited until it is back" sh -c 'test $1 -eq 0 && grep -q CscMatrix "$2/mi_disp0"' _ "$rc" "$TMP"
check "each step's time said (for the log)" sh -c 'printf "%s" "$1" | grep -q "/dev/l stopped (.* -> .* s)" && printf "%s" "$1" | grep -q "CSC section back (.* -> .* s)"' _ "$out"
check "no colours written to the CSC-less node in --repair mode" sh -c '! grep -q "^csc " "$1/mi_disp0"' _ "$TMP"
: > "$TMP/calls"
printf '#!/bin/sh\necho disp_init >> "%s/calls"\n' "$TMP" > "$TMP/bin/disp_init"
echo " DevStatus IrqNum" > "$TMP/mi_disp0"
sh "$TMP/cscr.sh" --repair > /dev/null
check "not back after 3 s: said so" test $? -eq 1
echo " DevStatus IrqNum" > "$TMP/mi_disp0"
: > "$TMP/calls"
: > "$TMP/di_running"
( sleep 0.2; echo " Interface DevTiming CscMatrix Luma" >> "$TMP/mi_disp0" ) &
out=$(sh "$TMP/cscr.sh" --repair)
wait
check "a disp_init already setting it up (PICO-8 exiting): waited for, nothing restarted" sh -c 'test "$(cat "$1/calls")" = "killall -0 disp_init" && printf "%s" "$2" | grep -q "already running"' _ "$TMP" "$out"
rm -f "$TMP/di_running"
rm -f "$TMP/mi_disp0"
: > "$TMP/calls"
printf '#!/bin/sh\necho disp_init >> "%s/calls"\nsleep 0.3\necho " Interface DevTiming CscMatrix Luma" >> "%s/mi_disp0"\n' "$TMP" "$TMP" > "$TMP/bin/disp_init"
sh "$TMP/cscr.sh" --repair > /dev/null
rc=$?
check "mi_disp0 gone after a game: repaired the same way" sh -c 'test $1 -eq 0 && grep -q "^disp_init$" "$2/calls" && grep -q CscMatrix "$2/mi_disp0"' _ "$rc" "$TMP"
end

begin runtime_leaves_display_repair_to_mainui
# runtime.sh no longer repairs the display after games (robcodedev): Open
# MainUI and DisplayCal run disp_csc_reset.sh --repair when they need it.
check "no repair after games or the game menu" sh -c "! grep -qE 'repair_display|display_repair.log|display_has_csc' '$RUNTIME'"
check "disp_csc_reset.sh keeps its --repair branch (Open MainUI checks for it)" grep -q '"$1" = "--repair"' "$ROOT/static/build/.tmp_update/script/disp_csc_reset.sh"
end

begin displaycal_applied_at_boot_and_after_blue_light
BLUE_LIGHT="$ROOT/static/build/.tmp_update/script/blue_light.sh"
DISPLAYCAL="$ROOT/static/packages/App/DisplayCal/App/DisplayCal/launch.sh"
boot=$(sed -n '/^main() {/,/^}/p' "$RUNTIME")
check "boot: DisplayCal --apply when the app is installed" sh -c 'printf "%s\n" "$1" | grep -q "sh /mnt/SDCARD/App/DisplayCal/launch.sh --apply"' _ "$boot"
check "boot: after /tmp/deviceSN and the Blue Light Filter, before the boot screen" sh -c 'printf "%s\n" "$1" | grep -n -e "> /tmp/deviceSN" -e "blue_light.sh enable$" -e "DisplayCal/launch.sh --apply" -e "bootScreen \"Boot\"" | cut -d: -f1 | tr "\n" " " | awk "{exit !(NF == 4 && \$1 < \$2 && \$2 < \$3 && \$3 < \$4)}"' _ "$boot"
check "boot: the filter and DisplayCal in one background job" sh -c 'printf "%s\n" "$1" | sed -n "/^    (\$/,/^    ) &\$/p" | grep -q -e "blue_light.sh check\$" && printf "%s\n" "$1" | sed -n "/^    (\$/,/^    ) &\$/p" | grep -q "DisplayCal/launch.sh --apply"' _ "$boot"
# disable_blue_light_filter, with the display node and the app in $TMP
eval "$(extract_fn "$BLUE_LIGHT" disable_blue_light_filter | sed "s#/proc/mi_modules/mi_disp/mi_disp0#$TMP/node#")"
check_disp_init() { :; }
usleep() { :; }
sysdir=$TMP
mkdir -p "$TMP/config/display"
echo 8421504 > "$TMP/config/display/blueLightRGB"
blf_key_on=$TMP/blfOn
blf_key_on_user=$TMP/blfOnUser
displaycal_launcher=$TMP/displaycal.sh
printf 'if [ -e "%s/blfOn" ]; then echo "$* with the filter flag" >> "%s/calls"; else echo "$*" >> "%s/calls"; fi\n' "$TMP" "$TMP" "$TMP" > "$displaycal_launcher"
: > "$blf_key_on"; : > "$blf_key_on_user"
disable_blue_light_filter > /dev/null
check "filter off: DisplayCal --apply once, after the flag is gone" test "$(cat "$TMP/calls")" = "--apply"
rm -f "$displaycal_launcher" "$TMP/calls"
: > "$blf_key_on"
disable_blue_light_filter > /dev/null
check "filter off without DisplayCal: nothing else run" test ! -e "$TMP/calls"
unset -f disable_blue_light_filter check_disp_init usleep
# DisplayCal's own --apply: this device's profile, B G R order, nothing with the filter on
mkdir -p "$TMP/dc"
printf 'R=120\nG=130\nB=140\nW=0\n' > "$TMP/dc/cal-0abc.cfg"
echo 0abc > "$TMP/sn"
: > "$TMP/dcnode"
DISPLAYCAL_CFGDIR=$TMP/dc DISPLAYCAL_NODE=$TMP/dcnode DISPLAYCAL_SERIAL_FILE=$TMP/sn DISPLAYCAL_BLF_FLAG=$TMP/blf SERIAL_NUMBER= sh "$DISPLAYCAL" --apply
check "DisplayCal --apply writes this device's profile (B G R)" test "$(cat "$TMP/dcnode")" = "colortemp 0 0 0 0 140 130 120"
: > "$TMP/dcnode"; : > "$TMP/blf"
DISPLAYCAL_CFGDIR=$TMP/dc DISPLAYCAL_NODE=$TMP/dcnode DISPLAYCAL_SERIAL_FILE=$TMP/sn DISPLAYCAL_BLF_FLAG=$TMP/blf SERIAL_NUMBER= sh "$DISPLAYCAL" --apply
check "DisplayCal --apply: nothing while the filter is on" test ! -s "$TMP/dcnode"
rm -f "$TMP/blf"; echo 0def > "$TMP/sn"
DISPLAYCAL_CFGDIR=$TMP/dc DISPLAYCAL_NODE=$TMP/dcnode DISPLAYCAL_SERIAL_FILE=$TMP/sn DISPLAYCAL_BLF_FLAG=$TMP/blf SERIAL_NUMBER= sh "$DISPLAYCAL" --apply
check "DisplayCal --apply: nothing without a profile for this device" sh -c 'test ! -s "$1"' _ "$TMP/dcnode"
end

begin kids_mode_saves_swap_survives_an_interruption
backupdir=$TMP/Saves/KidsMode
current_profile=$TMP/Saves/CurrentProfile
kids_profile=$TMP/Saves/KidsProfile/Main
source_profile=Main
isolated_subdirs="saves states romScreens"
profile_isolation_marker=$backupdir/profile_isolation_active
eval "$(extract_fn "$KIDS_LOOP" apply_profile_isolation)"
eval "$(extract_fn "$KIDS_LOOP" restore_profile_isolation)"
log() { :; }
kids_sd() { # a parent's and a kid's saves, nothing swapped
    rm -rf "$TMP/Saves"
    mkdir -p "$current_profile/saves" "$current_profile/states" "$kids_profile/saves"
    echo parent > "$current_profile/saves/p.srm"
    echo parent > "$current_profile/states/p.state"
    echo kid > "$kids_profile/saves/k.srm"
}
parent_back() {
    [ "$(cat "$current_profile/saves/p.srm" 2> /dev/null)" = parent ] &&
        [ "$(cat "$current_profile/states/p.state" 2> /dev/null)" = parent ] &&
        [ ! -e "$current_profile/saves/k.srm" ]
}
kids_sd
apply_profile_isolation
check "armed: the kid's saves in CurrentProfile" test "$(cat "$current_profile/saves/k.srm")" = kid
check "the parent's put away" test "$(cat "$backupdir/profile-parked-saves/p.srm")" = parent
restore_profile_isolation
check "unarmed: the parent's back" parent_back
check "the kid's kept for next time" test "$(cat "$kids_profile/saves/k.srm")" = kid
check "nothing left parked" sh -c '! ls "$1"/profile-parked-* > /dev/null 2>&1' _ "$backupdir"
# Cut short after the parent's saves were put away (the old code deleted them)
kids_sd
mkdir -p "$backupdir"
mv "$current_profile/saves" "$backupdir/profile-parked-saves"
apply_profile_isolation
check "an interrupted swap is finished: the parent's saves still parked" test "$(cat "$backupdir/profile-parked-saves/p.srm")" = parent
check "and the kid's in CurrentProfile" test "$(cat "$current_profile/saves/k.srm")" = kid
restore_profile_isolation
check "then put back as usual" parent_back
# Cut short after the kid's saves came in, before the marker
kids_sd
mkdir -p "$backupdir"
mv "$current_profile/saves" "$backupdir/profile-parked-saves"
mv "$kids_profile/saves" "$current_profile/saves"
apply_profile_isolation
restore_profile_isolation
check "cut short later: everything back" parent_back
check "the kid's saves kept" test "$(cat "$kids_profile/saves/k.srm")" = kid
# Put back cut short: the marker is still there, one folder already back
kids_sd
apply_profile_isolation
mv "$current_profile/saves" "$kids_profile/saves"
mv "$backupdir/profile-parked-saves" "$current_profile/saves"
apply_profile_isolation
check "armed again after a cut restore: the kid's saves, not the parent's" test "$(cat "$current_profile/saves/k.srm")" = kid
restore_profile_isolation
check "and the parent's back after it" parent_back
# Restored twice, or never swapped: nothing moves
kids_sd
restore_profile_isolation
check "never swapped: nothing moved" parent_back
check "the kid's saves left alone" test "$(cat "$kids_profile/saves/k.srm")" = kid
unset -f apply_profile_isolation restore_profile_isolation log kids_sd parent_back
end

begin kids_mode_cleaned_up_when_kidmode_deleted_from_a_computer
# runtime.sh at boot: only with .kidmode gone and something of Kids Mode left
eval "$(extract_fn "$RUNTIME" kids_mode_cleanup_if_disarmed | sed "s#/mnt/SDCARD#$TMP#g")"
eval "$(extract_fn "$RUNTIME" kids_mode_restore_parked_saves | sed "s#/mnt/SDCARD#$TMP#g")"
log() { :; }
sysdir=$TMP
kids_mode_loop=$TMP/loop.sh
printf 'echo "$KIDSMODE_HOST $1" >> "%s/calls"\n' "$TMP" > "$kids_mode_loop"
mkdir -p "$TMP/Saves/KidsMode"
kids_mode_cleanup_if_disarmed
check "nothing of Kids Mode left: not run" test ! -e "$TMP/calls"
: > "$TMP/Saves/KidsMode/retroarch.cfg.backup"
: > "$TMP/.kidmode"
kids_mode_cleanup_if_disarmed
check "still armed: not run" test ! -e "$TMP/calls"
rm -f "$TMP/.kidmode"
kids_mode_cleanup_if_disarmed
check ".kidmode gone, its RetroArch backup left: cleanup run" test "$(cat "$TMP/calls")" = "runtime cleanup"
rm -f "$TMP/calls" "$TMP/Saves/KidsMode/retroarch.cfg.backup"
mkdir -p "$TMP/Saves/KidsMode/profile-parked-saves"
kids_mode_cleanup_if_disarmed
check "the parent's saves still parked: cleanup run" test "$(cat "$TMP/calls")" = "runtime cleanup"
rm -f "$TMP/calls" "$kids_mode_loop"
kids_mode_cleanup_if_disarmed
check "Kids Mode not installed: nothing to run" test ! -e "$TMP/calls"
check "at boot, before Kids Mode or a startup app" sh -c "awk '/^    kids_mode_cleanup_if_disarmed\$/ { c = NR } /^    if kids_mode_armed; then\$/ && !k { k = NR } END { exit !(c && k > c) }' '$RUNTIME'"
unset -f kids_mode_cleanup_if_disarmed kids_mode_restore_parked_saves log
# Kids Mode's own cleanup: what an unlock puts back
backupdir=$TMP/Saves/KidsMode
flagfile=$TMP/.kidmode
current_profile=$TMP/Saves/CurrentProfile
kids_profile=$TMP/Saves/KidsProfile/Main
source_profile=Main
isolated_subdirs="saves states romScreens"
profile_isolation_marker=$backupdir/profile_isolation_active
game_environment_marker=$backupdir/game_environment_ready
racfg=$TMP/retroarch.cfg; rabackup=$backupdir/retroarch.cfg.backup
keymapcfg=$TMP/keymap.json; keymapbackup=$backupdir/keymap.json.backup; keymapnone=$backupdir/keymap-was-absent
blfscript=$TMP/blue_light.sh; blfbackup=$backupdir/blue_light.sh.backup
gambatte_remap_dir=$current_profile/config/remaps/Gambatte
gambatte_remap_backup=$backupdir/gambatte-remaps.backup
gambatte_remap_absent=$backupdir/gambatte-remaps.was-absent
gambatte_remap_active=$backupdir/gambatte-remaps.active
onion_battery_display_flag=$TMP/hasBatteryDisplay; onion_battery_display_marker=$TMP/hbd_created
legacy_battery_warning_flag=$TMP/.noBatteryWarning; legacy_battery_warning_marker=$backupdir/no_battery_warning.created
kidsmode_active_flag=$TMP/kidsmode; checkoff_dst=$TMP/kidmode_checkoff.sh
timesup_since_file=$TMP/ts; timesup_resume_file=$TMP/tr
for f in ra_lock_keys cmd_cleanup restore_ra_lock restore_gambatte_remap_lock restore_blf_lock restore_profile_isolation restore_onion_battery_warning restore_keymap_override; do
    eval "$(extract_fn "$KIDS_LOOP" $f | sed 's#/tmp/kidsmode_ra_lock_checked#'"$TMP"'/ra_checked#')"
done
log() { :; }
sync() { :; }
restart_keymon() { :; }
infoPanel() { echo "$*" > "$TMP/panel"; }
# Kids Mode armed, then .kidmode deleted from a computer
rm -rf "$TMP/Saves"; mkdir -p "$backupdir/profile-parked-saves" "$current_profile/saves" "$kids_profile"
echo 'kiosk_mode_enable = "false"' > "$rabackup"; echo 'kiosk_mode_enable = "true"' > "$racfg"
echo parent-keys > "$keymapbackup"; echo kid-keys > "$keymapcfg"
echo parent > "$backupdir/profile-parked-saves/p.srm"
echo kid > "$current_profile/saves/k.srm"
: > "$profile_isolation_marker"; : > "$game_environment_marker"; : > "$checkoff_dst"
: > "$legacy_battery_warning_flag"; : > "$legacy_battery_warning_marker"
: > "$flagfile"
cmd_cleanup
check "armed: cleanup does nothing" has_line "$racfg" 'kiosk_mode_enable = "true"'
rm -f "$flagfile"
cmd_cleanup
check "RetroArch's config back" has_line "$racfg" 'kiosk_mode_enable = "false"'
check "the keymap back" test "$(cat "$keymapcfg")" = parent-keys
check "the parent's saves back" test "$(cat "$current_profile/saves/p.srm")" = parent
check "the kid's kept for next time" test "$(cat "$kids_profile/saves/k.srm")" = kid
check "Onion's battery warning back" test ! -e "$legacy_battery_warning_flag"
check "shutdown hook and markers gone" sh -c 'test ! -e "$1" && test ! -e "$2" && test ! -e "$3"' _ "$checkoff_dst" "$game_environment_marker" "$profile_isolation_marker"
check "a message says so" grep -q "Turned off from a computer" "$TMP/panel"
: > "$game_environment_marker"
cmd_cleanup "why" "Kids Mode is incomplete"
check "its own words when the app is incomplete" grep -q "Kids Mode is incomplete" "$TMP/panel"
check "and the same cleanup" test ! -e "$game_environment_marker"
unset -f ra_lock_keys cmd_cleanup restore_ra_lock restore_gambatte_remap_lock restore_blf_lock restore_profile_isolation restore_onion_battery_warning restore_keymap_override log sync restart_keymon infoPanel
check "the parent's profile is read for cleanup too (Guest stays Guest)" sh -c "sed -n '/^detect_source_profile() {/,/^}/p' '$KIDS_LOOP' | grep -q 'cleanup'"
check "kid_mode_loop.sh has the cleanup command" sh -c "grep -q '^    cleanup)\$' '$KIDS_LOOP'"
check "an incomplete app puts back what Kids Mode locked, not only the flag" sh -c "sed -n '/^cmd_run() {/,/^}/p' '$KIDS_LOOP' | sed -n '/runtime incomplete/,/return 1/p' | grep -q 'cmd_cleanup '"
check "a profile switch stops when the parked saves cannot go back" sh -c "sed -n '/^switch_kids_profile() {/,/^}/p' '$KIDS_LOOP' | grep -q 'if ! restore_profile_isolation; then'"
check "no video is started once the play time is over" sh -c "sed -n '/PLAY | RESTART)/,/play_video/p' '$KIDS_LOOP' | grep -q 'timer_remaining.* = \"0\" \] && continue'"
end

begin blue_light_no_second_disp_init_after_a_repair
BLF="$ROOT/static/build/.tmp_update/script/blue_light.sh"
REC="$ROOT/static/build/.tmp_update/script/screen_recorder.sh"
eval "$(extract_fn "$BLF" check_disp_init | sed "s#/proc/mi_modules/mi_disp/mi_disp0#$TMP/mi_disp0#")"
sysdir=$TMP
mkdir -p "$TMP/bin"
printf '#!/bin/sh\necho started >> "%s/started"\n' "$TMP" > "$TMP/bin/disp_init"; chmod +x "$TMP/bin/disp_init"
pgrep() { [ "$L_RUNNING" = 1 ]; }
killall() { [ "$DI_RUNNING" = 1 ]; }
sleep() { :; }
: > "$TMP/mi_disp0"
export L_RUNNING=1 DI_RUNNING=0; check_disp_init; _rc=$?; unset L_RUNNING DI_RUNNING; rc $_rc; wait
check "/dev/l running: none started" test ! -e "$TMP/started"
export L_RUNNING=0 DI_RUNNING=1; check_disp_init; _rc=$?; unset L_RUNNING DI_RUNNING; rc $_rc; wait
check "disp_init running in its place (after a repair): none started" test ! -e "$TMP/started"
export L_RUNNING=0 DI_RUNNING=0; check_disp_init; _rc=$?; unset L_RUNNING DI_RUNNING; rc $_rc; wait
check "neither running: disp_init started" test "$(cat "$TMP/started" 2> /dev/null)" = started
rm -f "$TMP/started" "$TMP/mi_disp0"
export L_RUNNING=0 DI_RUNNING=1; check_disp_init; _rc=$?; unset L_RUNNING DI_RUNNING; rc $_rc; wait
check "mi_disp0 gone: started, as before" test -e "$TMP/started"
unset -f check_disp_init
rm -f "$TMP/started"
eval "$(extract_fn "$REC" check_disp_init)"
export L_RUNNING=0 DI_RUNNING=1; check_disp_init; _rc=$?; unset L_RUNNING DI_RUNNING; rc $_rc; wait
check "screen recorder: none started with disp_init running" test ! -e "$TMP/started"
export L_RUNNING=0 DI_RUNNING=0; check_disp_init; _rc=$?; unset L_RUNNING DI_RUNNING; rc $_rc; wait
check "screen recorder: started when neither runs" test -e "$TMP/started"
unset -f check_disp_init pgrep killall sleep
end

begin kids_mode_favorites_sorted_with_an_app_among_them
favfile=$TMP/favourite.json
backupdir=$TMP/Saves/KidsMode
favorites_signature_file=$backupdir/favorites_repaired.cksum
eval "$(extract_fn "$KIDS_LOOP" repair_onion_favorites_if_needed)"
eval "$(extract_fn "$KIDS_LOOP" favorites_signature)"
repair_favourites() { :; }
log() { echo "$*" >> "$TMP/log"; }
tools() { :; } # Onion's favfix: nothing to repair here
printf '%s\n' '{"label":"Zelda","rompath":"/mnt/SDCARD/Roms/GBA/z.gba"}' \
    '{"label":"Kids App","launch":"/mnt/SDCARD/App/KidsMode/launch.sh"}' \
    '{"label":"alpha","rompath":"/mnt/SDCARD/Roms/GB/a.gb"}' > "$favfile"
repair_onion_favorites_if_needed
check "with an app among them, all favorites sorted A-Z" test "$(jq -r .label "$favfile" | tr '\n' ,)" = "alpha,Kids App,Zelda,"
check "the app kept" grep -q KidsMode "$favfile"
check "the log says sorted" grep -q "sorted A-Z" "$TMP/log"
unset -f repair_onion_favorites_if_needed favorites_signature repair_favourites log tools
end

begin kids_mode_unlock_keeps_retroarch_changes_made_meanwhile
racfg=$TMP/retroarch.cfg
backupdir=$TMP/Saves/KidsMode
rabackup=$backupdir/retroarch.cfg.backup
cfg_allow_save_states=false
log() { :; }
eval "$(extract_fn "$KIDS_LOOP" ra_lock_keys)"
eval "$(extract_fn "$KIDS_LOOP" restore_ra_state_bindings)"
eval "$(sed -n '/^apply_ra_lock() {/,/RetroArch kiosk lock applied/p' "$KIDS_LOOP"; echo '}')"
eval "$(extract_fn "$KIDS_LOOP" restore_ra_lock)"
printf '%s\n' 'video_scale = "3"' 'kiosk_mode_enable = "false"' 'settings_show_video = "true"' \
    'input_reset_btn = "2"' 'menu_driver = "rgui"' > "$racfg"
mkdir -p "$backupdir"
apply_ra_lock
check "armed: kiosk on" has_line "$racfg" 'kiosk_mode_enable = "true"'
# Meanwhile (a RetroArch update, say): one setting changed, one added
sed -i 's/^video_scale = "3"$/video_scale = "4"/' "$racfg"
echo 'new_option = "x"' >> "$racfg"
restore_ra_lock
check "unlocked: kiosk back as it was" has_line "$racfg" 'kiosk_mode_enable = "false"'
check "a menu setting back" has_line "$racfg" 'settings_show_video = "true"'
check "a hotkey back" has_line "$racfg" 'input_reset_btn = "2"'
check "a setting Kids Mode added and was not there: out" sh -c "! grep -q '^input_menu_toggle_btn' '$racfg'"
check "a change made meanwhile kept" has_line "$racfg" 'video_scale = "4"'
check "a setting added meanwhile kept" has_line "$racfg" 'new_option = "x"'
check "each locked setting once" test "$(grep -c '^kiosk_mode_enable' "$racfg")" = 1
check "the backup gone" test ! -e "$rabackup"
rm -f "$racfg"; echo 'kiosk_mode_enable = "false"' > "$rabackup"
restore_ra_lock
check "retroarch.cfg gone meanwhile: the backup put back whole" has_line "$racfg" 'kiosk_mode_enable = "false"'
unset -f ra_lock_keys restore_ra_state_bindings apply_ra_lock restore_ra_lock log
end

begin displaycal_apply_skips_neutral_and_too_dark
DISPLAYCAL="$ROOT/static/packages/App/DisplayCal/App/DisplayCal/launch.sh"
mkdir -p "$TMP/dc"; echo 0abc > "$TMP/sn"
dc_apply() { : > "$TMP/dcnode"; DISPLAYCAL_CFGDIR=$TMP/dc DISPLAYCAL_NODE=$TMP/dcnode DISPLAYCAL_SERIAL_FILE=$TMP/sn DISPLAYCAL_BLF_FLAG=$TMP/blf SERIAL_NUMBER= sh "$DISPLAYCAL" --apply 2> /dev/null; }
printf 'R=128\nG=128\nB=128\nW=0\n' > "$TMP/dc/cal-0abc.cfg"
dc_apply
check "a neutral profile: nothing written at boot" sh -c 'test ! -s "$1"' _ "$TMP/dcnode"
printf 'R=0\nG=0\nB=0\nW=0\n' > "$TMP/dc/cal-0abc.cfg"
dc_apply; rc=$?
check "a black profile (total under 70): refused" test "$rc" = 2
check "and nothing written" sh -c 'test ! -s "$1"' _ "$TMP/dcnode"
printf 'R=30\nG=20\nB=20\nW=0\n' > "$TMP/dc/cal-0abc.cfg"
dc_apply
check "a dark one at the limit (70) is written" test "$(cat "$TMP/dcnode")" = "colortemp 0 0 0 0 20 20 30"
unset -f dc_apply
end

# ---- scraper/clean_media.sh: media of deleted games ----

CLEAN_MEDIA="$ROOT/static/build/.tmp_update/script/scraper/clean_media.sh"
cm_sd() { # a console with games still there, one deleted game, a hidden one
    R=$TMP/Roms
    mkdir -p "$R/GB/Imgs" "$R/GB/media/box2dfront" "$R/GB/media/screenshot" \
        "$R/GB/media/videos" "$R/GB/Manuals" "$R/GB/Zelda" "$R/GB/.disc"
    touch "$R/GB/Tetris (World).gb" "$R/GB/Zelda/Links Awakening.gb" "$R/GB/Sonic v1.1.zip"
    touch "$R/GB/Imgs/Tetris (World).png" "$R/GB/Imgs/links awakening.png" \
        "$R/GB/Imgs/Sonic v1.1.png" "$R/GB/Imgs/Zelda.png" "$R/GB/Imgs/Mario.png" "$R/GB/Imgs/.keep"
    touch "$R/GB/media/box2dfront/Mario.png" "$R/GB/media/screenshot/Mario-1.png" \
        "$R/GB/media/screenshot/Tetris (World)-1.png" "$R/GB/media/videos/Mario.mp4" \
        "$R/GB/media/.gameinfo_none" "$R/GB/Manuals/Mario.pdf" "$R/GB/Manuals/Tetris (World).pdf"
}

begin clean_media_lists_only_media_of_deleted_games
cm_sd
out=$(sh "$CLEAN_MEDIA" "$R/GB" | sed "s|$R/||" | sort | tr '\n' '|')
check "Mario's cover, box, screenshot, video and manual listed" \
    test "$out" = "GB/Imgs/Mario.png|GB/Manuals/Mario.pdf|GB/media/box2dfront/Mario.png|GB/media/screenshot/Mario-1.png|GB/media/videos/Mario.mp4|"
check "a listing deletes nothing" test -f "$R/GB/Imgs/Mario.png"
end

begin clean_media_delete_keeps_what_games_use
cm_sd
sh "$CLEAN_MEDIA" --delete "$R/GB" > /dev/null
check "Mario's media deleted" test ! -e "$R/GB/Imgs/Mario.png" -a ! -e "$R/GB/media/videos/Mario.mp4" -a ! -e "$R/GB/Manuals/Mario.pdf"
check "a ROM's cover kept" test -f "$R/GB/Imgs/Tetris (World).png"
check "a cover in another case kept (ROM in a subfolder)" test -f "$R/GB/Imgs/links awakening.png"
check "a name with dots kept" test -f "$R/GB/Imgs/Sonic v1.1.png"
check "folder art kept" test -f "$R/GB/Imgs/Zelda.png"
check "a gallery picture of a ROM kept" test -f "$R/GB/media/screenshot/Tetris (World)-1.png"
check "hidden files kept" test -f "$R/GB/Imgs/.keep" -a -f "$R/GB/media/.gameinfo_none"
check "ROMs untouched" test -f "$R/GB/Tetris (World).gb" -a -f "$R/GB/Zelda/Links Awakening.gb"
end

begin clean_media_skips_console_without_games
mkdir -p "$TMP/Roms/GBA/Imgs" && touch "$TMP/Roms/GBA/Imgs/Waiting.png"
out=$(sh "$CLEAN_MEDIA" --delete "$TMP/Roms/GBA/")
check "reported as skipped" test "$out" = "skip: $TMP/Roms/GBA"
check "its covers kept (ROMs not copied yet)" test -f "$TMP/Roms/GBA/Imgs/Waiting.png"
end

begin clean_media_several_consoles
cm_sd
mkdir -p "$R/MD/Imgs" && touch "$R/MD/Sonic.md" "$R/MD/Imgs/Sonic.png" "$R/MD/Imgs/Mario.png"
out=$(sh "$CLEAN_MEDIA" "$R"/*/ | sed "s|$R/||" | grep -c "Mario")
check "each console checked on its own (Mario on GB and MD)" test "$out" -eq 6
check "MD's Sonic kept" sh -c '! sh "$1" "$2" | grep -q Sonic' _ "$CLEAN_MEDIA" "$R/MD"
end

begin clean_media_keeps_media_named_in_the_game_lists
# A hand-made or imported list may name a cover unlike its ROM: it was
# listed as left behind and deleted.
cm_sd
cat > "$R/GB/miyoogamelist.xml" <<'XML'
<?xml version="1.0"?>
<gameList>
  <game>
    <path>./Tetris (World).gb</path>
    <name>Tetris</name>
    <image>./Imgs/tetris-cover.png</image>
  </game>
  <game><path>./Sonic v1.1.zip</path><image>./Imgs/Sonic &amp; Knuckles.png</image><image>./Imgs/Tom&apos;s.png</image></game>
</gameList>
XML
printf '<gameList><game><path>./Zelda</path><video>./media/videos/zelda-intro.mp4</video><manual>/mnt/SDCARD/Roms/GB/Manuals/LA manual.pdf</manual></game></gameList>\n' > "$R/GB/gamelist.xml"
touch "$R/GB/Imgs/tetris-cover.png" "$R/GB/Imgs/Sonic & Knuckles.png" "$R/GB/Imgs/Tom's.png" \
    "$R/GB/media/videos/zelda-intro.mp4" "$R/GB/Manuals/LA manual.pdf"
out=$(sh "$CLEAN_MEDIA" "$R/GB" | sed "s|$R/||" | sort | tr '\n' '|')
check "only Mario's media listed ($out)" \
    test "$out" = "GB/Imgs/Mario.png|GB/Manuals/Mario.pdf|GB/media/box2dfront/Mario.png|GB/media/screenshot/Mario-1.png|GB/media/videos/Mario.mp4|"
sh "$CLEAN_MEDIA" --delete "$R/GB" > /dev/null
check "a cover named in miyoogamelist.xml kept" test -f "$R/GB/Imgs/tetris-cover.png"
check "names with &amp; and &apos; kept" test -f "$R/GB/Imgs/Sonic & Knuckles.png" -a -f "$R/GB/Imgs/Tom's.png"
check "a video and a manual named in gamelist.xml kept" test -f "$R/GB/media/videos/zelda-intro.mp4" -a -f "$R/GB/Manuals/LA manual.pdf"
check "Mario's cover still deleted" test ! -e "$R/GB/Imgs/Mario.png"
printf '<gameList><game><image>./Imgs/A &amp;amp; B.png</image></game></gameList>\n' > "$R/GB/gamelist.xml"
eval "$(extract_fn "$CLEAN_MEDIA" listed_media)"
out=$(listed_media "$R/GB" | tail -n 1)
check "an entity decoded once (&amp;amp; is &amp;: $out)" test "$out" = "./Imgs/A &amp; B.png"
unset -f listed_media
# BusyBox awk reads "name (" as a function call: no such concatenation
check "no 'name (' in listed_media's awk (BusyBox)" sh -c '! sed -n "/^listed_media()/,/^}/p" "$1" | sed 1d | grep -v "^[[:space:]]*#" | grep -E "[A-Za-z0-9_\"] \(" | grep -vE "\b(if|for|while|else|function)\b \("' _ "$CLEAN_MEDIA"
end

begin clean_media_skips_a_console_it_could_not_read
# find skipped a folder it could not read (an SD card error) and went on:
# its games were missing from the list and their covers listed, then
# deleted. Here find reads everything but exits 1 for one folder.
cm_sd
mkdir -p "$TMP/bin"
real_find=$(command -v find)
printf '#!/bin/sh\n"%s" "$@"\nrc=$?\ncase "$1" in "$CM_FIND_FAIL") exit 1 ;; esac\ncase "$1 $*" in "$CM_FIND_FAIL_DIRS "*"( -type d )"*) exit 1 ;; esac\nexit $rc\n' "$real_find" > "$TMP/bin/find"
chmod +x "$TMP/bin/find"
out=$(CM_FIND_FAIL="$R/GB" PATH="$TMP/bin:$PATH" sh "$CLEAN_MEDIA" --delete "$R/GB")
check "reported as skipped ($out)" test "$out" = "skip: $R/GB (read error)"
check "nothing deleted" test -f "$R/GB/Imgs/Mario.png"
R2=$TMP/Roms
mkdir -p "$R2/ARCADE/Imgs" "$R2/MAME2003" "$TMP/RApp/mame2003"
printf '{"rompath":"../../Roms/MAME2003","imgpath":"../../Roms/ARCADE/Imgs"}\n' > "$TMP/RApp/mame2003/config.json"
touch "$R2/ARCADE/sf2.zip" "$R2/MAME2003/mslug.zip" "$R2/ARCADE/Imgs/mslug.png" "$R2/ARCADE/Imgs/gone.png"
out=$(CM_FIND_FAIL="$R2/MAME2003" PATH="$TMP/bin:$PATH" sh "$CLEAN_MEDIA" --delete "$R2/ARCADE")
check "a shared folder not read: skipped ($out)" test "$out" = "skip: $R2/ARCADE (read error)"
check "its covers kept" test -f "$R2/ARCADE/Imgs/mslug.png" -a -f "$R2/ARCADE/Imgs/gone.png"
out=$(PATH="$TMP/bin:$PATH" sh "$CLEAN_MEDIA" "$R2/ARCADE" | sed "s|$R2/||")
check "read normally: cleaned as before ($out)" test "$out" = "ARCADE/Imgs/gone.png"
# Only the folders pass fails: "Vol. 1" would be taken for a file
mkdir -p "$R/GB/Vol. 1" && touch "$R/GB/Vol. 1/game.gb" "$R/GB/Imgs/Vol. 1.png"
out=$(CM_FIND_FAIL_DIRS="$R/GB" PATH="$TMP/bin:$PATH" sh "$CLEAN_MEDIA" --delete "$R/GB")
check "folders pass not read: skipped ($out)" test "$out" = "skip: $R/GB (read error)"
check "folder art kept" test -f "$R/GB/Imgs/Vol. 1.png"
check "the Scraper menu counts the consoles not checked" grep -q 'grep -c "^skip: .\* (read error)\$"' "$ROOT/static/build/.tmp_update/script/scraper/menu.sh"
end

begin clean_media_keeps_covers_shared_with_another_console
# MAME 2003 (RApp) lists Roms/MAME2003 with its covers in Roms/ARCADE/Imgs
R=$TMP/Roms
mkdir -p "$R/ARCADE/Imgs" "$R/MAME2003/Neo" "$TMP/RApp/mame2003" "$TMP/Emu/ARCADE"
printf '{\n"rompath":"../../Roms/MAME2003",\n"imgpath":"../../Roms/ARCADE/Imgs/"\n}\n' > "$TMP/RApp/mame2003/config.json"
printf '{"rompath":"../../Roms/ARCADE","imgpath":"../../Roms/ARCADE/Imgs"}\n' > "$TMP/Emu/ARCADE/config.json"
touch "$R/ARCADE/sf2.zip" "$R/MAME2003/mslug.zip" "$R/MAME2003/Neo/kof98.zip"
touch "$R/ARCADE/Imgs/sf2.png" "$R/ARCADE/Imgs/mslug.png" "$R/ARCADE/Imgs/Neo.png" \
    "$R/ARCADE/Imgs/kof98.png" "$R/ARCADE/Imgs/gone.png"
out=$(sh "$CLEAN_MEDIA" "$R/ARCADE" | sed "s|$R/||" | tr '\n' '|')
check "only the cover of a game in neither folder listed" test "$out" = "ARCADE/Imgs/gone.png|"
rm "$TMP/RApp/mame2003/config.json"
out=$(sh "$CLEAN_MEDIA" "$R/ARCADE" | grep -c "mslug")
check "without the MAME 2003 config its covers count as deleted games" test "$out" -eq 1
end

begin scraper_press_a_waits_for_a_only
# "Press A to continue" went on with any button: a D-pad press (or the
# console resting on its buttons) skipped the message. Only A (a space)
# ends it now, or the end of input (the terminal closed).
SCRAPER_MENU="$ROOT/static/build/.tmp_update/script/scraper/menu.sh"
eval "$(extract_fn "$SCRAPER_MENU" press_a)"
eval "$(extract_fn "$SCRAPER_MENU" keys_red)"
noansi() { sed "s/$(printf '\033')\[[0-9;]*m//g"; }
# press_a needs read -n (the console's ash has it; dash, the usual sh of a
# PC, does not): run it with a shell that has it, not with the one that
# happens to run this file.
pa_sh=
for _s in sh bash "busybox sh"; do
    if (printf 'x' | $_s -c 'IFS= read -n 1 -r _k') > /dev/null 2>&1; then
        pa_sh=$_s
        break
    fi
done
pa_run() { # input, then what follows press_a in the same shell
    printf "$1" | $pa_sh -c 'for f in keys_red press_a; do eval "$(sed -n "/^$f() {/,/^}/p" "$1")"; done; press_a "Press A"; '"$2" _ "$SCRAPER_MENU" | noansi
}
if [ -n "$pa_sh" ]; then
    out=$(pa_run '\033[B\033[Dx\n z' 'cat')
    check "D-pad, a letter and START ignored, A goes on ($pa_sh)" test "$out" = "Press Az"
    out=$(pa_run 'abc' 'echo "|end"')
    check "no A before the end of input: no hang ($pa_sh)" test "$out" = "Press A|end"
else
    skip "press_a tests (no shell with read -n: sh, bash or busybox)"
fi
out=$(keys_red "Press A to continue, MENU to exit" | sed "s/$(printf '\033')\[31;1m/<R>/g; s/$(printf '\033')\[0m/<\/R>/g")
check "the buttons in red, as in the menus ($out)" test "$out" = "Press <R>A</R> to continue, <R>MENU</R> to exit"
out=$(set -f; keys_red "Press A" > /dev/null; case $- in *f*) echo off ;; *) echo on ;; esac)
check "keys_red leaves a script's set -f as it was ($out)" test "$out" = off
out=$(set +f; keys_red "Press A" > /dev/null; case $- in *f*) echo off ;; *) echo on ;; esac)
check "keys_red leaves globbing on when it was ($out)" test "$out" = on
unset -f pa_run
for f in "$SCRAPER_MENU" "$ROOT/static/build/.tmp_update/script/scraper/scrap_screenscraper.sh" "$ROOT/static/build/.tmp_update/script/ota_update.sh"; do
    check "no any-key prompt left in ${f##*/}" sh -c '! grep -q "read -n 1 -s -r -p" "$1"' _ "$f"
done
end

begin shellect_native_mode_answers_like_the_terminal
SHL="$ROOT/static/build/.tmp_update/script/shellect.sh"
mkdir -p "$TMP/bin"
{
    echo '#!/bin/sh'
    echo "for a; do printf '<%s>' \"\$a\"; done >> '$TMP/args'; echo >> '$TMP/args'"
    # like the real prompt: the index in --index-file (-1 for B, menu);
    # FAKE_IDX for an index the exit status cannot hold
    echo 'f=; while [ $# -gt 0 ]; do [ "$1" = --index-file ] && f=$2; shift; done'
    echo 'a=${FAKE_RC:-0}; case $a in 255) i=-1 ;; 254) i=menu ;; *) i=$a ;; esac'
    echo '[ -n "$FAKE_IDX" ] && i=$FAKE_IDX'
    echo '[ -n "$f" ] && [ -z "$FAKE_NO_ANSWER" ] && echo "$i" > "$f"'
    echo 'exit $a'
} > "$TMP/bin/prompt"
chmod +x "$TMP/bin/prompt"
sed "s#/mnt/SDCARD/.tmp_update/bin/prompt#$TMP/bin/prompt#" "$SHL" > "$TMP/shellect.sh"
r=$(printf 'One\nTwo\nBack' | SHELLECT_NATIVE=1 SHELLECT_PARENT=Scraper FAKE_RC=1 sh "$TMP/shellect.sh" -t "--== MAIN MENU ==--" -b x -s 2)
check "A on the second item: its text" test "$r" = Two
check "the path, the title in sentence case, the start item" grep -q "<-p><Scraper><-t><Main menu><-s><1><--index-file><[^>]*><One><Two><Back>" "$TMP/args"
r=$(printf 'One\nBack' | SHELLECT_NATIVE=1 FAKE_RC=255 sh "$TMP/shellect.sh" -t "--== X ==--")
check "B: the menu's Back item" test "$r" = Back
r=$(printf 'One\nTwo' | SHELLECT_NATIVE=1 FAKE_RC=255 sh "$TMP/shellect.sh" -t "--== X ==--")
check "B without a Back item: nothing" test -z "$r"
printf 'Yes\nNo' | SHELLECT_NATIVE=1 FAKE_RC=254 sh "$TMP/shellect.sh" -m -t x > /dev/null
check "MENU with -m: exit 2" test $? -eq 2
# More than 254 items (a big PDF or video folder): the exit status wraps
# at 256 and 254/255 are MENU and B, so the index comes from the file.
big=$(i=1; while [ $i -le 300 ]; do echo "file$i"; i=$((i + 1)); done)
r=$(printf '%s' "$big" | SHELLECT_NATIVE=1 FAKE_RC=4 FAKE_IDX=260 sh "$TMP/shellect.sh" -t x)
check "item 261 of 300: itself, not item 5 ($r)" test "$r" = file261
r=$(printf '%s' "$big" | SHELLECT_NATIVE=1 FAKE_RC=254 FAKE_IDX=254 sh "$TMP/shellect.sh" -m -t x)
check "item 255: itself, not MENU ($r, rc $?)" test "$r" = file255
r=$(printf '%s' "$big" | SHELLECT_NATIVE=1 FAKE_RC=255 FAKE_IDX=255 sh "$TMP/shellect.sh" -t x)
check "item 256: itself, not B ($r)" test "$r" = file256
r=$(printf 'One\nBack' | SHELLECT_NATIVE=1 FAKE_NO_ANSWER=1 sh "$TMP/shellect.sh" -t x)
rc=$?
check "prompt gave no answer: as B (Back), exit 3 ($r, $rc)" sh -c 'test "$1" -eq 3 && test "$2" = Back' _ "$rc" "$r"
r=$(printf 'One\nTwo' | SHELLECT_NATIVE=1 FAKE_NO_ANSWER=1 sh "$TMP/shellect.sh" -t x)
check "no answer and no Back item: nothing chosen ($r)" test -z "$r"
check "no index file left behind" sh -c '! ls /tmp/shellect_index.* > /dev/null 2>&1'
end

begin scraper_long_scraping_warning
# Before the long jobs (all the roms of a console, all favorites, all
# consoles): a warning that A starts and MENU cancels. Not for one game.
SCRAPER_MENU="$ROOT/static/build/.tmp_update/script/scraper/menu.sh"
noansi() { sed "s/$(printf '\033')\[[0-9;]*m//g"; }
lw_sh=
for _s in sh bash "busybox sh"; do
    if (printf 'x' | $_s -c 'IFS= read -n 1 -r _k') > /dev/null 2>&1; then
        lw_sh=$_s
        break
    fi
done
lw_run() { # input, background (true/false), args...
    _in=$1 _bg=$2
    shift 2
    printf "$_in" | ScrapeInBackground=$_bg $lw_sh -c 'clear() { :; }; stty() { :; }; for f in keys_red Long_Scraping_Warning; do eval "$(sed -n "/^$f() {/,/^}/p" "$1")"; done; shift; Long_Scraping_Warning "$@"; echo "|rc=$?"' _ "$SCRAPER_MENU" "$@" | noansi
}
if [ -n "$lw_sh" ]; then
    out=$(lw_run ' ' false "SCRAPE ALL GB ROMS")
    check "A starts it ($lw_sh)" sh -c 'printf "%s" "$1" | grep -q "|rc=0$"' _ "$out"
    check "the title" sh -c 'printf "%s" "$1" | grep -q -- "--== SCRAPE ALL GB ROMS ==--"' _ "$out"
    check "the warning" sh -c 'printf "%s" "$1" | grep -q "Long scraping sessions are not recommended:"' _ "$out"
    check "MENU stops it, in the foreground" sh -c 'printf "%s" "$1" | grep -q "You can stop at any time with MENU."' _ "$out"
    check "the keys" sh -c 'printf "%s" "$1" | grep -q "Press A to start, MENU to cancel."' _ "$out"
    check "what to do if it freezes" sh -c 'printf "%s" "$1" | grep -q "If it freezes, hold POWER for at least 10 seconds" && printf "%s" "$1" | grep -q "^until the device turns off.$"' _ "$out"
    out=$(lw_run '\033[Bx\n' false "SCRAPE ALL CONSOLES" "12 consoles to scrape: this may take hours.")
    check "other keys don't start it, end of input: 1" sh -c 'printf "%s" "$1" | grep -q "|rc=1$"' _ "$out"
    check "the first line (the count)" sh -c 'printf "%s" "$1" | grep -q "^12 consoles to scrape: this may take hours.$"' _ "$out"
    out=$(lw_run ' ' true "SCRAPE ALL FAVORITES")
    check "in the background: MENU doesn't stop it" sh -c 'printf "%s" "$1" | grep -q "It runs in the background once this menu closes." && ! printf "%s" "$1" | grep -q "stop at any time"' _ "$out"
else
    skip "Long_Scraping_Warning tests (no shell with read -n)"
fi
unset -f lw_run
fn() { sed -n "/^$1 *()/,/^}/p" "$SCRAPER_MENU"; }
check "all the roms of a console: the warning, not for one game" sh -c 'printf "%s" "$1" | grep -q "\"\$onerom\" != 1" && printf "%s" "$1" | grep -q "Long_Scraping_Warning \"SCRAPE ALL"' _ "$(fn Launch_Scraping)"
check "all favorites: the warning, after Wi-Fi" sh -c 'printf "%s" "$1" | grep -A1 "^ *Batch_Prepare" | grep -q "Long_Scraping_Warning \"SCRAPE ALL FAVORITES\""' _ "$(fn Scrape_Favorites)"
check "all consoles: the warning, after Wi-Fi" sh -c 'printf "%s" "$1" | grep -A2 "^ *Batch_Prepare" | grep -q "Long_Scraping_Warning \"SCRAPE ALL CONSOLES\""' _ "$(fn Scrape_All_Consoles)"
check "all consoles: no second confirmation" sh -c '! printf "%s" "$1" | grep -q "shellect"' _ "$(fn Scrape_All_Consoles)"
end

begin shellect_only_a_confirms
# Right (Esc [ C, "C2" in shellect's key case) and "l" confirmed like A: a
# D-pad press could run the highlighted item ("Delete 14 files").
SH_SELECT="$ROOT/static/build/.tmp_update/script/shellect.sh"
check "Right does not confirm" sh -c '! grep -qE "^[[:space:]]*[^#]*C2[|)]" "$1"' _ "$SH_SELECT"
check "A (space) still confirms" grep -q '^[[:space:]]*"\$space"?) # A' "$SH_SELECT"
end

begin ota_onion_progress_fits_the_screen
# The download shows an onion filling up. It must fit the terminal with 4
# free rows under it (MENU question, result or error + "Press A"), or the
# messages scroll it off the top.
OTA_SCRIPT="$ROOT/static/build/.tmp_update/script/ota_update.sh"
for f in onion_layout onion_draw onion_status onion_percent; do eval "$(extract_fn "$OTA_SCRIPT" $f)"; done
stty() { [ "$1" = size ] && echo "$FAKE_SIZE"; }
FAKE_SIZE="29 53"; onion_layout
check "Miyoo 29x53: a 20-row onion" test "$onion_rows" -eq 20
check "Miyoo 29x53: 4 rows under it end on the last row" test $((onion_msg_row + 3)) -eq 29
FAKE_SIZE="40 53"; onion_layout
check "taller screen: at most 4 more rows" test "$onion_rows" -eq 22
check "taller screen: rows left free" test $((onion_msg_row + 3)) -le 40
FAKE_SIZE="25 63"; onion_layout
check "16 px font, 25x63: the tall onion, 16 rows" test "$onion_tall" -eq 1 -a "$onion_rows" -eq 16
check "25x63: 4 rows under it end on the last row" test $((onion_msg_row + 3)) -eq 25
drawn=$(onion_draw 50 | sed "s/$(printf '\033')\[[0-9]*;1H/\n/g; s/$(printf '\033')\[[0-9;]*[mK]//g" | grep -c .)
check "25x63: the onion draws its 16 rows ($drawn)" test "$drawn" -eq 16
FAKE_SIZE="24 53"; onion_layout
check "short screen: the compact onion" test "$onion_tall" -eq 0 -a "$onion_rows" -eq 12
check "short screen: still fits" test $((onion_msg_row + 3)) -le 24
FAKE_SIZE=""; onion_layout
check "no stty: the Miyoo size" test "$onion_lines" -eq 29 -a "$onion_cols" -eq 53
FAKE_SIZE="29 53"; onion_layout
esc=$(printf '\033') # not \x1b in sed: GNU only
drawn=$(onion_draw 50 | sed "s/${esc}\[[0-9]*;1H/\n/g; s/${esc}\[[0-9;]*[mK]//g" | grep -c .)
check "the onion draws all its rows ($drawn)" test "$drawn" -eq 20
half=$(onion_draw 50 | sed "s/${esc}\[[0-9]*;1H/\n/g; s/${esc}\[[0-9;]*[mK]//g" | grep -c '#')
full=$(onion_draw 100 | sed "s/${esc}\[[0-9]*;1H/\n/g; s/${esc}\[[0-9;]*[mK]//g" | grep -c '#')
check "50% fills about half its body ($half of $full rows)" test "$half" -ge $((full / 2 - 1)) -a "$half" -le $((full / 2 + 1))
check "0% is empty" test "$(onion_draw 0 | grep -c '#')" -eq 0
# BusyBox awk reads "name (" as a function call: no such concatenation
check "no 'name (' in the onion's awk (BusyBox)" sh -c '! sed -n "/^onion_draw()/,/^}/p" "$1" | sed 1d | grep -v "^[[:space:]]*#" | grep -E "[A-Za-z0-9_\"] \(" | grep -vE "\b(if|for|while|else)\b \("' _ "$OTA_SCRIPT"
out=$(onion_status 5242880 10485760 | sed "s/${esc}\[[0-9;]*[HK]//g")
check "status: percent and MB ($out)" sh -c 'echo "$1" | grep -q "50%   5.0 / 10.0 MB"' _ "$out"
# bytes * 100 passed 2^31 after 21 MB: a 32-bit shell went negative
onion_percent 214748365 432013312; check "percent past 21 MB: 49 ($onion_pct)" test "$onion_pct" -eq 49
onion_percent 432013312 432013312; check "percent at the end: 100 ($onion_pct)" test "$onion_pct" -eq 100
onion_percent 500000000 432013312; check "percent never over 100 ($onion_pct)" test "$onion_pct" -eq 100
onion_percent 0 432013312; check "percent at the start: 0 ($onion_pct)" test "$onion_pct" -eq 0
onion_percent 1000 x; check "size not a number: 0 ($onion_pct)" test "$onion_pct" -eq 0
onion_percent 1000 ""; check "no size: 0 ($onion_pct)" test "$onion_pct" -eq 0
onion_percent 500 900; check "size under 1 KB: 0 ($onion_pct)" test "$onion_pct" -eq 0
onion_percent 1048576 4194304; check "returns 0 below 100 %" test $? -eq 0
check "no bytes * 100 in the updater" sh -c '! grep -nE "(done|\\\$1) \\* 100" "$1"' _ "$OTA_SCRIPT"
unset -f stty onion_layout onion_draw onion_status onion_percent
end

begin low_battery_shutdown_rechecks_the_battery
# keymon's "Low battery: save and quit" leaves /tmp/.lowBat; runtime.sh shut
# down after every later game exit, even after a charge. Now it checks the
# battery first.
RT="$ROOT/static/build/.tmp_update/runtime.sh"
eval "$(sed -n '/^low_battery_shutdown_due() {/,/^}/p' "$RT" | sed "s#/tmp/percBat#$TMP/percBat#; s#/sys/devices/gpiochip0/gpio/gpio59/value#$TMP/gpio59#")"
sysdir=$TMP; mkdir -p "$TMP/config/battery"; MODEL_MM=283
axp() { echo "0:$FAKE_AXP"; }
not_due() { ! low_battery_shutdown_due; }
DEVICE_ID=354; HAS_AXP=1; FAKE_AXP=70
echo 4 > "$TMP/percBat"
check "Plus at 4 %, default level, not charging: shut down" low_battery_shutdown_due
echo 6 > "$TMP/percBat"
check "6 %: still within 2 % of the level" low_battery_shutdown_due
echo 30 > "$TMP/percBat"
check "charged to 30 % since: no shutdown" not_due
echo 4 > "$TMP/percBat"; FAKE_AXP=74
check "on the charger: no shutdown" not_due
FAKE_AXP=70; echo 15 > "$TMP/config/battery/exitAt"; echo 14 > "$TMP/percBat"
check "level 15 %, at 14 %: shut down" low_battery_shutdown_due
rm -f "$TMP/percBat"
check "no reading: shut down, as before" low_battery_shutdown_due
DEVICE_ID=283; HAS_AXP=0; echo 1 > "$TMP/gpio59"; echo 3 > "$TMP/percBat"
check "Mini on the charger (gpio59): no shutdown" not_due
echo 0 > "$TMP/gpio59"
check "Mini off the charger at 3 %: shut down" low_battery_shutdown_due
check "the game exit uses it" grep -q "if low_battery_shutdown_due; then" "$RT"
unset -f axp not_due low_battery_shutdown_due; unset sysdir
end

# ---- update_networking.sh: no Play Activity resume behind the GameSwitcher ----

eval "$(extract_fn "$NETWORK" lowest_pid_named)"
eval "$(extract_fn "$NETWORK" play_activity_resume)"

begin time_sync_resume_skipped_behind_the_gameswitcher
proc_dir="$TMP/proc"
sysdir="$TMP/sd"
mkdir -p "$proc_dir/700" "$sysdir"
echo retroarch > "$proc_dir/700/comm"
playActivity() { echo "$*" >> "$TMP/pa"; }
play_activity_resume
check "game running: resumed" test "$(cat "$TMP/pa")" = "resume"
rm -f "$TMP/pa"
touch "$sysdir/.runGameSwitcher"
mkdir -p "$proc_dir/710"
echo gameSwitcher > "$proc_dir/710/comm"
play_activity_resume
check "GameSwitcher in front: not resumed" test ! -e "$TMP/pa"
rm -rf "$proc_dir/710"
play_activity_resume
check "flag left over, no GameSwitcher: resumed" test "$(cat "$TMP/pa")" = "resume"
unset -f playActivity
unset proc_dir
end

# ---- Scraper menu: "Delete media of deleted games" from the main menu ----
# Its two choices are Option6/Option7 of Menu_Main. Handled in Menu_Config,
# whose Option6 is "Back to Main Menu" and Option7 the Game Info media
# settings, those two started the media cleaning instead.

begin scraper_menu_clean_media_in_main_menu
menu_fn() { sed -n "/^$1 *()/,/^}/p" "$SCRAPER_MENU"; }
check "configuration: Back still goes to the main menu" sh -c 'printf "%s" "$1" | grep -qF "\"\$Option6\") Menu_Main ;;"' _ "$(menu_fn Menu_Config)"
check "main menu: one console" sh -c 'printf "%s" "$1" | grep -qF "\"\$Option6\") Clean_Media \"/mnt/SDCARD/Roms/\$CurrentSystem\" ;;"' _ "$(menu_fn Menu_Main)"
check "configuration: all consoles" sh -c 'printf "%s" "$1" | grep -qF "\"\$Option10\") Clean_Media all ;;"' _ "$(menu_fn Menu_Config)"
check "main menu: not all consoles" sh -c '! printf "%s" "$1" | grep -q "Clean_Media all"' _ "$(menu_fn Menu_Main)"
end

# ---- Scraper main menu: this game's commands only (zazzagone) ----
# The game's and its console's commands, then Configuration and Exit. The
# long jobs (all favorites, all consoles, media of all consoles) moved to
# Configuration: a wrong press could start hours of scraping. "Delete
# cover" was an empty item without a cover, and an empty choice (MENU)
# matched it: "Removed" for a file that wasn't there.

begin scraper_main_menu_game_commands_first
menu_fn() { sed -n "/^$1 *()/,/^}/p" "$SCRAPER_MENU"; }
# shellect stub: saves the items, answers with what mm_pick set (once)
# (it runs in $(...): the answer is a file, used once)
{ menu_fn Menu_Main; menu_fn Menu_Config; } | sed "s|/mnt/SDCARD/.tmp_update/script/shellect.sh|mm_shellect|" > "$TMP/mm.sh"
mm_shellect() { cat > "$TMP/items"; cat "$TMP/pick" 2> /dev/null; rm -f "$TMP/pick"; }
mm_pick() { printf '%s' "$1" > "$TMP/pick"; }
clear() { :; }
Launch_Scraping() { echo "scrape $onerom" >> "$TMP/calls"; }
Clean_Media() { echo "clean $1" >> "$TMP/calls"; }
Delete_Game_Media() { echo delete_media >> "$TMP/calls"; }
Game_Has_Media() { [ -f "$TMP/has_media" ]; }
Scrape_Favorites() { echo favorites >> "$TMP/calls"; }
Scrape_All_Consoles() { echo all_consoles >> "$TMP/calls"; }
Menu_Config_BackgroundScraping() { echo background >> "$TMP/calls"; Mychoice="Exit"; }
. "$TMP/mm.sh"
CurrentSystem=CPS1 romname=dino.zip romNameNoExtension=dino
touch "$TMP/has_media"
: > "$TMP/calls"; mm_pick ""; Menu_Main
items=$(tr '\n' '|' < "$TMP/items")
check "main: game, its media, console, Configuration, Exit ($items)" test "$items" = "Scrape current rom: dino.zip|Delete cover and media: dino|Scrape all CPS1 roms|Delete media of deleted games: CPS1|Configuration|Exit|"
check "MENU (empty choice) with media: nothing done" test ! -s "$TMP/calls"
: > "$TMP/calls"; mm_pick "Delete cover and media: dino"; Menu_Main
check "Delete cover and media runs" test "$(cat "$TMP/calls")" = delete_media
rm "$TMP/has_media"
: > "$TMP/calls"; mm_pick ""; Menu_Main
items=$(tr '\n' '|' < "$TMP/items")
check "no media: no Delete item ($items)" sh -c 'case "$1" in *"Delete cover"*) exit 1 ;; esac' _ "$items"
check "MENU without media: nothing run" test ! -s "$TMP/calls"
: > "$TMP/calls"; mm_pick "Scrape current rom: dino.zip"; Menu_Main
check "this game still scraped" test "$(cat "$TMP/calls")" = "scrape 1"
: > "$TMP/calls"; mm_pick "Scrape all favorites"; Menu_Main
check "from a console list: no Scrape all favorites" test ! -s "$TMP/calls"
glo_tab=2; : > "$TMP/calls"; mm_pick "Scrape all favorites"; Menu_Main
items=$(tr '\n' '|' < "$TMP/items")
check "from Favorites: Scrape all favorites after the console ($items)" test "$items" = "Scrape current rom: dino.zip|Scrape all CPS1 roms|Scrape all favorites|Delete media of deleted games: CPS1|Configuration|Exit|"
check "from Favorites: it runs" test "$(cat "$TMP/calls")" = favorites
unset glo_tab
check "the game list options pass their tab on" grep -q '^export glo_tab="\$current_tab"' "$ROOT/static/build/.tmp_update/script/game_list_options.sh"
: > "$TMP/calls"; mm_pick ""; Menu_Config
items=$(tr '\n' '|' < "$TMP/items")
check "configuration: the long jobs before Back ($items)" sh -c 'case "$1" in *"|Scrape all favorites|Scrape all consoles|Delete media of deleted games: all consoles|Back|") exit 0 ;; esac; exit 1' _ "$items"
for job in "Scrape all favorites:favorites" "Scrape all consoles:all_consoles" "Delete media of deleted games: all consoles:clean all"; do
    : > "$TMP/calls"; mm_pick "${job%:*}"; Menu_Config
    check "configuration: ${job%:*}" test "$(cat "$TMP/calls")" = "${job##*:}"
done
# A submenu that leaves Mychoice set to "Exit" no longer makes the
# main menu's later tests exit: one case, decided before the call.
: > "$TMP/calls"; mm_pick "Toggle background scraping"; ( Menu_Config; echo returned >> "$TMP/calls" )
check "configuration: a submenu's choice doesn't leak ($(tr '\n' ' ' < "$TMP/calls"))" test "$(tr '\n' ' ' < "$TMP/calls")" = "background returned "
unset -f mm_shellect mm_pick clear Launch_Scraping Delete_Game_Media Game_Has_Media Clean_Media Scrape_Favorites Scrape_All_Consoles Menu_Config_BackgroundScraping Menu_Main Menu_Config
check "shellect: red cursor" grep -q "esc SGR 31 1; printf '>'" "$ROOT/static/build/.tmp_update/script/shellect.sh"
check "shellect: red title" grep -qF "esc SGR 31 1; printf '%s' \"\$msg\"; esc SGR 0 0" "$ROOT/static/build/.tmp_update/script/shellect.sh"
# The keys at the bottom: the key red, its action normal, same text
SHELLECT="$ROOT/static/build/.tmp_update/script/shellect.sh"
esc_c=$(printf '\033')
eval "$(extract_fn "$SHELLECT" esc)"
eval "$(extract_fn "$SHELLECT" keys_print)"
kp_marked() { keys_print "$1" | sed "s/$esc_c\[31;1m/<R>/g; s/$esc_c\[0;0m/<\/R>/g"; }
check "keys: Menu/A" test "$(kp_marked "Menu: Exit      A: Validate")" = "<R>Menu:</R> Exit      <R>A:</R> Validate"
check "keys: three keys, two-word action" test "$(kp_marked "Menu: Close PDF   Start: Exit   A: Open")" = "<R>Menu:</R> Close PDF   <R>Start:</R> Exit   <R>A:</R> Open"
check "keys: no key, as it is" test "$(kp_marked "no keys here")" = "no keys here"
unset -f esc keys_print kp_marked
end

# ---- Scraper: "Delete cover and media" finds all of this game's media ----
# "Delete cover" removed Imgs/<game>.png only: the Game Info media,
# screenshots and manual stayed (zazzagone).

begin scraper_game_media_files
eval "$(sed -n '/^Game_Media_Files ()/,/^}/p' "$SCRAPER_MENU")"
eval "$(sed -n '/^Game_Has_Media ()/,/^}/p' "$SCRAPER_MENU")"
ROMS_DIR=$TMP/Roms CurrentSystem=CPS1
r=$ROMS_DIR/CPS1
mkdir -p "$r/Imgs" "$r/Manuals" "$r/media/box2dfront" "$r/media/screenshot" "$r/media/videos"
romNameNoExtension="Dino [!] (World)"
for f in "Imgs/Dino [!] (World).png" "Manuals/Dino [!] (World).pdf" "media/box2dfront/Dino [!] (World).png" \
    "media/screenshot/Dino [!] (World)-1.png" "media/screenshot/Dino [!] (World)-2.png" "media/videos/Dino [!] (World).mp4" \
    "Imgs/Dino [!] (World) 2.png" "Imgs/Dino x.png" "media/screenshot/Dino [!] (World)-a.png" "Imgs/Dino [!] (World)-.png"; do
    touch "$r/$f"
done
out=$(Game_Media_Files | sed "s|^$r/||" | sort | tr '\n' '|')
check "cover, manual, media, numbered screenshots; not other games ($out)" test "$out" = "Imgs/Dino [!] (World).png|Manuals/Dino [!] (World).pdf|media/box2dfront/Dino [!] (World).png|media/screenshot/Dino [!] (World)-1.png|media/screenshot/Dino [!] (World)-2.png|media/videos/Dino [!] (World).mp4|"
Game_Has_Media; check "the menu sees its media" test $? -eq 0
for only in "Imgs/Solo.png" "Manuals/Solo.pdf" "media/videos/Solo.mp4"; do
    rm -rf "$TMP/solo"; mkdir -p "$TMP/solo/CPS1/$(dirname "$only")"; touch "$TMP/solo/CPS1/$only"
    ( ROMS_DIR=$TMP/solo romNameNoExtension=Solo; Game_Has_Media ); check "the menu sees only $only" test $? -eq 0
done
romNameNoExtension="Nothing"
check "a game without media: nothing" test -z "$(Game_Media_Files)"
Game_Has_Media; check "the menu sees none" test $? -eq 1
romNameNoExtension=
check "no game name: nothing" test -z "$(Game_Media_Files)"
unset -f Game_Media_Files Game_Has_Media; unset ROMS_DIR CurrentSystem romNameNoExtension
check "menu: Delete cover and media runs Delete_Game_Media" grep -qF '"$Option3") Delete_Game_Media ;;' "$SCRAPER_MENU"
end

# ---- Scraper menu: "Scrape all favorites" ----

begin scraper_favorites_list
eval "$(sed -n '/^Favorites_List ()/,/^}/p' "$SCRAPER_MENU")"
cat > "$TMP/fav.json" << 'FAVEOF'
{"label":"Metroid","launch":"/mnt/SDCARD/Emu/GBA/launch.sh","type":5,"rompath":"/mnt/SDCARD/Emu/GBA/../../Roms/GBA/Metroid Fusion (USA).gba"}
{"label":"Kids Mode","launch":"/mnt/SDCARD/App/KidsMode/launch.sh","type":5,"rompath":"/mnt/SDCARD/App/KidsMode/launch.sh"}
{"label":"Sonic","launch":"/mnt/SDCARD/Emu/MD/launch.sh","type":5,"rompath":"/mnt/SDCARD/Roms/MD/Sega/Sonic [!].md"}
{"label":"No path","launch":"/mnt/SDCARD/Emu/GB/launch.sh","type":5}
FAVEOF
tab=$(printf '\t')
check "games only, console and file name" test "$(Favorites_List "$TMP/fav.json")" = "GBA${tab}Metroid Fusion (USA).gba
MD${tab}Sonic [!].md"
check "no file: nothing" test -z "$(Favorites_List "$TMP/none.json")"
check "the account question once (scrap_screenscraper.sh)" grep -q 'SCRAPER_NO_ACCOUNT_PROMPT' "$SCRAPER_SS"
for sc in screenscraper launchbox retroarch; do
    check "progress under the $sc title" grep -q 'echo -e "$SCRAPER_PROGRESS' "$ROOT/static/build/.tmp_update/script/scraper/scrap_$sc.sh"
done
check "configuration menu: shown and handled" sh -c "grep -q 'Option8=\"Scrape all favorites\"' '$SCRAPER_MENU' && grep -q '\"\$Option8\") Scrape_Favorites ;;' '$SCRAPER_MENU'"
end

# ---- Scraper menu: "Scrape all consoles" ----

begin scraper_consoles_list
eval "$(sed -n '/^Consoles_List ()/,/^}/p' "$SCRAPER_MENU")"
R="$TMP/Roms"
mkdir -p "$R/GBA/Imgs" "$R/MD/Sega" "$R/SFC/Imgs" "$R/PS/media/box3d" "$R/MYSTERY" "$R/FC" "$R/PICO"
: > "$R/GBA/Metroid.gba"
: > "$R/MD/Sega/Sonic.md"                 # one folder down
: > "$R/SFC/Imgs/Mario.png"               # covers only
: > "$R/SFC/SFC_cache6.db"                # and the list cache
: > "$R/SFC/miyoogamelist.xml"
: > "$R/PS/media/box3d/Crash.png"         # Game Info media only
: > "$R/MYSTERY/game.bin"                 # not a console the scraper knows
: > "$R/FC/.hidden"
: > "$R/PICO/celeste.p8.png"              # PICO-8 carts are PNG files
check "consoles with games, known to the scraper" test "$(SCRAPER_DIR="$(dirname "$SCRAPER_SS")" Consoles_List "$R" | tr '\n' ' ')" = "GBA MD PICO "
check "no Roms folder: nothing" test -z "$(SCRAPER_DIR="$(dirname "$SCRAPER_SS")" Consoles_List "$TMP/none")"
check "configuration menu: shown and handled" sh -c "grep -q 'Option9=\"Scrape all consoles\"' '$SCRAPER_MENU' && grep -q '\"\$Option9\") Scrape_All_Consoles ;;' '$SCRAPER_MENU'"
check "asks before starting" grep -q 'SCRAPE ALL CONSOLES' "$SCRAPER_MENU"
end

# ---- System log snapshot: network services as Tweaks has them ----

begin snapshot_netserv_status
eval "$(extract_fn "$ROOT/static/build/.tmp_update/script/diagnostics/util_snapshot.sh" get_netserv_status)"
log() { :; }
sysdir=$TMP
mkdir -p "$sysdir/config"
touch "$sysdir/config/.sshState" "$sysdir/config/.ftpState_" "$sysdir/config/.syncthingState"
out=$(get_netserv_status)
check "a service switched on is Enabled" sh -c 'printf "%s\n" "$1" | grep -q "^ssh: *Enabled$"' _ "$out"
check "one switched off is Disabled" sh -c 'printf "%s\n" "$1" | grep -q "^ftp: *Disabled$"' _ "$out"
check "one never switched is Disabled" sh -c 'printf "%s\n" "$1" | grep -q "^hotspot: *Disabled$"' _ "$out"
check "Syncthing is listed" sh -c 'printf "%s\n" "$1" | grep -q "^syncthing: *Enabled$"' _ "$out"
check "each service once" test "$(printf '%s\n' "$out" | cut -d: -f1 | sort | uniq -d)" = ""
unset -f get_netserv_status log
end

# ---- Netplay.sh: MENU during the set-up, and the CPU governor ----

begin netplay_menu_cancel_and_governor
NP="$ROOT/static/build/App/romscripts/Netplay.sh"
np_sh=$(command -v bash || echo sh) # Netplay.sh uses == (ash with bash extensions)
mkdir -p "$TMP/stub" "$TMP/sd/script/netplay" "$TMP/sd/script/network"
sed -e "s#/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor#$TMP/gov#g" \
    -e "s#^sysdir=.*#sysdir=$TMP/sd#" -e "s#LD_PRELOAD=[^ ]* ##" "$NP" > "$TMP/Netplay.sh"
# prompt answers from a file, one line per call
cat > "$TMP/stub/prompt" << 'STUB'
#!/bin/sh
n=$(head -n 1 "$NP_ANSWERS"); sed -i 1d "$NP_ANSWERS"; exit $n
STUB
# A session killed by MENU: hostapd and tcpsvd are left running
printf '#!/bin/sh\ntouch "$NP_RUN/hostapd" "$NP_RUN/tcpsvd"\n' > "$TMP/sd/script/netplay/easy-netplay_server.sh"
printf '#!/bin/sh\necho cleanup >> "$NP_RUN/log"; rm -f "$NP_RUN/hostapd" "$NP_RUN/tcpsvd"\n' > "$TMP/sd/script/network/hotspot_cleanup.sh"
printf '#!/bin/sh\n:\n' > "$TMP/sd/script/network/wifi_power_save.sh"
printf '#!/bin/sh\n[ -e "$NP_RUN/$1" ]\n' > "$TMP/stub/pgrep"
printf '#!/bin/sh\nshift; rm -f "$NP_RUN/$1"\n' > "$TMP/stub/killall"
printf '#!/bin/sh\ntouch "$NP_RUN/network_changed"\n' > "$TMP/stub/touch_nc"
chmod +x "$TMP"/stub/* "$TMP"/sd/script/*/*
sed -i "s#touch /tmp/network_changed#touch \"\$NP_RUN/network_changed\"#g" "$TMP/Netplay.sh"
np_run() { # answers...
    rm -rf "$TMP/run"; mkdir -p "$TMP/run"
    printf '%s\n' "$@" > "$TMP/answers"
    echo performance > "$TMP/gov"
    NP_ANSWERS="$TMP/answers" NP_RUN="$TMP/run" PATH="$TMP/stub:$PATH" \
        $np_sh "$TMP/Netplay.sh" /mnt/SDCARD/Roms/SFC/Mario.sfc "" > /dev/null 2>&1
}
np_run 255
check "B at the first question: ondemand" test "$(cat "$TMP/gov")" = ondemand
np_run 0 255
check "B at the second question: ondemand" test "$(cat "$TMP/gov")" = ondemand
np_run 0 1
check "host killed by MENU: hotspot cleaned up" test "$(cat "$TMP/run/log" 2> /dev/null)" = cleanup
check "no hostapd, no tcpsvd left" test ! -e "$TMP/run/hostapd" -a ! -e "$TMP/run/tcpsvd"
check "a network check is asked for" test -e "$TMP/run/network_changed"
check "after a session: ondemand" test "$(cat "$TMP/gov")" = ondemand
# A user hotspot running while Standard Netplay is used is left alone
printf '#!/bin/sh\ntouch "$NP_RUN/hostapd"\n' > "$TMP/sd/script/netplay/standard-netplay.sh"
chmod +x "$TMP/sd/script/netplay/standard-netplay.sh"
np_run 0 0
check "Standard Netplay: the user's hotspot stays" test -e "$TMP/run/hostapd" -a ! -e "$TMP/run/log"
# A joining Pokemon client killed by MENU leaves its FTP server
printf '#!/bin/sh\ntouch "$NP_RUN/tcpsvd"\n' > "$TMP/sd/script/netplay/easy-netplay_client_pokemon.sh"
chmod +x "$TMP/sd/script/netplay/easy-netplay_client_pokemon.sh"
sed -i "s#\$sysdir/script/netplay/easy-netplay_client_pokemon.sh#$TMP/sd/script/netplay/easy-netplay_client_pokemon.sh#" "$TMP/Netplay.sh"
rm -rf "$TMP/run"; mkdir -p "$TMP/run"; printf '1\n2\n' > "$TMP/answers"
NP_ANSWERS="$TMP/answers" NP_RUN="$TMP/run" PATH="$TMP/stub:$PATH" \
    $np_sh "$TMP/Netplay.sh" /mnt/SDCARD/Roms/GB/Red.gb "" > /dev/null 2>&1
check "client: its FTP server is stopped" test ! -e "$TMP/run/tcpsvd"
unset -f np_run
end

# ---- netplay: the appended configuration is never saved into retroarch.cfg ----

begin netplay_override_not_saved_on_exit
for f in netplay_override.cfg easynetplay_override.cfg; do
    check "$f: config_save_on_exit = false" grep -q '^config_save_on_exit = "false"$' "$ROOT/static/build/RetroArch/.retroarch/$f"
done
end

# ---- Easy Netplay: what a cookie may name, consoles without netplay, CPU speed ----

begin easy_netplay_cookie_and_cores
NPD="$ROOT/static/build/.tmp_update/script/netplay"
for f in easy-netplay_client.sh easy-netplay_client_pokemon.sh; do
    eval "$(extract_fn "$NPD/$f" cookie_paths_ok)"
    cp_ok() { core=$1; rom=$2; cookie_paths_ok; }
    C=/mnt/SDCARD/RetroArch/.retroarch/cores
    check "$f: a core and a game" cp_ok "$C/tgbdual_libretro.so" "/mnt/SDCARD/Roms/GB/Pokemon Red.gb"
    check "$f: a game in a sub-folder" cp_ok "$C/snes9x2005_libretro.so" "/mnt/SDCARD/Roms/SFC/RPG/Chrono... Trigger.sfc"
    ! cp_ok /mnt/SDCARD/.tmp_update/runtime.sh "/mnt/SDCARD/Roms/GB/a.gb"
    check "$f: no core elsewhere" test $? -eq 0
    ! cp_ok "/mnt/SDCARD/.tmp_update/x_libretro.so" "/mnt/SDCARD/Roms/GB/a.gb"
    check "$f: no core outside cores/" test $? -eq 0
    ! cp_ok "$C/../../../.tmp_update/x_libretro.so" "/mnt/SDCARD/Roms/GB/a.gb"
    check "$f: no .. in the core" test $? -eq 0
    ! cp_ok "$C/x_libretro.so" "/mnt/SDCARD/Roms/../.tmp_update/runtime.sh"
    check "$f: no .. in the ROM" test $? -eq 0
    ! cp_ok "$C/x_libretro.so" "/mnt/SDCARD/.tmp_update/runtime.sh"
    check "$f: no ROM outside Roms/" test $? -eq 0
    ! cp_ok "$C/none" "/mnt/SDCARD/Roms/NDS/a.nds"
    check "$f: no core named none" test $? -eq 0
    unset -f cookie_paths_ok cp_ok
done

# Host: a console without netplay stops before anything is set up
eval "$(extract_fn "$NPD/easy-netplay_server.sh" Get_NetplayCore)"
sysdir=$TMP/sd
mkdir -p "$sysdir/config"
cp "$ROOT/static/build/.tmp_update/config/netplay_cores.conf" "$sysdir/config/"
build_infoPanel_and_log() { echo "$1|$2" >> "$TMP/panels"; }
sleep() { :; }
cookie_rom_path="/mnt/SDCARD/Roms/NDS/Mario Kart DS.nds" cookie_core_path=x
netplaycore=
Get_NetplayCore
check "NDS: Get_NetplayCore fails" test $? -eq 1
check "NDS: says not compatible" grep -q "^Netplay impossible|NDS not compatible" "$TMP/panels"
cookie_rom_path="/mnt/SDCARD/Roms/SFC/Mario.sfc" netplaycore=
Get_NetplayCore
check "SFC: the table's core, full path" test "$netplaycore" = /mnt/SDCARD/RetroArch/.retroarch/cores/snes9x2005_libretro.so
check "SFC: 1500 MHz" test "$cpuspeed" = 1500
check "lets_go asks before the hotspot" sh -c 'sed -n "/^lets_go() {/,/^}/p" "$1" | grep -n "Get_NetplayCore\|hotspot_create" | head -n 1 | grep -q "Get_NetplayCore || exit"' _ "$NPD/easy-netplay_server.sh"
unset -f Get_NetplayCore build_infoPanel_and_log sleep

# The speed goes into the cookie, and into the core's folder on the client
eval "$(extract_fn "$NPD/easy-netplay_server.sh" create_cookie_info | sed "s#/mnt/SDCARD/RetroArch/retroarch.cookie#$TMP/cookie#")"
log() { :; }
xcrc() { echo 1234; }
netplaycore=$TMP/core.so cookie_rom_path=$TMP/rom.sfc cpuspeed=1500
: > "$netplaycore"; : > "$cookie_rom_path"
create_cookie_info
check "the cookie has the speed" grep -q '^\[cpuspeed\]: 1500$' "$TMP/cookie"
{ echo "[core]: /mnt/SDCARD/RetroArch/.retroarch/cores/snes9x2005_libretro.so"; echo "[rom]: /mnt/SDCARD/Roms/SFC/Mario.sfc"; echo "[cpuspeed]: 1500"; } > "$TMP/cookie.client"
eval "$(extract_fn "$NPD/easy-netplay_client.sh" read_cookie | sed "s#/mnt/SDCARD/RetroArch/retroarch.cookie.client#$TMP/cookie.client#")"
url_encode() { echo "$*"; }
core= rom= cpuspeed= core_config_folder=
read_cookie
check "client: speed kept" test "$cpuspeed" = 1500
check "client: the core's folder" test "$core_config_folder" = "Snes9x 2005"
sed -i 's#Roms/SFC#Roms/FC#' "$TMP/cookie.client"
cpuspeed= ; read_cookie
check "client: no folder, no speed" test -z "$cpuspeed"
check "host FTP is read-only" sh -c '! grep -n "ftpd -w" "$1"' _ "$NPD/easy-netplay_server.sh"
unset -f create_cookie_info read_cookie log xcrc url_encode
end

# ---- Pokemon trade (GB/GBC): the client's save is never lost, cookies, TGB Dual ----

begin pokemon_trade_saves_and_cookies
NPD="$ROOT/static/build/.tmp_update/script/netplay"
SRV="$NPD/easy-netplay_server_pokemon.sh"
CLI="$NPD/easy-netplay_client_pokemon.sh"
sysdir=$TMP/sd
mkdir -p "$sysdir/script/network" "$TMP/t"
: > "$sysdir/script/network/hotspot_cleanup.sh"
pk_stubs() {
    build_infoPanel_and_log() { echo "$1|$2" >> "$TMP/panels"; }
    log() { :; }
    pkill() { :; }
    killall() { :; }
    restore_ftp() { :; }
    disable_flag() { :; }
    sync() { :; }
    sleep() { :; }
}
# A. host cleanup: a played session whose save did not go back keeps it
eval "$(extract_fn "$SRV" cleanup | sed "s#/tmp/#$TMP/t/#g; s#/mnt/SDCARD/RetroArch/#$TMP/ra/#g")"
host_cleanup() { # session_ran returned
    printf 'client save' > "$TMP/client_client.srm"
    ( pk_stubs; save_new_path=$TMP/client_client.srm CurDate=20261120 session_ran=$1 client_save_returned=$2 cleanup ) > /dev/null 2>&1
}
host_cleanup 1 ""
check "played, not returned: kept with a date" test -f "$TMP/client_client.srm_20261120" -a ! -f "$TMP/client_client.srm"
rm -f "$TMP"/client_client.srm*
host_cleanup 1 1
check "played and returned: removed" test ! -e "$TMP/client_client.srm" -a ! -e "$TMP/client_client.srm_20261120"
host_cleanup "" ""
check "never played: removed" test ! -e "$TMP/client_client.srm" -a ! -e "$TMP/client_client.srm_20261120"
mkdir -p "$TMP/ra"; : > "$TMP/ra/retroarch.cookie"; : > "$TMP/ra/retroarch.cookie.client"; : > "$TMP/ra/retroarch.cookie_old"
host_cleanup "" ""
check "host cleanup removes the cookies" test -z "$(ls "$TMP/ra")"
check "lets_go marks the session after RetroArch" sh -c 'sed -n "/^lets_go() {/,/^}/p" "$1" | grep -A1 "^	start_retroarch$" | grep -q "session_ran=1"' _ "$SRV"
unset -f cleanup host_cleanup
for w in "$SRV:client_wait_for_save_return" "$CLI:wait_for_save_return"; do
    check "${w##*:}: waits 5 minutes" sh -c 'sed -n "/^$2() {/,/^}/p" "$1" | grep -q "counter -ge 300"' _ "${w%%:*}" "${w##*:}"
done

# B. host sync_file: the client's cookie never moves the host's own
if command -v bash > /dev/null; then
    cat > "$TMP/sf.sh" << 'SF'
. "$TMP/fns.sh"
build_infoPanel_and_log() { echo "$1|$2" >> "$TMP/panels"; }
log() { :; }; sleep() { :; }; url_encode() { echo "$*"; }
cleanup() { echo CLEANUP >> "$TMP/panels"; exit 0; }
notify_peer() { :; }
curl() {
    case "$1" in
    -I) [ -n "$REMOTE" ] && printf 'Content-Length: %s\n' "$(printf %s "$REMOTE" | wc -c)" ;;
    -o) [ -n "$REMOTE" ] && printf %s "$REMOTE" > "$2" ;;
    esac
}
client_ip=192.168.100.101
sync_file "Cookie" "$TMP/ra/retroarch.cookie" 0 0 -f -m
SF
    { extract_fn "$SRV" sync_file; extract_fn "$SRV" checksum_func; extract_fn "$SRV" checksize_func; } > "$TMP/fns.sh"
    rm -rf "$TMP/ra" "$TMP/panels"; mkdir -p "$TMP/ra"; printf 'host cookie' > "$TMP/ra/retroarch.cookie"
    TMP=$TMP REMOTE="client cookie" bash "$TMP/sf.sh" > /dev/null 2>&1
    check "the host's cookie stays" test "$(cat "$TMP/ra/retroarch.cookie")" = "host cookie"
    check "the client's is .client" test "$(cat "$TMP/ra/retroarch.cookie.client" 2> /dev/null)" = "client cookie"
    check "no _old" test ! -e "$TMP/ra/retroarch.cookie_old"
    # The client's FTP does not answer; a cookie of an earlier session is there
    rm -f "$TMP/panels"; printf 'old partner' > "$TMP/ra/retroarch.cookie.client"
    TMP=$TMP REMOTE="" bash "$TMP/sf.sh" > /dev/null 2>&1
    check "nothing fetched: no 'synced'" sh -c '! grep -q "Cookie synced" "$1"' _ "$TMP/panels"
    check "nothing fetched: the session stops" grep -q CLEANUP "$TMP/panels"
    check "the old cookie is not used" test ! -e "$TMP/ra/retroarch.cookie.client"
    check "a different ROM of the client is the one cloned" sh -c 'sed -n "/^sync_file() {/,/^}/p" "$1" | grep -A1 "^				rom=\$file_path$" | grep -q "client_rom=\$file_path"' _ "$SRV"
else
    skip "Pokemon host sync_file tests (bash not installed)"
fi

# C. client: the wait starts at 0, whatever hotspot_join.sh left in counter
eval "$(extract_fn "$CLI" wait_for_save_return | sed "s#/tmp/#$TMP/t/#g")"
rm -f "$TMP/panels"
(
    pk_stubs
    notify_peer() { :; }
    cleanup() { echo CLEANUP >> "$TMP/panels"; exit 0; }
    n=0
    sleep() { n=$((n + 1)); [ $n -eq 40 ] && touch "$TMP/t/ready_to_send"; }
    counter=25 save_file_matched=$TMP/s.srm SaveFromGambatte=0
    wait_for_save_return
) > /dev/null 2>&1
check "client: 40 s later the host is still waited for" sh -c '! grep -q "CLEANUP\|didn.t ready up" "$1"' _ "$TMP/panels"
unset -f wait_for_save_return

# D. client: a TGB Dual save replaced by the Gambatte one comes back
S=$TMP/saves
eval "$(extract_fn "$CLI" backup_and_send_save | sed "s#/tmp/#$TMP/t/#g; s#/mnt/SDCARD/Saves/CurrentProfile/saves#$S#g")"
eval "$(extract_fn "$CLI" cleanup | sed "s#/tmp/#$TMP/t/#g; s#/mnt/SDCARD/RetroArch/#$TMP/ra/#g")"
mkdir -p "$S/Gambatte" "$S/TGB Dual"
printf 'gambatte' > "$S/Gambatte/Blue.srm"; printf 'tgb dual 40 hours' > "$S/TGB Dual/Blue.srm"
(
    pk_stubs
    check_stop() { :; }; confirm_join_panel() { :; }; curl() { :; }; url_encode() { echo "$*"; }; is_running() { false; }
    client_rom_filename_NoExt=Blue CurDate=20261120
    backup_and_send_save
    [ "$(cat "$S/TGB Dual/Blue.srm")" = gambatte ] && echo used > "$TMP/during"
    cleanup
) > /dev/null 2>&1
check "client: Gambatte save used for the session" test -f "$TMP/during"
check "client: the TGB Dual save is back" test "$(cat "$S/TGB Dual/Blue.srm")" = "tgb dual 40 hours"
check "client: no dated copy left" test ! -e "$S/TGB Dual/Blue.srm_20261120"
unset -f backup_and_send_save cleanup pk_stubs
end

# ---- Pokemon trade (GBA) host: the auto state is put away only after a session ----

begin pokemon_gba_host_autostate
GBA="$ROOT/static/build/.tmp_update/script/netplay/easy-netplay_server_pokemon_GBA.sh"
ST=$TMP/states/gpSP
mkdir -p "$ST" "$TMP/sd/script/network"
: > "$TMP/sd/script/network/hotspot_create.sh"; : > "$TMP/sd/script/network/hotspot_cleanup.sh"
eval "$(extract_fn "$GBA" cleanup | sed "s#/tmp/#$TMP/#g; s#/mnt/SDCARD/Saves/CurrentProfile/states/gpSP#$ST#g")"
eval "$(extract_fn "$GBA" lets_go | sed "s#/mnt/SDCARD/Saves/CurrentProfile/states/gpSP#$ST#g")"
gba_run() { # retroarch_runs
    : > "$ST/Emerald.state.auto"
    (
        sysdir=$TMP/sd host_rom=/x/Emerald.gba host_rom_filename_NoExt=Emerald CurDate=20261120
        build_infoPanel_and_log() { :; }; log() { :; }; pkill() { :; }; restore_ftp() { :; }; disable_flag() { :; }; sync() { :; }
        pressMenu2Kill() { :; }; start_ftp() { :; }; ready_up() { :; }; stripped_game_names() { :; }
        wait_for_client() { [ "$1" = ok ] || cleanup; }
        confirm_join_panel() { :; }
        start_retroarch() { :; }
        if [ "$1" = 1 ]; then wait_for_client() { :; }; fi
        lets_go
    ) > /dev/null 2>&1
}
gba_run 0
check "nobody joined: the auto state stays" test -f "$ST/Emerald.state.auto"
rm -f "$ST"/*
gba_run 1
check "after a session: put away with a date" test -f "$ST/Emerald.state.auto_20261120" -a ! -f "$ST/Emerald.state.auto"
unset -f cleanup lets_go gba_run
# The launcher keeps gpSP for a game with a gpSP save and no gpSP state
GL="$ROOT/static/packages/Emu/Nintendo - GBA (gpSP)/Emu/GBA/launch.sh"
gl_core() { # files...
    rm -rf "$TMP/g"; mkdir -p "$TMP/g/saves/gpSP" "$TMP/g/states/mGBA" "$TMP/g/states/gpSP" "$TMP/g/cfg"
    for f in "$@"; do : > "$TMP/g/$f"; done
    (
        save_dir=$TMP/g/saves state_dir=$TMP/g/states romname=Emerald auto_core_marker=$TMP/g/cfg/marker
        gpsp_state="$state_dir/gpSP/$romname.state.auto" mgba_state="$state_dir/mGBA/$romname.state.auto"
        eval "$(sed -n '/^default_core=gpsp/,/^fi$/p' "$GL")"
        echo "$default_core"
    )
}
check "mGBA state only: mGBA" test "$(gl_core states/mGBA/Emerald.state.auto)" = mgba
check "mGBA state and a gpSP save: gpSP" test "$(gl_core states/mGBA/Emerald.state.auto saves/gpSP/Emerald.srm)" = gpsp
unset -f gl_core
end

# ---- runtime.sh load_settings: /appconfigs/system.json becomes the link ----

begin load_settings_links_system_json
mkdir -p "$TMP/appconfigs" "$TMP/sd"
echo '{"vol":5}' > "$TMP/sd/system.json"
link_part=$(sed -n '/^load_settings() {/,/^}/p' "$RUNTIME" | sed -n '/# link \/appconfigs\/system.json/,/ln -s/p' |
    sed "s#/appconfigs/#$TMP/appconfigs/#g; s#/mnt/SDCARD/#$TMP/sd/#g")
echo '{"vol":20}' > "$TMP/appconfigs/system.json"   # a regular file
eval "$link_part"
check "a regular file is replaced by the link" test -L "$TMP/appconfigs/system.json"
check "the link points to the card" test "$(readlink "$TMP/appconfigs/system.json")" = "$TMP/sd/system.json"
eval "$link_part"
check "a link already there stays a link" test "$(readlink "$TMP/appconfigs/system.json")" = "$TMP/sd/system.json"
end

# ---- installer: only the RetroArch configuration moved by this run is put back ----

begin installer_restores_only_its_own_ra_cfg
IN="$ROOT/static/dist/miyoo/app/.tmp_update/install.sh"
eval "$(extract_fn "$IN" install_retroarch | sed "s#/mnt/SDCARD/#$TMP/sd/#g")"
eval "$(extract_fn "$IN" restore_ra_config | sed "s#/mnt/SDCARD/#$TMP/sd/#g")"
mkdir -p "$TMP/sd/RetroArch/.retroarch" "$TMP/sd/Backup"
echo live > "$TMP/sd/RetroArch/.retroarch/retroarch.cfg"
echo "user's old copy" > "$TMP/sd/Backup/retroarch.cfg"
# An update that does not install RetroArch
( ra_cfg_saved=; restore_ra_config > /dev/null )
check "no RetroArch this time: the live cfg stays" test "$(cat "$TMP/sd/RetroArch/.retroarch/retroarch.cfg")" = live
check "and the user's copy stays" test -f "$TMP/sd/Backup/retroarch.cfg"
# An update that installs RetroArch
(
    RA_PACKAGE_FILE=$TMP/ra.zip; : > "$RA_PACKAGE_FILE"
    maybe_remove_retroarch() { :; }; unzip_progress() { :; }
    install_retroarch "x" > /dev/null
    restore_ra_config > /dev/null
)
check "with RetroArch: the live cfg comes back" test "$(cat "$TMP/sd/RetroArch/.retroarch/retroarch.cfg")" = live
unset -f install_retroarch restore_ra_config
end

# ---- clean_media.sh: games hidden with GLO keep their media ----

begin clean_media_keeps_media_of_hidden_games
cm_sd
mkdir -p "$R/GB/.hidden/Pokemon Red (USA)" "$R/GB/RPG/.hidden/Final Quest Vol. 1"
touch "$R/GB/.hidden/Pokemon Red (USA)/Pokemon Red (USA).gb" "$R/GB/RPG/.hidden/Final Quest Vol. 1/disc.cue"
touch "$R/GB/Imgs/Pokemon Red (USA).png" "$R/GB/Manuals/Pokemon Red (USA).pdf" \
    "$R/GB/media/box2dfront/Pokemon Red (USA).png" "$R/GB/Imgs/Final Quest Vol. 1.png"
sh "$CLEAN_MEDIA" --delete "$R/GB" > /dev/null
check "a hidden game's cover, manual and media stay" \
    test -f "$R/GB/Imgs/Pokemon Red (USA).png" -a -f "$R/GB/Manuals/Pokemon Red (USA).pdf" -a -f "$R/GB/media/box2dfront/Pokemon Red (USA).png"
check "one hidden in a sub-folder, a name with a dot" test -f "$R/GB/Imgs/Final Quest Vol. 1.png"
check "a deleted game's media still go" test ! -e "$R/GB/Imgs/Mario.png"
end

# ---- Random Game: starts the game it shows when there is no recent list yet ----

begin random_game_without_recent_list
RG="$ROOT/static/packages/App/Random Game/App/RandomGamePicker/random.sh"
mkdir -p "$TMP/stub" "$TMP/Roms" "$TMP/sys/config"
sed -e "s#/mnt/SDCARD/Roms/#$TMP/Roms/#g; s#/tmp/recentlist-temp.json#$TMP/rl.tmp#; s#^sysdir=.*#sysdir=$TMP/sys#" "$RG" > "$TMP/random.sh"
printf '#!/bin/sh\nprintf "emu=Game Boy\\nlabel=Tetris\\npath=/mnt/SDCARD/Roms/GB/Tetris.gb\\nimg=\\nlaunch=/mnt/SDCARD/Emu/GB/launch.sh\\n"\n' > "$TMP/stub/randomGamePicker"
printf '#!/bin/sh\nexit 0\n' > "$TMP/stub/infoPanel"
chmod +x "$TMP"/stub/*
PATH="$TMP/stub:$PATH" sh "$TMP/random.sh" > /dev/null 2>&1
check "no recent list: random.sh ends with 0 (the game starts)" test $? -eq 0
check "the hidden recent list now has the game" grep -q '"rompath":"/mnt/SDCARD/Roms/GB/Tetris.gb"' "$TMP/Roms/recentlist-hidden.json"
PATH="$TMP/stub:$PATH" sh "$TMP/random.sh" > /dev/null 2>&1
check "the next one is added on top" test "$(wc -l < "$TMP/Roms/recentlist-hidden.json")" -eq 2
end

# ---- runtime.sh update_time: sessions cut by a power loss are dropped first ----

begin update_time_drops_open_sessions_first
eval "$(extract_fn "$RUNTIME" update_time | sed "s#/mnt/SDCARD/#$TMP/sd/#g")"
for rtc in 1700000000 3; do
    (
        log() { :; }; touch() { :; }
        playActivity() { echo "playActivity $*" >> "$TMP/calls"; }
        date() { case "$1" in +%s) [ $# -eq 1 ] && echo "$rtc" ;; esac; [ "$2" = -s ] && echo "date -s" >> "$TMP/calls"; return 0; }
        sysdir=$TMP/sys
        update_time
    ) > /dev/null 2>&1
    check "clock $rtc: drop_open is the first call" test "$(head -n 1 "$TMP/calls")" = "playActivity drop_open"
    if [ $rtc -gt 15 ]; then
        check "RTC: the clock is left alone" sh -c '! grep -q "date -s" "$1"' _ "$TMP/calls"
    else
        check "no RTC: the clock is set back" grep -q "date -s" "$TMP/calls"
    fi
    rm -f "$TMP/calls"
done
unset -f update_time
end

# ---- Fable's review of the new UI (wip/new-ui) ----

begin scraper_native_b_stops_the_scraper_at_once
# B on the progress screen: the scraper gets TERM (its trap writes the Game
# Info descriptions waiting) and the menu goes on, even while the scraper
# writes nothing (a download, a quota wait): in a pipe it went on until
# its next line and died of SIGPIPE, skipping the trap.
SCRAPER_MENU="$ROOT/static/build/.tmp_update/script/scraper/menu.sh"
mkdir -p "$TMP/bin"
# the progress screen: reads the tip and the game, then B
printf '#!/bin/sh\nread -r _l; read -r _l; exit 2\n' > "$TMP/bin/progressUI"
{
    echo '#!/bin/sh'
    echo "trap 'echo flushed > \"$TMP/flushed\"; exit 1' TERM"
    echo 'echo "@@item 1 Game"'
    echo 'while :; do sleep 1; done'
} > "$TMP/scraper"
chmod +x "$TMP/bin/progressUI" "$TMP/scraper"
start=$(date +%s)
(
    eval "$(extract_fn "$SCRAPER_MENU" run_scraper)"
    scraper_tips() { echo "@@tip x"; }
    ui_scraper_label() { echo Test; }
    close_terminal() { echo closed > "$TMP/closed"; }
    SCRAPER_NATIVE=1 PATH="$TMP/bin:$PATH"
    run_scraper "$TMP/scraper" GB
) > /dev/null 2>&1 &
rs_pid=$!
n=0
while kill -0 $rs_pid 2> /dev/null && [ $n -lt 40 ]; do sleep 0.5; n=$((n + 1)); done
kill -0 $rs_pid 2> /dev/null && kill -9 $rs_pid 2> /dev/null
took=$(($(date +%s) - start))
check "back from the scraper in seconds ($took s)" test $took -le 10
check "the scraper's TERM trap ran (pending descriptions written)" test -f "$TMP/flushed"
check "stopped with B: no next source" test -f "$TMP/closed"
check "no fifo left" sh -c '! ls /tmp/scraper_progress.* > /dev/null 2>&1'
check "and when the progress screen goes away: the PIPE trap too" grep -q "HUP INT TERM PIPE" "$ROOT/static/build/.tmp_update/script/scraper/scrap_screenscraper.sh"
end

begin scraper_native_menu_on_the_account_question
# MENU there killed the main shell while the question ran in a subshell
# ("(onerom=0; Launch_Scraping;)"), which went on to scrape over MainUI.
SCRAPER_MENU="$ROOT/static/build/.tmp_update/script/scraper/menu.sh"
q=$(sed -n '/Native_Account_Question() {/,/^    }/p' "$SCRAPER_MENU")
check "asked with -m (MENU comes back as exit 2)" sh -c 'printf "%s" "$1" | grep -q "shellect.sh -m -t \"--== ADD A SCREENSCRAPER ACCOUNT? ==--\""' _ "$q"
check "exit 2: the scraper leaves" sh -c 'printf "%s" "$1" | grep -qF "[ \$? -eq 2 ] && exit 0"' _ "$q"
end

begin menu_leaves_only_after_its_press
# keystate[MENU] == RELEASED is also the state of every idle key: as an exit
# test it closed Activity Tracker and Battery Monitor on any button.
for f in "$ROOT/src/batteryMonitorUI/batteryMonitorUI.c" "$ROOT/src/playActivityUI/playActivityUI.c"; do
    check "${f##*/}: no exit on a bare MENU == RELEASED" sh -c '! grep -qF "|| keystate[SW_BTN_MENU] == RELEASED" "$1"' _ "$f"
    check "${f##*/}: MENU's press is seen first" grep -q "menu_armed = true" "$f"
done
check "prompt: MENU's release only after its press (menu_seen)" grep -q "!menu_combo && menu_seen" "$ROOT/src/prompt/prompt.c"
end

begin prompt_long_messages_and_long_lists
P="$ROOT/src/prompt/prompt.c"
# -m was cut at 255 bytes; the scraper's warnings run to 300+
check "the message holds 2 KB" grep -qF "char message_str[2048]" "$P"
check "and is copied to its own size" grep -qF "strncpy(message_str, argv[++i], sizeof(message_str) - 1);" "$P"
check "--index-file: the choice past 254 items" grep -qF '"--index-file"' "$P"
check "MENU written as menu (254 can be an item)" grep -qF 'fprintf(fp, "menu' "$P"
end

begin progressui_task_ends_early_and_nostop
PU="$ROOT/src/progressUI/progressUI.c"
check "an early end shows where it stopped" grep -qF '"Ended at %d of %d"' "$PU"
check "100% only when every item was reached" grep -qF "bool complete = !finished || total <= 0 || item >= total;" "$PU"
check "SIGTERM (SDL_QUIT) does not close a nostop task" grep -qF "event.type == SDL_QUIT && (!nostop || finished)" "$PU"
end

begin ota_extraction_does_not_depend_on_the_screen
OTA="$ROOT/static/build/.tmp_update/script/ota_update.sh"
ax=$(sed -n '/apply_extract() { # zip/,/^	}/p' "$OTA")
check "7z writes to a log, not into progressUI" sh -c 'printf "%s" "$1" | grep -qF "runtime.sh > /tmp/ota_7z.log 2>&1"' _ "$ax"
check "the old result removed first" sh -c 'printf "%s\n" "$1" | grep -q "rm -f /tmp/ota_7z_rc$"' _ "$ax"
check "the screen follows the log" sh -c 'printf "%s" "$1" | grep -qF "tail -c +\$((_shown + 1)) /tmp/ota_7z.log"' _ "$ax"
# Run: 7z's percentage as the bar, the file under it, not the scraper's
# FOUND / NOT FOUND / ALREADY THERE (all 0 while it worked, beta 6)
mkdir -p "$TMP/otax/bin" "$TMP/otax/sys/bin"
cat > "$TMP/otax/bin/7z" << 'EOF'
#!/bin/sh
echo "$*" > "$OTAX/args"
case "$*" in *-bsp1*)
    printf '  0%%\010\010\010\010    \010\010\010\010 35%% 120 - miyoo/app/skin/a.png'
    sleep 1
    printf '\010\010\010\010 80%% 900 - .tmp_update/bin/keymon\n'
    [ -n "$FAKE7Z_RC" ] && echo "ERROR: Disk full : miyoo/app/MainUI" ;;
esac
exit "${FAKE7Z_RC:-0}"
EOF
printf '#!/bin/sh\ncat > "%s/otax/screen"\n' "$TMP" > "$TMP/otax/sys/bin/progressUI"
chmod +x "$TMP/otax/bin/7z" "$TMP/otax/sys/bin/progressUI"
(
    OTAX="$TMP/otax" PATH="$TMP/otax/bin:$PATH" sysdir="$TMP/otax/sys" Release_Version=4.5.1-b7
    export OTAX
    eval "$ax"
    apply_extract "$TMP/otax/RedOnion.zip"
    echo "rc=$?" > "$TMP/otax/result"
) > /dev/null 2>&1
check "the extraction ends well" grep -q "^rc=0$" "$TMP/otax/result"
check "7z asked for its percentage and the files" grep -q -- "-bsp1 -bb1" "$TMP/otax/args"
check "a bar of 100 steps" grep -q "^@@total 100$" "$TMP/otax/screen"
check "35%: the file under the bar" grep -q "^@@item 36 miyoo/app/skin/a.png$" "$TMP/otax/screen"
check "80%: the next one" grep -q "^@@item 81 .tmp_update/bin/keymon$" "$TMP/otax/screen"
check "done: the bar full" grep -q "^@@item 101 Done$" "$TMP/otax/screen"
(
    OTAX="$TMP/otax" PATH="$TMP/otax/bin:$PATH" sysdir="$TMP/otax/sys" Release_Version=4.5.1-b7 FAKE7Z_RC=2
    export OTAX FAKE7Z_RC
    eval "$ax"
    apply_extract "$TMP/otax/RedOnion.zip"
    echo "rc=$?" > "$TMP/otax/result"
) > /dev/null 2>&1
check "7z's error shown in the log box" grep -q "^ERROR: Disk full : miyoo/app/MainUI$" "$TMP/otax/screen"
check "a failed extraction: 7z's result, the bar not filled" sh -c 'grep -q "^rc=2$" "$1/result" && ! grep -q "^@@item 101" "$1/screen"' _ "$TMP/otax"
check "no temporary files left" sh -c '! ls /tmp/ota_7z.log /tmp/ota_7z_rc > /dev/null 2>&1'
PU="$ROOT/src/progressUI/progressUI.c"
check "progressUI: the counts only for the scraper (-c) or once one came" grep -qF "static bool counts = false;" "$PU"
check "without counts: DONE, ELAPSED and TIME LEFT" sh -c 'grep -qF "panel(c, 20, 192, \"DONE\"" "$1" && grep -qF "\"ELAPSED\"" "$1"' _ "$PU"
check "the scraper's three screens ask for the counts" test "$(grep -c 'progressUI -t Scraper -i image -c ' "$ROOT/static/build/.tmp_update/script/scraper/menu.sh")" -eq 3
check "no terminal lines in the native download" grep -qF 'if [ -z "$OTA_NATIVE" ]; then' "$OTA"
end

begin install_tips_in_a_random_order
# The tips come in a new random order at each installation (the logo and
# the welcome first); the installer's last screen goes on in the same order.
IUI="$ROOT/src/installUI/installUI.c"
INS="$ROOT/static/dist/miyoo/app/.tmp_update/install.sh"
check "the order is shuffled from a seed" grep -q "static void buildSlideOrder(int num_slides, unsigned int seed)" "$IUI"
check "the seed is kept with the position" grep -qF 'config_setNumber("slideSeed", seed);' "$IUI"
check "each installation starts a new order (both places)" test "$(grep -c 'rm -f $sysdir/config/currentSlide $sysdir/config/slideSeed' "$INS")" -eq 2
check "the dots are drawn by installUI, not in the pictures" grep -q "draw_dots(&c, current_slide);" "$IUI"
end

begin themes_previews_any_png_and_bicubic
# Paletted previews (PAC-MIYOO, Super Onion Entertainment System) were black:
# SoftStretch needs one format on both sides. Now any PNG is blitted to
# 32-bit first, and scaled with a bicubic filter (robcodedev).
TS="$ROOT/src/themeSwitcher/themeSwitcher.c"
check "no SoftStretch on the preview any more" sh -c '! grep -q "SDL_SoftStretch(preview" "$1"' _ "$TS"
check "the preview is blitted to 32-bit first" grep -qF "SDL_BlitSurface(preview, NULL, rgb, NULL);" "$TS"
check "scaled with Catmull-Rom (a = -0.5)" grep -qF "const float a = -0.5f;" "$TS"
end

begin backup_on_the_progress_screen
# robcodedev: a progress meter in Backup. With BACKUP_PROGRESS (the app's
# screens) 7z runs on progressUI: its percentage as the bar, the file under
# it; the result and the log are as before, and without it nothing changes.
mkdir -p "$TMP/bin" "$TMP/sd/Saves/CurrentProfile/saves"
echo data > "$TMP/sd/Saves/CurrentProfile/saves/a.srm"
# 7z as p7zip draws its progress: the line redrawn with backspaces
cat > "$TMP/bin/fake7z" << 'EOF'
#!/bin/sh
echo "$*" >> "$FAKE_ARGS"
case "$*" in *-bsp1*)
    printf '  0%%\010\010\010\010    \010\010\010\010 40%% 1 + Saves/CurrentProfile/saves/a.srm'
    sleep 1
    printf '\010\010\010\010 90%% 2 + system.json\n' ;;
esac
for a; do case $a in *.part) echo zip > "$a" ;; esac; done
exit "${FAKE7Z_RC:-0}"
EOF
printf '#!/bin/sh\ncat > "%s/screen"\n' "$TMP" > "$TMP/bin/progressUI"
chmod +x "$TMP/bin/fake7z" "$TMP/bin/progressUI"
(
    BACKUP_SD="$TMP/sd" BACKUP_NO_MAIN=1 BACKUP_7Z="$TMP/bin/fake7z"
    export BACKUP_SD BACKUP_NO_MAIN BACKUP_7Z FAKE_ARGS="$TMP/args" PATH="$TMP/bin:$PATH"
    . "$ROOT/static/packages/App/Backup (Saves and settings)/App/Backup/launch.sh"
    LOG=$TMP/log
    free_kb() { echo 999999; }
    BACKUP_PROGRESS=1 BACKUP_TITLE="Backing up your saves and settings"
    make_backup ""
    echo "rc=$? file=${BACKUP_FILE##*/}" > "$TMP/result"
    BACKUP_PROGRESS=
    make_backup _plain
    echo "rc=$?" >> "$TMP/result"
) > /dev/null 2>&1
check "the backup is made ($(head -n 1 "$TMP/result"))" grep -q "^rc=0 file=RedOnion_backup_" "$TMP/result"
check "the title on the progress screen" grep -q "^@@title Backing up your saves and settings$" "$TMP/screen"
check "percentages as the bar, the file under it" grep -q "^@@item 41 Saves/CurrentProfile/saves/a.srm$" "$TMP/screen"
check "the next one too" grep -q "^@@item 91 system.json$" "$TMP/screen"
check "done: the bar full" grep -q "^@@item 101 Done$" "$TMP/screen"
check "cannot be cut, closes by itself" sh -c 'grep -q "^@@nostop$" "$1" && grep -q "^@@autoclose$" "$1"' _ "$TMP/screen"
check "7z's output still in the log" grep -q "system.json" "$TMP/log"
check "without the screen: as before (-bd, no progress)" sh -c 'sed -n 2p "$1" | grep -q -- " -bd$"' _ "$TMP/args"
check "and the second backup made too" grep -q "^rc=0$" "$TMP/result"
check "no temporary files left" sh -c '! ls /tmp/backup_7z.log /tmp/backup_7z_rc > /dev/null 2>&1'
end

begin clock_unhandled_key_keeps_the_redraw
check "the default case does not clear dirty" sh -c '! sed -n "/default:/,/break;/p" "$1" | grep -q "dirty = false"' _ "$ROOT/src/clock/main.c"
check "dirty is cleared only after drawing" test "$(grep -c "dirty = false" "$ROOT/src/clock/main.c")" -eq 1
end

echo ""
echo "========================================"
echo "  Tests: $tests | Assertions: $asserts | Failures: $fails | Skipped blocks: $skipped"
# A skipped block is not a pass: with TESTS_NO_SKIP=1 (CI, where every tool
# is installed) it fails the run; elsewhere it is shown here.
if [ $fails -eq 0 ] && { [ $skipped -eq 0 ] || [ -z "$TESTS_NO_SKIP" ]; }; then
    if [ $skipped -eq 0 ]; then
        echo "  Result: PASSED"
    else
        echo "  Result: PASSED, $skipped blocks SKIPPED (see [SKIP] above)"
    fi
else
    echo "  Result: FAILED"
fi
echo "========================================"
[ $fails -eq 0 ] && { [ $skipped -eq 0 ] || [ -z "$TESTS_NO_SKIP" ]; }
