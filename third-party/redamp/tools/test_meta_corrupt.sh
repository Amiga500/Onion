#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests
${CC:-gcc} -O1 -g -I. -Ithird_party -fsanitize=address,undefined -fno-omit-frame-pointer \
 -ffunction-sections -fdata-sections tools/test_meta_corrupt.c \
 -Wl,--gc-sections -o build/tests/test_meta_corrupt
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 build/tests/test_meta_corrupt
