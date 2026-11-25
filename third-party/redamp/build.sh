#!/bin/sh
# RedAmp v1.1.15 source-only ARM build. MiniCinema ARM GCC, or Zig fallback.
# Outputs: ./redamp, ./libpadsp_wrap.so, ./lib/ (ARM runtime dependencies).
# No App directory needed. FFmpeg libraries are built by make ffmpeg-audio.
set -eu
cd "$(dirname "$0")"
mkdir -p build

FILES='main.c netclock.c core.c stayawake.c audio.c decode.c meta.c library.c album_art.c covers.c skin.c zip.c font.c vis.c collections.c utf8_text.c playlist.c gamemusic.c opus_ogg.c radio.c ffaudio.c third_party/mini2d.c'
WARN='-Wall -Wno-format-truncation -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable'

case "${1:-host}" in
    host)
        "${CC:-gcc}" -O2 -g -DM2D_HOST $WARN -I. -Ithird_party $FILES \
            -o build/redamp-host -lz -lm -lpthread -ldl -lcurl
        "${CC:-gcc}" -O2 -shared -fPIC $WARN -I. padsp_wrap.c \
            -o build/libpadsp_wrap.so -ldl
        echo 'Host build complete: src/build/redamp-host (not a Miyoo binary)'
        exit 0
        ;;
    miyoo) ;;
    *) echo 'Usage: build.sh {host|miyoo [all|player|wrapper]}' >&2; exit 2 ;;
esac

part="${2:-all}"
case "$part" in all|player|wrapper) ;; *) echo 'Expected all, player or wrapper' >&2; exit 2 ;; esac

# MiniCinema's browser uses this GCC. Its engine uses Zig 0.14.1.
: "${ZIG_BIN:=/root/workspace/zig-0.14.1/zig}"
: "${ZIG_TARGET:=arm-linux-gnueabihf.2.28}"
if [ -n "${CROSS_CC:-}" ]; then
    if [ ! -x "$CROSS_CC" ] && ! command -v "$CROSS_CC" >/dev/null 2>&1; then
        echo "Specified CROSS_CC not found: $CROSS_CC" >&2; exit 1
    fi
    ARM_CC=$CROSS_CC
    USE_ZIG=0
elif [ -x /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc ]; then
    ARM_CC=/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc
    USE_ZIG=0
else
    if [ -x "$ZIG_BIN" ]; then
        :
    elif command -v "$ZIG_BIN" >/dev/null 2>&1; then
        ZIG_BIN=$(command -v "$ZIG_BIN")
    elif [ "$ZIG_BIN" = /root/workspace/zig-0.14.1/zig ] && command -v zig >/dev/null 2>&1; then
        ZIG_BIN=$(command -v zig)
    else
        echo 'Neither MiniCinema ARM GCC nor Zig was found.' >&2
        echo 'Expected /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc' >&2
        echo 'or /root/workspace/zig-0.14.1/zig; override CROSS_CC or ZIG_BIN.' >&2
        exit 1
    fi
    USE_ZIG=1
fi
arm_cc() {
    if [ "$USE_ZIG" -eq 1 ]; then
        "$ZIG_BIN" cc -target "$ZIG_TARGET" "$@"
    else
        "$ARM_CC" "$@"
    fi
}
if [ "$USE_ZIG" -eq 1 ]; then
    echo "ARM compiler: $ZIG_BIN cc -target $ZIG_TARGET"
else
    echo "ARM compiler: $ARM_CC"
fi

