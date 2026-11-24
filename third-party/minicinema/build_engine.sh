#!/bin/sh
# Cross-build MiniCinema's patched KidsPlay/FFmpeg 4.4.8 playback engine.
# Adapted from Veuks' src/kidsPlay/build_ffplay.sh (onion-kids-mode).
# Temporary compiler wrappers live outside the project package; FFmpeg
# fftools/ffplay.c is restored even if configuration or compilation fails.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
# Build-only tree: produce binaries here. Copy into App/MiniCinema yourself.
out="$root"
# Default paths for the RedOnion Linux VM; environment overrides remain supported.
: "${FFMPEG_SRC:=/root/workspace/ffmpeg-4.4.8}"
: "${SDL2_ROOT:=/root/workspace/miyoo-sdl2}"
: "${ZIG_BIN:=/root/workspace/zig-0.14.1/zig}"
FFMPEG_SRC=$(CDPATH= cd -- "$FFMPEG_SRC" && pwd -P)
SDL2_ROOT=$(CDPATH= cd -- "$SDL2_ROOT" && pwd -P)
# `command -v /missing/path` exits 127 in dash. With set -e it would
# terminate this script silently, leaving make's unhelpful "Error 127".
# Accept either an explicit executable path or a command on PATH, and use
# Zig from PATH when the default VM location is not populated.
zig_path="$ZIG_BIN"
if [ -x "$zig_path" ]; then
    ZIG_BIN="$zig_path"
elif command -v "$zig_path" >/dev/null 2>&1; then
    ZIG_BIN=$(command -v "$zig_path")
elif [ "$zig_path" = /root/workspace/zig-0.14.1/zig ] && command -v zig >/dev/null 2>&1; then
    ZIG_BIN=$(command -v zig)
else
    printf 'Zig compiler not found: %s\n' "$zig_path" >&2
    printf '%s\n' 'Install Zig or set ZIG_BIN=/full/path/to/zig before make engine.' >&2
    exit 1
fi
[ -f "$FFMPEG_SRC/fftools/ffplay.c" ] || { echo 'FFmpeg fftools/ffplay.c not found' >&2; exit 1; }
[ -f "$SDL2_ROOT/sdl2/include/SDL.h" ] || { echo 'Miyoo SDL2 headers not found' >&2; exit 1; }
# Accept the actual Steward-Fu SDL2 output layout: prebuilt/640x480.
# Older snapshots use prebuilt/mini; local builds may use sdl2/build/.libs.
# SDL2_LIB_DIR can override autodetection if the library lives elsewhere.
sdl_lib=''
if [ -n "${SDL2_LIB_DIR:-}" ]; then
    [ -d "$SDL2_LIB_DIR" ] || { echo "SDL2_LIB_DIR not found: $SDL2_LIB_DIR" >&2; exit 1; }
    sdl_lib=$(CDPATH= cd -- "$SDL2_LIB_DIR" && pwd -P)
else
    for candidate in "$SDL2_ROOT/prebuilt/640x480" \
                     "$SDL2_ROOT/prebuilt/mini" \
                     "$SDL2_ROOT/sdl2/build/.libs"; do
        if [ -f "$candidate/libSDL2.so" ] || [ -f "$candidate/libSDL2-2.0.so.0" ]; then
            sdl_lib="$candidate"
            break
        fi
    done
fi
[ -n "$sdl_lib" ] || { echo 'Miyoo ARM SDL2 library not found (checked prebuilt/640x480, prebuilt/mini and sdl2/build/.libs)' >&2; exit 1; }
if [ -f "$sdl_lib/libSDL2.so" ]; then
    sdl_shared="$sdl_lib/libSDL2.so"
elif [ -f "$sdl_lib/libSDL2-2.0.so.0" ]; then
    sdl_shared="$sdl_lib/libSDL2-2.0.so.0"
else
    echo "No libSDL2.so or libSDL2-2.0.so.0 in $sdl_lib" >&2
    exit 1
fi
echo "Using SDL2 library: $sdl_shared"
[ -f "$SDL2_ROOT/examples/libSDL2_image-2.0.so.0" ] || {
    echo 'Miyoo SDL2_image library missing' >&2; exit 1;
}
[ -f "$SDL2_ROOT/examples/libSDL2_ttf-2.0.so.0" ] || {
    echo 'Miyoo SDL2_ttf library missing' >&2; exit 1;
}

