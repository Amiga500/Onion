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
    eval "$(grep '^build_date()' "$OTA")"
    eval "$(grep '^PACKAGE_PREFIXES=' "$OTA")"

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
    echo "  [SKIP] ota_update.sh tests (jq not installed)"
fi

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
check "release lists read with -L" test "$(grep -c 'curl -k -s -L https://api.github.com' "$OTA")" -eq 2
unset -f curl run_bootstrap
unset BOOTSTRAP_BODY BOOTSTRAP_MARK
end

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
check "connection check has a timeout" grep -q 'wget -q -T [0-9]' "$OTA"
wait
unset -f ip sleep insmod ifconfig pkill clear udhcpc
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
check "restored on every exit" grep -q '^	trap restore_wifi EXIT' "$OTA"
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
(enable_wifi > /dev/null 2>&1; echo "$wifi_started_here" > "$TMP/started")
check "off but on in system.json: turned on" test -e "$TMP/udhcpc"
check "off but on in system.json: marked as turned on here" test "$(cat "$TMP/started")" = 1
check "decided by the process, not the setting" sh -c "sed -n '/^enable_wifi() {/,/^}/p' \"$OTA\" | grep -q 'wifi_radio_running' && ! sed -n '/^enable_wifi() {/,/^}/p' \"$OTA\" | grep -q 'wifi_setting_on'"
unset -f ip sleep clear pkill udhcpc wifi_setting_on wifi_radio_running
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
check "udhcpc started after the link is up" sh -c "sed -n '/^enable_wifi() {/,/^}/p' \"$OTA\" | grep -A4 'start_wpa_supplicant' | grep -q 'wait_for_link'"
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
while ! grep -qx udhcpc "$TMP/steps" && [ $n -lt 50 ]; do
    command sleep 0.1
    n=$((n + 1))
done
check "wpa_supplicant, up to 50 s for the link, then udhcpc" test "$(grep -v '^udhcpc$' "$TMP/steps" | tr '\n' ' ')" = "pkill -9 wpa_supplicant wpa link 50 pkill -9 udhcpc ip 20 "
check "udhcpc started" grep -qx udhcpc "$TMP/steps"
unset -f ip sleep clear insmod ifconfig iw pkill udhcpc wifi_radio_running wait_for_wlan0 start_wpa_supplicant wait_for_link wait_for_ip
end

begin ota_channel_cancel_keeps_channel
mkdir -p "$TMP/script" "$TMP/config"
sysdir=$TMP
echo beta > "$TMP/config/ota_channel"
clear() { :; }
eval "$(extract_fn "$OTA" channel_choice)"
printf '#!/bin/sh\ncat > /dev/null\nexit 1\n' > "$TMP/script/shellect.sh"
chmod +x "$TMP/script/shellect.sh"
(channel_choice; touch "$TMP/went_on") > /dev/null 2>&1
check "B leaves the updater" test ! -e "$TMP/went_on"
check "saved channel kept" test "$(cat "$TMP/config/ota_channel")" = beta
printf '#!/bin/sh\ncat > /dev/null\necho stable\n' > "$TMP/script/shellect.sh"
(channel_choice; touch "$TMP/went_on") > /dev/null 2>&1
check "choice saved" test "$(cat "$TMP/config/ota_channel")" = stable
check "choice goes on" test -e "$TMP/went_on"
check "Mini without Wi-Fi is told so" grep -q 'This device has no Wi-Fi' "$OTA"
unset -f clear channel_choice
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
fi

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

# ---- runtime.sh: temporaries of interrupted saves removed at boot ----

begin boot_removes_interrupted_save_temporaries
check "load_settings removes system.json and config temporaries" sh -c "sed -n '/^load_settings() {/,/^}/p' \"$RUNTIME\" | grep -q 'rm -f /mnt/SDCARD/system.json.tmp.\* /mnt/SDCARD/.tmp_update/config/\*.tmp.\*'"
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
check "no Refresh list entry: the Select menu has Refresh roms" \
    test -z "$(grep -n 'add_menu_option refresh_roms\|^refresh_roms()' "$GLO")"
