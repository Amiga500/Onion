# AudioTune v0.2.9 (robcodedev): the app and its native VQE hook.
# RedOnion build: make (from the root Makefile) puts both in $(BUILD_DIR).
# minimp3.h is the pinned CC0 header (MP3-DECODER.txt), kept here so the
# build needs no network; fetch-mp3-decoder.sh still checks its SHA-256
# and stops the build if it differs.

CROSS_COMPILE ?= arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc
STRIP := $(CROSS_COMPILE)strip

ifeq (,$(BUILD_DIR))
BUILD_DIR := $(shell pwd -P)
endif

TARGET := audiotune
HOOK := libaudiotune_vqe.so

CFLAGS := -Os -std=c99 -Wall -Wextra -Wshadow -Wstrict-prototypes \
          -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
LDFLAGS := -no-pie -Wl,--dynamic-linker=/lib/ld-linux-armhf.so.3 \
           -Wl,-z,max-page-size=4096
LDLIBS := -lSDL -lpthread -ldl -lm

.PHONY: all build verify-mp3 clean

all: build

# Moved out, as src/common/recipes.mk does: every build compiles afresh and
# no stale binary stays in src/ after make clean.
build: $(TARGET) $(HOOK)
	@mkdir -p "$(BUILD_DIR)"
	@mv -f $(TARGET) $(HOOK) "$(BUILD_DIR)/"

verify-mp3:
	@./fetch-mp3-decoder.sh

$(TARGET): audiotune.c audiotune_mp3.c audiotune_mp3.h audiotune_presets.h audiotune_eq.h minimp3.h | verify-mp3
	$(CC) $(CFLAGS) -fno-pic -fno-pie -o "$@" audiotune.c audiotune_mp3.c $(LDFLAGS) $(LDLIBS)
	$(STRIP) "$@"

# Loaded only into audioserver, and only on the MI_AO library whose ABI it
# was checked against (launch.sh compares its SHA-256).
$(HOOK): native_vqe_hook.c audiotune_presets.h audiotune_eq.h
	$(CC) $(CFLAGS) -fPIC -shared -o "$@" native_vqe_hook.c -ldl -lm
	$(STRIP) --strip-unneeded "$@"

clean:
	rm -f $(TARGET) $(HOOK)
