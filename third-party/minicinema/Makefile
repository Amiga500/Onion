# MiniCinema v1.1.41: source-only ARM build. Outputs stay in this directory.

CROSS_CC ?= /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-gcc
CLANG ?= clang
ARM_HEADERS ?= /usr/include/arm-linux-gnueabihf
BROWSER_FLAGS := -O2 -Wall -Wextra -Werror -std=c11 -fomit-frame-pointer \
    -ffreestanding -fno-builtin -fno-stack-protector -nostdlib -static \
    -Wl,-e,_start -Wl,--build-id=none
.PHONY: all browser browser-clang engine vsync check clean
# Keep the requested build order even when called with make -j all.
all:
	$(MAKE) vsync
	$(MAKE) browser
	$(MAKE) engine

browser:
	$(CROSS_CC) $(BROWSER_FLAGS) minicinema_browser.c -o minicinema

browser-clang:
	$(CLANG) --target=armv7a-linux-gnueabihf -marm \
		-isystem $(ARM_HEADERS) -isystem /usr/include -fuse-ld=lld \
		$(BROWSER_FLAGS) minicinema_browser.c -o minicinema

engine:
	./build_engine.sh

# Private framebuffer/vsync shim required by App/MiniCinema/launch.sh.
# Output is in src/; install under App/MiniCinema/libs/ yourself.
ZIG_BIN ?= /root/workspace/zig-0.14.1/zig
ZIG_TARGET ?= arm-linux-gnueabihf.2.28
vsync:
	$(ZIG_BIN) cc -target $(ZIG_TARGET) -O2 -fPIC -shared \
		-o libminivsync.so minicinema_vsync.c -ldl

check:
	sh -n build_engine.sh
	@test -f minicinema_browser.c -a -f minicinema_engine.c -a \
	    -f minicinema_ui.inc -a -f minicinema_srt.inc -a \
	    -f minicinema_menu_font.h -a -f minicinema_vsync.c || \
	    { echo 'Required MiniCinema sources are missing' >&2; exit 1; }
	@if [ -f minicinema ]; then file minicinema; \
	else echo 'Browser not built: run make browser'; fi
	@if [ -f minicinema-player ]; then file minicinema-player; \
	else echo 'Player not built: run make engine'; fi

	@if [ -f libminivsync.so ]; then file libminivsync.so; \
	else echo 'Vsync shim not built: run make vsync'; fi

clean:
	rm -f minicinema minicinema-player libminivsync.so
