#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC="${CC:-gcc}"
"$CC" -O1 -g -Wall -Wno-format-truncation -I. -fsanitize=address,undefined -fno-omit-frame-pointer \
 library.c tools/test_library.c -o build/test_library -lpthread
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/test_library