unset -f sqlite3
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

begin update_flag_cleared_after_update
check "installer removes .updateAvailable" \
    sh -c "sed -n '/^run_installation() {/,/^}/p' \"$INSTALLER\" | grep -q 'rm -f /mnt/SDCARD/.tmp_update/.updateAvailable'"
check "OTA check removes it when up to date" \
    sh -c "sed -n '/^main() {/,/^}/p' \"$OTA\" | grep -c 'rm -f \"\$sysdir/.updateAvailable\"' | grep -qx 2"
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
check "no setting: popup with the message, as in Onion" grep -qx 'infoPanel --message LOADING --persistent --romscreen' "$TMP/calls"
rm -f "$TMP/calls"
echo 1 > "$TMP/config/display/launchScreen"
show_launch_screen SAVING; wait
check "full screen: screenshot without message" grep -qx 'infoPanel --persistent --romscreen' "$TMP/calls"
check "full screen: dismissed like the popup" grep -qx 'touch /tmp/dismiss_info_panel' "$TMP/calls"
rm -f "$TMP/calls"
echo 2 > "$TMP/config/display/launchScreen"
show_launch_screen LOADING; wait
check "black: screen cleared, no infoPanel" test "$(cat "$TMP/calls")" = "bootScreen clear"
rm -f "$TMP/calls"
echo 3 > "$TMP/config/display/launchScreen"
show_launch_screen SAVING; wait
check "full popup: message without screenshot" grep -qx 'infoPanel --message SAVING --persistent' "$TMP/calls"
check "full popup: dismissed like the popup" grep -qx 'touch /tmp/dismiss_info_panel' "$TMP/calls"
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
check "red: RetroArch title colour" grep -qx 'rgui_title_color = "0xFFc43b59"' "$sd/RetroArch/.retroarch/assets/rgui/Onion.cfg"
check "red: AdvanceMENU accents" test "$(grep -c 9e2c45 "$sd/BIOS/.advance/advmenu.rc")" -eq 2
check "red: other AdvanceMENU settings untouched" grep -qx 'ui_mode custom 7247c2' "$sd/BIOS/.advance/advmenu.rc"
check "red: PCSX package and installed menu colour" test "$(grep -c '^selection_color=9e2c45$' "$sd/$pcsx_pkg" "$sd/RApp/PCSX-ReARMed/skin/skin.txt" | grep -c ':1$')" -eq 2
check "red: state remembers the theme" test "$(cat "$sd/.tmp_update/config/.systemSkin")" = "$sd/Themes/Red"

apply_system_skin "$sd/Themes/DiMo/"
check "onion: Package Manager artwork restored" test "$(cat "$sd/App/PackageManager/res/bgApp.png")" = onion
check "onion: installed Quick Guide restored" test "$(cat "$sd/App/Onion_Manual/page1.png")" = onion
check "onion: RetroArch title colour restored" grep -qx 'rgui_title_color = "0xFF9160fd"' "$sd/RetroArch/.retroarch/assets/rgui/Onion.cfg"
check "onion: AdvanceMENU accents restored" test "$(grep -c 7247c2 "$sd/BIOS/.advance/advmenu.rc")" -eq 3
check "onion: PCSX colour restored" grep -qx 'selection_color=8526eb' "$sd/RApp/PCSX-ReARMed/skin/skin.txt"
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
    .tmp_update/res/themes_has_icons.png) echo "$ROOT/src/themeSwitcher/res/themes_has_icons.png" ;;
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
cp "$(command -v sh)" "$TMP/st"
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
check "OTA menus leave with MENU only (-m: START ignored)" test "$(grep -c 'shellect.sh -m -t' "$ROOT/static/build/.tmp_update/script/ota_update.sh")" -eq 3
end

# ---- ota_update.sh: "Press A to continue, MENU to exit" ----

OTA_SCRIPT="$ROOT/static/build/.tmp_update/script/ota_update.sh"

