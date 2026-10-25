# OnionPlus's SDL_ttf: SDL_ttf 2.0.11 that also draws Arabic and Bengali.
#
#   make device    libSDL_ttf-2.0.so.0 and libSDL_ttf-script.so for the Miyoo,
#                  in build/device, stripped and checked
#   make install   copy the device build over static/build/miyoo/lib
#   make check     host build, unit tests and the comparison with the original
#                  SDL_ttf 2.0.11 over every string of Onion's language files
#   make clean
#
# The device build needs the Miyoo toolchain (CROSS_COMPILE, SYSROOT); the host
# build needs SDL 1.2 (sdl-config) and FreeType development files.

CROSS_COMPILE ?= /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-
SYSROOT       ?= /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr
ROOT          := $(abspath ../..)
LIB_DIR       := $(ROOT)/static/build/miyoo/lib
LANG_DIR      := $(ROOT)/static/build/miyoo/app/lang
FONT_DIR      := $(ROOT)/static/build/miyoo/app
PYTHON        ?= python3

# Same tuning as the rest of Onion.
ARCH      := -marm -mtune=cortex-a7 -march=armv7ve -mfpu=neon-vfpv4 -mfloat-abi=hard
TTF_SRC   := SDL_ttf.c
TTF_DEPS  := SDL_ttf.c SDL_ttf.h ttf_script.c ttf_bidi.c script/script_api.h
# A lean HarfBuzz: OpenType shaping only, no exceptions, RTTI or libstdc++.
HB_FLAGS  := -fPIC -fno-exceptions -fno-rtti -fno-threadsafe-statics \
             -fvisibility=hidden -ffunction-sections -fdata-sections -w -Os \
             -DHB_LEAN -DHB_MINI -DHB_OPTIMIZE_SIZE -DNDEBUG -DHAVE_PTHREAD
WARN      := -Wall -Wextra -Wshadow
# The original file's own warnings are left as upstream has them.
TTF_WARN  := -Wno-unused-parameter -Wno-sign-compare -Wno-overflow

.PHONY: device install host check clean

# ------------------------------------------------------------------ device

DEV       := build/device
DEV_CFLAGS := -O2 -g $(ARCH) -fPIC -D_GNU_SOURCE -D_REENTRANT \
              -I$(SYSROOT)/include/SDL -I$(SYSROOT)/include/freetype2

device: $(DEV)/libSDL_ttf-2.0.so.0 $(DEV)/libSDL_ttf-script.so

$(DEV)/libSDL_ttf-2.0.so.0: $(TTF_DEPS)
	@mkdir -p $(@D)
	$(CROSS_COMPILE)gcc $(DEV_CFLAGS) $(WARN) $(TTF_WARN) -shared -fvisibility=hidden \
	    -Wl,-soname,libSDL_ttf-2.0.so.0 $(TTF_SRC) -o $@ \
	    -L$(SYSROOT)/lib -lSDL -lfreetype -ldl -lpthread
	cp -p $@ $@.unstripped
	$(CROSS_COMPILE)strip --strip-unneeded $@
	READELF=$(CROSS_COMPILE)readelf $(PYTHON) tools/check_lib.py $@ upstream/miyoo-symbols.txt

$(DEV)/harfbuzz.o: harfbuzz/src/harfbuzz.cc
	@mkdir -p $(@D)
	$(CROSS_COMPILE)g++ $(ARCH) $(HB_FLAGS) -c $< -o $@

$(DEV)/script.o: script/script.c script/script_api.h
	@mkdir -p $(@D)
	$(CROSS_COMPILE)gcc $(DEV_CFLAGS) $(WARN) -std=gnu11 -fvisibility=hidden \
	    -Iharfbuzz/src -c $< -o $@

$(DEV)/libSDL_ttf-script.so: $(DEV)/script.o $(DEV)/harfbuzz.o
	$(CROSS_COMPILE)gcc -shared -Wl,--gc-sections -Wl,-z,defs $^ -o $@ \
	    -L$(SYSROOT)/lib -lfreetype -lm -lpthread
	cp -p $@ $@.unstripped
	$(CROSS_COMPILE)strip --strip-unneeded $@
	READELF=$(CROSS_COMPILE)readelf $(PYTHON) tools/check_lib.py $@

