# SD Card Doctor v0.6.3 - Onion/RedOnion Miyoo build
# Place this source directory next to your Onion devkit's common/ directory.
INCLUDE_UTILS=0
include ../common/config.mk

TARGET = sdcarddoctor

CFLAGS := $(CFLAGS) -std=c99 -Os -Wall -Wextra \
          -fstack-protector-strong -fno-pic -fno-pie

LDFLAGS := $(LDFLAGS) -no-pie \
           -Wl,--dynamic-linker=/lib/ld-linux-armhf.so.3 \
           -Wl,-z,max-page-size=4096 \
           -lSDL -lSDL_ttf -lpthread -ldl -lm

include ../common/commands.mk
include ../common/recipes.mk

%.o: %.S
	@$(CC) $(CFLAGS) -c "$<" -o "$@"
