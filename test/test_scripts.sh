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
