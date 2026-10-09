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
}

end() {
    rm -rf "$TMP"
    if [ $current_failed -eq 0 ]; then
        echo "  [ OK ] $current"
    else
        echo "  [FAIL] $current"
    fi
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
}

begin boot_services_start_without_game
stub_services
services_paused_flag="$TMP/paused"
start_services_outside_game
check "all five checkers ran" test "$(wc -l < "$TMP/started")" -eq 5
check "nothing killed" test ! -e "$TMP/killed"
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
for _sl in /bin/sleep /usr/bin/sleep; do [ -x "$_sl" ] && break; done
cp "$_sl" "$TMP/audioserver"
"$TMP/audioserver" 30 &
as_pid=$!
started=0
runifnecessary() { started=$((started + 1)); }
audioserver_pid=""
real_pgrep=$(command -v pgrep)
# runs in a $(...) subshell: count in a file
pgrep() { echo >> "$TMP/pgrep_calls"; "$real_pgrep" -x audioserver; }
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

    ota_asset() { # tag, prerelease (true/false), published_at (default: from the tag date)
        _pub=${3:-$(echo "$1" | sed 's/^.*-\([0-9]\{4\}\)\([0-9]\{2\}\)\([0-9]\{2\}\)-.*$/\1-\2-\3T12:00:00Z/')}
        printf '{"tag_name":"%s","prerelease":%s,"draft":false,"published_at":"%s","body":"","assets":[{"name":"OnionPlus-v%s.zip","browser_download_url":"https://x/%s.zip","size":1048576,"created_at":"2026-09-27T00:00:00Z"}]}' "$1" "$2" "$_pub" "$1" "$1"
    }
    curl() { # the last argument is the URL
        for _u; do :; done
        case "$_u" in
            */releases/latest) cat "$TMP/latest.json" ;;
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
    unset -f curl installUI ota_asset
else
    echo "  [SKIP] ota_update.sh tests (jq not installed)"
fi

# ---- ota_update.sh: Wi-Fi off must not leave the updater hanging ----

eval "$(extract_fn "$OTA" enable_wifi)"

