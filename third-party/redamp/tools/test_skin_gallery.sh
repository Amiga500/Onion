#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
${CC:-gcc} -O1 -g -I. -Ithird_party \
 -ffunction-sections -fdata-sections \
 tools/test_skin_gallery.c skin.c zip.c -Wl,--gc-sections -o build/test_skin_gallery -lm -lz
APP=${REDAMP_APP_DIR:-}
if [ -z "$APP" ]; then
    if [ -f ../App/RedAmp/launch.sh ]; then APP=../App/RedAmp
    else APP=../../App/RedAmp; fi
fi
# Always validate the default first; include any optional original CC0 skins.
set -- "$APP/skins/RedOnion.wsz" "$APP/skins/Breeze-Dark.wsz" \
       "$APP/skins/Graphite.wsz"
for skin in "$APP"/skins/*.wsz; do
    case "$skin" in
        */RedOnion.wsz|*/Breeze-Dark.wsz|*/Graphite.wsz) ;;
        *) set -- "$@" "$skin" ;;
    esac
done
./build/test_skin_gallery "$@"