if [ "$part" != wrapper ]; then
    # Unlike MiniCinema's standalone source browser, RedAmp uses MI_SYS and
    # MI_GFX headers and libraries; those cannot be provided by Zig alone.
    sdk=${MI_SDK:-}
    if [ -z "$sdk" ]; then
        for candidate in \
            /root/workspace/miyoo-sdl2/mini \
            /root/workspace/miyoo-sdl2/sdl2/mini \
            /root/workspace/sdl2/mini \
            /root/workspace/miyoo-sdk/mini \
            ../sdl2/mini; do
            if { [ -f "$candidate/inc/mi_gfx.h" ] || [ -f "$candidate/include/mi_gfx.h" ]; } \
                && [ -d "$candidate/lib" ]; then
                sdk=$candidate
                break
            fi
        done
    fi
    if [ -n "$sdk" ] && [ -f "$sdk/inc/mi_gfx.h" ]; then
        MI_INC=$sdk/inc
    elif [ -n "$sdk" ] && [ -f "$sdk/include/mi_gfx.h" ]; then
        MI_INC=$sdk/include
    else
        echo 'SigmaStar MI_GFX/MI_SYS SDK not found.' >&2
        echo 'Set MI_SDK=/path/to/sdk (with inc/mi_gfx.h and lib/libmi_gfx.so).' >&2
        echo 'MiniCinema does not use this SDK; RedAmp does.' >&2
        exit 1
    fi
    MI_LIB=${MI_LIB_DIR:-$sdk/lib}
    for lib in mi_gfx mi_sys mi_common; do
        if [ ! -f "$MI_LIB/lib$lib.so" ] && [ ! -f "$MI_LIB/lib$lib.a" ]; then
            echo "Missing SDK library $MI_LIB/lib$lib.so (or .a)" >&2
            echo 'Set MI_SDK or MI_LIB_DIR to your ARM SigmaStar libraries.' >&2
            exit 1
        fi
    done

    # Use the ARM cross-toolchain/sysroot when it already contains libcurl and
    # zlib. REDAMP_ARM_LIBS remains an optional override, not a requirement.
    # libopus is dlopen()'d at runtime and is NOT a compile/link dependency.
    # All detected runtime libraries must be 32-bit ARM ELF; never copy VM libs.
    arm_sysroot=$(arm_cc -print-sysroot 2>/dev/null || :)
    case "$arm_sysroot" in /|.) arm_sysroot=;; esac
    cross_root=/opt/miyoomini-toolchain/usr
    check_arm_elf() {
        [ -f "$1" ] || return 1
        # readelf checks the contents, not the filename / potentially bogus .so link.
        readelf -h "$1" 2>/dev/null | grep -Eq 'Class:[[:space:]]+ELF32' &&
        readelf -h "$1" 2>/dev/null | grep -Eq 'Machine:[[:space:]]+ARM'
    }
    arm_library() {
        name=$1
        # Explicit override comes first, followed by common MiniCinema/Miyoo
        # toolchain locations and standard Debian cross/multiarch directories.
        for dir in \
            "${REDAMP_ARM_LIBS:-}" \
            "$cross_root/lib/arm-linux-gnueabihf" \
            "$cross_root/arm-linux-gnueabihf/lib" \
            "$cross_root/lib" \
            "$cross_root/usr/lib/arm-linux-gnueabihf" \
            "$cross_root/usr/lib" \
            "$arm_sysroot/usr/lib/arm-linux-gnueabihf" \
            "$arm_sysroot/lib/arm-linux-gnueabihf" \
            "$arm_sysroot/usr/lib" \
            "$arm_sysroot/lib" \
            /usr/arm-linux-gnueabihf/lib \
            /usr/lib/arm-linux-gnueabihf \
            /lib/arm-linux-gnueabihf; do
            [ -n "$dir" ] && [ -d "$dir" ] || continue
            if check_arm_elf "$dir/$name"; then
                (CDPATH= cd -- "$dir" && printf '%s/%s\n' "$(pwd -P)" "$name")
                return 0
            fi
        done
        # GCC's -print-file-name also searches its multilib directories.
        guess=$(arm_cc -print-file-name="$name" 2>/dev/null || :)
        if [ "$guess" != "$name" ] && check_arm_elf "$guess"; then
            printf '%s\n' "$guess"
            return 0
        fi
        return 1
    }
    arm_header_dir() {
        header=$1
        override=$2
        if [ -n "$override" ]; then
            [ -f "$override/$header" ] || return 1
            printf '%s\n' "$override"
            return 0
        fi
        # First try the target toolchain's default header search path.
        if printf '#include <%s>\n' "$header" | arm_cc -E -x c - -o /dev/null >/dev/null 2>&1; then
            printf '.\n'
            return 0
        fi
        for dir in \
            "${CURL_ARM_PREFIX:-}/include" \
            "${ZLIB_ARM_PREFIX:-}/include" \
            "${REDAMP_ARM_LIBS:-}/../include" \
            "$cross_root/include" \
            "$cross_root/usr/include" \
            "$cross_root/usr/include/arm-linux-gnueabihf" \
            "$arm_sysroot/usr/include" \
            "$arm_sysroot/usr/include/arm-linux-gnueabihf" \
            /usr/arm-linux-gnueabihf/include \
            /usr/include/arm-linux-gnueabihf; do
            [ -d "$dir" ] && [ -f "$dir/$header" ] || continue
            printf '%s\n' "$dir"
            return 0
        done
        return 1
    }
    # Prefer our own auditable ABI-pinned libraries over anything in the VM
    # sysroot. They are also staged by make arm-deps.
    private_prefix="$(pwd -P)/build/arm-deps/prefix"
    if [ -f "$private_prefix/lib/libcurl.so.4" ] && \
       [ -f "$private_prefix/lib/libz.so.1" ]; then
        ARM_CURL="$private_prefix/lib/libcurl.so.4"
        ARM_ZLIB="$private_prefix/lib/libz.so.1"
        ARM_OPUS="$private_prefix/lib/libopus.so.0"
    else
        ARM_CURL=$(arm_library libcurl.so.4 || :)
        ARM_ZLIB=$(arm_library libz.so.1 || :)
        ARM_OPUS=$(arm_library libopus.so.0 || :)
    fi
    if [ -z "$ARM_CURL" ] || [ -z "$ARM_ZLIB" ]; then
        echo 'Missing ARM curl/zlib in toolchain; building private dependencies from pinned source...'
        ./tools/build_arm_deps.sh
        ARM_CURL="$private_prefix/lib/libcurl.so.4"
        ARM_ZLIB="$private_prefix/lib/libz.so.1"
        ARM_OPUS="$private_prefix/lib/libopus.so.0"
    fi
    if [ -z "$ARM_CURL" ] || [ -z "$ARM_ZLIB" ] || ! check_arm_elf "$ARM_CURL" || ! check_arm_elf "$ARM_ZLIB"; then
        echo 'Missing or invalid ARM cross-build dependency:' >&2
        [ -n "$ARM_CURL" ] || echo '  libcurl.so.4 (ARM hard-float)' >&2
        [ -n "$ARM_ZLIB" ] || echo '  libz.so.1 (ARM hard-float)' >&2
        echo 'Searched MiniCinema toolchain/sysroot and standard ARM multiarch paths.' >&2
        echo 'Install/provide the ARM -dev/runtime libraries or set REDAMP_ARM_LIBS.' >&2
        echo 'A cross compiler alone does not provide libcurl (and may not provide zlib).' >&2
        echo 'Use make wrapper independently if only the game-audio wrapper is needed.' >&2
        exit 1
    fi
    echo "ARM libcurl: $ARM_CURL"
    echo "ARM zlib:   $ARM_ZLIB"
    [ -z "$ARM_OPUS" ] || echo "ARM libopus: $ARM_OPUS"
    if [ -f "$private_prefix/include/curl/curl.h" ]; then
        CURL_INC="$private_prefix/include"
    else
        CURL_INC=$(arm_header_dir curl/curl.h "${CURL_ARM_INCLUDE:-}" || :)
    fi
    if [ -f "$private_prefix/include/zlib.h" ]; then
        ZLIB_INC="$private_prefix/include"
    else
        ZLIB_INC=$(arm_header_dir zlib.h "${ZLIB_ARM_INCLUDE:-}" || :)
    fi
    if [ -z "$CURL_INC" ] || [ -z "$ZLIB_INC" ]; then
        echo 'Missing cross-development headers:' >&2
        [ -n "$CURL_INC" ] || echo '  curl/curl.h (set CURL_ARM_INCLUDE)' >&2
        [ -n "$ZLIB_INC" ] || echo '  zlib.h (set ZLIB_ARM_INCLUDE)' >&2
        exit 1
    fi
    # Only create linker-name symlinks in the build directory. The toolchain
    # sysroot and source directories remain untouched.
    mkdir -p build/arm-link
    ln -sf "$ARM_CURL" build/arm-link/libcurl.so
    ln -sf "$ARM_ZLIB" build/arm-link/libz.so
    # DT_NEEDED of libcurl may include additional TLS or compression libraries;
    # their presence must be audited before distributing the final App package.

    # -O2 is adequate for a default build; CPU-tuning can be evaluated after
    # a working ARM binary is tested on the actual Miyoo hardware.
    arm_cc -O2 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard \
        -ffast-math -fno-finite-math-only $WARN \
        -I. -Ithird_party -I"$MI_INC" -I"$CURL_INC" -I"$ZLIB_INC" \
        $FILES -o build/redamp.arm.new \
        -L"$MI_LIB" -Lbuild/arm-link \
        -Wl,-rpath-link,"$(dirname "$ARM_CURL")" \
        -Wl,-rpath-link,"$(dirname "$ARM_ZLIB")" \
        -Wl,-rpath-link,"$private_prefix/lib" -Wl,--allow-shlib-undefined \
        -Wl,-rpath,'$ORIGIN/lib' \
        -lmi_gfx -lmi_sys -lmi_common -lcurl -lz -lm -lpthread -ldl

    # Stage only verified ARM runtimes, after a successful link. libopus is
    # optional (used via dlopen), and absence does not block compilation.
    mkdir -p build/arm-stage
    cp -L "$ARM_CURL" build/arm-stage/libcurl.so.4
    cp -L "$ARM_ZLIB" build/arm-stage/libz.so.1
    # The private curl build links to our own ARM OpenSSL 3. Bundle those
    # dependencies instead of assuming RedOnion exposes compatible versions.
    for name in libssl.so.3 libcrypto.so.3; do
        if [ -f "$private_prefix/lib/$name" ]; then
            check_arm_elf "$private_prefix/lib/$name" || {
                echo "Invalid ARM private TLS dependency: $name" >&2; exit 1;
            }
            cp -L "$private_prefix/lib/$name" "build/arm-stage/$name"
        fi
    done
    if [ -n "$ARM_OPUS" ] && check_arm_elf "$ARM_OPUS"; then
        cp -L "$ARM_OPUS" build/arm-stage/libopus.so.0
    else
        echo 'NOTE: no ARM libopus.so.0 found; Opus needs that runtime library on device.' >&2
    fi
    if [ -n "${REDAMP_ARM_EXTRA_LIBS:-}" ]; then
        for dep in $REDAMP_ARM_EXTRA_LIBS; do
            case "$dep" in */*|.*) echo "Invalid library name: $dep" >&2; exit 1 ;; esac
            extra=$(arm_library "$dep" || :)
            [ -n "$extra" ] || { echo "Missing ARM runtime $dep" >&2; exit 1; }
            cp -L "$extra" "build/arm-stage/$dep"
        done
    fi
    # Do not use host strip; strip only with a matching ARM-aware tool.
    if [ -n "${CROSS_STRIP:-}" ]; then
        "$CROSS_STRIP" build/redamp.arm.new
    elif [ -x /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-strip ]; then
        /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-strip build/redamp.arm.new
    elif command -v llvm-strip >/dev/null 2>&1; then
        llvm-strip build/redamp.arm.new
    fi
    mv -f build/redamp.arm.new redamp
    mkdir -p lib
    for dep in build/arm-stage/*.so*; do
        cp -f "$dep" "lib/$(basename "$dep")"
    done
    echo 'ARM player built: ./redamp (libraries in ./lib/)'
    echo 'Audit libcurl DT_NEEDED: other ARM runtimes may need to be staged.'
    readelf -d "$ARM_CURL" | grep NEEDED || :
fi

if [ "$part" != player ]; then
    arm_cc -O2 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard \
        -fPIC -shared $WARN -I. padsp_wrap.c \
        -o build/libpadsp_wrap.arm.new -ldl
    if [ -n "${CROSS_STRIP:-}" ]; then
        "$CROSS_STRIP" build/libpadsp_wrap.arm.new
    elif [ -x /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-strip ]; then
        /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-strip build/libpadsp_wrap.arm.new
    elif command -v llvm-strip >/dev/null 2>&1; then
        llvm-strip build/libpadsp_wrap.arm.new
    fi
    mv -f build/libpadsp_wrap.arm.new libpadsp_wrap.so
    echo 'ARM game-music wrapper built: ./libpadsp_wrap.so'
fi
