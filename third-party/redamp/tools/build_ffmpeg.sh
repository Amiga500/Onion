#!/bin/sh
# RedAmp-only FFmpeg 4.4.8 ARM shared audio libraries.
# MiniCinema's FFmpeg source is reused (never modified).
# All generated files and outputs live in src/RedAmp, with no App dependency.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
: "${FFMPEG_SRC:=/root/workspace/ffmpeg-4.4.8}"
: "${ZIG_BIN:=/root/workspace/zig-0.14.1/zig}"
: "${ZIG_TARGET:=arm-linux-gnueabihf.2.28}"
: "${JOBS:=4}"
: "${REDAMP_FFMPEG_BUILD:=$ROOT/build/ffmpeg-redamp-arm}"
if [ ! -f "$FFMPEG_SRC/configure" ]; then
    echo "FFmpeg 4.4.8 source not found: $FFMPEG_SRC" >&2; exit 1
fi
FFMPEG_SRC=$(CDPATH= cd -- "$FFMPEG_SRC" && pwd -P)
# Authentic source tarball digest (from the upstream 4.4.8 release archive).
# A pre-extracted MiniCinema source tree is accepted without re-downloading.
FFMPEG_SHA256=c73848c4ae283d9eaee7be3b276affbc3543380483555500d0dd2c9b7e1c39c3
if [ -n "${FFMPEG_TARBALL:-}" ]; then
    [ -f "$FFMPEG_TARBALL" ] || { echo "Tarball missing: $FFMPEG_TARBALL" >&2; exit 1; }
    actual_hash=$(sha256sum "$FFMPEG_TARBALL" | cut -d' ' -f1)
    [ "$actual_hash" = "$FFMPEG_SHA256" ] || {
        echo 'FFmpeg 4.4.8 tarball SHA256 mismatch' >&2; exit 1;
    }
fi
if [ -f "$FFMPEG_SRC/VERSION" ]; then
    actual=$(tr -d ' \t\r\n' < "$FFMPEG_SRC/VERSION")
    [ "$actual" = 4.4.8 ] || { echo "Expected FFmpeg 4.4.8, found $actual" >&2;exit 1; }
else
    echo "Expected an FFmpeg 4.4.8 release tree with a VERSION file" >&2;exit 1
fi
[ -x "$ZIG_BIN" ] || { echo "Zig missing: $ZIG_BIN (set ZIG_BIN)" >&2;exit 1; }
# Reuse the ARM OpenSSL development files from the MiniCinema toolchain if
# present. OPENSSL_ARM_PREFIX remains an override for custom ARM builds.
if [ -z "${OPENSSL_ARM_PREFIX:-}" ]; then
    for root in "$ROOT/build/arm-deps/prefix" \
                /opt/miyoomini-toolchain/usr \
                /usr/arm-linux-gnueabihf /usr; do
        [ -f "$root/include/openssl/ssl.h" ] || continue
        for ld in "$root/lib" "$root/lib/arm-linux-gnueabihf" \
                  "$root/arm-linux-gnueabihf/lib"; do
            if [ -e "$ld/libssl.so" ] && [ -e "$ld/libcrypto.so" ] &&
               [ -e "$ld/libssl.so.3" ] && [ -e "$ld/libcrypto.so.3" ] &&
               readelf -h "$ld/libssl.so.3" 2>/dev/null | grep -q 'Machine:.*ARM' &&
               readelf -h "$ld/libcrypto.so.3" 2>/dev/null | grep -q 'Machine:.*ARM'; then
                OPENSSL_ARM_PREFIX=$root
                echo "ARM OpenSSL found: $OPENSSL_ARM_PREFIX"
                # Preserve the detected nonstandard libdir (e.g. multiarch).
                DETECTED_SSL_LIB=$ld
                break
            fi
        done
        [ -z "${OPENSSL_ARM_PREFIX:-}" ] || break
    done
fi
if [ -z "${OPENSSL_ARM_PREFIX:-}" ]; then
    echo 'No ARM OpenSSL 3 in sysroot; bootstrapping private ARM dependencies...'
    "$ROOT/tools/build_arm_deps.sh"
    OPENSSL_ARM_PREFIX="$ROOT/build/arm-deps/prefix"
    DETECTED_SSL_LIB="$OPENSSL_ARM_PREFIX/lib"