# Onion keeps three copies (FAT has no symlinks).
install: device
	cp -f $(DEV)/libSDL_ttf-2.0.so.0 $(LIB_DIR)/libSDL_ttf-2.0.so.0
	cp -f $(DEV)/libSDL_ttf-2.0.so.0 $(LIB_DIR)/libSDL_ttf-2.0.so.0.10.1
	cp -f $(DEV)/libSDL_ttf-2.0.so.0 $(LIB_DIR)/libSDL_ttf.so
	cp -f $(DEV)/libSDL_ttf-script.so $(LIB_DIR)/libSDL_ttf-script.so

# -------------------------------------------------------------------- host

HOST      := build/host
SANITIZE  ?=
HOST_CFLAGS = -O1 -g -fPIC -D_GNU_SOURCE -D_REENTRANT $(SANITIZE) \
               $(shell sdl-config --cflags) $(shell pkg-config --cflags freetype2)
HOST_LIBS = $(shell sdl-config --libs) $(shell pkg-config --libs freetype2)

host: $(HOST)/patched/libSDL_ttf-2.0.so.0 $(HOST)/patched/libSDL_ttf-script.so \
      $(HOST)/original/libSDL_ttf-2.0.so.0 $(HOST)/compare $(HOST)/test_bidi

# The original 2.0.11 (upstream/SDL_ttf.c), built the same way: what the
# patched library must match for every text without Arabic or Bengali.
$(HOST)/original/libSDL_ttf-2.0.so.0: upstream/SDL_ttf.c SDL_ttf.h
	@mkdir -p $(@D)
	$(CC) $(HOST_CFLAGS) $(TTF_WARN) -I. -shared -Wl,-soname,libSDL_ttf-2.0.so.0 $< -o $@ \
	    $(HOST_LIBS)

$(HOST)/patched/libSDL_ttf-2.0.so.0: $(TTF_DEPS)
	@mkdir -p $(@D)
	$(CC) $(HOST_CFLAGS) $(WARN) $(TTF_WARN) -Werror -shared -Wl,-soname,libSDL_ttf-2.0.so.0 \
	    $(TTF_SRC) -o $@ $(HOST_LIBS) -ldl -lpthread

$(HOST)/harfbuzz.o: harfbuzz/src/harfbuzz.cc
	@mkdir -p $(@D)
	$(CXX) $(HB_FLAGS) $(SANITIZE) -c $< -o $@

$(HOST)/patched/libSDL_ttf-script.so: script/script.c script/script_api.h $(HOST)/harfbuzz.o
	@mkdir -p $(@D)
	$(CC) $(HOST_CFLAGS) $(WARN) -Werror -std=gnu11 -fvisibility=hidden -Iharfbuzz/src -shared \
	    -Wl,--gc-sections script/script.c $(HOST)/harfbuzz.o -o $@ \
	    $(shell pkg-config --libs freetype2) -lm -lpthread $(SANITIZE)

# Linked against the patched build; the comparison swaps the library at run time.
$(HOST)/compare: tests/compare.c SDL_ttf.h $(HOST)/patched/libSDL_ttf-2.0.so.0
	$(CC) $(HOST_CFLAGS) $(WARN) -Werror -I. $< -o $@ $(HOST)/patched/libSDL_ttf-2.0.so.0 \
	    $(HOST_LIBS) $(SANITIZE)

$(HOST)/test_bidi: tests/test_bidi.c ttf_bidi.c
	@mkdir -p $(@D)
	$(CC) -std=gnu11 -O1 -g $(WARN) -Werror $(SANITIZE) -I. $< -o $@

check: host
	$(HOST)/test_bidi
	$(PYTHON) tests/compare.py --build $(HOST) --lang $(LANG_DIR) --fonts $(FONT_DIR)

clean:
	rm -rf build
