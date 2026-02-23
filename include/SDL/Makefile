.PHONY: clean

TARGET = ../../lib/libSDL_rotozoom.so

CC = $(CROSS_COMPILE)gcc
SYSROOT := $(shell $(CC) -print-sysroot 2>/dev/null)
SDL_CFLAGS := $(shell $(CROSS_COMPILE)sdl-config --cflags 2>/dev/null || pkg-config --cflags sdl 2>/dev/null || if [ -d "$(SYSROOT)/usr/include/SDL" ]; then echo "-I$(SYSROOT)/usr/include"; fi)

$(TARGET):
	$(CC) $(SDL_CFLAGS) -shared -o $(TARGET) -fPIC SDL_rotozoom.c

clean:
	rm -f $(TARGET)