fi
[ -f "$OPENSSL_ARM_PREFIX/include/openssl/ssl.h" ] || {
    echo "Missing ARM OpenSSL development headers under $OPENSSL_ARM_PREFIX/include" >&2;exit 1;
}
if [ -n "${DETECTED_SSL_LIB:-}" ]; then
    SSL_LIB=$DETECTED_SSL_LIB
elif [ -d "$OPENSSL_ARM_PREFIX/lib" ]; then
    SSL_LIB="$OPENSSL_ARM_PREFIX/lib"
elif [ -d "$OPENSSL_ARM_PREFIX/lib64" ]; then
    SSL_LIB="$OPENSSL_ARM_PREFIX/lib64"
else
    echo "Missing ARM OpenSSL libraries in $OPENSSL_ARM_PREFIX/lib" >&2;exit 1
fi
# Verify that the selected runtime libraries are actually 32-bit ARM.
for lib in libssl.so.3 libcrypto.so.3; do
    [ -f "$SSL_LIB/$lib" ] &&
    readelf -h "$SSL_LIB/$lib" 2>/dev/null | grep -q 'Class:.*ELF32' &&
    readelf -h "$SSL_LIB/$lib" 2>/dev/null | grep -q 'Machine:.*ARM' || {
        echo "Invalid ARM OpenSSL runtime: $SSL_LIB/$lib" >&2; exit 1;
    }
done
# Cross-linking requires the unversioned developer symlinks too.
[ -f "$SSL_LIB/libssl.so" -o -L "$SSL_LIB/libssl.so" ] || {
    echo "Missing ARM libssl.so link in $SSL_LIB" >&2;exit 1;
}
[ -f "$SSL_LIB/libcrypto.so" -o -L "$SSL_LIB/libcrypto.so" ] || {
    echo "Missing ARM libcrypto.so link in $SSL_LIB" >&2;exit 1;
}
mkdir -p "$REDAMP_FFMPEG_BUILD" "$ROOT/build/fftools"
REDAMP_FFMPEG_BUILD=$(CDPATH= cd -- "$REDAMP_FFMPEG_BUILD" && pwd -P)
WRAP="$REDAMP_FFMPEG_BUILD/cross-tools"
mkdir -p "$WRAP"
for tool in gcc ar ranlib; do
    path="$WRAP/arm-linux-gnueabihf-$tool"
    if [ "$tool" = gcc ]; then
        printf '#!/bin/sh\nexec "%s" cc -target "%s" "$@"\n' "$ZIG_BIN" "$ZIG_TARGET" > "$path"
    else
        printf '#!/bin/sh\nexec "%s" %s "$@"\n' "$ZIG_BIN" "$tool" > "$path"
    fi
    chmod +x "$path"
