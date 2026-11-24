#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
./build.sh host >/dev/null
mkdir -p build/tests
cat > build/tests/original.c <<'C'
int redamp_test_original(int a) { return a + 1; }
C
${CC:-gcc} -shared -fPIC build/tests/original.c -o build/tests/original.so
${CC:-gcc} -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
  -I. -Ithird_party tools/test_gamemusic.c gamemusic.c \
  -Wl,--gc-sections -pthread -lm -o build/tests/test_gamemusic
mkdir -p build/tests/gm-app/lib build/tests/gm-data build/tests/gm-system
cp build/libpadsp_wrap.so build/tests/gm-app/lib/
# Use separate empty directories on each run.
rm -f build/tests/gm-data/* build/tests/gm-system/*
ASAN_OPTIONS=detect_leaks=0 build/tests/test_gamemusic "$PWD/build/tests/gm-app" \
  "$PWD/build/tests/gm-data" "$PWD/build/tests/gm-system" "$PWD/build/tests/original.so"
