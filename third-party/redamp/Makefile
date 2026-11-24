# RedAmp v1.1.11: source-only ARM build, following MiniCinema's VM workflow.
# No App/ directory is required. Build outputs remain here in src/RedAmp.
# make / make all: player, game-music wrapper, private FFmpeg audio libraries.

ZIG_BIN ?= /root/workspace/zig-0.14.1/zig
ZIG_TARGET ?= arm-linux-gnueabihf.2.28
CROSS_CC ?=
export ZIG_BIN ZIG_TARGET CROSS_CC

.PHONY: all miyoo arm-deps player wrapper ffmpeg-audio ffbridge host check test clean

# Explicit recursive calls preserve build order even for make -j all.
all:
	$(MAKE) wrapper
	$(MAKE) arm-deps
	$(MAKE) player
	$(MAKE) ffmpeg-audio

miyoo: all

arm-deps:
	./tools/build_arm_deps.sh

player:
	./build.sh miyoo player

wrapper:
	./build.sh miyoo wrapper

ffmpeg-audio:
	./tools/build_ffmpeg.sh

# Fast bridge-only build (AAC metadata / User-Agent changes, not FFmpeg security patches).
ffbridge:
	./tools/build_ffbridge.sh

host:
	./build.sh host

# No App assets needed for build validation.
check:
	@for script in build.sh tools/build_arm_deps.sh tools/build_ffmpeg.sh tools/build_ffbridge.sh tools/verify_ffmpeg.sh; do \
	    sh -n "$$script" || exit 1; \
	done
	@test -f main.c -a -f decode.c -a -f radio.c -a -f ffaudio.c \
	    -a -f ffbridge.c -a -f padsp_wrap.c -a -f third_party/mini2d.c || \
	    { echo 'Required RedAmp sources missing' >&2; exit 1; }
	@echo 'RedAmp source and build-script checks passed.'
	@if [ -f redamp ]; then file redamp; \
	else echo 'Player not built: run make player'; fi
	@if [ -f libpadsp_wrap.so ]; then file libpadsp_wrap.so; \
	else echo 'Wrapper not built: run make wrapper'; fi
	@if [ -f lib/libredamp_ffaudio.so ]; then \
	    file lib/libredamp_ffaudio.so; \
	else echo 'Private FFmpeg not built: run make ffmpeg-audio'; fi

# Asset-dependent skin and RedOnion launch tests are NOT run on a source-only VM.
# Run them on a complete App/+src checkout with REDAMP_APP_DIR set instead.
test: host check
	@mkdir -p build
	$${HOST_CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -I. adts_duration.c \
	    tools/test_adts_duration.c -o build/test_adts_duration -pthread
	./build/test_adts_duration
	python3 ./tools/test_eq_presets.py
	$${HOST_CC:-gcc} -std=gnu11 -Wall -Wextra -I. -Ithird_party -DM2D_HOST \
	    tools/test_smoothing.c third_party/mini2d.c -o build/test_smoothing -lm
	./build/test_smoothing
	$${HOST_CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -I. -DM2D_HOST \
	    tools/test_stayawake.c stayawake.c -o build/test_stayawake
	./build/test_stayawake
	@mkdir -p build
	$${HOST_CC:-gcc} -std=c11 -Wall -Wextra -Werror -I. tools/test_ffpath.c -o build/test_ffpath
	./build/test_ffpath
	python3 ./tools/test_ffmpeg_hls_patch.py
	./tools/test_ffaudio.sh
	./tools/test_meta_corrupt.sh
	./tools/test_hardening.sh
	./tools/test_gamemusic.sh
	./tools/test_ioctl_preload.sh
	./tools/chiptest/run.sh
	./tools/test_playlists.sh
	./tools/test_library.sh
	./tools/test_album_art.sh
	./tools/test_netclock.sh
	$${HOST_CC:-gcc} -std=gnu11 -Wall -Wextra -Werror -I. tools/test_audio_switch.c audio.c -o build/test_audio_switch -lpthread -lm
	./build/test_audio_switch
	python3 ./tools/test_covers.py
	python3 ./tools/test_https_policy.py
	$${HOST_CC:-gcc} -O1 -I. tools/test_radio_offline.c radio.c netclock.c -DM2D_HOST -o build/test_radio_offline -lcurl -lpthread
	./build/test_radio_offline
	./tools/test_codecs.sh
	@if [ -n "$(REDAMP_APP_DIR)" ] && [ -d "$(REDAMP_APP_DIR)/skins" ]; then \
	    ./tools/test_native_launch.sh && ./tools/test_skin_gallery.sh && \
	    python3 ./tools/test_menu_release.py && \
	    python3 ./tools/test_navigation_battery.py && \
	    python3 ./tools/test_ui_v112.py && \
	    python3 ./tools/test_library_views.py && \
	    python3 ./tools/test_navigation_v114.py && \
	    python3 ./tools/test_https_menu.py && \
	    python3 ./tools/test_event_loop.py && \
	    python3 ./tools/test_standby.py; \
	else \
	    echo 'Skipping App-dependent launch and skin tests (source-only VM).'; \
	fi

clean:
	rm -rf build
	rm -f redamp libpadsp_wrap.so cacert.pem
	# Avoid erasing any user-supplied libraries placed in ./lib.
	rm -f lib/libavutil_redamp.so.* lib/libavcodec_redamp.so.* \
	    lib/libavformat_redamp.so.* lib/libswresample_redamp.so.* \
	    lib/libredamp_ffaudio.so lib/libssl.so.3 lib/libcrypto.so.3 \
	    lib/libcurl.so.4 lib/libopus.so.0 lib/libz.so.1
	-rmdir lib