done
printf '#!/bin/sh\nexec nm "$@"\n' > "$WRAP/arm-linux-gnueabihf-nm"
chmod +x "$WRAP/arm-linux-gnueabihf-nm"
# MiniCinema may have configured its FFmpeg tree *in place*. FFmpeg 4.4
# refuses an out-of-tree configure when that source contains config.h.
# Prepare a private, clean *source* copy and build out of tree from it.
# Never run make clean/distclean, or remove files, in FFMPEG_SRC itself.
UPSTREAM_FFMPEG_SRC=$FFMPEG_SRC
PRIVATE_FFMPEG_SRC="$REDAMP_FFMPEG_BUILD/private-source"
PRIVATE_STAMP="$PRIVATE_FFMPEG_SRC/.redamp-upstream-path"
if [ ! -f "$PRIVATE_STAMP" ] ||
   [ "$(cat "$PRIVATE_STAMP")" != "$UPSTREAM_FFMPEG_SRC" ]; then
    echo "Preparing private FFmpeg 4.4.8 source (MiniCinema source unchanged)..."
    TMP_SOURCE="$REDAMP_FFMPEG_BUILD/.private-source-$$"
    rm -rf "$TMP_SOURCE"
    mkdir -p "$TMP_SOURCE"
    # Copy on the same filesystem; generated artifacts are removed only from
    # the private copy. This is performed once and cached across make runs.
    cp -a "$UPSTREAM_FFMPEG_SRC/." "$TMP_SOURCE/"
    rm -f "$TMP_SOURCE/config.h" "$TMP_SOURCE/config.asm" \
          "$TMP_SOURCE/config.mak" "$TMP_SOURCE/config.log" \
          "$TMP_SOURCE/ffbuild/config.mak" \
          "$TMP_SOURCE/ffbuild/config.log" \
          "$TMP_SOURCE/ffbuild/config.fate" \
          "$TMP_SOURCE/ffbuild/.config" \
          "$TMP_SOURCE/libavutil/avconfig.h"
    # Stale objects/libraries from MiniCinema must never be reused by RedAmp.
    find "$TMP_SOURCE" -type f \
        \( -name '*.o' -o -name '*.d' -o -name '*.a' \
           -o -name '*.so' -o -name '*.so.*' \) -delete
    [ -f "$TMP_SOURCE/configure" ] && [ ! -e "$TMP_SOURCE/config.h" ] || {
        echo 'Unable to prepare clean private FFmpeg source.' >&2
        rm -rf "$TMP_SOURCE"
        exit 1
    }
    printf '%s\n' "$UPSTREAM_FFMPEG_SRC" > "$TMP_SOURCE/.redamp-upstream-path"
    rm -rf "$PRIVATE_FFMPEG_SRC"
    mv "$TMP_SOURCE" "$PRIVATE_FFMPEG_SRC"
fi
[ ! -e "$PRIVATE_FFMPEG_SRC/config.h" ] || {
    echo 'Private FFmpeg source contains config.h; remove the cached private-source directory and retry.' >&2
    exit 1
}
FFMPEG_SRC=$PRIVATE_FFMPEG_SRC
# Protect HLS playlist, segment, and AES-key HTTPS requests. The cached
# FFmpeg source copy is private to RedAmp; never modify MiniCinema's tree.
python3 "$ROOT/tools/patch_ffmpeg_hls.py" "$FFMPEG_SRC/libavformat/hls.c"
# The generated configuration and objects still live separately in
# REDAMP_FFMPEG_BUILD. Using the private source avoids MiniCinema conflicts.
cd "$REDAMP_FFMPEG_BUILD"
"$FFMPEG_SRC/configure" \
    --enable-cross-compile --cross-prefix="$WRAP/arm-linux-gnueabihf-"     --pkg-config=false \
    --target-os=linux --arch=arm --cpu=cortex_a7 --enable-neon \
    --disable-runtime-cpudetect --disable-autodetect --disable-everything \
    --disable-programs --disable-doc --disable-debug --disable-stripping \
    --disable-static --enable-shared --enable-pic --build-suffix=_redamp \
    --disable-avdevice --disable-avfilter --disable-swscale --disable-postproc \
    --enable-avutil --enable-avcodec --enable-avformat --enable-swresample \
    --enable-pthreads --enable-network --enable-openssl --enable-version3 \
    --enable-decoder=aac,aac_fixed,aac_latm,alac,mp3,mp3float \
    --enable-parser=aac,aac_latm,mpegaudio \
    --enable-demuxer=hls,mpegts,aac,mov,mp3,ogg \
    --enable-protocol=file,http,https,tls,tcp,crypto \
    --enable-bsf=aac_adtstoasc \
    --extra-cflags="-Os -fPIC -DOPENSSL_API_COMPAT=0x10100000L -I$OPENSSL_ARM_PREFIX/include" \
    --extra-ldflags="-L$SSL_LIB" \
    --extra-libs="-lssl -lcrypto -lm -ldl -lpthread"
# Reject accidental feature loss or a license configuration drift.
for flag in CONFIG_HLS_DEMUXER CONFIG_AAC_DEMUXER CONFIG_MOV_DEMUXER \
            CONFIG_MPEGTS_DEMUXER CONFIG_AAC_DECODER CONFIG_ALAC_DECODER \
            CONFIG_HTTPS_PROTOCOL CONFIG_TLS_PROTOCOL CONFIG_OPENSSL \
            CONFIG_SWRESAMPLE CONFIG_VERSION3; do
    grep -Eq "^#define ${flag}[[:space:]]+1$" config.h || {
        echo "Required FFmpeg feature missing after configure: $flag" >&2; exit 1;
    }