scratch=$(mktemp -d "${TMPDIR:-/tmp}/minicinema-build.XXXXXX")
original_ui=0
original_srt=0
original_font=0
cp "$FFMPEG_SRC/fftools/ffplay.c" "$scratch/original_ffplay.c"
if [ -e "$FFMPEG_SRC/fftools/minicinema_menu_font.h" ]; then
    original_font=1
    cp "$FFMPEG_SRC/fftools/minicinema_menu_font.h" "$scratch/original_minicinema_menu_font.h"
fi
if [ -e "$FFMPEG_SRC/fftools/minicinema_srt.inc" ]; then
    original_srt=1
    cp "$FFMPEG_SRC/fftools/minicinema_srt.inc" "$scratch/original_minicinema_srt.inc"
fi
if [ -e "$FFMPEG_SRC/fftools/minicinema_ui.inc" ]; then
    original_ui=1
    cp "$FFMPEG_SRC/fftools/minicinema_ui.inc" "$scratch/original_minicinema_ui.inc"
fi
cleanup() {
    cp "$scratch/original_ffplay.c" "$FFMPEG_SRC/fftools/ffplay.c"
    if [ "$original_ui" -eq 1 ]; then
        cp "$scratch/original_minicinema_ui.inc" "$FFMPEG_SRC/fftools/minicinema_ui.inc"
    else
        rm -f "$FFMPEG_SRC/fftools/minicinema_ui.inc"
    fi
    if [ "$original_srt" -eq 1 ]; then
        cp "$scratch/original_minicinema_srt.inc" "$FFMPEG_SRC/fftools/minicinema_srt.inc"
    else
        rm -f "$FFMPEG_SRC/fftools/minicinema_srt.inc"
    fi
    if [ "$original_font" -eq 1 ]; then
        cp "$scratch/original_minicinema_menu_font.h" "$FFMPEG_SRC/fftools/minicinema_menu_font.h"
    else
        rm -f "$FFMPEG_SRC/fftools/minicinema_menu_font.h"
    fi
    rm -rf "$scratch"
}
trap cleanup 0
trap 'exit 1' 1 2 3 15
cp "$root/minicinema_engine.c" "$FFMPEG_SRC/fftools/ffplay.c"
cp "$root/minicinema_ui.inc" "$FFMPEG_SRC/fftools/minicinema_ui.inc"
cp "$root/minicinema_menu_font.h" "$FFMPEG_SRC/fftools/minicinema_menu_font.h"
cp "$root/minicinema_srt.inc" "$FFMPEG_SRC/fftools/minicinema_srt.inc"

cross_dir="$scratch/cross-tools"
mkdir -p "$cross_dir"
for tool in gcc ar ranlib nm; do
    wrapper="$cross_dir/arm-linux-gnueabihf-$tool"
    if [ "$tool" = gcc ]; then
        printf '#!/bin/sh\nexec "%s" cc -target arm-linux-gnueabihf.2.28 "$@"\n' \
            "$ZIG_BIN" > "$wrapper"
    elif [ "$tool" = nm ]; then
        printf '#!/bin/sh\nexec nm "$@"\n' > "$wrapper"
    else
        printf '#!/bin/sh\nexec "%s" %s "$@"\n' "$ZIG_BIN" "$tool" > "$wrapper"
    fi
    chmod +x "$wrapper"
done
printf '%s\n' '#!/bin/sh' \
    'case "$1" in' \
    '    --version) echo 2.0.22 ;;' \
    '    --cflags) echo "-I${KIDSPLAY_SDL2_INCLUDE} -I${KIDSPLAY_SDL2_CONFIG_INCLUDE}" ;;' \
    '    --libs) echo "-L${KIDSPLAY_SDL2_LIB} -lSDL2 -Wl,--allow-shlib-undefined" ;;' \
    '    *) exit 1 ;;' \
    'esac' > "$cross_dir/arm-linux-gnueabihf-sdl2-config"
chmod +x "$cross_dir/arm-linux-gnueabihf-sdl2-config"

