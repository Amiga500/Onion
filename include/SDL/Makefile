.PHONY: clean

TARGET = ../../lib/libSDL_rotozoom.so

CC = $(CROSS_COMPILE)gcc

PLATFORM ?= $(UNION_PLATFORM)
ifeq (,$(PLATFORM))
PLATFORM = linux
endif

ifeq ($(PLATFORM),miyoomini)
CFLAGS = -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
endif

$(TARGET):
	$(CC) -shared -o $(TARGET) -fPIC $(CFLAGS) SDL_rotozoom.c

clean:
	rm -f $(TARGET)
