# Button Lab v0.1 - Onion / Miyoo Mini / Mini Plus
# Place this Makefile and buttonlab.c in Onion's src/buttonlab/ directory.
INCLUDE_UTILS=0
PLATFORM ?= miyoomini
CROSS_COMPILE ?= arm-linux-gnueabihf-
include ../common/config.mk

TARGET = buttonlab

CFLAGS := $(CFLAGS) -std=c99 -Os -Wall -Wextra \
          -fno-stack-protector -fno-pic -fno-pie

# Keep normal libc startup. This is an SDL application, not a bare _start ELF.
LDFLAGS := $(LDFLAGS) -no-pie \
           -Wl,--dynamic-linker=/lib/ld-linux-armhf.so.3 \
           -Wl,-rpath,/mnt/SDCARD/.tmp_update/lib \
           -Wl,-z,max-page-size=4096 \
           -lSDL -lpthread -ldl -lm

# config.mk automatically includes *.c in this directory in OFILES.
include ../common/commands.mk
include ../common/recipes.mk

%.o: %.S
	@$(CC) $(CFLAGS) -c "$<" -o "$@"
