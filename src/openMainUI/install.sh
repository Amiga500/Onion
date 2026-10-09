#!/bin/sh
# Installs Open MainUI into an Onion build tree (test branch only).
#
#   install.sh BIN_DIR BUILD_DIR OPEN_MAINUI_BINARY WRAPPER_TEMPLATE
#
# For every .tmp_update/bin/MainUI-*-clean / -expert variant:
#   original binary -> .tmp_update/mainui-test/stock/<variant>
#   wrapper         -> .tmp_update/bin/<variant>   (knows its own stock file)
# and the Open MainUI binary goes to .tmp_update/mainui-test/MainUI.
# Safe to run again: an existing wrapper is regenerated, never moved to stock.
set -eu

bin_dir=$1
build_dir=$2
binary=$3
template=$4
testdir="$build_dir/.tmp_update/mainui-test"
marker=OPEN_MAINUI_WRAPPER

fail() {
    echo "open-mainui install: $*" >&2
    exit 1
}

[ -f "$binary" ] || fail "binary not found: $binary (did 'make device' run?)"
[ -f "$template" ] || fail "wrapper template not found: $template"
[ -d "$bin_dir" ] || fail "bin directory not found: $bin_dir"
mkdir -p "$testdir/stock"

count=0
for target in "$bin_dir"/MainUI-*-clean "$bin_dir"/MainUI-*-expert; do
    [ -f "$target" ] || continue
    variant=$(basename "$target")
    if grep -q "$marker" "$target" 2> /dev/null; then
        [ -f "$testdir/stock/$variant" ] || fail "$variant is a wrapper but its stock copy is missing"
    else
        mv -f "$target" "$testdir/stock/$variant"
    fi
    sed "s|@VARIANT@|$variant|g" "$template" > "$target"
    chmod 755 "$target" "$testdir/stock/$variant"
    count=$((count + 1))
    echo "open-mainui: wrapped $variant"
done
[ "$count" -gt 0 ] || fail "no MainUI-*-clean/-expert files in $bin_dir"

cp -f "$binary" "$testdir/MainUI"
chmod 755 "$testdir/MainUI"
echo "open-mainui: installed $(wc -c < "$binary") byte binary, $count variant(s) wrapped"
