#!/bin/sh
# Rebuild only RedAmp's FFmpeg bridge against the *already built* private ARM
# FFmpeg libraries. Use make ffmpeg-audio if the FFmpeg sources/options change.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
B=${REDAMP_FFMPEG_BUILD:-$ROOT/build/ffmpeg-redamp-arm}
CC="$B/cross-tools/arm-linux-gnueabihf-gcc"
FFSRC="$B/private-source"
if [ ! -x "$CC" ] || [ ! -f "$B/config.h" ] || \
   [ ! -f "$FFSRC/libavformat/avformat.h" ] || \
   [ ! -f "$B/libavformat/libavformat_redamp.so.58" ]; then
    echo 'Private ARM FFmpeg has not been built; run make ffmpeg-audio first.' >&2
    exit 1
fi
"$CC" -std=gnu11 -Os -g0 -fPIC -shared -Wall -Wextra \
    -Wno-deprecated-declarations \
    -I"$ROOT" -I"$B" -I"$FFSRC" \
    "$ROOT/ffbridge.c" "$ROOT/adts_duration.c" -o "$B/libredamp_ffaudio.so" \
    -L"$B/libavformat" -L"$B/libavcodec" \
    -L"$B/libavutil" -L"$B/libswresample" \
    -Wl,-rpath,'$ORIGIN' -Wl,-z,defs \
    -lavformat_redamp -lavcodec_redamp -lswresample_redamp -lavutil_redamp -lm -lpthread
mkdir -p "$ROOT/lib"
cp "$B/libredamp_ffaudio.so" "$ROOT/lib/libredamp_ffaudio.so"
readelf -h "$ROOT/lib/libredamp_ffaudio.so" | grep -q 'Machine:.*ARM' || {
    echo 'FFmpeg bridge was not built for ARM' >&2; exit 1;
}
echo 'ARM FFmpeg bridge updated; all previously built FFmpeg libraries were reused.'