begin ota_info_screen_menu_exits
eval "$(extract_fn "$OTA_SCRIPT" wait_key)"
printf ' ' > "$TMP/a"
wait_key "x" < "$TMP/a" > /dev/null
check "A continues" test $? -eq 0
printf 'x\n ' > "$TMP/other"
wait_key "x" < "$TMP/other" > /dev/null
check "other buttons (START...) are ignored until A" test $? -eq 0
printf '\033' > "$TMP/menu"
wait_key "x" < "$TMP/menu" > /dev/null
check "MENU (a lone Esc) leaves" test $? -eq 1
wait_key "x" < /dev/null > /dev/null
check "no input (terminal gone) leaves" test $? -eq 1
check "the version screen offers MENU to exit" grep -q 'wait_key "Press A to continue, MENU to exit" || exit 0' "$OTA_SCRIPT"
unset -f wait_key
end

# ---- migration 00022: RetroArch menu Border Filler OFF ----

begin migration_disables_rgui_border_filler
MIG="$ROOT/static/build/.tmp_update/script/migration/00022_disable_rgui_border_filler.sh"
printf 'menu_driver = "rgui"\nrgui_border_filler_enable = "true"\nrgui_border_filler_thickness_enable = "true"\n' > "$TMP/ra.cfg"
RA_CFG="$TMP/ra.cfg" sh "$MIG"
check "existing setting turned off" grep -qx 'rgui_border_filler_enable = "false"' "$TMP/ra.cfg"
check "other settings untouched" grep -qx 'rgui_border_filler_thickness_enable = "true"' "$TMP/ra.cfg"
check "only one line for the setting" test "$(grep -c '^rgui_border_filler_enable' "$TMP/ra.cfg")" -eq 1
printf 'menu_driver = "rgui"\n' > "$TMP/ra2.cfg"
RA_CFG="$TMP/ra2.cfg" sh "$MIG"
check "missing setting added as off" grep -qx 'rgui_border_filler_enable = "false"' "$TMP/ra2.cfg"
RA_CFG="$TMP/none.cfg" sh "$MIG"
check "no RetroArch config: nothing created" test ! -e "$TMP/none.cfg"
check "new installs default to off" grep -qx 'rgui_border_filler_enable = "false"' "$ROOT/static/configs/RetroArch/.retroarch/retroarch.cfg"
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

begin warmup_starts_after_display_before_mainui_layout
boot_fn=$(extract_fn "$RUNTIME" init_system)
line_of() { printf '%s\n' "$boot_fn" | grep -n "^ *$1" | head -1 | cut -d: -f1; }
res=$(line_of get_screen_resolution)
warm=$(line_of start_warmup)
layout=$(line_of "commit_mainui_fbmode")
check "after the fb driver is up (lines $res < $warm)" test "${res:-0}" -gt 0 -a "${warm:-0}" -gt "${res:-0}"
check "before MainUI's layout is set (lines $warm < $layout)" test "${layout:-0}" -gt "${warm:-0}"
check "no sleep left between them" test -z "$(printf '%s\n' "$boot_fn" | sed -n "${warm:-1},${layout:-1}p" | grep -E '^ *sleep')"
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
    check "$core: R3 unbound" grep -qx 'input_player1_btn_r3 = "-1"' "$remaps/$core/$core.rmp"
done
cores="$ROOT/static/build/RetroArch/.retroarch/cores"
for info in "$cores"/fbalpha2012*_libretro.info "$cores"/fbneo_libretro.info; do
    name=$(sed -n 's/^corename = "\(.*\)"$/\1/p' "$info")
    check "remap folder matches the core name: $name" test -f "$remaps/$name/$name.rmp"
