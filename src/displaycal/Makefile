INCLUDE_UTILS=0
include ../common/config.mk

TARGET = displaycal-gui

CFLAGS := $(CFLAGS) -Os -ffreestanding -fno-builtin \
          -fno-stack-protector -fno-pic -fno-pie

OFILES := $(OFILES) aeabi_div.o

LDFLAGS := $(LDFLAGS) -nostdlib -no-pie -Wl,-e,_start \
           -Wl,--dynamic-linker=/lib/ld-linux-armhf.so.3 \
           -Wl,-z,max-page-size=4096 -ldl -lgcc

include ../common/commands.mk
include ../common/recipes.mk

%.o: %.S
	@$(CC) $(CFLAGS) -c "$<" -o "$@"