#!/bin/sh
# RedAmp 1.0.7 dependency bootstrap. Build PRIVATE 32-bit ARM hard-float
# zlib, OpenSSL, curl and Opus from pinned source releases.
# No sudo, no dpkg multiarch, and no App/ directory required on the build VM.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
: "${REDAMP_DEPS_ROOT:=$ROOT/build/arm-deps}"
: "${REDAMP_SOURCE_CACHE:=$REDAMP_DEPS_ROOT/dist}"
: "${ZIG_BIN:=/root/workspace/zig-0.14.1/zig}"
: "${ZIG_TARGET:=arm-linux-gnueabihf.2.28}"
: "${JOBS:=4}"
PREFIX=$REDAMP_DEPS_ROOT/prefix
SRCDIR=$REDAMP_DEPS_ROOT/sources
WRAP=$REDAMP_DEPS_ROOT/cross-tools
mkdir -p "$PREFIX" "$SRCDIR" "$REDAMP_SOURCE_CACHE" "$WRAP"
PREFIX=$(CDPATH= cd -- "$PREFIX" && pwd -P)
SRCDIR=$(CDPATH= cd -- "$SRCDIR" && pwd -P)
REDAMP_SOURCE_CACHE=$(CDPATH= cd -- "$REDAMP_SOURCE_CACHE" && pwd -P)
WRAP=$(CDPATH= cd -- "$WRAP" && pwd -P)

# Prefer the exact ARM GCC used by RedAmp's player and wrapper.  The
# previous dependency bootstrap used Zig unconditionally; with zlib 1.3.1,
# that failed the shared-link configure probe and silently built libz.a only.
# Zig remains supported when the MiniCinema/Miyoo GCC is not installed.
# REDAMP_DEPS_CC (or CROSS_CC) selects a custom ARM GCC executable.
if [ -n "${REDAMP_DEPS_CC:-}" ]; then
    DEPS_CC=$REDAMP_DEPS_CC
elif [ -n "${CROSS_CC:-}" ]; then
    DEPS_CC=$CROSS_CC
elif [ -x /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc ]; then
    DEPS_CC=/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc
else
    DEPS_CC=
