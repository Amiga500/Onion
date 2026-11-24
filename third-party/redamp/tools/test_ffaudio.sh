#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
B=build/tests/ffaudio
mkdir -p "$B/empty/lib" "$B/installed/lib" "$B/legacy/lib"
${CC:-gcc} -O2 -g -std=gnu11 -Wall -I. -fPIC -shared \
    tools/mock_ffbridge.c -o "$B/installed/lib/libredamp_ffaudio.so"
${CC:-gcc} -O2 -g -std=gnu11 -Wall -I. -DMOCK_LEGACY -fPIC -shared \
    tools/mock_ffbridge.c -o "$B/legacy/lib/libredamp_ffaudio.so"
${CC:-gcc} -O2 -g -std=gnu11 -Wall -I. \
    tools/test_ffaudio.c ffaudio.c netclock.c -DM2D_HOST -o "$B/test_ffaudio" -ldl -lpthread
MA_HOME="$(pwd)/$B/empty" "$B/test_ffaudio" missing
MA_HOME="$(pwd)/$B/installed" "$B/test_ffaudio" present
MA_HOME="$(pwd)/$B/legacy" "$B/test_ffaudio" legacy
${CC:-gcc} -O2 -g -std=gnu11 -I. -Ithird_party \
    tools/test_ffmpeg_decode.c decode.c ffaudio.c radio.c netclock.c -DM2D_HOST opus_ogg.c \
    -o "$B/test_ffmpeg_decode" -lm -ldl -lpthread -lcurl -lz
MA_HOME="$(pwd)/$B/installed" "$B/test_ffmpeg_decode"
