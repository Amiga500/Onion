#!/bin/sh
# Fetch exactly the verified CC0 minimp3.h revision, never master.
# SHA-256 for this exact file is published by FreeBSD audio/minimp3 ports.
set -eu
cd "$(dirname "$0")"
COMMIT=afb604c06bc8beb145fecd42c0ceb5bda8795144
EXPECTED=5fb296a790734772b65a5514438cc06d14f24e539c13475ff2a7f737681c82c0
URL="https://raw.githubusercontent.com/lieff/minimp3/$COMMIT/minimp3.h"
check() {
    [ -s "$1" ] || return 1
    [ "$(sha256sum "$1" | awk '{print $1}')" = "$EXPECTED" ]
}
if [ -f minimp3.h ]; then
    check minimp3.h || { echo 'minimp3.h has unexpected SHA-256; refusing unsafe build.' >&2; exit 1; }
    exit 0
fi
trap 'rm -f minimp3.h.tmp' EXIT HUP INT TERM
if command -v curl >/dev/null 2>&1; then
    curl -fLsS --connect-timeout 10 --max-time 60 "$URL" -o minimp3.h.tmp
elif command -v wget >/dev/null 2>&1; then
    wget -q -T 60 -O minimp3.h.tmp "$URL"
else
    echo 'Need curl or wget to fetch pinned minimp3.h.' >&2
    exit 1
fi
check minimp3.h.tmp || { echo 'minimp3.h download SHA-256 mismatch; refusing build.' >&2; exit 1; }
mv minimp3.h.tmp minimp3.h