begin ota_wifi_off_does_not_hang
# No network ever comes up: udhcpc keeps retrying, as the real one does.
ip() { :; }
sleep() { :; }
insmod() { :; }
ifconfig() { :; }
pkill() { :; }
clear() { :; }
udhcpc() {
    echo "$*" > "$TMP/udhcpc_args"
    while [ ! -f "$TMP/stop" ]; do command sleep 0.1; done
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
check "connection check has a timeout" grep -q 'wget -q -T [0-9]' "$OTA"
wait
unset -f ip sleep insmod ifconfig pkill clear udhcpc
end

# ---- runtime.sh: network check skipped while update_networking.sh runs ----

eval "$(extract_fn "$RUNTIME" check_networking)"
eval "$(extract_fn "$RUNTIME" run_network_check)"
eval "$(extract_fn "$RUNTIME" queue_network_check)"

net_setup() {
    sysdir=$TMP
    mkdir -p "$sysdir/script/network"
    printf '#!/bin/sh\necho "$1" >> "%s/net_runs"\n' "$TMP" > "$sysdir/script/network/update_networking.sh"
    chmod +x "$sysdir/script/network/update_networking.sh"
    rm -rf /tmp/network_changed /tmp/network_changed.taken /tmp/network_check_queued
}
unset -f sleep # the waiter really waits (an earlier section stubs it)
pgrep() { [ -f "$TMP/running" ]; } # pgrep -f update_networking.sh
log() { :; }
perf_begin() { :; }
perf_end() { :; }
net_runs() { [ -f "$TMP/net_runs" ] && wc -l < "$TMP/net_runs" | tr -d ' ' || echo 0; }

begin network_check_runs_when_idle
net_setup
: > /tmp/network_changed
check_networking > /dev/null
check "check ran once" test "$(net_runs)" = 1
check "change consumed" test ! -e /tmp/network_changed
end

begin network_check_queued_while_running
net_setup
: > /tmp/network_changed
: > "$TMP/running"
check_networking > /dev/null
check_networking > /dev/null # a second state change: still one waiter
check "not run while the other check runs" test "$(net_runs)" = 0
check "one waiter queued" test -d /tmp/network_check_queued
rm -f "$TMP/running" # the running check ends
i=0
while [ -d /tmp/network_check_queued ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
sleep 0.3
check "run once after it ended" test "$(net_runs)" = 1
check "change consumed" test ! -e /tmp/network_changed
end

begin network_check_taken_once
net_setup
run_network_check
check "nothing pending: no run" test "$(net_runs)" = 0
: > /tmp/network_changed
run_network_check
run_network_check
check "a pending change runs once" test "$(net_runs)" = 1
end
unset -f pgrep log perf_begin perf_end net_runs net_setup
rm -rf /tmp/network_changed /tmp/network_changed.taken /tmp/network_check_queued

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
check "no pgrep -x under static/" test -z "$(command grep -rlE '^[^#]*pgrep +-[a-z]*x' "$ROOT/static")"
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
    PATH="$TMP/bin:$PATH" detect_device_model
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
eval "$(extract_fn "$GLO" refresh_roms)"

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
glo_filter_env 1
log() { :; }
filter() { echo "$*" > "$TMP/filter_args"; }
mkdir -p "$TMP/script"
printf '#!/bin/sh\n' > "$TMP/script/reset_list.sh"
chmod +x "$TMP/script/reset_list.sh"
(cd "$TMP" && refresh_roms)
check "Refresh list runs the filter refresh" test "$(cat "$TMP/filter_args")" = "refresh $emupath"
check "Refresh list drops active_filter" test ! -e "$emupath/active_filter"
unset -f sqlite3 filter log
end

# ---- cue_gen.sh: one CUE per game, next to its files (#264) ----

CUEGEN="$ROOT/static/build/.tmp_update/script/cue_gen.sh"

begin cue_gen_writes_valid_cues
mkdir -p "$TMP/PS/Alpha Game" "$TMP/PS/Beta Game" "$TMP/SEGACD"
for f in "PS/Flat Game (Track 1).bin" "PS/Flat Game (Track 2).bin" "PS/Flat Game (Track 10).bin" \
    "PS/Alpha Game/Alpha Game.bin" "PS/Beta Game/Beta Game (Track 01).bin" \
    "PS/Beta Game/Beta Game (Track 02).bin" "PS/Hack [T-En] (Track 1).bin" \
    "PS/Hack [T-En] (Track 2).bin" "PS/Novastorm (USA) (Disc 1).bin" \
    "PS/Novastorm (USA) (Disc 2).bin" "SEGACD/Kept (USA) (Track 1).bin"; do
    : > "$TMP/$f"
done
echo ORIGINAL > "$TMP/SEGACD/Kept (USA).cue"
rootdir=$TMP sh "$CUEGEN" "PS SEGACD" > "$TMP/cue_gen.log" 2>&1
cues=$(cd "$TMP" && find PS SEGACD -name '*.cue' | sort | tr '\n' '|')
check "one CUE per game, in the game's folder" test "$cues" = \
    "PS/Alpha Game/Alpha Game.cue|PS/Beta Game/Beta Game.cue|PS/Flat Game.cue|PS/Hack [T-En].cue|PS/Novastorm (USA) (Disc 1).cue|PS/Novastorm (USA) (Disc 2).cue|SEGACD/Kept (USA).cue|"
check "FILE is the bare file name" grep -q '^FILE "Beta Game (Track 02).bin" BINARY$' "$TMP/PS/Beta Game/Beta Game.cue"
check "no folder in any FILE" test -z "$(grep -h '^FILE "[^"]*/' "$TMP"/PS/*.cue "$TMP"/PS/*/*.cue)"
check "tracks in numeric order (10 after 2)" test "$(grep -o 'TRACK [0-9]*' "$TMP/PS/Flat Game.cue" | tr '\n' ' ')" = "TRACK 01 TRACK 02 TRACK 10 "
check "square brackets keep track 1" grep -q '^FILE "Hack \[T-En\] (Track 1).bin" BINARY$' "$TMP/PS/Hack [T-En].cue"
check "each disc gets its own CUE" test "$(grep -c '^FILE' "$TMP/PS/Novastorm (USA) (Disc 2).cue")" -eq 1
check "existing CUE left untouched" test "$(cat "$TMP/SEGACD/Kept (USA).cue")" = ORIGINAL
check "no CUE without a name" test -z "$(find "$TMP" -name '.cue')"
check "totals reported" grep -q '^6 cue files created$' "$TMP/cue_gen.log"
end

echo ""
echo "========================================"
echo "  Tests: $tests | Assertions: $asserts | Failures: $fails"
if [ $fails -eq 0 ]; then
    echo "  Result: PASSED"
else
    echo "  Result: FAILED"
fi
echo "========================================"
[ $fails -eq 0 ]
