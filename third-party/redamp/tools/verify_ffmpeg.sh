#!/bin/sh
# Verify the self-contained private FFmpeg ARM shared libraries from src/RedAmp/lib.
# Optional: tools/verify_ffmpeg.sh /path/to/staging/lib
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
DIR=${1:-"$ROOT/lib"}
readelf=${READELF:-readelf}
for spec in 'avutil 56' 'avcodec 58' 'avformat 58' 'swresample 3'; do
    set -- $spec
    f="$DIR/lib${1}_redamp.so.${2}"
    [ -f "$f" ] || { echo "Missing $f" >&2; exit 1; }
    "$readelf" -h "$f" | grep -q 'Machine:.*ARM' || { echo "Not ARM: $f" >&2; exit 1; }
done
for lib in libssl.so.3 libcrypto.so.3; do
    f="$DIR/$lib"
    [ -f "$f" ] || { echo "Missing ARM TLS runtime $f" >&2; exit 1; }
    "$readelf" -h "$f" | grep -q 'Machine:.*ARM' || { echo "Not ARM: $f" >&2; exit 1; }
done
f="$DIR/libredamp_ffaudio.so"
[ -f "$f" ] || { echo "Missing bridge $f" >&2; exit 1; }
"$readelf" -h "$f" | grep -q 'Machine:.*ARM' || { echo "Not ARM: $f" >&2; exit 1; }
for f in "$DIR"/lib*_redamp.so.* "$DIR/libredamp_ffaudio.so"; do
    "$readelf" -d "$f" | grep NEEDED | grep -Eq 'libav(codec|format|util)|libswresample' && {
        if "$readelf" -d "$f" | grep NEEDED | grep -E 'lib(av(codec|format|util)|swresample)\.so\.'; then
            echo "Unsafe unsuffixed FFmpeg DT_NEEDED: $f" >&2; exit 1
        fi
    }
done
"$readelf" -d "$DIR/libredamp_ffaudio.so" | grep -q 'libavformat_redamp.so.58' || {
    echo 'Bridge is not linked against the suffixed FFmpeg library' >&2;exit 1;
}
echo 'RedAmp FFmpeg ARM private ABI: PASS'