sdl_include="$SDL2_ROOT/sdl2/include"
sdl_config_include="${SDL2_CONFIG_INCLUDE:-$SDL2_ROOT/../include}"
[ -d "$sdl_config_include" ] || sdl_config_include="$sdl_include"
# -lSDL2 requires the unversioned linker name. Keep any symlink in scratch,
# never modify the SDL2 repository or the app's runtime libraries.
sdl_link="$scratch/sdl-link"
mkdir -p "$sdl_link"
ln -s "$sdl_shared" "$sdl_link/libSDL2.so"
sdl_ext_lib="$SDL2_ROOT/examples"
export KIDSPLAY_SDL2_INCLUDE="$sdl_include"
export KIDSPLAY_SDL2_CONFIG_INCLUDE="$sdl_config_include"
export KIDSPLAY_SDL2_LIB="$sdl_link"

cd "$FFMPEG_SRC"
./configure \
    --enable-cross-compile --cross-prefix="$cross_dir/arm-linux-gnueabihf-" \
    --target-os=linux --arch=arm --cpu=cortex_a7 \
    --enable-neon --disable-runtime-cpudetect \
    --disable-everything --disable-ffmpeg --disable-ffprobe --enable-ffplay \
    --enable-sdl2 --enable-pthreads --enable-avcodec --enable-avformat \
    --enable-avfilter --enable-swscale --enable-swresample --disable-avdevice \
    --disable-network --enable-protocol=file \
    --enable-demuxer=mov,matroska,avi,flv,mpegts,mpegps,mpegvideo,asf,mp3,ogg,wav,aac,flac \
    --enable-decoder=h264,hevc,mpeg4,mpeg2video,mpeg1video,flv,vp8,vp9,theora,mjpeg,wmv1,wmv2,wmv3,vc1,msmpeg4v1,msmpeg4v2,msmpeg4v3,ass,subrip,mov_text,webvtt,aac,aac_fixed,aac_latm,mp3,mp3float,ac3,eac3,vorbis,opus,flac,alac,wmav1,wmav2,wmapro,mp2,dca,h263,pcm_u8,pcm_dvd,adpcm_ms,adpcm_ima_wav,pcm_s16le,pcm_s16be,pcm_s24le,pcm_f32le \
    --enable-parser=h264,hevc,mpeg4video,mpegvideo,aac,aac_latm,ac3,mpegaudio,dca,h263,opus,vorbis \
    --enable-filter=crop,transpose,hflip,vflip,rotate,format,scale,pad,aresample,volume,acompressor,pan \
    --disable-doc --disable-debug --disable-autodetect --disable-stripping \
    --disable-shared --enable-static \
    --extra-cflags="-O3 -g0 -fomit-frame-pointer -I$sdl_include" \
    --extra-ldflags="-L$sdl_link -L$sdl_lib -L$sdl_ext_lib -Wl,--allow-shlib-undefined" \
    --extra-libs="-lSDL2 $sdl_ext_lib/libSDL2_image-2.0.so.0 $sdl_ext_lib/libSDL2_ttf-2.0.so.0 -lm -ldl -lpthread"

# FFmpeg 4.4.8 tests for the sysctl symbol but may not verify that its
# removed <sys/sysctl.h> header is available. Linux ARM does not need this
# optional fallback (CPU-count detection uses sysconf/getaffinity).
# Correct the generated configuration only; do not modify FFmpeg sources.
[ -f config.h ] || { echo 'FFmpeg configure did not create config.h' >&2; exit 1; }
if grep -Eq '^#define HAVE_SYSCTL[[:space:]]+1$' config.h; then
    sed -i 's/^#define HAVE_SYSCTL[[:space:]]\+1$/#define HAVE_SYSCTL 0/' config.h
    echo 'Disabled FFmpeg 4.4.8 sysctl: target libc has no sys/sysctl.h'
fi
if ! grep -Eq '^#define HAVE_SYSCTL[[:space:]]+0$' config.h; then
    echo 'Unexpected FFmpeg HAVE_SYSCTL setting in config.h' >&2
    exit 1
fi

make -j"${JOBS:-4}" ffplay

# Copy only after a full successful rebuild. Never leave an old player
# masked by a fresh launch script and falsely claim it has new controls.
cp ffplay "$out/minicinema-player.new"
chmod +x "$out/minicinema-player.new"
mv -f "$out/minicinema-player.new" "$out/minicinema-player"
printf '%s\n' 'MiniCinema ARM engine built successfully: '
ls -l "$out/minicinema-player"
