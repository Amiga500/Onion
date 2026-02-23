.PHONY: clean

TARGET = ../../lib/libSDL_rotozoom.so

CC = $(CROSS_COMPILE)gcc

SDL_CFLAGS := $(shell $(CROSS_COMPILE)sdl-config --cflags 2>/dev/null || pkg-config --cflags sdl 2>/dev/null)

$(TARGET):
	$(CC) -shared -o $(TARGET) -fPIC $(SDL_CFLAGS) SDL_rotozoom.c

clean:
	rm -f $(TARGET)
