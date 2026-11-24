#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests/ioctl
${HOST_CC:-gcc} -std=gnu11 -O2 -fPIC -shared -I. padsp_wrap.c -o build/tests/ioctl/libpadsp_wrap.so -ldl
${HOST_CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -I. tools/test_ioctl_preload.c -o build/tests/ioctl/check
REDAMP_NATIVE_PRELOAD=1 LD_PRELOAD="$PWD/build/tests/ioctl/libpadsp_wrap.so" \
    build/tests/ioctl/check