done
begin wifi_starts_with_power_save_on
# Power save off kept the radio awake while idle: a Mini Plus in the menu
# with Wi-Fi connected drained about 20% faster and ran warm. Wi-Fi now
# starts with power save on, as Onion's stock MainUI ran with it.
eval "$(extract_fn "$NETWORK" wifi_on)"
log() { :; }
sleep() { :; }
ifconfig() { :; }
udhcpc() { :; }
iw() { echo "$*" >> "$TMP/iw"; }
miyoodir_saved=${miyoodir-}
miyoodir=$TMP
mkdir -p "$TMP/app"
printf '#!/bin/sh\n' > "$TMP/app/wpa_supplicant"
chmod +x "$TMP/app/wpa_supplicant"
wifi_on > /dev/null 2>&1
check "wifi_on turns power save on" grep -qx "dev wlan0 set power_save on" "$TMP/iw"
check "the OTA's Wi-Fi start turns it on too" grep -q "iw dev wlan0 set power_save on" "$OTA"
check "no script hardcodes power save off" sh -c "! grep -rq 'power_save off' \"$ROOT/static/build\""
miyoodir=$miyoodir_saved
unset -f log sleep ifconfig udhcpc iw wifi_on
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
check "default hint kept" grep -qx "Press A to open, START to exit." "$TMP/hint"
check "the choice is written" grep -q "/a.pdf" "$TMP/media/.selected.pls"
sh "$TMP/sel.sh" "PDF Reader" "$TMP/media" "pdf" "A: open, START: exit, MENU: close a PDF." > /dev/null 2>&1
check "a caller's hint is shown" grep -qx "A: open, START: exit, MENU: close a PDF." "$TMP/hint"
check "the PDF Reader passes the MENU hint" grep -qF '"pdf" "A: open, START: exit, MENU: close a PDF."' "$ROOT/static/packages/App/PDF Reader (Green)/App/Green/launch.sh"
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
    printf '#!/bin/sh\nfor a; do last=$a; done\necho "$last" >> "%s/played"\n' "$TMP" > "$SD/.tmp_update/bin/ffplay"
    chmod +x "$SD/.tmp_update/bin/green/green" "$SD/.tmp_update/bin/ffplay"
    PDF_LAUNCH="$ROOT/static/packages/App/PDF Reader (Green)/App/Green/launch.sh"
    VID_LAUNCH="$ROOT/static/packages/App/Video Player (FFplay)/App/FFplay/launch.sh"
    sed -e "s#/mnt/SDCARD#$SD#g" "$PDF_LAUNCH" > "$TMP/pdf.sh"
    sed -e "s#/mnt/SDCARD#$SD#g" -e "s#/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor#$TMP/sys/governor#g" -e "s#/tmp/stay_awake#$TMP/stay_awake#g" "$VID_LAUNCH" > "$TMP/video.sh"
    export FAKE_ST="$TMP/st"
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
    timeout 30 sh "$TMP/pdf.sh" > /dev/null 2>&1
    check "PDF: A opens the file under the cursor" test "$(cat "$TMP/opened" 2> /dev/null)" = "$SD/Media/PDF/a.pdf"
    check "PDF: START in the list leaves the app (no reopening)" test "$(lists_shown)" = 2

    # MENU in the list leaves too.
    scenes "MENU"
    timeout 30 sh "$TMP/pdf.sh" > /dev/null 2>&1
    check "PDF: MENU in the list leaves the app" sh -c "test ! -f '$TMP/opened' && test \"\$(cat '$FAKE_ST/scene')\" = 1"

    # Two PDFs in a row, then START.
    scenes "DOWN A" "DOWN DOWN A" "START"
    timeout 30 sh "$TMP/pdf.sh" > /dev/null 2>&1
    check "PDF: the second choice opens the second file" test "$(sed -n 2p "$TMP/opened")" = "$SD/Media/PDF/b.pdf"
    check "PDF: then START leaves" test "$(lists_shown)" = 3

    # Video Player: same list, same keys.
    scenes "DOWN A" "START"
    timeout 30 sh "$TMP/video.sh" > /dev/null 2>&1
    check "Video: A plays the file under the cursor" test "$(cat "$TMP/played" 2> /dev/null)" = "$SD/Media/Videos/a.mp4"
    check "Video: START in the list leaves the app" test "$(lists_shown)" = 2
    unset FAKE_ST
    unset -f scenes lists_shown
fi
end

