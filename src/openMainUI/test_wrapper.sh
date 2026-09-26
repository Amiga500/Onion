#!/bin/sh
# Host test for install.sh and the generated wrapper. No device needed:
#   sh src/openMainUI/test_wrapper.sh
set -eu
here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
pass=0

check() {
    if [ "$2" = "$3" ]; then
        pass=$((pass + 1))
    else
        echo "FAIL: $1: expected '$3', got '$2'" >&2
        exit 1
    fi
}

sd="$work/sd"
tmp="$work/tmp"
mkdir -p "$sd/.tmp_update/bin" "$sd/.tmp_update/config" "$sd/miyoo/app" "$tmp"

# Fake stock launchers and a fake Open MainUI that report who ran.
for v in MainUI-354-clean MainUI-354-expert MainUI-283-clean; do
    printf '#!/bin/sh\necho stock:%s > "$RAN"\n' "$v" > "$sd/.tmp_update/bin/$v"
done
cat > "$work/open" << 'EOF'
#!/bin/sh
echo "open:$*" > "$RAN"
echo "open:$*"
[ -n "${FAKE_OPEN_DRAWS:-}" ] && rm -f "$MAINUI_START_MARKER"
exit 0
EOF

sh "$here/install.sh" "$sd/.tmp_update/bin" "$sd" "$work/open" "$here/MainUI-wrapper.sh.in" > /dev/null
# Second run must be idempotent (wrappers regenerated, stock kept).
sh "$here/install.sh" "$sd/.tmp_update/bin" "$sd" "$work/open" "$here/MainUI-wrapper.sh.in" > /dev/null
check "stock kept" "$(RAN=/dev/stdout sh "$sd/.tmp_update/mainui-test/stock/MainUI-354-clean")" "stock:MainUI-354-clean"
check "wrappers" "$(grep -l OPEN_MAINUI_WRAPPER "$sd"/.tmp_update/bin/MainUI-* | wc -l | tr -d ' ')" "3"

# Launchers record who ran in $RAN; stdout may legitimately go to /dev/null.
RAN="$work/ran"
export RAN
run() {
    rm -f "$RAN"
    OPEN_MAINUI_SD="$sd" OPEN_MAINUI_TMP="$tmp" sh "$sd/.tmp_update/bin/$1" > "$work/stdout" 2> /dev/null
    cat "$RAN" 2> /dev/null
}

# 1. Normal start runs Open MainUI with the device arguments.
out=$(FAKE_OPEN_DRAWS=1 run MainUI-354-clean)
check "normal start" "$out" "open:--sd-root $sd --device real --handoff-dir $tmp"
check "marker cleared" "$([ -f "$tmp/open-mainui.starting" ] && echo yes || echo no)" "no"

# 2. DISABLED selects this variant's own stock binary.
touch "$sd/.tmp_update/mainui-test/DISABLED"
check "disabled 354-expert" "$(run MainUI-354-expert)" "stock:MainUI-354-expert"
check "disabled 283-clean" "$(run MainUI-283-clean)" "stock:MainUI-283-clean"
rm "$sd/.tmp_update/mainui-test/DISABLED"

# 3. Two starts without a first frame -> stock until reboot (tmp cleared).
run MainUI-354-clean > /dev/null   # no draw: marker stays
run MainUI-354-clean > /dev/null   # fails=1
check "crash fallback" "$(run MainUI-354-clean)" "stock:MainUI-354-clean"
check "fallback sticks" "$(FAKE_OPEN_DRAWS=1 run MainUI-354-clean)" "stock:MainUI-354-clean"
rm -f "$tmp"/open-mainui.*        # simulated reboot
check "retry after reboot" "$(FAKE_OPEN_DRAWS=1 run MainUI-354-clean)" \
    "open:--sd-root $sd --device real --handoff-dir $tmp"

# 4. Logging on a read-only logs directory must still start the launcher.
touch "$sd/.tmp_update/config/.logging"
mkdir -p "$sd/.tmp_update/logs"
chmod 500 "$sd/.tmp_update/logs"
out=$(FAKE_OPEN_DRAWS=1 run MainUI-354-clean)
chmod 700 "$sd/.tmp_update/logs"
if [ "$(id -u)" = 0 ]; then
    echo "note: running as root, read-only log case not meaningful" >&2
else
    check "read-only log" "$out" "open:--sd-root $sd --device real --handoff-dir $tmp"
fi

# 5. Logging enabled and writable: output goes to the log, not stdout.
: > "$sd/.tmp_update/logs/MainUI.log"
FAKE_OPEN_DRAWS=1 run MainUI-354-clean > /dev/null
check "log stdout empty" "$(cat "$work/stdout")" ""
check "log written" "$(grep -c '^open:' "$sd/.tmp_update/logs/MainUI.log")" "1"
rm "$sd/.tmp_update/config/.logging"

# 6. Missing Open MainUI binary -> stock.
mv "$sd/.tmp_update/mainui-test/MainUI" "$work/saved"
check "binary missing" "$(run MainUI-354-clean)" "stock:MainUI-354-clean"
mv "$work/saved" "$sd/.tmp_update/mainui-test/MainUI"

# 7. Stock missing while disabled -> Open MainUI as last resort.
mv "$sd/.tmp_update/mainui-test/stock/MainUI-283-clean" "$work/stock283"
touch "$sd/.tmp_update/mainui-test/DISABLED"
out=$(FAKE_OPEN_DRAWS=1 run MainUI-283-clean)
check "stock missing" "$out" "open:--sd-root $sd --device real --handoff-dir $tmp"

echo "test_wrapper: $pass checks passed"
