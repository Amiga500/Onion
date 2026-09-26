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