begin release_named_redonion_with_onionplus_copy
# The project is RedOnion. Updaters before the rename (beta 8 and earlier)
# select an asset containing "OnionPlus-v", so every release also carries
# the same zip under that name; ota_update.sh takes either.
PRE="$ROOT/.github/workflows/pre-release.yml"
TAGGED="$ROOT/.github/workflows/tagged-release.yml"
check "the Makefile names the zip RedOnion" grep -qx "TARGET=RedOnion" "$ROOT/Makefile"
check "release tag and title are RedOnion" sh -c "grep -qF 'RELEASE_TAG=RedOnion-v' '$PRE' && grep -qF 'RELEASE_TITLE=RedOnion V' '$PRE'"
check "the release zip is RedOnion-v" grep -qF 'ZIP_NAME=RedOnion-v${BUILD_VERSION}-${SHA_SHORT}.zip' "$PRE"
check "an OnionPlus-v copy is made" grep -qF 'cp "release/${ZIP_NAME}" "release/${LEGACY_ZIP_NAME}"' "$PRE"
check "the copy keeps the same version" grep -qF 'LEGACY_ZIP_NAME=OnionPlus-v${BUILD_VERSION}-${SHA_SHORT}.zip' "$PRE"
check "both zips are published" sh -c "grep -qF 'release/\${{ env.ZIP_NAME }}' '$PRE' && grep -qF 'release/\${{ env.LEGACY_ZIP_NAME }}' '$PRE'"
check "tagged releases carry both too" sh -c "grep -qF 'release/RedOnion-v' '$TAGGED' && grep -qF 'release/OnionPlus-v' '$TAGGED'"
check "the updater takes both names" grep -qx 'PACKAGE_PREFIXES="RedOnion-v OnionPlus-v"' "$OTA"
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
check "Netplay.sh turns power save on at the end" sh -c \
    "grep -v '^[[:space:]]*\$' \"$NETPLAY\" | tail -n 1 | grep -qx '\"\\\$sysdir/script/network/wifi_power_save.sh\" on'"
check "only netplay turns power save off" sh -c \
    "! grep -rl 'wifi_power_save.sh\" off' \"$ROOT/static/build\" | grep -v '/script/netplay/'"
check "the helper is executable" [ -x "$PS" ]

mkdir -p "$TMP/sys/bin" "$TMP/net/wlan0"
printf '#!/bin/sh\necho "$*" >> "%s/iw"\necho "$LD_LIBRARY_PATH" > "%s/ld"\n' "$TMP" "$TMP" > "$TMP/sys/bin/iw"
chmod +x "$TMP/sys/bin/iw"
sysdir="$TMP/sys" WIFI_NET_DIR="$TMP/net" sh "$PS" off
check "off: iw sets wlan0 power save off" grep -qx "dev wlan0 set power_save off" "$TMP/iw"
check "iw finds its libraries" grep -q "$TMP/sys/lib/parasyte" "$TMP/ld"
rm -f "$TMP/iw"
sysdir="$TMP/sys" WIFI_NET_DIR="$TMP/net" sh "$PS" on
check "on: iw sets wlan0 power save on" grep -qx "dev wlan0 set power_save on" "$TMP/iw"
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

begin migration_adds_core_systemid
mig="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}/static/build/.tmp_update/script/migration/00024_core_info_systemid.sh"
mkdir -p "$TMP/cores"
printf 'corename = "GW"\nsystemname = "Handheld Electronic"' > "$TMP/cores/gw_libretro.info"
printf 'systemid = "game_boy_advance"\n' > "$TMP/cores/gpsp_libretro.info"
touch "$TMP/cores/core_info.cache"
CORES_DIR="$TMP/cores" sh "$mig"
CORES_DIR="$TMP/cores" sh "$mig"
check "gw gets a systemid" grep -qx 'systemid = "handheld_electronic"' "$TMP/cores/gw_libretro.info"
check "added once" test "$(grep -c systemid "$TMP/cores/gw_libretro.info")" = 1
check "the last line stays intact" grep -qx 'systemname = "Handheld Electronic"' "$TMP/cores/gw_libretro.info"
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