done
for flag in CONFIG_GPL CONFIG_NONFREE; do
    grep -Eq "^#define ${flag}[[:space:]]+0$" config.h || {
        echo "Unsafe FFmpeg license setting: $flag" >&2; exit 1;
    }
done
# FFmpeg 4.4's configure probe may detect sysctl on glibc without its
# removed sys/sysctl.h header. MiniCinema requires the same workaround.
if grep -Eq '^#define HAVE_SYSCTL[[:space:]]+1$' config.h; then
    sed -i 's/^#define HAVE_SYSCTL[[:space:]]\+1$/#define HAVE_SYSCTL 0/' config.h
fi
make -j"$JOBS"
# Compile bridge using exact configured FFmpeg 4.4.8 source+generated headers.
# It links against the suffix-named libraries; the main RedAmp does not.
CC="$WRAP/arm-linux-gnueabihf-gcc"
"$CC" -std=gnu11 -Os -g0 -fPIC -shared -Wall -Wextra \
    -Wno-deprecated-declarations \
    -I"$ROOT" -I"$REDAMP_FFMPEG_BUILD" -I"$FFMPEG_SRC" \
    "$ROOT/ffbridge.c" "$ROOT/adts_duration.c" -o "$REDAMP_FFMPEG_BUILD/libredamp_ffaudio.so" \
    -L"$REDAMP_FFMPEG_BUILD/libavformat" \
    -L"$REDAMP_FFMPEG_BUILD/libavcodec" \
    -L"$REDAMP_FFMPEG_BUILD/libavutil" \
    -L"$REDAMP_FFMPEG_BUILD/libswresample" \
    -Wl,-rpath,'$ORIGIN' -Wl,-z,defs \
    -lavformat_redamp -lavcodec_redamp -lswresample_redamp -lavutil_redamp -lm -lpthread
stage="$REDAMP_FFMPEG_BUILD/staging"
rm -rf "$stage" && mkdir -p "$stage"
for spec in 'avutil 56' 'avcodec 58' 'avformat 58' 'swresample 3'; do
    set -- $spec
    lib="lib${1}_redamp.so.${2}"
    f="$REDAMP_FFMPEG_BUILD/lib${1}/$lib"
    [ -f "$f" ] || { echo "Missing built $f" >&2; exit 1; }
    cp -L "$f" "$stage/$lib"
done
cp "$REDAMP_FFMPEG_BUILD/libredamp_ffaudio.so" "$stage/libredamp_ffaudio.so"
# Include the ARM TLS libraries used by FFmpeg (self-contained for OnionOS).
for lib in libssl.so.3 libcrypto.so.3; do
    [ -f "$SSL_LIB/$lib" ] || {
        echo "Missing ARM TLS runtime $SSL_LIB/$lib" >&2; exit 1;
    }
    cp -L "$SSL_LIB/$lib" "$stage/$lib"
done
# Verify ARM ABI and private dependency names before copying to the app.
"$ROOT/tools/verify_ffmpeg.sh" "$stage"
# Bundle a known CA store for HTTPS. Fail closed rather than ship an
# audio backend that accidentally disables TLS verification.
: "${REDAMP_CA_BUNDLE:=/etc/ssl/certs/ca-certificates.crt}"
[ -s "$REDAMP_CA_BUNDLE" ] || {
    echo "Missing CA bundle $REDAMP_CA_BUNDLE (set REDAMP_CA_BUNDLE)" >&2; exit 1;
}
# Keep output beside the RedAmp source, like MiniCinema's ARM build.
# The maintainer copies ./lib/*.so* and ./cacert.pem to App/RedAmp later.
mkdir -p "$ROOT/lib"
for lib in "$stage"/*.so*; do cp -f "$lib" "$ROOT/lib/$(basename "$lib")"; done
cp "$REDAMP_CA_BUNDLE" "$ROOT/cacert.pem"
printf 'FFmpeg 4.4.8 private ARM audio libraries: %s/lib\n' "$ROOT"
printf 'CA bundle: %s/cacert.pem\n' "$ROOT"
printf 'For installation, copy ./lib/*.so* into App/RedAmp/lib and cacert.pem beside launch.sh.\n'
