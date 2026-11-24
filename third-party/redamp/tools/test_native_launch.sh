#!/bin/sh
# Shell-only test: source fragment must not modify libraries or opt in itself.
set -eu
cd "$(dirname "$0")/.."
T="$PWD/build/tests/native-hook"
mkdir -p "$T/app/lib" "$T/app/data" "$T/system"
rm -f "$T/app/data/game_music.consent" "$T/app/data/settings.ini"
APP=${REDAMP_APP_DIR:-}
if [ -z "$APP" ]; then
    if [ -f ../App/RedAmp/launch.sh ]; then APP=../App/RedAmp
    else APP=../../App/RedAmp; fi
fi
cp "$APP/redonion-launch-audio.sh" "$T/fragment.src"
sed -e "s|/mnt/SDCARD/App/RedAmp|$T/app|g" \
    -e "s|/mnt/SDCARD/miyoo/lib/libpadsp.so|$T/system/libpadsp.so|g" \
    "$T/fragment.src" > "$T/fragment"
: > "$T/system/libpadsp.so"
: > "$T/app/lib/libpadsp_wrap.so"
LD_PRELOAD='test-preload'
export LD_PRELOAD
. "$T/fragment"
[ "$LD_PRELOAD" = 'test-preload' ] || exit 1
printf 'game_music=1\n' > "$T/app/data/settings.ini"
: > "$T/app/data/game_music.consent"
. "$T/fragment"
case "$LD_PRELOAD" in *libpadsp_wrap.so*libpadsp.so*test-preload*) :;; *) exit 1;; esac
[ "${REDAMP_NATIVE_PRELOAD:-}" = 1 ] || exit 1
PRE="$LD_PRELOAD"
. "$T/fragment"
[ "$LD_PRELOAD" = "$PRE" ] || { echo 'duplicate wrapper' >&2;exit 1; }
# Do not combine native preload with a legacy swapped wrapper.
printf 'MINIAMP_PADSP_WRAPPER_v1' > "$T/system/libpadsp.so"
LD_PRELOAD='test-preload'
unset REDAMP_NATIVE_PRELOAD
. "$T/fragment"
[ "$LD_PRELOAD" = 'test-preload' ] || exit 1
# Restore test input; the real hook did not change any library.
: > "$T/system/libpadsp.so"
printf 'RedOnion launch-only preload and consent: PASS\n'
