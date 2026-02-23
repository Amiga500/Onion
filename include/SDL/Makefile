.PHONY: clean

TARGET = ../../lib/libSDL_rotozoom.so

CC = $(CROSS_COMPILE)gcc
SYSROOT := $(shell $(CC) -print-sysroot 2>/dev/null)
SDL_CFLAGS := $(shell $(CROSS_COMPILE)sdl-config --cflags 2>/dev/null || pkg-config --cflags sdl 2>/dev/null || if [ -d "$(SYSROOT)/usr/include/SDL" ]; then echo "-I$(SYSROOT)/usr/include"; fi)

# Check if SDL headers are available before attempting to build
SDL_AVAILABLE := $(shell echo '#include <SDL/SDL.h>' | $(CC) $(SDL_CFLAGS) -x c -c - -o /dev/null 2>/dev/null && echo yes)

$(TARGET):
ifeq ($(SDL_AVAILABLE),yes)
	$(CC) $(SDL_CFLAGS) -shared -o $(TARGET) -fPIC SDL_rotozoom.c
else
	@echo "-- SDL headers not found, skipping libSDL_rotozoom.so build"
endif

clean:
	rm -f $(TARGET)