fi
if [ -n "$DEPS_CC" ]; then
    case "$DEPS_CC" in
        /*) [ -x "$DEPS_CC" ] || { echo "ARM compiler not executable: $DEPS_CC" >&2; exit 1; } ;;
        *)  DEPS_CC=$(command -v "$DEPS_CC" 2>/dev/null) || {
                echo "ARM compiler not found: $DEPS_CC" >&2; exit 1;
            } ;;
    esac
    CC=$DEPS_CC
    # Cross GCC toolchain supplies matching binutils.  Do not use host ar.
    bin_dir=$(dirname "$CC")
    gcc_base=$(basename "$CC")
    case "$gcc_base" in
        *-gcc) tool_prefix=${gcc_base%gcc} ;;
        *) echo "Expected an ARM cross GCC named *-gcc: $CC" >&2; exit 1 ;;
    esac
    AR="$bin_dir/${tool_prefix}ar"
    RANLIB="$bin_dir/${tool_prefix}ranlib"
    [ -x "$AR" ] && [ -x "$RANLIB" ] || {
        echo "Missing matching ARM ar/ranlib beside $CC" >&2; exit 1;
    }
    echo "ARM dependency compiler (GCC): $CC"
else
    if [ -x "$ZIG_BIN" ]; then
        :
    elif command -v "$ZIG_BIN" >/dev/null 2>&1; then
        ZIG_BIN=$(command -v "$ZIG_BIN")
    elif [ "$ZIG_BIN" = /root/workspace/zig-0.14.1/zig ] && command -v zig >/dev/null 2>&1; then
        ZIG_BIN=$(command -v zig)
    else
        echo "Missing MiniCinema ARM GCC and Zig cross compiler: $ZIG_BIN" >&2
        echo 'Set REDAMP_DEPS_CC, CROSS_CC, or ZIG_BIN.' >&2
        exit 1
    fi
    ZIG_BIN=$(readlink -f "$ZIG_BIN")
    for tool in gcc ar ranlib; do
        f="$WRAP/arm-linux-gnueabihf-$tool"
        case "$tool" in
            gcc) printf '#!/bin/sh\nexec "%s" cc -target "%s" "$@"\n' "$ZIG_BIN" "$ZIG_TARGET" > "$f" ;;
            *)   printf '#!/bin/sh\nexec "%s" %s "$@"\n' "$ZIG_BIN" "$tool" > "$f" ;;
        esac
        chmod +x "$f"
    done
    CC="$WRAP/arm-linux-gnueabihf-gcc"
    AR="$WRAP/arm-linux-gnueabihf-ar"
    RANLIB="$WRAP/arm-linux-gnueabihf-ranlib"
    echo "ARM dependency compiler (Zig): $ZIG_BIN ($ZIG_TARGET)"
fi
export CC AR RANLIB
check_arm() {
    [ -f "$1" ] &&
    readelf -h "$1" 2>/dev/null | grep -Eq 'Class:[[:space:]]+ELF32' &&
    readelf -h "$1" 2>/dev/null | grep -Eq 'Machine:[[:space:]]+ARM'
}
# Each upstream tarball must match its pinned SHA256. To build offline, place
# these same tarballs under REDAMP_SOURCE_CACHE before running make.
fetch() {
    fname=$1 url=$2 sha=$3
    fallback_url=${4:-}
    path="$REDAMP_SOURCE_CACHE/$fname"
    if [ -f "$path" ] && ! printf '%s  %s\n' "$sha" "$path" | sha256sum -c - >/dev/null 2>&1; then
        echo "Incorrect source checksum: $path" >&2; exit 1
    fi
    if [ ! -f "$path" ]; then
        echo "Fetching $fname (one-time)..." >&2
        tmp="$path.tmp.$$"
        trap 'rm -f "$tmp"' 0 HUP INT TERM
        fetched=0
        # A source mirror can have a broken or unavailable TLS chain. Use
        # an alternate HTTPS source, never --insecure/--no-check-certificate.
        # The pinned upstream SHA256 is verified regardless of download URL.
        for source_url in "$url" "$fallback_url"; do
            [ -n "$source_url" ] || continue
            rm -f "$tmp"
            echo "Download source: $source_url" >&2
            if command -v curl >/dev/null 2>&1; then
                if curl --fail --location --retry 3 --connect-timeout 20 --output "$tmp" "$source_url"; then
                    fetched=1
                fi
            elif command -v wget >/dev/null 2>&1; then
                if wget --tries=3 --timeout=20 -O "$tmp" "$source_url"; then
                    fetched=1
                fi
            else
                echo "Neither curl nor wget is available to download $fname" >&2
                exit 1
            fi
            [ "$fetched" -eq 1 ] && break
            echo "Download source failed; trying another verified source if available." >&2
        done
        if [ "$fetched" -ne 1 ]; then
            echo "Download failed for $fname (all HTTPS sources)." >&2
            echo "You can also put the checksum-verified $fname into $REDAMP_SOURCE_CACHE yourself." >&2
            exit 1
        fi
        printf '%s  %s\n' "$sha" "$tmp" | sha256sum -c - >/dev/null || {
            echo "Source checksum mismatch for $fname" >&2; exit 1;
        }
        mv "$tmp" "$path"
        trap - 0 HUP INT TERM
    fi
    printf '%s\n' "$path"
}
unpack() {
    name=$1 tarball=$2
    if [ ! -f "$SRCDIR/$name/.redamp-unpacked" ]; then
        rm -rf "$SRCDIR/$name"
        # Every archive was SHA256 verified, and extracts into its versioned dir.
        tar -xf "$tarball" -C "$SRCDIR"
        [ -d "$SRCDIR/$name" ] || { echo "No $name in $tarball" >&2; exit 1; }
        : > "$SRCDIR/$name/.redamp-unpacked"
    fi
}
# All source hashes are pinned, including the ones used by FFmpeg's TLS build.
ZLIB_NAME=zlib-1.3.1
ZLIB_HASH=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
OPENSSL_NAME=openssl-3.5.9
OPENSSL_HASH=603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a
CURL_NAME=curl-8.23.0
CURL_HASH=8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5
OPUS_NAME=opus-1.5.2
OPUS_HASH=65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1

if ! check_arm "$PREFIX/lib/libz.so.1"; then
    z=$(fetch "$ZLIB_NAME.tar.gz" "https://zlib.net/fossils/$ZLIB_NAME.tar.gz" "$ZLIB_HASH")
    unpack "$ZLIB_NAME" "$z"
    echo 'Building private ARM shared zlib...'
    # zlib's configure detects a failed shared link, then *succeeds* with a
    # static-only Makefile.  Force the selected cross compiler as LDSHARED,
    # verify its shared-link probe, and reject the static fallback immediately.
    # Keep the zlib version script so downstream libraries see the normal ABI.
    zdir="$SRCDIR/$ZLIB_NAME"
    if [ -f "$zdir/Makefile" ]; then
        make -C "$zdir" clean >/dev/null
    fi
    # Probe with exactly the linker/compiler pair that zlib will use.  This
    # catches missing cross linker/libc and reveals the useful failure text.
    probe="$REDAMP_DEPS_ROOT/zlib-shared-probe"
    mkdir -p "$probe"
    printf 'int redamp_zlib_probe(void) { return 42; }\n' > "$probe/probe.c"
    if ! "$CC" -Os -fPIC -c "$probe/probe.c" -o "$probe/probe.o" >"$probe/build.log" 2>&1 ||
       ! "$CC" -shared -Wl,-soname,libz.so.1 -o "$probe/libz.so.1" "$probe/probe.o" >>"$probe/build.log" 2>&1 ||
       ! check_arm "$probe/libz.so.1"; then
        echo "ARM cross compiler cannot build a 32-bit ARM shared library: $CC" >&2
        cat "$probe/build.log" >&2
        echo 'Inspect compiler/sysroot setup; zlib cannot be built shared yet.' >&2
        exit 1
    fi
    rm -rf "$probe"
    (cd "$zdir" &&
        CC="$CC" AR="$AR" RANLIB="$RANLIB" \
        LDSHARED="$CC -shared -Wl,-soname,libz.so.1,--version-script,zlib.map" \
        CFLAGS='-Os -fPIC' \
        ./configure --prefix="$PREFIX" --shared &&
        if ! grep -Eq '^SHAREDLIB[[:space:]]*=[[:space:]]*libz[.]so' Makefile; then
            echo 'zlib configure selected static-only instead of libz.so' >&2
            echo '---- zlib configure.log (last 60 lines) ----' >&2
            tail -n 60 configure.log >&2
            exit 1
        fi &&
        make -j"$JOBS" && make install)
fi
check_arm "$PREFIX/lib/libz.so.1" || {
    echo 'ARM zlib shared library missing or invalid after build.' >&2
    echo 'Look at build/arm-deps/sources/zlib-1.3.1/configure.log.' >&2
    exit 1
}

# OpenSSL prepends CROSS_COMPILE to CC, AR and RANLIB internally. Never
# pass a full compiler path in CC *and* a cross prefix: that creates paths
# such as arm-linux-gnueabihf-/opt/.../arm-linux-gnueabihf-gcc.
# The same path-based prefix works for both MiniCinema ARM GCC and our
# Zig-based ARM wrappers.
OPENSSL_CROSS_PREFIX=${CC%gcc}
[ "${OPENSSL_CROSS_PREFIX}gcc" = "$CC" ] || {
    echo "Cannot derive OpenSSL cross compiler prefix from $CC" >&2; exit 1;
}
for tool in gcc ar ranlib; do
    [ -x "${OPENSSL_CROSS_PREFIX}${tool}" ] || {
        echo "Missing OpenSSL cross tool: ${OPENSSL_CROSS_PREFIX}${tool}" >&2; exit 1;
    }
done

if ! check_arm "$PREFIX/lib/libssl.so.3" || ! check_arm "$PREFIX/lib/libcrypto.so.3"; then
    o=$(fetch "$OPENSSL_NAME.tar.gz" "https://www.openssl.org/source/$OPENSSL_NAME.tar.gz" "$OPENSSL_HASH")
    unpack "$OPENSSL_NAME" "$o"
    echo 'Building private ARM OpenSSL 3 (TLS for curl + FFmpeg)...'
    (
        cd "$SRCDIR/$OPENSSL_NAME"
        # Failed cross builds leave a configured Makefile behind. Reset the
        # generated configuration so the corrected compiler path is used.
        if [ -f Makefile ]; then
            make distclean >/dev/null 2>&1 || {
                echo 'OpenSSL distclean failed; cannot safely reuse previous build.' >&2
                exit 1
            }
        fi
        # Use exactly one toolchain prefix, not CC=/absolute/path + prefix.
        # POSIX sh unsets exported variables in this subshell only; curl,
        # zlib and Opus still receive their explicit cross compiler paths.
        unset CC AR RANLIB CROSS_COMPILE
        perl ./Configure linux-armv4 shared no-asm no-tests no-apps no-docs \
          --cross-compile-prefix="$OPENSSL_CROSS_PREFIX" \
          --prefix="$PREFIX" --libdir=lib --openssldir="$PREFIX/ssl" -Os
        make -j"$JOBS"
        make install_sw
    )
fi
check_arm "$PREFIX/lib/libssl.so.3" && check_arm "$PREFIX/lib/libcrypto.so.3" || {
    echo 'ARM OpenSSL shared build invalid' >&2; exit 1;
}

if ! check_arm "$PREFIX/lib/libcurl.so.4"; then
    c=$(fetch "$CURL_NAME.tar.xz" "https://curl.se/download/$CURL_NAME.tar.xz" "$CURL_HASH")
    unpack "$CURL_NAME" "$c"
    echo 'Building private ARM libcurl with verified HTTPS...'
    (cd "$SRCDIR/$CURL_NAME" &&
        PKG_CONFIG=false CC="$CC" AR="$AR" RANLIB="$RANLIB" \
        CPPFLAGS="-I$PREFIX/include" \
        LDFLAGS="-L$PREFIX/lib -Wl,-rpath-link,$PREFIX/lib" \
        CFLAGS='-Os -fPIC' \
        ./configure --host=arm-linux-gnueabihf \
          --prefix="$PREFIX" --libdir="$PREFIX/lib" \
          --enable-shared --disable-static --disable-curl --disable-manual \
          --with-openssl="$PREFIX" --with-zlib="$PREFIX" \
          --without-brotli --without-zstd --without-libpsl \
          --without-libidn2 --without-libssh2 --without-nghttp2 \
          --without-ngtcp2 --without-nghttp3 &&
        make -j"$JOBS" && make install)
fi
check_arm "$PREFIX/lib/libcurl.so.4" && [ -f "$PREFIX/include/curl/curl.h" ] || {
    echo 'ARM curl library or development headers missing after build' >&2; exit 1;
}

# Opus is dlopen()ed by the player: bundle it here so no external system
# libopus assumption leaks into device deployment.
if ! check_arm "$PREFIX/lib/libopus.so.0"; then
    # The Xiph downloads endpoint currently redirects to an OSUOSL mirror whose
    # CA chain can be untrusted on some build VMs. Prefer the official GitHub
    # release asset; use the GStreamer source mirror as an HTTPS fallback.
    p=$(fetch "$OPUS_NAME.tar.gz" \
        "https://github.com/xiph/opus/releases/download/v1.5.2/$OPUS_NAME.tar.gz" \
        "$OPUS_HASH" \
        "https://gstreamer.freedesktop.org/data/src/mirror/$OPUS_NAME.tar.gz")
    unpack "$OPUS_NAME" "$p"
    echo 'Building private ARM Opus...'
    (cd "$SRCDIR/$OPUS_NAME" &&
        CC="$CC" AR="$AR" RANLIB="$RANLIB" PKG_CONFIG=false \
        CFLAGS='-Os -fPIC' ./configure --host=arm-linux-gnueabihf \
        --prefix="$PREFIX" --enable-shared --disable-static &&
        make -j"$JOBS" && make install)
fi
check_arm "$PREFIX/lib/libopus.so.0" || { echo 'ARM Opus build invalid' >&2; exit 1; }

# Stage exact ARM runtime files for later copying to App/RedAmp/lib.
mkdir -p "$ROOT/lib"
for lib in libz.so.1 libssl.so.3 libcrypto.so.3 libcurl.so.4 libopus.so.0; do
    cp -L "$PREFIX/lib/$lib" "$ROOT/lib/$lib"
    check_arm "$ROOT/lib/$lib" || { echo "Staging failed: $lib" >&2; exit 1; }
done
printf '\nRedAmp private ARM dependencies ready: %s\n' "$PREFIX"
printf 'Staged: %s/lib/{libcurl.so.4,libz.so.1,libssl.so.3,libcrypto.so.3,libopus.so.0}\n' "$ROOT"
